#!/usr/bin/env python3
"""Checkpointed one-thread benchmark: stock Ultra, the harness control, the LSTM candidate, and zpaq -m5.

usage: run.py CONFIG.json             drive every (round, input, arm) cell that has no result yet
       run.py --cell CONFIG.json ID   run one cell (the driver calls this through $V/tools/timed.sh)

Each cell runs one encoder process and one decoder process, timed separately. Each process is measured with
os.wait4: wall seconds, user and system CPU, and ru_maxrss (bytes on macOS). Hashing happens after timing.
Each cell writes cells/<ID>.json, so a stopped run resumes at the first missing cell. The order within each
round is shuffled with a fixed seed, and the planned order is written to order.json before anything runs.
"""
import hashlib, json, os, random, shutil, subprocess, sys, time

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def timed(argv, stdin=None, stdout=None, env=None, cwd=None):
    fi = open(stdin, "rb") if stdin else subprocess.DEVNULL
    fo = open(stdout, "wb") if stdout else subprocess.DEVNULL
    fe = open(os.devnull + "", "wb")
    err_path = None
    t0 = time.perf_counter()
    p = subprocess.Popen(argv, stdin=fi, stdout=fo, stderr=subprocess.PIPE, env=env, cwd=cwd)
    err = p.stderr.read()
    _, status, ru = os.wait4(p.pid, 0)
    wall = time.perf_counter() - t0
    p.returncode = os.waitstatus_to_exitcode(status)
    for f in (fi, fo):
        if hasattr(f, "close"): f.close()
    fe.close()
    return {"argv": argv, "rc": p.returncode, "wall_s": wall, "user_s": ru.ru_utime, "sys_s": ru.ru_stime,
            "maxrss_b": ru.ru_maxrss, "stderr": err.decode(errors="replace")[-2000:]}

def arm_cmds(cfg, arm, src, work):
    """(encode step, decode step, frame path, restored path). Each step is (argv, stdin, stdout, env, cwd)."""
    fr, rs = os.path.join(work, "frame"), os.path.join(work, "restored")
    K, H = cfg["kolmo"], cfg["harness"]
    if arm == "stock":
        return ([K, "-U", "-T", "1", "-c", src], None, fr, None, None), ([K, "-d", "-T", "1", "-c", fr], None, rs, None, None), fr, rs
    if arm == "control":
        return ([H, "enc0", src, fr], None, None, None, None), ([H, "dec0", fr, rs], None, None, None, None), fr, rs
    if arm == "candidate":
        env = dict(os.environ, **{k: str(v) for k, v in cfg["candidate_env"].items()})
        return ([H, "enc", src, fr], None, None, env, None), ([H, "dec", fr, rs], None, None, None, None), fr, rs
    if arm == "zpaq":
        zin, zout = os.path.join(work, "zin"), os.path.join(work, "zout")
        os.makedirs(zin, exist_ok=True); os.makedirs(zout, exist_ok=True)
        f = os.path.join(zin, "f")
        if not os.path.exists(f): os.link(src, f) if os.stat(src).st_dev == os.stat(zin).st_dev else shutil.copyfile(src, f)
        arc = os.path.join(work, "frame.zpaq")
        return ([cfg["zpaq"], "a", arc, "f", "-m5", "-t1"], None, None, None, zin), \
               ([cfg["zpaq"], "x", arc, "-to", zout, "-t1"], None, None, None, work), arc, os.path.join(zout, "f")
    raise SystemExit("unknown arm " + arm)

def run_cell(cfg_path, cid):
    cfg = json.load(open(cfg_path)); out = cfg["out"]
    rnd, inp, arm = cid.split("__")
    src = cfg["inputs"][inp]
    work = os.path.join(out, "work", cid); shutil.rmtree(work, ignore_errors=True); os.makedirs(work)
    enc, dec, fr, rs = arm_cmds(cfg, arm, src, work)
    load0 = os.getloadavg()[0]
    e = timed(enc[0], enc[1], enc[2], enc[3], enc[4])
    d = timed(dec[0], dec[1], dec[2], dec[3], dec[4]) if e["rc"] == 0 else {"rc": -1}
    load1 = os.getloadavg()[0]
    n = os.path.getsize(src)
    rec = {"id": cid, "round": int(rnd[1:]), "input": inp, "arm": arm, "n": n, "enc": e, "dec": d,
           "load1_before": load0, "load1_after": load1, "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    rec["input_sha256"] = sha(src)
    rec["frame_bytes"] = os.path.getsize(fr) if os.path.exists(fr) else None
    rec["frame_sha256"] = sha(fr) if os.path.exists(fr) else None
    rec["restored_sha256"] = sha(rs) if rs and os.path.exists(rs) else None
    rec["roundtrip_ok"] = rec["restored_sha256"] == rec["input_sha256"]
    # keep the frame of round 1 for cross-checks; drop everything else
    if rec["round"] == 1 and rec["frame_sha256"]:
        keep = os.path.join(out, "frames"); os.makedirs(keep, exist_ok=True)
        shutil.copyfile(fr, os.path.join(keep, f"{inp}__{arm}.frame"))
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(out, "cells"), exist_ok=True)
    tmp = os.path.join(out, "cells", cid + ".json.tmp")
    json.dump(rec, open(tmp, "w"), indent=1); os.replace(tmp, os.path.join(out, "cells", cid + ".json"))
    print(cid, rec["frame_bytes"], round(e["wall_s"], 3), round(d.get("wall_s", -1), 3), "OK" if rec["roundtrip_ok"] else "FAIL", flush=True)

def drive(cfg_path):
    cfg = json.load(open(cfg_path)); out = cfg["out"]; os.makedirs(out, exist_ok=True)
    op = os.path.join(out, "order.json")
    if os.path.exists(op): order = json.load(open(op))
    else:
        rng = random.Random(cfg.get("seed", 20261125)); order = []
        for r in range(1, cfg["rounds"] + 1):
            cells = [f"r{r}__{i}__{a}" for i in cfg["inputs"] for a in cfg["arms"]]
            rng.shuffle(cells); order += cells
        json.dump(order, open(op, "w"), indent=1)
    for cid in order:
        if os.path.exists(os.path.join(out, "cells", cid + ".json")): continue
        subprocess.run([cfg["timed"], cfg.get("tag", "ultra-lstm"), sys.executable, os.path.abspath(__file__), "--cell", cfg_path, cid], check=False)

if __name__ == "__main__":
    if sys.argv[1] == "--cell": run_cell(sys.argv[2], sys.argv[3])
    else: drive(sys.argv[1])
