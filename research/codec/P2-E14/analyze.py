#!/usr/bin/env python3
"""P2-E14 analysis (per family and total; no pooled claims). Speed: fits zstd-3's speed cost on every pair, and
smaller. Archive: smallest pipeline inside the OpenZL typed level-9 profile's archive cost on every pair."""
import collections, json, lzma, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
SP = ['tp_phuf', 'tp_phuf_pg16', 'tp_prod3', 'tb_best', 'tb_lazy', 'tp_pent', 'tp_arch19', 'kolmo_fast']
def load(run):
    f = root / run / 'records.json.xz'
    return json.loads(lzma.decompress(f.read_bytes()) if f.exists() else (root / run / 'records.json').read_text())['records']
out = {}
for run in ('first', 'confirmation'):
    by = {}
    for r in load(run): by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    fam = collections.defaultdict(lambda: dict(n=0, speed={p: [0, 0] for p in SP}, speed_best=collections.Counter(), archive=collections.Counter()))
    tot = dict(n=0, speed={p: [0, 0] for p in SP}, speed_best=collections.Counter(), archive=collections.Counter())
    for inp, P in by.items():
        z = P['zstd_3']; F = fam[z['family']]
        for T in (F, tot): T['n'] += 1
        feas = []
        for p in SP:
            r = P[p]; fit = all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs']))
            for T in (F, tot): T['speed'][p][0] += fit and r['stored_bytes'] < z['stored_bytes']; T['speed'][p][1] += fit
            if fit: feas.append((r['stored_bytes'], p))
        feas.sort(); w = feas[0][1] if feas and feas[0][0] < z['stored_bytes'] else 'zstd_3'
        for T in (F, tot): T['speed_best'][w] += 1
        o = [P[k] for k in P if k.startswith('openzl_')][0]
        af = sorted((r['stored_bytes'], p) for p, r in P.items() if not p.startswith('openzl_') and all(a['archive_cost_usd'] <= b['archive_cost_usd'] for a, b in zip(r['runs'], o['runs'])))
        wa = af[0][1] if af and af[0][0] < o['stored_bytes'] else 'openzl_9'
        for T in (F, tot): T['archive'][wa] += 1
    conv = lambda v: dict(n=v['n'], speed_fit_and_smaller={p: v['speed'][p][0] for p in SP}, speed_fit={p: v['speed'][p][1] for p in SP},
                          speed_smallest_feasible=dict(v['speed_best']), archive_smallest_feasible=dict(v['archive']))
    out[run] = dict(total=conv(tot), families={f: conv(v) for f, v in sorted(fam.items())})
print(json.dumps(out, indent=1))
