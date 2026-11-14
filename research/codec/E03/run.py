#!/usr/bin/env python3
"""Costed E03 DEVELOPMENT matrix; no final validation or release claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent / 'E02C'))
from reference import restore
POLICIES = {0: 'native_base', 1: 'sampled_optional', 2: 'huffman_control', 3: 'rans_control'}
FIELDS = ('encode_ns', 'decode_ns', 'encode_cpu_ns', 'decode_cpu_ns',
          'baseline_coding_ns', 'sample_ns', 'optional_ns')
CHOICE = ('stored_bytes', 'baseline_bytes', 'baseline_codec', 'method', 'actual_codec',
          'attempted_method', 'entropy_calls', 'sample_values', 'sample_original_bytes',
          'sample_local_bytes', 'sample_patched_bytes', 'represented_bytes', 'residual_bytes')


def sha(b):
    return hashlib.sha256(b).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def expected_size(case, policy, attempted):
    sizes = case['previous_sizes']
    coders = ('stored', 'huffman') if policy == 2 else ('stored', 'rans') if policy == 3 else ('stored', 'huffman', 'rans')
    base = min(sizes['0:' + b] for b in coders)
    best = min([base] + [sizes[str(attempted) + ':' + b] for b in coders]) if attempted else base
    return base, best


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--prepared', type=Path, required=True)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--repeats', type=int, default=7)
    p.add_argument('--seed', type=int, default=303031)
    p.add_argument('--small-only', action='store_true')
    a = p.parse_args()
    if not 1 <= a.repeats <= 100:
        p.error('repeats outside limits')
    prepared, probe, out = a.prepared.resolve(), a.probe.resolve(), a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    scratch = out / 'scratch'; scratch.mkdir()
    frames = out / 'frames'; frames.mkdir()
    cases = json.loads((prepared / 'inputs.json').read_text())
    if a.small_only:
        cases = [c for c in cases if c['input_bytes'] <= 8192]
    tasks = [(case, policy) for case in cases for policy in POLICIES]
    random.Random(a.seed).shuffle(tasks)
    state = dict(status='running', scope='observed development; NOT fresh validation', configurations=len(tasks))
    save(out / 'status.json', state)
    environment = dict(platform=platform.platform(), python=platform.python_version(),
        cpu=Path('/proc/cpuinfo').read_text().split('\n\n')[0], repeats=a.repeats, shuffle_seed=a.seed,
        probe_sha256=sha(probe.read_bytes()),
        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip())
    save(out / 'environment.json', environment)
    rows, capture_records = [], {}
    try:
        with (out / 'timings.jsonl').open('w') as times, (out / 'commands.jsonl').open('w') as commands:
            for case, policy in tasks:
                name = case['id']; source = prepared / 'corpus' / (name + '.npy')
                original = source.read_bytes()
                if sha(original) != case['input_sha256']:
                    raise ValueError('Changed prepared input')
                prefix = scratch / (name + '-' + str(policy))
                cmd = [str(probe), 'bench', str(source), str(case['width']), str(case['head']),
                       str(policy), str(a.repeats), str(prefix)]
                start = time.perf_counter_ns()
                proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300,
                    env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                             UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
                wall = time.perf_counter_ns() - start
                commands.write(json.dumps(dict(argv=cmd, returncode=proc.returncode, process_batch_wall_ns=wall,
                                                stdout=proc.stdout, stderr=proc.stderr)) + '\n'); commands.flush()
                if proc.returncode:
                    raise RuntimeError(f'{name}/{policy}: {proc.stderr[-4000:]}')
                records = [json.loads(line) for line in proc.stdout.splitlines() if line.startswith('{')]
                ss = [r for r in records if r['type'] == 'summary']; ts = [r for r in records if r['type'] == 'timing']
                if len(ss) != 1 or len(ts) != a.repeats:
                    raise ValueError('Incomplete native timings')
                summary = {k: v for k, v in ss[0].items() if k != 'type'}
                if summary['sample_values'] > 1024 or summary['entropy_calls'] not in (1, 2, 4):
                    raise ValueError('Selection budget exceeded')
                attempted = summary['attempted_method']
                if attempted not in (0, 2, 3) or policy != 1 and attempted:
                    raise ValueError('Illegal representation trial')
                if summary['entropy_calls'] != (1 if policy in (2, 3) else 4 if attempted else 2):
                    raise ValueError('Wrong coder-call accounting')
                base, best = expected_size(case, policy, attempted)
                if summary['baseline_bytes'] != base or summary['stored_bytes'] != best:
                    raise ValueError('Actual selector differs from fixed-control minimum')
                data = {ext: Path(str(prefix) + '.' + ext).read_bytes()
                        for ext in ('frame', 'represented', 'decoded', 'residual', 'params', 'rest')}
                recovered, residual, meta = restore(data['frame'], data['represented'])
                if recovered != original or data['decoded'] != original or residual != data['residual']:
                    raise ValueError('Independent full-input reconstruction failed')
                hashes = {key: sha(data[key]) for key in ('params', 'residual', 'rest')}
                if hashes != case['capture']:
                    raise ValueError('Capture changed from frozen E02C/E02D')
                capture_records[name] = hashes
                frame_name = name + '-' + str(policy) + '.frame'
                (frames / frame_name).write_bytes(data['frame'])
                # Reconstructible manifests retain provenance; bulky intermediate buffers are not duplicated.
                for ext in data:
                    Path(str(prefix) + '.' + ext).unlink()
                for t in ts:
                    times.write(json.dumps(dict(id=name, policy=POLICIES[policy], **t)) + '\n')
                row = dict(id=name, source=case['source'], family=case['family'], kind=case['kind'],
                    size_class=case['size_class'], policy=POLICIES[policy], **summary,
                    input_sha256=case['input_sha256'], frame_sha256=sha(data['frame']),
                    frame_file='frames/' + frame_name, capture=hashes,
                    process_batch_wall_ns=wall, order=meta['order'], gcd=meta['gcd'])
                row.update({field: statistics.median(t[field] for t in ts) for field in FIELDS})
                rows.append(row)
                with (out / 'measurements.jsonl').open('a') as f:
                    f.write(json.dumps(row) + '\n')
        if len(rows) != len(tasks):
            raise ValueError('Incomplete matrix')
        save(out / 'capture_hashes.json', capture_records)
        state.update(status='passed_E03_development_matrix_NOT_RELEASE_ACCEPTANCE',
                     inputs=len(cases), configurations=len(rows), timing_pairs=len(rows) * a.repeats)
        save(out / 'status.json', state); print(json.dumps(state))
    except BaseException as exc:
        state.update(status='failed_or_interrupted', error=str(exc)); save(out / 'status.json', state); raise


if __name__ == '__main__':
    main()
