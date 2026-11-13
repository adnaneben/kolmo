#!/usr/bin/env python3
"""Derive the P2-E5 development CSV tables from the E02C development corpus (observed data; development only).
usage: make_tables.py CORPUS_DIR OUT_DIR"""
import sys, numpy as np, hashlib, json, pathlib
src, out = map(pathlib.Path, sys.argv[1:3]); out.mkdir(parents=True, exist_ok=True)
def ld(f): return np.load(src / f).astype(np.int64)
def write(name, headers, cols):
    n = min(len(c) for c in cols); cols = [c[:n] for c in cols]
    b = ('\n'.join([','.join(headers)] + [','.join(str(int(x)) for x in row) for row in zip(*cols)]) + '\n').encode()
    (out / name).write_bytes(b); return dict(name=name, rows=n, columns=len(cols), bytes=len(b), sha256=hashlib.sha256(b).hexdigest())
m = [write('fs_table.csv', ['size', 'mtime_ns', 'inode'], [ld('fs_size_u32.npy'), ld('fs_mtime_i64.npy'), ld('fs_inode_u64.npy')]),
     write('goog_table.csv', ['date', 'volume'], [ld('goog_date_i64.npy'), ld('goog_volume_i64.npy')]),
     write('syn_table.csv', ['walk', 'sparse', 'clustered'], [ld('syn_walk_i32.npy'), ld('syn_sparse_outliers_u32.npy'), ld('syn_clustered_outliers_u32.npy')]),
     write('mri_rows.csv', ['c%d' % i for i in range(256)], [ld('mri_s1045_u16.npy').reshape(256, 256)[:, i] for i in range(256)]),
     write('dem_rows.csv', ['c%d' % i for i in range(403)], [ld('dem_jacksboro_i16.npy').reshape(344, 403)[:, i] for i in range(403)])]
(out / 'manifest.json').write_text(json.dumps(dict(split='development (derived from observed E02C inputs)', inputs=m), indent=1) + '\n')
print(json.dumps(m, indent=1))
