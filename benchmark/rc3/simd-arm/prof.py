"""Flat profiles of Kolmo with macOS `sample`: one thread, every (input, mode, direction).

usage: python3 -I benchmark/rc3/simd-arm/prof.py PROPLOOP OUT_DIR SECONDS name=path [name=path ...]
Starts PROPLOOP (benchmark/rc3/simd-arm/proploop.c) running the case in a loop, attaches `sample` for SECONDS at 1 ms,
and keeps the "top of stack" self-time table. Writes OUT_DIR/<name>.<mode>.<c|d>.txt (raw) and prints the top
functions of each case. Inlined functions are charged to the function they were inlined into.
"""
import os
import re
import subprocess
import sys
import time

loop, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
files = [a.split("=", 1) for a in sys.argv[4:] if "=" in a]
modes = os.environ.get("MODES", "speed balanced eco hc ratio x").split()
os.makedirs(out, exist_ok=True)


def top(path, k=12):
    txt = open(path, errors="replace").read()
    m = txt.split("Sort by top of stack")
    if len(m) < 2:
        return []
    rows = []
    for line in m[1].splitlines()[1:]:
        mm = re.match(r"\s+(.+?)\s+\(in ([^)]+)\)\s+(\d+)\s*$", line)
        if mm:
            rows.append((mm.group(1), mm.group(2), int(mm.group(3))))
    tot = sum(r[2] for r in rows) or 1
    return [(n, lib, c, 100.0 * c / tot) for n, lib, c in rows[:k]], tot


for name, path in files:
    for mode in modes:
        for d in "cd":
            dst = os.path.join(out, f"{name}.{mode}.{d}.txt")
            p = subprocess.Popen([loop, mode, d, str(secs + 3), path], stdout=subprocess.PIPE)
            # the loop compresses once before timing; wait for that, roughly, by letting sample start a little later
            time.sleep(1.0 if mode not in ("x", "x1", "ratio") else 2.5)
            subprocess.run(["sample", str(p.pid), str(int(secs)), "1", "-mayDie", "-file", dst],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            o = p.communicate()[0].decode().strip()
            res = top(dst)
            if not res:
                print(f"== {name} {mode} {d}: no samples ({o})")
                continue
            rows, tot = res
            print(f"== {name} {mode} {d}  [{o}]  samples {tot}")
            for n, lib, c, pc in rows:
                print(f"   {pc:5.1f}%  {n[:70]}  ({lib})")
            sys.stdout.flush()
