"""N concurrent kolmo instances, with and without the machine-wide pool (docs/CONCURRENCY.md).

usage: python3 benchmark/w4/multi/run.py KOLMO WORK_DIR [--reps R] [--ns 1,2,4,8,16] [--modes balanced,ratio,x]
For each mode and N, starts N instances at once, instance i compressing file i of a generated set with the default
thread count (-T 0: every CPU), and measures the wall time until the last one ends, the throughput (input bytes of
all instances / wall time), the mean time an instance takes, and the machine's peak memory used by Kolmo (the sum of
the instances' resident memory, sampled every 5 ms from /proc). Pool on: KOLMO_JOBS_SHARED unset (default, tokens =
physical cores); off: KOLMO_JOBS_SHARED=off. The frames are compared between on and off (they must be identical).
Also measures the start-up cost of the pool on 300 sequential runs of a 64 KiB file. Writes results.json and prints
Markdown tables.
"""
import json
import os
import random
import statistics
import subprocess
import sys
import tempfile
import time

KOLMO = os.path.abspath(sys.argv[1])
W = os.path.abspath(sys.argv[2])
args = sys.argv[3:]
REPS = int(args[args.index("--reps") + 1]) if "--reps" in args else 3
NS = [int(x) for x in args[args.index("--ns") + 1].split(",")] if "--ns" in args else [1, 2, 4, 8, 16]
MODES = args[args.index("--modes") + 1].split(",") if "--modes" in args else ["balanced", "ratio", "x"]
os.makedirs(W, exist_ok=True)
POOL = tempfile.mkdtemp(prefix="kolmo-bench-pool-")
MODE_ARGS = {"balanced": ["-m", "balanced"], "ratio": ["-m", "ratio"], "x": ["-x"]}
SIZE = {"balanced": 24 << 20, "ratio": 8 << 20, "x": 512 << 10}


def gen(path, n, seed):
    """text and table-like data, different per seed (no third-party data)"""
    rnd = random.Random(seed)
    words = [("%s%d" % (rnd.choice(["data", "kol", "mo", "pool", "token"]), i)) for i in range(5000)]
    out = bytearray()
    while len(out) < n:
        if rnd.random() < 0.7:
            out += (" ".join(words[min(int(rnd.paretovariate(1.15)) - 1, 4999)] for _ in range(40)) + "\n").encode()
        else:
            out += ("%d,%d,%.4f,host%d\n" % (len(out), rnd.randint(0, 10 ** 6), rnd.random(), rnd.randint(0, 40))).encode()
    with open(path, "wb") as f:
        f.write(bytes(out[:n]))


def files_for(mode):
    fs = []
    for i in range(max(NS)):
        p = os.path.join(W, "in-%s-%d" % (mode, i))
        if not os.path.exists(p) or os.path.getsize(p) != SIZE[mode]:
            gen(p, SIZE[mode], 1000 + i)
        fs.append(p)
    return fs


def rss_kib(pid):
    try:
        with open("/proc/%d/status" % pid) as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except OSError:
        pass
    return 0


def run_batch(mode, n, shared, files, tag):
    env = dict(os.environ, KOLMO_JOBS_DIR=POOL)
    env.pop("KOLMO_JOBS", None)
    if shared:
        env.pop("KOLMO_JOBS_SHARED", None)
    else:
        env["KOLMO_JOBS_SHARED"] = "off"
    outs = [os.path.join(W, "out-%s-%s-%d-%d.k" % (mode, tag, n, i)) for i in range(n)]
    t0 = time.perf_counter()
    procs = [subprocess.Popen([KOLMO] + MODE_ARGS[mode] + [files[i], outs[i]], env=env) for i in range(n)]
    ends = [None] * n
    peak = 0
    while any(e is None for e in ends):
        tot = 0
        for i, p in enumerate(procs):
            if ends[i] is None:
                if p.poll() is not None:
                    ends[i] = time.perf_counter() - t0
                    if p.returncode:
                        sys.exit("kolmo failed: %s" % p.args)
                else:
                    tot += rss_kib(p.pid)
        peak = max(peak, tot)
        time.sleep(0.005)
    wall = time.perf_counter() - t0
    return dict(wall=wall, mean=statistics.mean(ends), peak_mib=peak / 1024.0,
                mbps=n * SIZE[mode] / 1e6 / wall, outs=outs)


def startup(shared, runs=300):
    p = os.path.join(W, "small")
    if not os.path.exists(p):
        gen(p, 64 << 10, 5)
    env = dict(os.environ, KOLMO_JOBS_DIR=POOL)
    if not shared:
        env["KOLMO_JOBS_SHARED"] = "off"
    else:
        env.pop("KOLMO_JOBS_SHARED", None)
    t0 = time.perf_counter()
    for _ in range(runs):
        subprocess.run([KOLMO, p, p + ".k"], env=env, check=True)
    return (time.perf_counter() - t0) / runs * 1000


def main():
    res = {"cpus": os.cpu_count(), "reps": REPS, "modes": {}}
    for mode in MODES:
        files = files_for(mode)
        res["modes"][mode] = {}
        for n in NS:
            row = {}
            for shared in (False, True):
                rs = []
                for r in range(REPS):
                    rs.append(run_batch(mode, n, shared, files, "on" if shared else "off"))
                med = sorted(rs, key=lambda x: x["wall"])[len(rs) // 2]
                row["on" if shared else "off"] = {k: med[k] for k in ("wall", "mean", "peak_mib", "mbps")}
                row["on" if shared else "off"]["walls"] = [x["wall"] for x in rs]
            same = all(open(os.path.join(W, "out-%s-on-%d-%d.k" % (mode, n, i)), "rb").read() ==
                       open(os.path.join(W, "out-%s-off-%d-%d.k" % (mode, n, i)), "rb").read() for i in range(n))
            row["same_frames"] = same
            if not same:
                sys.exit("FAIL: frames differ between pool on and off (%s, N=%d)" % (mode, n))
            res["modes"][mode][n] = row
            print(mode, n, json.dumps({k: (v if k == "same_frames" else {kk: round(vv, 3) if isinstance(vv, float) else vv for kk, vv in v.items()}) for k, v in row.items()}), flush=True)
            for f in os.listdir(W):
                if f.startswith("out-"):
                    os.unlink(os.path.join(W, f))
    s_off = [startup(False) for _ in range(3)]
    s_on = [startup(True) for _ in range(3)]
    res["startup_ms"] = {"off": statistics.median(s_off), "on": statistics.median(s_on)}
    with open(os.path.join(W, "results.json"), "w") as f:
        json.dump(res, f, indent=1)
    for mode in MODES:
        print("\n### %s (%d KiB per instance)\n" % (mode, SIZE[mode] >> 10))
        print("| N | wall off (s) | wall on (s) | wall on/off | MB/s off | MB/s on | mean instance off (s) | on (s) | peak MiB off | peak MiB on |")
        print("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for n in NS:
            r = res["modes"][mode][n]
            a, b = r["off"], r["on"]
            print("| %d | %.2f | %.2f | %.2f | %.1f | %.1f | %.2f | %.2f | %.0f | %.0f |" % (
                n, a["wall"], b["wall"], b["wall"] / a["wall"], a["mbps"], b["mbps"], a["mean"], b["mean"], a["peak_mib"], b["peak_mib"]))
    print("\nstart-up, 64 KiB file, mean of 300 runs: off %.2f ms, on %.2f ms" % (res["startup_ms"]["off"], res["startup_ms"]["on"]))


main()
