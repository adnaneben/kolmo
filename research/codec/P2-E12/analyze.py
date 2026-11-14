#!/usr/bin/env python3
"""P2-E12 analysis. Speed: fits zstd-3's speed cost on every pair (and smaller). Archive (informative): fits the
best available OpenZL configuration's archive cost on every pair. Reads results/*/records.json.xz or a plain dir."""
import json, lzma, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
def load(run):
    f = root / run / 'records.json.xz'
    return json.loads(lzma.decompress(f.read_bytes()) if f.exists() else (root / run / 'records.json').read_text())['records']
out = {}
for run in ('first', 'confirmation'):
    by = {}
    for r in load(run):
        if r.get('stored_bytes') is None: continue
        by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    rows = {}
    for inp, P in by.items():
        z = P['zstd_3']; row = {'zstd_3_bytes': z['stored_bytes'], 'speed': {}, 'archive': None}
        for p, r in P.items():
            if p == 'zstd_3' or p.startswith('openzl'): continue
            fit = all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs']))
            row['speed'][p] = dict(bytes=r['stored_bytes'], fit=fit, delta=round(r['stored_bytes'] / z['stored_bytes'] - 1, 4))
        oz = [P[k] for k in P if k.startswith('openzl')]
        if oz:
            o = min(oz, key=lambda r: r['stored_bytes'])
            feas = sorted((r['stored_bytes'], p) for p, r in P.items() if not p.startswith('openzl') and all(a['archive_cost_usd'] <= b['archive_cost_usd'] for a, b in zip(r['runs'], o['runs'])))
            row['archive'] = dict(rival=o['policy']['id'], rival_bytes=o['stored_bytes'], winner=(feas[0][1] if feas and feas[0][0] < o['stored_bytes'] else 'openzl'), winner_bytes=min(feas[0][0], o['stored_bytes']) if feas else o['stored_bytes'])
        rows[inp] = row
    pol = sorted({p for r in rows.values() for p in r['speed']})
    out[run] = dict(inputs=rows, totals={p: dict(fit=sum(r['speed'][p]['fit'] for r in rows.values()), fit_and_smaller=sum(r['speed'][p]['fit'] and r['speed'][p]['delta'] < 0 for r in rows.values())) for p in pol})
print(json.dumps(out, indent=1))
