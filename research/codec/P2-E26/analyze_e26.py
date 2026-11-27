#!/usr/bin/env python3
"""P2-E26 frozen analysis (P2-E24 analysis; PRIMARY = pick, Bonferroni alpha 0.05/5 for the 5th sequential CSV test). Per origin (development F38 / fresh PLOTLY), per pipeline: fits OpenZL csv-9 archive cost on
EVERY pair AND smaller. Inputs where OpenZL fails are excluded from n and listed. Primary (fresh only): tt_bsc0 passes
if the one-sided 95% Clopper-Pearson lower bound on k/n exceeds 0.5 in BOTH runs. Secondary (reported, not tested):
kolmo_eco and the other frames; byte-weighted 'rule' frontier (pipeline if it fits and is smaller, else OpenZL)."""
import json, lzma, math, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
POL = ['pick', 'kolmo_eco', 'tt_zstd12', 'kolmo_fast']
def load(run):
    for f in (root / run / 'records.json.xz', root / run / 'records.json'):
        if f.exists(): return json.loads(lzma.decompress(f.read_bytes()) if f.suffix == '.xz' else f.read_text())['records']
def cp_lower(k, n, a=0.05):
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
    for origin in ('development', 'fresh'):
        ins = [i for i, P in by.items() if (P['zstd_3'].get('input_id', i).startswith('TT__')) == (origin == 'fresh')]
        o_ok = [i for i in ins if by[i]['openzl_csv_9'].get('stored_bytes') is not None]
        row = dict(n=len(o_ok), openzl_missing=sorted(set(ins) - set(o_ok)))
        for p in POL:
            k = 0; b = ob = 0
            for i in o_ok:
                r, o = by[i][p], by[i]['openzl_csv_9']
                ok = r.get('stored_bytes') is not None and all(a['archive_cost_usd'] <= c['archive_cost_usd'] for a, c in zip(r['runs'], o['runs'])) and r['stored_bytes'] < o['stored_bytes']
                k += ok; b += r['stored_bytes'] if ok else o['stored_bytes']; ob += o['stored_bytes']
            row[p] = dict(fit_and_smaller=k, cp95_lower=round(cp_lower(k, len(o_ok)), 4), cp_bonf_lower=round(cp_lower(k, len(o_ok), 0.01), 4), rule_bytes_vs_openzl9=round(b / ob - 1, 4) if ob else None)
        res[origin] = row
    out[run] = res
n_fresh = min(out[r]['fresh']['n'] for r in ('first', 'confirmation'))
out['decision'] = dict(fresh_n=n_fresh, inconclusive=n_fresh < 18,
                       primary_pick_fresh_pass_both=n_fresh >= 18 and all(out[r]['fresh']['pick']['cp_bonf_lower'] > 0.5 for r in ('first', 'confirmation')),
                       unadjusted_cp95_pass_both=all(out[r]['fresh']['pick']['cp95_lower'] > 0.5 for r in ('first', 'confirmation')),
                       pick_minus_eco={r: out[r]['fresh']['pick']['fit_and_smaller'] - out[r]['fresh']['kolmo_eco']['fit_and_smaller'] for r in ('first', 'confirmation')})
print(json.dumps(out, indent=1))
