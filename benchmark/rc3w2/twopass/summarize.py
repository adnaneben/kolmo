"""Markdown tables from e2e.csv: base against new per file, mode and threads (size change, encode and decode MB/s,
peak memory). python3 -I summarize.py e2e.csv"""
import csv, sys, collections
rows = collections.defaultdict(dict)
for r in csv.DictReader(open(sys.argv[1])): rows[(r['mode'], r['threads'], r['file'])][r['bin']] = r
print('| mode | T | file | base bytes | new bytes | change | encode MB/s base → new | decode MB/s base → new | enc MiB base → new | version |')
print('|---|---|---|---:|---:|---:|---|---|---|---|')
for (m, t, f), d in rows.items():
    if 'base' not in d or 'new' not in d: continue
    b, n = d['base'], d['new']; N = int(b['n']) / 1e6
    print(f"| {m} | {t} | {f} | {int(b['size']):,} | {int(n['size']):,} | {100.0 * (int(n['size']) - int(b['size'])) / int(b['size']):+.2f}% | "
          f"{N / float(b['enc_s']):.2f} → {N / float(n['enc_s']):.2f} | {N / float(b['dec_s']):.2f} → {N / float(n['dec_s']):.2f} | "
          f"{b['enc_mib']} → {n['enc_mib']} | {b['version']} → {n['version']} |")
