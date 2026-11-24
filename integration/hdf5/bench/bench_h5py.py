"""Single-thread benchmark through h5py: Kolmo (this plugin) against the filters of hdf5plugin (Zstd, Blosc, Blosc2,
Bitshuffle, LZ4) and h5py's gzip, all running inside h5py's own HDF5.

    pip install h5py hdf5plugin numpy
    make -C integration/hdf5 && ./integration/hdf5/build/bench_hdf5 -w DIR [inputs]   # raw inputs, as bench_hdf5 uses
    HDF5_PLUGIN_PATH=integration/hdf5/build/plugin BLOSC_NTHREADS=1 python bench_h5py.py [-r 5] DIR/*.f64 DIR/*.i16 ...

Each file is named NAME.TYPE (u8 i16 i32 i64 f32 f64) and written as a 1-D dataset with 1 MiB chunks to an in-memory
file (driver "core", no backing store) with the chunk cache off, so every chunk passes through the filter. Times are
the best of `runs` whole-dataset writes and reads; speeds are uncompressed MB/s (10^6 bytes). BLOSC_NTHREADS=1 keeps
Blosc and Blosc2 on one thread (hdf5plugin is built without OpenMP); the Kolmo filter uses one thread per chunk.
"""
import argparse
import os
import sys
import time

import h5py
import hdf5plugin
import numpy as np

KOLMO = 480  # PLACEHOLDER filter ID, see ../H5Zkolmo.h

DTYPES = {"u8": "<u1", "i16": "<i2", "i32": "<i4", "i64": "<i8", "f32": "<f4", "f64": "<f8"}


def configs():
    B, B2 = hdf5plugin.Blosc, hdf5plugin.Blosc2
    return [
        ("gzip 6", dict(compression="gzip", compression_opts=6)),
        ("shuffle + gzip 6", dict(compression="gzip", compression_opts=6, shuffle=True)),
        ("hdf5plugin Zstd 3", dict(**hdf5plugin.Zstd(clevel=3))),
        ("shuffle + Zstd 3", dict(**hdf5plugin.Zstd(clevel=3), shuffle=True)),
        ("shuffle + Zstd 19", dict(**hdf5plugin.Zstd(clevel=19), shuffle=True)),
        ("LZ4", dict(**hdf5plugin.LZ4())),
        ("Bitshuffle + LZ4", dict(**hdf5plugin.Bitshuffle(cname="lz4"))),
        ("Bitshuffle + Zstd 3", dict(**hdf5plugin.Bitshuffle(cname="zstd", clevel=3))),
        ("Blosc lz4 5 + shuffle", dict(**B(cname="lz4", clevel=5, shuffle=B.SHUFFLE))),
        ("Blosc zstd 5 + shuffle", dict(**B(cname="zstd", clevel=5, shuffle=B.SHUFFLE))),
        ("Blosc zstd 5 + bitshuffle", dict(**B(cname="zstd", clevel=5, shuffle=B.BITSHUFFLE))),
        ("Blosc2 zstd 5 + shuffle", dict(**B2(cname="zstd", clevel=5, filters=B2.SHUFFLE))),
        ("Blosc2 zstd 9 + shuffle", dict(**B2(cname="zstd", clevel=9, filters=B2.SHUFFLE))),
        ("kolmo 1", dict(compression=KOLMO, compression_opts=(1,))),
        ("kolmo 3", dict(compression=KOLMO, compression_opts=(3,))),
        ("kolmo 5", dict(compression=KOLMO, compression_opts=(5,))),
    ]


def run(a, kw, runs, chunk_bytes):
    chunk = max(1, min(a.size, chunk_bytes // a.itemsize))
    back = np.empty_like(a)
    best_w = best_r = float("inf")
    stored = 0
    for r in range(runs):
        with h5py.File(f"bench{r}.h5", "w", driver="core", backing_store=False, rdcc_nbytes=0) as f:
            d = f.create_dataset("d", shape=a.shape, dtype=a.dtype, chunks=(chunk,), **kw)
            t0 = time.perf_counter()
            d.write_direct(a)
            t1 = time.perf_counter()
            d.read_direct(back)
            t2 = time.perf_counter()
            if back.tobytes() != a.tobytes():   # bytes: float data may hold NaNs
                raise RuntimeError("round trip differs")
            stored = d.id.get_storage_size()
        best_w, best_r = min(best_w, t1 - t0), min(best_r, t2 - t1)
    return a.nbytes / stored, a.nbytes / best_w / 1e6, a.nbytes / best_r / 1e6


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-r", type=int, default=5, help="runs (best of)")
    ap.add_argument("-c", type=int, default=1 << 20, help="chunk bytes")
    ap.add_argument("files", nargs="+")
    o = ap.parse_args()
    if os.environ.get("BLOSC_NTHREADS") != "1":
        print("note: BLOSC_NTHREADS is not 1; Blosc filters may use several threads", file=sys.stderr)
    print(f"h5py {h5py.__version__}, HDF5 {h5py.version.hdf5_version}, hdf5plugin {hdf5plugin.version}; one thread, "
          f"{o.c}-byte chunks, best of {o.r}; MB/s of uncompressed data\n")
    for path in o.files:
        name, typ = os.path.basename(path).rsplit(".", 1)
        a = np.fromfile(path, dtype=DTYPES[typ])
        print(f"### {name} ({typ}, {a.nbytes / 1e6:.2f} MB)\n\n| filter | ratio | compress MB/s | decompress MB/s |\n|---|---:|---:|---:|")
        for label, kw in configs():
            try:
                ratio, w, r = run(a, kw, o.r, o.c)
                print(f"| {label} | {ratio:.2f} | {w:.0f} | {r:.0f} |", flush=True)
            except Exception as e:  # a filter that is not available or fails is reported, not fatal
                print(f"| {label} | (failed: {type(e).__name__}: {e}) | | |", flush=True)
        print()


if __name__ == "__main__":
    main()
