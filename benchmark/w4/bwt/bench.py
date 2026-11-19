#!/usr/bin/env python3
"""Whole-process benchmark of the w4 bwt lane. Usage:
   bench.py OUT.jsonl THREADS TOOLSPEC... -- FILE...
TOOLSPEC: kolmo:NAME:PATH (runs `-m ratio -T THREADS`), bsc:NAME:PATH (benchmark/w4/bwt/bsclike.c, -b25 -e2,
OMP_NUM_THREADS=THREADS, OMP_WAIT_POLICY=passive), xz:NAME (xz -9 -T THREADS).
Per file and tool: compressed bytes, compress and decompress wall seconds and peak RSS (os.wait4 of the process),
best of ROUNDS (env, default 1) runs; the decoded output is compared with the input. One thread count per run."""
import json, os, sys, time, filecmp
ROUNDS = int(os.environ.get("ROUNDS", "1"))
W = os.environ.get("BENCH_WORK", "/home/user/w/bench")
os.makedirs(W, exist_ok=True)

def run(cmd, env=None, stdout=None):
    t = time.perf_counter(); pid = os.fork()
    if pid == 0:
        fd = os.open(os.devnull, os.O_WRONLY); os.dup2(fd, 2)
        if stdout: o = os.open(stdout, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644); os.dup2(o, 1)
        else: os.dup2(fd, 1)
        os.execvpe(cmd[0], cmd, env or os.environ)
    _, st, ru = os.wait4(pid, 0)
    return time.perf_counter() - t, ru.ru_maxrss / 1024.0, os.waitstatus_to_exitcode(st)

def main():
    out, T = sys.argv[1], int(sys.argv[2]); i = sys.argv.index("--")
    tools = [s.split(":") for s in sys.argv[3:i]]; files = sys.argv[i + 1:]
    with open(out, "a") as fo:
        for f in files:
            for spec in tools:
                kind, name = spec[0], spec[1]
                c, d = os.path.join(W, name + ".c"), os.path.join(W, name + ".d")
                env = dict(os.environ)
                if kind == "kolmo":
                    cc = [spec[2], "-m", "ratio", "-T", str(T), f, c]; dc = [spec[2], "-d", "-T", str(T), c, d]; so = None
                elif kind == "bsc":
                    env.update(OMP_NUM_THREADS=str(T), OMP_WAIT_POLICY="passive")
                    cc = [spec[2], "e", f, c, "-b25", "-e2", "-t%d" % T]; dc = [spec[2], "d", c, d, "-t%d" % T]; so = None
                elif kind == "xz":
                    cc = ["xz", "-9", "-k", "-c", "-T%d" % T, f]; dc = ["xz", "-d", "-c", "-T%d" % T, c]
                best = None
                for r in range(ROUNDS):
                    for p in (c, d):
                        if os.path.exists(p): os.unlink(p)
                    tc, mc, ec = run(cc, env, c if kind == "xz" else None)
                    td, md, ed = run(dc, env, d if kind == "xz" else None)
                    ok = ec == 0 and ed == 0 and filecmp.cmp(f, d, shallow=False)
                    rec = dict(file=os.path.basename(f), tool=name, threads=T, raw=os.path.getsize(f),
                               bytes=os.path.getsize(c) if os.path.exists(c) else -1, c_s=round(tc, 3), d_s=round(td, 3),
                               c_rss_mib=round(mc, 1), d_rss_mib=round(md, 1), ok=ok)
                    if best is None: best = rec
                    else:
                        best["c_s"] = min(best["c_s"], rec["c_s"]); best["d_s"] = min(best["d_s"], rec["d_s"]); best["ok"] &= ok
                fo.write(json.dumps(best) + "\n"); fo.flush()
                print(json.dumps(best), flush=True)
                for p in (c, d):
                    if os.path.exists(p): os.unlink(p)
main()
