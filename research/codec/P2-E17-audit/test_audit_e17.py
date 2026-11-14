"""Synthetic evidence only; no codec is executed and no performance is asserted."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import statistics
import tempfile
import unittest
import warnings
import zipfile

import audit_e17 as a


def encode(value):
    return (json.dumps(value, allow_nan=False) + '\n').encode()


def fixture():
    """One tiny CSV and eight explicitly synthetic fixed-configuration records."""
    raw = b'a,b\n1,2\n'
    inp = dict(id='fixture', family='SYNTHETIC', view='csv', profile='csv',
               file='inputs/fixture.csv', input_bytes=len(raw), input_sha256=a.sha256(raw), origin='synthetic')
    tools = {name: {'path': 'bin/' + name, 'sha256': a.sha256(name.encode())}
             for name in ('zstd', 'openzl', 'kolmo', 'tb')}
    members = {
        'COMMIT.txt': (a.SOURCE_COMMIT + '\n').encode(),
        'prepared/inputs/fixture.csv': raw,
        'prepared/manifest.json': encode({'experiment': 'P2-E10', 'split': 'development (observed sources)', 'inputs': [inp]}),
        'source-hashes.txt': ''.join(f'{digest}  {name}\n' for name, digest in a.SOURCE_RECEIPTS.items()).encode(),
        'toolchain.txt': ('Demo CLI for OpenZL. Version 0.3.0\n*** Zstandard CLI (64-bit) v1.5.7, by Yann Collet ***\n'
            + ''.join(f"{v['sha256']}  {v['path']}\n" for v in tools.values())).encode(),
        'asan.log': ''.join(f'asan-ok fixture.csv {p}\n' for p in a.RESEARCH).encode(),
        'confirmation.json': encode({'status': 'IDENTITIES_MATCH', 'configurations': 8, 'completed': 8, 'failed_profiles': 0}),
    }
    for phase in a.PHASES:
        members[f'{phase}/environment.json'] = encode({'tools': tools, 'python': 'SYNTHETIC', 'platform': ['SYNTHETIC'],
            'seed': 20261112 if phase == 'first' else 20261113, 'timing': a.TIMING_BOUNDARY,
            'candidate_acceptance': 'NOT_EVALUATED_OBSERVED_BASELINE_CONTROLS'})
        records, warmups, events = [], [], []
        for pid, (tool, enc, dec) in a.expected_policies(inp, tools).items():
            pol = dict(id=pid, tool=tool, enc=enc, dec=dec)
            if tool == 'openzl':
                pol.update(profile='csv', level=9, training_regime='untrained_per_source_builtin_models_allowed')
            if tool == 'zstd':
                pol.update(level=3)
            if tool == 'tb':
                pol.update(research=True, mode_family='archive', decoder='production kolmo CLI')
            size = 100 if tool == 'openzl' else 120
            if pid == 'tp_pent':
                size = 80
            def event(op, pair=None):
                base = enc if op == 'encode' else dec
                root = f'ev/{phase}/fixture/{pid}/'
                src = '/fixture/prepared/inputs/fixture.csv' if op == 'encode' else root + 'retained.frame'
                dst = root + (('retained.frame' if op == 'encode' else 'warmup.decoded') if pair is None else 'tmp-' + op)
                cmd = base + ([src, dst] if tool in ('kolmo', 'tb') else [src, '-o', dst])
                r = dict(command=cmd, returncode=0, timed_out=False, wall_s=0.01, cpu_s=0.009,
                    peak_rss_kib=10, assigned_cpu=0, thread_constraint='single_CPU_affinity_and_thread_environment',
                    rss_scope='individual_exec_child_process_not_codec_scratch')
                if pair is not None:
                    r.update(input_id='fixture', policy_id=pid, operation=op, pair_id=pair)
                return r
            rec = dict(input_id='fixture', family=inp['family'], view=inp['view'], policy=pol,
                input_bytes=inp['input_bytes'], input_sha256=inp['input_sha256'], warmup_encode=event('encode'),
                warmup_decode=event('decode'), status='READY', stored_bytes=size, frame_sha256=a.sha256(pid.encode()))
            warmups.append(copy.deepcopy(rec))
            samples = []
            for pair in a.ROUND_IDS:
                e, d = event('encode', pair), event('decode', pair)
                events.extend((e, d))
                sample = dict(pair_id=pair, encode_wall_s=e['wall_s'], encode_cpu_s=e['cpu_s'],
                    decode_wall_s=d['wall_s'], decode_cpu_s=d['cpu_s'], peak_memory_bytes=10240,
                    **a.costs(e, d, size))
                samples.append(sample)
            rec.update(status='MEASURED_DEVELOPMENT_CONTROL', byte_exact_verified=True, runs=samples,
                medians={k: statistics.median(s[k] for s in samples) for k in samples[0] if k != 'pair_id'})
            records.append(rec)
        members[f'{phase}/records.json'] = encode(dict(status='BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE',
            records=records, completed=8, failed_profiles=0, rounds=3))
        members[f'{phase}/warmups.json'] = encode(warmups)
        members[f'{phase}/timings.jsonl'] = b''.join(encode(e) for e in events)
    return members


def mutate_json(members, path, function):
    obj = json.loads(members[path]); function(obj); members[path] = encode(obj)


def mutate_events(members, function, phase='first'):
    name = phase + '/timings.jsonl'
    obj = [json.loads(x) for x in members[name].splitlines()]
    function(obj); members[name] = b''.join(encode(x) for x in obj)


def write_zip(path, members, manifest_override=None):
    manifest = {k: a.sha256(v) for k, v in members.items() if not k.startswith('prepared/inputs/') and k != 'MANIFEST.json'}
    if manifest_override:
        manifest_override(manifest)
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for name, value in members.items():
            z.writestr(name, value)
        z.writestr('MANIFEST.json', encode(manifest))
    return a.sha256(path.read_bytes())


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'fixture.zip'
        self.members = fixture()

    def audit(self):
        digest = write_zip(self.path, self.members)
        return a.audit_zip(self.path, expected_sha=digest, expected_inputs=1)

    def rejects(self, pattern):
        with self.assertRaisesRegex(a.AuditError, pattern):
            self.audit()

    def test_valid_complete_fixture(self):
        report = self.audit()
        self.assertEqual(report['status'], 'PASSED_EVIDENCE_CONSISTENCY_ONLY')
        self.assertEqual(report['acceptance'], 'NOT_EVALUATED')
        self.assertEqual(report['timing_events_reconciled'], 96)
        self.assertEqual(report['paired_cost_samples_recomputed'], 48)
        self.assertEqual(report['frontiers']['research_only']['joint_six_pairs']['strict_byte_wins'], 1)

    def test_outer_digest(self):
        write_zip(self.path, self.members)
        with self.assertRaisesRegex(a.AuditError, 'artifact SHA256'):
            a.audit_zip(self.path, expected_sha='0' * 64, expected_inputs=1)

    def test_manifest_digest(self):
        digest = write_zip(self.path, self.members, lambda m: m.update({'COMMIT.txt': '0' * 64}))
        with self.assertRaisesRegex(a.AuditError, 'manifest digest'):
            a.audit_zip(self.path, expected_sha=digest, expected_inputs=1)

    def test_unmanifested_evidence(self):
        digest = write_zip(self.path, {**self.members, 'extra.txt': b'x'}, lambda m: m.pop('extra.txt'))
        with self.assertRaisesRegex(a.AuditError, 'unmanifested'):
            a.audit_zip(self.path, expected_sha=digest, expected_inputs=1)

    def test_traversal_rejected(self):
        self.members['../escape'] = b'x'
        self.rejects('unsafe archive')

    def test_duplicate_archive_member(self):
        write_zip(self.path, self.members)
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(self.path, 'a') as z:
                z.writestr('COMMIT.txt', b'x')
        with self.assertRaisesRegex(a.AuditError, 'duplicate archive'):
            a.audit_zip(self.path, expected_sha=a.sha256(self.path.read_bytes()), expected_inputs=1)

    def test_symlink_archive_member(self):
        write_zip(self.path, self.members)
        with zipfile.ZipFile(self.path, 'a') as z:
            info = zipfile.ZipInfo('link'); info.external_attr = 0o120777 << 16
            z.writestr(info, '../outside')
        with self.assertRaisesRegex(a.AuditError, 'symlink'):
            a.audit_zip(self.path, expected_sha=a.sha256(self.path.read_bytes()), expected_inputs=1)

    def test_wrong_source_commit(self):
        self.members['COMMIT.txt'] = b'0' * 40
        self.rejects('wrong executed commit')

    def test_input_hash(self):
        self.members['prepared/inputs/fixture.csv'] = b'x' * len(self.members['prepared/inputs/fixture.csv'])
        self.rejects('input digest')

    def test_exposure_label(self):
        mutate_json(self.members, 'prepared/manifest.json', lambda x: x.update(split='fresh_holdout'))
        self.rejects('exposure partition')

    def test_source_receipt(self):
        self.members['source-hashes.txt'] = self.members['source-hashes.txt'].replace(b'7e477271', b'00000000')
        self.rejects('source receipt mismatch')

    def test_missing_sanitizer_gate_case(self):
        self.members['asan.log'] = self.members['asan.log'].split(b'\n', 1)[1]
        self.rejects('sanitizer-gate')

    def test_duplicate_record(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'].append(x['records'][0]))
        self.rejects('records: duplicate')

    def test_missing_rival(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'].pop(1))
        self.rejects('policy records')

    def test_duplicate_event(self):
        mutate_events(self.members, lambda x: x.append(x[0]))
        self.rejects('timings: duplicate')

    def test_missing_event(self):
        mutate_events(self.members, lambda x: x.pop())
        self.rejects('timing event')

    def test_failed_timed_operation(self):
        mutate_events(self.members, lambda x: x[0].update(returncode=1))
        self.rejects('failed timed operation')

    def test_timeout_with_zero_exit_code(self):
        mutate_events(self.members, lambda x: x[0].update(timed_out=True))
        self.rejects('timed-out')

    def test_nan_in_raw_json(self):
        name = 'first/timings.jsonl'
        self.members[name] = self.members[name].replace(b'"wall_s": 0.01', b'"wall_s": NaN', 1)
        self.rejects('non-finite JSON')

    def test_false_roundtrip_flag(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][0].update(byte_exact_verified=False))
        self.rejects('unverified record')

    def test_corrupt_cost_summary(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][0]['runs'][0].update(archive_cost_usd=1.0))
        self.rejects('archive_cost_usd: numeric mismatch')

    def test_corrupt_raw_timing(self):
        mutate_events(self.members, lambda x: x[0].update(wall_s=0.02))
        self.rejects('raw-to-record timing')

    def test_wrong_memory_summary(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][0]['runs'][0].update(peak_memory_bytes=1))
        self.rejects('memory summary')

    def test_wrong_median(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][0]['medians'].update(encode_wall_s=1))
        self.rejects('median: encode_wall_s')

    def test_wrong_pair_id(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][0]['runs'][0].update(pair_id='9'))
        self.rejects('paired sample IDs')

    def test_unexpected_rival_level(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][1]['policy'].update(level=6))
        self.rejects('rival profile/level')

    def test_wrong_policy_command(self):
        mutate_json(self.members, 'first/records.json', lambda x: x['records'][0]['policy']['enc'].append('--bad-flag'))
        self.rejects('policy registry')

    def test_wrong_raw_command(self):
        mutate_events(self.members, lambda x: x[0]['command'].append('--bad-flag'))
        self.rejects('command differs')

    def test_reject_leaf_only_input(self):
        mutate_events(self.members, lambda x: x[0]['command'].__setitem__(5, '/fixture/only-payload.bin'))
        self.rejects('wrong complete encoder input')

    def test_toolchain_receipt_mismatch(self):
        self.members['toolchain.txt'] = self.members['toolchain.txt'].replace(a.sha256(b'tb').encode(), b'0' * 64)
        self.rejects('toolchain/environment')

    def test_environment_mismatch(self):
        mutate_json(self.members, 'confirmation/environment.json', lambda x: x.update(platform=['OTHER_MACHINE']))
        self.rejects('cross-pass environment')

    def test_boundary_mismatch(self):
        mutate_json(self.members, 'first/environment.json', lambda x: x.update(timing='leaf_only'))
        self.rejects('measurement boundary')

    def test_frame_mismatch_across_passes(self):
        mutate_json(self.members, 'confirmation/records.json', lambda x: x['records'][0].update(frame_sha256='0' * 64))
        mutate_json(self.members, 'confirmation/warmups.json', lambda x: x[0].update(frame_sha256='0' * 64))
        self.rejects('cross-pass frame/policy identity')

    def test_confirmation_receipt_mismatch(self):
        mutate_json(self.members, 'confirmation.json', lambda x: x.update(completed=7))
        self.rejects('confirmation receipt')

    def test_wrong_repeat_count(self):
        mutate_json(self.members, 'first/records.json', lambda x: x.update(rounds=2))
        self.rejects('repeat count')

    def test_duplicate_json_key(self):
        self.members['confirmation.json'] = b'{"status":"x", "status":"y"}'
        self.rejects('duplicate JSON key')

    def test_cpu_override_strict_boundary(self):
        self.assertEqual(a.charged(1.0, 1.03), 1.0)
        self.assertEqual(a.charged(1.0, 1.030000001), 1.030000001)
        self.assertEqual(a.charged(1.0, 0.0), 1.0)

    def test_negative_nonfinite_and_bool_rejected(self):
        for bad in (-1, float('inf'), float('nan'), True, '1'):
            with self.subTest(bad=bad), self.assertRaises(a.AuditError):
                a.charged(bad, 0.0)

    def test_joint_frontier_needs_all_six_pairs(self):
        inp = {'fixture': {'profile': 'csv', 'family': 'SYNTHETIC'}}
        def rec(size, values):
            return {'stored_bytes': size, 'runs': [{'pair_id': str(i), 'archive_cost_usd': v} for i, v in enumerate(values)]}
        first = {('fixture', 'openzl_csv_9'): rec(100, [1, 1, 1]), ('fixture', 'tp_pent'): rec(80, [0.5, 0.5, 0.5])}
        second = copy.deepcopy(first); second[('fixture', 'tp_pent')] = rec(80, [0.5, 0.5, 1.01])
        r = a.frontier({'first': first, 'confirmation': second}, inp, a.PHASES, ('tp_pent',))
        self.assertEqual(r['strict_byte_wins'], 0)
        self.assertEqual(r['winner_counts'], {'openzl_csv_9': 1})

    def test_exact_size_tie_is_not_a_win(self):
        inp = {'fixture': {'profile': 'csv', 'family': 'SYNTHETIC'}}
        rec = {'stored_bytes': 100, 'runs': [{'pair_id': str(i), 'archive_cost_usd': 1} for i in range(3)]}
        data = {('fixture', 'openzl_csv_9'): rec, ('fixture', 'tp_pent'): copy.deepcopy(rec)}
        r = a.frontier({'first': data, 'confirmation': data}, inp, a.PHASES, ('tp_pent',))
        self.assertEqual(r['strict_byte_wins'], 0)
        self.assertEqual(r['winner_counts'], {'openzl_csv_9': 1})

    def test_frontier_rejects_truncated_pairs(self):
        valid = {'runs': [{'pair_id': str(i), 'archive_cost_usd': 1.0} for i in range(3)]}
        invalid = copy.deepcopy(valid); invalid['runs'].pop()
        with self.assertRaisesRegex(a.AuditError, 'every registered pair'):
            a.all_pairs_within(invalid, valid)

    def test_safe_paths(self):
        for path in ('/abs', '../x', 'a/../b', 'a\\b', 'a//b', 'a/./b', 'C:x', ''):
            with self.subTest(path=path):
                self.assertFalse(a.safe_name(path))


if __name__ == '__main__':
    unittest.main()
