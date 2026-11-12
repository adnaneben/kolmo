#!/usr/bin/env python3
"""Stacking/amplification diagnostics on committed raw records (no new runs). OFFLINE oracles are labelled as such:
they are ceilings for an ideal gate or selector, not encoders, and their selection cost is not charged.

usage: stack.py [EXTRA_COHORT_NAME RAW_JSONL_OR_XZ MANIFEST]   (from the repository root)
"""
import collections, json, lzma, statistics as st, sys

COHORTS = {'dev': ('research/codec/E02C-harness/results-followup2/raw.jsonl.xz', 'research/codec/E02C-harness/corpus/manifest.json'),
           'E02E': ('research/codec/E02E/results/raw.jsonl.xz', 'research/codec/E02E/results/manifest.json'),
           'E02F': ('research/codec/E02F/results/raw.jsonl.xz', 'research/codec/E02F/results/manifest.json')}
LVL = {'fast': '3', 'hc': '19'}

def opener(p): return lzma.open(p, 'rt') if p.endswith('.xz') else open(p)

def cohort(raw, man):
    M = {i['name']: i for i in json.load(open(man))['inputs']}
    fr, tot, dec = {}, collections.defaultdict(dict), collections.defaultdict(list)
    for l in opener(raw):
        r = json.loads(l)
        if r['kind'] == 'planner_frame': fr[(r['input'], r['mode'])] = r
        elif r['kind'] == 'planner_total': tot[(r['input'], r['mode'])][r['config']] = r['total']
        elif r['kind'] == 'e2e_time' and r['what'] in ('exp_decode_seq', 'prod_decode_seq'): dec[(r['input'], r['mode'], r.get('config') or 'prod')].append(r['wall_ns'])
    return M, fr, tot, dec

def main(extra):
    cos = dict(COHORTS)
    if extra: cos[extra[0]] = (extra[1], extra[2])
    out = {}
    for name, (raw, man) in cos.items():
        M, fr, tot, dec = cohort(raw, man)
        for mode in ('fast', 'hc'):
            ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks'] and M[k[0]]['origin'] != 'synthetic']
            if not ks: continue
            ex, pl = f'exist+prod{LVL[mode]}', f'planes+prod{LVL[mode]}'
            P = sum(fr[k]['frame'] for k in ks); md = lambda k, c: st.median(dec[(k[0], mode, c)])
            fixed = [c for c in tot[ks[0]] if not c.startswith('sel+') and all(c in tot[k] for k in ks)]
            gate = {k: (pl if tot[k][pl] < tot[k][ex] else ex) for k in ks}       # ideal sparse-plane gate (oracle)
            broad = {k: min(fixed, key=lambda c: tot[k][c]) for k in ks}          # ideal per-file choice among all fixed configs
            row = dict(files=len(ks), production=P,
                       exist_prod=sum(tot[k][ex] for k in ks), planes_prod=sum(tot[k][pl] for k in ks),
                       oracle_gate=sum(tot[k][gate[k]] for k in ks), oracle_gate_planes_chosen=sum(gate[k] == pl for k in ks),
                       oracle_broad=sum(tot[k][broad[k]] for k in ks), oracle_broad_choices=collections.Counter(broad.values()).most_common(6),
                       decode_planes_vs_exist=sum(md(k, pl) for k in ks) / sum(md(k, ex) for k in ks),
                       decode_gate_vs_exist=sum(md(k, gate[k]) for k in ks) / sum(md(k, ex) for k in ks))
            for c in ('exist_prod', 'planes_prod', 'oracle_gate', 'oracle_broad'): row[c + '_vs_production_pct'] = 100 * (row[c] / P - 1)
            ctx = collections.defaultdict(lambda: [0, 0, 0])                     # amplification context: residual width x order
            for k in ks:
                ib = [b['int'] for b in fr[k]['blocks_detail'] if 'int' in b]
                key = f"max_ow{max(b['ow'] for b in ib)}_order{max(b['order'] for b in ib)}"
                ctx[key][0] += 1; ctx[key][1] += tot[k][ex]; ctx[key][2] += tot[k][pl]
            row['planes_gain_by_context'] = {c: dict(files=v[0], change_pct=100 * (v[2] / v[1] - 1)) for c, v in sorted(ctx.items())}
            out[f'{name} | {mode}'] = row
    print(json.dumps(out, indent=1, default=str))

if __name__ == '__main__': main(sys.argv[1:4])
