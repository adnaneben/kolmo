"""Markdown table of -x sizes per file and build (lines `file build size` from sz.sh), relative to a base build."""
import sys, collections
rows = collections.defaultdict(dict); builds = []
for l in open(sys.argv[1]):
    p = l.split()
    if len(p) != 3: continue
    rows[p[0]][p[1]] = p[2]
    if p[1] not in builds: builds.append(p[1])
order = sys.argv[3].split(',') if len(sys.argv) > 3 else builds
base = sys.argv[2] if len(sys.argv) > 2 else order[0]
sz = ['10k', '100k', '1m', '10m']
print('| file | ' + ' | '.join(order) + ' |'); print('|---|' + '---:|' * len(order))
for f in sorted(rows, key=lambda x: (x.rsplit('_', 1)[0], sz.index(x.rsplit('_', 1)[1]))):
    r = rows[f]; b = r.get(base)
    cells = []
    for c in order:
        v = r.get(c, '—')
        if c != base and v.isdigit() and b and b.isdigit(): v = f'{int(v):,} ({100 * (int(v) - int(b)) / int(b):+.1f}%)'
        elif v.isdigit(): v = f'{int(v):,}'
        cells.append(v)
    print(f'| {f} | ' + ' | '.join(cells) + ' |')
