#!/usr/bin/env python3
"""Summarize lzbench output without averaging ratios or MB/s arithmetically."""
import collections
import csv
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parents[2]
base = pathlib.Path(__file__).resolve().parent
all_rows = []
for name in sys.argv[1:]:
    path = pathlib.Path(name)
    rows = list(csv.DictReader(path.open()))
    if path.name == 'development.csv':
        assert len(rows)==250
    elif path.name == 'validation.csv':
        assert len(rows)==126
    elif path.name == 'board.csv':
        assert len(rows)==9
    elif path.name in ['scaling-1.csv','scaling-10.csv']:
        assert len(rows)==4
    elif path.name == 'ratio-leaders.csv':
        assert len(rows)==1
    groups = collections.defaultdict(list)
    keys=set()
    for r in rows:
        key=(r['Filename'],r['Compressor name']);assert key not in keys;keys.add(key)
        assert int(r['Original size'])==(root/r['Filename']).stat().st_size
        assert int(r['Compressed size'])>0 and float(r['Compression speed'])>0 and float(r['Decompression speed'])>0
        p=r['Filename']
        family='Silesia files' if '/silesia/' in p else 'Canterbury' if '/canterbury/' in p else 'Calgary14' if '/calgary/' in p else 'Board-linked tar' if p.endswith('/silesia.tar') else pathlib.Path(p).name
        if path.stem.startswith('scaling-'):
            family='Mozilla, 1MiB blocks'
        groups[(family,r['Compressor name'])].append(r)
    summaries=[]
    for (family,codec),selected in sorted(groups.items()):
        n=sum(int(r['Original size']) for r in selected)
        z=sum(int(r['Compressed size']) for r in selected)
        enc=n/sum(int(r['Original size'])/float(r['Compression speed']) for r in selected)
        dec=n/sum(int(r['Original size'])/float(r['Decompression speed']) for r in selected)
        summaries.append({'run':path.stem,'family':family,'codec':codec,'files':len(selected),'input_bytes':n,'compressed_bytes':z,'ratio':n/z,'encode_mbps':enc,'decode_mbps':dec})
    for r in summaries:
        # A local three-metric frontier, not a public-board placement.
        r['dominated_by']='; '.join(s['codec'] for s in summaries if s is not r and s['family']==r['family'] and s['ratio']>=r['ratio'] and s['encode_mbps']>=r['encode_mbps'] and s['decode_mbps']>=r['decode_mbps'] and (s['ratio']>r['ratio'] or s['encode_mbps']>r['encode_mbps'] or s['decode_mbps']>r['decode_mbps']))
        all_rows.append(r)
        print(f"{path.stem:14} {r['family']:17} {r['codec']:29} ratio={r['ratio']:.3f} encode={r['encode_mbps']:.2f} decode={r['decode_mbps']:.2f}")
if all_rows:
    with (base/'summary.csv').open('w') as out:
        writer=csv.DictWriter(out,fieldnames=list(all_rows[0]));writer.writeheader();writer.writerows(all_rows)
