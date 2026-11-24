import sys
S = sys.argv[1]
j = {}
for l in open(S + '/jxl_sizes.txt'):
    p = l.split()
    if len(p) == 4 and p[2].startswith('e7='): j[p[0]] = (int(p[1]), int(p[2][3:]), int(p[3][3:]))
k = {}
for fn, key in (('k_bal.txt', 'bal'), ('k_ratio.txt', 'ratio'), ('k_x.txt', 'x')):
    for l in open(S + '/' + fn):
        p = l.split(); k.setdefault(p[0], {})[key] = int(p[-2]) if p[-1] == 'RT=ok' else None
tot = dict(n=0, e7=0, e9=0, bal=0, ratio=0, x=0)
print('%-24s %8s %8s %8s | %8s %7s | %8s %7s | %8s %7s' % ('file', 'bytes', 'jxl e7', 'jxl e9', 'balanced', 'vs e9', 'ratio', 'vs e9', '-x', 'vs e9'))
for f in sorted(j):
    n, e7, e9 = j[f]; r = k[f]
    print('%-24s %8d %8d %8d | %8d %+6.1f%% | %8d %+6.1f%% | %8d %+6.1f%%' % (f, n, e7, e9, r['bal'], 100*(r['bal']/e9-1), r['ratio'], 100*(r['ratio']/e9-1), r['x'], 100*(r['x']/e9-1)))
    tot['n'] += n; tot['e7'] += e7; tot['e9'] += e9
    for q in ('bal', 'ratio', 'x'): tot[q] += r[q]
print('%-24s %8d %8d %8d | %8d %+6.1f%% | %8d %+6.1f%% | %8d %+6.1f%%' % ('TOTAL', tot['n'], tot['e7'], tot['e9'], tot['bal'], 100*(tot['bal']/tot['e9']-1), tot['ratio'], 100*(tot['ratio']/tot['e9']-1), tot['x'], 100*(tot['x']/tot['e9']-1)))
for sub, name in ((lambda f: f.endswith('.ppm'), 'RGB (10)'), (lambda f: f.endswith('.pgm'), 'grey (7)')):
    t = {q: sum((k[f][q] if q in ('bal','ratio','x') else j[f][2]) for f in j if sub(f)) for q in ('e9', 'bal', 'ratio', 'x')}
    print(name, t, ' '.join('%s %+.1f%%' % (q, 100*(t[q]/t['e9']-1)) for q in ('bal','ratio','x')))
