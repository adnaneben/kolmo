"""8-bit images (RGB PPM / gray PGM): Kolmo against the lossless image codecs, in one Python process.

    python3 run_img8.py --binding bindings/python --lib build/libkolmo.dylib --dir IMAGES --threads T --out rows.jsonl

Specialists through imagecodecs: JPEG XL lossless (effort 7 and 9), WebP lossless (method 6), PNG (zlib level 9),
JPEG-LS, JPEG 2000 reversible. They get the pixels; the PPM/PGM header is added to their size as stored bytes. Kolmo
gets the file and is timed through its binding (balanced, ratio, -x). Timing as run_spec2.py: repeat until 3 runs and
0.25 s, or 3 s; the fastest run counts. Every decoded output is compared with the input.
"""
import argparse
import json
import os
import sys
import time


def timed(fn):
    wall, best, n, r = 0.0, float("inf"), 0, None
    while True:
        t0 = time.perf_counter(); r = fn(); t = time.perf_counter() - t0
        wall, n, best = wall + t, n + 1, min(best, t)
        if (n >= 3 and wall >= 0.25) or wall >= 3.0:
            return r, best


def read_pnm(raw):
    parts, pos = [], 0
    while len(parts) < 4:
        while raw[pos:pos + 1].isspace(): pos += 1
        e = pos
        while not raw[e:e + 1].isspace(): e += 1
        parts.append(raw[pos:e]); pos = e
    pos += 1
    w, h, c = int(parts[1]), int(parts[2]), 3 if parts[0] == b"P6" else 1
    return pos, w, h, c


def main():
    ap = argparse.ArgumentParser()
    for k in ("binding", "lib", "dir", "out"):
        ap.add_argument("--" + k, required=True)
    ap.add_argument("--threads", type=int, default=1)
    a = ap.parse_args()
    T = a.threads
    os.environ["KOLMO_LIB"] = a.lib
    sys.path.insert(0, a.binding)
    import imagecodecs as ic
    import kolmo
    import numpy as np
    log = open(a.out, "a")
    done = set()
    for l in open(a.out):
        r = json.loads(l)
        if r.get("type") == "row": done.add((r["file"], r["label"]))

    def emit(name, label, n, z, tc, td, ok):
        row = {"type": "row", "group": "img8", "file": name, "label": label, "input": n, "compressed": z, "c_s": tc, "d_s": td,
               "c_mbs": n / tc / 1e6, "d_mbs": n / td / 1e6, "roundtrip": bool(ok), "threads_requested": T}
        log.write(json.dumps(row) + "\n"); log.flush()
        print(f"img8 {name:28s} {label:30s} {z:>10,}  c {row['c_mbs']:8.2f}  d {row['d_mbs']:8.2f}" + ("" if ok else "  ROUND TRIP FAILED"), flush=True)

    specs = [
        ("JPEG XL lossless e7", lambda x: ic.jpegxl_encode(x, lossless=True, effort=7, numthreads=T), ic.jpegxl_decode),
        ("JPEG XL lossless e9", lambda x: ic.jpegxl_encode(x, lossless=True, effort=9, numthreads=T), ic.jpegxl_decode),
        ("WebP lossless m6", lambda x: ic.webp_encode(x, lossless=True, level=100, method=6), ic.webp_decode),
        ("PNG level 9", lambda x: ic.png_encode(x, level=9), ic.png_decode),
        ("JPEG-LS", lambda x: ic.jpegls_encode(x), ic.jpegls_decode),
        ("JPEG 2000 reversible", lambda x: ic.jpeg2k_encode(x, level=0, reversible=True, numthreads=T), ic.jpeg2k_decode),
    ]
    for f in sorted(os.listdir(a.dir)):
        if not f.endswith((".ppm", ".pgm")): continue
        raw = open(os.path.join(a.dir, f), "rb").read()
        off, w, h, c = read_pnm(raw)
        img = np.frombuffer(raw, np.uint8, w * h * c, off).reshape((h, w, c) if c == 3 else (h, w))
        for label, enc, dec in specs:
            if (f, label) in done: continue
            if label.startswith("WebP") and c == 1: continue          # WebP has no gray mode
            try:
                z, tc = timed(lambda: enc(img))
                back, td = timed(lambda: dec(z))
                emit(f, label, len(raw), len(z) + off, tc, td, np.array_equal(np.asarray(back).reshape(img.shape), img))
            except Exception as e:
                print(f"img8 {f} {label}: {type(e).__name__}: {e}", flush=True)
        for label, kw in (("kolmo -m balanced", dict(mode="balanced")), ("kolmo -m ratio", dict(mode="ratio")), ("kolmo -x", dict(mode="ultra"))):
            if (f, label) in done: continue
            z, tc = timed(lambda: kolmo.compress(raw, threads=T, **kw))
            back, td = timed(lambda: kolmo.decompress(z, threads=T))
            emit(f, label, len(raw), len(z), tc, td, back == raw)


if __name__ == "__main__":
    main()
