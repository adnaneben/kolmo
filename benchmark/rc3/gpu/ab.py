#!/usr/bin/env python3
"""Interleaved A/B timing of Kolmo with and without the GPU worker, on a shared machine.

usage: ab.py KOLMO REPS OUT.jsonl FILE MODE_ARGS... [-- FILE MODE_ARGS...]
  e.g. ab.py build/kolmo 3 out.jsonl corpus -m ratio -T 10 -B 8

For each configuration the runs alternate A (CPU only: KOLMO_GPU unset) and B (--gpu), REPS times each, so slow
phases of the shared machine hit both sides alike. Every run's wall time, user and system CPU time come from
os.wait4; the GPU's device utilisation is sampled from ioreg every 50 ms during the run (the whole GPU, including
other programs). Both outputs must be the same bytes; the first B output is decoded and compared with the input.
"""
import json, os, re, subprocess, sys, tempfile, threading, time

IOREG = re.compile(rb'"Device Utilization %"=(\d+)')


def gpu_sampler(stop, samples):
    while not stop.is_set():
        try:
            out = subprocess.run(["ioreg", "-r", "-d", "1", "-w", "0", "-c", "IOAccelerator"], capture_output=True, timeout=2).stdout
            m = IOREG.search(out)
            if m:
                samples.append(int(m.group(1)))
        except Exception:
            pass
        stop.wait(0.05)


def run(cmd, env):
    samples, stop = [], threading.Event()
    th = threading.Thread(target=gpu_sampler, args=(stop, samples)); th.start()
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    _, status, ru = os.wait4(p.pid, 0)
    wall = time.perf_counter() - t0
    err = p.stderr.read().decode(errors="replace"); p.stderr.close()
    stop.set(); th.join()
    if status:
        sys.exit(f"failed: {' '.join(cmd)}\n{err}")
    return {"wall": wall, "user": ru.ru_utime, "sys": ru.ru_stime, "maxrss_mb": ru.ru_maxrss / 1e6,
            "gpu_util_mean": sum(samples) / len(samples) if samples else None, "stderr": err.strip()}


def main():
    kolmo, reps, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    rest = sys.argv[4:]
    configs, cur = [], []
    for a in rest:
        if a == "--":
            configs.append(cur); cur = []
        else:
            cur.append(a)
    if cur:
        configs.append(cur)
    tmp = tempfile.mkdtemp(prefix="kolmo-ab-")
    with open(out, "a") as log:
        for cfg in configs:
            f, args = cfg[0], cfg[1:]
            n = os.path.getsize(f)
            rows = {"A": [], "B": []}
            outs = {}
            for r in range(reps):
                for side in ("A", "B") if r % 2 == 0 else ("B", "A"):
                    env = dict(os.environ); env.pop("KOLMO_GPU", None)
                    o = os.path.join(tmp, f"{side}.kz")
                    cmd = [kolmo] + args + (["--gpu"] if side == "B" else []) + [f, o]
                    if side == "B":
                        env["KOLMO_GPU_STATS"] = "1"
                    res = run(cmd, env)
                    res["size"] = os.path.getsize(o)
                    rows[side].append(res)
                    outs[side] = open(o, "rb").read()
            same = outs["A"] == outs["B"]
            back = os.path.join(tmp, "back")
            subprocess.run([kolmo, "-d", os.path.join(tmp, "B.kz"), back], check=True)
            rt = open(back, "rb").read() == open(f, "rb").read()
            os.unlink(back)
            best = {s: min(rows[s], key=lambda x: x["wall"]) for s in rows}
            med = {s: sorted(x["wall"] for x in rows[s])[len(rows[s]) // 2] for s in rows}
            rec = {"file": os.path.basename(f), "bytes": n, "args": " ".join(args), "reps": reps, "same_bytes": same,
                   "round_trip": rt, "size": best["A"]["size"],
                   "A_cpu_only": {"best_wall": best["A"]["wall"], "median_wall": med["A"], "runs": rows["A"]},
                   "B_gpu_worker": {"best_wall": best["B"]["wall"], "median_wall": med["B"], "runs": rows["B"]},
                   "speedup_best": best["A"]["wall"] / best["B"]["wall"], "speedup_median": med["A"] / med["B"]}
            log.write(json.dumps(rec) + "\n"); log.flush()
            print(f"{rec['file']:<24} {rec['args']:<22} size {rec['size']:>11,}  CPU only {med['A']:7.2f} s  "
                  f"with GPU worker {med['B']:7.2f} s  x{rec['speedup_median']:.3f} (best x{rec['speedup_best']:.3f})  "
                  f"same bytes {same}  round trip {rt}  | {best['B']['stderr'].splitlines()[-1] if best['B']['stderr'] else ''}",
                  flush=True)


if __name__ == "__main__":
    main()
