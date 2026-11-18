#!/usr/bin/env python3
"""Functions no target executed, from `llvm-cov report -show-functions` over several fuzz binaries: a static function
appears once per binary that contains it, and is listed here only when every copy has 0 executed regions."""
import re, sys
cur = None; seen = {}
for line in open(sys.argv[1]):
    m = re.match(r"File '(.*)':", line)
    if m: cur = m.group(1).split('/src/')[-1]; continue
    f = line.split()
    if len(f) >= 10 and f[0] not in ('Name', 'TOTAL') and f[3].endswith('%'):
        k = (cur, f[0].split(':')[-1]); seen[k] = seen.get(k, False) or f[3] != '0.00%'
out = {}
for (fn, name), hit in seen.items():
    if not hit: out.setdefault(fn, []).append(name)
for k in sorted(out): print('%s (%d): %s' % (k, len(out[k]), ', '.join(sorted(set(out[k])))))
