#!/usr/bin/env python3
"""Derive P2-E6 mixed development CSVs (integer, fixed-scale decimal and text columns) from the E02C development corpus.
Observed-data derivatives: development only. usage: make_mixed.py CORPUS_DIR OUT_DIR"""
import sys, datetime, hashlib, json, pathlib, numpy as np
src, out = map(pathlib.Path, sys.argv[1:3]); out.mkdir(parents=True, exist_ok=True)
def ld(f): return np.load(src / f).astype(np.int64)
def write(name, header, rows):
    b = (header + '\n' + ''.join(','.join(r) + '\n' for r in rows)).encode()
    (out / name).write_bytes(b); return dict(name=name, rows=len(rows), bytes=len(b), sha256=hashlib.sha256(b).hexdigest())
sz, mt, ino = ld('fs_size_u32.npy'), ld('fs_mtime_i64.npy'), ld('fs_inode_u64.npy')
kind = lambda s: 'empty' if s == 0 else 'small' if s < 4096 else 'medium' if s < 1 << 20 else 'large'
fs = [(str(i), kind(int(s)), '%d.%03d' % (s // 1000, s % 1000), datetime.datetime.utcfromtimestamp(int(m) // 10**9).strftime('%Y-%m-%dT%H:%M:%S'), str(int(n)))
      for i, (s, m, n) in enumerate(zip(sz, mt, ino))]
dt, vol = ld('goog_date_i64.npy'), ld('goog_volume_i64.npy')
goog = [((datetime.date(1970, 1, 1) + datetime.timedelta(days=int(d))).isoformat(), 'GOOG', str(int(v)), '%d.%02d' % (v // 100000, (v // 1000) % 100)) for d, v in zip(dt, vol)]
walk = ld('syn_walk_i32.npy')
syn = [(str(i), 'sensor-%d' % (i % 7), '%d.%d' % (w // 10, abs(w) % 10) if w >= 0 else '-%d.%d' % (abs(w) // 10, abs(w) % 10), '' if i % 13 == 0 else 'ok') for i, w in enumerate(walk[:200000])]
m = [write('fs_mixed.csv', 'id,kind,size_kb,mtime,inode', fs), write('goog_mixed.csv', 'date,ticker,volume,turnover_k', goog),
     write('syn_mixed.csv', 'id,sensor,value,status', syn)]
(out / 'manifest.json').write_text(json.dumps(dict(split='development (derived from observed E02C inputs)', inputs=m), indent=1) + '\n')
print(json.dumps(m, indent=1))
