#!/usr/bin/env python3
"""Tables of benchmark/w4/cm/RESULTS.md from the measurement rows (JSON lines: file, n, tool, threads, size, c_s, d_s,
c_rss, d_rss, ok), as written by the lane's runner (one process per tool and direction, wall time, peak RSS).
usage: summarize.py ROWS.jsonl..."""
import json, sys
GROUPS = {'text': ['enwik8', 'dickens', 'webster', 'reymont', 'xml'],
          'Silesia (12 files, each coded alone)': ['dickens', 'mozilla', 'mr', 'nci', 'ooffice', 'osdb', 'reymont', 'samba', 'sao', 'webster', 'x-ray', 'xml'],
          'Silesia binaries': ['mozilla', 'samba', 'nci', 'ooffice', 'osdb'],
          'structured': ['loghub_all.log', 'countries-aggregated.csv', 'owid-co2-data.csv'],
          'numeric': ['x-ray', 'mr', 'sao']}
rows = {}
for p in sys.argv[1:]:
    for l in open(p):
        l = l.strip()
        if not l.startswith('{'): continue
        r = json.loads(l.replace("'", '"').replace('True', 'true').replace('False', 'false'))
        rows[(r['file'], r['tool'].split()[0], str(r['threads']))] = r      # later rows replace earlier ones
for T in ('1', '4'):
    if not any(k[2] == T for k in rows): continue
    print(f'\n### {T} thread{"s" if T != "1" else ""}\n')
    print('| group | file | MB | kolmo -x | zpaq -m5 | size | compress s (x) | decompress s (x) | peak RSS MiB, c / d | RSS x |')
    print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|')
    for g, files in GROUPS.items():
        tk = tz = 0; ck = cz = dk = dz = 0.0; have = 0
        for f in files:
            k, z = rows.get((f, 'kolmo', T)), rows.get((f, 'zpaq', T))
            if not k or not z: continue
            have += 1; tk += k['size']; tz += z['size']; ck += k['c_s']; cz += z['c_s']; dk += k['d_s']; dz += z['d_s']
            ok = '' if k['ok'] and z['ok'] else ' (ROUND TRIP FAILED)'
            print(f"| {g} | {f}{ok} | {k['n'] / 1e6:.1f} | {k['size']:,} | {z['size']:,} | {100.0 * (k['size'] - z['size']) / z['size']:+.1f}% | "
                  f"{k['c_s']:.1f} / {z['c_s']:.1f} ({z['c_s'] / k['c_s']:.2f}x) | {k['d_s']:.1f} / {z['d_s']:.1f} ({z['d_s'] / k['d_s']:.2f}x) | "
                  f"{k['c_rss'] >> 20} / {k['d_rss'] >> 20} vs {z['c_rss'] >> 20} / {z['d_rss'] >> 20} | {max(k['c_rss'], k['d_rss']) / max(z['c_rss'], z['d_rss']):.2f} |")
        if have:
            print(f"| **{g}** | **{have} files** | | **{tk:,}** | **{tz:,}** | **{100.0 * (tk - tz) / tz:+.1f}%** | **{cz / ck:.2f}x** | **{dz / dk:.2f}x** | | |")
print('\nspeed ratios: zpaq time / Kolmo time (above 1: Kolmo faster); RSS x: Kolmo peak / zpaq peak (largest of the two directions)')
