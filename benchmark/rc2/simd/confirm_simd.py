"""Focused decode re-timing (fastest of N, builds interleaved): python3 -I confirm_simd.py N DATA WORK name=kolmo ...
The first build writes each frame (frames are byte-identical across builds, checked by run_simd.py)."""
import os, subprocess, sys, time
N, data, work = int(sys.argv[1]), sys.argv[2], sys.argv[3]; builds = [a.split("=", 1) for a in sys.argv[4:]]
CASES = [("trades.csv", "balanced", 1), ("trades.csv", "balanced", 4), ("trades.csv", "hc", 1), ("walk_i32.npy", "balanced", 1),
         ("walk_i32.npy", "speed", 1), ("silesia.tar", "speed", 1), ("silesia.tar", "hc", 4), ("events.ndjson", "balanced", 4)]
print("| input | mode | T | " + " | ".join(f"{b} ms" for b, _ in builds) + " | " + " | ".join(f"{b}/{builds[0][0]}" for b, _ in builds[1:]) + " |")
print("|---|---|---:|" + "---:|" * (2 * len(builds) - 1))
for inp, mode, th in CASES:
    fr = os.path.join(work, "c.k"); subprocess.run([builds[0][1], "-m", mode, f"-T{th}", os.path.join(data, inp), fr], check=True)
    best = {b: 9e9 for b, _ in builds}
    for r in range(N):
        for b, exe in builds[r % len(builds):] + builds[:r % len(builds)]:
            o = os.path.join(work, "c.out")
            if os.path.exists(o): os.unlink(o)
            t = time.perf_counter(); subprocess.run([exe, "-d", f"-T{th}", fr, o], check=True); best[b] = min(best[b], time.perf_counter() - t)
    b0 = best[builds[0][0]]
    print(f"| {inp} | {mode} | {th} | " + " | ".join(f"{1000*best[b]:.0f}" for b, _ in builds) + " | " + " | ".join(f"{best[b]/b0:.3f}" for b, _ in builds[1:]) + " |", flush=True)
