"""Sizes of two kolmo builds on a folder, with a round trip through each listed decoder.

usage: ab_sizes.py OLD_KOLMO NEW_KOLMO DATA_DIR WORK_DIR OUT_JSON [--decoders D1,D2,...] -- KOLMO_ARGS...
Prints totals and the files whose size changed; every NEW frame must decode to the input with every decoder.
"""
import filecmp
import json
import os
import subprocess
import sys

old, new, data, work, out = sys.argv[1:6]
rest = sys.argv[6:]
decoders = [new]
if rest and rest[0] == "--decoders":
    decoders = rest[1].split(",")
    rest = rest[2:]
assert rest and rest[0] == "--"
args = rest[1:]
os.makedirs(work, exist_ok=True)
rows, bad = [], []
for name in sorted(os.listdir(data)):
    src = os.path.join(data, name)
    if not os.path.isfile(src):
        continue
    sizes = []
    for k, exe in (("o", old), ("n", new)):
        dst = os.path.join(work, k + ".k")
        subprocess.run([exe, *args, src, dst], check=True, capture_output=True)
        sizes.append(os.path.getsize(dst))
    for d in decoders:
        back = os.path.join(work, "back")
        r = subprocess.run([d, "-d", os.path.join(work, "n.k"), back], capture_output=True)
        if r.returncode or not filecmp.cmp(src, back, shallow=False):
            bad.append((name, d))
    rows.append({"file": name, "raw": os.path.getsize(src), "old": sizes[0], "new": sizes[1]})
raw = sum(r["raw"] for r in rows)
o = sum(r["old"] for r in rows)
n = sum(r["new"] for r in rows)
changed = [r for r in rows if r["old"] != r["new"]]
print(f"files {len(rows)}  raw {raw:,}  old {o:,} (ratio {raw / o:.3f})  new {n:,} (ratio {raw / n:.3f})  change {100 * (n - o) / o:+.2f}%")
print(f"changed {len(changed)}  larger {sum(r['new'] > r['old'] for r in rows)}  round-trip failures {len(bad)}")
json.dump({"args": args, "rows": rows, "failures": bad}, open(out, "w"), indent=1)
