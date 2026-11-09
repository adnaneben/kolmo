#!/usr/bin/env python3
"""Aggregate complete lzbench CSV results using total bytes / total time."""
import csv
import collections
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
groups = collections.defaultdict(list)
for row in csv.DictReader(path.open()):
    if '/silesia/' in row['Filename']:
        groups[row['Compressor name']].append(row)
print('| Codec | Files | Ratio | Compress MB/s | Decompress MB/s |')
print('|---|---:|---:|---:|---:|')
for name, rows in groups.items():
    n = sum(int(r['Original size']) for r in rows)
    c = sum(int(r['Compressed size']) for r in rows)
    ct = sum(int(r['Original size']) / 1e6 / float(r['Compression speed']) for r in rows)
    dt = sum(int(r['Original size']) / 1e6 / float(r['Decompression speed']) for r in rows)
    print(f'| {name} | {len(rows)} | {n/c:.3f} | {n/1e6/ct:.1f} | {n/1e6/dt:.1f} |')
