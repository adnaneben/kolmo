"""Tier S of the rc3 leaders run (rc2's run_spec2.py with the zipnn 0.5.x constructor): specialists called in one Python process beside Kolmo's Python binding, one thread each.

    python3 run_spec2.py --binding bindings/python --lib build/libkolmo.dylib --data DIR --video DIR --fastq DIR
                         --audio DIR --work DIR --out rows.jsonl [--only GROUP]

Groups and specialists (every call single-threaded; CPU time above wall time marks a row `threads`):
  img16    JPEG-LS (CharLS), JPEG 2000 reversible (OpenJPEG), JPEG XL lossless effort 7 and 9 (libjxl), via
           imagecodecs, on Silesia's mr (19 slices of 512 x 512, 10-bit) and x-ray (1900 x 2230, 12-bit). The
           files' headers (9,092 and 240 bytes) are added to the specialists' sizes as stored bytes. Kolmo gets
           the original files.
  video    FFV1 (FFmpeg, through PyAV): level 3, large context model, intra only, one thread. Its size is the sum
           of the packets plus the codec's extradata; the y4m text headers are not counted for FFV1.
  fastq    CRAM 3.1, archive profile (htslib through pysam), unmapped reads with the read-name comment kept in a
           CO tag, so that the FASTQ text is rebuilt byte for byte. Its times include Python's per-read work.
  weights  ZipNN (bfloat16 byte grouping + zstd / Huffman), threads 1, on the three safetensors files.
  audio    FLAC levels 5 and 8 (libsndfile) on the 40 ESC-50 clips, in memory.
Kolmo is timed through its binding on the same inputs: compress(raw, threads=1, ...) and decompress(threads=1).
Timing as rc1's tier S: each call repeats until 3 runs and 0.25 s, or 3 s, or 5,000 runs; the fastest run counts.
"""
import argparse
import hashlib
import io
import json
import os
import sys
import time

KOLMO = {
    "img16": [("kolmo -m balanced", dict(mode="balanced")), ("kolmo -m ratio", dict(mode="ratio")),
              ("kolmo -x", dict(mode="ultra")), ("kolmo -x -l 2", dict(mode="ultra", level=2))],
    "weights": [("kolmo -m balanced", dict(mode="balanced")), ("kolmo -m ratio", dict(mode="ratio"))],
    "audio": [("kolmo -m balanced", dict(mode="balanced")), ("kolmo -m balanced --typed-huf", dict(mode="balanced", typed_huf=True)),
              ("kolmo -m ratio", dict(mode="ratio")), ("kolmo -x -l 2", dict(mode="ultra", level=2))],
}
IMAGES = [("mr", 512, 9728, 9092), ("x-ray", 1900, 2230, 240)]
WEIGHTS = ["llm_bf16_10mb.safetensors", "llm_bf16_47mb.safetensors", "llm_nvfp4_17mb.safetensors"]


def timed(fn):
    wall = cpu = 0.0
    best, n, r = float("inf"), 0, None
    while True:
        c0, t0 = time.process_time(), time.perf_counter()
        r = fn()
        t, c = time.perf_counter() - t0, time.process_time() - c0
        wall, cpu, n, best = wall + t, cpu + c, n + 1, min(best, t)
        if (n >= 3 and wall >= 0.25) or wall >= 3.0 or n >= 5000:
            return r, best, wall, cpu, n


def pair(enc, dec, size_of=len):
    z, tc, wc, cc, nc = timed(enc)
    back, td, wd, cd, nd = timed(lambda: dec(z))
    return z, back, {"compressed": size_of(z), "c_s": tc, "d_s": td, "c_runs": nc, "d_runs": nd,
                     "threads": cc > wc * 1.05 + 0.002 or cd > wd * 1.05 + 0.002}


def main():
    ap = argparse.ArgumentParser()
    for k in ("binding", "lib", "data", "video", "fastq", "audio", "work", "out"):
        ap.add_argument("--" + k, required=True)
    ap.add_argument("--only")
    ap.add_argument("--threads", type=int, default=1, help="threads for every codec that has them (default 1)")
    a = ap.parse_args()
    T = a.threads
    os.environ["KOLMO_LIB"] = a.lib
    sys.path.insert(0, a.binding)
    import kolmo
    import numpy as np
    os.makedirs(a.work, exist_ok=True)
    log = open(a.out, "a")

    def emit(group, name, label, n, f, ok, extra=None):
        row = {"type": "row", "group": group, "file": name, "label": label, "input": n, **f, "roundtrip": bool(ok), "threads_requested": T}
        row["c_mbs"], row["d_mbs"] = n / f["c_s"] / 1e6, n / f["d_s"] / 1e6
        row.update(extra or {})
        log.write(json.dumps(row) + "\n")
        log.flush()
        print(f"{group:8s} {name:28s} {label:36s} {f['compressed']:>11,}  c {row['c_mbs']:8.2f}  d {row['d_mbs']:8.2f}"
              + ("" if ok else "  ROUND TRIP FAILED") + ("  THREADS" if f["threads"] else ""), flush=True)

    def kolmo_rows(group, name, raw):
        for label, kw in KOLMO.get(group, []):
            z, back, f = pair(lambda: kolmo.compress(raw, threads=T, **kw), lambda b: kolmo.decompress(b, threads=T))
            emit(group, name, label, len(raw), f, back == raw)

    log.write(json.dumps({"type": "env", "threads": T, "when": time.strftime("%Y-%m-%d %H:%M:%S"), "lib": a.lib,
                          "lib_sha256": hashlib.sha256(open(a.lib, "rb").read()).hexdigest(),
                          "load": os.getloadavg()}) + "\n")
    only = set(a.only.split(",")) if a.only else None

    if not only or "img16" in only:
        import imagecodecs as ic
        for name, W, R, h in IMAGES:
            raw = open(os.path.join(a.data, name), "rb").read()
            img = np.frombuffer(raw, dtype="<u2", offset=h).reshape(R, W).copy()
            head = raw[:h]
            codecs = [
                ("JPEG-LS (CharLS)", lambda: ic.jpegls_encode(img), ic.jpegls_decode),
                ("JPEG 2000 reversible (OpenJPEG)", lambda: ic.jpeg2k_encode(img, level=0, reversible=True, numthreads=T), lambda b: ic.jpeg2k_decode(b, numthreads=T)),
                ("JPEG XL lossless e7 (libjxl)", lambda: ic.jpegxl_encode(img, lossless=True, effort=7, numthreads=T), lambda b: ic.jpegxl_decode(b, numthreads=T)),
                ("JPEG XL lossless e9 (libjxl)", lambda: ic.jpegxl_encode(img, lossless=True, effort=9, numthreads=T), lambda b: ic.jpegxl_decode(b, numthreads=T)),
            ]
            for label, enc, dec in codecs:
                z, back, f = pair(enc, dec)
                f["compressed"] += len(head)
                emit("img16", name, label, len(raw), f, np.array_equal(np.asarray(back).reshape(R, W), img))
            kolmo_rows("img16", name, raw)

    if not only or "weights" in only:
        from zipnn import ZipNN
        for name in WEIGHTS:
            raw = open(os.path.join(a.data, name), "rb").read()
            # this zipnn release: byte input, 2 byte groups (bfloat16), zstd on one thread
            z = ZipNN(method="zstd", input_format="byte", bytearray_dtype="bfloat16", threads=T)   # zipnn 0.5.x API
            # zipnn 0.5.x rewrites its input buffer in place (byte grouping): give each call a private copy, and keep the
            # file's bytes for the comparison and for Kolmo (the copy is made outside the timed call)
            zz, back, f = pair(lambda: z.compress(bytearray(raw)), lambda b: z.decompress(b))
            emit("weights", name, f"ZipNN (2 byte groups, zstd, threads {T})", len(raw), f, bytes(back) == raw)
            kolmo_rows("weights", name, raw)

    if not only or "audio" in only:
        import soundfile as sf
        files = sorted(x for x in os.listdir(a.audio) if x.endswith(".wav"))
        for name in files:
            raw = open(os.path.join(a.audio, name), "rb").read()
            pcm, rate = sf.read(io.BytesIO(raw), dtype="int16")
            info = sf.info(io.BytesIO(raw))
            for lvl in (5, 8):
                def enc(lvl=lvl):
                    b = io.BytesIO()
                    sf.write(b, pcm, rate, format="FLAC", subtype="PCM_16", compression_level=lvl / 8.0)
                    return b.getvalue()
                z, back, f = pair(enc, lambda b: sf.read(io.BytesIO(b), dtype="int16")[0])
                emit("audio", name, f"FLAC level {lvl}", len(raw), f, np.array_equal(back, pcm), {"channels": info.channels})
            kolmo_rows("audio", name, raw)

    if not only or "video" in only:
        import av
        from fractions import Fraction
        for name in sorted(x for x in os.listdir(a.video) if x.endswith(".y4m")):
            raw = open(os.path.join(a.video, name), "rb").read()
            hdr_end = raw.index(b"\n") + 1
            hdr = raw[:hdr_end].decode()
            W = int(next(t[1:] for t in hdr.split() if t[0] == "W"))
            H = int(next(t[1:] for t in hdr.split() if t[0] == "H"))
            fs = W * H * 3 // 2
            frames, i = [], hdr_end
            while i < len(raw):
                j = raw.index(b"\n", i) + 1
                frames.append(np.frombuffer(raw, np.uint8, fs, j).reshape(H * 3 // 2, W))
                i = j + fs

            def enc():
                c = av.CodecContext.create("ffv1", "w")
                c.width, c.height, c.pix_fmt = W, H, "yuv420p"
                c.time_base = Fraction(1001, 30000)
                c.thread_count = T
                c.options = {"level": "3", "context": "1", "g": "1", "slicecrc": "0", **({"slices": "16"} if T > 1 else {})}
                pk = []
                for k, fr in enumerate(frames):
                    vf = av.VideoFrame.from_ndarray(fr, format="yuv420p")
                    vf.pts = k
                    pk += [bytes(p) for p in c.encode(vf)]
                pk += [bytes(p) for p in c.encode(None)]
                return (bytes(c.extradata or b""), pk)

            def dec(z):
                ext, pk = z
                d = av.CodecContext.create("ffv1", "r")
                d.width, d.height, d.pix_fmt, d.thread_count = W, H, "yuv420p", T
                if ext:
                    d.extradata = ext
                out = []
                for p in pk:
                    out += [f.to_ndarray() for f in d.decode(av.Packet(p))]
                out += [f.to_ndarray() for f in d.decode(None)]
                return out

            z, back, f = pair(enc, dec, size_of=lambda z: len(z[0]) + sum(map(len, z[1])))
            ok = len(back) == len(frames) and all(np.array_equal(x, y) for x, y in zip(back, frames))
            emit("video", name, "FFV1 level 3, context 1 (FFmpeg)" + (f", {T} threads, 16 slices" if T > 1 else ""), len(raw), f, ok)

    if not only or "fastq" in only:
        import pysam
        for name in sorted(x for x in os.listdir(a.fastq) if x.endswith(".fastq")):
            raw = open(os.path.join(a.fastq, name), "rb").read()
            lines = raw.split(b"\n")[:-1]
            recs = [(lines[k][1:], lines[k + 1], lines[k + 3]) for k in range(0, len(lines), 4)]
            assert all(lines[k + 2] == b"+" for k in range(0, len(lines), 4))
            path = os.path.join(a.work, "reads.cram")
            hdr = {"HD": {"VN": "1.6", "SO": "unknown"}}

            def enc():
                with pysam.AlignmentFile(path, "wc", header=hdr, format_options=[b"version=3.1", b"archive"], threads=T) as o:
                    for nm, seq, q in recs:
                        s = pysam.AlignedSegment(o.header)
                        qn, _, co = nm.decode().partition(" ")
                        s.query_name, s.flag = qn, 4
                        s.query_sequence = seq.decode()
                        s.query_qualities = pysam.qualitystring_to_array(q.decode())
                        if co:
                            s.set_tag("CO", co, "Z")
                        o.write(s)
                return os.path.getsize(path)

            def dec(_):
                out = []
                with pysam.AlignmentFile(path, "rc", check_sq=False, threads=T) as f:
                    for s in f.fetch(until_eof=True):
                        nm = s.query_name + (" " + s.get_tag("CO") if s.has_tag("CO") else "")
                        out.append(f"@{nm}\n{s.query_sequence}\n+\n{pysam.qualities_to_qualitystring(s.query_qualities)}\n")
                return "".join(out).encode()

            z, back, f = pair(enc, dec, size_of=lambda z: z)
            emit("fastq", name, "CRAM 3.1 archive (htslib via pysam)", len(raw), f, back == raw)
    log.close()


if __name__ == "__main__":
    main()
