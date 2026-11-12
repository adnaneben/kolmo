#!/usr/bin/env python3
"""P2-E1 analysis: build policy.py (schema 2) documents from p2e1 records and evaluate them.

Two views, never mixed:
  1. policy: the unchanged reference evaluator (research/codec/competitor-policy-v2/policy.py). On development
     data it must refuse acceptance; its rejection reasons are recorded verbatim.
  2. development feasibility: the same run_cost() model applied to the same paired runs, reporting whether each
     candidate's every paired cost fits the rival's and whether it is smaller. This is plumbing evidence only.

usage: analyze.py RECORDS_DIR MANIFEST.json PROTOCOL.md OUT.json [--machine ID]
"""
import hashlib, importlib.util, json, pathlib, platform, sys
from statistics import median

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('p2policy', ROOT / 'competitor-policy-v2' / 'policy.py')
policy = importlib.util.module_from_spec(spec); sys.modules["p2policy"] = policy; spec.loader.exec_module(policy)

SPEED_RIVAL, ARCHIVE_RIVAL = 'zstd3', None          # OpenZL is not measured in this run (BLOCKED)
CANDS = {'fast': ['kolmo_speed', 'kolmo_rt', 'kolmo_fast', 'kolmo_fast_q', 'kolmo_fast_Q', 'kolmo_fast_n', 'kolmo_fast_seq',
                  'stack_fast', 'stack_fast_q', 'stack_fast_Q', 'typed_fast', 'typed_stack_fast'],
         'hc': ['kolmo_eco', 'kolmo_hc', 'kolmo_hc_seq', 'stack_hc', 'kolmo_max', 'typed_hc', 'typed_stack_hc']}   # rt and fast share the zstd-3 rival
MEMORY_LIMIT = 1 << 30                               # registered in PROTOCOL.md


def sha(b): return hashlib.sha256(b).hexdigest()


def main():
    rec_dir, manifest, protocol, out = map(pathlib.Path, sys.argv[1:5])
    machine = sys.argv[sys.argv.index('--machine') + 1] if '--machine' in sys.argv else platform.node() + '/' + platform.processor()
    man = {x['name']: x for x in json.loads(manifest.read_text())['inputs']}
    contract = sha(protocol.read_bytes())
    model = policy.CostModel()
    report = {'experiment': 'P2-E1', 'partition': 'development', 'comparison_contract_sha256': contract,
              'machine_id': machine, 'cost_model': __import__('dataclasses').asdict(model), 'inputs': {}}
    for f in sorted(rec_dir.glob('*.jsonl')):
        rows = [json.loads(l) for l in f.read_text().splitlines() if l.strip()]
        name = rows[0]['input']; m = man[name]
        size = {r['candidate']: r['stored'] for r in rows if r['kind'] == 'p2e1_size'}
        mem = {r['candidate']: r['vmhwm_kb'] * 1024 for r in rows if r['kind'] == 'p2e1_mem'}
        runs = {}
        for r in rows:
            if r['kind'] == 'p2e1_time':
                runs.setdefault(r['candidate'], []).append({k: r[k] for k in ('pair_id', 'encode_wall_s', 'encode_cpu_s', 'decode_wall_s', 'decode_cpu_s')})
        descriptor = json.dumps({'container': m['name'].rsplit('.', 1)[-1], 'dtype': m['dtype'], 'width': m['width'],
                                 'head': m['head'], 'tail': m['tail'], 'count': m['count']}, sort_keys=True).encode()
        ctx = {'input_sha256': m['sha256'], 'comparison_contract_sha256': contract, 'machine_id': machine, 'threads': 1,
               'memory_limit_bytes': MEMORY_LIMIT, 'input_bytes': m['bytes']}

        def record(c, rival=False):
            emulated = c.startswith('stack') or c.startswith('typed_stack')
            research = c.startswith('typed')        # research container (flag, length, checksum, payload), not a production frame
            d = dict(ctx, id=c, stored_bytes=size[c], peak_memory_bytes=mem[c], runs=runs[c], byte_exact_verified=True,
                     measurement_scope='complete_pipeline_emulated_container' if emulated else ('complete_pipeline_research_container' if research else 'complete_pipeline'),
                     all_work_included=True, model_cost_accounted=True,
                     implementation_id='kolmo@HEAD+P2-E1/p2e1.c' if not c.startswith('zstd') else 'libzstd-1.5.7-vendored',
                     configuration_id=c)
            if rival:
                d.update(tool='zstd', level=3)
            else:
                d.update(typed_schema_sha256=sha(descriptor),
                         typing_precedes_planning=research,  # typed_*: declared type parsed first; production planner: own detection
                         legal_dag_verified=not emulated and not research,  # real production frames only
                         source_partition='development')
            return d

        per = {'bytes': m['bytes'], 'family': m['family'], 'sizes': size, 'peak_memory_bytes': mem, 'modes': {}}
        for mode, cands in CANDS.items():
            speed = mode in policy.SPEED
            doc = {'schema_version': 2, 'mode': mode, 'context': ctx,
                   'competitor': record(SPEED_RIVAL, rival=True) if speed else None,
                   'candidates': [record(c) for c in cands]}
            if not speed:
                doc.pop('competitor')
            try:
                pol = policy.select(doc)
                pol = {'status': pol['status'], 'rejected': pol.get('rejected', [])}
            except ValueError as e:
                pol = {'status': 'INVALID', 'error': str(e)}
            dev = {}
            rival = SPEED_RIVAL if speed else None
            for c in cands + ([rival] if rival else []) + (['zstd19'] if not speed else []):
                costs = {r['pair_id']: policy.run_cost(r, size[c], speed, model) for r in runs[c]}
                dev[c] = {'bytes': size[c], 'cost_median': median(costs.values()),
                          'encode_s_median': median(policy.charged(r['encode_wall_s'], r['encode_cpu_s']) for r in runs[c]),
                          'decode_s_median': median(policy.charged(r['decode_wall_s'], r['decode_cpu_s']) for r in runs[c]),
                          'costs': costs}
            if rival:
                for c in cands:
                    dev[c]['fits_rival_every_pair'] = all(dev[c]['costs'][p] <= dev[rival]['costs'][p] for p in dev[rival]['costs'])
                    dev[c]['smaller_than_rival'] = size[c] < size[rival]
                    dev[c]['bytes_vs_rival_pct'] = 100 * (size[c] / size[rival] - 1)
                    dev[c]['cost_vs_rival_pct'] = 100 * (dev[c]['cost_median'] / dev[rival]['cost_median'] - 1)
            per['modes'][mode] = {'policy': pol, 'development_feasibility': dev,
                                  'rival': rival or 'BLOCKED: OpenZL not measured in this run'}
        report['inputs'][name] = per
    pathlib.Path(out).write_text(json.dumps(report, indent=1) + '\n')
    # compact table: per input, the smallest speed candidate that fits zstd-3 on every pair, and kolmo_fast's position
    print(f"{'input':30} {'z3 bytes':>9} | {'best feasible speed cand':24} {'bytes vs z3':>11} | {'kolmo_fast vs z3: bytes':>23} {'cost':>7}")
    for name, per in report['inputs'].items():
        d = per['modes']['fast']['development_feasibility']
        feas = [c for c in CANDS['fast'] if d[c]['fits_rival_every_pair']]
        best = min(feas, key=lambda c: (d[c]['bytes'], d[c]['cost_median'])) if feas else None
        k = d['kolmo_fast']
        print(f"{name:30} {d['zstd3']['bytes']:9d} | {best or '-':24} {(d[best]['bytes_vs_rival_pct'] if best else float('nan')):+10.2f}% | "
              f"{k['bytes_vs_rival_pct']:+22.2f}% {k['cost_vs_rival_pct']:+6.0f}%")
    stat = {}
    for per in report['inputs'].values():
        for mode in ('fast', 'hc'):
            s = per['modes'][mode]['policy']['status']; stat[(mode, s)] = stat.get((mode, s), 0) + 1
    print('policy statuses:', stat)


if __name__ == '__main__':
    main()
