#!/usr/bin/env python3
"""R06 schedule simulator. Input: R06BLK / R06DEC lines (per-block thread-CPU ms measured at -T 1 by the instrumented
rc3 build). Output: the makespan of the block phase on T threads for
  input   : rc3's rule (blocks taken in input order from a shared counter, one thread per block)
  longest : longest-first dispatch (largest measured cost first)
  bound   : max(longest block, total / T)  -- the best any block-level schedule can do with serial blocks
  split   : bound if blocks of the codec ids in --split could use any number of threads at efficiency --eff
            (models intra-block parallelism: an upper limit of what chunked LZMA2 / elastic BWT can add)
All figures are CPU-seconds from the thread CPU clock, so they do not depend on the machine's load."""
import sys, re, heapq, argparse
ap = argparse.ArgumentParser(); ap.add_argument("log"); ap.add_argument("--T", type=int, nargs="+", default=[2, 4, 8, 10])
ap.add_argument("--split", type=int, nargs="*", default=[]); ap.add_argument("--eff", type=float, default=1.0); a = ap.parse_args()
blk = []
for l in open(a.log, errors="replace"):
    m = re.match(r"R06(BLK|DEC) i=(\d+) (?:len|raw)=(\d+).*?codec=(\d+).*?cpu_ms=([\d.]+)", l)
    if m: blk.append((int(m.group(2)), int(m.group(3)), int(m.group(4)), float(m.group(5)) / 1000))
blk.sort()
def run(order, T):
    free = [0.0] * T; heapq.heapify(free); end = 0.0
    for c in order:
        t = heapq.heappop(free) + c; end = max(end, t); heapq.heappush(free, t)
    return end
cost = [b[3] for b in blk]; tot = sum(cost); big = max(cost); bi = cost.index(big)
by = {}
for b in blk: by[b[2]] = by.get(b[2], 0) + b[3]
print(f"blocks={len(blk)} total_cpu_s={tot:.2f} longest={big:.2f}s (block {blk[bi][0]}, {blk[bi][1]} bytes, codec {blk[bi][2]}) cpu by codec: " + ", ".join(f"{k}:{v:.2f}s" for k, v in sorted(by.items())))
print("T  input  longest  bound  speedup_input  speedup_bound" + ("  split_bound  speedup_split" if a.split else ""))
for T in a.T:
    i = run(cost, T); lf = run(sorted(cost, reverse=True), T); bd = max(big, tot / T)
    s = f"{T:<2d} {i:6.2f} {lf:7.2f} {bd:6.2f} {tot/i:8.2f}x {tot/bd:12.2f}x"
    if a.split:
        ser = [c for b, c in zip(blk, cost) if b[2] not in a.split]; par = sum(c for b, c in zip(blk, cost) if b[2] in a.split) / a.eff
        sb = max(max(ser) if ser else 0, (sum(ser) + par) / T); s += f" {sb:10.2f} {tot/sb:12.2f}x"
    print(s)
