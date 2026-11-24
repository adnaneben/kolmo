#!/usr/bin/env python3
"""Kolmo against numcodecs' Zstd, Blosc and LZ4 on generated scientific arrays, one thread, one Zarr-sized chunk.

    python bench.py               # table in Markdown on stdout
    python bench.py --repeat 5 --mib 8

Method: every codec runs on one thread (Kolmo threads=1, Blosc use_threads=False and nthreads 1, Zstd and LZ4 are
single-threaded in numcodecs). Each array is encoded and decoded whole, as one Zarr chunk; the time is the best of
--repeat runs (wall clock, perf_counter), and every decode is checked against the input. Speeds are MB/s of
uncompressed data (1 MB = 10^6 bytes). The arrays are generated with a fixed seed, so the run needs no download.
"""
import argparse
import platform
import time

import numpy as np
import numcodecs
from numcodecs import LZ4, Blosc, Zstd

from numcodecs_kolmo import Kolmo


def datasets(mib, seed=0):
    rng = np.random.default_rng(seed)
    n4 = mib * 2**20 // 4

    # 1. temperature on a lat x lon grid over time: smooth field + small sensor noise, float32 rounded to 0.01 K
    t, y, x = 16, 256, n4 // (16 * 256)
    tt, yy, xx = np.meshgrid(np.arange(t), np.linspace(-1.5, 1.5, y), np.linspace(0, 6.28, x), indexing="ij")
    temp = 288 - 30 * yy**2 + 5 * np.sin(xx + tt / 8) + rng.normal(scale=0.3, size=tt.shape)
    yield "temperature grid, f4 (0.01 K steps)", np.round(temp, 2).astype("<f4")

    # 2. the same field at full float32 precision (no rounding): the hard case for every lossless codec
    yield "temperature grid, f4 (full precision)", temp.astype("<f4")

    # 3. accelerometer: int16 ADC samples, 3 axes interleaved, low-pass signal + noise
    n = mib * 2**20 // 6
    s = np.cumsum(rng.normal(size=(n, 3)), axis=0)
    s -= np.convolve(s[:, 0], np.ones(512) / 512, mode="same")[:, None]
    yield "accelerometer, i2 x3 axes", np.clip(s * 40, -32768, 32767).astype("<i2")

    # 4. particle counts per detector bin: Poisson around a spectrum, uint16
    n = mib * 2**20 // 2
    lam = 50 * np.exp(-np.linspace(0, 8, n) % 4) + 2
    yield "detector counts, u2 (Poisson)", rng.poisson(lam).astype("<u2")

    # 5. ocean model: float64 random walk with physical drift (salinity-like)
    n = mib * 2**20 // 8
    yield "salinity series, f8 (random walk)", 35 + np.cumsum(rng.normal(scale=1e-4, size=n))

    # 6. land-cover / mask classes: uint8 with large uniform patches
    side = int(np.sqrt(mib * 2**20))
    coarse = rng.integers(0, 12, size=(side // 32 + 1, side // 32 + 1), dtype="u1")
    mask = np.ascontiguousarray(np.kron(coarse, np.ones((32, 32), "u1"))[:side, :side])
    flips = rng.random(mask.shape) < 0.02
    mask[flips] = rng.integers(0, 12, flips.sum())
    yield "land-cover classes, u1", mask

    # 7. timestamps of events: int64 nanoseconds, irregular spacing
    n = mib * 2**20 // 8
    yield "event timestamps, i8 ns", (1_700_000_000_000_000_000 + np.cumsum(rng.exponential(1e6, n))).astype("<i8")


def codecs():
    numcodecs.blosc.use_threads = False
    numcodecs.blosc.set_nthreads(1)
    return [
        ("kolmo 3", Kolmo(level=3, threads=1)),
        ("kolmo 5", Kolmo(level=5, threads=1)),
        ("kolmo 9", Kolmo(level=9, threads=1)),
        ("zstd 3", Zstd(level=3)),
        ("zstd 19", Zstd(level=19)),
        ("blosc lz4 5 shuffle", Blosc(cname="lz4", clevel=5, shuffle=Blosc.SHUFFLE)),
        ("blosc zstd 5 shuffle", Blosc(cname="zstd", clevel=5, shuffle=Blosc.SHUFFLE)),
        ("lz4", LZ4(acceleration=1)),
    ]


def best(fn, repeat):
    t = float("inf")
    for _ in range(repeat):
        t0 = time.perf_counter()
        r = fn()
        t = min(t, time.perf_counter() - t0)
    return t, r


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--mib", type=int, default=8, help="size of each array (one chunk), MiB")
    ap.add_argument("--repeat", type=int, default=3)
    args = ap.parse_args()

    import kolmo
    print("Machine: %s %s, Python %s, numcodecs %s, kolmoc from %s; arrays of %d MiB, best of %d, one thread.\n"
          % (platform.system(), platform.machine(), platform.python_version(), numcodecs.__version__,
             kolmo.__file__.rsplit("/", 2)[-2], args.mib, args.repeat))
    cs = codecs()
    for name, a in datasets(args.mib):
        print("**%s**, %s, %.1f MiB\n" % (name, a.shape, a.nbytes / 2**20))
        print("| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |")
        print("|---|---:|---:|---:|---:|")
        rows = []
        for cname, c in cs:
            te, enc = best(lambda: c.encode(a), args.repeat)
            td, dec = best(lambda: c.decode(enc), args.repeat)
            assert bytes(dec) == a.tobytes(), cname
            rows.append((cname, a.nbytes / len(enc), a.nbytes / te / 1e6, a.nbytes / td / 1e6))
        ref = dict((r[0], r[1]) for r in rows)["zstd 19"]
        for cname, ratio, es, ds in rows:
            print("| %s | %.2f | %.2fx | %.0f | %.0f |" % (cname, ratio, ratio / ref, es, ds))
        print()


if __name__ == "__main__":
    main()
