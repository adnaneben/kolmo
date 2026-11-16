# SIMD paths, runtime dispatch and assembly (1.2.0-rc2, x86-64)

16 November 2026. Branch `claude/simd-dispatch-20261116`, made from `claude/release-rc2-20261115` at `9d4496a`.
The code change is in commit `fc9a356`. Scripts are in `benchmark/rc2/simd/`, raw runs in `benchmark/rc2/simd/results/runs.jsonl`.

## Summary

- **The missing SIMD paths cost little, except in one place.** Release builds use `ARCH=`, which compiles for baseline
  x86-64. Kolmo's compile-time AVX2/SSE4.2 code is the AVX2 decimal formatter (`fmt_slots`, `fmt_run`) and the
  SSE4.2 CRC32C. The formatter matters: generic builds decode numeric CSV **22–27 % slower** than `-mavx2` builds
  (`-m balanced` and `-m hc`, 1 and 4 threads; fastest of 15). CRC32C only covers frame headers (28 bytes plus the
  index), so hardware CRC32C saves nothing measurable. Across all 4 inputs × 4 modes the geometric mean of
  `-mavx2`/generic time is 0.98 for compression and 0.93–0.95 for decompression, and most of the decompression gain
  is the CSV case. `-march=native` (AVX-512 on this CPU) adds nothing beyond `-mavx2`.
- **Runtime dispatch closes that gap.** Each AVX2/SSE4.2 body is now compiled with `__attribute__((target(...)))` and
  chosen at first use with `__builtin_cpu_supports`. A generic (`ARCH=`) build decodes CSV 19–23 % faster than
  before, within 2–4 % of the `-mavx2` build. XXH3, the block checksum, also uses xxHash's AVX2 kernel on long inputs.
  `-DKOLMO_NO_DISPATCH` restores compile-time selection.
- **New SSE2/AVX2 integer inverse.** In the profile, the hottest Kolmo-owned loop is the inverse for 32-bit integer
  arrays (`fi_inv_4_2_0` with its inlined `kg_unzz`, about 27 % of typed-array decode). It was rewritten as
  in-register prefix sums: SSE2 on baseline x86-64, AVX2 by CPUID. The loop alone runs 1.8× faster on 16 M values
  and up to 3.5× faster in cache. End to end, `-m balanced` decode of a 64 MiB int32 array is 18 % faster in the
  dispatched build; the `-mavx2` build without the new loop gains only 4–6 %.
- **Output bytes are unchanged.** All 32 (input, mode, threads) cases give byte-identical frames from the generic,
  `-msse4.2 -mavx2 -mbmi2 -mfma`, `-march=native` and dispatched builds, and every round trip is exact. `-m eco`
  frames are also identical across 5 builds (those 4 plus dispatched `-march=native`). There are new tests: `make
  check-dispatch` checks CRC32C and XXH3 against scalar references; `test_fastint` already compares the new
  inverse with the reference inverse on 1.1 M programs, and both vector paths are hit. `make check`,
  `check-sanitizers` and `check-rc` pass.
- **Hand-written assembly would add little.** The new inverse loop has no spills, is about 40 instructions per 16
  values, and reaches memory bandwidth on large arrays. Assembly could gain at most a few percent in cache and
  nothing out of cache. Most of Kolmo's remaining time is in vendored zstd (already dispatched with BMI2, with its own
  x86-64 Huffman assembly), in the kernel (`pread`/`pwrite`, page faults), or in memory-bound or latency-bound code:
  the context-mixing model, MED image prediction, the `lines` scatter and histograms. Assembly does not help there.

## Kolmo options and their backends

Every option this report mentions, with the backend codec it runs and what Kolmo adds or changes on top. "This
branch" means `claude/simd-dispatch-20261116`; "rc2" means the 1.2.0-rc2 changes in `CHANGELOG.md`. All modes share
the container: independent blocks, XXH3-64 per block (this branch: xxHash's AVX2 kernel chosen at run time) and
CRC32C over the header and index (this branch: SSE4.2 instruction chosen at run time).

| option | backend | Kolmo on top (transforms, choices, tweaks) |
|---|---|---|
| `-m speed` (= `-m rt -Q`) | LZ4 1.x, `LZ4_compress_fast`, acceleration 1 | adaptive blocks (~8 per input, ≤4 MiB); minimal analysis (`-Q`): the table and log transforms are dropped, only cheap ones kept; per-block XXH3 recorded in the index |
| `-m balanced` (default, = `-m fast`) | zstd 1.5.x level 3 (`CZ_FAST_LEVEL`) | adaptive blocks; structure transforms (typed table columns with delta/entity/delta-of-delta/third-order/formula predictors, log templates, fixed-width lines, JSON split, decimal doubles, fixed records, 16-bit images, x86 branch transform, per-member tar models); integer arrays go to the integer graph (predicted, zig-zag residuals in byte planes; each leaf coded by the mode's coder, Huffman, or in the slower modes rANS order 0, whichever is smallest). rc2: faster integer forward path (one-sweep order choice, no division when the factor is 1; same bytes). This branch: decode of numeric CSV uses the AVX2 decimal formatter by CPUID, and 32-bit integer arrays use the new SSE2/AVX2 prefix-sum inverse (same bytes) |
| `--native-leaf` (with balanced) | Kolmo's plane-context leaves instead of zstd/Huffman for integer residuals | container version 9; files that do not use it are unchanged |
| `--typed-huf` (with `-m fast -T1`) | per-plane Huffman on integer NPY and PCM16 WAV under 8 MiB, coded type first | rc2: WAV clips with long repeats get zstd-3 leaves instead of Huffman |
| `-m eco` | Brotli quality 8 (`CZ_ECO_LEVEL`), window 2^24, `BROTLI_MODE_GENERIC` | 16 MiB blocks; the transforms above plus word codes (frequent words become 2-byte codes from unused byte values). This branch: Brotli and all other vendored code compiled with `-ffp-contract=off`, so its float cost model never uses FMA |
| `-m hc` | zstd level 19; text after the word transform: zstd level 22 (`CZ_HC_TEXT_LEVEL`) with one 128 MiB window | 16 MiB blocks; transforms as in eco |
| `-m ratio` (= `-m max`) | LZMA2 (liblzma, preset 9 + EXTREME, dictionary the next power of two above the block, at most 64 MiB, raw LZMA2 with stored lc/lp/pb) for binary blocks; BWT from libbsc (LZP hash 15, min length 72, BWT sorter, static QLFC coder, fast mode; XXH32 guard) for text and JSON token streams | 128 MiB blocks; binary blocks switch to BWT when it is at least 3 % smaller (`KOLMO_BWT_GAIN_PCT`); `-l 1`: BWT for every block |
| `-x` (= `-m ultra`) | Kolmo's own context-mixing coder (orders 1–7, words, columns, indirect contexts, bit histories, neural mixing, APMs; model 3 on text, model 2 otherwise), an image model for 8-bit images | 128 MiB blocks; best per block of CM, LZMA2 and BWT. rc2: the squash table, its inverse and the arithmetic coder were rewritten from their definitions (bitstream unchanged). `-x -l 1`: model 1 only; `-x -l 2`: integer-graph leaves may use CM too |
| `-T n` | — | threads; blocks are independent, frames are the same for every `n` (checked in this study) |

In the tables below, `speed` = LZ4, `balanced` = zstd 3 + transforms, `hc` = zstd 19/22 + transforms,
`ratio` = LZMA2/BWT + transforms, as in this table.

## Setup

| | |
|---|---|
| Machine | KVM guest, Intel Xeon @ 2.10 GHz, 4 vCPUs (AVX2, BMI2, FMA, AVX-512F/BW/VL/VBMI), 15 GiB, Linux 6.18 |
| Compilers | gcc 13.3.0 (all builds); clang 18.1.3 (compile check of the dispatch code) |
| Builds | `generic` = `make ARCH=` (release flags); `avx2` = `make ARCH="-msse4.2 -mavx2 -mbmi2 -mfma"`; `native` = `make ARCH=-march=native`, all three at `9d4496a`; `dispatch` = `make ARCH=` at `fc9a356` |
| Timing | whole `kolmo` processes (`run_simd.py`): wall time via `perf_counter` around `posix_spawn`/`wait4`; builds rotate every round; 5 rounds, fastest kept; output file deleted before each run, outside the timing |
| Steal | read from `/proc/stat` around every run: mean 0.54 % of CPU time, max 12.7 %, more than 1 % in 174 of 1,280 runs |
| Threads | `-T1` and `-T4` (all vCPUs) |

### Inputs

| file | bytes | SHA-256 | source |
|---|---:|---|---|
| silesia.tar | 211,948,032 | `e0c4982e7446cf3d6ded2d5f4605a3043869d19b690b0fe680d66a09657b4225` | the 12 files of https://sun.aei.polsl.pl/~sdeor/corpus/silesia.zip in alphabetical order; `tar -b1 --format=ustar --owner=0 --group=0 --mtime=2000-01-01` |
| trades.csv | 53,908,132 | `669db8a5cfbfc3dd0c60d66e5835c3041eba7ba7d9b87af0c74daf9f7c9e0fa4` | generated by `simd/mk_inputs.py` (fixed seed): 1 M rows, 7 columns incl. 2/3/4-decimal values |
| events.ndjson | 75,510,157 | `3c62889207f9a4d1e9cc79df0495a5c416f9d1ad12e36a4bd1e24f0239e8b091` | generated by `simd/mk_inputs.py`: 600 k log-like JSON records |
| walk_i32.npy | 67,108,992 | `372880e50c64932c48b4954ee1632e174f47ecf39b847b87ae31c627dcf53990` | generated by `simd/mk_inputs.py`: 2^24-value int32 random walk (typed integer array) |

## 1. Cost of the compiled-out paths

Where the AVX2/SSE4.2 code sits (`src/kolmo.c`, before this change):

| path | gate | where it runs | weight |
|---|---|---|---|
| CRC32C (`_mm_crc32_u64`) | `__SSE4_2__` | frame header and index checksum (`kolmo.c` ~3108, 3130; `typed_int_encoder.inc`) | negligible: 28 bytes plus the index per frame |
| decimal formatter, 4 values per step (`fmt_slots`, `fmt_run`) | `__AVX2__` | table decoder (`tbl2.inc` 853, 1090): numeric columns of CSV/TSV | the CSV decode gap below |
| XXH3 long-input kernel (vendored `xxhash.h`) | `__AVX2__` (`XXH_VECTOR`) | block checksums (`kolmo_xxh3`) | 2–5 % of decode in the profile |
| everything else | the compiler's own auto-vectorisation at 256 bits | — | no consistent gain measured |

zstd already selects its BMI2 entropy code at run time (`DYNAMIC_BMI2`) and uses `huf_decompress_amd64.S` in every
build, so its fast paths are not missing from generic builds.

### Byte identity and FMA

Kolmo compiles `kolmo.c` with `-ffp-contract=off`. Kolmo's own object has **0 FMA instructions** in the `avx2` and
`native` builds (`objdump -d build/kolmo.o | grep vfmadd`). At `9d4496a` and `fc9a356` the vendored sources did
*not* get `-ffp-contract=off`. With `-mfma` or `-march=native`, 201 FMA instructions appeared in the **Brotli encoder's** float cost model
(`WriteMetaBlockInternal`, `BrotliPopulationCost*`, `BlockSplitterFinishBlock*`, `BrotliCompareAndPushToQueue*`,
`ShouldCompress`, `ShouldMergeBlock`). Kolmo uses Brotli only in `-m eco`. Eco frames of all four inputs were
identical across 5 builds, but no flag guarantees that, so this is a risk for local `-march=native` builds (the
Makefile default) and for arm64, where contraction is on by default. **Fixed since:** every C and C++ file of every
build is now compiled with `-ffp-contract=off` (section 5).

Frames of the timed runs (round 1; every build, every case):

| input | mode | T | size (bytes) | SHA-256 (first 16) | builds agreeing |
|---|---|---:|---:|---|---:|
| input | mode | T | size (bytes) | SHA-256 (first 16) | builds agreeing |
|---|---|---:|---:|---|---:|
| silesia.tar | speed (LZ4) | 1 | 99,918,100 | `88007657ae0a774f` | 4/4 |
| silesia.tar | speed (LZ4) | 4 | 99,918,100 | `88007657ae0a774f` | 4/4 |
| silesia.tar | balanced (zstd 3) | 1 | 58,245,943 | `43ef81d7fc5da90e` | 4/4 |
| silesia.tar | balanced (zstd 3) | 4 | 58,245,943 | `43ef81d7fc5da90e` | 4/4 |
| silesia.tar | hc (zstd 19/22) | 1 | 47,624,828 | `2342befaad9eb08e` | 4/4 |
| silesia.tar | hc (zstd 19/22) | 4 | 47,624,828 | `2342befaad9eb08e` | 4/4 |
| silesia.tar | ratio (LZMA2/BWT) | 1 | 40,687,857 | `649ccb3c09a84e36` | 4/4 |
| silesia.tar | ratio (LZMA2/BWT) | 4 | 40,687,857 | `649ccb3c09a84e36` | 4/4 |
| trades.csv | speed (LZ4) | 1 | 36,817,721 | `849c8ec4a1840aa6` | 4/4 |
| trades.csv | speed (LZ4) | 4 | 36,817,721 | `849c8ec4a1840aa6` | 4/4 |
| trades.csv | balanced (zstd 3) | 1 | 12,085,484 | `f4d6fc81bbd9fca6` | 4/4 |
| trades.csv | balanced (zstd 3) | 4 | 12,085,484 | `f4d6fc81bbd9fca6` | 4/4 |
| trades.csv | hc (zstd 19/22) | 1 | 11,782,261 | `7a690f0d706ac084` | 4/4 |
| trades.csv | hc (zstd 19/22) | 4 | 11,782,261 | `7a690f0d706ac084` | 4/4 |
| trades.csv | ratio (LZMA2/BWT) | 1 | 11,615,464 | `ec4d6ac51681e842` | 4/4 |
| trades.csv | ratio (LZMA2/BWT) | 4 | 11,615,464 | `ec4d6ac51681e842` | 4/4 |
| events.ndjson | speed (LZ4) | 1 | 19,727,354 | `59041f724700f1f8` | 4/4 |
| events.ndjson | speed (LZ4) | 4 | 19,727,354 | `59041f724700f1f8` | 4/4 |
| events.ndjson | balanced (zstd 3) | 1 | 6,770,194 | `a2eeaf90fe08beb7` | 4/4 |
| events.ndjson | balanced (zstd 3) | 4 | 6,770,194 | `a2eeaf90fe08beb7` | 4/4 |
| events.ndjson | hc (zstd 19/22) | 1 | 5,178,170 | `a47ce72a4fb6a5af` | 4/4 |
| events.ndjson | hc (zstd 19/22) | 4 | 5,178,170 | `a47ce72a4fb6a5af` | 4/4 |
| events.ndjson | ratio (LZMA2/BWT) | 1 | 4,252,578 | `d9a0b7c3b127b8c3` | 4/4 |
| events.ndjson | ratio (LZMA2/BWT) | 4 | 4,252,578 | `d9a0b7c3b127b8c3` | 4/4 |
| walk_i32.npy | speed (LZ4) | 1 | 67,107,281 | `dc0bd9071ff4bcf2` | 4/4 |
| walk_i32.npy | speed (LZ4) | 4 | 67,107,281 | `dc0bd9071ff4bcf2` | 4/4 |
| walk_i32.npy | balanced (zstd 3) | 1 | 26,541,943 | `2bcea0bf21426717` | 4/4 |
| walk_i32.npy | balanced (zstd 3) | 4 | 26,541,943 | `2bcea0bf21426717` | 4/4 |
| walk_i32.npy | hc (zstd 19/22) | 1 | 26,541,311 | `8601ab034763a27f` | 4/4 |
| walk_i32.npy | hc (zstd 19/22) | 4 | 26,541,311 | `8601ab034763a27f` | 4/4 |
| walk_i32.npy | ratio (LZMA2/BWT) | 1 | 26,377,410 | `5645d7d50063f32b` | 4/4 |
| walk_i32.npy | ratio (LZMA2/BWT) | 4 | 26,377,410 | `5645d7d50063f32b` | 4/4 |

### Timing: compression

| input | mode | T | generic | avx2 | native | dispatch | avx2/generic | native/generic | dispatch/generic |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| silesia.tar | speed (LZ4) | 1 | 416.3 | 419.4 | 409.5 | 401.7 | 0.993 | 1.017 | 1.036 |
| silesia.tar | speed (LZ4) | 4 | 1407.4 | 1445.7 | 1413.9 | 1373.9 | 0.974 | 0.995 | 1.024 |
| silesia.tar | balanced (zstd 3) | 1 | 175.8 | 183.8 | 190.8 | 192.0 | 0.957 | 0.921 | 0.916 |
| silesia.tar | balanced (zstd 3) | 4 | 629.6 | 641.6 | 638.0 | 652.7 | 0.981 | 0.987 | 0.965 |
| silesia.tar | hc (zstd 19/22) | 1 | 2.6 | 2.6 | 2.5 | 2.5 | 1.015 | 1.039 | 1.057 |
| silesia.tar | hc (zstd 19/22) | 4 | 8.2 | 8.4 | 8.5 | 8.7 | 0.984 | 0.972 | 0.947 |
| silesia.tar | ratio (LZMA2/BWT) | 1 | 2.8 | 2.7 | 2.8 | 2.6 | 1.015 | 0.990 | 1.050 |
| silesia.tar | ratio (LZMA2/BWT) | 4 | 5.6 | 5.6 | 5.9 | 6.0 | 1.008 | 0.950 | 0.934 |
| trades.csv | speed (LZ4) | 1 | 325.2 | 352.4 | 357.2 | 340.2 | 0.923 | 0.910 | 0.956 |
| trades.csv | speed (LZ4) | 4 | 1017.2 | 1048.1 | 1047.9 | 1008.5 | 0.971 | 0.971 | 1.009 |
| trades.csv | balanced (zstd 3) | 1 | 180.4 | 199.8 | 197.5 | 178.0 | 0.903 | 0.913 | 1.014 |
| trades.csv | balanced (zstd 3) | 4 | 454.9 | 521.8 | 555.5 | 462.1 | 0.872 | 0.819 | 0.984 |
| trades.csv | hc (zstd 19/22) | 1 | 9.0 | 9.0 | 9.0 | 8.7 | 1.003 | 1.003 | 1.043 |
| trades.csv | hc (zstd 19/22) | 4 | 26.9 | 27.1 | 24.8 | 24.9 | 0.992 | 1.085 | 1.077 |
| trades.csv | ratio (LZMA2/BWT) | 1 | 4.6 | 4.6 | 4.7 | 4.5 | 0.989 | 0.968 | 1.017 |
| trades.csv | ratio (LZMA2/BWT) | 4 | 4.6 | 4.5 | 4.4 | 4.6 | 1.011 | 1.025 | 0.983 |
| events.ndjson | speed (LZ4) | 1 | 478.5 | 495.7 | 507.3 | 499.7 | 0.965 | 0.943 | 0.958 |
| events.ndjson | speed (LZ4) | 4 | 1536.2 | 1565.7 | 1631.2 | 1605.7 | 0.981 | 0.942 | 0.957 |
| events.ndjson | balanced (zstd 3) | 1 | 180.3 | 179.7 | 188.9 | 177.5 | 1.003 | 0.954 | 1.015 |
| events.ndjson | balanced (zstd 3) | 4 | 574.9 | 532.4 | 591.8 | 547.5 | 1.080 | 0.971 | 1.050 |
| events.ndjson | hc (zstd 19/22) | 1 | 5.1 | 5.1 | 5.0 | 4.9 | 1.002 | 1.021 | 1.037 |
| events.ndjson | hc (zstd 19/22) | 4 | 14.5 | 15.0 | 14.9 | 14.6 | 0.972 | 0.975 | 1.000 |
| events.ndjson | ratio (LZMA2/BWT) | 1 | 39.8 | 39.2 | 42.3 | 40.0 | 1.014 | 0.942 | 0.994 |
| events.ndjson | ratio (LZMA2/BWT) | 4 | 38.7 | 38.7 | 39.3 | 39.4 | 1.002 | 0.986 | 0.984 |
| walk_i32.npy | speed (LZ4) | 1 | 916.5 | 943.9 | 916.8 | 845.1 | 0.971 | 1.000 | 1.084 |
| walk_i32.npy | speed (LZ4) | 4 | 2025.8 | 2212.2 | 1716.4 | 1797.0 | 0.916 | 1.180 | 1.127 |
| walk_i32.npy | balanced (zstd 3) | 1 | 302.5 | 304.1 | 319.8 | 318.6 | 0.995 | 0.946 | 0.950 |
| walk_i32.npy | balanced (zstd 3) | 4 | 897.5 | 715.4 | 870.9 | 918.7 | 1.255 | 1.031 | 0.977 |
| walk_i32.npy | hc (zstd 19/22) | 1 | 5.5 | 5.6 | 5.2 | 5.2 | 0.994 | 1.060 | 1.071 |
| walk_i32.npy | hc (zstd 19/22) | 4 | 20.5 | 19.8 | 19.9 | 19.7 | 1.034 | 1.029 | 1.040 |
| walk_i32.npy | ratio (LZMA2/BWT) | 1 | 2.6 | 2.6 | 2.6 | 2.6 | 1.010 | 1.013 | 1.018 |
| walk_i32.npy | ratio (LZMA2/BWT) | 4 | 2.7 | 2.6 | 2.6 | 2.6 | 1.041 | 1.017 | 1.005 |

### Timing: decompression

| input | mode | T | generic | avx2 | native | dispatch | avx2/generic | native/generic | dispatch/generic |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| silesia.tar | speed (LZ4) | 1 | 1252.5 | 1384.3 | 1354.2 | 1221.0 | 0.905 | 0.925 | 1.026 |
| silesia.tar | speed (LZ4) | 4 | 3644.6 | 3809.7 | 3837.9 | 3671.8 | 0.957 | 0.950 | 0.993 |
| silesia.tar | balanced (zstd 3) | 1 | 657.3 | 647.8 | 682.6 | 675.6 | 1.015 | 0.963 | 0.973 |
| silesia.tar | balanced (zstd 3) | 4 | 2147.9 | 2094.2 | 2145.9 | 2147.7 | 1.026 | 1.001 | 1.000 |
| silesia.tar | hc (zstd 19/22) | 1 | 427.2 | 413.7 | 418.7 | 411.1 | 1.033 | 1.020 | 1.039 |
| silesia.tar | hc (zstd 19/22) | 4 | 1011.6 | 1162.1 | 1189.7 | 1144.6 | 0.870 | 0.850 | 0.884 |
| silesia.tar | ratio (LZMA2/BWT) | 1 | 41.1 | 40.2 | 40.2 | 38.1 | 1.023 | 1.021 | 1.078 |
| silesia.tar | ratio (LZMA2/BWT) | 4 | 83.1 | 90.3 | 92.6 | 95.5 | 0.920 | 0.898 | 0.870 |
| trades.csv | speed (LZ4) | 1 | 1377.9 | 1518.5 | 1413.4 | 1511.5 | 0.907 | 0.975 | 0.912 |
| trades.csv | speed (LZ4) | 4 | 2655.9 | 2611.0 | 2758.6 | 2731.6 | 1.017 | 0.963 | 0.972 |
| trades.csv | balanced (zstd 3) | 1 | 313.6 | 443.2 | 461.1 | 426.0 | 0.708 | 0.680 | 0.736 |
| trades.csv | balanced (zstd 3) | 4 | 831.9 | 1126.2 | 1080.6 | 1103.1 | 0.739 | 0.770 | 0.754 |
| trades.csv | hc (zstd 19/22) | 1 | 171.1 | 201.0 | 216.1 | 201.4 | 0.851 | 0.792 | 0.850 |
| trades.csv | hc (zstd 19/22) | 4 | 282.4 | 305.9 | 268.8 | 299.8 | 0.923 | 1.051 | 0.942 |
| trades.csv | ratio (LZMA2/BWT) | 1 | 76.0 | 82.8 | 81.2 | 77.6 | 0.917 | 0.935 | 0.979 |
| trades.csv | ratio (LZMA2/BWT) | 4 | 80.0 | 87.4 | 84.0 | 79.6 | 0.915 | 0.952 | 1.005 |
| events.ndjson | speed (LZ4) | 1 | 1459.4 | 1567.5 | 1510.3 | 1606.6 | 0.931 | 0.966 | 0.908 |
| events.ndjson | speed (LZ4) | 4 | 2868.4 | 3266.2 | 3511.3 | 3452.4 | 0.878 | 0.817 | 0.831 |
| events.ndjson | balanced (zstd 3) | 1 | 616.3 | 671.2 | 665.3 | 644.8 | 0.918 | 0.926 | 0.956 |
| events.ndjson | balanced (zstd 3) | 4 | 1371.5 | 1591.1 | 2053.9 | 1890.8 | 0.862 | 0.668 | 0.725 |
| events.ndjson | hc (zstd 19/22) | 1 | 247.5 | 218.8 | 242.2 | 237.3 | 1.131 | 1.022 | 1.043 |
| events.ndjson | hc (zstd 19/22) | 4 | 280.6 | 321.7 | 360.3 | 286.4 | 0.872 | 0.779 | 0.980 |
| events.ndjson | ratio (LZMA2/BWT) | 1 | 62.5 | 64.1 | 62.5 | 62.3 | 0.976 | 1.000 | 1.003 |
| events.ndjson | ratio (LZMA2/BWT) | 4 | 64.0 | 61.1 | 63.7 | 61.6 | 1.047 | 1.005 | 1.040 |
| walk_i32.npy | speed (LZ4) | 1 | 1619.2 | 1532.4 | 1562.1 | 1759.1 | 1.057 | 1.037 | 0.920 |
| walk_i32.npy | speed (LZ4) | 4 | 2632.7 | 2969.0 | 2522.6 | 2872.1 | 0.887 | 1.044 | 0.917 |
| walk_i32.npy | balanced (zstd 3) | 1 | 989.6 | 1044.3 | 1028.7 | 1127.9 | 0.948 | 0.962 | 0.877 |
| walk_i32.npy | balanced (zstd 3) | 4 | 1993.1 | 1940.6 | 2200.8 | 2043.1 | 1.027 | 0.906 | 0.976 |
| walk_i32.npy | hc (zstd 19/22) | 1 | 202.3 | 223.6 | 196.3 | 226.2 | 0.905 | 1.031 | 0.894 |
| walk_i32.npy | hc (zstd 19/22) | 4 | 291.2 | 339.1 | 314.0 | 326.7 | 0.859 | 0.927 | 0.891 |
| walk_i32.npy | ratio (LZMA2/BWT) | 1 | 165.1 | 151.1 | 164.9 | 161.3 | 1.093 | 1.002 | 1.024 |
| walk_i32.npy | ratio (LZMA2/BWT) | 4 | 168.6 | 158.6 | 160.8 | 164.7 | 1.063 | 1.048 | 1.024 |

### Noise and summary

fastest vs second fastest of 5: median gap 4.4%, 90th percentile 16.4%
median of 5 vs fastest: median 11.1%

| direction | T | avx2/generic | native/generic | dispatch/generic |
|---|---:|---:|---:|---:|
| compress | 1 | 0.984 | 0.976 | 1.012 |
| compress | 4 | 1.002 | 0.993 | 1.003 |
| decompress | 1 | 0.952 | 0.949 | 0.947 |
| decompress | 4 | 0.925 | 0.907 | 0.920 |

The gap between the fastest and second-fastest of 5 runs is 4.4 % at the median and 16 % at the 90th percentile,
so single cells within about ±10 % are noise on this VM. Several cells in the tables above flip sign from one view
to another. The decode cases that matter were re-timed with 15 interleaved runs (`simd/confirm_simd.py`), decoding
the same frame with every build:

| input | mode | T | generic ms | avx2 ms | native ms | dispatch ms | avx2/generic | native/generic | dispatch/generic |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| trades.csv | balanced (zstd 3) | 1 | 153 | 115 | 112 | 118 | 0.748 | 0.729 | 0.766 |
| trades.csv | balanced (zstd 3) | 4 | 52 | 41 | 40 | 41 | 0.781 | 0.770 | 0.787 |
| trades.csv | hc (zstd 19/22) | 1 | 173 | 135 | 133 | 139 | 0.780 | 0.769 | 0.806 |
| walk_i32.npy | balanced (zstd 3) | 1 | 65 | 62 | 61 | 54 | 0.958 | 0.942 | 0.824 |
| walk_i32.npy | speed (LZ4) | 1 | 36 | 36 | 35 | 36 | 1.007 | 0.996 | 1.013 |
| silesia.tar | speed (LZ4) | 1 | 144 | 144 | 140 | 142 | 0.996 | 0.969 | 0.986 |
| silesia.tar | hc (zstd 19/22) | 4 | 150 | 138 | 159 | 147 | 0.922 | 1.062 | 0.982 |
| events.ndjson | balanced (zstd 3) | 4 | 39 | 40 | 40 | 41 | 1.029 | 1.023 | 1.052 |

Conclusions from both views:

- **Numeric CSV decode** (the AVX2 decimal formatter) is the only large effect of the compiled-out paths: −22 to
  −27 % with `-mavx2`, at 1 and 4 threads and in `-m balanced` and `-m hc`. The dispatched build gets −19 to −23 %.
  The 2–4 % left over is probably the rest of the table decoder that `-mavx2` compiles with VEX/AVX2 (the dispatched
  build only switches the two formatter loops).
- **Integer arrays:** `-mavx2` gains 4–6 % (the XXH3 checksum). The dispatched build gains 18 % with the new inverse
  (section 3).
- **silesia.tar, JSON and all compression:** within noise in every mode. Compression is zstd/LZMA/BWT/CM work that
  the compiler does not vectorise any further at 256 bits. The 4-thread `hc`/`ratio` cells that favour non-generic
  builds by 8–13 % in the 5-round table were not confirmed: `silesia hc -T4` decode re-timed at 0.92 / 1.06 / 0.98.

## 2. Profile of the default mode (`-m balanced`, `-T1`)

`perf` is not installed and installing it was refused in this session, so `simd/sampler.c` was used instead. It is
an `LD_PRELOAD` sampler: `ITIMER_PROF` with `SIGPROF`, recording the interrupted RIP and mapping it to file offsets.
The timer ticks every 4 ms here, so samples were pooled over 20–150 repeated runs. `simd/symbolize.py` names each
sample by its innermost source function (`addr2line -i`; inlined helpers count as themselves, as perf does with
inline unwinding; intrinsics are charged to their caller). It names libc samples by inspecting the instruction:
code after a `syscall` is kernel time of that call, and `rep movsb`/vector moves are memcpy/memset. `ITIMER_PROF`
counts kernel time too, so syscalls and page faults appear under the user instruction that caused them. Build:
`generic` flags plus `-g` (the text section is the same size: no code change).

### silesia.tar, compression (6,365 samples)

| # | function | share | owner |
|---:|---|---:|---|
| 1 | `ZSTD_compressBlock_doubleFast_noDict_generic` | 36.0% | vendored: zstd |
| 2 | `ZSTD_selectAddr` | 4.6% | vendored: zstd |
| 3 | `ZSTD_count` | 3.6% | vendored: zstd |
| 4 | `syscall pread (libc.so.6, kernel time)` | 3.3% | system |
| 5 | `lines_encode` | 2.7% | Kolmo |
| 6 | `HIST_count_parallel_wksp` | 2.7% | vendored: zstd |
| 7 | `img16_detect` | 2.6% | Kolmo |
| 8 | `FSE_encodeSymbol` | 2.5% | vendored: zstd |
| 9 | `ZSTD_hash8` | 2.5% | vendored: zstd |
| 10 | `ZSTD_hash5` | 2.1% | vendored: zstd |
| 11 | `syscall pwrite (libc.so.6, kernel time)` | 2.0% | system |
| 12 | `ZSTD_encodeSequences_body` | 1.9% | vendored: zstd |
| 13 | `BIT_addBits` | 1.7% | vendored: zstd |
| 14 | `ZSTD_compressBlock_fast_noDict_generic` | 1.6% | vendored: zstd |
| 15 | `kg_img8_trial` | 1.6% | Kolmo |

top 15: 71.5% of samples; all Kolmo-owned code 14.7%, vendored 77.9%, system libraries 7.4%

### silesia.tar, decompression (4,279 samples)

| # | function | share | owner |
|---:|---|---:|---|
| 1 | `syscall pwrite (libc.so.6, kernel time)` | 15.6% | system |
| 2 | `ZSTD_decodeSequence` | 13.0% | vendored: zstd |
| 3 | `ZSTD_wildcopy` | 9.1% | vendored: zstd |
| 4 | `img16_inv` | 7.7% | Kolmo |
| 5 | `lines_decode` | 6.1% | Kolmo |
| 6 | `_HUF_decompress4X1_usingDTable_internal_fast_asm_loop` | 5.3% | vendored: zstd |
| 7 | `BIT_reloadDStream_internal` | 3.9% | vendored: zstd |
| 8 | `ZSTD_execSequence` | 3.8% | vendored: zstd |
| 9 | `BIT_getMiddleBits` | 3.3% | vendored: zstd |
| 10 | `syscall pread (libc.so.6, kernel time)` | 3.3% | system |
| 11 | `HUF_decompress4X2_usingDTable_internal_fast_asm_loop` | 3.0% | vendored: zstd |
| 12 | `XXH3_accumulate_512_sse2` | 2.9% | vendored: xxhash |
| 13 | `ZSTD_copy16` | 2.8% | vendored: zstd |
| 14 | `ZSTD_buildFSETable_body` | 2.3% | vendored: zstd |
| 15 | `ZSTD_updateFseStateWithDInfo` | 2.1% | vendored: zstd |

top 15: 84.1% of samples; all Kolmo-owned code 16.9%, vendored 61.6%, system libraries 21.5%

### walk_i32.npy (typed int32 array), compression (3,374 samples)

| # | function | share | owner |
|---:|---|---:|---|
| 1 | `kg_zz` | 15.3% | Kolmo |
| 2 | `kgf_write_4_0` | 15.2% | Kolmo |
| 3 | `HIST_count_parallel_wksp` | 9.9% | vendored: zstd |
| 4 | `ZSTD_compressBlock_doubleFast_noDict_generic` | 8.1% | vendored: zstd |
| 5 | `kgf_scan_4_0` | 6.3% | Kolmo |
| 6 | `kg_hist` | 6.0% | Kolmo |
| 7 | `memcpy/memmove/memset (libc.so.6, stripped)` | 5.3% | system |
| 8 | `HUF_addBits` | 4.7% | vendored: zstd |
| 9 | `syscall pread (libc.so.6, kernel time)` | 4.3% | system |
| 10 | `ZSTD_selectAddr` | 3.6% | vendored: zstd |
| 11 | `syscall pwrite (libc.so.6, kernel time)` | 3.1% | system |
| 12 | `HUF_encodeSymbol` | 3.1% | vendored: zstd |
| 13 | `HUF_flushBits` | 1.9% | vendored: zstd |
| 14 | `XXH3_accumulate_512_sse2` | 1.8% | vendored: xxhash |
| 15 | `kgf_write_4_1` | 1.5% | Kolmo |

top 15: 90.0% of samples; all Kolmo-owned code 47.6%, vendored 38.0%, system libraries 14.4%

### walk_i32.npy, decompression (2,765 samples)

| # | function | share | owner |
|---:|---|---:|---|
| 1 | `syscall pwrite (libc.so.6, kernel time)` | 21.5% | system |
| 2 | `fi_inv_4_2_0` | 19.5% | Kolmo |
| 3 | `memcpy/memmove/memset (libc.so.6, stripped)` | 12.5% | system |
| 4 | `HUF_decodeSymbolX2` | 7.7% | vendored: zstd |
| 5 | `kg_unzz` | 7.4% | Kolmo |
| 6 | `syscall rename (libc.so.6, kernel time)` | 5.5% | system |
| 7 | `BIT_lookBitsFast` | 5.3% | vendored: zstd |
| 8 | `syscall pread (libc.so.6, kernel time)` | 5.1% | system |
| 9 | `XXH3_accumulate_512_sse2` | 4.8% | vendored: xxhash |
| 10 | `fi_inv_4_2_1` | 3.1% | Kolmo |
| 11 | `HUF_decompress4X2_usingDTable_internal_body` | 2.8% | vendored: zstd |
| 12 | `BIT_skipBits` | 1.2% | vendored: zstd |
| 13 | `BIT_reloadDStream_internal` | 0.7% | vendored: zstd |
| 14 | `BIT_reloadDStreamFast` | 0.5% | vendored: zstd |
| 15 | `syscall munmap (libc.so.6, kernel time)` | 0.5% | system |

top 15: 98.2% of samples; all Kolmo-owned code 30.2%, vendored 24.3%, system libraries 45.5%

On general data, Kolmo's own code is 15–17 % of the time: vendored zstd dominates, plus kernel I/O on decode. On
typed arrays, Kolmo's own code is 30–48 %: the forward transform `kg_zz`, `kgf_write_4_0`, `kgf_scan_4_0` and
`kg_hist`, and the inverse `fi_inv_4_2_0` + `kg_unzz`. On decode, 45 % is the kernel (`pwrite` of the output,
`rename`, page faults in memcpy), which no instruction set changes.

## 3. Runtime dispatch (commit `fc9a356`)

| path | file | how |
|---|---|---|
| CRC32C | `src/kolmo.c` | `crc32c_sse42` with `target("sse4.2")`; `kolmo_crc32c` calls it when `__builtin_cpu_supports("sse4.2")`, otherwise the slice-by-8 table. `-msse4.2` builds call it directly. |
| decimal formatter | `src/kolmo.c` | the AVX2 loops of `fmt_slots` and `fmt_run` moved into `fmt_slots_avx2` / `fmt_run_avx2` with `target("avx2")`; they return how many values they wrote, and the scalar loop does the rest. The lookup tables are built on every x86-64 build. |
| XXH3 | `vendor/xxhash/kolmo_xxh3.c` (Kolmo's wrapper; `xxhash.h` unchanged) | `XXH_DISPATCH_AVX2` plus `XXH_TARGET_AVX2` compile xxHash's AVX2 kernels; inputs over 240 bytes go to `XXH3_hashLong_64b_internal(..., XXH3_accumulate_avx2, XXH3_scrambleAcc_avx2)` on AVX2 CPUs, as upstream's `xxh_x86dispatch.c` does. |
| integer inverse (new) | `src/kgraph.inc` | `fi_vec4_sse2` (baseline) and `fi_vec4_avx2` (`target("avx2")`), see section 4. |

The capability test is cached in a static variable (relaxed atomics; the result is the same in every thread). When
the build already targets the ISA, `kolmo_have_avx2()` is the constant 1 and the generic code is dead. Nothing is
dispatched on non-x86 targets (the NEON paths are unchanged), nor with `-DKOLMO_NO_DISPATCH`. The code compiles with
gcc 13 and clang 18.

**A bug found on the way.** The first version of the XXH3 dispatch kept the seeded secret in a buffer aligned to
`XXH_SEC_ALIGN` (8 in a non-AVX2 build), but the AVX2 kernel stores to it with 32-byte aligned stores, so it crashed
(SIGSEGV in `vmovdqa`, at the first seeded hash of an input over 240 bytes, i.e. the first block of any
compression). It was fixed before the first commit: the buffer is `XXH_ALIGN(64)`, as upstream's dispatcher uses,
so no pushed commit has the bug. `tests/test_dispatch.c` (`make check-dispatch`, part of `check-rc`) crashes with
SIGSEGV when the old alignment is restored and passes with the fix (gcc 13 -O3, clang 18 -O3 and -O0, ASan/UBSan).

**Proof of identical output:**

- The four builds wrote byte-identical frames in all 32 timed cases, and all round trips were exact (table in
  section 1).
- `make check-dispatch` (new, part of `check-rc`, also built with ASan/UBSan): 28,182 checks of `kolmo_crc32c` and
  `kolmo_xxh3` against a bitwise CRC32C and xxHash compiled with `XXH_VECTOR=XXH_SCALAR`. It covers lengths 0–131,072,
  33 alignments and 5 seeds: 0 mismatches.
- `make check-fastint` compares the fast inverse with the reference `kg_int_inv` on 1,098,727 programs and passes in
  the ASan, optimised and `KOLMO_FORCE_GENERIC_INT_INV` builds. With a breakpoint, `fi_vec4_avx2` is hit 9,954
  times in the default build, and `fi_vec4_sse2` 9,954 times in a `-DKOLMO_NO_DISPATCH` build. Both paths pass.
- `make ARCH= check` (`ALL PASSED`), `make ARCH= check-sanitizers` and `make ARCH= check-rc` pass on `fc9a356`.

**Packaging.** The Python and Rust packages build from copies under `bindings/*/native/`. The three changed files
(`src/kolmo.c`, `src/kgraph.inc`, `vendor/xxhash/kolmo_xxh3.c`) are now copied there too. `bindings/rust/build.rs`
compiled with `-march=native`, so Rust builds were host-specific; it is portable now (section 5).

## 4. Assembly

Candidates from the profile (Kolmo-owned only):

| loop | share | bound by | SIMD / asm outlook |
|---|---|---|---|
| `fi_inv_4_2_0` + `kg_unzz` (integer inverse, W = 4) | ~27 % of typed-array decode | serial prefix chain (scalar), then memory bandwidth | **done with intrinsics**, see below |
| `kg_zz`, `kgf_write_4_0`, `kgf_scan_4_0` (forward transform) | ~37 % of typed-array encode | integer divide by `bg` when g > 1, gcd; byte-plane stores | prefix differences vectorise like the inverse (next candidate); the gcd/divide path is data dependent |
| `img16_inv` (16-bit image inverse) | 7.7 % of silesia decode | MED mode: a serial median chain (min/max/add per pixel) | mode 0 is the same prefix sum as above (SSE2 would help); MED is latency bound, so asm cannot shorten the chain |
| `lines_decode` | 6.1 % of silesia decode | pass 2 scatters columns to strided row positions | memory/scatter bound; no gain from asm |
| `kg_hist`, `img16_detect`, `lines_encode`, `kg_img8_trial` | 1.6–6 % | histogram (load/store ports), scans | histograms are port bound; no gain from asm |
| context-mixing model (`-m ratio` on text) | most of `ratio` | cache misses on large hashed tables | memory bound: assembly does not help, and SIMD helps only the mixer dot product, which is already small |

**Integer inverse with intrinsics.** The scalar recurrence is: `v = v1 + unzigzag(lo[i] | hi[i] << 8)`, or
`2·v1 − v2 + …` for order 2. Each value depends on the previous one through a 1-cycle add, but the loads, the plane
merge, the unzigzag and the stores per value keep the scalar loop at about 2 cycles per value. The vector version
merges the planes and unzigzags 8 or 16 values at a time, then does a log-step prefix sum in the register
(`slli_si128`/`slli_si256` plus a cross-lane carry) and carries the last lane into the next step. Order 2 is two
prefix sums: first differences, then values. 32-bit lanes give the same bytes, because only the low 32 bits are
stored and the recurrence is mod 2^64. Microbenchmark (`simd/fi_inv_bench.c`, fastest of 15, prefaulted buffers,
identical bytes in every case):

| values | order | planes | scalar ns/value | SSE2 ns/value | AVX2 ns/value |
|---:|---:|---:|---:|---:|---:|
| 16,777,215 | 1 | 1 | 0.864 | 0.565 | 0.561 |
| 16,777,215 | 1 | 2 | 1.024 | 0.570 | 0.567 |
| 16,777,215 | 2 | 1 | 1.154 | 0.659 | 0.583 |
| 16,777,215 | 2 | 2 | 1.177 | 0.681 | 0.637 |
| 100,003 | 1 | 1 | 0.691 | 0.357 | 0.245 |
| 100,003 | 1 | 2 | 0.863 | 0.361 | 0.291 |
| 100,003 | 2 | 1 | 1.061 | 0.569 | 0.412 |
| 100,003 | 2 | 2 | 1.063 | 0.571 | 0.465 |

On 16 M values both vector versions run at about 0.57 ns/value. Each value moves 6 bytes (2 read, 4 written), so
that is about 10.5 GB/s: the VM's memory bandwidth. In cache, AVX2 is 1.2–1.5× faster than SSE2. End to end,
`-m balanced -T1` decode of walk_i32.npy drops from 65 ms to 54 ms (−18 %, fastest of 15).

**What assembly could add.** GCC turns the AVX2 loop into one unswitched loop per (planes, order). Each is about 40
instructions per 16 values, with no spills (the only stack access is loading the 7th argument), and runs at about
0.5–0.75 cycles per value in cache depending on the turbo clock: around 3+ instructions per cycle, near the
shuffle-port limit of the prefix sum. Hand scheduling could gain a few percent in cache and **nothing** on
out-of-cache arrays, which are bandwidth bound. A different algorithm (a transposed 4×8 prefix sum, which trades
shuffles for adds) is worth more than assembly, and it can be written with intrinsics too. So no inline-asm version
was written: the profile does not justify one.

Where assembly will not help:
- **The context-mixing model** is bound by cache misses into large hash tables.
- **The MED predictor** is latency bound (a serial min/max chain).
- **`lines_decode`** is a scatter.
- **Histograms** are port bound.
- **Kernel I/O** is 20–45 % of decode time.
- **zstd's hot loops** (`ZSTD_compressBlock_doubleFast`, `ZSTD_decodeSequence`) are vendored and already tuned,
  with zstd's own asm Huffman decoder.

## 5. Follow-up fixes (after the first report)

Commit after `7b68a2c` on the same branch.

| fix | files | effect on frames |
|---|---|---|
| **No FMA contraction in any codec.** `-ffp-contract=off` is added to every C and C++ compile, whatever `CFLAGS` says (`override CFLAGS += $(FPFLAGS)`, `override SANFLAGS += $(FPFLAGS)`). This covers the vendored zstd, Brotli, LZ4/LZ4HC, xxHash, libsais and the libbsc C++ sources, and the rc2 research codecs in `experimental.mk` that missed it: KRC1 (`krc-cpu`, `krc-auto`), KBC1 (`kbc`), KBX3 (`kbx`), `kolmo-fastq`, `kvid` (only `kfam` had it). It also covers the tools and tests, and the lzbench builds, which go through the Makefile. | `Makefile`, `fuzz/build.sh` (the fuzz harness), `bindings/python/setup.py` (all C and C++ files), `bindings/rust/build.rs` (all C and C++ files) | none on generic x86-64: the machine code of all 77 vendored objects and of `kolmo` itself is identical with and without the flag at `ARCH=`. With `-march=native` the Brotli encoder (`-m eco` = Brotli quality 8) now has 0 FMA instructions instead of 201, so local native builds and arm64 builds make the same Brotli choices as the release build |
| **Portable Rust build.** `build.rs` no longer passes `-march=native` / `-mcpu=native`; `KOLMO_NATIVE=1` opts back in. It also re-runs when `kgraph.inc` or `kolmo_xxh3.c` changes. | `bindings/rust/build.rs` | none (same sources; the dispatched paths still reach AVX2 on CPUs that have it). The static library now has VEX code only in the four dispatched functions (`kolmo_xxh3_long_avx2`, `fi_vec4_avx2`, `fmt_slots_avx2`, `fmt_run_avx2`), no AVX-512, no FMA |
| **Bindings carry the dispatch.** `src/kolmo.c`, `src/kgraph.inc` and `vendor/xxhash/kolmo_xxh3.c` copied into `bindings/python/native/` and `bindings/rust/native/`. `sync-native.sh` was not run: it would also copy the rc2 research tools, which the packages never shipped. | `bindings/*/native/` | none |
| **kfam (rc2 research codec): `make kfam-check` failed at rc2.** LeakSanitizer reported 4,351 bytes: `main` never freed the input, the frame and the decoded copy. `fastq_join` also leaked its name buffer on every error return. Both fixed. | `src/experimental/kfam.c` | none (kfam frames unchanged; it never writes Kolmo frames) |
| **kfam: a damaged frame could hang the decoder.** A stream header's raw length was accepted up to 2^40, and the arithmetic decoder fed zeros after its input ran out. Two flipped bits made `kfam -d` run for more than 10 minutes, reserving 21 GB. Now (1) a stream that reads more than 8 bytes past its end is corrupt (a valid stream never reads past its end: the decoder renormalises in step with the encoder, which flushes exactly 4 bytes); (2) a raw length above 8,192 symbols per compressed byte is refused before allocation (every symbol costs at least 2 decisions of at least 0.000727 bits, so valid streams stay under about 5,500; 20 MB of zeros, 1,415:1, still round trips). | `src/experimental/kfam.c`, `tests/kfam_damage.py`, `experimental.mk` | none for valid frames; the hanging frame is now rejected in 18 ms. `tests/kfam_damage.py`, 800 damaged frames under ASan/UBSan: 0 hangs, 0 sanitizer reports; a 120-frame run is now part of `kfam-check` |
| **XXH3 dispatch alignment** (the crash found while building section 3) | `vendor/xxhash/kolmo_xxh3.c` (in `fc9a356`) | fixed before any push; regression test in `make check-dispatch` |

For the research codecs, `-ffp-contract=off` changes nothing on generic x86-64 either. Their FMA instructions with
`-march=native` (4 in `krc-cpu`, `kbc` and `kbx`, 8 in `kolmo-fastq` and `kvid`) are all in `main` (timing and
statistics output), not in their coders.

**Proof that frames did not change** (`benchmark/rc2/simd/results/frames-fixes.txt`). Five builds:
- `generic` and `native`: `9d4496a`, `ARCH=` and `ARCH=-march=native`, the old flags
- `disp`: `fc9a356`, `ARCH=`
- `fp-generic` and `fp-native`: this fix, `ARCH=` and `ARCH=-march=native`

Frames compared by SHA-256, every one also decoded back to the input. Every option is listed with its backend
from the table at the top. Full inputs for the four main modes and `-m eco`; 4 MB prefixes of each input for
`-x` (all three levels) and the opt-in leaves:

(frame comparison running; filled in by the next commit)

**Tests on the fixed tree:** `make ARCH= check`, `check-sanitizers` and `check-rc` (with `check-dispatch`);
`experimental-check` and `kfam-check` (the rc2 research codecs, now built with the flag); `cargo test --release` in
`bindings/rust` (4 tests and the doc test); `python3 setup.py build` plus `test_kolmo.py` for the Python package.
Results: (running; filled in by the next commit)

## Reproduce

```
python3 -I benchmark/rc2/simd/mk_inputs.py DATA                    # + silesia.tar as in the input table
make ARCH= build/kolmo                                             # and the other three builds, in separate trees
python3 -I benchmark/rc2/simd/run_simd.py runs.jsonl 5 DATA WORK generic=… avx2=… native=… dispatch=…
python3 -I benchmark/rc2/simd/summarize_simd.py runs.jsonl generic; python3 -I benchmark/rc2/simd/noise_simd.py runs.jsonl generic
python3 -I benchmark/rc2/simd/confirm_simd.py 15 DATA WORK generic=… avx2=… native=… dispatch=…
cc -O2 -shared -fPIC benchmark/rc2/simd/sampler.c -o sampler.so   # profile: SAMPLER_OUT=s.txt LD_PRELOAD=./sampler.so kolmo …
python3 -I benchmark/rc2/simd/symbolize.py s.txt 15
gcc -O3 benchmark/rc2/simd/fi_inv_bench.c -o fib && ./fib && ./fib 100003
make ARCH= check-dispatch
```
