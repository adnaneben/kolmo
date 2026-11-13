#!/usr/bin/env python3
"""P2-E3 analysis of N1-boundary records (research/codec/P2-E3/results/*/records.json.xz). Development only.
Speed: every policy vs zstd_3 (speed_cost_s, every pair). Archive: vs openzl_<profile>_9 (archive_cost_usd, every
pair) where that rival measured; OpenZL v0.3.0 has no 'numpy' profile, so NPY archive comparisons are BLOCKED."""
import json, lzma, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
out = {}
for run in ('first', 'confirmation'):
    R = json.loads(lzma.decompress((root / run / 'records.json.xz').read_bytes()))['records']
    by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    res = {}
    for inp, P in by.items():
        view = 'csv' if inp.endswith('__table') else 'npy'
        z = P['zstd_3']; rows = {}
        for pid, r in P.items():
            if r['status'] != 'MEASURED_DEVELOPMENT_CONTROL' or pid == 'zstd_3': continue
            rows[pid] = dict(bytes=r['stored_bytes'], bytes_vs_zstd3_pct=100 * (r['stored_bytes'] / z['stored_bytes'] - 1),
                             speed_fits_every_pair=all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs'])),
                             speed_cost_vs_zstd3_pct=100 * (r['medians']['speed_cost_s'] / z['medians']['speed_cost_s'] - 1))
            o = P.get(('openzl_csv_9' if view == 'csv' else 'openzl_numpy_9'))
            if o and o['status'] == 'MEASURED_DEVELOPMENT_CONTROL':
                rows[pid].update(bytes_vs_openzl9_pct=100 * (r['stored_bytes'] / o['stored_bytes'] - 1),
                                 archive_fits_every_pair=all(a['archive_cost_usd'] <= b['archive_cost_usd'] for a, b in zip(r['runs'], o['runs'])))
            else:
                rows[pid]['archive'] = 'BLOCKED: OpenZL typed profile not measured (v0.3.0 has no numpy profile)'
        res[inp] = dict(view=view, zstd3_bytes=z['stored_bytes'], policies=rows)
    out[run] = res
print(json.dumps(out, indent=1))
