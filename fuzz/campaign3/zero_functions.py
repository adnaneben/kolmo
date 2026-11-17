#!/usr/bin/env python3
"""List the functions with 0 executed regions from `llvm-cov report -show-functions` output, grouped by file."""
import re, sys
cur = None; out = {}
for line in open(sys.argv[1]):
    m = re.match(r"File '(.*)':", line)
    if m: cur = m.group(1).split('/src/')[-1]; continue
    f = line.split()
    if len(f) >= 10 and f[3] == '0.00%' and f[0] not in ('Name', 'TOTAL'):
        out.setdefault(cur, []).append(f[0].split(':')[-1])
for k, v in out.items(): print('%s (%d): %s' % (k, len(v), ', '.join(v)))
