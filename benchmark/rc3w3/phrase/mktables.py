#!/usr/bin/env python3
"""Tables of RESULTS.md from results/*.jsonl (rc3w3 phrase lane). Run from the repository root:
   python3 -I benchmark/rc3w3/phrase/mktables.py"""
import json, glob, os, collections

B = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results')
GROUPS = [('prose', ['enwik8_1m', 'enwik8_10m', 'enwik8', 'dickens', 'webster', 'reymont']), ('XML', ['xml']),
          ('logs', ['loghub_all.log', 'Apache.log']), ('CSV', ['owid-co2-data.csv', 'countries-aggregated.csv']),
          ('JSON', ['twitter.json', 'citm_catalog.json', 'gharchive.json'])]
G6 = {'countries-aggregated.csv': 282986, 'owid-co2-data.csv': 1127785, 'loghub_all.log': 272847, 'Apache.log': 99500,
      'citm_catalog.json': 22020}       # GR6+ alone (benchmark/rc3/gr6/g6tool.c), whole file

def load(pattern):
    for p in sorted(glob.glob(os.path.join(B, pattern))):
        for l in open(p):
            if l.startswith('{'): yield json.loads(l)

def kolmo(threads):
    k = collections.defaultdict(dict)
    for j in load('t%d.jsonl' % threads): k[j['file']][j['mode']] = j
    return k

cm, sub, adapt, ctxd, null = {}, collections.defaultdict(list), {}, {}, {}
for j in load('full/*.jsonl'):
    f, v = j['file'], j['variant']
    if v == 'cm': cm[f] = j['size']
    elif v == 'phrase+cm': sub[f].append(j['size'])     # unmodified CM only (sub+cmx carries the study CM's extra models)
    elif v == 'cmx-adapt': adapt[f] = j['size']
    elif v == 'cmx-dict': ctxd[f] = j['size']
    elif v == 'cmx-null': null[f] = j['size']
# 1 MB slices (enwik8_1m is the whole first MB)
for j in load('s1m/base.jsonl'):
    if j['variant'] == 'cm' and j['file'] == 'enwik8_1m': cm['enwik8_1m'] = j['size']
for p in ['s1m/g_*.jsonl', 's1m/h_*.jsonl']:
    for j in load(p):
        if j['file'] == 'enwik8_1m' and j['variant'] == 'phrase+cm': sub['enwik8_1m'].append(j['size'])
for j in load('s1m/c_u50.jsonl'):
    if j['file'] == 'enwik8_1m' and j['variant'] == 'cmx-null': null['enwik8_1m'] = j['size']
for j in load('s1m/a_v2T8.jsonl'):
    if j['file'] == 'enwik8_1m': adapt['enwik8_1m'] = j['size']

k1 = kolmo(1)
pc = lambda a, b: '%+.2f%%' % (100.0 * (a - b) / b)
print('### Sizes: Kolmo modes against the phrase variants\n')
print('| group | file | bytes | `-x` | `-m hc` (word transform) | GR6+ alone | plain CM | best phrase substitution vs `-x` | two extra contexts, no phrases (cmx-null) vs CM | phrase contexts, sent dictionary, vs cmx-null | adaptive phrase context vs cmx-null |')
print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
tot = {}
for g, files in GROUPS:
    for f in files:
        x = k1.get(f, {}).get('-x'); h = k1.get(f, {}).get('-m hc')
        if not x: continue
        bs = min(sub[f]) if sub.get(f) else None
        cells = [g, f, '{:,}'.format(x['n']), '{:,}'.format(x['size']), '{:,}'.format(h['size']) if h else '',
                 '{:,}'.format(G6[f]) if f in G6 else '', '{:,}'.format(cm[f]) if f in cm else '',
                 pc(bs, x['size']) if bs else '', pc(null[f], cm[f]) if f in null and f in cm else '',
                 pc(ctxd[f], null[f]) if f in ctxd and f in null else '',
                 pc(adapt[f], null[f]) if f in adapt and f in null else (pc(adapt[f], cm[f]) + ' (vs CM)' if f in adapt and f in cm else '')]
        print('| ' + ' | '.join(cells) + ' |')
        t = tot.setdefault(g, [0, 0, 0, 0])
        t[0] += x['size']; t[1] += min(x['size'], bs) if bs else x['size']
        if f in adapt: r = null.get(f, cm.get(f)); t[2] += r; t[3] += adapt[f]
print('\n### Per group: best of `-x` and phrase substitution (kept per file only when smaller), adaptive context\n')
print('| group | `-x` bytes | best of `-x` / substitution | change | adaptive context vs cmx-null (files measured) |')
print('|---|---:|---:|---:|---:|')
for g, _ in GROUPS:
    if g not in tot: continue
    t = tot[g]
    print('| %s | {:,} | {:,} | %s | %s |'.format(t[0], t[1]) % (g, pc(t[1], t[0]), pc(t[3], t[2]) if t[2] else ''))

for th in (1, 4):
    k = kolmo(th)
    if not k: continue
    print('\n### Kolmo at %d thread%s (wall seconds, compress / decompress)\n' % (th, '' if th == 1 else 's'))
    print('| file | `-x` bytes | `-x` s | `-m hc` bytes | `-m hc` s |')
    print('|---|---:|---:|---:|---:|')
    for g, files in GROUPS:
        for f in files:
            if f not in k: continue
            x, h = k[f].get('-x'), k[f].get('-m hc')
            print('| %s | %s | %s | %s | %s |' % (f, '{:,}'.format(x['size']) if x else '', '%.1f / %.1f' % (x['enc_s'], x['dec_s']) if x else '',
                                              '{:,}'.format(h['size']) if h else '', '%.2f / %.2f' % (h['enc_s'], h['dec_s']) if h else ''))
