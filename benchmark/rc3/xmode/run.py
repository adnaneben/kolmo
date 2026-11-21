"""xmode lane benchmark: size, wall time and peak resident memory of `kolmo -x` (encode and decode).

usage: python3 -I benchmark/rc3/xmode/run.py KOLMO DATA_DIR OUT.csv [--sizes 100k,1m,10m] [--kinds text,csv,bin,fq]
       [--threads 1,4] [--args "-x"] [--label NAME] [--nodec]
Each run is a separate process; time and peak RSS come from wait4 (ru_maxrss, KiB on Linux). The decoded file is
compared with the input byte for byte. Appends one row per (file, threads) to OUT.csv.
"""
import argparse, csv, os, resource, shlex, subprocess, sys, tempfile, time, filecmp

ap = argparse.ArgumentParser()
ap.add_argument("kolmo"); ap.add_argument("data"); ap.add_argument("out")
ap.add_argument("--sizes", default="100k,1m,10m"); ap.add_argument("--kinds", default="text,csv,bin,fq")
ap.add_argument("--threads", default="1,4"); ap.add_argument("--args", default="-x"); ap.add_argument("--label", default="")
ap.add_argument("--nodec", action="store_true")
a = ap.parse_args()

def run(cmd):
    t0 = time.perf_counter(); p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    _, st, ru = os.wait4(p.pid, 0); dt = time.perf_counter() - t0
    err = p.stderr.read().decode(errors="replace"); p.stderr.close()
    if st != 0: sys.exit(f"failed ({st}): {' '.join(cmd)}\n{err}")
    return dt, ru.ru_maxrss / 1024.0, err

new = not os.path.exists(a.out)
with open(a.out, "a", newline="") as f:
    w = csv.writer(f)
    if new: w.writerow(["label", "file", "bytes", "threads", "size", "ratio", "enc_s", "enc_MBps", "enc_MiB", "dec_s", "dec_MBps", "dec_MiB"])
    tmp = tempfile.mkdtemp(prefix="xm")
    for s in a.sizes.split(","):
        for k in a.kinds.split(","):
            src = os.path.join(a.data, f"{k}_{s}"); n = os.path.getsize(src)
            for t in a.threads.split(","):
                z = os.path.join(tmp, "z"); d = os.path.join(tmp, "d")
                for p_ in (z, d):
                    if os.path.exists(p_): os.remove(p_)
                et, em, _ = run([a.kolmo, *shlex.split(a.args), "-T", t, src, z]); zs = os.path.getsize(z)
                dt = dm = float("nan")
                if not a.nodec:
                    dt, dm, _ = run([a.kolmo, "-d", "-T", t, z, d])
                    if not filecmp.cmp(src, d, shallow=False): sys.exit(f"MISMATCH {src} T{t}")
                row = [a.label, f"{k}_{s}", n, t, zs, f"{n / zs:.3f}", f"{et:.3f}", f"{n / et / 1e6:.3f}", f"{em:.0f}",
                       f"{dt:.3f}", f"{n / dt / 1e6:.3f}" if dt == dt else "", f"{dm:.0f}" if dm == dm else ""]
                w.writerow(row); f.flush(); print(",".join(map(str, row)), flush=True)
