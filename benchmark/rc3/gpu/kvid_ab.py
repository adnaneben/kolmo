#!/usr/bin/env python3
"""kvid's prediction on 1 CPU thread, on all CPU threads and on the GPU: same bytes, and the time of each.

usage: kvid_ab.py KVID REPS OUT.jsonl MODE CLIP.y4m...

The three variants are run interleaved (1 thread, 10 threads, 10 threads + --gpu), REPS rounds of `kvid -b`, which
reports the fastest in-memory compress of at least 3 runs and the fastest transform alone. The Kolmo stage after the
transform always uses the -T threads given.
"""
import json, os, subprocess, sys, tempfile

kvid, reps, out, mode = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
variants = [("cpu1", ["-T", "1"]), ("cpu10", ["-T", "10"]), ("gpu+cpu10", ["-T", "10", "--gpu"])]
tmp = tempfile.mkdtemp(prefix="kvid-ab-")
with open(out, "a") as log:
    for clip in sys.argv[5:]:
        frames = {}
        for name, a in variants:
            o = os.path.join(tmp, name)
            subprocess.run([kvid, "-m", mode] + a + [clip, o], check=True)
            frames[name] = open(o, "rb").read()
        same = len(set(frames.values())) == 1
        best = {}
        for r in range(reps):
            order = variants if r % 2 == 0 else variants[::-1]
            for name, a in order:
                j = json.loads(subprocess.run([kvid, "-b", "-m", mode] + a + [clip], check=True, capture_output=True).stdout)
                b = best.get(name)
                if b is None or j["compress_mbs"] > b["compress_mbs"]:
                    if b is not None:
                        j["transform_ms"] = min(j["transform_ms"], b["transform_ms"])
                    best[name] = j
                else:
                    b["transform_ms"] = min(j["transform_ms"], b["transform_ms"])
        rec = {"clip": os.path.basename(clip), "mode": mode, "same_bytes": same, "variants": best}
        log.write(json.dumps(rec) + "\n")
        n = best["cpu1"]["input"]
        line = f"{rec['clip']:<18} {mode:<9} same bytes {same}"
        for name, _ in variants:
            v = best[name]
            line += f" | {name}: transform {v['transform_ms']:6.1f} ms, compress {n / v['compress_mbs'] / 1e3:7.1f} ms"
        print(line, flush=True)
