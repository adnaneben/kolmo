"""matrix.csv -> markdown: per file, one row per configuration, single thread and 4 threads side by side.
usage: python3 matrix_tables.py matrix.csv"""
import csv, sys
from collections import OrderedDict

rows = list(csv.DictReader(open(sys.argv[1])))
files = list(OrderedDict.fromkeys(r['file'] for r in rows))
for f in files:
    print(f'\n#### {f}\n')
    print('| option | ratio (1 thr) | vs Huffman | MB/s comp / decomp (1 thr) | ratio (4 thr, blocks) | MB/s comp / decomp (4 thr) |')
    print('|---|---|---|---|---|---|')
    R = [r for r in rows if r['file'] == f]
    huf = float([r for r in R if r['config'].startswith('plain Huffman') and r['threads'] == '1'][0]['ratio'])
    for c in OrderedDict.fromkeys(r['config'] for r in R):
        a = [r for r in R if r['config'] == c and r['threads'] == '1']
        b = [r for r in R if r['config'] == c and r['threads'] == '4']
        a = a[0] if a else None; b = b[0] if b else None
        def fmt(x): return f"{float(x['enc_MBps']):,.1f} / {float(x['dec_MBps']):,.1f}" if x else '—'
        ra = f"{float(a['ratio']):.2f}" if a else '—'
        rel = f"{float(a['ratio']) / huf:.2f}x" if a else '—'
        rb = f"{float(b['ratio']):.2f} ({b['blocks']})" if b else '—'
        print(f'| {c} | {ra} | {rel} | {fmt(a)} | {rb} | {fmt(b)} |')
