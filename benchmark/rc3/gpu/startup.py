#!/usr/bin/env python3
"""Process start-up cost of linking Metal: the same small compression with two builds, interleaved.

usage: startup.py KOLMO_A KOLMO_B FILE RUNS [args...]
Prints the median and best wall time of each build (os.wait4 around the whole process)."""
import os, subprocess, sys, tempfile, time

a, b, f, runs = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
args = sys.argv[5:]
out = os.path.join(tempfile.mkdtemp(prefix="kolmo-start-"), "o")
t = {a: [], b: []}
for r in range(runs):
    for k in ((a, b) if r % 2 == 0 else (b, a)):
        s = time.perf_counter()
        subprocess.run([k] + args + [f, out], check=True)
        t[k].append(time.perf_counter() - s)
for k in (a, b):
    v = sorted(t[k])
    print(f"{k}: median {v[len(v) // 2] * 1e3:.2f} ms, best {v[0] * 1e3:.2f} ms over {runs} runs")
