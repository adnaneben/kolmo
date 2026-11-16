"""Interleaved A/B timing of Kolmo's CPU kernels against their C references (one binary, one thread).

usage: python3 -I benchmark/rc3/simd-arm/ab.py PROPLOOP WORK ROUNDS SECS case [case ...]  [--json out.jsonl]
case = MODE:c|d:PATH  (MODE as proploop: speed balanced eco hc ratio x x1)
Each round runs, in alternating order, A = C references (PROPLOOP_FORCE_C=1), B = kernels (PROPLOOP_FORCE_C=0) and a
second A run (A2) for an A/A control; each run is a fresh process looping the case for SECS and reporting its fastest
iteration. Per case: median of the per-round fastest times, the median paired ratio B/A with its interquartile range,
and the A2/A ratio (the noise floor of the same build). Decompression reuses one frame per case (WORK/<name>.k).
"""
import json
import os
import statistics
import subprocess
import sys

args = [a for a in sys.argv[1:] if not a.startswith("--")]
jout = sys.argv[sys.argv.index("--json") + 1] if "--json" in sys.argv else None
if jout:
    args = [a for a in args if a != jout]
loop, work, rounds, secs = args[0], args[1], int(args[2]), args[3]
cases = [c.split(":", 2) for c in args[4:]]
os.makedirs(work, exist_ok=True)


def run(mode, d, path, force, frame):
    env = dict(os.environ, PROPLOOP_FORCE_C=str(force), PROPLOOP_FRAME=frame)
    out = subprocess.run([loop, mode, d, secs, path], capture_output=True, text=True, env=env)
    if out.returncode:
        sys.exit(f"failed: {mode} {d} {path}: {out.stderr}")
    w = out.stdout.split()
    return float(w[w.index("best") + 1]), int(w[4])


def q(xs, f):
    xs = sorted(xs)
    k = (len(xs) - 1) * f
    lo = int(k)
    return xs[lo] + (xs[min(lo + 1, len(xs) - 1)] - xs[lo]) * (k - lo)


for mode, d, path in cases:
    frame = os.path.join(work, f"{os.path.basename(path)}.{mode}.k")
    run(mode, d, path, 1, frame)                                   # warm the file cache and write the frame
    A, B, A2 = [], [], []
    for r in range(rounds):
        order = [("A", 1), ("B", 0), ("A2", 1)] if r % 2 == 0 else [("B", 0), ("A2", 1), ("A", 1)]
        res = {}
        for tag, force in order:
            res[tag] = run(mode, d, path, force, frame)
        A.append(res["A"][0]); B.append(res["B"][0]); A2.append(res["A2"][0])
        size = res["B"][1]
    rat = [b / a for a, b in zip(A, B)]
    aa = [x / a for a, x in zip(A, A2)]
    n = os.path.getsize(path)
    rec = {"case": f"{os.path.basename(path)} {mode} {d}", "bytes": n, "frame": size, "rounds": rounds,
           "A_ms": statistics.median(A), "B_ms": statistics.median(B), "ratio": statistics.median(rat),
           "ratio_q1": q(rat, 0.25), "ratio_q3": q(rat, 0.75), "aa": statistics.median(aa), "aa_q1": q(aa, 0.25),
           "aa_q3": q(aa, 0.75)}
    print(f"{rec['case']:40s} C {rec['A_ms']:9.2f} ms  kernels {rec['B_ms']:9.2f} ms  B/A {rec['ratio']:.3f} "
          f"[{rec['ratio_q1']:.3f}, {rec['ratio_q3']:.3f}]  A/A {rec['aa']:.3f} [{rec['aa_q1']:.3f}, {rec['aa_q3']:.3f}]  "
          f"({n / rec['B_ms'] / 1e3:.0f} MB/s)", flush=True)
    if jout:
        with open(jout, "a") as f:
            f.write(json.dumps(rec) + "\n")
