"""Kolmo as a Zarr v3 bytes-to-bytes codec, registered under the name "kolmo" through the `zarr.codecs` entry point.

    import zarr
    from numcodecs_kolmo.zarr3 import KolmoCodec
    z = zarr.create_array("data.zarr", shape=(1000, 1000), chunks=(250, 1000), dtype="f4",
                          compressors=KolmoCodec(level=5, threads=1))

Array metadata stores {"name": "kolmo", "configuration": {"level": 5, "threads": 1}}. Needs zarr >= 3.
"""
import asyncio
from dataclasses import dataclass

from zarr.abc.codec import BytesBytesCodec

from .codec import as_contiguous, check_level, check_threads

import kolmo as _kolmo

CODEC_NAME = "kolmo"


@dataclass(frozen=True)
class KolmoCodec(BytesBytesCodec):
    """level: 1..9 or "ultra" (3 the default); threads: per chunk, 0 = all cores (zarr already runs chunks
    concurrently, so 1 is usually right). `threads` never changes the bytes written."""

    is_fixed_size = False

    level: object = 3
    threads: int = 0

    def __init__(self, *, level=3, threads=0):
        object.__setattr__(self, "level", check_level(level))
        object.__setattr__(self, "threads", check_threads(threads))

    @classmethod
    def from_dict(cls, data):
        if not isinstance(data, dict) or data.get("name") != CODEC_NAME:
            raise ValueError("not a kolmo codec: %r" % (data,))
        return cls(**(data.get("configuration") or {}))

    def to_dict(self):
        return {"name": CODEC_NAME, "configuration": {"level": self.level, "threads": self.threads}}

    def _encode_sync(self, chunk_bytes, chunk_spec):
        arr = as_contiguous(chunk_bytes.as_numpy_array())
        blob = _kolmo.compress(memoryview(arr).cast("B"), level=self.level, threads=self.threads)
        return chunk_spec.prototype.buffer.from_bytes(blob)

    def _decode_sync(self, chunk_bytes, chunk_spec):
        blob = as_contiguous(chunk_bytes.as_numpy_array()).tobytes()
        return chunk_spec.prototype.buffer.from_bytes(_kolmo.decompress(blob, threads=self.threads))

    async def _encode_single(self, chunk_bytes, chunk_spec):
        return await asyncio.to_thread(self._encode_sync, chunk_bytes, chunk_spec)

    async def _decode_single(self, chunk_bytes, chunk_spec):
        return await asyncio.to_thread(self._decode_sync, chunk_bytes, chunk_spec)

    def compute_encoded_size(self, _input_byte_length, _chunk_spec):
        raise NotImplementedError


def register():
    """Register the codec with zarr by hand (the entry point does this when the package is installed)."""
    from zarr.registry import register_codec

    register_codec(CODEC_NAME, KolmoCodec)


__all__ = ["KolmoCodec", "register", "CODEC_NAME"]
