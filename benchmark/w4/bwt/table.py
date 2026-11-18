#!/usr/bin/env python3
"""Tables of the w4 bwt sweep: best time of the rounds per (file, tool, threads), largest peak RSS. Usage: table.py DIR"""
import json, sys, glob, os
rows = {}
for p in sorted(glob.glob(os.path.join(sys.argv[1], "t*.jsonl"))):
    for l in open(p):
        r = json.loads(l); k = (r["threads"], r["file"], r["tool"])
        if k not in rows: rows[k] = dict(r)
        else:
            q = rows[k]; q["c_s"] = min(q["c_s"], r["c_s"]); q["d_s"] = min(q["d_s"], r["d_s"])
            q["c_rss_mib"] = max(q["c_rss_mib"], r["c_rss_mib"]); q["d_rss_mib"] = max(q["d_rss_mib"], r["d_rss_mib"]); q["ok"] &= r["ok"]
            assert q["bytes"] == r["bytes"], k
files = []
for (t, f, tool) in rows:
    if f not in files: files.append(f)
for T in sorted({k[0] for k in rows}):
    print(f"\n### {T} thread{'s' if T > 1 else ''}\n")
    print("| file | MB | tool | bytes | vs bsc | compress MB/s | decompress MB/s | compress RSS MiB | decompress RSS MiB | ok |")
    print("|---|---:|---|---:|---:|---:|---:|---:|---:|---|")
    tot = {}
    for f in files:
        b = rows.get((T, f, "bsc"))
        for tool in ("base", "new", "bsc"):
            r = rows.get((T, f, tool))
            if not r: continue
            mb = r["raw"] / 1e6
            vs = "%+.2f%%" % (100.0 * (r["bytes"] / b["bytes"] - 1)) if b else ""
            print(f"| {f} | {mb:.1f} | {tool} | {r['bytes']:,} | {vs} | {mb / r['c_s']:.1f} | {mb / r['d_s']:.1f} | {r['c_rss_mib']:.0f} | {r['d_rss_mib']:.0f} | {'ok' if r['ok'] else 'BAD'} |")
            a = tot.setdefault(tool, [0, 0, 0.0, 0.0, 0.0, 0.0]); a[0] += r["raw"]; a[1] += r["bytes"]; a[2] += r["c_s"]; a[3] += r["d_s"]
            a[4] = max(a[4], r["c_rss_mib"]); a[5] = max(a[5], r["d_rss_mib"])
    print(f"\nTotals, {T} thread(s) (all files; speeds are total MB / total seconds, RSS the largest):\n")
    print("| tool | MB | bytes | vs bsc | compress MB/s | decompress MB/s | max compress RSS | max decompress RSS |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")
    for tool, a in tot.items():
        vs = "%+.2f%%" % (100.0 * (a[1] / tot["bsc"][1] - 1)) if "bsc" in tot else ""
        print(f"| {tool} | {a[0] / 1e6:.0f} | {a[1]:,} | {vs} | {a[0] / 1e6 / a[2]:.1f} | {a[0] / 1e6 / a[3]:.1f} | {a[4]:.0f} | {a[5]:.0f} |")
