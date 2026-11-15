"""Markdown tables from the rc2 result rows (tiers C, S and G).

usage: summarize.py ROWS.jsonl [ROWS.jsonl ...] > tables.md
Per group: the totals over the group's files for every setting (sizes summed; speed = total input bytes divided
by the total of the per-file fastest times), sorted by size, with the ratio input/compressed. Rows with an error
are listed apart; rows marked `threads` are flagged. When a later row repeats (group, file, label), it wins.
"""
import collections
import json
import sys

rows = {}
for path in sys.argv[1:]:
    for line in open(path):
        r = json.loads(line)
        if r.get("type") == "row":
            rows[(r["group"], r["file"], r["label"])] = r

groups = collections.defaultdict(list)
for r in rows.values():
    groups[r["group"]].append(r)

for g, rs in groups.items():
    files = sorted({r["file"] for r in rs})
    ok = [r for r in rs if "error" not in r and r.get("roundtrip", True)]
    bad = [r for r in rs if r not in ok]
    by = collections.defaultdict(list)
    for r in ok:
        by[r["label"]].append(r)
    print(f"\n### {g} ({len(files)} files: {', '.join(files)})\n")
    print("| Setting | Files | Input bytes | Compressed bytes | Ratio | Compress MB/s | Decompress MB/s | Note |")
    print("|---|---|---|---|---|---|---|---|")
    lines = []
    for label, xs in by.items():
        n = sum(x["input"] for x in xs)
        z = sum(x["compressed"] for x in xs)
        cs = sum(x["c_s"] for x in xs)
        ds = sum(x["d_s"] for x in xs)
        note = "threads" if any(x.get("threads") for x in xs) else ""
        if len(xs) < len(files):
            note = (note + " " if note else "") + f"{len(xs)} of {len(files)} files"
        lines.append((len(xs) < len(files), z, f"| {label} | {len(xs)} | {n:,} | {z:,} | {n / z:.3f} | {n / cs / 1e6:.2f} | {n / ds / 1e6:.2f} | {note} |"))
    for _, _, l in sorted(lines):
        print(l)
    for r in bad:
        print(f"\nFailed: {r['file']} {r['label']}: {r.get('error', 'round trip differs')[:200]}")
