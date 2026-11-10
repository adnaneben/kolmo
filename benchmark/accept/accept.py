#!/usr/bin/env python3
"""Release acceptance check. Reads a results file written by the Mac benchmark (list of
{file, n, set, size, tc, td, ok}) and decides whether the candidate may be released.

  accept.py results.json [candidate-label] [baseline-label]      (defaults: cand, 1.0.1)

A candidate is accepted only when all three hold:
  A. Every rival in the file is beaten by one kolmo setting on the whole set: ratio at least equal,
     encode and decode speed at least 98% of the rival's, and better on at least one of the three.
  B. No file whose compressed size is unchanged from the baseline encodes or decodes more than 5% slower.
  C. Per setting, against the baseline: geometric-mean ratio not lower, encode and decode not below 97%.
  D. No file is larger than with the baseline at the same setting.
Ratio is averaged over all files; speeds over files of 1 MB or more (smaller files measure start-up).
Exit status 0 = accepted, 1 = not yet."""
import json, math, sys, collections
R = json.load(open(sys.argv[1])); cand = sys.argv[2] if len(sys.argv) > 2 else 'cand'; base = sys.argv[3] if len(sys.argv) > 3 else '1.0.1'
D = collections.defaultdict(dict)
for r in R:
    if not r['ok']: print('ROUND TRIP FAILURE', r['file'], r['set']); sys.exit(1)
    D[r['set']][r['file']] = (r['n'] / r['size'], r['n'] / 1e6 / r['tc'], r['n'] / 1e6 / r['td'], r['n'], r['size'])
gm = lambda v: math.exp(sum(map(math.log, v)) / len(v)) if v else float('nan')
def summ(s, files=None):
    v = [x for f, x in D[s].items() if files is None or f in files]; big = [x for x in v if x[3] >= 1e6]
    return gm([x[0] for x in v]), gm([x[1] for x in big]), gm([x[2] for x in big])
REDEFINED = ('max -l 1',)   # settings whose meaning changed since the baseline: compared with rivals only
ours = [s for s in D if s.startswith('kolmo ' + cand + ' ')]; rivals = [s for s in D if not s.startswith('kolmo ')]
if not ours: print('no rows for candidate label', cand); sys.exit(1)
fail = 0
print('A. rivals (ratio, encode MB/s, decode MB/s)')
for rv in rivals:
    files = set(D[rv]); a = summ(rv); best = None
    for s in ours:
        if not files <= set(D[s]): continue
        b = summ(s, files)
        if b[0] >= a[0] and b[1] >= 0.98 * a[1] and b[2] >= 0.98 * a[2] and (b[0] > a[0] * 1.001 or b[1] > a[1] * 1.02 or b[2] > a[2] * 1.02):
            if best is None or b[0] > best[1][0]: best = (s, b)
    if best: print(f"  beaten   {rv:16s} {a[0]:6.2f} {a[1]:7.0f} {a[2]:7.0f}   by {best[0][6:]:16s} {best[1][0]:6.2f} {best[1][1]:7.0f} {best[1][2]:7.0f}")
    else:
        fail += 1; near = max(ours, key=lambda s: min(summ(s, files)[0] / a[0], summ(s, files)[1] / a[1], summ(s, files)[2] / a[2])); b = summ(near, files)
        print(f"  OPEN     {rv:16s} {a[0]:6.2f} {a[1]:7.0f} {a[2]:7.0f}   closest {near[6:]:16s} {b[0]:6.2f} {b[1]:7.0f} {b[2]:7.0f}  (x{b[0]/a[0]:.2f} ratio, x{b[1]/a[1]:.2f} enc, x{b[2]/a[2]:.2f} dec)")
print('B. unchanged output must not be more than 5% slower than', base)
nb = 0
for s in ours:
    o = 'kolmo ' + base + ' ' + s[len('kolmo ' + cand + ' '):]
    if s.endswith(REDEFINED): continue
    for f, x in D[s].items():
        y = D.get(o, {}).get(f)
        if y and x[3] >= 1e6 and x[4] == y[4] and (x[1] < 0.95 * y[1] or x[2] < 0.95 * y[2]):
            nb += 1; print(f"  SLOWER   {s[6:]:16s} {f:28s} enc x{x[1]/y[1]:.2f} dec x{x[2]/y[2]:.2f}")
print('  none' if not nb else f'  {nb} case(s)'); fail += nb
print('C. per setting against', base, '(ratio, encode, decode multiples)')
for s in ours:
    o = 'kolmo ' + base + ' ' + s[len('kolmo ' + cand + ' '):]
    if o not in D or s.endswith(REDEFINED): continue
    files = set(D[s]) & set(D[o]); a = summ(o, files); b = summ(s, files); m = (b[0] / a[0], b[1] / a[1], b[2] / a[2])
    bad = m[0] < 0.999 or m[1] < 0.97 or m[2] < 0.97; fail += bad
    print(f"  {'BEHIND  ' if bad else 'ok      '} {s[6:]:16s} x{m[0]:.3f} x{m[1]:.3f} x{m[2]:.3f}")
print('D. no file may be larger than with', base, 'at the same setting')
nd = 0
for s in ours:
    o = 'kolmo ' + base + ' ' + s[len('kolmo ' + cand + ' '):]
    if o not in D or s.endswith(REDEFINED): continue
    for f, x in D[s].items():
        y = D[o].get(f)
        if y and x[4] > y[4]:
            nd += 1; print(f"  LARGER   {s[6:]:16s} {f:28s} {y[4]} -> {x[4]} bytes")
print('  none' if not nd else f'  {nd} case(s)'); fail += nd
print('ACCEPTED' if not fail else f'NOT ACCEPTED: {fail} open item(s)')
sys.exit(1 if fail else 0)
