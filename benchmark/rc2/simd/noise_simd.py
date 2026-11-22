"""Noise floor and geometric means for run_simd.py output: python3 -I noise_simd.py RESULTS.jsonl BASE
Noise: per (input, mode, threads, build, direction) the gap between the fastest and the second-fastest of the rounds.
Geometric mean of time ratios to BASE, per direction and thread count, over all inputs and modes."""
import collections, json, math, statistics, sys
recs = [json.loads(l) for l in open(sys.argv[1])]; base = sys.argv[2]
t = collections.defaultdict(list)
for r in recs:
    for d in ("c", "d"): t[(r["input"], r["mode"], r["threads"], r["build"], d)].append(r[d]["wall"])
gaps = [sorted(v)[1] / sorted(v)[0] - 1 for v in t.values()]
print(f"fastest vs second fastest of 5: median gap {100*statistics.median(gaps):.1f}%, 90th percentile {100*sorted(gaps)[int(.9*len(gaps))]:.1f}%")
spread = [statistics.median(v) / min(v) - 1 for v in t.values()]
print(f"median of 5 vs fastest: median {100*statistics.median(spread):.1f}%")
builds = list(dict.fromkeys(r["build"] for r in recs))
print("\n| direction | T | " + " | ".join(f"{b}/{base}" for b in builds if b != base) + " |\n|---|---:|" + "---:|" * (len(builds) - 1))
for d, dn in (("c", "compress"), ("d", "decompress")):
    for th in sorted({k[2] for k in t}):
        row = []
        for b in builds:
            if b == base: continue
            l = [math.log(min(t[k]) / min(t[(k[0], k[1], k[2], base, d)])) for k in t if k[3] == b and k[4] == d and k[2] == th]
            row.append(f"{math.exp(sum(l)/len(l)):.3f}")
        print(f"| {dn} | {th} | " + " | ".join(row) + " |")
