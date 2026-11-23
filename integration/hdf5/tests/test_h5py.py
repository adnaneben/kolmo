"""Round trip of the Kolmo HDF5 filter through h5py (run by `make test` when h5py and numpy are installed).

    HDF5_PLUGIN_PATH=build/plugin python3 tests/test_h5py.py

The filter is a dynamically loaded plugin; h5py only needs its ID and client data (level). The plugin resolves its
HDF5 symbols to the HDF5 inside h5py, so the same file works with h5py wheels and with a system HDF5.
"""
import os
import sys
import tempfile

import h5py
import numpy as np

FILTER_ID = 480  # PLACEHOLDER from the HDF5 testing range; see ../H5Zkolmo.h and ../FILTER-ID-REQUEST.md


def main():
    if not os.environ.get("HDF5_PLUGIN_PATH"):
        print("note: HDF5_PLUGIN_PATH is not set")
    rng = np.random.default_rng(7)
    arrays = {
        "f32_field": (np.sin(np.linspace(0, 40, 2_000_000)) * 20 + rng.normal(0, 0.01, 2_000_000)).astype("<f4").reshape(2000, 1000),
        "f64_prices": np.round(np.cumsum(rng.normal(0, 1, 300_000)) + 1000, 2),
        "i16_sensor": (np.sin(np.arange(500_000) * 0.003) * 800 + rng.integers(0, 17, 500_000)).astype("<i2"),
        "i64_stamps": 1_700_000_000_000 + np.arange(200_000, dtype="<i8") * 1000 + rng.integers(0, 50, 200_000),
        "u8_noise": rng.integers(0, 256, 100_000, dtype=np.uint8),
    }
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "kolmo_h5py.h5")
        with h5py.File(path, "w") as f:
            for i, (name, a) in enumerate(arrays.items()):
                level = (1, 3, 5, 3, 9)[i]
                chunks = tuple(min(s, 262144 if a.ndim == 1 else 256) for s in a.shape)
                f.create_dataset(name, data=a, chunks=chunks, compression=FILTER_ID, compression_opts=(level,))
        failures = 0
        with h5py.File(path, "r") as f:
            for name, a in arrays.items():
                d = f[name]
                back = d[()]
                ok = back.dtype == a.dtype and np.array_equal(back, a)
                opts = d.compression_opts
                ok = ok and opts is not None and opts[1] == a.dtype.itemsize
                stored = d.id.get_storage_size()
                print(f"{'ok ' if ok else 'FAIL'} {name:12s} {a.nbytes:>9d} -> {stored:>8d} bytes ({a.nbytes / stored:.2f}x) cd_values={opts}")
                failures += not ok
                # a partial read decodes only the chunks it touches
                if a.ndim == 1:
                    assert np.array_equal(d[1000:5000], a[1000:5000])
    print("all h5py tests passed" if not failures else f"FAILED: {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
