# Native lane results (rc3 wave 2)

Branch `claude/rc3w2-native-20261117`, from `claude/release-rc3-20261116` (`1536036`). The census, the decisions and the
plan are in `docs/NATIVE-PLAN.md`. This file holds the numbers, the commands and what did not work.

**Machine:** a cloud VM with an Intel Xeon at 2.10 GHz (4 vCPUs, AVX-512, BMI2), Linux 6.18, GCC 13.3. Builds use the
repository defaults (`-O3 -march=native`). `benchmark/rc2/PROTOCOL.md` applies: 1 thread and 4 threads, never mixed.

**Data:** kept outside the repository; downloads were treated as untrusted (own directory, `python3 -I` to unpack).
- **Silesia:** 12 files, 211,938,580 bytes.
- **Structured set:** 13 files, 28,825,260 bytes:
  - `owid-co2-data.csv`, `owid-energy-data.csv` and `owid-covid-latest.json` (OWID, GitHub);
  - the 10 inputs that `benchmark/determinism/check.py` generates (text, CSV, log, int16/int64/float64 arrays, image,
    NPY, WAV, Y4M).

  It is the deps lane's set plus `video.y4m`. That generator input did not exist when the deps lane ran.

## What changed in the code

| change | files | default output | opt-in output |
|---|---|---|---|
| Kolmo Huffman (khuf), codec 27 | `src/algo/khuf.c`, `khuf.h` | identical | khuf leaves |
| KLZ (codec 28) and KLZH (codec 29) | `src/algo/klz.c`, `klz.h` | identical | KLZ / KLZH blocks and leaves |
| KLZMA, codec 24 | `src/algo/klzma.c`, `klzma.h` | identical | KLZMA blocks and leaves |
| khash64 | `src/algo/khash.c`, `khash.h` | not used by the container yet | — |
| opt-in, codec tables | `src/codecs/native.c`; hooks: `codecs/codec.h` (ids), `codecs/codec.c` (registry, substitution in `codec_compress`), `core/blocks.c`, `transforms/graph.c`, `codecs/planes_leaf.c`, `codecs/zrun_leaf.c`, `transforms/table_cols.c` (`codec_wire` where ids are written), `codecs/planes_leaf.c` (leaf whitelist), `core/decode.c` and `transforms/graph.c` (version gate), `codecs/graph_leaf.c` (plane-aligned Huffman), `codecs/nplane.c` (calls HUF directly; same bytes), `src/kolmo.c` (one include) | identical | version 11 frames |
| build | `Makefile` (`NATSRC`, `check-native` in `check-rc`), both bindings' source lists | — | — |
| tests | `tests/test_native.c`, `tests/test_native_modes.py` | — | — |
| docs | `docs/NATIVE-PLAN.md`; rows in `research/CODECS.md`; codec ids 24 and 27–29 in `SPEC.md`; a correction note in `docs/DEPENDENCIES.md` | — | — |

The default output is unchanged: `python3 benchmark/determinism/check.py build/kolmo build/determinism` reports 122 frames
OK. With `KOLMO_NATIVE=...` the encoder uses Kolmo's coders. Decoders read the new ids in version 11 frames without any
opt-in.

## 1. Census: where the libraries are, per mode

```
make all && sh benchmark/rc3w2/native/build_libprof.sh
python3 benchmark/rc3w2/native/profile_modes.py build/kolmo-libprof SILESIA results/libprof-silesia.tsv speed balanced eco hc ratio
python3 benchmark/rc3w2/native/profile_modes.py build/kolmo-libprof STRUCT results/libprof-struct.tsv speed balanced eco hc ratio
KOLMO_NATIVE=all python3 benchmark/rc3w2/native/profile_modes.py build/kolmo-libprof STRUCT results/libprof-struct-native.tsv ...
KOLMO_NATIVE=all,strong python3 benchmark/rc3w2/native/profile_modes.py build/kolmo-libprof STRUCT results/libprof-struct-strong.tsv eco hc
python3 benchmark/rc3w2/native/census.py results/libprof-*.tsv
```

`libprof.c` is the deps lane's `--wrap` profiler with three changes:
- It counts the compressed bytes of every call; for a decoder call, that is the part of the output file coded through it.
- It counts zstd's internal Huffman calls apart. They were mixed into the deps lane's HUF figure.
- It counts the Huffman stream of codec 22 apart, because that stream is a zstd output.

Each cell: share of compress time / share of decompress time / share of output bytes (1 thread, process start
included).

**Silesia, library build (default)** (`results/libprof-silesia.tsv`)

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | HUF c / d / bytes | LZ4 c / d / bytes | Brotli c / d / bytes | LZMA2 c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|---|---|---|---|
| speed | 99,938,346 | 0.69 | 0.36 | 0.1% / 0.0% / 0.0% | — | 71.0% / 28.0% / 98.9% | — | — | 0.5% / 1.4% / 0.0% |
| balanced | 58,487,839 | 1.58 | 0.64 | 73.6% / 46.9% / 100.0% | — | — | — | — | 0.7% / 1.4% / 0.0% |
| eco | 50,773,162 | 21.29 | 1.21 | 1.7% / 0.0% / 0.0% | — | — | 91.3% / 60.8% / 100.0% | — | 0.2% / 1.5% / 0.0% |
| hc | 47,871,778 | 81.65 | 0.73 | 98.2% / 41.0% / 100.0% | — | — | — | — | 0.0% / 2.0% / 0.0% |
| ratio | 40,924,097 | 65.76 | 6.58 | 0.6% / 0.0% / 0.0% | — | — | — | 80.7% / 17.9% / 62.2% | 0.1% / 0.3% / 0.0% |

**Structured set, library build (default)** (`results/libprof-struct.tsv`)

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | HUF c / d / bytes | LZ4 c / d / bytes | Brotli c / d / bytes | LZMA2 c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|---|---|---|---|
| speed | 13,888,171 | 0.14 | 0.07 | 0.2% / 0.0% / 0.0% | — | 61.1% / 22.3% / 99.0% | — | — | 0.4% / 0.7% / 0.0% |
| balanced | 3,181,407 | 0.50 | 0.14 | 15.8% / 12.1% / 93.4% | 0.2% / 0.1% / 2.6% | — | — | — | 0.3% / 0.9% / 0.0% |
| eco | 2,974,328 | 2.04 | 0.21 | 3.6% / 0.0% / 0.0% | — | — | 40.3% / 36.0% / 93.0% | — | 0.2% / 1.2% / 0.0% |
| hc | 2,913,190 | 4.18 | 0.16 | 79.1% / 13.2% / 92.8% | 0.0% / 0.1% / 2.8% | — | — | — | 0.1% / 1.6% / 0.0% |
| ratio | 2,750,996 | 12.49 | 0.39 | 0.5% / 0.0% / 0.0% | — | — | — | 89.6% / 36.7% / 88.1% | 0.0% / 0.7% / 0.0% |

**Silesia, `KOLMO_NATIVE=all` (what is left)** (`results/libprof-silesia-native.tsv`)

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | LZ4 c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|---|
| speed | 93,467,859 | 0.80 | 0.33 | 0.2% / 0.0% / 0.0% | — | 0.4% / 1.4% / 0.0% |
| balanced | 60,160,766 | 1.70 | 0.60 | 5.9% / 0.0% / 0.0% | — | 0.6% / 1.3% / 0.0% |
| eco | 54,704,671 | 15.64 | 0.79 | 2.4% / 0.0% / 0.0% | — | 0.2% / 1.9% / 0.0% |
| hc | 53,754,879 | 38.20 | 0.78 | 1.0% / 0.0% / 0.0% | — | 0.1% / 2.0% / 0.0% |
| ratio | 40,969,885 | 62.83 | 7.85 | 0.5% / 0.0% / 0.0% | — | 0.1% / 0.2% / 0.0% |

**Structured set, `KOLMO_NATIVE=all`** (`results/libprof-struct-native.tsv`)

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | LZ4 c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|---|
| speed | 13,060,561 | 0.16 | 0.08 | 0.3% / 0.0% / 0.0% | — | 0.3% / 0.7% / 0.0% |
| balanced | 3,273,273 | 0.38 | 0.12 | 1.7% / 0.0% / 0.0% | — | 0.3% / 0.9% / 0.0% |
| eco | 3,237,564 | 1.16 | 0.22 | 3.2% / 0.0% / 0.0% | — | 0.3% / 3.2% / 0.0% |
| hc | 3,151,770 | 1.28 | 0.20 | 3.6% / 0.0% / 0.0% | — | 0.3% / 1.4% / 0.0% |
| ratio | 2,753,031 | 9.26 | 0.45 | 0.5% / 0.0% / 0.0% | — | 0.0% / 0.6% / 0.0% |

**Silesia, `KOLMO_NATIVE=all,strong` (eco and hc)** (`results/libprof-silesia-strong.tsv`)

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|
| eco | 45,862,036 | 86.39 | 3.68 | 0.4% / 0.0% / 0.0% | 0.0% / 0.7% / 0.0% |
| hc | 45,348,556 | 102.79 | 3.77 | 0.4% / 0.0% / 0.0% | 0.0% / 0.5% / 0.0% |

**Structured set, `KOLMO_NATIVE=all,strong`** (`results/libprof-struct-strong.tsv`)

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|
| eco | 2,821,898 | 3.86 | 0.32 | 0.9% / 0.0% / 0.0% | 0.1% / 0.8% / 0.0% |
| hc | 2,797,816 | 5.73 | 0.33 | 0.7% / 0.0% / 0.0% | 0.1% / 0.8% / 0.0% |

## 2. Components: native coder against library, same role and blocks

```
sh benchmark/rc3w2/native/build.sh        # build/natbench, build/hashq
sh benchmark/rc3w2/native/run_components.sh SILESIA STRUCT results/components-pass1.txt
sh benchmark/rc3w2/native/run_components.sh SILESIA STRUCT results/components-pass2.txt "speed balanced huf"
python3 benchmark/rc3w2/native/proto_table.py results/components-pass2.txt results/components-pass1.txt
```

How `natbench` measures:
- It cuts each file into blocks, as Kolmo does: 4 MiB for the speed and balanced roles, 16 MiB for eco, hc and ratio.
- It codes the blocks on 1 or 4 workers.
- Compress speed is one pass for the slow coders and the best of 3–5 for the others; decompress is the best of 2–5.
- Every block is decoded and compared.
- A block a coder declines (output not smaller) counts as stored, as Kolmo stores it.

Two passes:
- **Pass 1** (`results/components-pass1.txt`): all roles.
- **Pass 2** (`results/components-pass2.txt`): the speed, balanced and Huffman roles again, after two decoder changes —
  the KLZ fast path for long literal runs and the khuf two-symbol table (which also speeds up KLZH's literals).

Pass-2 rows replace pass-1 rows where both exist. The eco and hc rows for KLZH are from pass 1, so their decode speed
predates the two-symbol table.

"size" is the native size against the library's (negative = native smaller); speeds are native / library.

| role | set | library → native | threads | library size | native size | size | compress | decompress |
|---|---|---|---|---|---|---|---|---|
| speed | silesia | lz4 → klz1 | 1 | 100,934,019 | 93,972,375 | -6.90% | 0.79× (386 / 487 MB/s) | 0.77× (1954 / 2530 MB/s) |
| speed | silesia | lz4 → klz1 | 4 | 100,934,019 | 93,972,375 | -6.90% | 0.76× (1332 / 1745 MB/s) | 0.71× (6141 / 8702 MB/s) |
| speed | silesia | lz4 → klz2 | 1 | 100,934,019 | 90,320,404 | -10.52% | 0.69× (337 / 487 MB/s) | 0.74× (1884 / 2530 MB/s) |
| speed | silesia | lz4 → klz2 | 4 | 100,934,019 | 90,320,404 | -10.52% | 0.73× (1282 / 1745 MB/s) | 0.74× (6424 / 8702 MB/s) |
| speed | struct | lz4 → klz1 | 1 | 14,121,449 | 13,284,807 | -5.92% | 0.90× (303 / 336 MB/s) | 0.73× (1525 / 2084 MB/s) |
| speed | struct | lz4 → klz1 | 4 | 14,121,449 | 13,284,807 | -5.92% | 0.68× (840 / 1228 MB/s) | 0.60× (4264 / 7089 MB/s) |
| speed | struct | lz4 → klz2 | 1 | 14,121,449 | 12,952,330 | -8.28% | 0.84× (282 / 336 MB/s) | 0.61× (1271 / 2084 MB/s) |
| speed | struct | lz4 → klz2 | 4 | 14,121,449 | 12,952,330 | -8.28% | 0.58× (717 / 1228 MB/s) | 0.45× (3184 / 7089 MB/s) |
| balanced | silesia | zstd1 → klzh1 | 1 | 73,302,233 | 69,843,373 | -4.72% | 0.65× (211 / 325 MB/s) | 0.94× (946 / 1005 MB/s) |
| balanced | silesia | zstd1 → klzh1 | 4 | 73,302,233 | 69,843,373 | -4.72% | 0.50× (750 / 1504 MB/s) | 0.87× (2956 / 3395 MB/s) |
| balanced | struct | zstd1 → klzh1 | 1 | 8,064,372 | 8,863,617 | +9.91% | 0.47× (160 / 343 MB/s) | 0.74× (777 / 1056 MB/s) |
| balanced | struct | zstd1 → klzh1 | 4 | 8,064,372 | 8,863,617 | +9.91% | 0.39× (417 / 1068 MB/s) | 0.54× (1730 / 3227 MB/s) |
| balanced | silesia | zstd3 → klzh2 | 1 | 66,360,059 | 68,981,648 | +3.95% | 0.88× (187 / 212 MB/s) | 0.90× (799 / 888 MB/s) |
| balanced | silesia | zstd3 → klzh2 | 4 | 66,360,059 | 68,981,648 | +3.95% | 0.99× (646 / 654 MB/s) | 0.82× (1978 / 2408 MB/s) |
| balanced | silesia | zstd3 → klzh3 | 1 | 66,360,059 | 67,934,827 | +2.37% | 0.76× (161 / 212 MB/s) | 0.88× (784 / 888 MB/s) |
| balanced | silesia | zstd3 → klzh3 | 4 | 66,360,059 | 67,934,827 | +2.37% | 0.84× (548 / 654 MB/s) | 0.99× (2390 / 2408 MB/s) |
| balanced | silesia | zstd3 → klzh5 | 1 | 66,360,059 | 64,516,432 | -2.78% | 0.23× (48.0 / 212 MB/s) | 1.01× (901 / 888 MB/s) |
| balanced | silesia | zstd3 → klzh5 | 4 | 66,360,059 | 64,516,432 | -2.78% | 0.24× (157 / 654 MB/s) | 1.20× (2880 / 2408 MB/s) |
| balanced | struct | zstd3 → klzh2 | 1 | 8,688,819 | 8,963,888 | +3.17% | 0.77× (137 / 177 MB/s) | 0.94× (679 / 721 MB/s) |
| balanced | struct | zstd3 → klzh2 | 4 | 8,688,819 | 8,963,888 | +3.17% | 0.86× (510 / 591 MB/s) | 0.78× (1609 / 2059 MB/s) |
| balanced | struct | zstd3 → klzh3 | 1 | 8,688,819 | 9,020,030 | +3.81% | 0.76× (135 / 177 MB/s) | 0.92× (666 / 721 MB/s) |
| balanced | struct | zstd3 → klzh3 | 4 | 8,688,819 | 9,020,030 | +3.81% | 0.63× (371 / 591 MB/s) | 0.96× (1979 / 2059 MB/s) |
| balanced | struct | zstd3 → klzh5 | 1 | 8,688,819 | 8,546,854 | -1.63% | 0.23× (40.0 / 177 MB/s) | 0.83× (600 / 721 MB/s) |
| balanced | struct | zstd3 → klzh5 | 4 | 8,688,819 | 8,546,854 | -1.63% | 0.24× (143 / 591 MB/s) | 1.05× (2159 / 2059 MB/s) |
| huf | silesia | huf → khuf | 1 | 131,930,366 | 132,141,179 | +0.16% | 0.90× (666 / 740 MB/s) | 0.97× (1036 / 1064 MB/s) |
| huf | silesia | huf → khuf | 4 | 131,930,366 | 132,141,179 | +0.16% | 0.78× (2232 / 2859 MB/s) | 0.69× (2857 / 4131 MB/s) |
| huf | struct | huf → khuf | 1 | 12,812,644 | 12,823,721 | +0.09% | 0.83× (844 / 1021 MB/s) | 0.77× (1180 / 1529 MB/s) |
| huf | struct | huf → khuf | 4 | 12,812,644 | 12,823,721 | +0.09% | 0.66× (2134 / 3247 MB/s) | 0.74× (3913 / 5307 MB/s) |
| leafchunks | kolmo | huf → khuf | 1 | 162,656 | 162,856 | +0.12% | 0.93× (810 / 868 MB/s) | 1.02× (1335 / 1307 MB/s) |
| eco | silesia | brotli8 → klzh7 | 1 | 57,101,231 | 61,784,605 | +8.20% | 1.60× (12.8 / 8.0 MB/s) | 3.04× (858 / 282 MB/s) |
| eco | silesia | brotli8 → klzh7 | 4 | 57,101,231 | 61,784,605 | +8.20% | 1.65× (40.8 / 24.7 MB/s) | 1.99× (2174 / 1090 MB/s) |
| eco | silesia | brotli8 → klzma3 | 1 | 57,101,231 | 50,231,737 | -12.03% | 0.28× (2.2 / 8.0 MB/s) | 0.22× (61.7 / 282 MB/s) |
| eco | silesia | brotli8 → klzma3 | 4 | 57,101,231 | 50,231,737 | -12.03% | 0.30× (7.3 / 24.7 MB/s) | 0.17× (187 / 1090 MB/s) |
| eco | struct | brotli8 → klzh7 | 1 | 7,722,954 | 8,336,708 | +7.95% | 1.67× (11.5 / 6.9 MB/s) | 1.52× (440 / 289 MB/s) |
| eco | struct | brotli8 → klzh7 | 4 | 7,722,954 | 8,336,708 | +7.95% | 2.11× (21.9 / 10.4 MB/s) | 2.45× (1180 / 482 MB/s) |
| eco | struct | brotli8 → klzma3 | 1 | 7,722,954 | 6,342,746 | -17.87% | 0.25× (1.7 / 6.9 MB/s) | 0.20× (58.4 / 289 MB/s) |
| eco | struct | brotli8 → klzma3 | 4 | 7,722,954 | 6,342,746 | -17.87% | 0.27× (2.8 / 10.4 MB/s) | 0.19× (92.5 / 482 MB/s) |
| hc | silesia | zstd19 → klzh8 | 1 | 53,260,623 | 60,549,563 | +13.69% | 2.79× (5.3 / 1.9 MB/s) | 1.08× (647 / 598 MB/s) |
| hc | silesia | zstd19 → klzh8 | 4 | 53,260,623 | 60,549,563 | +13.69% | 2.58× (17.8 / 6.9 MB/s) | 0.84× (2063 / 2444 MB/s) |
| hc | silesia | zstd19 → klzma6 | 1 | 53,260,623 | 49,331,820 | -7.38% | 0.89× (1.7 / 1.9 MB/s) | 0.11× (66.7 / 598 MB/s) |
| hc | silesia | zstd19 → klzma6 | 4 | 53,260,623 | 49,331,820 | -7.38% | 0.86× (5.9 / 6.9 MB/s) | 0.09× (216 / 2444 MB/s) |
| hc | struct | zstd19 → klzh8 | 1 | 6,770,232 | 8,179,534 | +20.82% | 2.43× (3.4 / 1.4 MB/s) | 0.92× (467 / 506 MB/s) |
| hc | struct | zstd19 → klzh8 | 4 | 6,770,232 | 8,179,534 | +20.82% | 2.78× (6.4 / 2.3 MB/s) | 0.83× (1116 / 1346 MB/s) |
| hc | struct | zstd19 → klzma6 | 1 | 6,770,232 | 6,283,751 | -7.19% | 0.93× (1.3 / 1.4 MB/s) | 0.12× (58.8 / 506 MB/s) |
| hc | struct | zstd19 → klzma6 | 4 | 6,770,232 | 6,283,751 | -7.19% | 0.91× (2.1 / 2.3 MB/s) | 0.08× (103 / 1346 MB/s) |
| ratio | silesia | lzma9e → klzma8 | 1 | 49,186,892 | 49,226,023 | +0.08% | 1.14× (1.6 / 1.4 MB/s) | 0.76× (67.7 / 89.5 MB/s) |
| ratio | silesia | lzma9e → klzma8 | 4 | 49,186,892 | 49,226,023 | +0.08% | 1.09× (5.8 / 5.3 MB/s) | 0.70× (205 / 293 MB/s) |
| ratio | struct | lzma9e → klzma8 | 1 | 6,229,239 | 6,248,718 | +0.31% | 1.33× (1.2 / 0.9 MB/s) | 0.72× (60.5 / 84.4 MB/s) |
| ratio | struct | lzma9e → klzma8 | 4 | 6,229,239 | 6,248,718 | +0.31% | 1.25× (2.0 / 1.6 MB/s) | 0.83× (104 / 124 MB/s) |

`leafchunks` are the 342,144 bytes of Huffman leaf chunks that Kolmo's own `balanced` and `hc` frames of both sets
contain. The profiler dumped them with `LIBPROF_DUMP_HUF`, and they are coded one chunk per block.

### KLZMA's parse, step by step (6 files, 56.4 MB: two OWID CSVs, sao, osdb, x-ray, ooffice; 16 MiB blocks, 4 threads)

| version | size | against xz -9e (19,434,243) | compress |
|---|---|---|---|
| hash chains, depth 256 | — | +2.1% on 3 MB of dickens | 0.3 MB/s on dickens (0.2× xz) |
| binary-tree match finder, level 6 | 19,505,596 | +0.37% | 4.3 MB/s (1.5×) |
| levels 7 / 8 / 9 | 19,483,392 / 19,467,389 / 19,470,260 | +0.25% / +0.17% / +0.19% | 3.9 / 3.7 / 3.5 MB/s |
| + "literal then repeat 0" in the parse, level 6 | 19,477,336 | +0.22% | 4.3 MB/s |
| same, level 8 (the `ratio` mapping) | 19,444,004 | **+0.05%** | 3.8 MB/s (1.3×) |

On all of Silesia (16 MiB blocks, 4 threads), the binary-tree level 6 without the new transition was 49,360,504 bytes
against xz's 49,186,892 (+0.35%), and compressed 1.39× faster. Level 8 with it is +0.08% (table above).

Decode, step by step:
- First version: 0.6–0.74× of liblzma 5.8.1.
- Branch-free tree bits: 0.89× on text.
- Branch-free matched literals: +9% on text, nothing on binary data.

liblzma 5.6+ decodes bits with inline x86-64 assembly. A kernel of Kolmo's own is the next step (plan item 2).

## 3. End to end: Kolmo with and without the libraries (the same planner)

The census runs above give the 1-thread figures (output bytes and times of the `_total_` rows). For 4 threads:

```
python3 benchmark/rc3w2/native/e2e.py build/kolmo DATA 4 results/e2e-T4.tsv all speed balanced eco hc ratio
python3 benchmark/rc3w2/native/e2e.py build/kolmo DATA 4 results/e2e-T4.tsv all,strong eco hc
```

Each figure is the sum over the files of process wall time, the best of 2 per file and phase. The decoder runs with no
opt-in.

| set | mode | threads | `KOLMO_NATIVE` | output, library | output, native | size | compress s (library → native) | decompress s (library → native) |
|---|---|---|---|---|---|---|---|---|
| Silesia | speed | 1 | all | 99,938,346 | 93,467,859 | -6.47% | 0.69 → 0.80 (1.16×) | 0.36 → 0.33 (0.91×) |
| Silesia | speed | 4 | all | 99,938,346 | 93,467,859 | -6.47% | 0.28 → 0.31 (1.10×) | 0.18 → 0.20 (1.14×) |
| Silesia | balanced | 1 | all | 58,487,839 | 60,160,766 | +2.86% | 1.58 → 1.70 (1.08×) | 0.64 → 0.60 (0.94×) |
| Silesia | balanced | 4 | all | 58,487,839 | 60,160,766 | +2.86% | 0.60 → 0.89 (1.48×) | 0.27 → 0.28 (1.03×) |
| Silesia | eco | 1 | all | 50,773,162 | 54,704,671 | +7.74% | 21.29 → 15.64 (0.73×) | 1.21 → 0.79 (0.65×) |
| Silesia | eco | 4 | all | 50,773,162 | 54,704,671 | +7.74% | 12.14 → 9.70 (0.80×) | 0.76 → 0.52 (0.69×) |
| Silesia | eco | 1 | all,strong | 50,773,162 | 45,862,036 | -9.67% | 21.29 → 86.39 (4.06×) | 1.21 → 3.68 (3.04×) |
| Silesia | eco | 4 | all,strong | 50,773,162 | 45,862,036 | -9.67% | 12.14 → 49.96 (4.11×) | 0.76 → 2.30 (3.02×) |
| Silesia | hc | 1 | all | 47,871,778 | 53,754,879 | +12.29% | 81.65 → 38.20 (0.47×) | 0.73 → 0.78 (1.07×) |
| Silesia | hc | 4 | all | 47,871,778 | 53,754,879 | +12.29% | 44.28 → 23.67 (0.53×) | 0.52 → 0.53 (1.02×) |
| Silesia | hc | 1 | all,strong | 47,871,778 | 45,348,556 | -5.27% | 81.65 → 102.79 (1.26×) | 0.73 → 3.77 (5.13×) |
| Silesia | hc | 4 | all,strong | 47,871,778 | 45,348,556 | -5.27% | 44.28 → 53.97 (1.22×) | 0.52 → 2.23 (4.32×) |
| Silesia | ratio | 1 | all | 40,924,097 | 40,969,885 | +0.11% | 65.76 → 62.83 (0.96×) | 6.58 → 7.85 (1.19×) |
| Silesia | ratio | 4 | all | 40,924,097 | 40,969,885 | +0.11% | 40.29 → 38.07 (0.94×) | 3.16 → 3.62 (1.15×) |
| structured | speed | 1 | all | 13,888,171 | 13,060,561 | -5.96% | 0.14 → 0.16 (1.14×) | 0.07 → 0.08 (1.14×) |
| structured | speed | 4 | all | 13,888,171 | 13,060,561 | -5.96% | 0.08 → 0.07 (0.96×) | 0.05 → 0.05 (0.94×) |
| structured | balanced | 1 | all | 3,181,407 | 3,273,273 | +2.89% | 0.50 → 0.38 (0.75×) | 0.14 → 0.12 (0.85×) |
| structured | balanced | 4 | all | 3,181,407 | 3,273,273 | +2.89% | 0.21 → 0.24 (1.14×) | 0.07 → 0.08 (1.18×) |
| structured | eco | 1 | all | 2,974,328 | 3,237,564 | +8.85% | 2.04 → 1.16 (0.57×) | 0.21 → 0.22 (1.04×) |
| structured | eco | 4 | all | 2,974,328 | 3,237,564 | +8.85% | 1.18 → 0.88 (0.75×) | 0.17 → 0.15 (0.85×) |
| structured | eco | 1 | all,strong | 2,974,328 | 2,821,898 | -5.12% | 2.04 → 3.86 (1.89×) | 0.21 → 0.32 (1.49×) |
| structured | eco | 4 | all,strong | 2,974,328 | 2,821,898 | -5.12% | 1.18 → 3.26 (2.78×) | 0.17 → 0.29 (1.67×) |
| structured | hc | 1 | all | 2,913,190 | 3,151,770 | +8.19% | 4.18 → 1.28 (0.31×) | 0.16 → 0.20 (1.21×) |
| structured | hc | 4 | all | 2,913,190 | 3,151,770 | +8.19% | 3.44 → 1.04 (0.30×) | 0.15 → 0.13 (0.85×) |
| structured | hc | 1 | all,strong | 2,913,190 | 2,797,816 | -3.96% | 4.18 → 5.73 (1.37×) | 0.16 → 0.33 (2.03×) |
| structured | hc | 4 | all,strong | 2,913,190 | 2,797,816 | -3.96% | 3.44 → 5.03 (1.46×) | 0.15 → 0.29 (1.92×) |
| structured | ratio | 1 | all | 2,750,996 | 2,753,031 | +0.07% | 12.49 → 9.26 (0.74×) | 0.39 → 0.45 (1.15×) |
| structured | ratio | 4 | all | 2,750,996 | 2,753,031 | +0.07% | 10.54 → 7.35 (0.70×) | 0.30 → 0.35 (1.17×) |

## 4. khash64 against XXH3-64

`build/hashq /path/to/webster` (`results/hashq.txt`).

Quality: both hashes, on the full 64 bits and on the low 32 bits the block header stores.

```
avalanche (worst bias over input bit x output bit; 4000 keys each; noise ~0.025):
  len     3:  xxh3 0.0292  khash 0.0373
  len     8:  xxh3 0.0277  khash 0.0312
  len    16:  xxh3 0.0302  khash 0.0292
  len    24:  xxh3 0.0297  khash 0.0325
  len    40:  xxh3 0.0325  khash 0.0318
  len    64:  xxh3 0.0300  khash 0.0337
  len    65:  xxh3 0.0375  khash 0.0380
  len   200:  xxh3 0.0325  khash 0.0353
  len  1024:  xxh3 0.0367  khash 0.0353
  len  4100:  xxh3 0.0317  khash 0.0368
collisions:
  xxh3   counter in a 4 KiB zero block      keys   4194304  collisions 64-bit 0  low 32-bit   2035 (expected 2048.0)
  xxh3   1 and 2 bits set in 256 zero bytes keys   2098176  collisions 64-bit 0  low 32-bit    546 (expected 512.5)
  xxh3   zero blocks of length 0..65536     keys     65537  collisions 64-bit 0  low 32-bit      2 (expected 0.5)
  xxh3   distinct lines of the text file (*) keys    657657  collisions 64-bit 0  low 32-bit     48 (expected 50.4)
  khash  counter in a 4 KiB zero block      keys   4194304  collisions 64-bit 0  low 32-bit   2060 (expected 2048.0)
  khash  1 and 2 bits set in 256 zero bytes keys   2098176  collisions 64-bit 0  low 32-bit    541 (expected 512.5)
  khash  zero blocks of length 0..65536     keys     65537  collisions 64-bit 0  low 32-bit      1 (expected 0.5)
  khash  distinct lines of the text file (*) keys    657657  collisions 64-bit 0  low 32-bit     46 (expected 50.4)
corruption of 64 KiB random blocks (low 32 bits unchanged = undetected), 200000 trials each:
  xxh3   undetected: 1 bit 0, 2 bits 0, 3 bits 0, 8 bits 0, zeroed range 0 (expected ~0.000 each)
  khash  undetected: 1 bit 0, 2 bits 0, 3 bits 0, 8 bits 0, zeroed range 0 (expected ~0.000 each)
```

Speed (best of 7, Kolmo's checksum seed). These figures are from the default `-march=native` (AVX-512) build; XXH3 uses
its AVX-512 kernel there.

| input | XXH3 | khash64 | khash64 / XXH3 |
|---|---|---|---|
| 16 B | 5.9 GB/s | 4.4 GB/s | 0.73× |
| 64 B | 11.9 GB/s | 12.3 GB/s | 1.03× |
| 256 B | 10.5 GB/s | 7.6 GB/s | 0.73× |
| 1 KiB | 30.7 GB/s | 14.6 GB/s | 0.48× |
| 4 KiB | 43.7 GB/s | 24.0 GB/s | 0.55× |
| 64 KiB | 55.7 GB/s | 28.3 GB/s | 0.51× |
| 1 MiB | 59.8 GB/s | 26.0 GB/s | 0.43× |
| 64 MiB | 28.3 GB/s | 22.8 GB/s | 0.80× |

On a baseline x86-64 build (`-O2`, release binaries), XXH3 dispatches to its AVX2 kernel at run time and khash64 does
not:

| input | XXH3 | khash64 |
|---|---|---|
| 4 KiB | 28.1 GB/s | 8.8 GB/s |
| 1 MiB | 23.1 GB/s | 9.0 GB/s |
| 64 MiB | 19.7 GB/s | 5.3 GB/s |

With `-mavx2` (what a dispatched AVX2 kernel would give), khash64 reaches 14–15.5 GB/s.

In Kolmo, checksums are 0.5–2% of decode time, so khash64 as it stands would add up to 1–2% to decode time. Its
AVX2/NEON kernels are plan item 1.

## 5. Tests

| test | result |
|---|---|
| `make check-native` (part of `make check-rc`) | OK. The round trips, corrupt-payload decodes under ASan/UBSan, complete Huffman codes and pinned khash64 values. Every mode through the CLI with `KOLMO_NATIVE=all` and `all,strong`, at 1 and 4 threads: decoded with no opt-in, identical frames at 1 and 4 threads |
| `make check` | TEST_CHECK |
| `make check-rc` | TEST_CHECKRC |
| `make check-sanitizers` | TEST_SAN |
| `make experimental-check GPU_CHECK=` | TEST_EXP |
| `make kfam-check` | TEST_KFAM |
| determinism (`benchmark/determinism/check.py`) | TEST_DET |

## What did not work, and other notes

- **khash64, first version: 1.56 million collisions** on 2.1 million keys with one or two set bits in 256 bytes.
  - Cause: one key word per lane, so swapping the words of two stripes in the same lane gave the same sum.
  - Fix: the key now rotates per stripe. Collisions are at the birthday expectation since.
  - The rotation first cost 60% of the speed, because a rotating index does not vectorise. Contiguous key windows
    (the key array repeated) restored it.
  - GCC with AVX-512 turns the 32×32→64 product into `vpmullq`. Writing it with generic vector types gave
    `vpmuludq`-class speed.
- **khuf: a per-stream worst-case bound** refused chunks that would have coded (+0.2% size). Replaced by one exact bound
  from the total bit count.
- **khuf: the two-symbol table, first version** decoded wrongly. The one-symbol loop that follows it assumed the four
  streams move in lockstep. It now tests every stream's room.
- **KLZMA: a decoder macro** used the same local name as its caller (`b_`). The first decode failed; renamed.
- **KLZMA: hash chains of depth 256** made level 9 run at 0.3 MB/s. The binary-tree finder finds better matches at
  5× the speed (and 2.2% smaller: 870,422 against 889,557 bytes on 3 MB of dickens).
- **KLZMA levels above 8** gain nothing: level 9 is 0.01% larger than level 8 on the 6-file set.
- **KLZH: the first parse** (generic chains for all levels) compressed at 136 MB/s at level 1. The two-table fast parse
  reaches 189–213 MB/s, and is smaller (−4.8% against the first version on 4 Silesia files).
- **KLZH: a 6-byte minimum match** in the fast parse is 3.2% smaller on the structured set and 0.8% larger on Silesia.
  4 is kept; a per-block choice is a tuning item.
- **KLZH against zstd -1 on the structured set: +9.9%,** while it is −4.7% on Silesia. The CSV files favour zstd's
  longer minimum match and its local entropy tables (above).
- **The opt-in replaces the coder, not the plan.** The planner's decisions (zstd probes, thresholds tuned for zstd) stay
  the same, so end-to-end figures are the native coder inside a plan made for the library.
- **`strong` maps every zstd call of the process to KLZMA.** Ids are written where the level is not known. Use it for
  `eco` and `hc` runs only.
- **Codec 22 (Huffman then zstd)** is not written under `KOLMO_NATIVE=lz`; its inner coder is zstd.
