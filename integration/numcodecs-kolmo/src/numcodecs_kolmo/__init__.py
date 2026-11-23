"""numcodecs-kolmo: Kolmo as a numcodecs codec (Zarr v2 and the numcodecs API) and a Zarr v3 bytes-to-bytes codec.

    import numcodecs, numcodecs_kolmo
    codec = numcodecs.get_codec({"id": "kolmo", "level": 5})       # found through the numcodecs.codecs entry point
    z = zarr.create_array(store, shape=..., dtype=..., compressors=numcodecs_kolmo.zarr3.KolmoCodec(level=5))  # zarr 3

The compressed chunk is a plain Kolmo frame: `kolmo -d` and `kolmo.decompress` read it.
"""
from .codec import Kolmo, LEVELS

__all__ = ["Kolmo", "LEVELS"]
__version__ = "0.0.1.dev0"
