#!/usr/bin/env python3
"""P2-E15: realizable speed-mode selection rules evaluated on the recorded P2-E14 runs only (no new measurement).
A rule maps an input to ONE production-frame pipeline using information free at encode time. Score per input:
the rule's pipeline counts as 'fit and smaller' only if it fits zstd-3's cost on every pair and is smaller; for the
byte total an infeasible choice is charged zstd-3's bytes (a deployed rule would have to fall back). Size-threshold
rules are evaluated leave-one-family-out (threshold chosen on the other 10 families). Development only."""
import collections, itertools, json, lzma, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parents[1] / 'P2-E14' / 'results')
C = ['tp_phuf_pg16', 'tp_phuf', 'tb_lazy', 'tp_prod3', 'tb_best', 'tp_pent']
def load(run):
    f = root / run / 'records.json.xz'; R = json.loads(lzma.decompress(f.read_bytes()))['records']; by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    return by
def fits(r, z): return all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs']))
def score(by, ins, rule):
    k = b = zb = 0
    for i in ins:
        P = by[i]; z = P['zstd_3']; r = P[rule(P)]; f = fits(r, z)
        k += f and r['stored_bytes'] < z['stored_bytes']; b += r['stored_bytes'] if f else z['stored_bytes']; zb += z['stored_bytes']
    return k, b / zb - 1
out = {}
for run in ('first', 'confirmation'):
    by = load(run); fam = collections.defaultdict(list)
    for i, P in by.items(): fam[P['zstd_3']['family']].append(i)
    ins = list(by); res = {}
    orc = 0; ob = 0; zb = 0
    for i, P in by.items():
        z = P['zstd_3']; f = [P[p]['stored_bytes'] for p in P if p not in ('zstd_3',) and not p.startswith('openzl') and fits(P[p], z)]
        w = min(f + [z['stored_bytes']]); orc += w < z['stored_bytes']; ob += w; zb += z['stored_bytes']
    res['oracle_per_input'] = dict(fit_and_smaller=orc, bytes_vs_zstd3=round(ob / zb - 1, 4))
    for c in C:
        k, b = score(by, ins, lambda P, c=c: c); res['always_' + c] = dict(fit_and_smaller=k, bytes_vs_zstd3=round(b, 4))
    thr = sorted({P['zstd_3']['input_bytes'] for P in by.values()})
    for a, bb in itertools.permutations(['tp_phuf_pg16', 'tb_lazy', 'tb_best', 'tp_pent'], 2):
        k = 0; byt = 0; zz = 0
        for f, held in fam.items():
            train = [i for i in ins if i not in held]
            t = max(thr, key=lambda t: score(by, train, lambda P: a if P['zstd_3']['input_bytes'] < t else bb))
            s, _ = score(by, held, lambda P: a if P['zstd_3']['input_bytes'] < t else bb); k += s
            for i in held:
                P = by[i]; z = P['zstd_3']; r = P[a if P['zstd_3']['input_bytes'] < t else bb]
                byt += r['stored_bytes'] if fits(r, z) else z['stored_bytes']; zz += z['stored_bytes']
        res[f'lofo_size_{a}_below_{bb}_above'] = dict(fit_and_smaller=k, bytes_vs_zstd3=round(byt / zz - 1, 4))
    out[run] = res
print(json.dumps(out, indent=1))
