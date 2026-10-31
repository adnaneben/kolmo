"""kolmo — Python binding for the kolmo compressor (ctypes over libkolmo).

    import kolmo
    blob = kolmo.compress(open("data.csv","rb").read(), mode="hc")
    data = kolmo.decompress(blob)                 # whole file
    part = kolmo.read_range(blob, 1_000_000, 4096) # bytes [off, off+len) without decoding the rest

Build the library first:  make lib   (produces build/libkolmo.so / .dylib).
"""
import ctypes, os, sys

_MODES = {"rt": 0, "fast": 1, "eco": 2, "hc": 3, "max": 4, "ultra": 5}

def _load():
    here = os.path.dirname(os.path.abspath(__file__))
    name = "libkolmo.dylib" if sys.platform == "darwin" else "libkolmo.so"
    for p in (os.environ.get("KOLMO_LIB"), os.path.join(here, "..", "..", "build", name), name):
        if not p:
            continue
        try:
            return ctypes.CDLL(p)
        except OSError:
            continue
    raise OSError("libkolmo not found; run `make lib` and/or set KOLMO_LIB to the shared library path")

_lib = _load()

class _Params(ctypes.Structure):
    _fields_ = [("mode", ctypes.c_int), ("level", ctypes.c_int), ("threads", ctypes.c_int),
                ("block", ctypes.c_uint32), ("transforms", ctypes.c_int)]

_lib.czn_bound.restype = ctypes.c_size_t
_lib.czn_bound.argtypes = [ctypes.c_size_t, ctypes.POINTER(_Params)]
_lib.czn_compress.restype = ctypes.c_int64
_lib.czn_compress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(_Params)]
_lib.czn_original_size.restype = ctypes.c_int64
_lib.czn_original_size.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_lib.czn_decompress.restype = ctypes.c_int64
_lib.czn_decompress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int]
_lib.czn_read_range.restype = ctypes.c_int64
_lib.czn_read_range.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint64, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_int]
_lib.czn_strerror.restype = ctypes.c_char_p
_lib.czn_strerror.argtypes = [ctypes.c_int64]

def _check(code):
    if code < 0:
        raise ValueError("kolmo: " + _lib.czn_strerror(code).decode())
    return code

def compress(data, mode="fast", threads=0, block=0, transforms=True, level=0):
    """Compress bytes; returns the compressed bytes. mode: rt|fast|eco|hc|max|ultra."""
    if mode not in _MODES:
        raise ValueError("mode must be one of " + ", ".join(_MODES))
    data = bytes(data)
    p = _Params(_MODES[mode], level, threads, block, 1 if transforms else 0)
    cap = _lib.czn_bound(len(data), ctypes.byref(p))
    out = ctypes.create_string_buffer(cap)
    n = _check(_lib.czn_compress(data, len(data), out, cap, ctypes.byref(p)))
    return out.raw[:n]

def original_size(blob):
    """Original byte length recorded in a compressed blob's header."""
    blob = bytes(blob)
    return _check(_lib.czn_original_size(blob, len(blob)))

def decompress(blob, threads=0):
    """Decompress a whole blob; returns the original bytes."""
    blob = bytes(blob)
    n = original_size(blob)
    out = ctypes.create_string_buffer(n if n else 1)
    got = _check(_lib.czn_decompress(blob, len(blob), out, n, threads))
    return out.raw[:got]

def read_range(blob, off, length, threads=0):
    """Return bytes [off, off+length) of the original, decoding only the blocks that overlap."""
    blob = bytes(blob)
    out = ctypes.create_string_buffer(length if length else 1)
    got = _check(_lib.czn_read_range(blob, len(blob), off, length, out, threads))
    return out.raw[:got]
