#!/usr/bin/env python3
"""P2-E29 frozen analysis. Per pipeline: fits zstd-3's speed cost on EVERY pair AND smaller (N1 fields).
PRIMARY: tp_phuf_pg16_fd (research fast-inverse decoder). Pass if the one-sided Clopper-Pearson lower bound at
alpha = 0.05/2 (second sequential audio speed test after P2-E20 H1) exceeds 0.5 in BOTH runs; < 20 inputs -> inconclusive.
Reported only: everything else, including tp_phuf_pg16 with the production decoder."""
import json, lzma, math, pathlib, statistics, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
def load(run):
    for f in (root / run / 'records.json.xz', root / run / 'records.json'):
        if f.exists(): return json.loads(lzma.decompress(f.read_bytes()) if f.suffix == '.xz' else f.read_text())['records']
def lb(k, n, a):
    if k == 0 or n == 0: return 0.0
    lo, hi = 0.0, 1.0
    for _ in range(60):
        p = (lo + hi) / 2; t = sum(math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(k, n + 1)); lo, hi = (p, hi) if t < a else (lo, p)
    return lo
out = {}
for run in ('first', 'confirmation'):
    by = {}
    for r in load(run): by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    res = {}
    for p in sorted({p for P in by.values() for p in P if p != 'zstd_3'}):
        k = sum(1 for P in by.values() if P[p].get('stored_bytes') is not None and all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(P[p]['runs'], P['zstd_3']['runs'])) and P[p]['stored_bytes'] < P['zstd_3']['stored_bytes'])
        res[p] = dict(fit_and_smaller=k, n=len(by), lb_bonf=round(lb(k, len(by), 0.025), 4),
                      median_bytes_vs_zstd3=round(statistics.median(P[p]['stored_bytes'] / P['zstd_3']['stored_bytes'] - 1 for P in by.values()), 4))
    out[run] = res
n = min(out[r]['tp_phuf_pg16_fd']['n'] for r in ('first', 'confirmation'))
out['decision'] = dict(n=n, inconclusive=n < 20, primary_pass_both=n >= 20 and all(out[r]['tp_phuf_pg16_fd']['lb_bonf'] > 0.5 for r in ('first', 'confirmation')))
print(json.dumps(out, indent=1))
