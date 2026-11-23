"""The numcodecs codec `kolmo`: any buffer in, a Kolmo frame out."""
import numpy as np
from numcodecs.abc import Codec
from numcodecs.compat import ensure_bytes, ensure_contiguous_ndarray, ndarray_copy

import kolmo as _kolmo

# The scale of docs/LEVELS.md: 1 (fastest) .. 9 (max), 3 the default, "ultra" outside the scale.
LEVELS = tuple(range(1, 10)) + ("ultra",)


def check_level(level):
    """Return the level if it is 1..9 or "ultra", else raise ValueError. Names (fastest, light...) are not taken, so
    one level has one spelling in stored metadata."""
    if level == "ultra" or (isinstance(level, int) and not isinstance(level, bool) and 1 <= level <= 9):
        return level
    raise ValueError("kolmo level must be 1..9 or 'ultra', got %r" % (level,))


def check_threads(threads):
    if isinstance(threads, bool) or not isinstance(threads, int) or threads < 0:
        raise ValueError("kolmo threads must be an int >= 0 (0 = all cores), got %r" % (threads,))
    return threads


def as_contiguous(buf):
    """A C-contiguous view of `buf` (copied only when it is a non-contiguous array), flattened to one dimension."""
    if isinstance(buf, np.ndarray) and not (buf.flags.c_contiguous or buf.flags.f_contiguous):
        buf = np.ascontiguousarray(buf)
    return ensure_contiguous_ndarray(buf)


class Kolmo(Codec):
    """Kolmo, a structure-aware lossless compressor (https://github.com/adnaneben/kolmo).

    level: 1 (fastest) .. 9 (max), 3 the default, or "ultra" (about as slow to decode as to encode, ~1 GB+ of memory).
    threads: threads per chunk, 0 = all cores. Never changes the bytes written. Zarr already spreads chunks over
    threads, so 1 is often the better choice there.
    """

    codec_id = "kolmo"

    def __init__(self, level=3, threads=0):
        self.level = check_level(level)
        self.threads = check_threads(threads)

    @classmethod
    def from_config(cls, config):
        """Takes the output of get_config as it is (numcodecs.get_codec removes "id" itself)."""
        config = dict(config)
        if config.pop("id", cls.codec_id) != cls.codec_id:
            raise ValueError("not a kolmo codec config: %r" % (config,))
        return cls(**config)

    def encode(self, buf):
        arr = as_contiguous(buf)
        return _kolmo.compress(memoryview(arr).cast("B"), level=self.level, threads=self.threads)

    def decode(self, buf, out=None):
        blob = ensure_bytes(as_contiguous(buf)) if not isinstance(buf, bytes) else buf
        if out is not None:
            dest = ensure_contiguous_ndarray(out)
            # a frame that claims more than `out` holds is refused before anything is allocated
            dec = _kolmo.decompress(blob, threads=self.threads, max_output_size=dest.nbytes)
            if len(dec) != dest.nbytes:
                raise ValueError("kolmo: decoded %d bytes, out holds %d" % (len(dec), dest.nbytes))
            return ndarray_copy(dec, out)
        return _kolmo.decompress(blob, threads=self.threads)

    def __repr__(self):
        return "%s(level=%r, threads=%r)" % (type(self).__name__, self.level, self.threads)
