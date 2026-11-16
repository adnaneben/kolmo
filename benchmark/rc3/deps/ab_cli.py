"""A/B of two kolmo binaries (deps lane, rc3): same files, same settings, alternating runs, best of N wall times.

usage: python3 benchmark/rc3/deps/ab_cli.py A B ROUNDS "MODE ARGS" FILE...   (e.g. "-m balanced")
Prints per binary: total input, output bytes, best total compress and decompress seconds, and checks that A and B write
the same bytes (they must when only speed changed)."""
import os, subprocess, sys, tempfile, time
A, B, R, ARGS = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4].split()
FILES = sys.argv[5:]
tmp = tempfile.mkdtemp()
best = {A: [1e9, 1e9], B: [1e9, 1e9]}; outs = {}
for r in range(R):
    for k in (A, B) if r % 2 == 0 else (B, A):
        tc = td = 0.0; tot = 0
        for f in FILES:
            z = os.path.join(tmp, "z"); o = os.path.join(tmp, "o")
            t = time.perf_counter(); subprocess.run([k, *ARGS, "-T", "1", f, z], check=True); tc += time.perf_counter() - t
            t = time.perf_counter(); subprocess.run([k, "-d", "-T", "1", z, o], check=True); td += time.perf_counter() - t
            tot += os.path.getsize(z)
            if r == 0: outs[(k, f)] = open(z, "rb").read()
        best[k][0] = min(best[k][0], tc); best[k][1] = min(best[k][1], td); best[k].append(tot) if len(best[k]) == 2 else None
insz = sum(os.path.getsize(f) for f in FILES)
same = all(outs[(A, f)] == outs[(B, f)] for f in FILES)
print(f"settings {' '.join(ARGS)}  files {len(FILES)}  input {insz}  same bytes: {same}")
for k in (A, B):
    print(f"  {k:28} out {best[k][2]:>11}  c {best[k][0]:7.3f}s {insz/best[k][0]/1e6:8.1f} MB/s  d {best[k][1]:7.3f}s {insz/best[k][1]/1e6:8.1f} MB/s")
