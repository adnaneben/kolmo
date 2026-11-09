#!/usr/bin/env python3
"""Check payload stability and reciprocal decode against a saved pre-change dylib."""
import ctypes as C
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'bindings/python'))
from kolmo import _Params

def library(path):
    x = C.CDLL(str(path))
    x.czn_bound.argtypes = [C.c_size_t, C.POINTER(_Params)]; x.czn_bound.restype = C.c_size_t
    x.czn_compress.argtypes = [C.c_char_p, C.c_size_t, C.c_char_p, C.c_size_t, C.POINTER(_Params)]; x.czn_compress.restype = C.c_int64
    x.czn_decompress.argtypes = [C.c_char_p, C.c_size_t, C.c_char_p, C.c_size_t, C.c_int]; x.czn_decompress.restype = C.c_int64
    return x
old = library(root / 'build/baseline/libkolmo.dylib')
new = library(root / 'build/libkolmo.dylib')
assert C.cast(old.czn_compress, C.c_void_p).value != C.cast(new.czn_compress, C.c_void_p).value
files = sorted((root / 'benchmark/audit/data/silesia').iterdir()) + [root / 'benchmark/audit/data/owid-co2-data.csv']
count = 0
for file in files:
    data = file.read_bytes()
    for mode in [0, 1, 3]:
        p = _Params(mode, 0, 1, 0, 1)
        blobs = []
        for lib in [old, new]:
            buf = C.create_string_buffer(lib.czn_bound(len(data), C.byref(p)))
            n = lib.czn_compress(data, len(data), buf, len(buf), C.byref(p)); assert n > 0
            blobs.append(buf.raw[:n])
        # Header version/checksum may become more precise; payload and index must remain identical.
        assert blobs[0][32:] == blobs[1][32:], (file.name, mode, 'payload changed')
        for lib, blob in [(new, blobs[0]), (old, blobs[1])]:
            out = C.create_string_buffer(len(data))
            assert lib.czn_decompress(blob, len(blob), out, len(out), 1) == len(data)
            assert out.raw == data
        count += 1
        print(f'{file.name} mode={mode}: identical payload; old/new decode passed; versions {blobs[0][4]}/{blobs[1][4]}', flush=True)
print(f'COMPATIBILITY PASSED ({count} old/new frame pairs)', flush=True)
