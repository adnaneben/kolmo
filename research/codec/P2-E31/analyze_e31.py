#!/usr/bin/env python3
"""P2-E31 frozen analysis. Per pipeline: fits zstd-3's speed cost on EVERY pair AND smaller (N1 fields), per source and
total. PRIMARY: ta_phuf_fd. Pass if the one-sided Clopper-Pearson lower bound at alpha = 0.05/3 (third sequential audio
speed test: P2-E20 H1, P2-E29, P2-E31) exceeds 0.5 in BOTH runs; fewer than 20 inputs -> inconclusive.
Everything else reported only (incl. ratio_total and ta_rep_fd)."""
import json, lzma, math, pathlib, sys
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
        ok = {i: bool(P[p].get('stored_bytes') is not None and all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(P[p]['runs'], P['zstd_3']['runs'])) and P[p]['stored_bytes'] < P['zstd_3']['stored_bytes']) for i, P in by.items()}
        k = sum(ok.values()); src = {}
        for i, v in ok.items(): s = i.split('__')[1]; src.setdefault(s, [0, 0]); src[s][0] += v; src[s][1] += 1
        res[p] = dict(fit_and_smaller=k, n=len(by), lb_bonf=round(lb(k, len(by), 0.05 / 3), 4), per_source={s: f'{a}/{b}' for s, (a, b) in src.items()},
                      ratio_total=round(sum(P['zstd_3']['input_bytes'] for P in by.values()) / sum(P[p]['stored_bytes'] for P in by.values()), 4))
    out[run] = res
n = min(out[r]['ta_phuf_fd']['n'] for r in ('first', 'confirmation'))
out['decision'] = dict(n=n, inconclusive=n < 20, primary_pass_both=n >= 20 and all(out[r]['ta_phuf_fd']['lb_bonf'] > 0.5 for r in ('first', 'confirmation')))
print(json.dumps(out, indent=1))
