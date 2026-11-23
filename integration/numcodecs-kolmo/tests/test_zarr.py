"""Zarr stores written and read back: zarr 3 (v3 format with KolmoCodec, v2 format with the numcodecs codec), or
zarr 2 with the numcodecs codec."""
import json
import os

import numpy as np
import pytest

zarr = pytest.importorskip("zarr")
ZARR3 = int(zarr.__version__.split(".")[0]) >= 3

import kolmo
from numcodecs_kolmo import Kolmo


def field(shape=(40, 60, 50), dtype="f4", seed=1):
    """A smooth 3-D field with noise, like a model output."""
    rng = np.random.default_rng(seed)
    z, y, x = np.meshgrid(*(np.linspace(0, 3, n) for n in shape), indexing="ij")
    a = 280 + 10 * np.sin(x) * np.cos(y) - 2 * z + rng.normal(scale=0.05, size=shape)
    return a.astype(dtype)


@pytest.mark.skipif(not ZARR3, reason="zarr 3 only")
@pytest.mark.parametrize("dtype", ["f4", ">f8", "i2", "u1"])
def test_zarr3_v3_store(tmp_path, dtype):
    from numcodecs_kolmo.zarr3 import KolmoCodec

    a = field(dtype="f8").astype(dtype) if dtype[-1] != "1" else (field() % 200).astype(dtype)
    path = str(tmp_path / "a.zarr")
    z = zarr.create_array(path, shape=a.shape, chunks=(10, 30, 50), dtype=a.dtype,
                          compressors=KolmoCodec(level=5, threads=1), zarr_format=3)
    z[:] = a
    meta = json.load(open(os.path.join(path, "zarr.json")))
    names = [c["name"] for c in meta["codecs"]]
    assert "kolmo" in names
    assert meta["codecs"][names.index("kolmo")]["configuration"] == {"level": 5, "threads": 1}

    z2 = zarr.open_array(path, mode="r")                     # the codec is found by name through the registry
    assert np.array_equal(z2[:], a)
    assert np.array_equal(z2[3:17, 5:40:3, ::7], a[3:17, 5:40:3, ::7])
    # each stored chunk is a plain Kolmo frame
    chunk = os.path.join(path, "c", "0", "0", "0")
    assert len(kolmo.decompress(open(chunk, "rb").read())) == 10 * 30 * 50 * a.dtype.itemsize


@pytest.mark.skipif(not ZARR3, reason="zarr 3 only")
def test_zarr3_partial_chunks_and_fill(tmp_path):
    from numcodecs_kolmo.zarr3 import KolmoCodec

    path = str(tmp_path / "b.zarr")
    z = zarr.create_array(path, shape=(1001,), chunks=(100,), dtype="i4", fill_value=-1,
                          compressors=KolmoCodec(level="ultra", threads=1))
    z[5:997] = np.arange(992, dtype="i4")
    got = zarr.open_array(path, mode="r")[:]
    expect = np.full(1001, -1, "i4")
    expect[5:997] = np.arange(992, dtype="i4")
    assert np.array_equal(got, expect)


@pytest.mark.skipif(not ZARR3, reason="zarr 3 only")
def test_zarr3_codec_dict_round_trip():
    from numcodecs_kolmo.zarr3 import KolmoCodec
    from zarr.registry import get_codec_class

    c = KolmoCodec(level="ultra", threads=2)
    assert KolmoCodec.from_dict(c.to_dict()) == c
    assert KolmoCodec.from_dict({"name": "kolmo"}) == KolmoCodec()
    assert get_codec_class("kolmo") is KolmoCodec
    with pytest.raises(ValueError):
        KolmoCodec(level=0)


def test_zarr_v2_format(tmp_path):
    a = field(shape=(30, 40, 20), dtype="<f8")
    path = str(tmp_path / "v2.zarr")
    if ZARR3:
        z = zarr.create_array(path, shape=a.shape, chunks=(10, 40, 20), dtype=a.dtype, zarr_format=2,
                              compressors=Kolmo(level=3, threads=1))
    else:
        z = zarr.open(path, mode="w", shape=a.shape, chunks=(10, 40, 20), dtype=a.dtype,
                      compressor=Kolmo(level=3, threads=1))
    z[:] = a
    meta = json.load(open(os.path.join(path, ".zarray")))
    assert meta["compressor"] == {"id": "kolmo", "level": 3, "threads": 1}
    z2 = zarr.open_array(path, mode="r")                     # the numcodecs entry point resolves "kolmo"
    assert np.array_equal(z2[:], a)
