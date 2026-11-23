# numcodecs-kolmo

[Kolmo](https://github.com/adnaneben/kolmo) as a [numcodecs](https://numcodecs.readthedocs.io) codec and a
[Zarr](https://zarr.dev) v3 codec. Kolmo is a structure-aware lossless compressor; on numeric and scientific arrays it
is often smaller than zstd at its highest level while encoding at zstd's default-level speed (see
[Benchmark](#benchmark)).

> Not on PyPI yet. Install from this directory: `pip install ./integration/numcodecs-kolmo` (it needs `kolmoc`, the
> Kolmo binding, from PyPI or `pip install ./bindings/python`).

## Use

numcodecs, and Zarr with the v2 format (the codec is found through the `numcodecs.codecs` entry point):

```python
import numcodecs
from numcodecs_kolmo import Kolmo

codec = Kolmo(level=5, threads=1)          # or numcodecs.get_codec({"id": "kolmo", "level": 5})
blob = codec.encode(array)                 # any buffer: ndarray (also non-contiguous), bytes, memoryview
array2 = codec.decode(blob, out=np.empty_like(array))
```

Zarr 3 (the v3 format), a bytes-to-bytes codec named `kolmo`, found through the `zarr.codecs` entry point:

```python
import zarr
from numcodecs_kolmo.zarr3 import KolmoCodec

z = zarr.create_array("data.zarr", shape=(365, 721, 1440), chunks=(1, 721, 1440), dtype="f4",
                      compressors=KolmoCodec(level=3, threads=1))
z[:] = data
zarr.open_array("data.zarr")[:]            # reading needs only this package installed
```

The array metadata stores `{"name": "kolmo", "configuration": {"level": 3, "threads": 1}}` (v3) or
`{"id": "kolmo", "level": 3, "threads": 1}` (v2). Each stored chunk is a plain Kolmo frame, which `kolmo -d` and
`kolmo.decompress` also read.

## Parameters

- `level`: `1` (fastest) to `9` (max), `3` the default, or `"ultra"` (outside the scale: about as slow to decode as to
  encode, ~1 GB+ of memory). The levels are those of [docs/LEVELS.md](../../docs/LEVELS.md) and give the same bytes as
  `kolmo -N`.
- `threads`: threads per chunk, `0` (the default) = all cores. It never changes the bytes written. Zarr already
  encodes and decodes chunks concurrently, so `threads=1` is usually the better choice inside Zarr.

## Benchmark

`python bench.py` (needs numpy and numcodecs). Seven generated arrays of 8 MiB, each encoded as one chunk:

| array (8 MiB, one chunk) | kolmo 3 | kolmo 5 | kolmo 9 | zstd 3 | zstd 19 | blosc lz4 | blosc zstd | lz4 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| temperature grid, f4 rounded to 0.01 K | **4.25** | **4.27** | **4.32** | 1.97 | 2.50 | 1.71 | 2.12 | 1.40 |
| temperature grid, f4 full precision | **1.96** | **1.97** | **1.98** | 1.25 | 1.40 | 1.71 | 1.85 | 1.00 |
| accelerometer, i2, 3 axes | **2.62** | **2.73** | **2.84** | 1.52 | 1.83 | 1.62 | 2.21 | 1.10 |
| detector counts, u2 Poisson | **2.38** | **2.38** | **2.35** | 1.96 | 2.13 | 1.79 | 2.18 | 1.17 |
| salinity series, f8 random walk | **1.77** | **1.79** | **1.79** | 1.22 | 1.59 | 1.59 | 1.61 | 1.00 |
| land-cover classes, u1 patches | 17.37 | 17.16 | **28.03** | 12.12 | 24.87 | 4.63 | 14.17 | 4.70 |
| event timestamps, i8 ns | **4.72** | **4.75** | **4.70** | 3.70 | 3.73 | 3.48 | 3.79 | 1.59 |

Compression ratio (original / compressed; higher is better; bold where Kolmo beats every other codec).

Speed, MB/s of uncompressed data, encode / decode:

| array | kolmo 3 | kolmo 5 | kolmo 9 | zstd 3 | zstd 19 | blosc lz4 | blosc zstd | lz4 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| temperature, 0.01 K | 289 / 1163 | 60 / 776 | 3 / 47 | 335 / 1113 | 7 / 1002 | 1915 / 2683 | 183 / 1229 | 684 / 5242 |
| temperature, full | 212 / 1911 | 11 / 1365 | 4 / 812 | 615 / 1723 | 11 / 690 | 1923 / 2794 | 253 / 2267 | 9204 / 56347 |
| accelerometer | 408 / 600 | 22 / 652 | 27 / 27 | 483 / 1155 | 8 / 675 | 1177 / 2357 | 123 / 1134 | 2075 / 10237 |
| detector counts | 559 / 1206 | 24 / 1291 | 12 / 24 | 315 / 1057 | 13 / 836 | 2349 / 3116 | 238 / 1769 | 870 / 5785 |
| salinity | 549 / 1709 | 10 / 1960 | 8 / 2064 | 425 / 1235 | 9 / 1534 | 3295 / 3610 | 763 / 2177 | 30662 / 58885 |
| land cover | 286 / 1291 | 26 / 2111 | 3 / 723 | 1365 / 2509 | 4 / 3581 | 1396 / 3008 | 135 / 2891 | 1466 / 3120 |
| timestamps | 643 / 1618 | 61 / 3648 | 21 / 143 | 697 / 1504 | 6 / 1449 | 2388 / 2616 | 643 / 1706 | 770 / 4129 |

What it shows: Kolmo level 3 is 1.1x to 1.7x smaller than zstd 19 on six of the seven arrays, at roughly zstd 3's
encode and decode speed; Blosc with shuffle closes part of the gap on floats but stays behind. On the land-cover
classes (long uniform runs) zstd 19 beats Kolmo levels 3 and 5 and only level 9 is smaller. Level 9 can decode slowly
(24-143 MB/s on four arrays here): it is for archives read rarely, and level 5 seldom gains over 3 on these arrays.

**Method (fair-benchmark rule).** Every codec runs on one thread: Kolmo `threads=1`; Blosc with
`numcodecs.blosc.use_threads = False` and `set_nthreads(1)`; numcodecs' Zstd and LZ4 are single-threaded. Each array
is encoded and decoded whole as one chunk through the numcodecs API (the path Zarr uses), the time is the best of 3
runs of `time.perf_counter`, and every decode is compared with the input. MB = 10^6 bytes. Blosc runs at clevel 5
with byte shuffle (its common Zarr setting), LZ4 at acceleration 1. The arrays are synthetic, generated with a fixed
seed (`bench.py`, `datasets()`), so the run needs no download; they imitate real data but are not real data, and
results on your arrays will differ. Run: Apple M5 (arm64), macOS, Python 3.12.8, numcodecs 0.17.1 (its bundled zstd,
lz4 and c-blosc), kolmoc built from this repository at 0.0.2, 2026-11-23.

## Tests

```
pip install ./bindings/python ./integration/numcodecs-kolmo pytest zarr
pytest integration/numcodecs-kolmo/tests
```

Round trips over 23 dtypes (big-endian included) and 7 shapes (empty included), `out=`, non-contiguous and
bytes-like input, corrupt input, the config round trip, the entry points, and Zarr stores written and read back
(v3 format with `KolmoCodec`, v2 format with `Kolmo`).

## Licence

Apache-2.0, as the rest of [Kolmo](https://github.com/adnaneben/kolmo).
