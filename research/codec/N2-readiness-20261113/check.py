#!/usr/bin/env python3
"""Fail-closed N2 correctness gate. One-round diagnostics are NOT performance results."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from typing import Any

ARMS = (
    'zstd3', 'kolmo_fast', 'tp_prod3', 'tp_phuf_pg16', 'tp_pent', 'tp_arch19',
    'oz_exact6', 'oz_exact9', 'oz_whole9', 'oz_serial9',
    'zstd3_duplicate', 'oz_exact9_duplicate',
)
RUN_BLOB = '9cb8e4fd00d270435a2e01b8f9ddfe39e1f1aabe'
NATIVE_BLOB = '2a0f3fa7e3c7f2dbf233fbfd9fd6a7decfe74be3'
REFERENCE = '4c78ea9893a7882bc4d427cca29076623312d66a'
OPENZL = '32246b48faee46807f84183dac4db479089f5445'


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def batch_for(length: int) -> int:
    if type(length) is not int or not 0 < length <= (16 << 20):
        raise ValueError('original input must be 1..16 MiB')
    return max(1, min(16, (4 << 20) // length))


def audit_frames(rows: list[dict[str, Any]], original: bytes, folder: Path) -> dict[str, str]:
    """Bind the ledger to actual input bytes and the registered batch rule."""
    expected = {(a, op, 0) for a in ARMS for op in ('encode', 'decode')}
    seen: set[tuple[str, str, int]] = set()
    sizes: dict[str, int] = {}
    for row in rows:
        for field in ('round', 'batch', 'input_bytes', 'stored_bytes', 'wall_ns', 'cpu_ns'):
            if type(row.get(field)) is not int or row[field] < 0:
                raise ValueError('invalid integer: ' + field)
        key = (row.get('arm'), row.get('operation'), row['round'])
        if key not in expected or key in seen:
            raise ValueError('unexpected or duplicate sample')
        seen.add(key)
        if row['input_bytes'] != len(original) or row['batch'] != batch_for(len(original)):
            raise ValueError('sample input length/batch differs from the actual input contract')
        if not 0 < row['stored_bytes'] <= (64 << 20) or row['wall_ns'] <= 0:
            raise ValueError('empty frame or nonpositive duration')
        arm = row['arm']
        if arm in sizes and sizes[arm] != row['stored_bytes']:
            raise ValueError('encode/decode frame sizes differ')
        sizes[arm] = row['stored_bytes']
    if seen != expected:
        raise ValueError('missing sample')
    expected_names = {a + '.frame' for a in ARMS}
    if {p.name for p in folder.glob('*.frame')} != expected_names:
        raise ValueError('missing or extra frame')
    hashes = {}
    for arm in ARMS:
        path = folder / (arm + '.frame')
        if path.is_symlink() or not path.is_file() or path.stat().st_size != sizes[arm]:
            raise ValueError('invalid frame file or frame/ledger size mismatch')
        hashes[arm] = sha(path.read_bytes())
    for arm, duplicate in [('zstd3', 'zstd3_duplicate'), ('oz_exact9', 'oz_exact9_duplicate')]:
        if hashes[arm] != hashes[duplicate]:
            raise ValueError('duplicate control frame mismatch')
    return hashes


def validate_decoder_config(config: Any) -> dict[str, dict[str, Any]]:
    if not isinstance(config, dict) or set(config) != {'kolmo', 'openzl', 'zstd'}:
        raise ValueError('all three independent decoder entries are required')
    for name, entry in config.items():
        if not isinstance(entry, dict):
            raise ValueError('decoder entry must be an object: ' + name)
        argv = entry.get('argv')
        if not isinstance(argv, list) or not argv or not all(isinstance(x, str) and x for x in argv):
            raise ValueError('decoder argv must be a nonempty string array')
        if argv.count('{input}') != 1 or argv.count('{output}') != 1:
            raise ValueError('decoder argv needs one standalone input and output placeholder')
        for token in argv:
            if ('{' in token or '}' in token) and token not in ('{input}', '{output}'):
                raise ValueError('unknown or embedded decoder placeholder')
        executable = Path(argv[0])
        if not executable.is_absolute() or not executable.is_file() or not os.access(executable, os.X_OK):
            raise ValueError('decoder executable must be an existing absolute executable path')
        if sha(executable.read_bytes()) != entry.get('sha256'):
            raise ValueError('decoder executable hash mismatch: ' + name)
    return config


def execute(argv: list[str], prefix: Path, timeout: float) -> dict[str, Any]:
    """Save logs even on failure; kill the whole child process group on timeout."""
    if not 0 < timeout <= 180:
        raise ValueError('timeout must be positive and at most 180 seconds')
    record = {'argv': argv, 'timeout_seconds': timeout, 'timed_out': False}
    with prefix.with_suffix('.stdout').open('wb') as stdout, prefix.with_suffix('.stderr').open('wb') as stderr:
        process = subprocess.Popen(argv, stdout=stdout, stderr=stderr, start_new_session=True)
        try:
            record['returncode'] = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            record['timed_out'] = True
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            record['returncode'] = process.wait()
    prefix.with_suffix('.command.json').write_text(json.dumps(record, indent=2) + '\n')
    if record['timed_out'] or record['returncode']:
        raise RuntimeError('command failed; see ' + str(prefix.with_suffix('.command.json')))
    return record


def check_restored(path: Path, original: bytes) -> None:
    if path.is_symlink() or not path.is_file() or path.stat().st_size != len(original):
        raise ValueError('independent decoder output length/file mismatch')
    if path.read_bytes() != original:
        raise ValueError('independent decoder byte mismatch')


def load_runner(repo: Path):
    path = repo / 'research/codec/N2-matched-native/run.py'
    if git_blob(path.read_bytes()) != RUN_BLOB:
        raise ValueError('frozen N2 runner changed; review required')
    spec = importlib.util.spec_from_file_location('n2_frozen_gate_runner', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    if tuple(module.ARMS) != ARMS:
        raise ValueError('registered arms changed')
    return module


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'binary', 'source-receipt', 'artifact', 'decoders', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--total-timeout', type=float, default=600)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    status = {'status': 'BLOCKED_PREREQUISITE', 'performance': 'NOT_RUN',
              'sanitizers': 'NOT_ESTABLISHED_BY_THIS_GATE', 'inputs_completed': 0,
              'frames_checked': 0, 'independent_crossdecodes': 0,
              'fresh_data': False, 'learned_selector': False}
    identities = {}
    try:
        if not 0 < args.timeout <= 180 or not 0 < args.total_timeout <= 900:
            raise ValueError('timeouts must be positive; per-command <=180s and total <=900s')
        deadline = time.monotonic() + args.total_timeout
        def remaining():
            value = min(args.timeout, deadline - time.monotonic())
            if value <= 0:
                raise TimeoutError('total correctness-gate time limit reached')
            return value
        repo, binary = args.repo.resolve(), args.binary.resolve()
        runner = load_runner(repo)
        runner.load_policy(repo / 'research/codec/competitor-policy-v2/policy.py')
        receipt = json.loads(args.source_receipt.read_text())
        if receipt.get('reference') != REFERENCE or receipt.get('openzl') != OPENZL:
            raise ValueError('build source pins do not match N2')
        binary_sha = sha(binary.read_bytes())
        native_data = (repo / 'research/codec/N2-matched-native/native.cpp').read_bytes()
        if git_blob(native_data) != NATIVE_BLOB:
            raise ValueError('frozen N2 native driver changed; review required')
        native_sha = sha(native_data)
        if receipt.get('sha256', {}).get('n2_native') != binary_sha or receipt['sha256'].get('native.cpp') != native_sha:
            raise ValueError('build receipt binary/native source mismatch')
        config = validate_decoder_config(json.loads(args.decoders.read_text()))
        (out / 'DECODERS.json').write_text(json.dumps(config, indent=2) + '\n')
        (out / 'SOURCE_RECEIPT.json').write_bytes(args.source_receipt.read_bytes())
        (out / 'inputs').mkdir()
        inputs = runner.prepare(args.artifact, out / 'inputs')
        (out / 'INPUTS.json').write_text(json.dumps(inputs, indent=2) + '\n')
        status.update(status='FAILED_CORRECTNESS_GATE', binary_sha256=binary_sha,
                      runner_git_blob=RUN_BLOB, source_reference=REFERENCE, openzl_commit=OPENZL)
        for index, item in enumerate(inputs):
            original = (out / 'inputs' / item['name']).read_bytes()
            batch_for(len(original))
            dest = out / item['name']
            dest.mkdir()
            execute([str(binary), str(out / 'inputs' / item['name']), str(dest), '1', str(2026111303 + index)],
                    dest / 'native', remaining())
            rows = [json.loads(line) for line in (dest / 'native.stdout').read_text().splitlines()]
            runner.validate_rows(rows, 1)
            identities[item['name']] = audit_frames(rows, original, dest)
            status['frames_checked'] += len(ARMS)
            for arm in ARMS:
                codec = 'openzl' if arm.startswith('oz_') else ('zstd' if arm.startswith('zstd3') else 'kolmo')
                restored = dest / (arm + '.restored')
                if restored.exists():
                    raise ValueError('refusing to reuse an existing decoded output')
                argv = [str(dest / (arm + '.frame')) if token == '{input}' else str(restored) if token == '{output}' else token
                        for token in config[codec]['argv']]
                # Recheck identity immediately before each use; no shell command evaluation.
                if sha(Path(argv[0]).read_bytes()) != config[codec]['sha256']:
                    raise ValueError('decoder changed during the gate')
                execute(argv, dest / ('cross-' + arm), remaining())
                check_restored(restored, original)
                status['independent_crossdecodes'] += 1
            status['inputs_completed'] += 1
        status['status'] = 'PASSED_EXACT_ROUNDTRIP_AND_CROSSDECODE_GATE_ONLY'
        return 0
    except Exception as error:
        status['error_type'] = type(error).__name__
        status['error'] = str(error)
        return 2
    finally:
        (out / 'FRAME_HASHES.json').write_text(json.dumps(identities, indent=2) + '\n')
        (out / 'STATUS.json').write_text(json.dumps(status, indent=2) + '\n')
        manifest = {str(p.relative_to(out)): sha(p.read_bytes()) for p in sorted(out.rglob('*'))
                    if p.is_file() and not p.is_symlink() and p.name != 'MANIFEST.json'}
        (out / 'MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(json.dumps(status, sort_keys=True))


if __name__ == '__main__':
    raise SystemExit(main())
