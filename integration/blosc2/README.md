# Kolmo Blosc2 codec plugin

Kolmo as a [c-blosc2](https://github.com/Blosc/c-blosc2) codec. Each Blosc2 block is compressed as one Kolmo frame, so
Blosc2 keeps its chunking, multithreading, super-chunks and frames, and Kolmo replaces the codec stage.

> **Codec ID 244 is a placeholder** from Blosc2's user range (160-255). Data written with it can be read only by a
> program that registers the same ID. A global ID (probably 41) is to be requested from the Blosc team
> ([REGISTRY-SUBMISSION.md](REGISTRY-SUBMISSION.md)).

## Motivation

Blosc2 is built for speed: blosclz, LZ4 and Zstd, after a byte or bit shuffle. On numeric data, Kolmo's own analysis is
1.04-2.7x smaller than the best of those settings in the benchmark below:
- byte planes, deltas, and integer and float series models, chosen per block;
- Kolmo's entropy coders.

It decompresses at 1-3.7 GB/s per thread, but compresses at 40-310 MB/s (level 3). It suits data written once and read
many times, not the hot paths where LZ4 shines.

## Build

Requirements:
- a C compiler;
- c-blosc2 with its headers. Tested with 3.8.0 here; CI also builds against Ubuntu 24.04's package. Install with `brew install c-blosc2` or `apt install libblosc2-dev`.

```bash
make -C integration/blosc2
```

c-blosc2 is found with `pkg-config blosc2`, or pass `BLOSC2_DIR=/prefix`. The build makes two files:
- `build/plugin/libblosc2_kolmo.so`, a dynamically loadable plugin. It is named `.so` on macOS too, because that is the
  name Blosc2 opens.
- `build/libblosc2_kolmo.a`, to link into an application.

Both contain Kolmo itself; the plugin does not link libblosc2.

## Use

**Linked in (works today, with the placeholder ID):**

```c
#include "blosc2.h"
#include "blosc2_kolmo.h"

blosc2_init();
blosc2_kolmo_register();                       /* once, before compressing or decompressing */
blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
cp.compcode = BLOSC_CODEC_KOLMO;
cp.clevel = 3;                                 /* Kolmo level 1-9 */
cp.typesize = sizeof(double);
cp.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOSHUFFLE;   /* Kolmo does its own byte planes */
cp.blocksize = chunk_bytes;                    /* large blocks: see below */
```

**As a dynamic plugin.** Once the codec has a global ID, c-blosc2 lists it by name with no encoder or decoder (as for
openzl or grok). On first use, Blosc2 opens `libblosc2_kolmo.so` from the library path. If that fails, it runs
`python -c "import blosc2_kolmo; blosc2_kolmo.print_libpath()"` (see [python/blosc2_kolmo](python/blosc2_kolmo)).
It then loads the encoder and decoder named by the library's `info` symbol. Programs and python-blosc2 then only set
`compcode`. [tests/test_dynamic_load.c](tests/test_dynamic_load.c) exercises exactly this path today, under the
placeholder ID.

### Settings

| setting | meaning |
|---|---|
| `clevel` | 1-9: the Kolmo level of the same number ([docs/LEVELS.md](../../docs/LEVELS.md)). Blosc2 stores clevel 0 uncompressed. |
| `compcode_meta` | 0: use `clevel`. 1-10: this Kolmo level instead (10 = ultra, the archive tier). |
| `filters` | **use `BLOSC_NOSHUFFLE`**. Kolmo finds byte planes itself. Blosc2's default shuffle in front of it costs up to 2.8x in size: 13.18x becomes 4.74x on owid_energy. |
| `blocksize` | **use large blocks (the chunk, or at least 1 MiB)**. Blosc2's automatic block size is much smaller than the chunk. It leaves Kolmo less to analyse and repeats its per-block analysis: 7.6x slower to compress and 8% larger on owid_energy, 45% larger on the audio clip. |
| `typesize` | recorded in every chunk header; Kolmo detects the element width itself |
| `nthreads` | Blosc2 compresses blocks in parallel; each block uses one Kolmo thread |

Strengths:
- the smallest output of any Blosc2 codec on the numeric data tested;
- no tuning beyond the level;
- checksummed frames: a damaged block is an error, never wrong data.

Weaknesses:
- compression is 5-190x slower than LZ4 and Zstd at their fast levels;
- decompression is slower than LZ4;
- level 1 is weak on arrays today: in the tables below it is often larger than Zstd 1 + shuffle.

## Test

```bash
make -C integration/blosc2 test
```

[tests/test_blosc2_kolmo.c](tests/test_blosc2_kolmo.c) covers:
- in-process registration;
- round trips for typesizes 1, 2, 4 and 8, with and without shuffle, at levels 1, 5 and 9, with automatic and 1 MiB blocks;
- the `compcode_meta` override (ultra);
- incompressible and tiny inputs;
- a damaged chunk, which must be an error and never wrong data;
- a super-chunk serialised to a contiguous frame and reopened;
- the plugin library opened by hand through its `info` symbol.

[tests/test_dynamic_load.c](tests/test_dynamic_load.c) does not link the plugin. It registers the ID with a NULL
encoder and decoder, as c-blosc2's registry does, and checks that Blosc2 itself loads `libblosc2_kolmo.so` and
round-trips through it. It runs twice: once with the library path set, and once through the `blosc2_kolmo` Python package (`PYTHONPATH`). c-blosc2 3.x tries the library path first; 2.x, as packaged in Ubuntu 24.04, uses only the Python route, so there the library-path run is skipped. Both tests call `blosc2_init()` first and `blosc2_destroy()` last, as the Blosc registry
requires.

**Without c-blosc2 installed:** the build stops with the message `c-blosc2 not found` and the install commands. Nothing
else in the repository depends on Blosc2. Install it, or point `BLOSC2_DIR` at an installation, to run the tests. CI
runs them on Ubuntu 24.04 and macOS (`.github/workflows/integrations.yml`).

## Benchmark

```bash
make -C integration/blosc2 bench                     # the three synthetic arrays
make -C integration/blosc2 bench BENCH_ARGS="-r 5 path/to/file.npy:f64:128 synth:f32_grid"
```

[bench/bench_blosc2.c](bench/bench_blosc2.c) cuts each input into 4 MiB chunks and runs `blosc2_compress_ctx` and
`blosc2_decompress_ctx` with `nthreads = 1`. Timings are the best of 5 passes. Speeds are uncompressed MB/s (10^6
bytes), and sizes include Blosc2's chunk headers. The inputs are those of the HDF5 benchmark
([../hdf5/README.md](../hdf5/README.md#benchmark)): three real files (OWID energy float64, OWID population int64, an
ESC-50 audio clip as int16) and three generated arrays.

Machine and setup:
- Apple M5, macOS, c-blosc2 3.8.0, Kolmo at main 7b2eb08 built portable, 2026-11-23.
- The machine was shared with other jobs (load average 7-12), so speeds can be up to about 30% low.

The full output is in [bench/RESULTS.md](bench/RESULTS.md).

**Compression ratio** (higher is better):

| codec (+ filter) | owid_energy f64 | population i64 | esc50 i16 | f32_grid | i64_ticks | i16_wave |
|---|---:|---:|---:|---:|---:|---:|
| blosclz 5 + shuffle | 4.22 | 2.77 | 16.52 | 1.33 | 5.95 | 1.36 |
| lz4 5 + shuffle | 4.10 | 2.73 | 16.28 | 1.39 | 5.87 | 1.25 |
| zstd 1 + shuffle | 5.00 | 2.85 | 18.43 | 2.06 | 6.46 | 1.48 |
| zstd 5 + shuffle | 5.16 | 2.95 | 19.36 | 2.19 | 6.90 | 1.56 |
| zstd 9 + shuffle | 5.37 | 3.05 | 19.96 | 2.35 | 7.17 | 1.68 |
| zstd 5 + bitshuffle | 3.97 | 2.85 | 20.04 | 1.55 | 5.70 | 1.43 |
| kolmo 3 (automatic blocks) | 13.18 | 3.71 | 16.42 | 4.14 | 8.94 | 1.74 |
| **kolmo 3, block = chunk** | 14.30 | 3.83 | 23.74 | **5.26** | 9.09 | 1.75 |
| kolmo 5, block = chunk | **14.98** | **3.93** | **24.10** | 4.68 | **9.39** | **1.85** |
| kolmo 3 + shuffle | 4.74 | 2.83 | 18.18 | 1.79 | 6.72 | 1.47 |

**Speed** (MB/s, compress / decompress):

| codec | owid_energy f64 | population i64 | esc50 i16 | f32_grid | i64_ticks | i16_wave |
|---|---:|---:|---:|---:|---:|---:|
| lz4 5 + shuffle | 4475 / 11021 | 7198 / 18894 | 9188 / 14700 | 2362 / 9326 | 5343 / 16811 | 1284 / 4136 |
| zstd 1 + shuffle | 1647 / 3105 | 2852 / 8397 | 4200 / 6785 | 934 / 2030 | 3403 / 5883 | 680 / 1968 |
| zstd 9 + shuffle | 3 / 3264 | 8 / 3359 | 97 / 6300 | 9 / 1389 | 11 / 7182 | 8 / 1954 |
| kolmo 3, block = chunk | 314 / 2509 | 38 / 1574 | 149 / 1441 | 131 / 1063 | 250 / 3720 | 97 / 1013 |
| kolmo 5, block = chunk | 48 / 1945 | 7 / 1128 | 36 / 1413 | 9 / 903 | 35 / 2700 | 5 / 388 |

What it shows:
- **Size.** Kolmo 3 with block = chunk is 1.04-2.7x smaller than the best non-Kolmo setting for each input. That
  includes Zstd 9, which compresses 1.5-100x more slowly than Kolmo 3. The largest gains are on float tables and gridded
  floats.
- **Speed.** Against Zstd 1 + shuffle, the fast Blosc2 setting, Kolmo 3 compresses 5-75x more slowly and decompresses
  1.2-5.3x more slowly.
- **Shuffle** in front of Kolmo always costs size.
- **Automatic blocks** cost speed and, on some inputs, size.
- **f32_grid:** level 5 is larger than level 3 here (4.68 against 5.26). This is a level-ordering defect on main, not
  a plugin issue; it is reported to the "weak levels" work.
