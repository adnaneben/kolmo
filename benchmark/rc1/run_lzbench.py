#!/usr/bin/env python3
"""The lzbench tiers of benchmark/rc1/PROTOCOL.md: kolmo and its rivals, in memory, on one thread.

usage: run_lzbench.py --lzbench <lzbench with kolmo> --out <dir> --tier A1|A2|A3
                      [--silesia-tar <file>] [--data <dir>] [--typed <dir>] [--raw <dir>]
                      [--only <regex matched against "file entry">] [--list]

One lzbench process is started per (file, codec setting). That keeps every measurement apart from the others and
lets this driver read the process's own CPU time and peak memory when it ends (wait4):
  * CPU time above elapsed time means more than one thread ran. Such a row is marked `threads`.
  * A benchmark of the nightly watcher (bench_mac3) seen before or after a row marks it `busy`.
  * Any other process that used at least half a core just before a row is written down in `others` (not marked).
Marked rows and failed rows are run again when the driver is started again; finished rows are skipped.
Results are appended to <out>/<tier>.jsonl, one JSON object per line.
"""
import argparse
import csv
import hashlib
import io
import json
import os
import platform
import re
import subprocess
import sys
import time

# Minimum number of (compression, decompression) iterations per class; lzbench reports the fastest iteration.
#   f  fast or medium: at least three of each
#   a  slow to compress, quick to decompress: one compression, at least three decompressions
#   s  slow both ways: one of each
ITERS = {"f": "3,3", "a": "1,3", "s": "1,1"}

CONTROL = ["lz4", "kolmo,16", "zstd,3", "kolmo,4"]   # measured first and again at the end of a tier

A1 = [  # silesia.tar; minimum time 1 s compression, 2 s decompression (lzbench's default)
    ("lz4", "f"), ("kolmo,16", "f"), ("zstd,3", "f"), ("kolmo,4", "f"), ("memcpy", "f"),
    # lowest latency
    ("lzav,1", "f"), ("zxc,1", "f"), ("zxc,3", "f"), ("snappy", "f"), ("lzfse", "f"), ("kolmo,0", "f"),
    # fast
    ("zstd,1", "f"), ("kolmo,17", "f"), ("kolmo,1", "f"), ("kolmo,3", "f"), ("kolmo_ctl,4", "f"),
    # DEFLATE, brotli, middle levels
    ("libdeflate,6", "f"), ("zlib-ng,6", "f"), ("brotli,5", "f"), ("kolmo,8", "f"), ("zstd,9", "f"), ("kolmo,5", "f"),
    # block sorting
    ("bsc0", "f"), ("bsc1", "f"), ("bzip3,5", "f"), ("bzip3,10", "f"), ("kolmo,7", "a"),
    # high compression, quick decompression; LZMA
    ("zstd,19", "a"), ("zstd,22", "a"), ("kolmo,9", "a"), ("xz,9", "a"), ("lzma,9", "a"), ("kolmo,10", "a"),
    ("brotli,11", "a"),
    # context mixing and PPM
    ("ppmd8,4", "s"), ("kanzi,7", "s"), ("kolmo,11", "s"), ("kolmo,12", "s"), ("kolmo,13", "s"), ("kanzi,9", "s"),
    ("zpaq,5", "s"),
]

A2_FIRST = [  # each of the 35 files; minimum time 1 s each way
    ("memcpy", "f"), ("lz4", "f"), ("kolmo,16", "f"), ("zstd,1", "f"), ("kolmo,17", "f"), ("zstd,3", "f"), ("kolmo,4", "f"),
    ("kolmo_ctl,4", "f"), ("kolmo,14", "f"), ("libdeflate,6", "f"), ("brotli,5", "f"), ("kolmo,8", "f"), ("zstd,9", "f"),
    ("kolmo,5", "f"), ("zstd,19", "a"), ("kolmo,9", "a"), ("xz,9", "a"), ("kolmo,10", "a"), ("bsc0", "f"), ("bzip3,5", "f"),
]
A2_SECOND = [("brotli,11", "a"), ("kolmo,12", "s")]   # the slowest two, after every file has its first pass
A2_END = ["dickens", "mozilla", "api.json"]            # files on which the control settings are measured again

A3 = [  # typed arrays; minimum time 1 s each way
    ("memcpy", "f"), ("zstd,3", "f"), ("kolmo,4", "f"), ("kolmo_ctl,4", "f"), ("kolmo,14", "f"), ("kolmo,15", "f"),
    ("kolmo_ctl,15", "f"), ("zstd,19", "a"), ("xz,9", "a"), ("bsc0", "f"), ("bzip3,5", "f"), ("kolmo,10", "a"),
    ("kolmo,12", "s"), ("kolmo,13", "s"),
]
A3_DATA = ["year.i32", "population.i64", "log_timestamps_ms.i64", "lfw_subset.npy", "owid_energy_f64.npy"]
OPENZL = {".i16": "openzl_le_i16", ".i32": "openzl_le_i32", ".i64": "openzl_le_i64"}   # raw little-endian arrays only


def plan(a):
    rows = []   # (round, path, entry, minimum times, class)
    if a.tier == "A1":
        rows += [("main", a.silesia_tar, e, "1,2", c) for e, c in A1]
        rows += [("end", a.silesia_tar, e, "1,2", "f") for e in CONTROL]
    elif a.tier == "A2":
        files = sorted((os.path.getsize(os.path.join(a.data, f)), f) for f in os.listdir(a.data) if not f.startswith("."))
        paths = [os.path.join(a.data, f) for _, f in files]
        for p in paths:
            rows += [("main", p, e, "1,1", c) for e, c in A2_FIRST]
        for p in paths:
            rows += [("main", p, e, "1,1", c) for e, c in A2_SECOND]
        for f in A2_END:
            rows += [("end", os.path.join(a.data, f), e, "1,1", "f") for e in CONTROL]
    elif a.tier == "A3":
        paths = [os.path.join(a.typed, f) for f in sorted(os.listdir(a.typed)) if f.endswith(".npy")]
        paths += [os.path.join(a.raw, f) for f in sorted(os.listdir(a.raw)) if os.path.splitext(f)[1] in OPENZL]
        paths += [os.path.join(a.data, f) for f in A3_DATA]
        for p in paths:
            rows += [("main", p, e, "1,1", c) for e, c in A3]
            if os.path.splitext(p)[1] in OPENZL:
                rows.append(("main", p, OPENZL[os.path.splitext(p)[1]], "1,1", "f"))
        rows += [("end", paths[0], e, "1,1", "f") for e in CONTROL]
    else:
        sys.exit("unknown tier " + a.tier)
    return rows


def watcher_running():
    return subprocess.run(["pgrep", "-f", "bench_mac3"], stdout=subprocess.DEVNULL).returncode == 0


def others():
    """Other processes that used at least half a core just before a row: [name, percent]. Recorded, not acted on."""
    p = subprocess.run(["ps", "-Ao", "pcpu=,comm="], stdout=subprocess.PIPE, text=True)
    out = []
    for line in p.stdout.splitlines():
        a = line.split(None, 1)
        try:
            pc = float(a[0].replace(",", "."))
        except (ValueError, IndexError):
            continue
        if pc >= 50 and len(a) == 2 and "lzbench" not in a[1]:
            out.append([os.path.basename(a[1].strip()), pc])
    return out


def run_one(lzbench, path, entry, tmin, iters, tmp):
    cmd = [lzbench, "-o4", "-t" + tmin, "-i" + iters, "-e" + entry, path]
    r = {"cmd": " ".join(cmd[1:-1]), "load1": round(os.getloadavg()[0], 2), "busy": watcher_running(), "others": others()}
    with open(tmp + ".out", "wb") as fo, open(tmp + ".err", "wb") as fe:
        t0 = time.perf_counter()
        p = subprocess.Popen(cmd, stdout=fo, stderr=fe)
        _, status, ru = os.wait4(p.pid, 0)
        r["elapsed_s"] = round(time.perf_counter() - t0, 3)
    p.returncode = os.waitstatus_to_exitcode(status)
    r["busy"] = r["busy"] or watcher_running()
    r["rc"] = p.returncode
    r["cpu_s"] = round(ru.ru_utime + ru.ru_stime, 3)
    r["peak_rss"] = ru.ru_maxrss if sys.platform == "darwin" else ru.ru_maxrss * 1024   # bytes
    out = open(tmp + ".out", errors="replace").read()
    err = open(tmp + ".err", errors="replace").read()
    data = [x for x in csv.reader(io.StringIO(out)) if len(x) >= 7 and x[0] != "Compressor name"]
    r["threads"] = r["cpu_s"] > r["elapsed_s"] * 1.03 + 0.05
    if p.returncode != 0 or "ERROR" in out or "ERROR" in err or len(data) != 1:
        r["error"] = (out + err)[-400:]
        return r
    x = data[0]
    r.update(name=x[0], c_mbps=float(x[1]), d_mbps=float(x[2]), n=int(x[3]), size=int(x[4]))
    if r["c_mbps"] <= 0 or r["d_mbps"] <= 0 or r["size"] <= 0:
        r["error"] = "lzbench reported a failed round trip: " + ",".join(x[:6])
    return r


def ok(r):
    return "error" not in r and not r.get("threads") and not r.get("busy")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lzbench", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--tier", required=True)
    ap.add_argument("--silesia-tar")
    ap.add_argument("--data")
    ap.add_argument("--typed")
    ap.add_argument("--raw")
    ap.add_argument("--only")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    rows = plan(a)
    if a.only:
        rows = [x for x in rows if re.search(a.only, os.path.basename(x[1]) + " " + x[2])]
    if a.list:
        for rnd, path, entry, tmin, c in rows:
            print(rnd, os.path.basename(path), entry, "-t" + tmin, "-i" + ITERS[c])
        print(len(rows), "rows")
        return
    os.makedirs(a.out, exist_ok=True)
    log = os.path.join(a.out, a.tier + ".jsonl")
    done = set()
    if os.path.exists(log):
        for line in open(log):
            r = json.loads(line)
            if r.get("type") == "row" and ok(r):
                done.add((r["round"], r["file"], r["entry"]))
    with open(log, "a") as f:
        info = os.path.join(os.path.dirname(a.lzbench), "misc", "kolmo", "BUILD-INFO.txt")
        env = {"type": "env", "when": time.strftime("%Y-%m-%d %H:%M:%S"), "tier": a.tier, "platform": platform.platform(),
               "machine": subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], stdout=subprocess.PIPE, text=True).stdout.strip(),
               "lzbench": a.lzbench, "lzbench_sha256": sha256(a.lzbench),
               "kolmo_build": open(info).read() if os.path.exists(info) else "", "load": os.getloadavg(),
               "watcher_running": watcher_running(), "rows_planned": len(rows), "rows_done_before": len(done)}
        f.write(json.dumps(env) + "\n")
        f.flush()
        for i, (rnd, path, entry, tmin, c) in enumerate(rows, 1):
            base = os.path.basename(path)
            if (rnd, base, entry) in done:
                continue
            r = run_one(a.lzbench, path, entry, tmin, ITERS[c], os.path.join(a.out, a.tier + ".last"))
            r.update(type="row", tier=a.tier, round=rnd, file=base, entry=entry, when=time.strftime("%H:%M:%S"))
            f.write(json.dumps(r) + "\n")
            f.flush()
            os.fsync(f.fileno())
            if "error" in r:
                print("%s [%d/%d] %s %s: FAILED rc=%s %s" % (a.tier, i, len(rows), base, entry, r["rc"], r["error"][-200:].replace("\n", " ")), flush=True)
            else:
                print("%s [%d/%d] %s %s %s: %d -> %d, %.1f / %.1f MB/s, cpu %.1f s of %.1f s%s%s" % (
                    a.tier, i, len(rows), rnd, base, entry, r["n"], r["size"], r["c_mbps"], r["d_mbps"], r["cpu_s"], r["elapsed_s"],
                    " THREADS" if r["threads"] else "", " BUSY" if r["busy"] else "") +
                    "".join(" +%s %.0f%%" % (o[0], o[1]) for o in r["others"]), flush=True)
        f.write(json.dumps({"type": "end", "when": time.strftime("%Y-%m-%d %H:%M:%S"), "tier": a.tier,
                            "watcher_running": watcher_running(), "load": os.getloadavg()}) + "\n")
    print(a.tier, "finished", time.strftime("%H:%M:%S"), flush=True)


if __name__ == "__main__":
    main()
