#!/usr/bin/env python3
"""P2-E25 (development, analysis of runner records only): can a near-free feature rule choose between production
`kolmo -m eco` and the zstd-12 table frame (`tt_zstd12`) per CSV, against OpenZL csv-9 archive cost?
Outcomes: runner records of P2-E21 (FiveThirtyEight dev part), P2-E22, P2-E23, P2-E24 (all now observed).
Features: computed from the first 64 KiB of each file only (as an encoder could, nearly free).
Evaluation: leave-one-experiment-out (train the rule on three experiments, test on the fourth).
usage: selector_csv.py FILE_ROOTS_JSON   ({"P2-E21": dir_for_F38, "P2-E22": dir_for_TT, ...} mapping source_path roots)"""
import json, lzma, math, pathlib, sys, collections, statistics
HERE = pathlib.Path(__file__).resolve().parent.parent
roots = json.loads(pathlib.Path(sys.argv[1]).read_text())
def feats(b):
    s = b[:65536]; lines = s.split(b'\n')[:-1] or [s]
    k = [l.count(b',') + 1 for l in lines]; kk = statistics.mode(k)
    reg = sum(1 for x in k if x == kk) / len(k)
    fields = [f for l in lines[1:200] for f in l.split(b',')]
    num = sum(1 for f in fields if f.strip().replace(b'.', b'', 1).replace(b'-', b'', 1).isdigit()) / max(1, len(fields))
    quotes = s.count(b'"') / max(1, len(s))
    return dict(cols=kk, regular=reg, numeric=num, quotes=quotes, avg_line=len(s) / len(lines), log_n=math.log2(len(b)), cr=s.count(b'\r') / max(1, len(s)))
items = []
for exp in ('P2-E21', 'P2-E22', 'P2-E23', 'P2-E24'):
    m = json.loads(lzma.decompress((HERE / exp / 'results/prepared/manifest.json.xz').read_bytes()))
    src = {r['id']: r for r in m['inputs']}
    R = json.loads(lzma.decompress((HERE / exp / 'results/first/records.json.xz').read_bytes()))['records']; by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    for i, P in by.items():
        if exp == 'P2-E21' and not i.startswith('F38'): continue
        o = P['openzl_csv_9']
        if o.get('stored_bytes') is None: continue
        ok = lambda p: P[p].get('stored_bytes') is not None and all(a['archive_cost_usd'] <= c['archive_cost_usd'] for a, c in zip(P[p]['runs'], o['runs'])) and P[p]['stored_bytes'] < o['stored_bytes']
        b = (pathlib.Path(roots[exp]) / src[i]['source_path']).read_bytes()
        items.append(dict(exp=exp, id=i, f=feats(b), eco=ok('kolmo_eco'), z12=ok('tt_zstd12'), ob=o['stored_bytes'],
                          eb=P['kolmo_eco']['stored_bytes'], zb=P['tt_zstd12']['stored_bytes']))
def score(its, rule):
    k = 0; b = ob = 0
    for it in its:
        c = rule(it['f']); w = it[c]; k += w; b += (it['eb'] if c == 'eco' else it['zb']) if w else it['ob']; ob += it['ob']
    return k, b / ob - 1
def fit(train):
    best = (score(train, lambda f: 'eco'), lambda f: 'eco', 'always eco')
    for key in train[0]['f']:
        for t in sorted({it['f'][key] for it in train}):
            for lo, hi in (('eco', 'z12'), ('z12', 'eco')):
                ch = lambda f, key=key, t=t, lo=lo, hi=hi: lo if f[key] <= t else hi
                s = score(train, ch)
                if (s[0], -s[1]) > (best[0][0], -best[0][1]): best = (s, ch, f'{key}<={t:.4g}?{lo}:{hi}')
    return best
out = dict(n=len(items), always_eco=score(items, lambda f: 'eco'), always_z12=score(items, lambda f: 'z12'),
           oracle=sum(it['eco'] or it['z12'] for it in items), lofo={})
tot = [0, 0]
for exp in ('P2-E21', 'P2-E22', 'P2-E23', 'P2-E24'):
    tr = [it for it in items if it['exp'] != exp]; te = [it for it in items if it['exp'] == exp]
    _, ch, desc = fit(tr); s = score(te, ch); e = score(te, lambda f: 'eco')
    out['lofo'][exp] = dict(rule=desc, n=len(te), rule_k=s[0], eco_k=e[0]); tot[0] += s[0]; tot[1] += e[0]
out['lofo_total'] = dict(rule_k=tot[0], eco_k=tot[1]); out['in_sample_rule'] = fit(items)[2]
print(json.dumps(out, indent=1))
