#!/usr/bin/env python3
"""Tier S of benchmark/rc1/PROTOCOL.md, in-process part: kolmo against specialists that lzbench does not contain.

usage: run_inprocess.py --binding <dir with kolmo.py> --lib <libkolmo.dylib> --data <dir> --raw <dir>
                        --audio <dir with .wav files> --work <dir> --out <out.jsonl> [--only <regex on "file setting">]

Everything is called from this one Python process and must stay on one thread:
    kolmo      its Python binding (ctypes over the release library), threads = 1
    pcodec     arrays of numbers, levels 8 (the default) and 12 (the highest)
    FLAC       16-bit audio through libsndfile (python-soundfile), levels 5 (the default) and 8 (the highest)
    JPEG XL    lossless, effort 7 (the default) and 9, through pillow-jxl-plugin with its thread pool switched off
Each call is repeated until it has run three times and for 0.25 s in total (a call that takes longer than 3 s runs
once) and the fastest run is kept, as lzbench does. The wall clock and the process's CPU clock are read around every
run: CPU time above wall time means that more than one thread worked, and the row is marked `threads`.

kolmo is timed twice: through the public binding (which allocates the output and copies it, as the specialists'
bindings do) and as the bare library call into buffers allocated beforehand (`lib_c_s`, `lib_d_s`), so the binding's
own cost can be seen. pcodec and FLAC store samples only, so the bytes of a file's header are added to their sizes.
Every result is decoded and compared with the input. Rows already in the output file are skipped on a restart.
"""
import argparse
import ctypes
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import time

KOLMO = [  # label, keyword arguments of kolmo.compress
    ("kolmo -m balanced", dict(mode="balanced")),
    ("kolmo -m balanced --native-leaf", dict(mode="balanced", native_leaf=True)),
    ("kolmo -m balanced --typed-huf", dict(mode="balanced", typed_huf=True)),
    ("kolmo -m hc", dict(mode="hc")),
    ("kolmo -m ratio", dict(mode="ratio")),
    ("kolmo -x", dict(mode="ultra")),
    ("kolmo -x -l 2", dict(mode="ultra", level=2)),
]
ARRAYS = [  # file, directory key, element type ("npy": read the type from the file)
    ("year.i32", "data", "<i4"), ("population.i64", "data", "<i8"), ("log_timestamps_ms.i64", "data", "<i8"),
    ("mitdb-100-ch0.i16", "raw", "<i2"), ("mitdb-100.i16", "raw", "<i2"), ("mitdb-100-ch0.i32", "raw", "<i4"),
    ("mr", "data", "<u2"), ("x-ray", "data", "<u2"), ("lfw_subset.npy", "data", "npy"), ("owid_energy_f64.npy", "data", "npy"),
]
IMAGES = [("img_astronaut.u8", "RGB", (512, 512)), ("img_camera.u8", "L", (512, 512))]


def timed(fn):
    """-> (result, fastest wall time, total wall time, total CPU time, runs)"""
    wall = cpu = 0.0
    best, n, r = float("inf"), 0, None
    while True:
        c0, t0 = time.process_time(), time.perf_counter()
        r = fn()
        t, c = time.perf_counter() - t0, time.process_time() - c0
        wall, cpu, n, best = wall + t, cpu + c, n + 1, min(best, t)
        if (n >= 3 and wall >= 0.25) or wall >= 3.0 or n >= 5000:
            return r, best, wall, cpu, n


def pair(enc, dec):
    """Time enc() and dec(blob). -> (blob, decoded, fields for the row)"""
    z, tc, wc, cc, nc = timed(enc)
    back, td, wd, cd, nd = timed(lambda: dec(z))
    f = {"c_s": tc, "d_s": td, "c_runs": nc, "d_runs": nd, "c_wall": round(wc, 4), "c_cpu": round(cc, 4),
         "d_wall": round(wd, 4), "d_cpu": round(cd, 4),
         "threads": cc > wc * 1.05 + 0.002 or cd > wd * 1.05 + 0.002}
    return z, back, f


def watcher_running():
    return subprocess.run(["pgrep", "-f", "bench_mac3"], stdout=subprocess.DEVNULL).returncode == 0


def others():
    """Other processes that used at least half a core just now: [name, percent]. Recorded, not acted on."""
    out = []
    for line in subprocess.run(["ps", "-Ao", "pcpu=,pid=,comm="], stdout=subprocess.PIPE, text=True).stdout.splitlines():
        a = line.split(None, 2)
        try:
            pc, pid = float(a[0].replace(",", ".")), int(a[1])
        except (ValueError, IndexError):
            continue
        if pc >= 50 and pid != os.getpid() and len(a) == 3:
            out.append([os.path.basename(a[2].strip()), pc])
    return out


def main():
    ap = argparse.ArgumentParser()
    for k in ("binding", "lib", "data", "raw", "audio", "work", "out"):
        ap.add_argument("--" + k, required=True)
    ap.add_argument("--only")
    a = ap.parse_args()
    os.environ["KOLMO_LIB"] = a.lib
    sys.path.insert(0, a.binding)
    import kolmo
    import numpy as np
    import soundfile as sf
    from pcodec import ChunkConfig, standalone
    from PIL import Image
    from pillow_jxl import JpegXLImagePlugin
    JpegXLImagePlugin.DECODE_THREADS = 0   # 0 switches the plugin's thread pool off
    os.makedirs(a.work, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    lib = kolmo._lib

    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            r = json.loads(line)
            if r.get("type") == "row" and "error" not in r and not r.get("threads") and not r.get("busy"):
                done.add((r["file"], r["set"]))
    log = open(a.out, "a")

    def write(obj):
        log.write(json.dumps(obj) + "\n")
        log.flush()
        os.fsync(log.fileno())

    write({"type": "env", "when": time.strftime("%Y-%m-%d %H:%M:%S"), "python": sys.version.split()[0],
           "numpy": np.__version__, "soundfile": sf.__version__, "libsndfile": sf.__libsndfile_version__,
           "lib": a.lib, "lib_sha256": hashlib.sha256(open(a.lib, "rb").read()).hexdigest(), "load": os.getloadavg(),
           "watcher_running": watcher_running(), "rows_done_before": len(done)})

    def want(name, label):
        return (name, label) not in done and (not a.only or re.search(a.only, name + " " + label))

    def emit(group, name, n, label, size, f, extra=0, **more):
        f = dict(f)
        f.update(type="row", group=group, file=name, n=n, set=label, size=size, extra=extra, when=time.strftime("%H:%M:%S"), **more)
        write(f)
        if "error" in f:
            print("%-24s %-34s FAILED %s" % (name, label, f["error"]), flush=True)
        else:
            print("%-24s %-34s %9d  c %9.2f  d %9.2f MB/s%s%s" % (name, label, size, n / 1e6 / f["c_s"], n / 1e6 / f["d_s"],
                  " THREADS" if f["threads"] else "", " BUSY" if f.get("busy") else ""), flush=True)

    def kolmo_rows(group, name, raw):
        n = len(raw)
        for label, kw in KOLMO:
            if not want(name, label):
                continue
            busy, oth = watcher_running(), others()
            try:
                z, back, f = pair(lambda: kolmo.compress(raw, threads=1, **kw), lambda b: kolmo.decompress(b, threads=1))
                if back != raw:
                    raise ValueError("round trip differs")
                # the bare library call, with the same parameters the binding builds
                p = kolmo._Params(kolmo._MODES[kw["mode"]], kw.get("level", 0), 1, 0,
                                  1 | (16 if kw.get("native_leaf") else 0) | (32 if kw.get("typed_huf") else 0))
                cap = lib.kolmo_bound(n, ctypes.byref(p))
                out, dst = ctypes.create_string_buffer(cap), ctypes.create_string_buffer(n if n else 1)
                zn, tc, wc, cc, _ = timed(lambda: lib.kolmo_compress(raw, n, out, cap, ctypes.byref(p)))
                if zn != len(z) or out.raw[:zn] != z:
                    raise ValueError("the library call wrote other bytes than the binding")
                got, td, wd, cd, _ = timed(lambda: lib.kolmo_decompress(z, len(z), dst, n, 1))
                if got != n or dst.raw[:n] != raw:
                    raise ValueError("round trip of the library call differs")
                f.update(lib_c_s=tc, lib_d_s=td)
                f["threads"] = f["threads"] or cc > wc * 1.05 + 0.002 or cd > wd * 1.05 + 0.002
                emit(group, name, n, label, len(z), f, busy=busy or watcher_running(), others=oth)
            except Exception as e:   # noqa: BLE001  (a failed setting must not stop the run)
                emit(group, name, n, label, 0, {"error": "%s: %s" % (type(e).__name__, e)}, others=oth)

    def rival(group, name, n, label, enc, dec, check, extra=0):
        if not want(name, label):
            return
        busy, oth = watcher_running(), others()
        try:
            z, back, f = pair(enc, dec)
            if not check(back):
                raise ValueError("round trip differs")
            emit(group, name, n, label, len(z) + extra, f, extra=extra, busy=busy or watcher_running(), others=oth)
        except Exception as e:   # noqa: BLE001
            emit(group, name, n, label, 0, {"error": "%s: %s" % (type(e).__name__, e)}, others=oth)

    def pcodec_rows(group, name, n, arr, extra):
        for lvl in (8, 12):
            rival(group, name, n, "pcodec level %d" % lvl,
                  lambda: standalone.simple_compress(arr, ChunkConfig(compression_level=lvl)),
                  standalone.simple_decompress,
                  lambda back: back.dtype == arr.dtype and back.tobytes() == arr.tobytes(), extra)

    # 1. arrays of numbers
    dirs = {"data": a.data, "raw": a.raw}
    for name, key, dt in ARRAYS:
        raw = open(os.path.join(dirs[key], name), "rb").read()
        n = len(raw)
        if dt == "npy":
            x = np.load(io.BytesIO(raw), allow_pickle=False)
            body = x.tobytes(order="F" if x.flags["F_CONTIGUOUS"] and not x.flags["C_CONTIGUOUS"] else "C")
            extra = n - len(body)
            assert raw[extra:] == body, "the NPY body is not the array bytes"
            arr, group = np.frombuffer(body, dtype=x.dtype), "numbers (NumPy %s)" % x.dtype
        else:
            arr, extra, group = np.frombuffer(raw, dtype=dt), 0, "numbers (%s)" % dt
        kolmo_rows(group, name, raw)
        pcodec_rows(group, name, n, arr, extra)

    # 2. raw 8-bit images
    for name, mode, size in IMAGES:
        raw = open(os.path.join(a.data, name), "rb").read()
        im = Image.frombytes(mode, size, raw)
        assert im.tobytes() == raw
        group = "raw 8-bit image (%s %dx%d)" % (mode, size[0], size[1])
        kolmo_rows(group, name, raw)
        for effort in (7, 9):
            def enc():
                b = io.BytesIO()
                im.save(b, "JXL", lossless=True, effort=effort, num_threads=0)
                return b.getvalue()
            rival(group, name, len(raw), "JPEG XL lossless, effort %d" % effort, enc,
                  lambda z: Image.open(io.BytesIO(z)).convert(mode).tobytes(), lambda back: back == raw)

    # 3. audio: 16-bit PCM WAV files
    tmp = os.path.join(a.work, "clip.flac")
    for name in sorted(f for f in os.listdir(a.audio) if f.endswith(".wav")):
        path = os.path.join(a.audio, name)
        raw = open(path, "rb").read()
        n = len(raw)
        info = sf.info(path)
        assert info.subtype == "PCM_16", info.subtype
        x, sr = sf.read(path, dtype="int16", always_2d=False)
        body = x.tobytes()
        hdr = n - len(body)
        assert hdr >= 44 and raw[hdr:] == body, "the WAV body is not the samples as read"
        group = "audio (16-bit, %d channel, %d Hz)" % (info.channels, sr)
        kolmo_rows(group, name, raw)
        same = lambda back: back.shape == x.shape and back.tobytes() == body   # noqa: E731
        for lvl, cl in ((5, 0.625), (8, 1.0)):
            def enc_mem():
                b = io.BytesIO()
                sf.write(b, x, sr, format="FLAC", subtype="PCM_16", compression_level=cl)
                return b.getvalue()

            def enc_file():
                sf.write(tmp, x, sr, format="FLAC", subtype="PCM_16", compression_level=cl)
                return open(tmp, "rb").read()

            def dec_file(z):
                return sf.read(tmp, dtype="int16", always_2d=False)[0]
            rival(group, name, n, "FLAC level %d" % lvl, enc_mem,
                  lambda z: sf.read(io.BytesIO(z), dtype="int16", always_2d=False)[0], same, hdr)
            rival(group, name, n, "FLAC level %d, through a file" % lvl, enc_file, dec_file, same, hdr)
        pcodec_rows(group, name, n, np.ascontiguousarray(x).reshape(-1), hdr)

    write({"type": "end", "when": time.strftime("%Y-%m-%d %H:%M:%S"), "watcher_running": watcher_running(), "load": os.getloadavg()})
    print("finished", time.strftime("%H:%M:%S"), flush=True)


if __name__ == "__main__":
    main()
