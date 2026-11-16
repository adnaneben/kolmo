"""Interleaved whole-process timing of several kolmo builds (benchmark/rc2/SIMD-ASM.md, step 1 and 3).

usage: python3 -I run_simd.py OUT.jsonl ROUNDS DATA_DIR WORK_DIR name=path/to/kolmo [name=path ...]
For every round, input, mode and thread count, each build compresses then decompresses the input; the order of
the builds rotates from round to round, so a slow spell of the shared VM falls on every build alike. Each run is
timed with perf_counter around fork/exec/wait4 (wall) and wait4's rusage (user, sys); steal is read from /proc/stat
before and after the run and given as a share of the run's wall time on all CPUs. Round 1 records SHA-256 of every
frame and checks that decompression gives back the input. The output file is deleted before each run (outside the
timing) so no run pays for freeing the previous one's pages.
"""
import hashlib, json, os, subprocess, sys, time

out, rounds, data, work = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
builds = [a.split("=", 1) for a in sys.argv[5:]]
INPUTS = os.environ.get("SIMD_INPUTS", "silesia.tar,trades.csv,events.ndjson,walk_i32.npy").split(",")
MODES = os.environ.get("SIMD_MODES", "speed,balanced,hc,ratio").split(",")
THREADS = [int(t) for t in os.environ.get("SIMD_THREADS", f"1,{os.cpu_count()}").split(",")]
HZ = os.sysconf("SC_CLK_TCK"); NCPU = os.cpu_count()

def steal():
    f = open("/proc/stat").readline().split(); return int(f[8])

def run(cmd):
    for p in (cmd[-1],):
        try: os.unlink(p)
        except FileNotFoundError: pass
    s0 = steal(); t0 = time.perf_counter()
    pid = os.posix_spawn(cmd[0], cmd, os.environ)
    _, status, ru = os.wait4(pid, 0)
    wall = time.perf_counter() - t0; s1 = steal()
    if status != 0: raise SystemExit(f"failed ({status}): {' '.join(cmd)}")
    return {"wall": wall, "user": ru.ru_utime, "sys": ru.ru_stime, "steal": (s1 - s0) / HZ / (wall * NCPU), "maxrss_kb": ru.ru_maxrss}

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

ref = {i: sha(os.path.join(data, i)) for i in INPUTS}
fo = open(out, "a")
for r in range(rounds):
    for inp in INPUTS:
        src = os.path.join(data, inp)
        for mode in MODES:
            for th in THREADS:
                k = r % len(builds); order = builds[k:] + builds[:k]
                for name, exe in order:
                    frame = os.path.join(work, f"{name}.k"); back = os.path.join(work, f"{name}.out")
                    c = run([exe, "-m", mode, f"-T{th}", src, frame])
                    d = run([exe, "-d", f"-T{th}", frame, back])
                    rec = {"round": r, "build": name, "input": inp, "mode": mode, "threads": th, "size": os.path.getsize(frame),
                           "c": c, "d": d}
                    if r == 0:
                        rec["frame_sha256"] = sha(frame); rec["roundtrip"] = sha(back) == ref[inp]
                        if not rec["roundtrip"]: raise SystemExit(f"round trip failed: {name} {inp} {mode} T{th}")
                    fo.write(json.dumps(rec) + "\n"); fo.flush()
                    os.unlink(frame); os.unlink(back)
