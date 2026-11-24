#!/usr/bin/env python3
"""R10: cost of discarded coder calls, from the KOLMO_XPSTAT research build's stderr log (-T 1).
A call is 'kept' when it is the last call before its xpblock line whose codec and output size are the block's.
Graph blocks (codec 12) keep their leaf calls: every call of the block made with n < block length is counted kept
unless a later whole-block call replaced the graph (then the graph's calls are the discarded ones)."""
import re, sys
CN = {0:'store',3:'zstd',4:'brotli',6:'lzma2',7:'cm',12:'graph',13:'huf',14:'rans',16:'fq',17:'cmi',18:'bwt',19:'aud',21:'planes',22:'hz',23:'zrun',25:'cmp',26:'gr6'}
def parse(path):
    calls = []; tot = dict(plan_ms=0.0, blocks_ms=0.0, kept_ms=0.0, disc_ms=0.0, nblocks=0, plan_calls_ms=0.0)
    winners = {}; disc_by = {}
    inblocks = False
    for line in open(path, errors='replace'):
        m = re.match(r'xpcall codec=(\d+) level=(-?\d+) n=(\d+) z=(\d+) ms=([\d.]+)', line)
        if m:
            calls.append((int(m[1]), int(m[2]), int(m[3]), int(m[4]), float(m[5]))); continue
        m = re.match(r'mp n=(\d+) ms=([\d.]+)', line)
        if m:   # planner of one member finished: its calls were planning trials
            tot['plan_calls_ms'] += sum(c[4] for c in calls); calls = []; continue
        m = re.match(r'xpblock i=(\d+) len=(\d+) inlen=(\d+) plan=(\d+) tflag=(\d+) codec=(\d+) cl=(\d+) meta=(\d+)', line)
        if m:
            ln, inlen, codec, cl = int(m[2]), int(m[3]), int(m[6]), int(m[7])
            tot['nblocks'] += 1; winners[CN.get(codec, codec)] = winners.get(CN.get(codec, codec), 0) + ln
            kept = None
            if codec == 12:   # graph: leaves are kept, whole-block alternatives are discarded
                for c in calls:
                    if c[2] < min(ln, inlen) and c[2] != 262144 and c[2] != 524288 and c[2] != 1048576: tot['kept_ms'] += c[4]
                    else: tot['disc_ms'] += c[4]; disc_by[CN.get(c[0], c[0])] = disc_by.get(CN.get(c[0], c[0]), 0) + c[4]
            else:
                for k in range(len(calls) - 1, -1, -1):
                    c = calls[k]
                    if c[0] == codec and c[3] == cl and c[2] in (ln, inlen): kept = k; break
                for k, c in enumerate(calls):
                    if k == kept: tot['kept_ms'] += c[4]
                    else: tot['disc_ms'] += c[4]; disc_by[CN.get(c[0], c[0])] = disc_by.get(CN.get(c[0], c[0]), 0) + c[4]
            calls = []; continue
        m = re.match(r'compress bytes=(\d+) threads=(\d+) plan_ms=([\d.]+) blocks_ms=([\d.]+)', line)
        if m: tot['plan_ms'] += float(m[3]); tot['blocks_ms'] += float(m[4])
    return tot, winners, disc_by
if __name__ == '__main__':
    print('| file | total ms | plan ms | kept coder ms | discarded coder ms | discarded share | discarded by coder | winners (bytes) |')
    print('|---|---:|---:|---:|---:|---:|---|---|')
    T = dict(plan_ms=0.0, blocks_ms=0.0, kept_ms=0.0, disc_ms=0.0)
    for p in sys.argv[1:]:
        t, w, d = parse(p); total = t['plan_ms'] + t['blocks_ms']
        for k in T: T[k] += t[k]
        name = re.sub(r'^tax_[^_]+_|\.log$', '', p.split('/')[-1])
        print(f"| {name} | {total:.0f} | {t['plan_ms']:.0f} | {t['kept_ms']:.0f} | {t['disc_ms']:.0f} | {100*t['disc_ms']/total if total else 0:.1f}% | "
              + ', '.join(f'{k} {v:.0f}' for k, v in sorted(d.items(), key=lambda x: -x[1])) + ' | ' + ', '.join(f'{k} {v}' for k, v in sorted(w.items(), key=lambda x: -x[1])) + ' |')
    total = T['plan_ms'] + T['blocks_ms']
    if total: print(f"| **all** | {total:.0f} | {T['plan_ms']:.0f} | {T['kept_ms']:.0f} | {T['disc_ms']:.0f} | {100*T['disc_ms']/total:.1f}% | plan share {100*T['plan_ms']/total:.1f}% | |")
