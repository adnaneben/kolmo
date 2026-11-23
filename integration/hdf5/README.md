# Kolmo HDF5 filter plugin

A dynamically loadable HDF5 filter that compresses each chunk with Kolmo. It works from C, from h5py and from any
tool built on HDF5 (h5repack, h5dump, netCDF-4, ...), once HDF5 can find the plugin.

> **Filter ID 480 is a placeholder** from the range The HDF Group keeps for testing (256-511). Do not keep or share
> files written with it: another tool may use 480 for a different filter. A permanent ID has been requested
> ([FILTER-ID-REQUEST.md](FILTER-ID-REQUEST.md)); this README will change when it is granted.

## Build

Requirements:
- a C compiler;
- HDF5 1.10 or later with its headers. Tested with 2.2.0. Install with `brew install hdf5` or `apt install libhdf5-dev`.

```bash
make -C integration/hdf5
```

This builds `build/plugin/libH5Zkolmo.dylib` (`.so` on Linux), a single file with Kolmo linked in. HDF5 is found with
`pkg-config` (`hdf5`, or `hdf5-serial` on Debian and Ubuntu); otherwise pass `HDF5_DIR=/prefix`. Kolmo is compiled
portable by default; `KOLMO_ARCH=-mcpu=native` (or `-march=native`) tunes it for the build machine only.

How the plugin finds HDF5:
- **macOS:** it does not link libhdf5. Its HDF5 calls resolve to the HDF5 already loaded in the process, so the same
  file works with a C program, with h5py's bundled HDF5 (tested with h5py 3.16.0 and HDF5 2.0.0) and with the HDF5
  tools.
- **Linux:** it links the libhdf5 it was built against, because Python loads h5py's HDF5 privately (`RTLD_LOCAL`).
  It works with C programs, the HDF5 tools, and an h5py that uses the same system HDF5, such as Debian/Ubuntu
  `python3-h5py`. h5py wheels from PyPI carry their own HDF5: for them, build the plugin against that HDF5, or wait
  for the hdf5plugin package.

## Use

Point HDF5 at the plugin directory:

```bash
export HDF5_PLUGIN_PATH=/path/to/kolmo/integration/hdf5/build/plugin
```

Or copy the file into HDF5's default plugin directory (`/usr/local/hdf5/lib/plugin` on Unix).

**h5py**

```python
import h5py
KOLMO = 480          # placeholder ID, see above
with h5py.File("data.h5", "w") as f:
    f.create_dataset("t2m", data=array, chunks=(256, 1024), compression=KOLMO, compression_opts=(3,))
# reading needs only HDF5_PLUGIN_PATH: the filter's parameters are stored with the dataset
```

**C**

```c
#include "H5Zkolmo.h"
unsigned cd[1] = {3};                       /* level */
H5Pset_chunk(dcpl, rank, chunk_dims);
H5Pset_filter(dcpl, H5Z_FILTER_KOLMO, H5Z_FLAG_MANDATORY, 1, cd);
```

An application can also compile `H5Zkolmo.c` in and call `H5Z_kolmo_register()` instead of using the plugin path.

**Command line**

```bash
h5repack -f UD=480,0,1,3 in.h5 out.h5     # UD=<id>,<flags>,<number of cd_values>,<level>
```

### Parameters (`cd_values`)

| index | meaning | default |
|---|---|---|
| 0 | level: 1 (fastest) to 9 (smallest), 10 = ultra ([docs/LEVELS.md](../../docs/LEVELS.md)); 0 = 3 | 3 |
| 1 | element size in bytes; filled in from the dataset's type (`H5Tget_size`) when the dataset is created | type size |
| 2 | threads per chunk; 0 = 1 | 1 |
| 3 | layout version, written by the filter | 1 |

All are optional. The decoder reads none of them: each chunk is a self-describing, checksummed Kolmo frame. A damaged
chunk is a read error, never wrong data.

### Advice

- **Leave the shuffle filter off.** Kolmo finds the element width and splits byte planes itself. In front of Kolmo,
  HDF5's shuffle makes the output 1.1-3.1x larger (the "shuffle + kolmo 3" rows below).
- **Levels 3-5** suit most data. Level 7 is rarely smaller than 5 on numeric arrays and compresses much more slowly.
  Levels 8-9 decompress about 10x more slowly. Level 1 is fast but weak on arrays: see the tables.
- **Chunks of 256 KiB to a few MiB** give Kolmo room to find structure. HDF5 runs filters on one thread per call;
  `cd_values[2]` > 1 helps only for large chunks.
- The element size is recorded with every dataset. This version of Kolmo does not use it yet: its planner tries the
  word widths 1-8 itself. Files written now carry the value for a later version that takes it as a hint.

## Test

```bash
make -C integration/hdf5 test
```

The test writes and reads back chunked datasets through the C API, with the plugin loaded from `HDF5_PLUGIN_PATH`. The datasets cover:
- uint8, int16, int32 and int64, float32 and float64, and a compound type;
- 1-D, 2-D and 3-D shapes, with partial edge chunks;
- levels 1-10, including incompressible data.

It also checks:
- the stored `cd_values`;
- that level 11 is refused;
- that a damaged chunk is a read error.

Then, if `python3 -c "import h5py, numpy"` works, it runs [tests/test_h5py.py](tests/test_h5py.py) through h5py.

**Without HDF5 installed:** the build stops with the message `HDF5 not found` and the install commands; nothing else
in the repository depends on HDF5. To run the tests, install HDF5 (`brew install hdf5`, `apt install libhdf5-dev`) or
point `HDF5_DIR` at an existing installation. CI runs them on Ubuntu 24.04 (with h5py) and macOS
(`.github/workflows/integrations.yml`). Without h5py the Python test prints `SKIP` and the C test still runs.

## Benchmark

```bash
make -C integration/hdf5 bench                       # the three synthetic arrays
make -C integration/hdf5 bench BENCH_ARGS="-r 5 path/to/file.npy:f64:128 path/to/file.i64:i64 synth:f32_grid"
```

[bench/bench_hdf5.c](bench/bench_hdf5.c) writes each input as a 1-D dataset with 1 MiB chunks to an in-memory HDF5 file
(core driver, so there is no disk I/O). The chunk cache is off, so every chunk passes through the filter. The timings
are whole-dataset `H5Dwrite` and `H5Dread`, best of 5, on one thread. Speeds are uncompressed MB/s (10^6 bytes).

The filters compared:
- gzip and shuffle are HDF5's own; szip is libaec;
- zstd is filter 32015 with `cd_values[0]` = level, as in hdf5plugin. The benchmark builds a minimal copy of it,
  [bench/H5Zzstd_bench.c](bench/H5Zzstd_bench.c), against the system libzstd 1.5.7.

Inputs:
- **owid_energy**: Our World in Data energy table as float64, 23.9 MB (the `.npy` from the 0.0.2 market review corpus).
- **population**: OWID population as int64, 0.15 MB.
- **esc50 clip**: one ESC-50 audio clip as int16 (the WAV data chunk), 0.44 MB.
- **f32_grid**, **i64_ticks**, **i16_wave**: the generated arrays of [../common/bench_data.h](../common/bench_data.h).
  They are deterministic, so anyone can reproduce them.

Machine and setup:
- Apple M5, macOS, HDF5 2.2.0, Kolmo at main 7b2eb08 built portable (`ARCH=`), 2026-11-23.
- The machine was shared with other jobs (load average 6-13 on 10 cores). Ratios are exact; speeds can be up to about
  30% low.

The full output is in [bench/RESULTS.md](bench/RESULTS.md).

**Compression ratio** (higher is better; best in bold):

| filter | owid_energy f64 | population i64 | esc50 i16 | f32_grid | i64_ticks | i16_wave |
|---|---:|---:|---:|---:|---:|---:|
| gzip 6 | 8.39 | 2.05 | 16.00 | 2.64 | 3.05 | 1.09 |
| shuffle + gzip 6 | 5.03 | 2.97 | 18.21 | 2.14 | 6.90 | 1.59 |
| szip nn | 2.87 | 3.08 | 15.38 | 1.63 | 5.69 | 1.53 |
| zstd 3 | 8.49 | 2.33 | 16.93 | 2.76 | 3.94 | 1.08 |
| shuffle + zstd 3 | 5.03 | 2.94 | 18.90 | 2.20 | 6.65 | 1.57 |
| shuffle + zstd 19 | 5.34 | 3.05 | 19.99 | 2.35 | 7.16 | 1.68 |
| kolmo 1 | 6.73 | 2.92 | 13.94 | 1.70 | 6.39 | 1.00 |
| **kolmo 3** | 15.38 | 3.83 | 23.79 | 5.49 | 9.09 | 1.75 |
| kolmo 5 | **15.73** | **3.93** | **24.15** | **5.61** | **9.34** | **1.85** |
| shuffle + kolmo 3 | 4.98 | 2.92 | 18.68 | 2.15 | 6.37 | 1.56 |

**Speed** (MB/s, compress / decompress):

| filter | owid_energy f64 | population i64 | esc50 i16 | f32_grid | i64_ticks | i16_wave |
|---|---:|---:|---:|---:|---:|---:|
| gzip 6 | 141 / 2394 | 23 / 922 | 398 / 3128 | 61 / 1177 | 25 / 1003 | 27 / 452 |
| shuffle + zstd 3 | 857 / 1673 | 1608 / 2478 | 2321 / 2436 | 742 / 1513 | 1802 / 2572 | 705 / 1451 |
| shuffle + zstd 19 | 7 / 1828 | 22 / 1963 | 190 / 2689 | 23 / 1247 | 29 / 2725 | 11 / 1392 |
| kolmo 3 | 271 / 2535 | 46 / 1758 | 177 / 1646 | 158 / 1170 | 231 / 3732 | 98 / 1121 |
| kolmo 5 | 60 / 1872 | 9 / 1172 | 45 / 1531 | 36 / 898 | 34 / 2553 | 8 / 639 |

What it shows:
- **Size.** Kolmo 3 is the smallest of the usual filters on all six inputs. Against the best non-Kolmo setting for
  each input (including zstd 19), it is 1.04-2.0x smaller. Against shuffle + zstd 3, a common choice, it is 1.1-3.1x
  smaller. The largest gains are on float tables and gridded floats (owid_energy, f32_grid).
- **Compression speed.** Kolmo 3 compresses at 46-271 MB/s: 3-35x slower than shuffle + zstd 3. It is faster than
  gzip 6 and zstd 19 on five of the six inputs, and slightly slower on the audio clip.
- **Decompression speed** is in the same range as zstd and gzip: 1.1-3.7 GB/s.
- **Where it fits:** data written once and read many times, such as archives, published datasets and simulation output.
  It is not the choice for write-heavy streaming at GB/s.
- **Level 1** is fast but often larger than shuffle + zstd 3, and it does not compress i16_wave at all. The level-1
  engine is being reworked on main ("weak levels").
