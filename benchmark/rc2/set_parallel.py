"""Tier M2: many files at once. Each tool compresses (then decompresses) a whole set of files with W workers running
side by side, one file per worker at a time, as a user would run a tool with no internal threads on a folder.

usage: set_parallel.py --tools tools.json --group NAME --dir DIR --suffix .wav --workers 10 --work DIR --out rows.jsonl
tools.json as for run_cli2.py; a setting may instead be {"label", "py": "flac5" | "flac8"} for FLAC through
libsndfile (run in worker processes). Times: wall time for the whole set, fastest of 3 rounds; every decoded file is
compared with its input.
"""
import argparse
import concurrent.futures as cf
import hashlib
import io
import json
import os
import subprocess
import time


def sha(b):
    return hashlib.sha256(b).hexdigest()


def flac_job(args):
    import numpy as np
    import soundfile as sf
    kind, src, dst, level = args
    if kind == "c":
        raw = open(src, "rb").read()
        pcm, rate = sf.read(io.BytesIO(raw), dtype="int16")
        b = io.BytesIO(); sf.write(b, pcm, rate, format="FLAC", subtype="PCM_16", compression_level=level / 8.0)
        open(dst, "wb").write(b.getvalue())
        return sha(pcm.tobytes())
    pcm, _ = sf.read(dst, dtype="int16")
    return sha(np.ascontiguousarray(pcm).tobytes())


def cli_job(args):
    argv = args
    r = subprocess.run(argv, capture_output=True)
    return r.returncode


def main():
    ap = argparse.ArgumentParser()
    for k in ("tools", "group", "dir", "work", "out"):
        ap.add_argument("--" + k, required=True)
    ap.add_argument("--suffix", default="")
    ap.add_argument("--workers", type=int, default=os.cpu_count())
    a = ap.parse_args()
    settings = json.load(open(a.tools))[a.group]
    files = sorted(os.path.join(a.dir, f) for f in os.listdir(a.dir) if f.endswith(a.suffix))
    raw_total = sum(os.path.getsize(f) for f in files)
    os.makedirs(a.work, exist_ok=True)
    log = open(a.out, "a")
    for s in settings:
        best_c = best_d = None; size = 0; ok = True
        for rnd in range(3):
            outs = [os.path.join(a.work, f"{i}.z") for i in range(len(files))]
            backs = [os.path.join(a.work, f"{i}.back") for i in range(len(files))]
            if "py" in s:
                level = 5 if s["py"] == "flac5" else 8
                with cf.ProcessPoolExecutor(a.workers) as ex:
                    t0 = time.perf_counter(); want = list(ex.map(flac_job, [("c", f, o, level) for f, o in zip(files, outs)])); tc = time.perf_counter() - t0
                    t0 = time.perf_counter(); got = list(ex.map(flac_job, [("d", f, o, level) for f, o in zip(files, outs)])); td = time.perf_counter() - t0
                ok = ok and want == got
            else:
                sub = lambda argv, f, o, b: [x.format(**{"in": f, "out": o, "back": b}) for x in argv]
                with cf.ThreadPoolExecutor(a.workers) as ex:
                    t0 = time.perf_counter(); rc = list(ex.map(cli_job, [sub(s["c"], f, o, b) for f, o, b in zip(files, outs, backs)])); tc = time.perf_counter() - t0
                    t0 = time.perf_counter(); rd = list(ex.map(cli_job, [sub(s["d"], f, o, b) for f, o, b in zip(files, outs, backs)])); td = time.perf_counter() - t0
                ok = ok and not any(rc) and not any(rd) and all(open(f, "rb").read() == open(b, "rb").read() for f, b in zip(files, backs))
            size = sum(os.path.getsize(o) for o in outs)
            best_c = tc if best_c is None else min(best_c, tc); best_d = td if best_d is None else min(best_d, td)
        row = {"type": "row", "group": a.group, "file": f"{len(files)} files", "label": s["label"], "input": raw_total, "compressed": size,
               "workers": a.workers, "c_s": best_c, "d_s": best_d, "c_mbs": raw_total / best_c / 1e6, "d_mbs": raw_total / best_d / 1e6, "roundtrip": ok}
        log.write(json.dumps(row) + "\n"); log.flush()
        print(f"{s['label']:34s} {size:>12,}  c {row['c_mbs']:8.2f}  d {row['d_mbs']:8.2f} MB/s" + ("" if ok else "  ROUND TRIP FAILED"), flush=True)


if __name__ == "__main__":
    main()
