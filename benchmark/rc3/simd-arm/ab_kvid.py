"""Interleaved A/B of kvid's NEON kernels (KVID_FORCE_C=1 is the C code): `kvid -b` per round, 15 rounds or more.

usage: python3 -I benchmark/rc3/simd-arm/ab_kvid.py KVID ROUNDS MODE clip.y4m
Prints the median compress and decompress MB/s of each side, the paired ratios (kernels / C) with their
interquartile range, and an A/A control (a second C run per round).
"""
import json
import os
import statistics
import subprocess
import sys

kvid, rounds, mode, path = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]


def run(force):
    r = subprocess.run([kvid, "-b", "-m", mode, path], capture_output=True, text=True, env=dict(os.environ, KVID_FORCE_C=str(force)))
    j = json.loads(r.stdout)
    return j["compress_mbs"], j["decompress_mbs"]


def q(xs, f):
    xs = sorted(xs); k = (len(xs) - 1) * f; lo = int(k)
    return xs[lo] + (xs[min(lo + 1, len(xs) - 1)] - xs[lo]) * (k - lo)


A, B, A2 = [], [], []
for r in range(rounds):
    order = [("A", 1), ("B", 0), ("A2", 1)] if r % 2 == 0 else [("B", 0), ("A2", 1), ("A", 1)]
    res = {t: run(f) for t, f in order}
    A.append(res["A"]); B.append(res["B"]); A2.append(res["A2"])
for i, name in enumerate(("compress", "decompress")):
    rat = [a[i] / b[i] for a, b in zip(A, B)]            # time ratio kernels / C = speed C / speed kernels
    aa = [a[i] / x[i] for a, x in zip(A, A2)]
    print(f"{os.path.basename(path)} {mode} {name}: C {statistics.median(a[i] for a in A):.0f} MB/s, kernels "
          f"{statistics.median(b[i] for b in B):.0f} MB/s, time B/A {statistics.median(rat):.3f} [{q(rat, .25):.3f}, {q(rat, .75):.3f}], "
          f"A/A {statistics.median(aa):.3f} [{q(aa, .25):.3f}, {q(aa, .75):.3f}]")
