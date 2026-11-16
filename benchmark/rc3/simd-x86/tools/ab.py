"""ab.py ROUNDS WORK case... -- name=binary ...
case = file:mode-args[:threads] (e.g. data/img8_rgb.npy:-m balanced:4; default 1 thread). Binaries name=path, or
name=path@VAR=value to set an environment variable. With AB_JSON=file, every case is also appended there as JSON. Compresses with the first binary, checks every binary writes
the same frame, then times compression and decompression of every binary, builds interleaved, ROUNDS rounds.
Prints the fastest and median per binary and the ratio to the first binary. Wall time of whole processes (wait4)."""
import os, sys, time, hashlib, subprocess, statistics
R = int(sys.argv[1]); W = sys.argv[2]; i = sys.argv.index("--")
cases = sys.argv[3:i]; bins = []
for a in sys.argv[i + 1:]:
    n, rest = a.split("=", 1); env = dict(os.environ)
    if "@" in rest: rest, e = rest.split("@", 1); k, v = e.split("=", 1); env[k] = v
    bins.append((n, rest, env))
import json
os.makedirs(W, exist_ok=True)
def run(argv, env=None):
    t = time.perf_counter(); pid = os.posix_spawn(argv[0], argv, env or os.environ); _, st, _ = os.wait4(pid, 0); d = time.perf_counter() - t
    if st: sys.exit(f"failed: {argv}")
    return d
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]
for case in cases:
    parts = case.split(":"); f, mode = parts[0], parts[1]; T = "-T" + (parts[2] if len(parts) > 2 else "1"); margs = mode.split()
    ref = os.path.join(W, "ref.kol"); run([bins[0][1], *margs, T, f, ref], bins[0][2]); h0 = sha(ref); size = os.path.getsize(ref)
    for n, b, env in bins[1:]:
        o = os.path.join(W, "o.kol"); run([b, *margs, T, f, o], env)
        if sha(o) != h0: print(f"  FRAME DIFFERS: {n} {case}")
    res = {n: {"c": [], "d": []} for n, _, _ in bins}
    for r in range(R):
        for n, b, env in (bins if r % 2 == 0 else bins[::-1]):
            o = os.path.join(W, "t.kol"); u = os.path.join(W, "t.out")
            for p in (o, u):
                if os.path.exists(p): os.unlink(p)
            res[n]["c"].append(run([b, *margs, T, f, o], env))
            res[n]["d"].append(run([b, "-d", T, ref, u], env))
    nb = os.path.getsize(f); base = bins[0][0]
    for d in ("c", "d"):
        b0 = min(res[base][d]); m0 = statistics.median(res[base][d])
        cells = "  ".join(f"{n} {min(res[n][d])*1e3:7.1f}/{statistics.median(res[n][d])*1e3:7.1f}ms x{b0/min(res[n][d]):.3f}/{m0/statistics.median(res[n][d]):.3f}" for n, _, _ in bins)
        print(f"{os.path.basename(f):18s} {mode:14s} {T} {d} {nb/1e6:6.1f}MB z={size} h={h0}  {cells}", flush=True)
        if os.environ.get("AB_JSON"):
            with open(os.environ["AB_JSON"], "a") as j:
                j.write(json.dumps({"file": os.path.basename(f), "bytes": nb, "mode": mode, "threads": int(T[2:]), "dir": d, "frame": h0, "size": size,
                                    "times_s": {n: res[n][d] for n, _, _ in bins}}) + "\n")
