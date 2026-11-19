#!/usr/bin/env python3
"""Kolmo RC3 review benchmark harness (evidence contract, runbook phase F). macOS (Darwin) only for timing runs.

Subcommands (every --out / --new-run must not exist; nothing is ever overwritten or deleted except this run's own
verified decode outputs, see DECODED OUTPUTS below):
  self-test  --out DIR                       harness tests with synthetic codecs (no Kolmo needed)
  inventory  --config C --out DIR            tool paths, versions, SHA-256, architectures; UNAVAILABLE rows
  preflight  --config C --out DIR            input/binary identity, one untimed round trip per codec x input,
                                             thread observation pass (sampled thread counts, CPU/wall)
  run        --config C --suite S --new-run DIR [--resume]   timed trials (append-only trials.jsonl)
  report     --run DIR --out DIR             medians, IQR, paired uncertainty, green/red/gray scoreboard

Trial protocol (cold-process CLI track): for each round (warmups, then repetitions) the (input, codec) pairs run in
an order shuffled with the saved seed. A trial = encode in a fresh process, then decode its output in another fresh
process. Each process: wall time spawn->wait4, user/system CPU from wait4 rusage, kernel peak RSS (ru_maxrss; bytes on
Darwin), and a sampler thread polling proc_pidinfo(PROC_PIDTASKINFO) for resident size and thread count every
sample_ms: time-weighted average RSS = integral(RSS dt) / sampled interval (trapezoid), sampled max, coverage.
Hashing happens outside timed regions. Every decode is byte-verified against the input SHA-256.

Quiet machine (preregistered in the config, independent of results): other_cores = (host busy CPU seconds during
the process - the process's own CPU - this harness's own CPU) / wall. A process whose other_cores exceeds
quiet.max_other_cores, or that starts on battery or under thermal limit, makes the trial REJECTED (kept, with the
reason); the pair is retried later in a new trial id, up to quiet.max_retries. Host ticks are 10 ms per CPU, so
short processes have coarse contention evidence: recorded as quiet_resolution.

DECODED OUTPUTS: a verified decoded copy equals the input byte for byte and is removed after verification to bound
disk use (scratch created by this run); a mismatching decode is kept. Compressed outputs: the first of each
(codec, input) is kept in artifacts/, later ones are kept only when their hash differs; every hash is recorded.
"""
import argparse, ctypes, ctypes.util, hashlib, json, os, platform, random, resource, shutil, signal, statistics
import subprocess, sys, threading, time

HARNESS_VERSION = "review-rc3-audit/1"

# ----------------------------------------------------------------------------------------------------------- utils
def sha256_file(p, bufsize=1 << 20):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while True:
            c = f.read(bufsize)
            if not c: break
            h.update(c)
    return h.hexdigest()

def sha256_bytes(b): return hashlib.sha256(b).hexdigest()

def now_utc(): return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

def new_dir(p):
    """create a NEW directory; refuse an existing path (file, dir or symlink)"""
    if os.path.lexists(p): raise SystemExit(f"refusing existing destination: {p}")
    os.makedirs(p)
    return p

def xopen(p, mode="x"):
    return open(p, mode)   # 'x' fails if the file exists

def maxrss_bytes(ru_maxrss, system=None):
    """normalise ru_maxrss: Darwin reports bytes, Linux KiB"""
    system = system or platform.system()
    if system == "Darwin": return int(ru_maxrss)
    if system == "Linux": return int(ru_maxrss) * 1024
    raise ValueError(f"unknown ru_maxrss unit on {system}")

def time_weighted_avg(samples):
    """samples: [(t_seconds, value)] sorted by t. Trapezoid integral / interval. Returns (avg, interval_s, n)."""
    if not samples: return None, 0.0, 0
    if len(samples) == 1: return float(samples[0][1]), 0.0, 1
    area = 0.0
    for (t0, v0), (t1, v1) in zip(samples, samples[1:]):
        area += (t1 - t0) * (v0 + v1) / 2.0
    span = samples[-1][0] - samples[0][0]
    return (area / span if span > 0 else float(samples[0][1])), span, len(samples)

def quantiles(xs):
    xs = sorted(xs)
    if not xs: return None
    def q(p):
        k = (len(xs) - 1) * p; f = int(k); c = min(f + 1, len(xs) - 1)
        return xs[f] + (xs[c] - xs[f]) * (k - f)
    return {"n": len(xs), "min": xs[0], "q1": q(0.25), "median": q(0.5), "q3": q(0.75), "max": xs[-1]}

def bootstrap_ci(xs, seed, n=4000, stat=statistics.median, alpha=0.05):
    if len(xs) < 2: return None
    r = random.Random(seed); m = len(xs); vals = []
    for _ in range(n):
        vals.append(stat([xs[r.randrange(m)] for _ in range(m)]))
    vals.sort()
    return [vals[int(alpha / 2 * n)], vals[int((1 - alpha / 2) * n) - 1]]

# ----------------------------------------------------------------------------------------------- macOS probes
class Darwin:
    PROC_PIDTASKINFO = 4
    class TaskInfo(ctypes.Structure):
        _fields_ = [("virtual_size", ctypes.c_uint64), ("resident_size", ctypes.c_uint64),
                    ("total_user", ctypes.c_uint64), ("total_system", ctypes.c_uint64),
                    ("threads_user", ctypes.c_uint64), ("threads_system", ctypes.c_uint64),
                    ("policy", ctypes.c_int32), ("faults", ctypes.c_int32), ("pageins", ctypes.c_int32),
                    ("cow_faults", ctypes.c_int32), ("messages_sent", ctypes.c_int32),
                    ("messages_received", ctypes.c_int32), ("syscalls_mach", ctypes.c_int32),
                    ("syscalls_unix", ctypes.c_int32), ("csw", ctypes.c_int32), ("threadnum", ctypes.c_int32),
                    ("numrunning", ctypes.c_int32), ("priority", ctypes.c_int32)]
    def __init__(self):
        self.ok = platform.system() == "Darwin"
        if not self.ok: return
        self.libproc = ctypes.CDLL("/usr/lib/libproc.dylib")
        self.libproc.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
        self.libc = ctypes.CDLL(ctypes.util.find_library("c"))
        self.libc.mach_host_self.restype = ctypes.c_uint32
        self.host = self.libc.mach_host_self()
    def task(self, pid):
        ti = Darwin.TaskInfo()
        n = self.libproc.proc_pidinfo(pid, Darwin.PROC_PIDTASKINFO, 0, ctypes.byref(ti), ctypes.sizeof(ti))
        return ti if n == ctypes.sizeof(ti) else None
    def cpu_ticks(self):
        """host-wide (user, system, idle, nice) ticks, 100 Hz per CPU"""
        info = (ctypes.c_uint32 * 4)(); cnt = ctypes.c_uint32(4)
        if self.libc.host_statistics(self.host, 3, ctypes.byref(info), ctypes.byref(cnt)) != 0: return None
        return tuple(info)

def busy_seconds(t0, t1):
    if not t0 or not t1: return None
    d = [b - a for a, b in zip(t0, t1)]
    return (d[0] + d[1] + d[3]) / 100.0

def machine_state():
    s = {"utc": now_utc()}
    for k, cmd in (("therm", ["pmset", "-g", "therm"]), ("batt", ["pmset", "-g", "batt"])):
        try: s[k] = subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout.strip()
        except Exception as e: s[k] = f"unavailable: {e!r}"
    s["on_ac"] = "AC Power" in s.get("batt", "")
    t = s.get("therm", "")
    s["thermal_limited"] = ("CPU_Speed_Limit" in t and "CPU_Speed_Limit \t= 100" not in t and "CPU_Speed_Limit = 100" not in t
                            and any(ch.isdigit() for ch in t.split("CPU_Speed_Limit")[-1][:12]) and "= 100" not in t.split("CPU_Speed_Limit")[-1][:12])
    s["loadavg"] = os.getloadavg()
    return s

def machine_identity():
    def sh(c):
        try: return subprocess.run(c, capture_output=True, text=True, timeout=10).stdout.strip()
        except Exception as e: return f"unavailable: {e!r}"
    return {"platform": platform.platform(), "machine": platform.machine(), "python": sys.version,
            "cpu": sh(["sysctl", "-n", "machdep.cpu.brand_string"]), "memsize": sh(["sysctl", "-n", "hw.memsize"]),
            "ncpu": sh(["sysctl", "-n", "hw.ncpu"]), "perf_cores": sh(["sysctl", "-n", "hw.perflevel0.physicalcpu"]),
            "eff_cores": sh(["sysctl", "-n", "hw.perflevel1.physicalcpu"]), "sw_vers": sh(["sw_vers"]),
            "model": sh(["sysctl", "-n", "hw.model"]), "translated": sh(["sysctl", "-n", "sysctl.proc_translated"])}

# -------------------------------------------------------------------------------------------- process runner
STRIP_ENV = ("KOLMO_", "OMP_", "MKL_", "OPENBLAS_", "VECLIB_", "NUMEXPR_", "RAYON_", "ASAN_", "UBSAN_", "PYTHON")
SINGLE_THREAD_ENV = {"OMP_NUM_THREADS": "1", "OMP_THREAD_LIMIT": "1", "OPENBLAS_NUM_THREADS": "1", "MKL_NUM_THREADS": "1",
                     "VECLIB_MAXIMUM_THREADS": "1", "NUMEXPR_NUM_THREADS": "1", "RAYON_NUM_THREADS": "1"}

def child_env(extra):
    env = {k: v for k, v in os.environ.items() if not k.startswith(STRIP_ENV)}
    env.update(SINGLE_THREAD_ENV); env.update(extra or {})
    return env

def measure(argv, env, log_prefix, timeout, sample_ms, dar, cwd=None, stdin_path=None, stdout_path=None, window0=None):
    """run argv as a fresh process; stdout/stderr go to NEW files (no pipe, so no deadlock). stdin_path / stdout_path
    connect the codec's own stdin/stdout to files (no shell in between, so the measured process is the codec).
    Returns a dict with wall, user, sys, kernel peak RSS, sampled RSS trace stats, thread stats, contention."""
    out_f = xopen(stdout_path or (log_prefix + ".stdout"), "wb"); err_f = xopen(log_prefix + ".stderr", "wb")
    in_f = open(stdin_path, "rb") if stdin_path else subprocess.DEVNULL
    # contention window: from window0 (end of the previous measured step or start of the quiet pre-window) to the end
    # of this process, so that it is never shorter than ~0.3 s (host ticks are 10 ms per CPU: a 20 ms process alone
    # cannot be judged). Harness CPU over the whole window is subtracted, as is the process's own CPU.
    if window0: ticks0, self0, w0 = window0
    else: self0 = resource.getrusage(resource.RUSAGE_SELF); ticks0 = dar.cpu_ticks() if dar.ok else None; w0 = time.perf_counter()
    t_start = time.perf_counter()
    p = subprocess.Popen(argv, env=env, stdout=out_f, stderr=err_f, stdin=in_f, cwd=cwd,
                         start_new_session=True)
    samples, threads, running = [], [], []
    stop = threading.Event()
    def sampler():
        while not stop.is_set():
            ti = dar.task(p.pid) if dar.ok else None
            t = time.perf_counter() - t_start
            if ti is not None:
                samples.append((t, ti.resident_size)); threads.append(ti.threadnum); running.append(ti.numrunning)
            stop.wait(sample_ms / 1000.0)
    th = threading.Thread(target=sampler, daemon=True); th.start()
    timed_out = False; deadline = t_start + timeout if timeout else None
    while True:
        pid, status, ru = os.wait4(p.pid, os.WNOHANG)
        if pid: break
        if deadline and time.perf_counter() > deadline:
            timed_out = True
            for sig_ in (signal.SIGTERM, signal.SIGKILL):
                try: os.killpg(p.pid, sig_)
                except OSError: pass
                time.sleep(2)
            pid, status, ru = os.wait4(p.pid, 0); break
        time.sleep(0.0005)
    wall = time.perf_counter() - t_start
    p.returncode = os.waitstatus_to_exitcode(status) if hasattr(os, "waitstatus_to_exitcode") else (
        -os.WTERMSIG(status) if os.WIFSIGNALED(status) else os.WEXITSTATUS(status))
    stop.set(); th.join()
    ticks1 = dar.cpu_ticks() if dar.ok else None; self1 = resource.getrusage(resource.RUSAGE_SELF)
    out_f.close(); err_f.close()
    if stdin_path: in_f.close()
    avg, span, n = time_weighted_avg(samples)
    own = ru.ru_utime + ru.ru_stime
    harness = (self1.ru_utime - self0.ru_utime) + (self1.ru_stime - self0.ru_stime)
    busy = busy_seconds(ticks0, ticks1)
    other = None if busy is None else max(0.0, busy - own - harness)
    t_end = time.perf_counter(); win = t_end - w0
    return {"argv": argv, "exit": p.returncode, "timed_out": timed_out, "wall_s": wall,
            "user_s": ru.ru_utime, "sys_s": ru.ru_stime, "cpu_over_wall": (own / wall if wall > 0 else None),
            "peak_rss_kernel_bytes": maxrss_bytes(ru.ru_maxrss),
            "rss_avg_time_weighted_bytes": avg, "rss_sampled_max_bytes": max((v for _, v in samples), default=None),
            "rss_samples": n, "rss_sampled_span_s": span, "rss_coverage": (span / wall if wall > 0 else None),
            "sample_ms": sample_ms, "threads_max": max(threads, default=None), "running_max": max(running, default=None),
            "harness_cpu_s": harness, "host_busy_cpu_s": busy, "other_cpu_s": other,
            "other_cores": (other / win if other is not None and win > 0 else None), "contention_window_s": win,
            "_window_end": (ticks1, self1, t_end),
            "stdout_bytes": os.path.getsize(stdout_path or (log_prefix + ".stdout")), "stderr_bytes": os.path.getsize(log_prefix + ".stderr"),
            "trace": [(round(t, 5), v) for t, v in samples]}

# ------------------------------------------------------------------------------------------------- config
def load_config(path):
    cfg = json.load(open(path))
    cfg["_path"] = os.path.abspath(path); cfg["_sha256"] = sha256_file(path)
    return cfg

def fsname(x):
    """file-name-safe form of a codec/input id (ids may contain '/', '+', ',')"""
    return "".join(ch if ch.isalnum() or ch in "-._" else "~" for ch in x)

def fill(argv, **kw):
    return [a.format(**kw) for a in argv]

def run_codec(cfg, c, phase, src, dst, log_prefix, sample_ms, dar, orig=None, window0=None):
    """phase 'encode'|'decode'. c[phase] is the argv after the tool path; c[phase+'_stdin'] / c[phase+'_stdout'] (bool)
    feed src on stdin / take dst from stdout instead of {in} / {out} arguments."""
    t = cfg["tools"][c["tool"]]
    return measure(fill([t["path"], *c[phase]], **{"in": src, "out": dst, "orig": orig or src}), child_env(c.get("env")), log_prefix,
                   c.get("timeout", 7200), sample_ms, dar, stdin_path=src if c.get(phase + "_stdin") else None,
                   stdout_path=dst if c.get(phase + "_stdout") else None, window0=window0)

def codec_map(cfg): return {c["id"]: c for c in cfg["codecs"]}
def input_map(cfg): return {i["id"]: i for i in cfg["inputs"]}

def identity(cfg, suite):
    """exact identity used for --resume: harness file, config file, every binary and input in the suite"""
    s = cfg["suites"][suite]; cm, im = codec_map(cfg), input_map(cfg)
    tools = {}
    for cid in s["codecs"]:
        t = cfg["tools"][cm[cid]["tool"]]; tools[cm[cid]["tool"]] = {"path": t["path"], "sha256": sha256_file(t["path"])}
    return {"harness_version": HARNESS_VERSION, "harness_sha256": sha256_file(os.path.abspath(__file__)),
            "config_sha256": cfg["_sha256"], "suite": suite, "suite_def": s, "tools": tools,
            "inputs": {i: {"path": im[i]["path"], "sha256": im[i]["sha256"], "bytes": im[i]["bytes"]} for i in s["inputs"]},
            "codecs": {c: cm[c] for c in s["codecs"]}, "seed": cfg["seed"], "repetitions": cfg["repetitions"],
            "warmups": cfg["warmups"]}

# -------------------------------------------------------------------------------------------- inventory
def cmd_inventory(a):
    cfg = load_config(a.config); out = new_dir(a.out); rows = []
    for name, t in sorted(cfg["tools"].items()):
        r = {"tool": name, "path": t.get("path")}
        if not t.get("path") or not os.path.exists(t["path"]):
            r.update(status="UNAVAILABLE", source=t.get("source"), build=t.get("build")); rows.append(r); continue
        r["sha256"] = sha256_file(t["path"]); r["realpath"] = os.path.realpath(t["path"])
        for k, c in (("file", ["file", "-b", r["realpath"]]), ("archs", ["lipo", "-archs", r["realpath"]])):
            try: r[k] = subprocess.run(c, capture_output=True, text=True, timeout=10).stdout.strip()
            except Exception as e: r[k] = repr(e)
        if t.get("version_argv"):
            p = subprocess.run([t["path"], *t["version_argv"]], capture_output=True, text=True, timeout=30, env=child_env({}))
            r["version_output"] = (p.stdout + p.stderr).strip()[:2000]
        r.update(status="AVAILABLE", license=t.get("license"), thread_note=t.get("thread_note")); rows.append(r)
    for m in cfg.get("missing_tools", []):
        rows.append({"tool": m["name"], "status": "UNAVAILABLE", **m})
    json.dump({"machine": machine_identity(), "utc": now_utc(), "tools": rows}, xopen(os.path.join(out, "inventory.json")), indent=1)
    with xopen(os.path.join(out, "TOOL_INVENTORY.md")) as f:
        f.write("| tool | status | path | sha256 | archs | version |\n|---|---|---|---|---|---|\n")
        for r in rows:
            f.write(f"| {r['tool']} | {r['status']} | {r.get('path') or ''} | {(r.get('sha256') or '')[:16]} | {r.get('archs','')} | "
                    f"{(r.get('version_output') or r.get('version') or '').splitlines()[0][:60] if (r.get('version_output') or r.get('version')) else ''} |\n")
    print(f"inventory: {sum(r['status']=='AVAILABLE' for r in rows)} available, {sum(r['status']=='UNAVAILABLE' for r in rows)} unavailable -> {out}")

# -------------------------------------------------------------------------------------------- preflight
def cmd_preflight(a):
    cfg = load_config(a.config); out = new_dir(a.out); dar = Darwin(); cm = codec_map(cfg); bad = 0
    log = xopen(os.path.join(out, "preflight.jsonl"))
    for i in cfg["inputs"]:
        h = sha256_file(i["path"]); ok = h == i["sha256"] and os.path.getsize(i["path"]) == i["bytes"]
        log.write(json.dumps({"check": "input", "id": i["id"], "ok": ok, "sha256": h}) + "\n"); bad += not ok
    for name, t in cfg["tools"].items():
        if t.get("sha256") and os.path.exists(t["path"]):
            ok = sha256_file(t["path"]) == t["sha256"]
            log.write(json.dumps({"check": "tool", "tool": name, "ok": ok}) + "\n"); bad += not ok
    suites = [a.suite] if a.suite else list(cfg["suites"])
    pairs = sorted({(i, c) for s in suites for i in cfg["suites"][s]["inputs"] for c in cfg["suites"][s]["codecs"]})
    for n, (iid, cid) in enumerate(pairs):
        inp = input_map(cfg)[iid]; c = cm[cid]; t = cfg["tools"][c["tool"]]
        d = os.path.join(out, "rt", f"{n:04d}"); os.makedirs(d)
        comp = os.path.join(d, "c" + c.get("ext", ".bin")); back = os.path.join(d, "back" + inp.get("ext", ""))
        e = run_codec(cfg, c, "encode", inp["path"], comp, os.path.join(d, "enc"), cfg.get("preflight_sample_ms", 2), dar)
        r = {"check": "roundtrip+threads", "input": iid, "codec": cid, "enc_exit": e["exit"], "enc_threads_max": e["threads_max"],
             "enc_running_max": e["running_max"], "enc_cpu_over_wall": e["cpu_over_wall"], "enc_wall_s": e["wall_s"]}
        if e["exit"] == 0 and os.path.exists(comp):
            dd = run_codec(cfg, c, "decode", comp, back, os.path.join(d, "dec"), cfg.get("preflight_sample_ms", 2), dar, orig=inp["path"])
            r.update(dec_exit=dd["exit"], dec_threads_max=dd["threads_max"], dec_running_max=dd["running_max"],
                     dec_cpu_over_wall=dd["cpu_over_wall"], comp_bytes=os.path.getsize(comp), comp_sha256=sha256_file(comp))
            r["roundtrip_ok"] = dd["exit"] == 0 and os.path.exists(back) and sha256_file(back) == inp["sha256"]
            if r["roundtrip_ok"]: os.remove(back)
        else:
            r["roundtrip_ok"] = False
        strict = c.get("thread_policy", "strict-T1") == "strict-T1"
        r["thread_policy"] = c.get("thread_policy", "strict-T1")
        # strict T1 = at most one RUNNING thread in every sample (idle helper threads are recorded but allowed);
        # sampling can miss short-lived helpers, so CPU/wall is recorded beside it
        r["strict_t1_observed"] = ((e.get("running_max") or 0) <= 1 and (r.get("dec_running_max") or 0) <= 1)
        r["single_thread_total"] = (e.get("threads_max") == 1 and r.get("dec_threads_max") == 1)
        if not r["roundtrip_ok"] or (strict and not r["strict_t1_observed"]): bad += 1
        log.write(json.dumps(r) + "\n"); log.flush()
        print(f"{iid:28s} {cid:28s} rt={'ok' if r['roundtrip_ok'] else 'FAIL'} thr enc={r['enc_threads_max']} dec={r.get('dec_threads_max')} {'' if r['strict_t1_observed'] else '(not strict T1)'}")
    log.close()
    json.dump({"utc": now_utc(), "problems": bad, "machine": machine_identity(), "state": machine_state()},
              xopen(os.path.join(out, "summary.json")), indent=1)
    print(f"preflight problems: {bad}"); sys.exit(1 if bad else 0)

# ---------------------------------------------------------------------------------------------------- run
def wait_quiet(cfg, dar, log):
    q = cfg["quiet"]
    for attempt in range(q.get("pre_wait_tries", 60)):
        t0 = dar.cpu_ticks(); s0 = resource.getrusage(resource.RUSAGE_SELF); w0 = time.perf_counter(); time.sleep(q.get("pre_window_s", 1.0))
        t1 = dar.cpu_ticks(); s1 = resource.getrusage(resource.RUSAGE_SELF)
        other = busy_seconds(t0, t1) - ((s1.ru_utime - s0.ru_utime) + (s1.ru_stime - s0.ru_stime))
        cores = other / q.get("pre_window_s", 1.0)
        if cores <= q["max_other_cores"]: return {"pre_other_cores": cores, "pre_attempts": attempt + 1, "_window0": (t0, s0, w0)}
        log.write(json.dumps({"event": "not-quiet", "utc": now_utc(), "other_cores": cores}) + "\n"); log.flush()
        time.sleep(q.get("pre_backoff_s", 5))
    return {"pre_other_cores": cores, "pre_attempts": q.get("pre_wait_tries", 60), "pre_quiet_failed": True, "_window0": (t0, s0, w0)}

def judge(proc, cfg, state):
    q = cfg["quiet"]; reasons = []
    if proc["other_cores"] is not None and proc["other_cores"] > q["max_other_cores"]:
        reasons.append(f"contention other_cores={proc['other_cores']:.3f}>{q['max_other_cores']}")
    if q.get("require_ac", True) and not state.get("on_ac", True): reasons.append("on battery")
    if state.get("thermal_limited"): reasons.append("thermal limit")
    return reasons

def cmd_run(a):
    cfg = load_config(a.config); suite = a.suite; dar = Darwin()
    if not dar.ok: raise SystemExit("timing runs are implemented for macOS only")
    ident = identity(cfg, suite)
    if a.resume:
        if not os.path.isdir(a.new_run): raise SystemExit("--resume needs the existing run directory")
        old = json.load(open(os.path.join(a.new_run, "identity.json")))
        if old != json.loads(json.dumps(ident)): raise SystemExit("--resume refused: identity differs (config, harness, binaries or inputs)")
        run = a.new_run
    else:
        run = new_dir(a.new_run)
        json.dump(ident, xopen(os.path.join(run, "identity.json")), indent=1, sort_keys=True)
        json.dump({"machine": machine_identity(), "start": machine_state(), "argv": sys.argv, "cwd": os.getcwd()},
                  xopen(os.path.join(run, "run.json")), indent=1)
        for d in ("artifacts", "logs", "work", "traces"): os.mkdir(os.path.join(run, d))
    done = set(); attempts = {}
    tpath = os.path.join(run, "trials.jsonl")
    if os.path.exists(tpath):
        for l in open(tpath):
            r = json.loads(l)
            attempts[r["slot"]] = attempts.get(r["slot"], 0) + 1
            if r["status"] in ("accepted", "failed", "timeout", "roundtrip_mismatch", "encode_failed", "warmup"): done.add(r["slot"])
    trials = open(tpath, "a"); events = open(os.path.join(run, "events.jsonl"), "a")
    s = cfg["suites"][suite]; cm, im = codec_map(cfg), input_map(cfg)
    pairs = [(i, c) for i in s["inputs"] for c in s["codecs"]]
    kept = {}
    for f in os.listdir(os.path.join(run, "artifacts")):
        kept.setdefault(f.split("__")[0] + "__" + f.split("__")[1], set()).add(f.split("__")[-1].split(".")[0])
    rounds = [("warmup", k) for k in range(cfg["warmups"])] + [("timed", k) for k in range(cfg["repetitions"])]
    state = machine_state(); state_t = time.time()
    for ri, (role, k) in enumerate(rounds):
        order = pairs[:]; random.Random(f"{cfg['seed']}:{suite}:{ri}").shuffle(order)
        events.write(json.dumps({"event": "round", "round": ri, "role": role, "order": order, "utc": now_utc()}) + "\n"); events.flush()
        queue = list(order)
        while queue:
            iid, cid = queue.pop(0); slot = f"r{ri}:{iid}:{cid}"
            if slot in done: continue
            n_att = attempts.get(slot, 0)
            if n_att > cfg["quiet"].get("max_retries", 4):
                continue
            attempts[slot] = n_att + 1
            if time.time() - state_t > cfg.get("state_every_s", 300): state = machine_state(); state_t = time.time()
            inp = im[iid]; c = cm[cid]; t = cfg["tools"][c["tool"]]
            tid = f"{slot}:a{n_att}"; safe = fsname(tid.replace(":", "__"))
            work = os.path.join(run, "work", safe)
            os.mkdir(work)       # new per attempt; fails if it exists
            comp = os.path.join(work, "c" + c.get("ext", ".bin")); back = os.path.join(work, "back" + inp.get("ext", ""))
            for p_ in (comp, back):
                if os.path.lexists(p_): raise SystemExit(f"refusing to let a codec overwrite {p_}")
            pre = wait_quiet(cfg, dar, events); window0 = pre.pop("_window0")
            env = child_env(c.get("env"))
            row = {"trial_id": tid, "slot": slot, "round": ri, "role": role, "rep": k, "suite": suite, "input": iid,
                   "codec": cid, "utc": now_utc(), "tool": c["tool"], "tool_path": t["path"], "tool_sha256": ident["tools"][c["tool"]]["sha256"],
                   "input_sha256": inp["sha256"], "input_bytes": inp["bytes"], "input_domain": inp.get("domain"),
                   "env_set": c.get("env", {}), "thread_policy": c.get("thread_policy", "strict-T1"), **pre,
                   "power": {"on_ac": state.get("on_ac"), "thermal_limited": state.get("thermal_limited")}}
            e = run_codec(cfg, c, "encode", inp["path"], comp, os.path.join(run, "logs", safe + ".enc"), cfg["sample_ms"], dar, window0=window0)
            row["encode"] = {k_: v for k_, v in e.items() if k_ not in ("trace", "_window_end")}
            json.dump(e["trace"], xopen(os.path.join(run, "traces", safe + ".enc.json")))
            reasons = judge(e, cfg, state) + (["pre-window not quiet"] if pre.get("pre_quiet_failed") else [])
            if e["timed_out"] or e["exit"] != 0 or not os.path.exists(comp):
                row["status"] = "timeout" if e["timed_out"] else "encode_failed"
            else:
                cb = os.path.getsize(comp); ch = sha256_file(comp); row.update(comp_bytes=cb, comp_sha256=ch)
                key = f"{fsname(cid)}__{fsname(iid)}"
                if ch[:16] not in kept.get(key, set()):
                    dst = os.path.join(run, "artifacts", f"{key}__{ch[:16]}{c.get('ext', '.bin')}")
                    if not os.path.lexists(dst): shutil.copyfile(comp, dst)
                    kept.setdefault(key, set()).add(ch[:16])
                pre_d = wait_quiet(cfg, dar, events); window0_d = pre_d.pop("_window0")   # own >= 0.3 s window
                row["decode_pre"] = pre_d
                d = run_codec(cfg, c, "decode", comp, back, os.path.join(run, "logs", safe + ".dec"), cfg["sample_ms"], dar, orig=inp["path"], window0=window0_d)
                row["decode"] = {k_: v for k_, v in d.items() if k_ not in ("trace", "_window_end")}
                json.dump(d["trace"], xopen(os.path.join(run, "traces", safe + ".dec.json")))
                reasons += judge(d, cfg, state) + (["decode pre-window not quiet"] if pre_d.get("pre_quiet_failed") else [])
                if d["timed_out"]: row["status"] = "timeout"
                elif d["exit"] != 0 or not os.path.exists(back): row["status"] = "failed"
                else:
                    bh = sha256_file(back); row["decoded_sha256"] = bh
                    if bh != inp["sha256"]: row["status"] = "roundtrip_mismatch"
                    else:
                        os.remove(back); row["decoded_removed_after_verify"] = True
                        row["status"] = "warmup" if role == "warmup" else ("rejected" if reasons else "accepted")
                os.remove(comp) if os.path.exists(comp) and row["status"] in ("accepted", "warmup", "rejected") else None
            row["reject_reasons"] = reasons
            trials.write(json.dumps(row) + "\n"); trials.flush()
            if row["status"] == "rejected": queue.append((iid, cid))   # retried later in this round, new trial id
            else: done.add(slot)
            print(f"[{ri}:{role}] {iid:24s} {cid:26s} {row['status']:9s} enc={e['wall_s']:.3f}s "
                  f"dec={row.get('decode', {}).get('wall_s', float('nan')):.3f}s {'; '.join(reasons)}", flush=True)
    k = 0
    while os.path.lexists(os.path.join(run, f"end-{time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())}-{k}.json")): k += 1
    json.dump({"end": machine_state(), "resumed": bool(a.resume)},
              xopen(os.path.join(run, f"end-{time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())}-{k}.json")), indent=1)
    trials.close(); events.close()

# ------------------------------------------------------------------------------------------------- report
def cmd_report(a):
    run = a.run; out = new_dir(a.out)
    ident = json.load(open(os.path.join(run, "identity.json")))
    rows = [json.loads(l) for l in open(os.path.join(run, "trials.jsonl"))]
    acc = [r for r in rows if r["status"] == "accepted"]
    seed = ident["seed"]; tol = a.tolerance
    by = {}
    for r in acc: by.setdefault((r["codec"], r["input"]), []).append(r)
    stats = {}
    for (cid, iid), rs in by.items():
        sizes = sorted({r["comp_bytes"] for r in rs})
        stats[(cid, iid)] = {
            "codec": cid, "input": iid, "domain": rs[0].get("input_domain"), "input_bytes": rs[0]["input_bytes"], "n": len(rs),
            "comp_bytes": sizes[0] if len(sizes) == 1 else sizes, "deterministic_size": len(sizes) == 1,
            "ratio": rs[0]["input_bytes"] / sizes[0] if sizes[0] else None,
            "enc_wall": quantiles([r["encode"]["wall_s"] for r in rs]), "dec_wall": quantiles([r["decode"]["wall_s"] for r in rs]),
            "enc_cpu": quantiles([r["encode"]["user_s"] + r["encode"]["sys_s"] for r in rs]),
            "dec_cpu": quantiles([r["decode"]["user_s"] + r["decode"]["sys_s"] for r in rs]),
            "enc_rss_avg": quantiles([r["encode"]["rss_avg_time_weighted_bytes"] for r in rs if r["encode"]["rss_avg_time_weighted_bytes"]]),
            "dec_rss_avg": quantiles([r["decode"]["rss_avg_time_weighted_bytes"] for r in rs if r["decode"]["rss_avg_time_weighted_bytes"]]),
            "enc_peak_kernel_max": max(r["encode"]["peak_rss_kernel_bytes"] for r in rs),
            "dec_peak_kernel_max": max(r["decode"]["peak_rss_kernel_bytes"] for r in rs),
            "enc_threads_max": max((r["encode"]["threads_max"] or 0) for r in rs), "dec_threads_max": max((r["decode"]["threads_max"] or 0) for r in rs),
        }
    status_count = {}
    for r in rows: status_count[r["status"]] = status_count.get(r["status"], 0) + 1
    expected = {(c, i) for c in ident["suite_def"]["codecs"] for i in ident["suite_def"]["inputs"]}
    missing = sorted(expected - set(by))
    comps = []
    for pair in a.compare or []:
        A, B = pair.split(":")   # A = candidate, B = reference
        for iid in ident["suite_def"]["inputs"]:
            ra = {r["round"]: r for r in by.get((A, iid), [])}; rb = {r["round"]: r for r in by.get((B, iid), [])}
            common = sorted(set(ra) & set(rb))
            res = {"candidate": A, "reference": B, "input": iid, "paired_rounds": len(common)}
            if not common:
                res["status"] = "MISSING"; comps.append(res); continue
            sa, sb = stats[(A, iid)], stats[(B, iid)]
            ca, cb = sa["comp_bytes"], sb["comp_bytes"]
            if isinstance(ca, int) and isinstance(cb, int):
                res["space_improvement_pct"] = 100.0 * (cb - ca) / cb
                res["size"] = "win" if ca < cb else "loss" if ca > cb else "tie"
            for ph in ("encode", "decode"):
                ratios = [rb[k][ph]["wall_s"] / ra[k][ph]["wall_s"] for k in common]   # >1: candidate faster
                med = statistics.median(ratios); ci = bootstrap_ci(ratios, seed)
                lab = "provisional" if len(common) < 5 else ("win" if ci and ci[0] > 1 + tol else "loss" if ci and ci[1] < 1 - tol else "tie")
                res[f"{ph}_speedup_median"] = med; res[f"{ph}_speedup_ci95"] = ci; res[ph] = lab
                for m, key in (("avg", "rss_avg_time_weighted_bytes"), ("peak", "peak_rss_kernel_bytes")):
                    va = statistics.median([ra[k][ph][key] for k in common if ra[k][ph][key]])
                    vb = statistics.median([rb[k][ph][key] for k in common if rb[k][ph][key]])
                    imp = 100.0 * (vb - va) / vb
                    res[f"{ph}_mem_{m}_improvement_pct"] = imp
                    res[f"{ph}_mem_{m}"] = "win" if imp > 100 * tol else "loss" if imp < -100 * tol else "tie"
            comps.append(res)
    # domain aggregates: total bytes / sum of median times
    agg = {}
    for (cid, iid), s_ in stats.items():
        g = agg.setdefault((cid, s_["domain"]), {"codec": cid, "domain": s_["domain"], "orig": 0, "comp": 0, "enc_s": 0.0, "dec_s": 0.0, "files": 0})
        if isinstance(s_["comp_bytes"], int):
            g["orig"] += s_["input_bytes"]; g["comp"] += s_["comp_bytes"]; g["enc_s"] += s_["enc_wall"]["median"]; g["dec_s"] += s_["dec_wall"]["median"]; g["files"] += 1
    for g in agg.values():
        g["ratio"] = g["orig"] / g["comp"] if g["comp"] else None
        g["enc_MiBps"] = g["orig"] / 2**20 / g["enc_s"] if g["enc_s"] else None
        g["dec_MiBps"] = g["orig"] / 2**20 / g["dec_s"] if g["dec_s"] else None
    rep = {"run": os.path.abspath(run), "identity_sha256": sha256_bytes(json.dumps(ident, sort_keys=True).encode()),
           "status_counts": status_count, "missing_pairs": missing, "tolerance": tol,
           "stats": list(stats.values()), "comparisons": comps, "aggregates": list(agg.values())}
    json.dump(rep, xopen(os.path.join(out, "report.json")), indent=1)
    write_html(rep, ident, os.path.join(out, "scoreboard.html"))
    with xopen(os.path.join(out, "per_file.csv")) as f:
        f.write("codec,input,domain,input_bytes,comp_bytes,ratio,n,enc_med_s,enc_q1,enc_q3,dec_med_s,dec_q1,dec_q3,enc_rss_avg_med,enc_peak_max,dec_rss_avg_med,dec_peak_max,enc_threads_max,dec_threads_max\n")
        for s_ in sorted(stats.values(), key=lambda x: (x["input"], x["codec"])):
            f.write(",".join(str(x) for x in [s_["codec"], s_["input"], s_["domain"], s_["input_bytes"], s_["comp_bytes"] if isinstance(s_["comp_bytes"], int) else "VARIES",
                    f"{s_['ratio']:.5f}" if s_["ratio"] else "", s_["n"], f"{s_['enc_wall']['median']:.5f}", f"{s_['enc_wall']['q1']:.5f}", f"{s_['enc_wall']['q3']:.5f}",
                    f"{s_['dec_wall']['median']:.5f}", f"{s_['dec_wall']['q1']:.5f}", f"{s_['dec_wall']['q3']:.5f}",
                    int(s_["enc_rss_avg"]["median"]) if s_["enc_rss_avg"] else "", s_["enc_peak_kernel_max"],
                    int(s_["dec_rss_avg"]["median"]) if s_["dec_rss_avg"] else "", s_["dec_peak_kernel_max"], s_["enc_threads_max"], s_["dec_threads_max"]]) + "\n")
    print(f"report: {len(stats)} codec x input cells, {len(comps)} comparisons, statuses {status_count}, missing {len(missing)} -> {out}")

def write_html(rep, ident, path):
    css = {"win": "#1b7f3b", "loss": "#b3261e", "tie": "#6b6b6b", "provisional": "#a86b00", "MISSING": "#a86b00"}
    def cell(lab, txt): return f'<td style="color:{css.get(lab, "#333")}">{txt} <b>[{lab}]</b></td>'
    h = ["<!doctype html><meta charset=utf-8><title>Kolmo review scoreboard</title>",
         "<style>body{font:13px system-ui;margin:16px}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:3px 6px;text-align:right}th{background:#eee}</style>",
         f"<h1>Scoreboard</h1><p>run {rep['run']}<br>identity {rep['identity_sha256'][:16]} | statuses {rep['status_counts']} | missing pairs {len(rep['missing_pairs'])} | tolerance {rep['tolerance']}</p>",
         "<p>Labels: win / loss require the paired 95% bootstrap interval to clear the tolerance; tie otherwise; provisional = fewer than 5 paired rounds. Speedup = reference time / candidate time.</p>",
         "<table><tr><th>candidate</th><th>reference</th><th>input</th><th>rounds</th><th>space %</th><th>enc speedup</th><th>dec speedup</th><th>enc avg RSS %</th><th>enc peak %</th><th>dec avg RSS %</th><th>dec peak %</th></tr>"]
    for c in rep["comparisons"]:
        if c.get("status") == "MISSING":
            h.append(f"<tr><td>{c['candidate']}</td><td>{c['reference']}</td><td>{c['input']}</td><td>0</td><td colspan=7>missing [MISSING]</td></tr>"); continue
        def sp(ph): return f"{c[ph + '_speedup_median']:.3f}x " + (f"[{c[ph + '_speedup_ci95'][0]:.3f},{c[ph + '_speedup_ci95'][1]:.3f}]" if c[ph + "_speedup_ci95"] else "")
        h.append(f"<tr><td>{c['candidate']}</td><td>{c['reference']}</td><td>{c['input']}</td><td>{c['paired_rounds']}</td>"
                 + (cell(c.get("size", "tie"), f"{c.get('space_improvement_pct', 0):+.3f}%") ) + cell(c["encode"], sp("encode")) + cell(c["decode"], sp("decode"))
                 + "".join(cell(c[f"{ph}_mem_{m}"], f"{c[f'{ph}_mem_{m}_improvement_pct']:+.1f}%") for ph in ("encode", "decode") for m in ("avg", "peak")) + "</tr>")
    h.append("</table><h2>Per file</h2><table><tr><th>codec</th><th>input</th><th>bytes</th><th>ratio</th><th>enc med s [IQR]</th><th>dec med s [IQR]</th><th>enc avg/peak MiB</th><th>dec avg/peak MiB</th><th>thr e/d</th></tr>")
    for s_ in sorted(rep["stats"], key=lambda x: (x["input"], x["codec"])):
        mib = lambda v: f"{v / 2**20:.1f}" if v else "-"
        h.append(f"<tr><td>{s_['codec']}</td><td>{s_['input']}</td><td>{s_['comp_bytes']}</td><td>{(s_['ratio'] or 0):.4f}</td>"
                 f"<td>{s_['enc_wall']['median']:.4f} [{s_['enc_wall']['q1']:.4f}-{s_['enc_wall']['q3']:.4f}]</td><td>{s_['dec_wall']['median']:.4f} [{s_['dec_wall']['q1']:.4f}-{s_['dec_wall']['q3']:.4f}]</td>"
                 f"<td>{mib(s_['enc_rss_avg']['median'] if s_['enc_rss_avg'] else None)}/{mib(s_['enc_peak_kernel_max'])}</td><td>{mib(s_['dec_rss_avg']['median'] if s_['dec_rss_avg'] else None)}/{mib(s_['dec_peak_kernel_max'])}</td>"
                 f"<td>{s_['enc_threads_max']}/{s_['dec_threads_max']}</td></tr>")
    h.append("</table>")
    xopen(path).write("\n".join(h))

# ---------------------------------------------------------------------------------------------- self-test
FAKE = r'''
import sys, time, zlib, os
mode, op, src, dst = sys.argv[1:5]
data = sys.stdin.buffer.read() if src == "-" else open(src, "rb").read()
if mode == "fail": sys.exit(3)
if mode == "timeout": time.sleep(60)
if mode == "noisy": sys.stderr.write("x" * (20 << 20)); sys.stderr.flush()
if mode == "mem" and op == "e":   # explicit mmap/munmap: malloc'd memory stays resident after free on macOS
    import mmap
    m = mmap.mmap(-1, 200 << 20)
    for i in range(0, 200 << 20, 4096): m[i] = 1
    time.sleep(0.3); m.close(); time.sleep(1.2)
if op == "e": out = zlib.compress(data, 6)
else:
    out = zlib.decompress(data)
    if mode == "corrupt": out = out[:-1] + bytes([out[-1] ^ 1])
if dst == "-": sys.stdout.buffer.write(out)
else: open(dst, "wb").write(out)
'''

def cmd_self_test(a):
    out = new_dir(a.out); results = []; dar = Darwin()
    def check(name, cond, detail=""):
        results.append({"test": name, "ok": bool(cond), "detail": detail}); print(("PASS " if cond else "FAIL ") + name, detail)
    # pure arithmetic
    check("maxrss Darwin bytes", maxrss_bytes(1000, "Darwin") == 1000)
    check("maxrss Linux KiB", maxrss_bytes(1000, "Linux") == 1024000)
    try: maxrss_bytes(1, "Plan9"); check("maxrss unknown platform refused", False)
    except ValueError: check("maxrss unknown platform refused", True)
    avg, span, n = time_weighted_avg([(0, 100), (1, 100), (1.0001, 1000), (1.1, 1000), (1.1001, 100), (10, 100)])
    check("time-weighted average is not mean of samples or peak", abs(avg - 109.9) < 1.0 and avg < 1000, f"avg={avg:.2f}")
    # preservation
    sent = os.path.join(out, "existing"); os.mkdir(sent); open(os.path.join(sent, "SENTINEL"), "x").write("keep")
    try: new_dir(sent); check("existing --out refused", False)
    except SystemExit: check("existing --out refused", open(os.path.join(sent, "SENTINEL")).read() == "keep")
    fx = new_dir(os.path.join(out, "fixture")); src = os.path.join(fx, "in.txt")
    xopen(src, "wb").write((b"kolmo review harness self-test " * 40000))
    fake = os.path.join(fx, "fake.py"); xopen(fake).write(FAKE)
    def cfg_for(modes, seed=7, reps=2):
        h = sha256_file(src)
        return {"seed": seed, "repetitions": reps, "warmups": 1, "sample_ms": 2,
                "quiet": {"max_other_cores": 64.0, "pre_window_s": 0.05, "pre_wait_tries": 1, "max_retries": 1, "require_ac": False},
                "tools": {"py": {"path": sys.executable}},
                "codecs": [{"id": m, "tool": "py", "encode": ["-I", fake, m, "e", "{in}", "{out}"], "decode": ["-I", fake, m, "d", "{in}", "{out}"],
                            "timeout": 5, "thread_policy": "python-fake"} for m in modes if m != "stdio"]
                          + ([{"id": "py/std+io", "tool": "py", "encode": ["-I", fake, "ok", "e", "-", "-"], "decode": ["-I", fake, "ok", "d", "-", "-"],
                               "encode_stdin": True, "encode_stdout": True, "decode_stdin": True, "decode_stdout": True,
                               "timeout": 5, "thread_policy": "python-fake"}] if "stdio" in modes else []),
                "inputs": [{"id": "in", "path": src, "sha256": h, "bytes": os.path.getsize(src), "domain": "test"}],
                "suites": {"t": {"inputs": ["in"], "codecs": [m if m != "stdio" else "py/std+io" for m in modes]}}}
    cpath = os.path.join(fx, "cfg.json"); json.dump(cfg_for(["ok", "fail", "timeout", "noisy", "corrupt", "mem", "stdio"]), xopen(cpath))
    rdir = os.path.join(out, "run1")
    p = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), "run", "--config", cpath, "--suite", "t", "--new-run", rdir],
                       capture_output=True, text=True, timeout=900)
    check("run completes despite failing children", p.returncode == 0, p.stderr[-300:])
    rows = [json.loads(l) for l in open(os.path.join(rdir, "trials.jsonl"))] if os.path.exists(os.path.join(rdir, "trials.jsonl")) else []
    st = lambda c: {r["status"] for r in rows if r["codec"] == c}
    check("ok codec accepted", st("ok") >= {"accepted", "warmup"}, str(st("ok")))
    check("stdin/stdout codec round-trips without a shell", st("py/std+io") >= {"accepted", "warmup"} and any("py~std~io" in f for f in os.listdir(os.path.join(rdir, "artifacts"))), str(st("py/std+io")))
    check("failing encoder recorded as encode_failed", st("fail") == {"encode_failed"}, str(st("fail")))
    check("hung child timed out and recorded", st("timeout") == {"timeout"}, str(st("timeout")))
    check("20 MiB stderr does not deadlock; stderr kept", "accepted" in st("noisy") and any(
        os.path.getsize(os.path.join(rdir, "logs", fsname(r["trial_id"].replace(":", "__")) + ".enc.stderr")) >= 20 << 20 for r in rows if r["codec"] == "noisy"), str(st("noisy")))
    check("round-trip mismatch detected and decode kept", st("corrupt") == {"roundtrip_mismatch"} and any(
        os.path.exists(os.path.join(rdir, "work", fsname(r["trial_id"].replace(":", "__")), "back")) for r in rows if r["codec"] == "corrupt"), str(st("corrupt")))
    mem = [r for r in rows if r["codec"] == "mem" and r["status"] in ("accepted", "warmup")]
    if mem:
        e = mem[-1]["encode"]
        check("peak RSS >= 200 MiB and time-weighted average well below peak", e["peak_rss_kernel_bytes"] >= 200 << 20 and
              e["rss_avg_time_weighted_bytes"] < 0.8 * e["peak_rss_kernel_bytes"],
              f"peak={e['peak_rss_kernel_bytes'] >> 20}MiB avg={int(e['rss_avg_time_weighted_bytes']) >> 20}MiB sampled_max={e['rss_sampled_max_bytes'] >> 20}MiB cov={e['rss_coverage']:.2f}")
        check("decode peak independent of encode (fresh process)", mem[-1]["decode"]["peak_rss_kernel_bytes"] < 100 << 20,
              f"dec peak={mem[-1]['decode']['peak_rss_kernel_bytes'] >> 20}MiB")
    else: check("mem codec ran", False)
    check("every trial row has identity fields", all(r.get("tool_sha256") and r.get("input_sha256") and r.get("trial_id") for r in rows))
    n0 = len(rows)
    p = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), "run", "--config", cpath, "--suite", "t", "--new-run", rdir],
                       capture_output=True, text=True, timeout=60)
    check("second --new-run on same dir refused", p.returncode != 0 and "refusing" in (p.stdout + p.stderr))
    p = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), "run", "--config", cpath, "--suite", "t", "--new-run", rdir, "--resume"],
                       capture_output=True, text=True, timeout=900)
    rows2 = [json.loads(l) for l in open(os.path.join(rdir, "trials.jsonl"))]
    check("--resume with identical identity appends nothing for completed slots", p.returncode == 0 and len(rows2) == n0 and rows2[:n0] == rows, f"{n0}->{len(rows2)}")
    cpath2 = os.path.join(fx, "cfg2.json"); json.dump(cfg_for(["ok", "fail", "timeout", "noisy", "corrupt", "mem", "stdio"], seed=8), xopen(cpath2))
    p = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), "run", "--config", cpath2, "--suite", "t", "--new-run", rdir, "--resume"],
                       capture_output=True, text=True, timeout=60)
    check("--resume with a different config refused", p.returncode != 0 and "identity differs" in (p.stdout + p.stderr))
    p = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), "report", "--run", rdir, "--out", os.path.join(out, "report1"), "--compare", "noisy:ok"],
                       capture_output=True, text=True, timeout=120)
    check("report builds", p.returncode == 0 and os.path.exists(os.path.join(out, "report1", "scoreboard.html")), p.stderr[-300:])
    json.dump({"utc": now_utc(), "harness_sha256": sha256_file(os.path.abspath(__file__)), "results": results},
              xopen(os.path.join(out, "self-test.json")), indent=1)
    bad = sum(not r["ok"] for r in results); print(f"self-test: {len(results) - bad} passed, {bad} failed"); sys.exit(1 if bad else 0)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    x = sp.add_parser("self-test"); x.add_argument("--out", required=True); x.set_defaults(f=cmd_self_test)
    x = sp.add_parser("inventory"); x.add_argument("--config", required=True); x.add_argument("--out", required=True); x.set_defaults(f=cmd_inventory)
    x = sp.add_parser("preflight"); x.add_argument("--config", required=True); x.add_argument("--out", required=True); x.add_argument("--suite"); x.set_defaults(f=cmd_preflight)
    x = sp.add_parser("run"); x.add_argument("--config", required=True); x.add_argument("--suite", required=True)
    x.add_argument("--new-run", required=True); x.add_argument("--resume", action="store_true"); x.set_defaults(f=cmd_run)
    x = sp.add_parser("report"); x.add_argument("--run", required=True); x.add_argument("--out", required=True)
    x.add_argument("--compare", nargs="*", help="CANDIDATE:REFERENCE codec ids"); x.add_argument("--tolerance", type=float, default=0.02)
    x.set_defaults(f=cmd_report)
    a = ap.parse_args(); a.f(a)

if __name__ == "__main__":
    main()
