#!/usr/bin/env python3
"""Audit E03 repeated evidence and apply the predeclared development screen."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
from run import CHOICE, FIELDS, POLICIES


def load(path):
    return [json.loads(x) for x in path.read_text().splitlines() if x]


def index_rows(rows, expected):
    result = {}
    for row in rows:
        key = row['id'], row['policy']
        if key in result or row['policy'] not in POLICIES.values():
            raise ValueError('duplicate/unknown configuration')
        result[key] = row
    if len(result) != expected:
        raise ValueError('incomplete configuration matrix')
    return result


def audit(path, expected=232):
    rows = load(path / 'measurements.jsonl')
    indexed = index_rows(rows, expected)
    status = json.loads((path / 'status.json').read_text())
    if status['status'] != 'passed_E03_development_matrix_NOT_RELEASE_ACCEPTANCE':
        raise ValueError('matrix did not finish successfully')
    timings = {}
    for t in load(path / 'timings.jsonl'):
        timings.setdefault((t['id'], t['policy']), []).append(t)
    reps = json.loads((path / 'environment.json').read_text())['repeats']
    for key, row in indexed.items():
        ts = timings.get(key, [])
        if len(ts) != reps or sorted(t['repeat'] for t in ts) != list(range(reps)):
            raise ValueError('missing/duplicated timing repetition')
        if hashlib.sha256((path / row['frame_file']).read_bytes()).hexdigest() != row['frame_sha256']:
            raise ValueError('stored frame hash mismatch')
        if any(row[field] != statistics.median(t[field] for t in ts) for field in FIELDS):
            raise ValueError('reported median mismatch')
        if row['stored_bytes'] > row['baseline_bytes'] or row['sample_values'] > 1024:
            raise ValueError('no-expansion/sample-budget violation')
    return indexed


def compare(a, b):
    if set(a) != set(b):
        raise ValueError('confirmation configuration mismatch')
    for key in a:
        for field in (*CHOICE, 'input_sha256', 'frame_sha256', 'capture', 'order', 'gcd'):
            if a[key][field] != b[key][field]:
                raise ValueError(f'confirmation mismatch {key}/{field}')


def group(rows, ids, inputs):
    def selected(policy):
        return [rows[(name, policy)] for name in ids]
    base, opt = selected('native_base'), selected('sampled_optional')
    bsize = sum(r['stored_bytes'] for r in base); osize = sum(r['stored_bytes'] for r in opt)
    sums = lambda rr, field: sum(r[field] for r in rr)
    oracle = sum(min(v for k, v in inputs[name]['previous_sizes'].items()
                     if k.split(':')[1] in ('stored', 'huffman', 'rans')) for name in ids)
    encode = sums(opt, 'encode_ns') / sums(base, 'encode_ns')
    decode = sums(opt, 'decode_ns') / sums(base, 'decode_ns')
    saving = 100 * (1 - osize / bsize)
    result = dict(inputs=len(ids), original_bytes=sum(inputs[x]['input_bytes'] for x in ids),
        baseline_bytes=bsize, optional_bytes=osize, saving_pct=saving,
        optional_encode_ratio=encode, optional_decode_ratio=decode,
        attempted=sum(r['attempted_method'] != 0 for r in opt),
        accepted=sum(r['method'] != 0 for r in opt),
        tried_local=sum(r['attempted_method'] == 2 for r in opt),
        tried_patched=sum(r['attempted_method'] == 3 for r in opt),
        selected_local=sum(r['method'] == 2 for r in opt),
        selected_patched=sum(r['method'] == 3 for r in opt),
        sample_fraction_of_encoding=sums(opt, 'sample_ns') / sums(opt, 'encode_ns'),
        optional_trial_fraction_of_encoding=sums(opt, 'optional_ns') / sums(opt, 'encode_ns'),
        archived_offline_oracle_bytes=oracle,
        oracle_headroom_captured=(bsize - osize) / (bsize - oracle) if bsize > oracle else None,
        provisional_screen=saving >= 3 and encode <= 1.25 and decode <= 1.10)
    result['policies'] = {}
    for policy in POLICIES.values():
        rr = selected(policy)
        result['policies'][policy] = dict(stored_bytes=sum(r['stored_bytes'] for r in rr),
            sum_median_encode_ns=sums(rr, 'encode_ns'), sum_median_decode_ns=sums(rr, 'decode_ns'),
            post_exec_rss_min_kib=min(r['post_exec_image_peak_rss_kib'] for r in rr),
            post_exec_rss_max_kib=max(r['post_exec_image_peak_rss_kib'] for r in rr))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--prepared', type=Path, required=True)
    p.add_argument('--first', type=Path, required=True)
    p.add_argument('--confirmation', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    first, second = audit(a.first), audit(a.confirmation)
    compare(first, second)
    inputs = {x['id']: x for x in json.loads((a.prepared / 'inputs.json').read_text())}
    groups = {
        'C_real': [k for k, v in inputs.items() if v['source'] == 'C' and v['kind'] == 'real_source_derived'],
        'C_synthetic': [k for k, v in inputs.items() if v['source'] == 'C' and v['kind'] == 'synthetic'],
    }
    for size in ('full', 'small128', 'small1024'):
        groups['D_' + size] = [k for k, v in inputs.items() if v['source'] == 'D' and v['size_class'] == size]
    families = sorted({v['family'] for v in inputs.values() if v['kind'] == 'real_source_derived'})
    for family in families:
        groups['family:' + family] = [k for k, v in inputs.items() if v['family'] == family and v['size_class'] == 'full']
    output = dict(status='completed_development_NOT_RELEASE_ACCEPTANCE', configurations_per_pass=len(first),
                  inputs=len(inputs), repeat_identity_passed=True, groups={})
    for name, ids in groups.items():
        output['groups'][name] = {'first': group(first, ids, inputs), 'confirmation': group(second, ids, inputs)}
    winners = [name for name, r in output['groups'].items() if name.startswith('family:')
               and r['first']['provisional_screen'] and r['confirmation']['provisional_screen']]
    output['provisional_families'] = winners
    output['decision'] = 'eligible_for_FRESH_validation_NOT_promotion' if len(winners) >= 2 else 'do_not_promote_this_selector'
    output['limitations'] = [
        'E02C/E02D are observed development; no new holdout was evaluated',
        'nested prefixes/correlated columns are not independent datasets',
        'forced typed numeric research envelope, not automatic production integration',
        'native HUF code uses vendored zstd; not a dependency-free binary',
        'shared Linux runner; medians are descriptive, not confidence-qualified M5 results',
        'process-image VmHWM includes runtime/input/capture/reference/output buffers',
        'independent reference verifies packing/integer inverse, not entropy internals',
        'archived complete-product controls are size context, not current timing comparators']
    a.output.write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    main()
