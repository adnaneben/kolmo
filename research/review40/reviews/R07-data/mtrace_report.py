#!/usr/bin/env python3
"""Reconstruct the live set of large allocations at the peak from an mtrace log; resolve callers with addr2line -f."""
import sys, subprocess, collections, re
log, binary = sys.argv[1], sys.argv[2]
base = int(sys.argv[3], 16) if len(sys.argv) > 3 else None
live = {}; tot = 0; peak = 0; snap = None; area = 0.0; lastt = None; t0 = None
for l in open(log):
    p = l.split()
    k = p[0]; t = float(p[1])
    if t0 is None: t0 = t
    if lastt is not None: area += tot * (t - lastt)
    lastt = t
    if k in 'MCAf':
        if k == 'f': continue
        live[p[2]] = (int(p[3]), k, p[5:]); tot += int(p[3])
    elif k == 'R':
        old = live.pop(p[4], None)
        if old: tot -= old[0]
        live[p[2]] = (int(p[3]), k, p[5:]); tot += int(p[3])
    elif k == 'F':
        old = live.pop(p[2], None)
        if old: tot -= old[0]
    elif k == 'U':
        old = live.pop(p[2], None)
        if old: tot -= old[0]
    if tot > peak: peak = tot; snap = (t - t0, dict(live))
print(f"peak large-allocation total: {peak/2**20:.1f} MiB at t={snap[0]:.2f}s; time-average {area/max(lastt-t0,1e-9)/2**20:.1f} MiB over {lastt-t0:.2f}s")
maps_base = base
def sym(addrs):
    out = []
    for a in addrs[:6]:
        v = int(a, 16) - maps_base
        if v < 0 or v > 1 << 26: out.append('?'); continue
        r = subprocess.run(['addr2line', '-f', '-e', binary, hex(v)], capture_output=True, text=True).stdout.split('\n')[0]
        out.append(r)
    return ' < '.join(out)
agg = collections.defaultdict(lambda: [0, 0])
for ptr, (n, k, bt) in snap[1].items():
    key = (k, sym(bt)) if maps_base is not None else (k, ' '.join(bt[:4]))
    agg[key][0] += n; agg[key][1] += 1
for key, (n, c) in sorted(agg.items(), key=lambda x: -x[1][0])[:25]:
    print(f"{n/2**20:9.1f} MiB x{c:<3d} {key[0]} {key[1]}")
