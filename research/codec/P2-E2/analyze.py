#!/usr/bin/env python3
"""P2-E2 analysis (adapted from P2-E1/analyze.py): build policy.py (schema 2) documents from p2e1 records and evaluate them.

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
CANDS = {'fast': ['typed_fast', 'tf_prod3', 'tf_prod3_pinv', 'tf_huf', 'tf_planes_huf', 'tf_planes_huf_pinv', 'tf_planes_ent',
                  'tf_planes_prod3g', 'tf_planes_store', 'tf_prod3_g', 'tf_planes_huf_g', 'tf_prod3_pg', 'tf_planes_huf_pg',
                  'tf_sel', 'tf_sel_pg', 'tf_planes_huf_pg16', 'tf_huf_pg16'],
         'hc': ['typed_hc', 'tf_prod19', 'tf_planes_prod19', 'tf_planes_ent19']}
MEMORY_LIMIT = 1 << 30                               # registered in PROTOCOL.md


def sha(b): return hashlib.sha256(b).hexdigest()


def main():
    rec_dir, manifest, protocol, out = map(pathlib.Path, sys.argv[1:5])
    machine = sys.argv[sys.argv.index('--machine') + 1] if '--machine' in sys.argv else platform.node() + '/' + platform.processor()
    man = {x['name']: x for x in json.loads(manifest.read_text())['inputs']}
    contract = sha(protocol.read_bytes())
    model = policy.CostModel()
    report = {'experiment': 'P2-E2', 'partition': 'development', 'comparison_contract_sha256': contract,
              'machine_id': machine, 'cost_model': __import__('dataclasses').asdict(model), 'inputs': {}}
    for f in sorted(rec_dir.glob('*.jsonl')):
        rows = [json.loads(l) for l in f.read_text().splitlines() if l.strip()]
        rows = [r for r in rows if r['kind'] != 'p2e2_identity'] + [r for r in rows if r['kind'] == 'p2e2_identity']
        name = rows[0]['input']; m = man[name]
        ident = [r for r in rows if r['kind'] == 'p2e2_identity']
        size = {r['candidate']: r['stored'] for r in rows if r['kind'] == 'p2e2_size'}
        mem = {r['candidate']: r['vmhwm_kb'] * 1024 for r in rows if r['kind'] == 'p2e2_mem'}
        runs = {}
        for r in rows:
            if r['kind'] == 'p2e2_time':
                runs.setdefault(r['candidate'], []).append({k: r[k] for k in ('pair_id', 'encode_wall_s', 'encode_cpu_s', 'decode_wall_s', 'decode_cpu_s')})
        descriptor = json.dumps({'container': m['name'].rsplit('.', 1)[-1], 'dtype': m['dtype'], 'width': m['width'],
                                 'head': m['head'], 'tail': m['tail'], 'count': m['count']}, sort_keys=True).encode()
        ctx = {'input_sha256': m['sha256'], 'comparison_contract_sha256': contract, 'machine_id': machine, 'threads': 1,
               'memory_limit_bytes': MEMORY_LIMIT, 'input_bytes': m['bytes']}

        def record(c, rival=False):
            emulated = False                        # every P2-E2 candidate is a real serialized research container
            research = not c.startswith('zstd')
            d = dict(ctx, id=c, stored_bytes=size[c], peak_memory_bytes=mem[c], runs=runs[c], byte_exact_verified=True,
                     measurement_scope='complete_pipeline_emulated_container' if emulated else ('complete_pipeline_research_container' if research else 'complete_pipeline'),
                     all_work_included=True, model_cost_accounted=True,
                     implementation_id='kolmo@HEAD+P2-E2/p2e2.c' if not c.startswith('zstd') else 'libzstd-1.5.7-vendored',
                     configuration_id=c)
            if rival:
                d.update(tool='zstd', level=3)
            else:
                d.update(typed_schema_sha256=sha(descriptor),
                         typing_precedes_planning=research,  # typed_*: declared type parsed first; production planner: own detection
                         legal_dag_verified=not emulated and not research,  # real production frames only
                         source_partition='development')
            return d

        per = {'bytes': m['bytes'], 'family': m['family'], 'sizes': size, 'peak_memory_bytes': mem, 'identity': ident, 'modes': {}}
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
    # compact table: fits-every-pair and smaller counts per speed candidate
    print(f"{'candidate':22} {'fits':>5} {'fits+smaller':>12} {'worst bytes vs z3':>18}")
    for c in CANDS['fast']:
        v = [p['modes']['fast']['development_feasibility'][c] for p in report['inputs'].values()]
        print(f"{c:22} {sum(x['fits_rival_every_pair'] for x in v):5d} {sum(x['fits_rival_every_pair'] and x['smaller_than_rival'] for x in v):12d} {max(x['bytes_vs_rival_pct'] for x in v):+17.1f}%")
    stat = {}
    for per in report['inputs'].values():
        for mode in ('fast', 'hc'):
            s = per['modes'][mode]['policy']['status']; stat[(mode, s)] = stat.get((mode, s), 0) + 1
    print('policy statuses:', stat)


if __name__ == '__main__':
    main()
