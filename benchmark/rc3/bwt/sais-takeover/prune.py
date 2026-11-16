#!/usr/bin/env python3
"""Remove C function definitions by name from libsais.c. A definition starts at a column-0 line that declares the
function (static or not) and ends at the next column-0 '}' line. Also removes the preceding blank line."""
import re, sys
path, names = sys.argv[1], set(sys.argv[2:])
lines = open(path).read().split('\n')
out = []; i = 0; removed = []
pat = re.compile(r'^(?:static\s+)?(?:INLINE\s+|LIBSAIS_[A-Z_]*\s+)*[A-Za-z_][A-Za-z0-9_ \*]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
while i < len(lines):
    m = pat.match(lines[i])
    if m and m.group(1) in names and not lines[i].rstrip().endswith(';'):
        j = i
        while j < len(lines) and lines[j] != '}': j += 1
        removed.append(m.group(1))
        while out and out[-1].strip() == '': out.pop()
        out.append('')
        i = j + 1
        continue
    out.append(lines[i]); i += 1
open(path, 'w').write('\n'.join(out))
print("removed", len(removed), "missing", sorted(names - set(removed)))
