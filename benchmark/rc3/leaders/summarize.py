"""rc3 against the leaders, per group and level, from the rows of run_cli2.py (and of run_spec2.py / run_img8.py).

usage: summarize.py ROWS.jsonl ...
For each group and thread count: every Kolmo level against the leader of its tier, on the files both measured. Size =
total bytes, Kolmo vs leader (negative = Kolmo smaller); speeds = Kolmo MB/s / leader MB/s (above 1 = Kolmo faster).
A second table gives, per group, the smallest result of any tool and where each Kolmo level stands against it.
"""
import collections
import json
import re
import sys

TIERS = [("speed", ["lz4 -1"]), ("balanced", ["zstd -3"]), ("eco", ["brotli -11"]), ("hc", ["zstd -19", "zstd -19 --long=27"]),
         ("ratio", ["xz -9", "7-Zip LZMA2 -mx9", "bsc -m0 -e2", "bzip3 -b 16"]), ("-x", ["zpaq -m5"])]

rows = {}
for p in sys.argv[1:]:
    for l in open(p):
        r = json.loads(l)
        if r.get("type") != "row" or "error" in r or not r.get("roundtrip", True):
            continue
        T = str(r.get("threads_requested") or (re.search(r"-T(\d+)", p) or re.search(r"[TSI](\d+)", p.split("/")[-1])).group(1))
        lab = re.sub(r" -T \d+$", "", r["label"]).replace(" (one thread: this build has no threads)", "")
        rows[(r["group"], T, r["file"].split("/")[-1], lab)] = r
by = collections.defaultdict(lambda: collections.defaultdict(dict))
for (g, T, f, l), r in rows.items():
    by[(g, T)][l][f] = r


def tot(lab, d, fs):
    n = sum(d[lab][f]["input"] for f in fs)
    return sum(d[lab][f]["compressed"] for f in fs), n / sum(d[lab][f]["c_s"] for f in fs), n / sum(d[lab][f]["d_s"] for f in fs)


for (g, T) in sorted(by, key=lambda x: (x[0], int(x[1]))):
    d = by[(g, T)]
    print(f"\n### {g}, {T} thread{'s' if T != '1' else ''}\n\n| Kolmo | leader of the tier | files | size | compress | decompress |\n|---|---|---|---:|---:|---:|")
    for k, rivals in TIERS:
        kl = f"kolmo {k}"
        if kl not in d: continue
        best = None
        for rv in rivals:
            if rv not in d: continue
            fs = sorted(set(d[kl]) & set(d[rv]))
            if not fs: continue
            z = tot(rv, d, fs)
            if best is None or z[0] < best[1][0]: best = (rv, z, fs)
        if not best: continue
        rv, zr, fs = best
        zk = tot(kl, d, fs)
        print(f"| {k} | {rv} | {len(fs)} | {100 * (zk[0] / zr[0] - 1):+.1f}% | {zk[1] / zr[1]:.2f}x | {zk[2] / zr[2]:.2f}x |")
    labs = list(d)
    fs = sorted(set.intersection(*[set(d[l]) for l in labs])) if labs else []
    if fs:
        sizes = sorted((tot(l, d, fs)[0], l) for l in labs)
        print(f"\nSmallest of all on the {len(fs)} common files: **{sizes[0][1]}** ({sizes[0][0]:,} bytes); next: "
              + ", ".join(f"{l} {100 * (z / sizes[0][0] - 1):+.1f}%" for z, l in sizes[1:5]))
