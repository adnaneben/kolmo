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
[`benchmark/BENCHMARKS.md`](benchmark/BENCHMARKS.md). How we decide whether a ratio gain is worth a speed loss (or
the reverse), with the research and industry figures behind it, is in [`benchmark/TRADEOFFS.md`](benchmark/TRADEOFFS.md).

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

### lzbench: Silesia, one thread

kolmo also runs inside [lzbench](https://github.com/inikep/lzbench) as an ordinary codec (same harness, same flags,
one thread, round trips verified; see [`benchmark/lzbench`](benchmark/lzbench)). Each lzbench level is a plain CLI
setting. On the 212 MB Silesia corpus (2-core 2.1 GHz Xeon VM; speeds vary by about ±15% between runs, ratios are
exact):

| kolmo setting | Ratio | Compress | Decompress | Best rival at that speed |
|---|---|---|---|---|
| `rt` | 46.58% | 540 MB/s | 2.5 GB/s | lz4: 47.59%, 539 MB/s, 3.3 GB/s |
| `fast -l1 -q` | **30.33%** | 355 MB/s | 1.2 GB/s | zstd -2: 32.70%, 372 MB/s |
| `fast -l2 -q` | **28.89%** | 294–337 MB/s | 1.1 GB/s | zstd -3: 31.20%, 210–232 MB/s |
| `fast -q` | **27.87%** | 194–234 MB/s | 0.9–1.1 GB/s | zstd -3: 31.20% |
| `fast` (default) | 27.49% | 132–188 MB/s | 0.8–1.0 GB/s | zstd -5: 29.60%, 110 MB/s |
| `hc -l 9` | **24.70%** | 48.6 MB/s | 672 MB/s | brotli -5: 28.10%, 41 MB/s |
| `max -l 1` | **20.93%** | 19–22 MB/s | 33 MB/s | bsc: 23.26%, 15 MB/s; bzip3 -5: 22.29%, 9.4 MB/s |
| `hc` | 22.47% | 2.6 MB/s | **631 MB/s** | xz -9: 23.00%, 99 MB/s decode; bzip3 -5: 22.29%, 9.7 MB/s decode |
| `max` | **19.34%** | 2.7 MB/s | 54 MB/s | kanzi -9: 19.73%, 1.7 MB/s |
| `ultra -l 1` | **17.99%** | 0.43 MB/s | 0.45 MB/s | zpaq -5: 19.09%, 0.39 MB/s |
| `ultra` | **17.01%** | 0.28 MB/s | 0.30 MB/s | — (smallest result on the board) |

From about 350 MB/s down to the slowest tier, the bold settings give the smallest output of any entry at that
compression speed or faster. `hc` (and `eco`) trade some of that for decode speed: `hc` decompresses 6× faster than
xz and 65× faster than bzip3 at a similar ratio. Above 350 MB/s, and on raw decode speed in the LZ4 class, zstd -1
and lz4 still lead.

## How it works

| Data | Model |
|---|---|
| Tables (CSV/TSV) | Columns become typed streams. Numeric columns are exact decimals at a common scale, predicted per block by raw, delta, delta within an entity (e.g. country), **delta-of-delta** and **third-order** within an entity (for counters and steady trends), or a discovered **formula of other columns** (`y = k·a/b`, change and % change): 51 formulas on the OWID energy table. Quoted fields, short rows and odd values are lossless. |
| Logs | Lines are split into **templates and variables**; timestamps, IPs and IDs become integer columns ("skeleton" columns). |
| Fixed-width text | Lines that come in a few fixed lengths (chemistry and geometry files, fixed-width reports) are stored **column by column per line length**: a line scan and a transpose, several times cheaper than templates. Files that use it carry container version 8. |
| Prose and markup | Frequent words become 2-byte codes from byte values the block never uses (eco and above); `hc` then uses zstd 22 with a 128 MiB window. |
| Decimal doubles | Float64 arrays whose values were written in decimal (`.npy` tables, raw arrays) are stored as **integers over a power of ten per column**, with missing values as a tag and anything else kept verbatim. |
| Scientific binary | **Fixed records** (period found by self-similarity; fields of 1/2/4/8 bytes stored raw, as deltas, as dictionary tokens, or as multiples of a discovered quantum with exact bit corrections) and **16-bit images** (byte order, row stride and predictor found automatically). |
| Executables, archives | x86 call/jump transform; each tar member gets its own model. |
| Coders | LZ4 (`rt`), zstd (`fast`, `hc`), brotli (`eco`), BWT via libbsc and LZMA2 (`max`), and an integer **context-mixing** coder (`ultra`: orders 1–7, words, columns, indirect contexts, bit histories, neural mixing, APMs), best per block. |
| Container | Independent blocks with an index, so any byte range decodes alone. XXH3 per block and CRC32C over the header and index: corrupt input is reported, never returned as data. Blocks compress and decompress in parallel, so throughput scales with cores. |

All prediction that must agree between encoder and decoder is integer or plain IEEE double without fused multiply-add
(`-ffp-contract=off`), so **Intel and Apple-silicon builds produce byte-identical files** and decode each other's output.

## Modes

Three modes cover almost every use:

| Mode | What it is for | Same as |
|---|---|---|
| `-m speed` | fastest; data that is read or sent right away | `rt -Q` |
| `-m balanced` (default) | small and fast; everyday storage and transfer | `fast` |
| `-m ratio` | smallest at practical speed; archives | `max` |

`-x` asks for the smallest output of all (`ultra`): context mixing, very slow, large memory. It was never removed; it
is left out of the speed benchmarks because it runs at well under 1 MB/s.

Advanced settings, for tuning between the three:

| Mode | Block | Coder | Use |
|---|---|---|---|
| `rt` | adaptive (≤4 MiB) | LZ4 | decode speed first |
| `fast` | adaptive (≤4 MiB) | zstd 3 | default (`-q`: quick analysis, cheap transforms only) |
| `eco` | 16 MiB | brotli 8 | lowest storage cost at moderate CPU |
| `hc` | 16 MiB (text 128 MiB) | zstd 19 / 22 | archive with fast reads |
| `max` | 128 MiB | BWT for text; LZMA2 otherwise, or BWT where it is at least 3% smaller (`-l 1`: BWT only) | cold archive |
| `ultra` | 128 MiB | best of context mixing, LZMA2, BWT (`-l 1`: context mixing model 1 only, faster) | coldest archive, best ratio |

`ultra` sizes its context-mixing tables to the block (up to about 4.5 GB per thread for blocks of 4 MiB and more, in
both directions; 3.8 GB with `-l 1`). On Linux the tables use transparent huge pages. Parallel blocks share a budget of 60% of physical memory and wait rather than swap.

### Columnar decode (tables)

A table block can be decoded straight to **typed columns** — numeric columns as 64-bit integers at a common scale
(plus a validity flag per cell), text columns as offset/length into the block's bytes — skipping CSV row reassembly
and decimal formatting (`tbl2_columns` / `exp/colbench.c`). An analytical reader (Arrow, DuckDB, pandas) that would
otherwise decompress to CSV and re-parse reaches typed values about **3× faster**, and the columnar path is 1.2–1.4×
faster than full row decode on its own. The default lossless round trip is unchanged.

### Dictionaries (many small files)

A single small file has too little history for any coder to find its structure, so on records of a few KB kolmo
(like zstd) trails its own large-file ratios. Train a dictionary once on a representative sample and every similar
file compresses against it. On 4,802 held-out ~4 KB OWID record files (dictionary trained on a disjoint 1,500),
`fast` goes from **3.17× to 4.77×** — about a third smaller — with the dictionary adding no measurable decode cost.

```bash
./build/kolmo -t sample.dict records/*.csv       # train a dictionary from sample files
./build/kolmo -m fast -D sample.dict one.csv one.kolmo   # compress against it
./build/kolmo -d -D sample.dict one.kolmo one.back        # decompress (same dictionary required)
```

The dictionary id is recorded in the header; a file written with one refuses to decode without the matching
dictionary (`KOLMO_ERR_DICT`) rather than returning wrong data. The API is `kolmo_train_dict` / `kolmo_compress_dict` /
`kolmo_decompress_dict` (C), `kolmo.train_dict` + `compress(..., dict=d)` (Python), and `train_dict` / `compress_dict`
/ `decompress_dict` (Rust). Dictionaries apply to the zstd-coded blocks (`fast`, `hc`).

### Streaming (large inputs)

For inputs too large to hold in memory, `-s` (or `-S <segMiB>`) compresses a stream of
independent frames — each a normal container for one segment — so compress and decompress run in
**bounded memory** (one segment, not the whole file) and work on pipes. On a 47 MB input, one-shot
compression peaks at ~114 MB of RAM; streaming at 4 MiB segments peaks at ~32 MB, and decompression
at ~11 MB. `-d` detects a streamed file automatically. The library API is `kolmo_stream_compress` /
`kolmo_stream_decompress` (file descriptors in and out).

For archival jobs with less available memory, build the optional CLI with
`make build/kolmo-cm20` and run `./build/kolmo-cm20 -m ultra -T 1 input output.kolmo`.
It uses smaller context-mixing tables when encoding. The standard decoder reads these
files. The encoder setting controls table size; total process memory also depends on
input, output buffers, other codecs and worker count.

### Decoder limits

C callers can use `kolmo_decompress_limited` with a `kolmo_decode_limits` structure to bound the
original size, raw block size and block count of a frame. Each zero field disables that limit;
exceeding a limit returns `KOLMO_ERR_LIMIT`. These are format-size limits, not a cap on total heap
memory: an `ultra` block can still require gigabytes of context-mixing state. Existing decode
functions keep their current behavior.

## Install

| | Install | Package |
|---|---|---|
| CLI (macOS, Linux) | `brew install adnaneben/kolmo/kolmo` | [adnaneben/homebrew-kolmo](https://github.com/adnaneben/homebrew-kolmo) |
| Python 3.8+ | `pip install kolmoc` | [kolmoc on PyPI](https://pypi.org/project/kolmoc/) |
| Rust | `cargo add kolmo` | [kolmo on crates.io](https://crates.io/crates/kolmo) |

- **Homebrew** builds the `kolmo` command from source in a few seconds and pulls in `xz` for liblzma.
- **Python:** the package is published as `kolmoc` because `kolmo` was already taken on PyPI, but you still
  `import kolmo`. Prebuilt wheels cover Linux (x86-64, aarch64) and, from 1.0.1, Apple silicon Macs
  (macOS 11 or later). Other platforms, Intel Macs included, build the sdist, which needs a C compiler and
  takes about a minute.
- **Rust:** the crate compiles the C core in `build.rs` and links it statically. It needs a C compiler at
  build time.
- **liblzma** is optional for both packages. If it isn't found, `max` and `ultra` use the other coders.

Usage for each binding is in [`bindings/python`](bindings/python/README.md) and
[`bindings/rust`](bindings/rust/README.md). To build the CLI and library from source, see below.

## Build and use

The examples below run the source build (`./build/kolmo`). After `brew install`, the same commands work
with plain `kolmo`.

```bash
make                                    # build/kolmo, build/kolmobench, build/kolmotest (Linux, macOS; gcc or clang)
make check                             # resource failures, concurrent callers, round trips, ranges and corruption
./build/kolmo -m ratio in.csv out.kolmo     # compress: -m speed|balanced|ratio (-x: smallest of all), -T threads; advanced: -m rt|fast|eco|hc|max|ultra, -l level, -q
./build/kolmo -d out.kolmo back.csv         # decompress (auto-detects one-shot vs streamed input)
./build/kolmo -s huge.csv huge.czs        # streaming compress: bounded memory, any size (-S N for N-MiB segments)
./build/kolmo -r 1000000:4096 out.kolmo x   # read 4 KiB at offset 1,000,000 without decoding the rest
./build/kolmo -t s.dict samples/*.csv     # train a dictionary; then -D s.dict to compress/decompress small files
./build/kolmobench -X -o results.tsv file  # benchmark against LZ4, zstd, brotli and xz built from the same sources
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
1.0.0 is published on Homebrew, PyPI and crates.io (see Install); `RELEASING.md` describes how to cut a release.

## Layout

```
src/kolmo.h, kolmo.c   library: planning, container, codecs, thread pool, table v1, field transposition, shuffle
src/tbl2.inc       tables: typed columns, predictors, formula discovery
src/log.inc        logs: templates and skeleton columns
src/lines.inc      fixed-width text: line classes stored column by column
src/sci.inc        fixed records and 16-bit images
src/word.inc       word dictionary for prose
src/cm.inc         context-mixing coder
src/cli.c          command line          bench/kolmobench.c   benchmark          tests/kolmotest.c   tests
tune/ga.py         genetic tuning        mac/              Mac scripts        vendor/           zstd, LZ4, libbsc
bindings/python    PyPI package kolmoc   bindings/rust     crate kolmo        packaging/homebrew  brew formula
```

## Licence

Apache-2.0 (see LICENSE). Vendored code keeps its own licence (see NOTICE).
