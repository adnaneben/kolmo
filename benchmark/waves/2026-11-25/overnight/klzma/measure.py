"""Measure both pinned libraries on identical inputs; write one CSV row per arm."""
import argparse
import csv
import ctypes as c
import hashlib
from pathlib import Path
import random
import sys
import time

def library(path):
    lib = c.CDLL(str(Path(path).resolve()))
    buf = c.c_void_p
    size = c.c_size_t
    lib.kolmo_klzma_compress_mt.argtypes = [buf, size, buf, size, c.c_int, c.c_int]
    lib.kolmo_klzma_compress_mt.restype = size
    lib.kolmo_klzma_decompress.argtypes = [buf, size, buf, size]
    lib.kolmo_klzma_decompress.restype = c.c_int
    return lib

def timed(f, *args):
    wall = time.perf_counter_ns()
    cpu = time.process_time_ns()
    result = f(*args)
    return result, time.perf_counter_ns() - wall, time.process_time_ns() - cpu

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline')
    parser.add_argument('candidate')
    parser.add_argument('inputs', nargs='+', type=Path)
    parser.add_argument('--reps', type=int, default=7)
    args = parser.parse_args()
    base, candidate = map(library, (args.baseline, args.candidate))
    libs = {'A': base, 'A2': base, 'B': candidate}
    rng = random.Random(70825)
    writer = csv.writer(sys.stdout)
    writer.writerow('file bytes input_sha256 level threads round arm stored frame_sha256 enc_ns dec_ns enc_cpu_ns dec_cpu_ns'.split())
    for path in args.inputs:
        for limit in (32768, 262144):
            with path.open('rb') as src:
                data = src.read(limit)
            if limit > 32768 and len(data) <= 32768:
                continue
            digest = hashlib.sha256(data).hexdigest()
            for level in (4, 8):
                for threads in (1, 4):
                    expected = None
                    for rep in range(args.reps + 1):
                        for arm in rng.sample(list(libs), 3):
                            lib = libs[arm]
                            dst = c.create_string_buffer(len(data) + len(data) // 64 + 1024)
                            recovered = c.create_string_buffer(len(data))
                            n, enc, enc_cpu = timed(lib.kolmo_klzma_compress_mt, data, len(data), dst, len(dst), level, threads)
                            if not n:
                                raise ValueError(f'codec declined {path} in arm {arm}')
                            frame = dst.raw[:n]
                            if expected is None:
                                expected = frame
                            if frame != expected:
                                raise ValueError(f'frame differs for {path} in arm {arm}')
                            rc, dec, dec_cpu = timed(base.kolmo_klzma_decompress, frame, n, recovered, len(data))
                            if rc or recovered.raw != data:
                                raise ValueError(f'base decoder mismatch for {path} in arm {arm}')
                            writer.writerow([path.name, len(data), digest, level, threads, rep, arm, n,
                                             hashlib.sha256(frame).hexdigest(), enc, dec, enc_cpu, dec_cpu])
                        sys.stdout.flush()
if __name__ == '__main__':
    main()
