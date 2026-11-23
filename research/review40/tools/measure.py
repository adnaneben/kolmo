#!/usr/bin/env python3
"""One measured command at a time (global lock), with wall time, peak RSS and time-average RSS of the process tree root.
  measure.py [--runs 3] [--label L] [--out results.jsonl] -- cmd args...
Speeds from this shared 4-core container are INDICATIVE ONLY (other agents compile concurrently); sizes are exact."""
import argparse, fcntl, json, os, subprocess, sys, time
D = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
p = argparse.ArgumentParser(); p.add_argument("--runs", type=int, default=3); p.add_argument("--label", default=""); p.add_argument("--out", default="")
p.add_argument("cmd", nargs=argparse.REMAINDER); a = p.parse_args()
cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
def rss_kb(pid):
    try:
        for l in open(f"/proc/{pid}/status"):
            if l.startswith("VmRSS:"): return int(l.split()[1])
    except OSError: pass
    return 0
env = dict(os.environ, OMP_NUM_THREADS="1")
with open(os.path.join(D, ".bench.lock"), "w") as lk:
    fcntl.flock(lk, fcntl.LOCK_EX)
    rows = []
    for i in range(a.runs):
        t0 = time.perf_counter(); pr = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL); s = []; pk = 0
        while pr.poll() is None:
            r = rss_kb(pr.pid); s.append(r); pk = max(pk, r); time.sleep(0.005)
        rows.append(dict(run=i, wall_s=round(time.perf_counter()-t0, 4), peak_rss_kb=pk, avg_rss_kb=int(sum(s)/len(s)) if s else 0, exit=pr.returncode))
    rec = dict(label=a.label, cmd=cmd, load1=os.getloadavg()[0], runs=rows, best_wall_s=min(r["wall_s"] for r in rows), t=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
    print(json.dumps(rec))
    if a.out: open(a.out, "a").write(json.dumps(rec) + "\n")
