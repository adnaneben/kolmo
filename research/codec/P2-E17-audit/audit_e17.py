#!/usr/bin/env python3
"""Independently audit frozen P2-E17 evidence; NEVER issue codec acceptance.

Standard library only. Does not extract ZIPs, execute codecs, fetch data, train,
change a repository, or infer an online selector from the measured frontier.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import statistics
import sys
from typing import Any
import zipfile

ARTIFACT_SHA256 = 'f2e4c589b9e77c3c1cce609f3df120d6a6fc0be6033cc18f2a3cf8ec00669e1a'
SOURCE_COMMIT = 'f4fcadcd762d7d7bc9e484a4730900eb3388d5e9'
OPENZL_COMMIT = '32246b48faee46807f84183dac4db479089f5445'
PHASES = ('first', 'confirmation')
RESEARCH = ('tb_best', 'tp_pent', 'tp_arch19')
PRODUCTION = ('kolmo_fast', 'kolmo_hc', 'kolmo_max')
ROUND_IDS = ('0', '1', '2')
TIMING_BOUNDARY = 'complete_CLI_wall_and_child_CPU_includes_startup_IO_no_postverification'
SHA_RE = re.compile(r'^[0-9a-f]{64}$')
SOURCE_RECEIPTS = {
    'research/codec/N1-baselines/baselines.py': '7e477271e6e9e72c2a8034c62087f27d79981cef853ebe24bdb8046130017d26',
    'research/codec/P2-E17/run_e17.py': '2c4f2c41b998ab05ca89ff164940b55ed4845c875e0710ee0afe922f4e9e80d1',
    'research/codec/P2-E17/PROTOCOL.md': 'fac8828d796a20b14369de285bb28a6950e8d91b5fb6a483ef23b685e364f91f',
}


class AuditError(ValueError):
    """Incomplete, inconsistent, malformed, or untrusted evidence."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AuditError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def safe_name(name: str) -> bool:
    if not isinstance(name, str) or not name or '\\' in name or '\x00' in name:
        return False
    p = PurePosixPath(name)
    return not p.is_absolute() and ':' not in name and all(x not in ('', '.', '..') for x in name.split('/'))


def json_read(data: bytes) -> Any:
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, f'duplicate JSON key: {key}')
            result[key] = value
        return result

    def constant(value):
        raise AuditError(f'non-finite JSON constant: {value}')

    try:
        return json.loads(data, object_pairs_hook=pairs, parse_constant=constant)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise AuditError(f'invalid JSON: {exc}') from exc


def number(value: Any, label: str) -> float:
    require(type(value) in (int, float), f'{label}: not a numeric value')
    require(math.isfinite(value) and value >= 0, f'{label}: negative/non-finite')
    return float(value)


def integer(value: Any, label: str, positive: bool = False) -> int:
    require(type(value) is int and value >= int(positive), f'{label}: invalid integer')
    return value


def same_number(a: Any, b: Any, label: str) -> None:
    a, b = number(a, label), number(b, label)
    require(math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-15), f'{label}: numeric mismatch')


def charged(wall: Any, cpu: Any) -> float:
    wall, cpu = number(wall, 'wall time'), number(cpu, 'CPU time')
    return cpu if cpu > 1.03 * wall else wall


def costs(enc: dict, dec: dict, stored: int) -> dict:
    integer(stored, 'stored bytes', positive=True)
    e, d = charged(enc['wall_s'], enc['cpu_s']), charged(dec['wall_s'], dec['cpu_s'])
    return {
        'speed_cost_s': e + d + stored / 125_000_000,
        'archive_cost_usd': (e + 3.3 * d) * (3.89 / 72) / 3600 + stored / 1e9 * 0.023,
    }


def unique_index(rows: list, fields: tuple[str, ...], label: str) -> dict:
    require(isinstance(rows, list), f'{label}: expected list')
    result = {}
    for row in rows:
        require(isinstance(row, dict), f'{label}: expected object')
        key = tuple(row[f] for f in fields)
        require(key not in result, f'{label}: duplicate key {key}')
        result[key] = row
    return result


class EvidenceZip:
    """Bounded read-only archive; verify an externally pinned digest before parsing."""

    def __init__(self, path: Path, expected_sha: str):
        require(bool(SHA_RE.fullmatch(expected_sha)), 'invalid expected artifact SHA256')
        require(path.stat().st_size <= 512 << 20, 'compressed artifact too large')
        h = hashlib.sha256()
        with path.open('rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        self.digest = h.hexdigest()
        require(self.digest == expected_sha, 'artifact SHA256 mismatch')
        self.z = zipfile.ZipFile(path)
        try:
            infos = self.z.infolist()
            require(0 < len(infos) <= 20000, 'archive file-count limit')
            names = [i.filename for i in infos]
            require(len(names) == len(set(names)), 'duplicate archive member')
            require(all(safe_name(n) for n in names), 'unsafe archive member path')
            require(all(not i.is_dir() and not (i.flag_bits & 1) for i in infos), 'directory/encrypted member')
            require(all((i.external_attr >> 16) & 0o170000 != 0o120000 for i in infos), 'symlink member')
            require(all(i.file_size <= 128 << 20 for i in infos), 'member size limit')
            require(sum(i.file_size for i in infos) <= 1 << 30, 'expanded artifact size limit')
            self.names = set(names)
        except Exception:
            self.z.close()
            raise

    def read(self, name: str) -> bytes:
        require(name in self.names, f'missing archive member: {name}')
        return self.z.read(name)

    def json(self, name: str) -> Any:
        return json_read(self.read(name))

    def close(self) -> None:
        self.z.close()


def expected_policies(inp: dict, tools: dict) -> dict:
    profile = inp['profile']
    require(profile in ('le-i16', 'le-i32', 'le-i64', 'csv'), 'unexpected OpenZL profile')
    t = {key: value['path'] for key, value in tools.items()}
    result = {
        'zstd_3': ('zstd', [t['zstd'], '--single-thread', '-3', '-q', '-f'], [t['zstd'], '-d', '-q', '-f']),
        f'openzl_{profile}_9': ('openzl', [t['openzl'], 'compress', '-p', profile, '-l', '9', '-f'], [t['openzl'], 'decompress', '-f']),
    }
    for mode in ('fast', 'hc', 'max'):
        result['kolmo_' + mode] = ('kolmo', [t['kolmo'], '-m', mode, '-T1'], [t['kolmo'], '-d', '-T1'])
    for policy in RESEARCH:
        result[policy] = ('tb', [t['tb'], 'enc', policy], [t['kolmo'], '-d', '-T1'])
    return result


def check_command(event: dict, policy: dict, phase: str, inp: dict, operation: str, warmup: bool) -> None:
    base = policy['enc' if operation == 'encode' else 'dec']
    cmd = event['command']
    require(isinstance(cmd, list) and all(isinstance(x, str) for x in cmd), 'invalid command')
    root = f"ev/{phase}/{inp['id']}/{policy['id']}/"
    if operation == 'encode':
        require(len(cmd) > len(base), 'incomplete encoder command')
        src = cmd[len(base)]
        require(src.startswith('/') and src.endswith('/prepared/' + inp['file']), 'wrong complete encoder input')
        dst = root + ('retained.frame' if warmup else 'tmp-encode')
    else:
        src = root + 'retained.frame'
        dst = root + ('warmup.decoded' if warmup else 'tmp-decode')
    suffix = [src, dst] if policy['tool'] in ('kolmo', 'tb') else [src, '-o', dst]
    require(cmd == base + suffix, 'command differs from frozen whole-file policy')


def check_event(event: dict, policy: dict, phase: str, inp: dict, operation: str, warmup: bool) -> None:
    require(type(event['returncode']) is int and event['returncode'] == 0, 'failed timed operation')
    require(event['timed_out'] is False, 'timed-out operation')
    number(event['wall_s'], 'wall_s')
    number(event['cpu_s'], 'cpu_s')
    integer(event['peak_rss_kib'], 'peak_rss_kib')
    integer(event['assigned_cpu'], 'assigned_cpu')
    require(event['thread_constraint'] == 'single_CPU_affinity_and_thread_environment', 'thread constraint missing')
    require(event['rss_scope'] == 'individual_exec_child_process_not_codec_scratch', 'RSS scope mismatch')
    check_command(event, policy, phase, inp, operation, warmup)


def audit_phase(archive: EvidenceZip, phase: str, inputs: dict) -> tuple[dict, dict]:
    env = archive.json(f'{phase}/environment.json')
    require(env['timing'] == TIMING_BOUNDARY, 'incomparable measurement boundary')
    require(env['candidate_acceptance'] == 'NOT_EVALUATED_OBSERVED_BASELINE_CONTROLS', 'unexpected acceptance label')
    require(env['seed'] == (20261112 if phase == 'first' else 20261113), 'wrong randomized-order seed')
    require(set(env['tools']) == {'kolmo', 'zstd', 'openzl', 'tb'}, 'missing/unexpected tool')
    for tool in env['tools'].values():
        require(bool(SHA_RE.fullmatch(tool['sha256'])), 'bad binary receipt hash')
        require(safe_name(tool['path']), 'bad tool path')
    data = archive.json(f'{phase}/records.json')
    require(data['status'] == 'BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE', 'unexpected record status')
    require(type(data['rounds']) is int and data['rounds'] == 3, 'wrong repeat count')
    require(data['completed'] == len(inputs) * 8 and data['failed_profiles'] == 0, 'incomplete configuration count')
    rows = data['records']
    for row in rows:
        row['_policy_id'] = row['policy']['id']
    records = unique_index(rows, ('input_id', '_policy_id'), 'records')
    warmups = archive.json(f'{phase}/warmups.json')
    for w in warmups:
        w['_policy_id'] = w['policy']['id']
    warmups = unique_index(warmups, ('input_id', '_policy_id'), 'warmups')
    events = [json_read(line) for line in archive.read(f'{phase}/timings.jsonl').splitlines()]
    events = unique_index(events, ('input_id', 'policy_id', 'operation', 'pair_id'), 'timings')
    expected = {(ident, p) for ident, inp in inputs.items() for p in expected_policies(inp, env['tools'])}
    require(set(records) == expected and set(warmups) == expected, 'missing/unexpected policy records')
    event_keys = {(*key, op, pair) for key in expected for op in ('encode', 'decode') for pair in ROUND_IDS}
    require(set(events) == event_keys, 'missing/unexpected timing event')
    for key, rec in records.items():
        ident, policy_id = key
        inp, pol, warm = inputs[ident], rec['policy'], warmups[key]
        tool, enc, dec = expected_policies(inp, env['tools'])[policy_id]
        require(pol['tool'] == tool and pol['enc'] == enc and pol['dec'] == dec, 'policy registry mismatch')
        if tool == 'openzl':
            require(pol['profile'] == inp['profile'] and pol['level'] == 9, 'wrong rival profile/level')
            require(pol['training_regime'] == 'untrained_per_source_builtin_models_allowed', 'changed training regime')
        if tool == 'zstd':
            require(pol['level'] == 3, 'wrong speed rival level')
        if tool == 'tb':
            require(pol['research'] is True and pol['decoder'] == 'production kolmo CLI', 'research decoder contract')
        for field, value in (('family', inp['family']), ('view', inp['view']), ('input_bytes', inp['input_bytes']), ('input_sha256', inp['input_sha256'])):
            require(rec[field] == value, 'record input identity mismatch: ' + field)
        require(rec['status'] == 'MEASURED_DEVELOPMENT_CONTROL' and rec['byte_exact_verified'] is True, 'unverified record')
        integer(rec['stored_bytes'], 'stored bytes', positive=True)
        require(bool(SHA_RE.fullmatch(rec['frame_sha256'])), 'bad frame hash')
        require(warm['status'] == 'READY', 'incomplete warmup')
        for field in ('policy', 'input_bytes', 'input_sha256', 'stored_bytes', 'frame_sha256', 'warmup_encode', 'warmup_decode'):
            require(warm[field] == rec[field], 'warmup identity mismatch: ' + field)
        for operation in ('encode', 'decode'):
            check_event(rec['warmup_' + operation], pol, phase, inp, operation, True)
        runs = unique_index(rec['runs'], ('pair_id',), 'paired samples')
        require(set(runs) == {(r,) for r in ROUND_IDS}, 'wrong paired sample IDs')
        for pair in ROUND_IDS:
            sample = runs[(pair,)]
            enc_event = events[(*key, 'encode', pair)]
            dec_event = events[(*key, 'decode', pair)]
            for op, event in (('encode', enc_event), ('decode', dec_event)):
                check_event(event, pol, phase, inp, op, False)
                for timing in ('wall', 'cpu'):
                    same_number(sample[f'{op}_{timing}_s'], event[f'{timing}_s'], 'raw-to-record timing')
            for field, value in costs(enc_event, dec_event, rec['stored_bytes']).items():
                same_number(sample[field], value, field)
            require(sample['peak_memory_bytes'] == 1024 * max(enc_event['peak_rss_kib'], dec_event['peak_rss_kib']), 'memory summary mismatch')
        fields = set(rec['runs'][0]) - {'pair_id'}
        require(set(rec['medians']) == fields, 'median field mismatch')
        for field in fields:
            same_number(rec['medians'][field], statistics.median(s[field] for s in rec['runs']), 'median: ' + field)
    affinity = {e['assigned_cpu'] for e in events.values()}
    require(len(affinity) == 1, 'CPU affinity changed')
    return records, env


def all_pairs_within(candidate: dict, rival: dict) -> bool:
    """No zip truncation, median substitution, or opportunistic repeat selection."""
    left = unique_index(candidate['runs'], ('pair_id',), 'frontier candidate pairs')
    right = unique_index(rival['runs'], ('pair_id',), 'frontier rival pairs')
    expected = {(x,) for x in ROUND_IDS}
    require(set(left) == expected and set(right) == expected, 'frontier requires every registered pair')
    return all(number(left[key]['archive_cost_usd'], 'candidate cost') <=
               number(right[key]['archive_cost_usd'], 'rival cost') for key in expected)


def frontier(records: dict, inputs: dict, phases: tuple, subset: tuple) -> dict:
    """Offline diagnostic only: requires every sample of the SAME fixed plan."""
    rows, winners, family = [], Counter(), {}
    saved, rival_total = 0, 0
    for ident, inp in sorted(inputs.items()):
        rival_id = f"openzl_{inp['profile']}_9"
        rival = records['first'][(ident, rival_id)]
        feasible = []
        for pid in (*subset, rival_id):
            if all(all_pairs_within(records[phase][(ident, pid)], records[phase][(ident, rival_id)])
                   for phase in phases):
                feasible.append(pid)
        # Prefer rival on an exact byte tie; parity must never be counted as a win.
        win = min(feasible, key=lambda p: (records['first'][(ident, p)]['stored_bytes'], p != rival_id, p))
        size = records['first'][(ident, win)]['stored_bytes']
        delta = rival['stored_bytes'] - size
        winners[win] += 1
        f = family.setdefault(inp['family'], {'files': 0, 'strict_byte_wins': 0, 'rival_bytes': 0, 'selected_bytes': 0})
        f['files'] += 1
        f['strict_byte_wins'] += int(delta > 0)
        f['rival_bytes'] += rival['stored_bytes']
        f['selected_bytes'] += size
        saved += delta
        rival_total += rival['stored_bytes']
        rows.append({'input_id': ident, 'family': inp['family'], 'winner': win, 'stored_bytes': size,
                     'rival_bytes': rival['stored_bytes'], 'strict_byte_win': delta > 0, 'feasible_policies': feasible})
    return {'scope': 'OFFLINE_FIXED_CONFIG_FRONTIER_NOT_AN_ONLINE_SELECTOR', 'phases': list(phases),
            'winner_counts': dict(sorted(winners.items())), 'strict_byte_wins': sum(x['strict_byte_win'] for x in rows),
            'rival_total_bytes': rival_total, 'selected_total_bytes': rival_total - saved,
            'byte_weighted_saving_fraction': saved / rival_total, 'family_labels_not_independence_claims': family,
            'per_input': rows}


def audit_zip(path: Path, expected_sha: str = ARTIFACT_SHA256,
              expected_commit: str = SOURCE_COMMIT, expected_inputs: int = 61) -> dict:
    archive = EvidenceZip(path, expected_sha)
    try:
        manifest = archive.json('MANIFEST.json')
        require(isinstance(manifest, dict), 'invalid evidence manifest')
        for name, digest in manifest.items():
            require(safe_name(name) and bool(SHA_RE.fullmatch(digest)), 'bad manifest entry')
            require(sha256(archive.read(name)) == digest, 'manifest digest mismatch: ' + name)
        require(archive.read('COMMIT.txt').decode().strip() == expected_commit, 'wrong executed commit')
        prepared = archive.json('prepared/manifest.json')
        require(prepared['experiment'] == 'P2-E10' and prepared['split'] == 'development (observed sources)', 'unexpected exposure partition')
        ix = unique_index(prepared['inputs'], ('id',), 'input manifest')
        inputs = {key[0]: value for key, value in ix.items()}
        require(len(inputs) == expected_inputs, 'wrong input count')
        input_members = set()
        for ident, inp in inputs.items():
            require(safe_name(ident) and '/' not in ident, 'unsafe input identifier')
            require(inp['file'].startswith('inputs/') and safe_name(inp['file']), 'unsafe input file path')
            require(inp['view'] in ('npy', 'wav', 'csv'), 'unknown input view')
            name = 'prepared/' + inp['file']
            require(name not in input_members, 'duplicate input path')
            input_members.add(name)
            raw = archive.read(name)
            require(len(raw) == integer(inp['input_bytes'], 'input size', positive=True), 'input length mismatch')
            require(sha256(raw) == inp['input_sha256'], 'input digest mismatch')
        require(archive.names == set(manifest) | input_members | {'MANIFEST.json'}, 'unmanifested evidence member')
        require(not set(manifest) & input_members, 'unexpected duplicate manifest coverage')
        receipts = {}
        for line in archive.read('source-hashes.txt').decode().splitlines():
            digest, name = line.split(None, 1)
            require(name not in receipts and bool(SHA_RE.fullmatch(digest)), 'invalid source receipt')
            receipts[name] = digest
        for name, digest in SOURCE_RECEIPTS.items():
            require(receipts.get(name) == digest, 'source receipt mismatch: ' + name)
        expected_asan = {f"asan-ok {PurePosixPath(i['file']).name} {p}" for i in inputs.values() for p in RESEARCH}
        asan = archive.read('asan.log').decode().splitlines()
        require(len(asan) == len(expected_asan) and set(asan) == expected_asan, 'incomplete sanitizer-gate log')
        records, environments = {}, {}
        for phase in PHASES:
            records[phase], environments[phase] = audit_phase(archive, phase, inputs)
        for field in ('tools', 'python', 'platform', 'timing', 'candidate_acceptance'):
            require(environments['first'][field] == environments['confirmation'][field], 'cross-pass environment mismatch: ' + field)
        toolchain = archive.read('toolchain.txt').decode().splitlines()
        require(toolchain[:2] == ['Demo CLI for OpenZL. Version 0.3.0',
                '*** Zstandard CLI (64-bit) v1.5.7, by Yann Collet ***'], 'tool version mismatch')
        binary_receipts = {}
        for line in toolchain[2:]:
            digest, name = line.split(None, 1)
            require(name not in binary_receipts and bool(SHA_RE.fullmatch(digest)), 'invalid toolchain receipt')
            binary_receipts[name] = digest
        require(binary_receipts == {v['path']: v['sha256'] for v in environments['first']['tools'].values()},
                'toolchain/environment binary receipt mismatch')
        for key, rec in records['first'].items():
            other = records['confirmation'][key]
            for field in ('policy', 'input_sha256', 'stored_bytes', 'frame_sha256'):
                require(rec[field] == other[field], 'cross-pass frame/policy identity mismatch: ' + field)
        confirm = archive.json('confirmation.json')
        require(confirm == {'status': 'IDENTITIES_MATCH', 'configurations': len(inputs) * 8,
                            'completed': len(inputs) * 8, 'failed_profiles': 0}, 'confirmation receipt mismatch')
        result = {
            'status': 'PASSED_EVIDENCE_CONSISTENCY_ONLY', 'acceptance': 'NOT_EVALUATED',
            'artifact_sha256': archive.digest, 'executed_commit': expected_commit,
            'openzl_commit_from_executed_workflow': OPENZL_COMMIT,
            'manifest_entries_verified': len(manifest), 'input_files_separately_verified': len(inputs),
            'configurations_per_pass': len(inputs) * 8, 'matching_recorded_frame_identities': len(inputs) * 8,
            'timing_events_reconciled': len(inputs) * 8 * 3 * 2 * 2,
            'paired_cost_samples_recomputed': len(inputs) * 8 * 3 * 2,
            'recorded_sanitizer_gate_cases': len(expected_asan),
            'view_counts': dict(Counter(i['view'] for i in inputs.values())),
            'largest_input_bytes': max(i['input_bytes'] for i in inputs.values()),
            'modeled_cost_parameters': {'link_bytes_s': 125000000, 'reads': 3.3, 'retention_months': 1,
                'storage_usd_per_GB_month': 0.023, 'cpu_usd_per_hour': 3.89 / 72,
                'cpu_override_ratio': 1.03, 'egress_transfers': 0},
            'environment_receipts': environments,
            'limits': [
                'The frozen experiment uses previously observed development inputs, including derived NPY files; synthetic unit fixtures are not codec experiments.',
                'Hashes and logs corroborate recorded execution, not an independent re-decode: frames/binaries are absent.',
                'OpenZL le-* profiles consume complete NPY/WAV headers as numeric values; not header-separated native controls.',
                'The measured frontier is an offline oracle: cross-configuration selection/trials were not timed as one selector.',
                'Production-mode controls do not establish mandatory type-first planning.',
                'Checksums/frame metadata inside each CLI are timed; harness postverification is outside measured CLI calls.',
                'Matching source/binary receipts do not independently rebuild or authenticate the recorded binaries.',
                'Family labels are not proof of independent sources; no fresh source-disjoint validation is performed.',
            ],
            'frontiers': {},
        }
        subsets = {'all_codec_candidates': PRODUCTION + RESEARCH, 'production_only': PRODUCTION, 'research_only': RESEARCH}
        for subset, policies in subsets.items():
            result['frontiers'][subset] = {name: frontier(records, inputs, phases, policies)
                for name, phases in [('first', ('first',)), ('confirmation', ('confirmation',)), ('joint_six_pairs', PHASES)]}
        return result
    finally:
        archive.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('artifact', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = audit_zip(args.artifact)
    except (AuditError, KeyError, TypeError, OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f'AUDIT_FAILED: {exc}', file=sys.stderr)
        return 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('frontiers', 'environment_receipts', 'limits')}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
