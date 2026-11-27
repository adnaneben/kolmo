#!/usr/bin/env python3
"""X-01: per-plane leaves vs an E03-style costed native baseline, from committed raw records only (no new runs).

`exist+ent` = original residual layout, per leaf the smaller of the existing Huffman-only and rANS coders with stored
fallback, all trials charged: the same policy as E03's native_base, on Kolmo's complete-planner frames instead of the
E2C1 envelope. `planes+ent` = the same policy per residual byte plane (one stream per plane).
usage: x01.py   (from the repository root)
"""
import collections, json, lzma, statistics as st

COHORTS = {'dev (E02C-harness run 3)': ('research/codec/E02C-harness/results-followup2/raw.jsonl.xz', 'research/codec/E02C-harness/corpus/manifest.json'),
           'E02E': ('research/codec/E02E/results/raw.jsonl.xz', 'research/codec/E02E/results/manifest.json'),
           'E02F': ('research/codec/E02F/results/raw.jsonl.xz', 'research/codec/E02F/results/manifest.json')}
CONFS = ['exist+ent', 'planes+ent', 'exist+prod3', 'planes+prod3', 'exist+prod19', 'planes+prod19']

def main():
    res = {}
    for name, (raw, man) in COHORTS.items():
        M = {i['name']: i for i in json.load(open(man))['inputs']}
        fr, tot, dec = {}, collections.defaultdict(dict), collections.defaultdict(list)
        for l in lzma.open(raw, 'rt'):
            r = json.loads(l)
            if r['kind'] == 'planner_frame': fr[(r['input'], r['mode'])] = r
            elif r['kind'] == 'planner_total': tot[(r['input'], r['mode'])][r['config']] = r['total']
            elif r['kind'] == 'e2e_time' and r['what'] in ('exp_decode_seq', 'prod_decode_seq'): dec[(r['input'], r['mode'], r.get('config') or 'prod')].append(r['wall_ns'])
        for mode in ('fast', 'hc'):
            ks = [k for k in fr if k[1] == mode and fr[k]['int_blocks'] and M[k[0]]['origin'] != 'synthetic']
            if not ks: continue
            P = sum(fr[k]['frame'] for k in ks); pd = sum(st.median(dec[(k[0], mode, 'prod')]) for k in ks)
            row = {c: dict(bytes=sum(tot[k][c] for k in ks)) for c in CONFS}
            for c in CONFS:
                row[c]['vs_production_pct'] = 100 * (row[c]['bytes'] / P - 1)
                row[c]['decode_vs_production'] = sum(st.median(dec[(k[0], mode, c)]) for k in ks) / pd
            fams = collections.defaultdict(list)
            for k in ks:
                if any(b['int']['ow'] >= 2 for b in fr[k]['blocks_detail'] if 'int' in b): fams[M[k[0]]['family']].append(k)
            res[f'{name} | {mode}'] = dict(int_files=len(ks), production=P, configs=row,
                planes_ent_vs_exist_ent_pct=100 * (row['planes+ent']['bytes'] / row['exist+ent']['bytes'] - 1),
                multibyte_families={f: dict(files=len(v), planes_ent_vs_exist_ent_pct=100 * (sum(tot[k]['planes+ent'] for k in v) / sum(tot[k]['exist+ent'] for k in v) - 1),
                                            exist_ent_vs_production_pct=100 * (sum(tot[k]['exist+ent'] for k in v) / sum(fr[k]['frame'] for k in v) - 1),
                                            planes_ent_vs_production_pct=100 * (sum(tot[k]['planes+ent'] for k in v) / sum(fr[k]['frame'] for k in v) - 1)) for f, v in sorted(fams.items())})
    print(json.dumps(res, indent=1))

if __name__ == '__main__': main()
