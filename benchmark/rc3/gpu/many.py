#!/usr/bin/env python3
"""Many files, one process per core: all CPU cores busy, the GPU otherwise idle. Interleaved A (CPU only) and B
(--gpu: each process may hand its block sorts to the one GPU).

usage: many.py KOLMO ROUNDS FILE... -- ARGS... [-- A_ARGS...]
Starts one `KOLMO ARGS [--gpu] FILE out` per file at once and reports the wall time until the last one ends
(median over ROUNDS), whether A's and B's outputs are the same bytes, and the GPU sorts B's processes made. With
A_ARGS, side A runs those arguments instead (for example -T 2: the GPU worker is one more thread, so B is compared
with A given one more CPU thread)."""
import os, re, subprocess, sys, tempfile, time

kolmo, rounds = sys.argv[1], int(sys.argv[2])
seps = [i for i, a in enumerate(sys.argv) if a == "--"]
files = sys.argv[3:seps[0]]
args = sys.argv[seps[0] + 1:seps[1]] if len(seps) > 1 else sys.argv[seps[0] + 1:]
aargs = sys.argv[seps[1] + 1:] if len(seps) > 1 else args
tmp = tempfile.mkdtemp(prefix="kolmo-many-")
walls = {"A": [], "B": []}
outs, sorts, per_file = {}, [], {}
for r in range(rounds):
    for side in ("A", "B") if r % 2 == 0 else ("B", "A"):
        env = dict(os.environ); env.pop("KOLMO_GPU", None)
        if side == "B":
            env["KOLMO_GPU_STATS"] = "1"
        a = aargs if side == "A" else args + ["--gpu"]
        t0 = time.perf_counter()
        ps = [subprocess.Popen([kolmo] + a + [f, os.path.join(tmp, f"{side}{i}")], env=env, stderr=subprocess.PIPE)
              for i, f in enumerate(files)]
        errs = [p.communicate()[1].decode() for p in ps]
        if any(p.returncode for p in ps):
            sys.exit("a run failed")
        walls[side].append(time.perf_counter() - t0)
        outs[side] = [open(os.path.join(tmp, f"{side}{i}"), "rb").read() for i in range(len(files))]
        if side == "B":
            s = [int(m.group(1)) if (m := re.search(r"(\d+) GPU sorts", e)) else 0 for e in errs]
            sorts.append(sum(s))
            per_file = {os.path.basename(f): k for f, k in zip(files, s)}
same = outs["A"] == outs["B"]
med = {s: sorted(v)[len(v) // 2] for s, v in walls.items()}
print(f"{len(files)} processes, A: {' '.join(aargs)} | B: {' '.join(args)} --gpu: A {med['A']:.2f} s, B {med['B']:.2f} s, "
      f"x{med['A'] / med['B']:.3f}, same bytes {same}, GPU sorts per B round {sorted(sorts)} (last: {per_file})")
