"""kolmo — Python binding for the kolmo compressor (ctypes over libkolmo).

    import kolmo
    blob = kolmo.compress(open("data.csv","rb").read(), mode="hc")
    data = kolmo.decompress(blob)                 # whole file
    part = kolmo.read_range(blob, 1_000_000, 4096) # bytes [off, off+len) without decoding the rest

Build the library first:  make lib   (produces build/libkolmo.so / .dylib).
"""
import ctypes, os, sys

_MODES = {"speed": 0, "balanced": 1, "ratio": 4,                 # the three modes
          "rt": 0, "fast": 1, "eco": 2, "hc": 3, "max": 4, "ultra": 5}   # advanced settings

def _load():
    here = os.path.dirname(os.path.abspath(__file__))
    name = "libkolmo.dylib" if sys.platform == "darwin" else "libkolmo.so"
    for p in (os.environ.get("KOLMO_LIB"), os.path.join(here, name), os.path.join(here, "..", "..", "build", name), name):
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
                ("block", ctypes.c_uint32), ("transforms", ctypes.c_int),
                ("mem_limit", ctypes.c_uint64)]   # 0 = auto (docs/MEMORY-MODEL.md); mirrors kolmo_params

_lib.kolmo_bound.restype = ctypes.c_size_t
_lib.kolmo_bound.argtypes = [ctypes.c_size_t, ctypes.POINTER(_Params)]
_lib.kolmo_compress.restype = ctypes.c_int64
_lib.kolmo_compress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(_Params)]
_lib.kolmo_original_size.restype = ctypes.c_int64
_lib.kolmo_original_size.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_lib.kolmo_decompress.restype = ctypes.c_int64
_lib.kolmo_decompress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int]
_lib.kolmo_read_range.restype = ctypes.c_int64
_lib.kolmo_read_range.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint64, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_int]
_lib.kolmo_strerror.restype = ctypes.c_char_p
_lib.kolmo_strerror.argtypes = [ctypes.c_int64]
_lib.kolmo_train_dict.restype = ctypes.c_int64
_lib.kolmo_train_dict.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
                                ctypes.c_char_p, ctypes.c_size_t]
_lib.kolmo_compress_dict.restype = ctypes.c_int64
_lib.kolmo_compress_dict.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t,
                                   ctypes.POINTER(_Params), ctypes.c_char_p, ctypes.c_size_t]
_lib.kolmo_decompress_dict.restype = ctypes.c_int64
_lib.kolmo_decompress_dict.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t,
                                     ctypes.c_int, ctypes.c_char_p, ctypes.c_size_t]

def _check(code):
    if code < 0:
        raise ValueError("kolmo: " + _lib.kolmo_strerror(code).decode())
    return code

def original_size(blob):
    """Original byte length recorded in a compressed blob's header."""
    blob = bytes(blob)
    return _check(_lib.kolmo_original_size(blob, len(blob)))

def read_range(blob, off, length, threads=0):
    """Return bytes [off, off+length) of the original, decoding only the blocks that overlap."""
    blob = bytes(blob)
    out = ctypes.create_string_buffer(length if length else 1)
    got = _check(_lib.kolmo_read_range(blob, len(blob), off, length, out, threads))
    return out.raw[:got]

def train_dict(samples, dict_cap=112 * 1024):
    """Dictionaries are not available in this build (they needed library codecs): raises ValueError with the
    library's KOLMO_ERR_UNSUPPORTED message. Kept so that existing callers fail clearly."""
    samples = [bytes(s) for s in samples]
    if not samples:
        raise ValueError("train_dict: need at least one sample")
    blob = b"".join(samples)
    sizes = (ctypes.c_size_t * len(samples))(*[len(s) for s in samples])
    out = ctypes.create_string_buffer(dict_cap)
    n = _check(_lib.kolmo_train_dict(blob, sizes, len(samples), out, dict_cap))
    return out.raw[:n]

def compress(data, mode="balanced", threads=0, block=0, transforms=True, level=0, dict=None, native_leaf=False,  # noqa: A002
             typed_huf=False):
    """Compress bytes; returns the compressed bytes. mode: speed|balanced|ratio (advanced: rt|fast|eco|hc|max|ultra).
    transforms: True (analyse and transform), False (codec only) or "quick" (cheap transforms only; fastest).
    native_leaf: with mode="balanced", integer series may use Kolmo's native plane-context leaves when smaller
    (sensor/ECG/numeric columns, typically 5-12% smaller; such files decode ~1.5-2.6x slower).
    level: 0 = mode default; with mode="ultra", 1 = model 1 only (faster) and 2 = maximum ratio (numeric graph
    leaves may also use context mixing: smaller integer series, much slower decoding of those blocks).
    typed_huf: opt-in, with mode="balanced" (or "fast"), threads=1 and the default level: a little-endian integer
    NPY array or a PCM16 WAV of at most 16 MiB is coded type first (integer prediction, Huffman planes). A speed
    option: several times faster to compress. On single-channel 16-bit signals (ECG, mono audio) the output is
    often smaller too; on other arrays and on multi-channel data (channels are not split) it is often larger than
    the default, so compare sizes before relying on it. typed_huf excludes native_leaf.
    dict: dictionaries are not available in this build; a non-empty dict raises ValueError (KOLMO_ERR_UNSUPPORTED).
    Every blob is container version 12; blobs of earlier Kolmo versions are refused by decompress (KOLMO_ERR_LEGACY)."""
    if mode not in _MODES:
        raise ValueError("mode must be one of " + ", ".join(_MODES))
    if native_leaf and typed_huf:
        raise ValueError("choose native_leaf or typed_huf, not both")
    data = bytes(data)
    tr = 2 if transforms == "quick" else 1 if transforms else 0
    # 16 = KOLMO_T_NATIVE_LEAF, 32 = KOLMO_T_TYPED_HUF
    p = _Params(_MODES[mode], level, threads, block,
                tr | (16 if native_leaf and tr else 0) | (32 if typed_huf and tr else 0))
    cap = _lib.kolmo_bound(len(data), ctypes.byref(p))
    out = ctypes.create_string_buffer(cap)
    if dict:
        d = bytes(dict)
        n = _check(_lib.kolmo_compress_dict(data, len(data), out, cap, ctypes.byref(p), d, len(d)))
    else:
        n = _check(_lib.kolmo_compress(data, len(data), out, cap, ctypes.byref(p)))
    return out.raw[:n]

def decompress(blob, threads=0, dict=None):  # noqa: A002
    """Decompress a whole blob; returns the original bytes. Blobs of earlier Kolmo versions (library codecs) raise
    ValueError (KOLMO_ERR_LEGACY). dict: not available in this build (a non-empty dict raises ValueError)."""
    blob = bytes(blob)
    n = original_size(blob)
    out = ctypes.create_string_buffer(n if n else 1)
    if dict:
        d = bytes(dict)
        got = _check(_lib.kolmo_decompress_dict(blob, len(blob), out, n, threads, d, len(d)))
    else:
        got = _check(_lib.kolmo_decompress(blob, len(blob), out, n, threads))
    return out.raw[:got]
