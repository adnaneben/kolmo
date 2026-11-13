#!/usr/bin/env python3
"""P2-E16 analysis: leave-one-family-out selectors over encode-time features (results/features.jsonl.xz, recorded on the
runner) and P2-E14's committed records (choices and outcomes). Development only; see PROTOCOL.md.
Models: always tp_phuf_pg16 (baseline); depth-1/depth-2 threshold rules choosing among CHOICES; 1-nearest-neighbour on
standardised features. Score: fits-and-smaller count and bytes (an infeasible choice is charged zstd-3's bytes)."""
import collections, json, lzma, math, pathlib, statistics, sys
HERE = pathlib.Path(__file__).resolve().parent
E14 = HERE.parent / 'P2-E14' / 'results'
CHOICES = ['tp_phuf_pg16', 'tb_best', 'tp_pent', 'tb_lazy']
feat_rows = [json.loads(l) for l in lzma.decompress((HERE / 'results' / 'features.jsonl.xz').read_bytes()).decode().splitlines() if l.strip()]
F = collections.defaultdict(list)
for r in feat_rows: F[pathlib.Path(r['input']).stem].append(r['f'])
def fvec(fs):
    f = fs[0]; ent = f['ent'] or [0.0]
    return dict(log_n=math.log2(max(f['input_bytes'], 1)), typed=f['typed'] * f['forward_ok'], w=f['w'], order=f['order'], signed=f['signed'],
                ow=f['ow'], planes=f['planes'], log_gcd=math.log2(max(f['gcd'], 1)), res_ratio=f['res_bytes'] / max(f['input_bytes'], 1),
                zero=f['zero_frac'], ent_mean=sum(ent) / len(ent), ent_max=max(ent), ent_min=min(ent))
def load(run):
    R = json.loads(lzma.decompress((E14 / run / 'records.json.xz').read_bytes()))['records']; by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    return by
def fits(r, z): return all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs']))
def outcome(P, c):
    z = P['zstd_3']; r = P[c]; f = fits(r, z)
    return (f and r['stored_bytes'] < z['stored_bytes']), (r['stored_bytes'] if f else z['stored_bytes']), z['stored_bytes']
def evaluate(items, choose):
    k = b = zb = 0
    for i, P, x in items:
        a, bb, z = outcome(P, choose(x)); k += a; b += bb; zb += z
    return k, b / zb - 1
def utility(items, choose):  # training objective: bytes (lower better), ties by count
    k, b = evaluate(items, choose); return (-b, k)
def fit_tree(train, depth):
    keys = list(train[0][2]); best = (utility(train, lambda x: 'tp_phuf_pg16'), lambda x: 'tp_phuf_pg16', 'always pg16')
    for key in keys:
        for t in sorted({x[key] for _, _, x in train}):
            for lo in CHOICES:
                for hi in CHOICES:
                    if lo == hi: continue
                    ch = lambda x, key=key, t=t, lo=lo, hi=hi: lo if x[key] <= t else hi
                    u = utility(train, ch)
                    if u > best[0]: best = (u, ch, f'{key}<={t:.4g}?{lo}:{hi}')
    if depth == 1: return best
    # depth 2: split on the best depth-1 key/threshold, then a depth-1 rule on each side
    _, ch1, desc = best
    if '<=' not in desc: return best
    key, rest = desc.split('<='); t = float(rest.split('?')[0])
    L = [it for it in train if it[2][key] <= t]; R = [it for it in train if it[2][key] > t]
    if not L or not R: return best
    _, cl, dl = fit_tree(L, 1); _, cr, dr = fit_tree(R, 1)
    ch2 = lambda x: cl(x) if x[key] <= t else cr(x); u2 = utility(train, ch2)
    return (u2, ch2, f'{key}<={t:.4g}?[{dl}]:[{dr}]') if u2 > best[0] else best
def nn1(train):
    keys = list(train[0][2]); mu = {k: statistics.mean(x[k] for _, _, x in train) for k in keys}
    sd = {k: statistics.pstdev([x[k] for _, _, x in train]) or 1.0 for k in keys}
    lab = []
    for i, P, x in train:
        best = min(CHOICES, key=lambda c: (outcome(P, c)[1], -outcome(P, c)[0])); lab.append((x, best))
    def ch(x):
        d = lambda y: sum(((x[k] - y[k]) / sd[k]) ** 2 for k in keys)
        return min(lab, key=lambda t: d(t[0]))[1]
    return ch
out = {}
for run in ('first', 'confirmation'):
    by = load(run); items = [(i, P, fvec(F[i])) for i, P in by.items()]
    fam = collections.defaultdict(list)
    for it in items: fam[it[1]['zstd_3']['family']].append(it)
    res = {'always_tp_phuf_pg16': evaluate(items, lambda x: 'tp_phuf_pg16')}
    for name, mk in (('lofo_tree1', lambda tr: fit_tree(tr, 1)[1]), ('lofo_tree2', lambda tr: fit_tree(tr, 2)[1]), ('lofo_1nn', nn1)):
        k = b = zb = 0
        for f, held in fam.items():
            train = [it for it in items if it[1]['zstd_3']['family'] != f]; ch = mk(train)
            for i, P, x in held:
                a, bb, z = outcome(P, ch(x)); k += a; b += bb; zb += z
        res[name] = (k, b / zb - 1)
    res['in_sample_tree2_rule'] = fit_tree(items, 2)[2]
    ex = [r['f']['extract_s'] for r in feat_rows]; zt = [statistics.median(a['encode_wall_s'] + a['decode_wall_s'] for a in P['zstd_3']['runs']) for P in by.values()]
    res['feature_extract_s_median'] = statistics.median(ex); res['zstd3_roundtrip_s_median'] = statistics.median(zt)
    out[run] = res
print(json.dumps(out, indent=1))
