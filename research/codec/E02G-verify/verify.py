#!/usr/bin/env python3
"""Apply the E02G-verify estimators and outcome rules (PROTOCOL.md). usage: verify.py DTIME_JSONL MANIFEST_JSON OUT_JSON"""
import collections, json, statistics as st, sys

def pct(v, p): v = sorted(v); return v[min(len(v) - 1, max(0, int(round(p * (len(v) - 1)))))]
MISSES = {('fast', 'F1_backblaze'), ('hc', 'F2_owid_covid')}

def main(path, manifest, out):
    inp = {i['name']: i for i in json.load(open(manifest))['inputs']}
    frames, T = {}, collections.defaultdict(list)
    for l in open(path):
        d = json.loads(l)
        if d['kind'] == 'dtime_frame': frames[(d['input'], d['mode'])] = d
        elif d['kind'] == 'dtime': T[(d['input'], d['mode'], d['regime'], d['policy'])].append((d['round'], d['wall_ns']))
    R = {'cells': {}, 'files': {}}
    for (name, mode), f in frames.items():
        for reg in ('warm', 'displaced'):
            m = {p: st.median(v for _, v in T[(name, mode, reg, p)]) for p in ('P', 'C', 'C2', 'K')}
            R['files'][f'{name}|{mode}|{reg}'] = dict(bytes=f['bytes'], int_blocks=f['int_blocks'], us={p: m[p] / 1e3 for p in m},
                                                      K_over_C=m['K'] / m['C'], C2_over_C=m['C2'] / m['C'], size_change_pct=100 * (f['candidate'] / f['control'] - 1))
    for mode in ('fast', 'hc'):
        for fam in sorted({i['family'] for i in inp.values()}):
            names = [n for (n, md) in frames if md == mode and inp[n]['family'] == fam and frames[(n, md)]['int_blocks'] and frames[(n, md)]['bytes'] >= 256 * 1024]
            if not names: continue
            for reg in ('warm', 'displaced'):
                med = lambda p: sum(st.median(v for _, v in T[(n, mode, reg, p)]) for n in names)
                ratio = med('K') / med('C')
                rounds = sorted({r for n in names for r, _ in T[(n, mode, reg, 'C')]})
                per_round = lambda a: [sum(dict(T[(n, mode, reg, a)])[r] for n in names) / sum(dict(T[(n, mode, reg, 'C')])[r] for n in names) for r in rounds]
                noise = per_round('C2'); kr = per_round('K')
                band = (pct(noise, 0.05), pct(noise, 0.95))
                outcome = None
                if (mode, fam) in MISSES:
                    outcome = 'reproduced' if ratio > 1.05 and ratio > band[1] else 'within noise' if band[0] <= ratio <= band[1] else 'not reproduced'
                R['cells'][f'{mode}|{fam}|{reg}'] = dict(files=names, K_over_C=ratio, noise_band_C2_over_C=band, K_over_C_per_round_min_median_max=(min(kr), st.median(kr), max(kr)),
                                                         abs_added_us=(med('K') - med('C')) / 1e3, registered_miss=(mode, fam) in MISSES, outcome=outcome)
    json.dump(R, open(out, 'w'), indent=1)
    for k, v in R['cells'].items(): print(k, f"K/C {v['K_over_C']:.3f} band [{v['noise_band_C2_over_C'][0]:.3f},{v['noise_band_C2_over_C'][1]:.3f}] added {v['abs_added_us']:.0f} us", v['outcome'] or '')

if __name__ == '__main__': main(*sys.argv[1:4])
