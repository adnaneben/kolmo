# kolmo — a structure-aware compressor

kolmo reads the structure of its input — tables, logs, prose, scientific records and images, executables, archives — and
stores it in a form that entropy coders compress far better, then encodes independent, checksummed blocks on every core.
It has six modes, from LZ4-speed `rt` to the context-mixing archive tier `ultra`. In every mode the goal is the same:
**at the speed of the leading compressor of that class, produce smaller files.**

## Results

Single core, in memory, best of repeated runs; every output decoded and compared byte for byte. Each cell is the
compression ratio (original size / compressed size). Machine: 2-core Intel Xeon VM, 2.1 GHz, gcc 13, `-O3 -march=native`.
The leaders were built from the same sources with the same flags (OpenZL: its own CLI, best profile, trained on the
tables). Full per-mode tables, speeds, and a one-command way to reproduce all of this on your own machine are in
[`benchmark/BENCHMARKS.md`](benchmark/BENCHMARKS.md).

| Data | kolmo fast | kolmo eco | kolmo hc | kolmo max | kolmo ultra | zstd -19 | brotli -11 | xz -9e | OpenZL |
|---|---|---|---|---|---|---|---|---|---|
| Tables: OWID CO2 + energy, 23.6 MB | 10.31 | 10.58 | 10.82 | 11.38 | 11.44 | 4.20 | 4.56 | 4.54 | 4.82 |
| Logs: 16 Loghub logs, 4.5 MB | 14.10 | 15.79 | 16.27 | 17.67 | 22.42 | 13.48 | 14.50 | 14.67 | 9.84 |
| Text: enwik8, 100 MB | 2.81 | 3.48 | 4.10 | 4.62 | 5.11 | 3.71 | 3.88 | 4.03 | 3.06 |
| Scientific: star catalogue + X-ray, 15.7 MB | 2.64 | 2.69 | 2.68 | 2.71 | 2.76 | 1.55 | 1.70 | 1.76 | 2.56 |
| Mixed: Silesia corpus, 212 MB | 3.64 | 4.19 | 4.45 | 5.08 | 5.58 | 4.01 | 4.27 | 4.38 | 3.35 |

Speeds (single core, MB/s, compress / decompress) for the same runs are in `results_*.tsv`. In short: `fast` compresses
at 100–190 MB/s and decompresses at 500–1100 MB/s on this machine (2.3–3.3× faster on Apple M5 cores); `hc` decompresses
at 460–1030 MB/s, 4–10× faster than xz; `max` and `ultra` are archive tiers (`ultra` runs at well under 1 MB/s per core).

Blocks are independent, so both directions scale with cores while the single-stream LZ coders do not. The speed modes
(`rt`, `fast`) size blocks adaptively (~1 MiB, ≥8 per input) so decode and compress parallelise; the archive modes keep
large windows for ratio. On a 4-core Apple-silicon build, `kolmo fast` **decompresses faster than `zstd -3` on every data
type — tables 3.6 GB/s vs 1.6, scientific 6.4 vs 1.2, logs 8.5 vs 4.1 — and compresses tables faster too (759 vs 600
MB/s)**, all while holding the ratio lead, because `zstd` is flat with cores and kolmo scales ~3.7×. On a single core kolmo
leads decode on scientific and trades 1.2–2× decode speed on tables/logs for 2–4× the ratio, making it back across cores.

## How it works

| Data | Model |
|---|---|
| Tables (CSV/TSV) | Columns become typed streams. Numeric columns are exact decimals at a common scale, predicted per block by raw, delta, delta within an entity (e.g. country), **delta-of-delta** and **third-order** within an entity (for counters and steady trends), or a discovered **formula of other columns** (`y = k·a/b`, change and % change): 51 formulas on the OWID energy table. Quoted fields, short rows and odd values are lossless. |
| Logs | Lines are split into **templates and variables**; timestamps, IPs and IDs become integer columns ("skeleton" columns). |
| Prose and markup | Frequent words become 2-byte codes from byte values the block never uses (eco and above); `hc` then uses zstd 22 with a 128 MiB window. |
| Scientific binary | **Fixed records** (period found by self-similarity; fields of 1/2/4/8 bytes stored raw, as deltas, as dictionary tokens, or as multiples of a discovered quantum with exact bit corrections) and **16-bit images** (byte order, row stride and predictor found automatically). |
| Executables, archives | x86 call/jump transform; each tar member gets its own model. |
| Coders | LZ4 (`rt`), zstd (`fast`, `hc`), brotli (`eco`), BWT via libbsc and LZMA2 (`max`), and an integer **context-mixing** coder (`ultra`: orders 1–7, words, columns, indirect contexts, bit histories, neural mixing, APMs), best per block. |
| Container | Independent blocks with an index, so any byte range decodes alone. XXH3 per block and CRC32C over the header and index: corrupt input is reported, never returned as data. Blocks compress and decompress in parallel, so throughput scales with cores. |

All prediction that must agree between encoder and decoder is integer or plain IEEE double without fused multiply-add
(`-ffp-contract=off`), so **Intel and Apple-silicon builds produce byte-identical files** and decode each other's output.

## Modes

| Mode | Block | Coder | Use |
|---|---|---|---|
| `rt` | adaptive (≤4 MiB) | LZ4 | decode speed first |
| `fast` | adaptive (≤4 MiB) | zstd 3 | default |
| `eco` | 16 MiB | brotli 8 | lowest storage cost at moderate CPU |
| `hc` | 16 MiB (text 128 MiB) | zstd 19 / 22 | archive with fast reads |
| `max` | 128 MiB | BWT for text, LZMA2 otherwise | cold archive |
| `ultra` | 128 MiB | best of context mixing, LZMA2, BWT | coldest archive, best ratio |

`ultra` sizes its context-mixing tables to the block (up to about 4 GB per thread for blocks of 4 MiB and more, in
both directions). Parallel blocks share a budget of 60% of physical memory and wait rather than swap.

### Columnar decode (tables)

A table block can be decoded straight to **typed columns** — numeric columns as 64-bit integers at a common scale
(plus a validity flag per cell), text columns as offset/length into the block's bytes — skipping CSV row reassembly
and decimal formatting (`tbl2_columns` / `exp/colbench.c`). An analytical reader (Arrow, DuckDB, pandas) that would
otherwise decompress to CSV and re-parse reaches typed values about **3× faster**, and the columnar path is 1.2–1.4×
faster than full row decode on its own. The default lossless round trip is unchanged.

### Streaming (large inputs)

For inputs too large to hold in memory, `-s` (or `-S <segMiB>`) compresses a stream of
independent frames — each a normal container for one segment — so compress and decompress run in
**bounded memory** (one segment, not the whole file) and work on pipes. On a 47 MB input, one-shot
compression peaks at ~114 MB of RAM; streaming at 4 MiB segments peaks at ~32 MB, and decompression
at ~11 MB. `-d` detects a streamed file automatically. The library API is `czn_stream_compress` /
`czn_stream_decompress` (file descriptors in and out).

## Install

```bash
cargo add kolmo                 # Rust    (bindings/rust)
pip install kolmo               # Python  (bindings/python; builds the native core)
```

Or build the CLI and library from source (below). The Rust and Python packages compile and
bundle the native core, so they have no runtime dependency beyond `liblzma`.

## Build and use

```bash
make                                    # build/kolmo, build/czbench, build/czntest (Linux, macOS; gcc or clang)
./build/czntest                         # round trips in every mode, range reads, corruption fuzzing
./build/kolmo -m hc in.csv out.czn        # compress: -m rt|fast|eco|hc|max|ultra, -T threads, -B block MiB, -n no transforms
./build/kolmo -d out.czn back.csv         # decompress (auto-detects one-shot vs streamed input)
./build/kolmo -s huge.csv huge.czs        # streaming compress: bounded memory, any size (-S N for N-MiB segments)
./build/kolmo -r 1000000:4096 out.czn x   # read 4 KiB at offset 1,000,000 without decoding the rest
./build/czbench -X -o results.tsv file  # benchmark against LZ4, zstd, brotli and xz built from the same sources
```

Requires liblzma (`apt install liblzma-dev`, `brew install xz`); zstd, LZ4, brotli and libbsc are vendored. Build with `make NO_LZMA=1`
where liblzma is unavailable (max and ultra then use the other coders).

On a Mac: `bash mac/setup.sh` once, then `bash mac/bench.sh` for the full benchmark and `bash mac/tune.sh` for an
overnight genetic search of the build-time settings (`tune/ga.py`).

## Status

1.x. The container is version 5 (a stream uses v4/v5 only when the delta-of-delta or third-order predictor is selected, else v3); versions 1–3 remain readable, and an older decoder reports an error on a newer version rather than returning wrong data. Output is byte-identical across
x86-64 and Apple silicon (verified: rt/fast/hc on tables, logs and scientific data hash the same on both). The decoder
is fuzzed (OSS-Fuzz harnesses in `fuzz/`, run under ASan/UBSan in CI); corrupt input is reported, never returned as
data. A shared library and Python and Rust bindings are in `bindings/`; the format is documented in `SPEC.md`.

## Layout

```
src/czn.h, czn.c   library: planning, container, codecs, thread pool, table v1, field transposition, shuffle
src/tbl2.inc       tables: typed columns, predictors, formula discovery
src/log.inc        logs: templates and skeleton columns
src/sci.inc        fixed records and 16-bit images
src/word.inc       word dictionary for prose
src/cm.inc         context-mixing coder
src/cli.c          command line          bench/czbench.c   benchmark          tests/czntest.c   tests
tune/ga.py         genetic tuning        mac/              Mac scripts        vendor/           zstd, LZ4, libbsc
```

## Licence

Apache-2.0 (see LICENSE). Vendored code keeps its own licence (see NOTICE).
