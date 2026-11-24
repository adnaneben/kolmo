"""kolmo — Python binding for the kolmo compressor (ctypes over libkolmo).

    import kolmo
    blob = kolmo.compress(open("data.csv","rb").read(), level=7)   # or level="compact"
    data = kolmo.decompress(blob)                 # whole file
    part = kolmo.read_range(blob, 1_000_000, 4096) # bytes [off, off+len) without decoding the rest

Build the library first:  make lib   (produces build/libkolmo.so / .dylib).
"""
import ctypes, os, sys

# Levels (docs/LEVELS.md): 1 (fastest) .. 9 (max), 3 (light) the default, "ultra" outside the scale. The library's
# table (kolmo_params_for_level) turns a level into settings, so the same level gives the same bytes as `kolmo -N`.
LEVEL_NAMES = {"fastest": 1, "light": 3, "balanced": 5, "compact": 7, "max": 9, "ultra": 10}
_ULTRA = 10

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

class _Limits(ctypes.Structure):
    _fields_ = [("max_output_bytes", ctypes.c_uint64), ("max_block_bytes", ctypes.c_uint32),
                ("max_blocks", ctypes.c_uint32), ("max_memory", ctypes.c_uint64)]   # mirrors kolmo_decode_limits

_lib.kolmo_mem_parse.restype = ctypes.c_int
_lib.kolmo_mem_parse.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint64)]
_lib.kolmo_bound.restype = ctypes.c_size_t
_lib.kolmo_bound.argtypes = [ctypes.c_size_t, ctypes.POINTER(_Params)]
_lib.kolmo_compress.restype = ctypes.c_int64
_lib.kolmo_compress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(_Params)]
_lib.kolmo_original_size.restype = ctypes.c_int64
_lib.kolmo_original_size.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_lib.kolmo_decompress.restype = ctypes.c_int64
_lib.kolmo_decompress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int]
_lib.kolmo_decompress_limited.restype = ctypes.c_int64
_lib.kolmo_decompress_limited.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int,
                                         ctypes.POINTER(_Limits)]
_lib.kolmo_read_range.restype = ctypes.c_int64
_lib.kolmo_read_range.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint64, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_int]
_lib.kolmo_strerror.restype = ctypes.c_char_p
_lib.kolmo_strerror.argtypes = [ctypes.c_int64]
_lib.kolmo_params_for_level.restype = ctypes.c_int
_lib.kolmo_params_for_level.argtypes = [ctypes.c_int, ctypes.POINTER(_Params)]
_lib.kolmo_check_frame.restype = ctypes.c_int
_lib.kolmo_check_frame.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(_Limits)]

def _check(code):
    if code < 0:
        raise ValueError("kolmo: " + _lib.kolmo_strerror(code).decode())
    return code

def _refuse_archive(blob):
    """A `kolmo archive` file (a DDP1 section in the KEXT trailer) is one frame holding the internal chunk store of
    many files, not one compressed file: decompress and read_range refuse it instead of returning the store."""
    if len(blob) < 24 or blob[-24:-20] != b"KEXT":
        return
    count = int.from_bytes(blob[-16:-12], "little"); total = int.from_bytes(blob[-12:-4], "little")
    p = len(blob) - 24 - total
    if total > len(blob) - 24:
        return
    for _ in range(count):
        if p + 16 > len(blob) - 24:
            return
        if blob[p:p + 4] == b"DDP1":
            raise ValueError("kolmo: a 'kolmo archive' file (many files and a chunk store), not one compressed file; "
                             "unpack it with 'kolmo extract FILE', list it with 'kolmo contents FILE'")
        p += 16 + int.from_bytes(blob[p + 4:p + 12], "little")

def original_size(blob):
    """Original byte length recorded in a compressed blob's header."""
    blob = bytes(blob)
    return _check(_lib.kolmo_original_size(blob, len(blob)))

def read_range(blob, off, length, threads=0):
    """Return bytes [off, off+length) of the original, decoding only the blocks that overlap."""
    blob = bytes(blob)
    _refuse_archive(blob)
    total = original_size(blob)
    _check(_lib.kolmo_check_frame(blob, len(blob), None))          # before allocating anything from the header
    if off < 0 or length < 0:
        raise ValueError("read_range: off and length must be >= 0")
    if off > total:
        raise ValueError("kolmo: " + _lib.kolmo_strerror(-1).decode() + " (offset past the end)")
    length = min(length, total - off)
    out = ctypes.create_string_buffer(length if length else 1)
    got = _check(_lib.kolmo_read_range(blob, len(blob), off, length, out, threads))
    return out.raw[:got]

def _level(level):
    """1..9, "ultra", or a name (fastest, light, balanced, compact, max) -> the library's level number."""
    if isinstance(level, str):
        if level not in LEVEL_NAMES:
            raise ValueError("level must be 1..9, 'ultra' or one of " + ", ".join(LEVEL_NAMES))
        return LEVEL_NAMES[level]
    if isinstance(level, bool) or not isinstance(level, int) or not 1 <= level <= 9:
        raise ValueError("level must be 1..9, 'ultra' or one of " + ", ".join(LEVEL_NAMES))
    return level

def _memlimit(memlimit):
    """None/0 = auto; an int = bytes; a string as the CLI's --memlimit (512M, 8G, 50%, auto, max)."""
    if memlimit is None:
        return 0
    if isinstance(memlimit, str):
        v = ctypes.c_uint64()
        if _lib.kolmo_mem_parse(memlimit.encode(), ctypes.byref(v)):
            raise ValueError("memlimit: a size (512M, 8G), a share of RAM (50%), auto or max")
        return v.value
    if isinstance(memlimit, bool) or not isinstance(memlimit, int) or memlimit < 0:
        raise ValueError("memlimit must be bytes (int) or a string such as '512M'")
    return memlimit

def compress(data, level=3, threads=0, memlimit=None, block=0, native_leaf=False, typed_huf=False):
    """Compress bytes; returns the compressed bytes, the same bytes as `kolmo -N` at the same level.
    level: 1 (fastest) .. 9 (max), 3 ("light") is the default; "ultra" is the extreme tier outside the scale (about as
    slow to decompress as to compress, ~1 GB+ of memory). Names: "fastest" (1), "light" (3), "balanced" (5),
    "compact" (7), "max" (9), "ultra".
    threads: 0 = all cores (never changes the output).
    block: expert, bytes per block (0 = the level's choice), as `kolmo --block-size`.
    memlimit: None (auto), bytes, or "512M" / "8G" / "50%" / "max" as `kolmo --memlimit`: an explicit limit also sizes
    the blocks of levels 8, 9 and ultra so that the output decodes within it.
    native_leaf: level 3 only; integer series may use Kolmo's native plane-context leaves when smaller (sensor/ECG/
    numeric columns, typically 5-12% smaller; such files decode ~1.5-2.6x slower).
    typed_huf: opt-in, level 3 with threads=1: a little-endian integer NPY array or a PCM16 WAV of at most 16 MiB is
    coded type first. Several times faster to compress, sometimes larger: compare sizes. Excludes native_leaf.
    Every blob is container version 12; blobs of earlier Kolmo versions are refused by decompress (KOLMO_ERR_LEGACY)."""
    lv = _ULTRA if level == "ultra" else _level(level)
    if native_leaf and typed_huf:
        raise ValueError("choose native_leaf or typed_huf, not both")
    data = bytes(data)
    p = _Params(0, 0, threads, block, 0, _memlimit(memlimit))
    _check(_lib.kolmo_params_for_level(lv, ctypes.byref(p)))
    if p.transforms:   # 16 = KOLMO_T_NATIVE_LEAF, 32 = KOLMO_T_TYPED_HUF
        p.transforms |= (16 if native_leaf else 0) | (32 if typed_huf else 0)
    cap = _lib.kolmo_bound(len(data), ctypes.byref(p))
    out = ctypes.create_string_buffer(cap)
    n = _check(_lib.kolmo_compress(data, len(data), out, cap, ctypes.byref(p)))
    return out.raw[:n]

def decompress(blob, threads=0, memlimit=None, max_output_size=None):
    """Decompress a whole blob; returns the original bytes. The frame (header, index checksum, block sizes against
    the claimed size, the memory it declares) is checked before the output is allocated, so a damaged or hostile
    blob fails with ValueError instead of a huge allocation. memlimit as in compress (None = auto). Blobs of earlier
    Kolmo versions raise ValueError (KOLMO_ERR_LEGACY).
    max_output_size: None (the default) accepts any size the frame validly claims; a valid frame can claim well over
    100,000 times its own size (long runs), so a caller decoding untrusted input should set a bound in bytes: a frame
    that claims more raises ValueError (KOLMO_ERR_LIMIT) before anything is allocated."""
    blob = bytes(blob)
    _refuse_archive(blob)
    n = original_size(blob)
    if max_output_size is not None and (isinstance(max_output_size, bool) or not isinstance(max_output_size, int) or max_output_size < 0):
        raise ValueError("max_output_size must be a number of bytes")
    # max_output_bytes 0 means no limit in the library; a bound of 0 bytes is kept as 0 by refusing any non-empty claim
    if max_output_size == 0 and n:
        raise ValueError("kolmo: " + _lib.kolmo_strerror(-7).decode())
    lim = _Limits(max_output_size or 0, 0, 0, _memlimit(memlimit))
    _check(_lib.kolmo_check_frame(blob, len(blob), ctypes.byref(lim)))
    out = ctypes.create_string_buffer(n if n else 1)
    got = _check(_lib.kolmo_decompress_limited(blob, len(blob), out, n, threads, ctypes.byref(lim)))
    return out.raw[:got]
