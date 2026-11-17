#!/usr/bin/env python3
"""Compact per-type tables for docs/ROUTING-MAP.md from run.py's JSON lines: for each Kolmo mode the route it took
(the transform+codec covering most input bytes), ratio, compress/decompress MB/s (one thread, four jobs at once), and
the gap to the best general-purpose tool and to the type's specialist.
usage: maptables.py MATRIX.jsonl [XNEW.jsonl]   (XNEW: -x rows of a newer build replace the baseline's)"""
import collections, json, os, sys

rows = [json.loads(l) for l in open(sys.argv[1])]
if len(sys.argv) > 2:
    new = {(r['file']): r for r in map(json.loads, open(sys.argv[2])) if 'size' in r}
    for r in rows:
        if r['tool'] == 'kolmo-x' and r['file'] in new:
            # sizes and routes from the newer build; times from the baseline run when the frame size is unchanged
            # (the newer run overlapped other work), the newer run's times where the frame changed
            n = new[r['file']]
            if n['size'] != r['size']: r.update(c=n['c'], d=n['d'])
            r.update(size=n['size'], route=n.get('route', ''))
rows = [r for r in rows if 'size' in r and r.get('ok')]
by = collections.defaultdict(dict)
for r in rows: by[r['type']].setdefault(r['tool'], []).append(r)
GEN = ['zstd-3', 'zstd-19', 'zstd-22', 'xz-9e', 'brotli-11', 'bzip2-9', 'lz4-1']
SPEC = ['flac-8', 'ffv1', 'jxl-e9', 'jpegls']
MODES = ['speed', 'balanced', 'eco', 'hc', 'ratio', 'x']
RIVAL = {'speed': 'lz4-1', 'balanced': 'zstd-3', 'eco': 'brotli-11', 'hc': 'zstd-19', 'ratio': 'xz-9e', 'x': 'xz-9e'}


def total(rs, k): return sum(r[k] for r in rs)


def route(rs):
    agg = collections.Counter()
    for r in rs:
        parts = r.get('route', '').split()[1:]
        for p in parts:
            k, v = p.rsplit(':', 1); agg[k] += r['n'] * float(v.rstrip('%')) / 100
    tot = sum(agg.values()) or 1
    return ', '.join('%s %.0f%%' % (k, 100 * v / tot) for k, v in agg.most_common(3) if v / tot >= 0.05)


for t in sorted(by):
    T = by[t]; files = sorted({os.path.basename(r['file']) for r in T['kolmo-balanced']})
    n = total(T['kolmo-balanced'], 'n')
    best_gen = min((total(T[g], 'size'), g) for g in GEN if g in T and len(T[g]) == len(files))
    specs = [(total(T[s], 'size'), s) for s in SPEC if s in T and len(T[s]) == len(files)]
    best_spec = min(specs) if specs else None
    print('### %s\n\n%d files, %.1f MB: %s. Best general tool: %s (%.2f×, %.1f / %.1f MB/s)%s.\n' % (
        t, len(files), n / 1e6, ', '.join(files), best_gen[1], n / best_gen[0],
        n / total(T[best_gen[1]], 'c') / 1e6, n / total(T[best_gen[1]], 'd') / 1e6,
        '; specialist: %s (%.2f×, %.1f / %.1f MB/s)' % (best_spec[1], n / best_spec[0], n / total(T[best_spec[1]], 'c') / 1e6,
                                                          n / total(T[best_spec[1]], 'd') / 1e6) if best_spec else ''))
    print('| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |')
    print('|---|---|---:|---:|---:|---:|---:|')
    for m in MODES:
        k = 'kolmo-' + m
        if k not in T or len(T[k]) != len(files): continue
        s = total(T[k], 'size')
        rv = RIVAL[m]; rs = total(T[rv], 'size') if rv in T and len(T[rv]) == len(files) else None
        print('| %s | %s | %.2f | %.1f / %.1f | %s | %+.1f%% | %s |' % (
            m if m != 'x' else '-x', route(T[k]), n / s, n / total(T[k], 'c') / 1e6, n / total(T[k], 'd') / 1e6,
            '%+.1f%% (%s)' % (100 * (s / rs - 1), rv) if rs else '—',
            100 * (s / best_gen[0] - 1), '%+.1f%%' % (100 * (s / best_spec[0] - 1)) if best_spec else '—'))
    print()
