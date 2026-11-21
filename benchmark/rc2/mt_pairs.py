"""Multithreaded tiers: each Kolmo setting or research codec against its rival, per group (and per file subset).

usage: mt_pairs.py ROWS.jsonl [ROWS.jsonl ...] [--files REGEX]
Totals over the files both settings measured: size difference, and compress / decompress speed ratios
(Kolmo's MB/s divided by the rival's; above 1 = Kolmo faster). Speeds are total bytes over total time.
"""
import collections
import json
import re
import sys

args = sys.argv[1:]
rx = None
if "--files" in args:
    i = args.index("--files"); rx = re.compile(args[i + 1]); args = args[:i] + args[i + 2:]
rows = {}
for p in args:
    for l in open(p):
        r = json.loads(l)
        if r.get("type") == "row" and "error" not in r and r.get("roundtrip", True) and (not rx or rx.search(r["file"])):
            rows[(r["group"], r["file"], r["label"])] = r
by = collections.defaultdict(lambda: collections.defaultdict(dict))
for (g, f, l), r in rows.items():
    by[g][l][f] = r
PAIRS = [("kolmo -m speed", "lz4 -1"), ("kolmo -m fast -q", "zstd -1"), ("kolmo -m balanced", "zstd -3"),
         ("kolmo -m hc", "zstd -19 -T0"), ("kolmo -m hc", "zstd -19 --long"), ("kolmo -m ratio", "xz -9"),
         ("kolmo -m ratio", "7-Zip"), ("kolmo -m ratio", "bsc"), ("kolmo -m ratio", "bzip3"), ("kolmo -x -T", "zpaq"),
         ("kfam", "kolmo -x -T"), ("kfam", "kolmo -m ratio"), ("kvid -m balanced", "kolmo -m balanced"), ("kvid -m x", "kolmo -x -T"),
         ("kvid -m ratio", "xz -9"), ("kbin -C", "kolmo -m ratio"), ("kbin -C", "kolmo -m balanced"), ("kbin -T", "zstd -19 -T0"),
         ("kolmo -x -l 2", "zpaq"), ("krans", "zstd -3"), ("kolmo -m eco", "brotli")]


def find(labels, key):
    c = [l for l in labels if l.startswith(key)]
    return c[0] if c else None


for g, labs in sorted(by.items()):
    print(f"\n## {g}" + (f" (files matching {rx.pattern})" if rx else ""))
    print("| Kolmo / research | Rival | Files | Size | Compress | Decompress |")
    print("|---|---|---|---|---|---|")
    for a, b in PAIRS:
        la, lb = find(labs, a), find(labs, b)
        if not la or not lb or la == lb:
            continue
        fs = sorted(set(labs[la]) & set(labs[lb]))
        if not fs:
            continue
        A = [labs[la][f] for f in fs]; B = [labs[lb][f] for f in fs]
        n = sum(r["input"] for r in A)
        za, zb = sum(r["compressed"] for r in A), sum(r["compressed"] for r in B)
        ca, cb = n / sum(r["c_s"] for r in A), n / sum(r["c_s"] for r in B)
        da, db = n / sum(r["d_s"] for r in A), n / sum(r["d_s"] for r in B)
        print(f"| {la} | {lb} | {len(fs)} | {100 * (za / zb - 1):+.1f}% | {ca / cb:.2f}x | {da / db:.2f}x |")
