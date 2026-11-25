"""Run write and read as independent processes with installed wheels, from outside the source tree."""
import hashlib
import json
import sys
from pathlib import Path

import numpy as np

mode, family, location = sys.argv[1:]
if mode not in {"read", "write"}:
    raise ValueError(mode)
path = Path(location)
source = np.arange(513 * 93, dtype="<i4").reshape(513, 93)
chunks = (32, 23)
settings = {"level": 3, "threads": 1}
if family == "numcodecs":
    import numcodecs
    codec = numcodecs.get_codec({"id": "kolmo", **settings})
    if mode == "write":
        path.write_bytes(codec.encode(source))
    restored = bytes(codec.decode(path.read_bytes()))
    assert restored == source.tobytes()
elif family == "hdf5":
    import h5py
    if mode == "write":
        with h5py.File(path, "w") as root:
            root.create_dataset("points", data=source, chunks=chunks, compression=480, compression_opts=(3,))
    with h5py.File(path, "r") as root:
        dataset = root["points"]
        assert dataset.id.get_create_plist().get_filter_by_id(480)[1] == (3, 4, 0, 1)
        assert np.array_equal(dataset[19::79, 2:7], source[19::79, 2:7])
        assert dataset.id.read_direct_chunk((0,0))[0] == 0
        restored = dataset[:].tobytes()
elif family in {"zarr2", "zarr3"}:
    import zarr
    fmt = int(family[-1])
    if mode == "write":
        if fmt == 2:
            from numcodecs_kolmo import Kolmo
            compressor = Kolmo(**settings)
        else:
            from numcodecs_kolmo.zarr3 import KolmoCodec
            compressor = KolmoCodec(**settings)
        if int(zarr.__version__.split(".")[0]) == 2:
            assert fmt == 2
            data = zarr.open_array(str(path), mode="w", shape=source.shape, dtype=source.dtype,
                                   chunks=chunks, compressor=compressor)
        else:
            data = zarr.create_array(str(path), shape=source.shape, dtype=source.dtype, chunks=chunks,
                                     zarr_format=fmt, compressors=compressor)
        data[:] = source
    if fmt == 2:
        assert json.loads((path / ".zarray").read_text())["compressor"] == {"id": "kolmo", **settings}
    else:
        assert {"name": "kolmo", "configuration": settings} in json.loads((path / "zarr.json").read_text())["codecs"]
    data = zarr.open_array(str(path), mode="r")
    assert np.array_equal(data[19::79, 2:7], source[19::79, 2:7])
    restored = data[:].tobytes()
else:
    raise ValueError(family)
assert restored == source.tobytes()
print(f"{mode} {family} {source.nbytes} bytes, recovered SHA-256 {hashlib.sha256(restored).hexdigest()}")
