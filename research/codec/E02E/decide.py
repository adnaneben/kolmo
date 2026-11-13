#!/usr/bin/env python3
"""Apply the E02E preregistered decision rules (PREREGISTRATION.md) to raw harness records. Rules are fixed here.

usage: decide.py RAW_JSONL MANIFEST_JSON OUT_JSON
"""
import collections, json, statistics, sys

PAIRS = {'H1_fast': ('fast', 'exist+prod3', 'planes+prod3'), 'H2_hc': ('hc', 'exist+prod19', 'planes+prod19')}
CAL = {'fast': 'exist+prod3', 'hc': 'exist+prod19'}
POOLED_MAX, FILE_MAX, FILE_MIN_BYTES, DEC_MAX, CAL_MAX = -0.50, 1.00, 64 * 1024, 1.05, 0.10

def main(raw, manifest, out):
    fam = {i['name']: i['family'] for i in json.load(open(manifest))['inputs']}
    fr, tot, dec = {}, collections.defaultdict(dict), collections.defaultdict(list)
    for l in open(raw):
        d = json.loads(l)
        if d['kind'] == 'planner_frame': fr[(d['input'], d['mode'])] = d
        elif d['kind'] == 'planner_total': tot[(d['input'], d['mode'])][d['config']] = d['total']
        elif d['kind'] == 'e2e_time' and d['what'] == 'exp_decode_seq': dec[(d['input'], d['mode'], d['config'])].append(d['wall_ns'])
    R = {'calibration': {}, 'hypotheses': {}}
    cal_ok = True
    for mode, cf in CAL.items():
        ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks']]
        if not ks: R['calibration'][mode] = 'no INT files'; continue
        p = sum(fr[k]['frame'] for k in ks); c = sum(tot[k][cf] for k in ks); pct = 100 * (c / p - 1)
        R['calibration'][mode] = dict(config=cf, production=p, control=c, change_pct=pct, ok=pct <= CAL_MAX)
        cal_ok &= pct <= CAL_MAX
    guard = {}
    for h, (mode, ctl, cand) in PAIRS.items():
        fams = {}
        for f in sorted(set(fam.values())):
            ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks'] and fam[k[0]] == f]
            if len(ks) < 2: fams[f] = dict(status='not exercised', int_files=len(ks)); continue
            a = sum(tot[k][ctl] for k in ks); b = sum(tot[k][cand] for k in ks); pooled = 100 * (b / a - 1)
            worst = max(((100 * (tot[k][cand] / tot[k][ctl] - 1), k[0]) for k in ks if fr[k]['bytes'] >= FILE_MIN_BYTES), default=(None, None))
            dc = sum(statistics.median(dec[(k[0], mode, cand)]) for k in ks); dx = sum(statistics.median(dec[(k[0], mode, ctl)]) for k in ks)
            ok = pooled <= POOLED_MAX and (worst[0] is None or worst[0] <= FILE_MAX) and dc / dx <= DEC_MAX
            fams[f] = dict(int_files=len(ks), control_bytes=a, candidate_bytes=b, pooled_change_pct=pooled, worst_file=worst,
                           decode_ratio=dc / dx, vs_production_pct=100 * (b / sum(fr[k]['frame'] for k in ks) - 1), meets=ok)
        ex = [v for v in fams.values() if v.get('status') != 'not exercised']; met = sum(v['meets'] for v in ex)
        status = 'inconclusive (calibration failed)' if not cal_ok else 'inconclusive (not exercised)' if not ex else \
                 'supported' if met >= 2 else 'provisional' if met == 1 else 'not supported'
        R['hypotheses'][h] = dict(mode=mode, control=ctl, candidate=cand, status=status, families=fams,
                                  family_mean_change_pct=statistics.mean(v['pooled_change_pct'] for v in ex) if ex else None)
    ks = [k for k in fr if k[1] == 'fast' and fr[k]['int_blocks']]
    R['hypotheses']['H3_guard'] = dict(mode='fast', descriptive=True, files_changed=sum(tot[k]['planes+prod3g'] != tot[k]['planes+prod3'] for k in ks),
                                       bytes_saved=sum(tot[k]['planes+prod3'] - tot[k]['planes+prod3g'] for k in ks))
    json.dump(R, open(out, 'w'), indent=1); print(json.dumps({h: v.get('status', v) for h, v in R['hypotheses'].items()}, indent=1)); print('calibration', R['calibration'])

if __name__ == '__main__': main(*sys.argv[1:4])
