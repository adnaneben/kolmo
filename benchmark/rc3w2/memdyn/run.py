"""memdyn lane benchmark: size, wall time and peak resident memory of one kolmo setting, encode and decode, each a
separate process (wait4: ru_maxrss), the decoded file compared with the input byte for byte.

usage: python3 -I benchmark/rc3w2/memdyn/run.py KOLMO OUT.csv --label L --args "-x" --files F1,F2 [--threads 1,4]
       [--budget 64M] [--memlimit 512M] [--env K=V]
--budget sets KOLMO_MEM_FORCE_BUDGET (the governor's budget, as if that much memory were available); --memlimit passes
-M (an explicit cap, which may change the frame). Appends one row per (file, threads), with the frame's SHA-256 so that
rows can be compared for identical output.
"""
import argparse, csv, filecmp, hashlib, os, shlex, subprocess, sys, tempfile, time

ap = argparse.ArgumentParser()
ap.add_argument("kolmo"); ap.add_argument("out")
ap.add_argument("--label", default=""); ap.add_argument("--args", default="")
ap.add_argument("--files", required=True); ap.add_argument("--threads", default="4")
ap.add_argument("--budget", default=""); ap.add_argument("--memlimit", default="")
ap.add_argument("--nodec", action="store_true")
a = ap.parse_args()
env = dict(os.environ)
if a.budget: env["KOLMO_MEM_FORCE_BUDGET"] = a.budget
else: env.pop("KOLMO_MEM_FORCE_BUDGET", None)
env.pop("KOLMO_MEMLIMIT", None)


def run(cmd):
    t0 = time.perf_counter(); p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env)
    _, st, ru = os.wait4(p.pid, 0); dt = time.perf_counter() - t0
    err = p.stderr.read().decode(errors="replace"); p.stderr.close()
    if st != 0: sys.exit(f"failed ({st}): {' '.join(cmd)}\n{err}")
    return dt, ru.ru_maxrss / 1024.0


new = not os.path.exists(a.out)
with open(a.out, "a", newline="") as f:
    w = csv.writer(f)
    if new: w.writerow(["label", "file", "bytes", "threads", "budget", "memlimit", "size", "declared_MiB", "enc_s", "enc_MiB", "dec_s", "dec_MiB", "sha256"])
    tmp = tempfile.mkdtemp(prefix="md")
    mem = ["-M", a.memlimit] if a.memlimit else []
    for src in a.files.split(","):
        n = os.path.getsize(src)
        for t in a.threads.split(","):
            z = os.path.join(tmp, "z"); d = os.path.join(tmp, "d")
            for p_ in (z, d):
                if os.path.exists(p_): os.remove(p_)
            et, em = run([a.kolmo, *shlex.split(a.args), *mem, "-T", t, src, z])
            frame = open(z, "rb").read(); h = hashlib.sha256(frame).hexdigest()[:16]
            decl = 0.0
            if frame[:4] == b"CZN1" and frame[11]:
                v = frame[11]; steps = [1048576, 1143481, 1246975, 1359835, 1482911, 1617126, 1763488, 1923097]
                decl = (steps[v & 7] << (v >> 3)) / 1048576.0
            dt = dm = float("nan")
            if not a.nodec:
                dt, dm = run([a.kolmo, "-d", *mem, "-T", t, z, d])
                if not filecmp.cmp(src, d, shallow=False): sys.exit(f"MISMATCH {src} T{t}")
            w.writerow([a.label, os.path.basename(src), n, t, a.budget or "auto", a.memlimit or "auto", len(frame), f"{decl:.0f}",
                        f"{et:.3f}", f"{em:.0f}", f"{dt:.3f}", f"{dm:.0f}", h])
            f.flush()
            print(a.label, os.path.basename(src), t, a.budget, len(frame), f"{et:.2f}s {em:.0f}MiB / {dt:.2f}s {dm:.0f}MiB", h, flush=True)
