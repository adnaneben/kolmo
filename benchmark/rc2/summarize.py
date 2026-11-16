"""Markdown tables from the rc2 result rows (tiers C, S and G).

usage: summarize.py [--ref LABEL] [--boot N] ROWS.jsonl [ROWS.jsonl ...] > tables.md
Per group: the totals over the group's files for every setting (sizes summed; speed = total input bytes divided
by the total of the per-file fastest times), sorted by size, with the ratio input/compressed. Rows with an error
are listed apart; rows marked `threads` are flagged. When a later row repeats (group, file, label), it wins.
With --ref, each group also gets a table of every setting against that reference setting: the change in total
bytes with a 95% bootstrap interval over files, and the geometric mean of per-file ratios (they differ when file
sizes are skewed; the first answers "how much storage", the second "how much on a typical file").
"""
import collections
import json
import math
import random
import sys

args = sys.argv[1:]
REF, BOOT = None, 2000
while args and args[0].startswith("--"):
    if args[0] == "--ref":
        REF, args = args[1], args[2:]
    elif args[0] == "--boot":
        BOOT, args = int(args[1]), args[2:]
    else:
        sys.exit(f"unknown option {args[0]}")
rows = {}
for path in args:
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
    if REF:
        ref = {x["file"]: x for x in by.get(REF, [])}
        if not ref:
            continue
        print(f"\nAgainst `{REF}`, on the files both measured: change in total bytes with a 95% interval (bootstrap over "
              f"files, {BOOT} resamples), and the geometric mean of the per-file size ratios.\n")
        print("| Setting | Files | Total bytes vs reference | 95% interval | Geometric mean of per-file ratios |")
        print("|---|---|---|---|---|")
        rng = random.Random(20261116)
        for label, xs in sorted(by.items()):
            if label == REF:
                continue
            pairs = [(x["compressed"], ref[x["file"]]["compressed"]) for x in xs if x["file"] in ref]
            if not pairs:
                continue
            tot = lambda ps: sum(a for a, _ in ps) / sum(b for _, b in ps) - 1
            boots = sorted(tot([pairs[rng.randrange(len(pairs))] for _ in pairs]) for _ in range(BOOT))
            lo, hi = boots[int(0.025 * BOOT)], boots[int(0.975 * BOOT) - 1]
            gm = math.exp(sum(math.log(a / b) for a, b in pairs) / len(pairs)) - 1
            print(f"| {label} | {len(pairs)} | {100 * tot(pairs):+.2f}% | {100 * lo:+.2f}% to {100 * hi:+.2f}% | {100 * gm:+.2f}% |")
