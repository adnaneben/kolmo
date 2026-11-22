"""Resolve `#if defined(LIBSAIS_OPENMP)` blocks as true (keep the if branch, drop #else branches and directives)."""
import re, sys
p = sys.argv[1]; L = open(p).read().split('\n'); out = []
stack = []   # entries: 'omp' (resolving) or 'other'; for omp: state 'if' or 'else'
def emitting():
    return all(not (e[0] == 'omp' and e[1] == 'else') for e in stack)
for l in L:
    t = l.strip()
    if re.match(r'#\s*if', t):
        if t == '#if defined(LIBSAIS_OPENMP)':
            stack.append(['omp', 'if']); continue
        stack.append(['other', None])
        if emitting(): out.append(l)
        continue
    if re.match(r'#\s*(else|elif)', t) and stack and stack[-1][0] == 'omp':
        assert t == '#else', l
        stack[-1][1] = 'else'; continue
    if re.match(r'#\s*endif', t):
        e = stack.pop()
        if e[0] == 'omp': continue
        if emitting(): out.append(l)
        continue
    if emitting(): out.append(l)
assert not stack
s = '\n'.join(out)
s = re.sub(r'\n{3,}', '\n\n', s)
open(p, 'w').write(s)
