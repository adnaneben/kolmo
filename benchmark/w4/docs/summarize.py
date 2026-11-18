#!/usr/bin/env python3
"""Summarise run.py rows: each Kolmo level against its rival, per group and per file.
usage: summarize.py ROWS.jsonl [--files] [--base OLD.jsonl]
size = Kolmo bytes / rival bytes - 1 (negative: Kolmo smaller); c, d = Kolmo MB/s / rival MB/s (group: total bytes over
total time); rss = Kolmo peak RSS / rival peak RSS (the largest ratio over the group's files). --base adds the Kolmo
size change against an older run of the same files."""
import json, sys, collections
GROUPS = {'text': ['enwik8', 'dickens', 'webster', 'reymont'], 'xml': ['xml'], 'source': ['samba', 'source.tar'],
          'csv': ['owid-co2-data.csv', 'owid-energy-data.csv', 'covid20.csv'],
          'json': ['citm_catalog.json', 'twitter.json', 'github_events.json', 'gharchive.ndjson'], 'logs': ['loghub_all.log']}
PAIRS = [('kolmo speed', 'lz4 -1'), ('kolmo balanced', 'zstd -3'), ('kolmo eco', 'brotli -11'), ('kolmo hc', 'zstd -19 --long=27')]
rows = [json.loads(l) for l in open(sys.argv[1])]
base = {}
if '--base' in sys.argv:
    for l in open(sys.argv[sys.argv.index('--base') + 1]):
        r = json.loads(l); base[(r['file'], r['tool'], r['threads'])] = r
R = {(r['file'], r['tool'], r['threads']): r for r in rows}
threads = sorted({r['threads'] for r in rows})
def agg(fs, tool, T):
    rs = [R.get((f, tool, T)) for f in fs]
    if any(r is None for r in rs): return None
    n = sum(r['input'] for r in rs)
    return dict(size=sum(r['compressed'] for r in rs), c=n / sum(r['input'] / r['c_mbs'] for r in rs),
                d=n / sum(r['input'] / r['d_mbs'] for r in rs), rss=[r['c_rss'] for r in rs], drss=[r['d_rss'] for r in rs])
for T in threads:
    print(f'\n### {T} thread{"s" if T > 1 else ""}\n')
    print('| group | Kolmo | rival | size | compress | decompress | peak RSS c / d (worst file) |' + (' vs base |' if base else ''))
    print('|---|---|---|---:|---:|---:|---:|' + ('---:|' if base else ''))
    for g, fs in GROUPS.items():
        for k, rv in PAIRS:
            a, b = agg(fs, k, T), agg(fs, rv, T)
            if not a or not b: continue
            rc = max(x / y for x, y in zip(a['rss'], b['rss'])); rd = max(x / y for x, y in zip(a['drss'], b['drss']))
            extra = ''
            if base:
                ob = [base.get((f, k, T)) for f in fs]
                extra = f" {sum(R[(f, k, T)]['compressed'] for f in fs) / sum(o['compressed'] for o in ob) - 1:+.1%} |" if all(ob) else ' |'
            print(f"| {g} | {k[6:]} | {rv} | {a['size'] / b['size'] - 1:+.1%} | {a['c'] / b['c']:.2f}x | {a['d'] / b['d']:.2f}x | {rc:.2f}x / {rd:.2f}x |{extra}")
    if '--files' in sys.argv:
        print(f'\n| file | Kolmo | bytes | MB/s c / d | RSS c MB | rival bytes | MB/s c / d | RSS c MB | size | c | d |')
        print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
        for fs in GROUPS.values():
            for f in fs:
                for k, rv in PAIRS:
                    a, b = R.get((f, k, T)), R.get((f, rv, T))
                    if not a or not b: continue
                    print(f"| {f} | {k[6:]} | {a['compressed']:,} | {a['c_mbs']:.1f} / {a['d_mbs']:.0f} | {a['c_rss'] >> 20} | {b['compressed']:,} | {b['c_mbs']:.1f} / {b['d_mbs']:.0f} | {b['c_rss'] >> 20} | {a['compressed'] / b['compressed'] - 1:+.1%} | {a['c_mbs'] / b['c_mbs']:.2f}x | {a['d_mbs'] / b['d_mbs']:.2f}x |")
