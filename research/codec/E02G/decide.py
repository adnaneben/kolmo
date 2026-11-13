#!/usr/bin/env python3
"""Apply the E02G preregistered rules (PREREGISTRATION.md) to raw harness records. Rules are fixed here.

usage: decide.py RAW_JSONL MANIFEST_JSON OUT_JSON
"""
import collections, json, statistics, sys

PAIRS = {'H1_fast': ('fast', 'exist+prod3', 'planes+prod3g'), 'H2_hc': ('hc', 'exist+prod19', 'planes+prod19')}
CAL = {'fast': 'exist+prod3', 'hc': 'exist+prod19'}
POOLED_MAX, FILE_MAX, FILE_MIN, LARGE, NS_PER_BYTE, LARGE_RATIO, CAL_MAX, CONTROL_MAX = -0.50, 1.00, 64 * 1024, 256 * 1024, 0.50, 1.05, 0.10, 0.50

def main(raw, manifest, out):
    inp = {i['name']: i for i in json.load(open(manifest))['inputs']}
    fr, tot, dec = {}, collections.defaultdict(dict), collections.defaultdict(list)
    for l in open(raw):
        d = json.loads(l)
        if d['kind'] == 'planner_frame': fr[(d['input'], d['mode'])] = d
        elif d['kind'] == 'planner_total': tot[(d['input'], d['mode'])][d['config']] = d['total']
        elif d['kind'] == 'e2e_time' and d['what'] == 'exp_decode_seq': dec[(d['input'], d['mode'], d['config'])].append(d['wall_ns'])
    ints = lambda k: [b['int'] for b in fr[k]['blocks_detail'] if 'int' in b]
    eligible = lambda k: any(b['ow'] >= 4 or (b['order'] == 0 and b['ow'] >= 2) for b in ints(k))
    size = lambda k, c: tot[k][c] if c in tot[k] else fr[k]['frame']     # no INT block: the leaf is unchanged
    R = {'calibration': {}, 'eligibility': {}, 'hypotheses': {}}; cal_ok = True
    fams = sorted({i['family'] for i in inp.values()})
    for mode, cf in CAL.items():
        ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks']]
        R['eligibility'][mode] = {f: dict(files=sum(inp[k[0]]['family'] == f for k in fr if k[1] == mode), int_files=sum(inp[k[0]]['family'] == f for k in ks),
                                          eligible=sum(inp[k[0]]['family'] == f and eligible(k) for k in ks)) for f in fams}
        if not ks: R['calibration'][mode] = 'no INT files'; cal_ok = False; continue
        p = sum(fr[k]['frame'] for k in ks); c = sum(tot[k][cf] for k in ks); pct = 100 * (c / p - 1)
        R['calibration'][mode] = dict(config=cf, production=p, control=c, change_pct=pct, ok=pct <= CAL_MAX); cal_ok &= pct <= CAL_MAX
    md = lambda k, mode, c: statistics.median(dec[(k[0], mode, c)]) if dec[(k[0], mode, c)] else 0.0
    for h, (mode, ctl, cand) in PAIRS.items():
        targets, controls = {}, {}
        for f in fams:
            role = {inp[n]['role'] for n in inp if inp[n]['family'] == f}.pop()
            allk = [k for k in fr if k[1] == mode and inp[k[0]]['family'] == f]
            if role == 'control':
                a = sum(size(k, ctl) for k in allk); b = sum(size(k, cand) for k in allk)
                controls[f] = dict(files=len(allk), control_bytes=a, candidate_bytes=b, change_pct=100 * (b / a - 1) if a else 0.0, ok=(b / a - 1) * 100 <= CONTROL_MAX if a else True)
                continue
            ks = [k for k in allk if fr[k]['int_blocks'] and eligible(k)]
            if len(ks) < 2: targets[f] = dict(status='not exercised', eligible_files=len(ks)); continue
            a = sum(tot[k][ctl] for k in ks); b = sum(tot[k][cand] for k in ks); pooled = 100 * (b / a - 1)
            worst = max(((100 * (tot[k][cand] / tot[k][ctl] - 1), k[0]) for k in ks if fr[k]['bytes'] >= FILE_MIN), default=(None, None))
            dt = sum(md(k, mode, cand) - md(k, mode, ctl) for k in ks); nb = sum(fr[k]['bytes'] for k in ks)
            big = [k for k in ks if fr[k]['bytes'] >= LARGE]
            ratio = sum(md(k, mode, cand) for k in big) / sum(md(k, mode, ctl) for k in big) if big else None
            ok = pooled <= POOLED_MAX and (worst[0] is None or worst[0] <= FILE_MAX) and dt / nb <= NS_PER_BYTE and (ratio is None or ratio <= LARGE_RATIO)
            targets[f] = dict(eligible_files=len(ks), control_bytes=a, candidate_bytes=b, pooled_change_pct=pooled, worst_file=worst,
                              added_decode_ns_per_input_byte=dt / nb, large_files=len(big), large_file_decode_ratio=ratio,
                              vs_production_pct=100 * (b / sum(fr[k]['frame'] for k in ks) - 1), meets=ok)
        ex = [v for v in targets.values() if v.get('status') != 'not exercised']; met = sum(v['meets'] for v in ex)
        ctl_ok = all(v['ok'] for v in controls.values())
        status = ('inconclusive (calibration failed)' if not cal_ok else 'inconclusive (not exercised)' if not ex else
                  'supported' if met >= 2 and ctl_ok else 'provisional' if met >= 1 else 'not supported')
        R['hypotheses'][h] = dict(mode=mode, control=ctl, candidate=cand, status=status, control_check_ok=ctl_ok, targets=targets, controls=controls)
    ks = [k for k in fr if k[1] == 'fast' and fr[k]['int_blocks']]
    R['hypotheses']['H3_guard'] = dict(mode='fast', descriptive=True,
        files_changed=sorted(k[0] for k in ks if tot[k]['planes+prod3g'] != tot[k]['planes+prod3']),
        bytes_saved=sum(tot[k]['planes+prod3'] - tot[k]['planes+prod3g'] for k in ks),
        planes_without_guard_vs_control_pct=100 * (sum(tot[k]['planes+prod3'] for k in ks) / sum(tot[k]['exist+prod3'] for k in ks) - 1) if ks else None,
        planes_with_guard_vs_control_pct=100 * (sum(tot[k]['planes+prod3g'] for k in ks) / sum(tot[k]['exist+prod3'] for k in ks) - 1) if ks else None)
    with open(out, 'w') as f: json.dump(R, f, indent=1); f.write('\n')
    print(json.dumps({h: v.get('status', 'descriptive') for h, v in R['hypotheses'].items()}, indent=1)); print('calibration', R['calibration'])

if __name__ == '__main__': main(*sys.argv[1:4])
