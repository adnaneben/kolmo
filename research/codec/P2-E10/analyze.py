#!/usr/bin/env python3
"""P2-E10 per-family analysis from the recovered records (research/codec/P2-E10/results/*/records.json.xz).
Speed: fits zstd-3's speed cost on every pair and smaller. Archive: smallest pipeline fitting the OpenZL typed level-9
profile's archive cost on every pair (OpenZL applied to the whole original file). Families reported separately."""
import collections, json, lzma, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
SP = ['tf_planes_huf', 'tf_planes_huf_pg16', 'mcsv_planes_huf', 'kolmo_fast', 'kolmo_fast_quick', 'kolmo_hc', 'kolmo_max', 'tf_prod19', 'tf_planes_ent19']
out = {}
for run in ('first', 'confirmation'):
    R = json.loads(lzma.decompress((root / run / 'records.json.xz').read_bytes()))['records']; by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    fam = collections.defaultdict(lambda: dict(n=0, speed={p: [0, 0] for p in SP}, archive=collections.Counter()))
    for inp, P in by.items():
        z = P['zstd_3']; F = fam[z['family']]; F['n'] += 1
        for p in SP:
            r = P[p]; fit = all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs']))
            F['speed'][p][0] += fit and r['stored_bytes'] < z['stored_bytes']; F['speed'][p][1] += fit
        prof = [k for k in P if k.startswith('openzl_') and k != 'openzl_serial_9'][0]; o = P[prof]
        feas = sorted((r['stored_bytes'], p) for p, r in P.items() if p != prof and all(a['archive_cost_usd'] <= b['archive_cost_usd'] for a, b in zip(r['runs'], o['runs'])))
        F['archive'][feas[0][1] if feas and feas[0][0] < o['stored_bytes'] else 'openzl'] += 1
    out[run] = {f: dict(n=v['n'], speed_fit_and_smaller={p: v['speed'][p][0] for p in SP}, speed_fit={p: v['speed'][p][1] for p in SP},
                        archive_smallest_feasible=dict(v['archive'])) for f, v in sorted(fam.items())}
print(json.dumps(out, indent=1))
