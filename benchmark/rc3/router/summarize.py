#!/usr/bin/env python3
"""Tables from run.py's JSON lines: per type, each Kolmo mode's total size, ratio, MB/s and route, the leaders on the
same files, and the gap to the best leader in the mode's speed class.
usage: summarize.py MATRIX.jsonl [--tag kolmo] [--files]"""
import collections, json, os, sys

rows = [json.loads(l) for l in open(sys.argv[1])]
TAG = sys.argv[sys.argv.index('--tag') + 1] if '--tag' in sys.argv else 'kolmo'
bad = [r for r in rows if 'error' in r or not r.get('ok', True)]
for r in bad: print('FAILED', r['tool'], r['file'], r.get('error', 'mismatch')[:120])
rows = [r for r in rows if 'error' not in r and r.get('ok')]
by = collections.defaultdict(dict)
for r in rows: by[(r['type'], r['file'])][r['tool']] = r
types = sorted({t for t, _ in by})
MODES = ['speed', 'balanced', 'eco', 'hc', 'ratio', 'x']
# the speed class each mode is compared within (rival tools)
RIVALS = {'speed': ['lz4-1'], 'balanced': ['zstd-3'], 'eco': ['brotli-11', 'zstd-19'], 'hc': ['zstd-19', 'zstd-22'],
          'ratio': ['xz-9e', 'bzip2-9', 'zstd-22', 'brotli-11'], 'x': ['xz-9e', 'bzip2-9', 'zstd-22', 'brotli-11']}
SPEC = ['flac-8', 'ffv1', 'jxl-e9', 'jpegls', 'webp-ll']


def tot(files, tool, key='size'):
    v = [by[f].get(tool) for f in files]
    if any(x is None for x in v): return None
    return sum(x[key] for x in v)


def mbs(n, t): return n / t / 1e6 if t else float('inf')


print('# Routing matrix\n')
for t in types:
    files = sorted(f for (tt, f) in by if tt == t)
    n = sum(by[(t, f)][next(iter(by[(t, f)]))]['n'] for f in files)
    keys = [(t, f) for f in files]
    print('## %s (%d files, %.1f MB): %s\n' % (t, len(files), n / 1e6, ', '.join(os.path.basename(f) for f in files)))
    print('| tool | ratio | comp MB/s | dec MB/s | vs best rival | route (share of input bytes) |')
    print('|---|---:|---:|---:|---:|---|')
    tools = ['%s-%s' % (TAG, m) for m in MODES] + list(RIVALS['x']) + ['zstd-3', 'zstd-19', 'lz4-1'] + SPEC
    seen = set()
    for tool in tools:
        if tool in seen: continue
        seen.add(tool)
        s = tot(keys, tool)
        if s is None: continue
        c, d = tot(keys, tool, 'c'), tot(keys, tool, 'd')
        gap = ''
        if tool.startswith(TAG + '-'):
            m = tool[len(TAG) + 1:]
            rv = [(tot(keys, x), x) for x in RIVALS[m] + SPEC]
            rv = [x for x in rv if x[0] is not None]
            if rv:
                b = min(rv); gap = '%+.1f%% (%s)' % (100 * (s / b[0] - 1), b[1])
        route = ''
        if tool.startswith(TAG + '-'):
            route = '; '.join('%s: %s' % (os.path.basename(f), by[(t, f)][tool].get('route', '')) for f in files)
        print('| %s | %.3f | %.1f | %.1f | %s | %s |' % (tool, n / s, mbs(n, c), mbs(n, d), gap, route))
    print()
    if '--files' in sys.argv:
        for f in files:
            line = ['%s:%s' % (k, v['size']) for k, v in sorted(by[(t, f)].items())]
            print('   ', os.path.basename(f), ' '.join(line))
        print()
