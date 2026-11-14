#!/usr/bin/env python3
"""Apply the E02F preregistered rules (PREREGISTRATION.md) to raw harness records. Rules are fixed here.

usage: decide.py RAW_JSONL MANIFEST_JSON OUT_JSON
"""
import collections, json, statistics, sys

PAIRS = {'H1_fast': ('fast', 'exist+prod3', 'planes+prod3'), 'H2_hc': ('hc', 'exist+prod19', 'planes+prod19')}
CAL = {'fast': 'exist+prod3', 'hc': 'exist+prod19'}
POOLED_MAX, FILE_MAX, FILE_MIN, LARGE, NS_PER_BYTE, LARGE_RATIO, CAL_MAX = -0.50, 1.00, 64 * 1024, 256 * 1024, 0.50, 1.05, 0.10

def main(raw, manifest, out):
    fam = {i['name']: i['family'] for i in json.load(open(manifest))['inputs']}
    fr, tot, dec = {}, collections.defaultdict(dict), collections.defaultdict(list)
    for l in open(raw):
        d = json.loads(l)
        if d['kind'] == 'planner_frame': fr[(d['input'], d['mode'])] = d
        elif d['kind'] == 'planner_total': tot[(d['input'], d['mode'])][d['config']] = d['total']
        elif d['kind'] == 'e2e_time' and d['what'] == 'exp_decode_seq': dec[(d['input'], d['mode'], d['config'])].append(d['wall_ns'])
    pheno = lambda k: any(b['int']['ow'] >= 2 for b in fr[k]['blocks_detail'] if 'int' in b)
    R = {'calibration': {}, 'phenotype': {}, 'hypotheses': {}}; cal_ok = True
    for mode, cf in CAL.items():
        ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks']]
        R['phenotype'][mode] = {f: dict(int_files=sum(fam[k[0]] == f for k in ks), multibyte_files=sum(fam[k[0]] == f and pheno(k) for k in ks)) for f in sorted(set(fam.values()))}
        if not ks: R['calibration'][mode] = 'no INT files'; continue
        p = sum(fr[k]['frame'] for k in ks); c = sum(tot[k][cf] for k in ks); pct = 100 * (c / p - 1)
        R['calibration'][mode] = dict(config=cf, production=p, control=c, change_pct=pct, ok=pct <= CAL_MAX); cal_ok &= pct <= CAL_MAX
    for h, (mode, ctl, cand) in PAIRS.items():
        fams = {}
        for f in sorted(set(fam.values())):
            ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks'] and fam[k[0]] == f and pheno(k)]
            if len(ks) < 2: fams[f] = dict(status='not exercised', multibyte_files=len(ks)); continue
            a = sum(tot[k][ctl] for k in ks); b = sum(tot[k][cand] for k in ks); pooled = 100 * (b / a - 1)
            worst = max(((100 * (tot[k][cand] / tot[k][ctl] - 1), k[0]) for k in ks if fr[k]['bytes'] >= FILE_MIN), default=(None, None))
            md = lambda k, c: statistics.median(dec[(k[0], mode, c)])
            dt = sum(md(k, cand) - md(k, ctl) for k in ks); nbytes = sum(fr[k]['bytes'] for k in ks)
            big = [k for k in ks if fr[k]['bytes'] >= LARGE]
            ratio = sum(md(k, cand) for k in big) / sum(md(k, ctl) for k in big) if big else None
            dec_ok = dt / nbytes <= NS_PER_BYTE and (ratio is None or ratio <= LARGE_RATIO)
            ok = pooled <= POOLED_MAX and (worst[0] is None or worst[0] <= FILE_MAX) and dec_ok
            fams[f] = dict(multibyte_files=len(ks), control_bytes=a, candidate_bytes=b, pooled_change_pct=pooled, worst_file=worst,
                           added_decode_ns_per_input_byte=dt / nbytes, large_files=len(big), large_file_decode_ratio=ratio,
                           vs_production_pct=100 * (b / sum(fr[k]['frame'] for k in ks) - 1), meets=ok)
        ex = [v for v in fams.values() if v.get('status') != 'not exercised']; met = sum(v['meets'] for v in ex)
        status = 'inconclusive (calibration failed)' if not cal_ok else 'inconclusive (not exercised)' if not ex else \
                 'supported' if met >= 2 else 'provisional' if met == 1 else 'not supported'
        R['hypotheses'][h] = dict(mode=mode, control=ctl, candidate=cand, status=status, families=fams,
                                  family_mean_change_pct=statistics.mean(v['pooled_change_pct'] for v in ex) if ex else None)
    with open(out, 'w') as f: json.dump(R, f, indent=1); f.write('\n')
    print(json.dumps({h: v['status'] for h, v in R['hypotheses'].items()}, indent=1)); print('calibration', R['calibration']); print('phenotype', R['phenotype'])

if __name__ == '__main__': main(*sys.argv[1:4])
