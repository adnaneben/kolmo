"""Per-file component benchmark of the wave-4 LZ lane: each (codec, file, thread count) in its own natbench process,
so peak RSS (from wait4) is the coder's plus the harness buffers (the same for both coders of a pair).

usage: python3 benchmark/w4/lz/perfile.py build/natbench OUT.tsv BLOCK_KIB REPS THREADS PAIR [PAIR ...] -- FILE ...
PAIR is rival:native (e.g. lz4:klz1). Prints and appends rows: file, threads, codec, in, out, c MB/s, d MB/s, RSS KiB.
1 thread and N threads are separate invocations (benchmark/rc2/PROTOCOL.md)."""
import os, subprocess, sys
NB, OUT, BK, REPS, T = sys.argv[1:6]
i = sys.argv.index("--"); pairs = [p.split(":") for p in sys.argv[6:i]]; files = sys.argv[i + 1:]
def run(codec, f):
    p = subprocess.Popen(["taskset", "-c", "0-3", NB, codec, T, BK, REPS, f], stdout=subprocess.PIPE, text=True)
    out = p.stdout.read(); _, st, ru = os.wait4(p.pid, 0)
    if st: raise SystemExit(f"{codec} {f}: failed")
    w = out.split()
    return int(w[3]), int(w[5]), float(w[9]), float(w[12]), ru.ru_maxrss
with open(OUT, "a") as o:
    for f in files:
        for a, b in pairs:
            for c in (a, b):
                n, z, cs, ds, rss = run(c, f)
                row = f"{os.path.basename(f)}\tT{T}\t{c}\t{n}\t{z}\t{cs:.1f}\t{ds:.1f}\t{rss}"
                print(row); o.write(row + "\n"); o.flush()
