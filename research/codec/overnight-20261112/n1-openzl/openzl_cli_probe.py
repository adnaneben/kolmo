#!/usr/bin/env python3
"""OpenZL v0.3.0 CLI reference probe; not comparable to in-memory P2-E1 timings.

Only pinned, untrained profiles are supported. All invocation overhead and file I/O
are included, and every round trip must reproduce original bytes exactly.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys
import tempfile
import time

RELEASE = '32246b48faee46807f84183dac4db479089f5445'  # meta/openzl v0.3.0
PROFILES = {'serial', 'csv', 'parquet', 'numpy', 'pickle', 'onnx',
            'safetensors', 'gguf', 'sao', 'u8', 'le-u16', 'le-u32',
            'le-u64', 'le-i16', 'le-i32', 'le-i64', 'be-u16',
            'be-u32', 'be-u64', 'be-i16', 'be-i32', 'be-i64'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def checked_cmd(argv, timeout):
    # Includes subprocess launch, file I/O and process termination in wall time.
    a = resource.getrusage(resource.RUSAGE_CHILDREN)
    t = time.perf_counter_ns()
    try:
        p = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=timeout, check=False)
    except subprocess.TimeoutExpired as e:
        raise RuntimeError(f'timed out: {argv[1]}') from e
    w = (time.perf_counter_ns() - t) * 1e-9
    b = resource.getrusage(resource.RUSAGE_CHILDREN)
    cpu = b.ru_utime + b.ru_stime - a.ru_utime - a.ru_stime
    if p.returncode:
        raise RuntimeError(f'{argv[1]} failed ({p.returncode}): {p.stderr[-500:]!r}')
    return {'wall_s': w, 'cpu_s': max(cpu, 0)}, p.stdout.decode('utf-8', 'replace')


def run(manifest, zli, out_path):
    if manifest.get('schema') != 1 or manifest.get('openzl_git_sha') != RELEASE:
        raise ValueError('must pin OpenZL v0.3.0 release commit')
    if manifest.get('training_regime') != 'untrained':
        raise ValueError('trained model needs separate training and amortization protocol')
    if manifest.get('scope') not in {'development', 'mock_test'}:
        raise ValueError('declare development or mock_test; no fresh validation in this probe')
    if manifest.get('level') not in (6, 7, 9):
        raise ValueError('only predeclared levels 6/7/9 supported')
    if manifest.get('profile') not in PROFILES:
        raise ValueError('profile not preregistered in allowed set')
    if not isinstance(manifest.get('repetitions'), int) or not 3 <= manifest['repetitions'] <= 7:
        raise ValueError('paired repetitions must be 3..7')
    if not isinstance(manifest.get('timeout_seconds'), (int, float)) or not 1 <= manifest['timeout_seconds'] <= 120:
        raise ValueError('per-command timeout must be 1..120 seconds')
    if not isinstance(manifest.get('max_input_bytes'), int) or not 1 <= manifest['max_input_bytes'] <= 50_000_000:
        raise ValueError('input cap must be 1..50MB')
    src = Path(manifest['input_path']).resolve()
    raw = src.read_bytes()
    if not raw or len(raw) > manifest['max_input_bytes'] or sha(raw) != manifest.get('input_sha256'):
        raise ValueError('input size or hash mismatch')
    zli = Path(zli).resolve()
    if not zli.is_file() or not os.access(zli, os.X_OK):
        raise ValueError('zli executable unavailable')
    exe_sha = sha(zli.read_bytes())
    if exe_sha != manifest.get('zli_binary_sha256'):
        raise ValueError('zli binary differs from pinned build')
    _, version = checked_cmd([str(zli), '--version'], 10)
    if manifest.get('expected_version_fragment') not in version:
        raise ValueError('zli version string differs from pin')
    rows = []
    with tempfile.TemporaryDirectory(prefix='kolmo-ozl-') as tmp:
        for i in range(manifest['repetitions']):
            enc = Path(tmp) / f'{i}.zl'
            back = Path(tmp) / f'{i}.back'
            a, _ = checked_cmd([str(zli), 'compress', '--profile', manifest['profile'],
                                '--level', str(manifest['level']), str(src), '--output', str(enc)],
                               manifest['timeout_seconds'])
            size = enc.stat().st_size
            b, _ = checked_cmd([str(zli), 'decompress', str(enc), '--output', str(back)],
                               manifest['timeout_seconds'])
            if back.read_bytes() != raw:
                raise RuntimeError(f'byte-exact decompression failed at pair {i}')
            rows.append({'pair_id': f'r{i}', 'stored_bytes': size,
                         'frame_sha256': sha(enc.read_bytes()),
                         'encode_wall_s': a['wall_s'], 'encode_cpu_s': a['cpu_s'],
                         'decode_wall_s': b['wall_s'], 'decode_cpu_s': b['cpu_s']})
    if len({r['stored_bytes'] for r in rows}) != 1 or len({r['frame_sha256'] for r in rows}) != 1:
        raise RuntimeError('non-deterministic OpenZL stored frame; inspect before comparison')
    return {'schema': 1, 'status': 'measured_mock_only' if manifest['scope'] == 'mock_test' else 'development_cli_only',
            'comparison_eligible': False, 'reason': 'CLI startup/file I/O boundary differs from in-memory P2-E1; no matched Kolmo candidate',
            'source_sha256': sha(raw), 'original_bytes': len(raw),
            'manifest_sha256': sha(json.dumps(manifest, sort_keys=True).encode()),
            'zli_binary_sha256': exe_sha, 'zli_version': version.strip(),
            'openzl_git_sha_claimed': RELEASE, 'training_regime': 'untrained',
            'profile': manifest['profile'], 'level': manifest['level'],
            'measurement_scope': 'complete_cli_including_process_and_file_io',
            'platform': sys.platform, 'rows': rows, 'byte_exact_verified': True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('manifest', type=Path)
    p.add_argument('--zli', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    m = json.loads(a.manifest.read_text())
    result = run(m, a.zli, a.output)
    a.output.write_text(json.dumps(result, sort_keys=True, indent=2) + '\n')
    print(result['status'])


if __name__ == '__main__':
    main()
