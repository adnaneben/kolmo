"""Time one cell (one configuration on one input): REPS encodes and REPS decodes.

Run by run.py under timed.sh (the machine-wide timing lock). Reads a JSON spec
from argv[1], prints one JSON result on stdout. Each encode and decode is a
separate process; wall time is measured around fork/exec/wait (process start and
file I/O included, as in Codex's tip-benchmark), peak RSS comes from wait4.
Every decode's output is checked by SHA-256 against the input.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def run(argv, stdin_path, stdout_path, cwd, env):
    fin = open(stdin_path, "rb") if stdin_path else subprocess.DEVNULL
    fout = open(stdout_path, "wb") if stdout_path else subprocess.DEVNULL
    try:
        t0 = time.perf_counter_ns()
        p = subprocess.Popen(argv, stdin=fin, stdout=fout, stderr=subprocess.PIPE, cwd=cwd, env=env)
        _, status, ru = os.wait4(p.pid, 0)
        t1 = time.perf_counter_ns()
        err = p.stderr.read().decode("utf-8", "replace")[-400:]
        p.stderr.close()
        p.returncode = os.waitstatus_to_exitcode(status)
    finally:
        if stdin_path:
            fin.close()
        if stdout_path:
            fout.close()
    # macOS reports ru_maxrss in bytes.
    return t1 - t0, p.returncode, ru.ru_maxrss, ru.ru_utime + ru.ru_stime, err


def main():
    spec = json.load(open(sys.argv[1]))
    load_start = os.getloadavg()[0]
    if load_start >= spec["max_load"]:
        # Inside the lock, but the machine is busy: time nothing, release the lock.
        print(json.dumps(dict(skipped=True, load_start=load_start)))
        return
    work = spec["work"]
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    src = spec["input"]
    subs = dict(spec["subs"], **{"in": src, "out": os.path.join(work, "c.bin"), "rt": os.path.join(work, "rt.bin"),
                                 "rtdir": os.path.join(work, "rtdir"), "dir": os.path.dirname(src),
                                 "name": os.path.basename(src)})

    def fill(x):
        return x.format(**subs)

    enc = [fill(a) for a in spec["enc"]]
    dec = [fill(a) for a in spec["dec"]]
    so_enc, so_dec = spec["stdout"]
    rt_path = fill(spec.get("rt_path", "{rt}"))
    enc_cwd = fill(spec["enc_cwd"]) if spec.get("enc_cwd") else work
    env = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}
    res = dict(enc_ns=[], dec_ns=[], enc_rss=[], dec_rss=[], enc_cpu=[], dec_cpu=[], comp_bytes=[], comp_sha=[],
               rt_ok=[], error=None)
    for _ in range(spec["reps"]):
        for p in (subs["out"], subs["rt"]):
            if os.path.exists(p):
                os.remove(p)
        shutil.rmtree(subs["rtdir"], ignore_errors=True)
        ns, rc, rss, cpu, err = run(enc, None, subs["out"] if so_enc else None, enc_cwd, env)
        if rc != 0:
            res["error"] = f"encode rc={rc}: {err}"
            break
        res["enc_ns"].append(ns); res["enc_rss"].append(rss); res["enc_cpu"].append(cpu)
        res["comp_bytes"].append(os.path.getsize(subs["out"]))
        res["comp_sha"].append(sha256(subs["out"]))
        os.makedirs(subs["rtdir"], exist_ok=True)
        ns, rc, rss, cpu, err = run(dec, None, subs["rt"] if so_dec else None, work, env)
        if rc != 0:
            res["error"] = f"decode rc={rc}: {err}"
            break
        res["dec_ns"].append(ns); res["dec_rss"].append(rss); res["dec_cpu"].append(cpu)
        ok = os.path.exists(rt_path) and sha256(rt_path) == spec["input_sha256"]
        res["rt_ok"].append(ok)
        if not ok:
            res["error"] = "round trip SHA-256 mismatch"
            break
    shutil.rmtree(work, ignore_errors=True)
    res["load_start"] = load_start
    res["load_end"] = os.getloadavg()[0]
    print(json.dumps(res))


if __name__ == "__main__":
    main()
