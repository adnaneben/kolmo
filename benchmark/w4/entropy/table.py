#!/usr/bin/env python3
"""table.py FILE.jsonl...: markdown tables of e2e.py rows (A = baseline b61b805, B = this branch, rivals)."""
import json, sys
rows = [json.loads(l) for f in sys.argv[1:] for l in open(f)]
keys = []
for r in rows:
    k = (r['file'], r['mode'], r['threads'])
    if k not in keys: keys.append(k)
print('| file | mode | T | tool | bytes | vs A | decode s | decode MB/s | dec RSS MB | compress s |')
print('|---|---|---:|---|---:|---:|---:|---:|---:|---:|')
for k in keys:
    g = [r for r in rows if (r['file'], r['mode'], r['threads']) == k]
    a = next((r for r in g if r['tool'] == 'A'), None)
    for r in g:
        rel = '%+.2f%%' % (100 * (r['size'] / a['size'] - 1)) if a else ''
        print('| %s | %s | %d | %s | %d | %s | %.3f | %.0f | %.0f | %.2f |%s' % (r['file'], r['mode'], r['threads'], 'rc3 (A)' if r['tool'] == 'A' else 'w4 (B)' if r['tool'] == 'B' else r['tool'],
              r['size'], rel, r['d_s'], r['raw'] / r['d_s'] / 1e6, r['d_rss_kb'] / 1024, r['c_s'], '' if r['ok'] else ' BAD'))
