import sys
S = sys.argv[1]
j = {}
for l in open(S + '/jxl_sizes.txt'):
    p = l.split()
    if len(p) == 4 and p[2].startswith('e7='): j[p[0]] = (int(p[1]), int(p[2][3:]), int(p[3][3:]))
def rd(fn):
    d = {}
    for l in open(S + '/' + fn):
        p = l.split(); d[p[0]] = int(p[-2])
    return d
base = {'balanced': rd('k_bal.txt'), 'hc': rd('k_hc.txt'), 'ratio': rd('k_ratio.txt'), '-x': rd('k_x.txt')}
ct = {'balanced': rd('ct_bal.txt'), 'hc': rd('ct_hc.txt'), 'ratio': rd('ct_ratio.txt'), '-x': rd('ct_x.txt')}
for lvl in ('balanced', 'hc', 'ratio', '-x'):
    print('== %s: plain (rc3) | subtract-green | chain | best of the three | change | JXL e9 | best vs e9' % lvl)
    tb = tn = te = 0
    for f in sorted(j):
        if not f.endswith('.ppm'): continue
        b = base[lvl][f]; sg = ct[lvl].get(f[:-4] + '.sg.ppm'); ch = ct[lvl].get(f[:-4] + '.ch.ppm')
        best = min(x for x in (b, sg, ch) if x); e9 = j[f][2]
        print('%-24s %8d %8s %8s %8d %+6.1f%% %8d %+6.1f%%' % (f, b, sg or '-', ch or '-', best, 100*(best/b-1), e9, 100*(best/e9-1)))
        tb += b; tn += best; te += e9
    print('%-24s %8d %17s %8d %+6.1f%% %8d %+6.1f%%  (rc3 vs e9 %+.1f%%)' % ('TOTAL RGB (10)', tb, '', tn, 100*(tn/tb-1), te, 100*(tn/te-1), 100*(tb/te-1)))
