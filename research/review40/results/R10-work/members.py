#!/usr/bin/env python3
"""R10: per-block outcome of the full LZMA2 and block-sorting codings at -m ratio, from a KOLMO_XPSTAT log."""
import re, sys
for path in sys.argv[1:]:
    calls = []; rows = []
    for line in open(path, errors='replace'):
        m = re.match(r'xpcall codec=(\d+) level=(-?\d+) n=(\d+) z=(\d+) ms=([\d.]+)', line)
        if m: calls.append(tuple(float(x) for x in m.groups())); continue
        if line.startswith('mp n='): calls = []; continue
        m = re.match(r'xpblock i=(\d+) len=(\d+) inlen=(\d+) plan=(\d+) tflag=(\d+) codec=(\d+) cl=(\d+)', line)
        if m:
            i, ln, inlen, plan, tflag, codec, cl = [int(x) for x in m.groups()]
            full = {}
            for c in calls:
                if int(c[2]) in (ln, inlen) and c[1] == 9: full.setdefault(int(c[0]), []).append((int(c[3]), c[4]))
            rows.append((i, ln, plan, tflag, codec, cl, full)); calls = []
    print('==', path, len(rows), 'blocks')
    for i, ln, plan, tflag, codec, cl, full in rows:
        lz = full.get(6, [(0, 0)])[0]; bw = full.get(18, [(0, 0)])
        r = '%.3f' % (bw[0][0] / lz[0]) if lz[0] and bw[0][0] else '-'
        print(f"  blk {i:2d} len={ln:9d} plan={plan} tflag={tflag} kept={'bwt' if codec == 18 else 'lzma2' if codec == 6 else codec} "
              f"lzma2={lz[0]} ({lz[1]:.0f} ms) bwt={[(b[0], round(b[1])) for b in bw]} bwt/lzma2={r}")
