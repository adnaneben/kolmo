#!/usr/bin/env python3
"""P2-E30 analysis (copy of P2-E28 analyze_e28.py, per source too): per pipeline, fits zstd-3's speed cost on EVERY pair and smaller, production decoder vs research
fast-inverse decoder (same frames). Development data; descriptive only."""
import json, lzma, pathlib, statistics, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
out = {}
for run in ('first', 'confirmation'):
    f = root / run / 'records.json.xz'; R = json.loads(lzma.decompress(f.read_bytes()) if f.exists() else (root / run / 'records.json').read_text())['records']
    by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    pols = sorted({p for P in by.values() for p in P if p != 'zstd_3'}); res = {}
    for p in pols:
        k = 0; dec = []
        for P in by.values():
            z, r = P['zstd_3'], P[p]
            k += all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs'])) and r['stored_bytes'] < z['stored_bytes']
            dec.append(statistics.median(a['decode_wall_s'] for a in r['runs']))
        src = {}
        for i, P in by.items():
            z, r = P['zstd_3'], P[p]; key = i.split('__')[0]
            src.setdefault(key, [0, 0]); src[key][1] += 1
            src[key][0] += all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs'])) and r['stored_bytes'] < z['stored_bytes']
        res[p] = dict(fit_and_smaller=k, n=len(by), per_source={s_: f'{v[0]}/{v[1]}' for s_, v in src.items()}, ratio_total=round(sum(P['zstd_3']['input_bytes'] for P in by.values()) / sum(P[p]['stored_bytes'] for P in by.values()), 4), median_decode_ms=round(1e3 * statistics.median(dec), 3))
    res['zstd_3'] = dict(median_decode_ms=round(1e3 * statistics.median(statistics.median(a['decode_wall_s'] for a in P['zstd_3']['runs']) for P in by.values()), 3))
    out[run] = res
print(json.dumps(out, indent=1))
