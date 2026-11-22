# kolmo — a structure-aware compressor

kolmo reads the structure of its input — tables, logs, prose, scientific records and images, executables, archives — and
stores it in a form that entropy coders compress far better, then encodes independent, checksummed blocks on every core.
It has six modes, from the fastest, `rt`, to the context-mixing archive tier `ultra`. In every mode the goal is the same:
**at the speed of the leading compressor of that class, produce smaller files.**

## Results

Every coder is Kolmo's own: no zstd, Brotli, LZ4, liblzma or xxHash in the code, the build or the bindings (container
version 12). Kolmo does not read files written by its 1.x versions (`KOLMO_ERR_LEGACY`; see Status).

21 files held out from all tuning (364 MB: the Silesia corpus files mozilla, mr, nci, osdb, reymont, samba and sao, an
OWID CSV table, a server log, a 47 MB bf16 model-weights file, a FASTQ run, two raw CIF videos, four photos and four
audio clips). Apple M5, one thread (`-T 1`), every output decoded and compared byte for byte. Ratio = total input /
total output; speeds in MiB/s are total input over total time, compress / decompress. Sizes are from this release's
code; speeds are medians of 5 timed runs of its release candidates, with the same coders (eco's speed was measured
before a final encoder tuning that made its output 0.2% smaller).

| kolmo mode | Ratio | MiB/s | Nearest rival | Ratio | MiB/s | kolmo size vs rival |
|---|---|---|---|---|---|---|
| `speed` | 1.66 | 413.8 / 733.7 | lz4 -1 | 1.50 | 496.8 / 757.8 | **-9.8%** |
| `balanced` | 2.59 | 163.3 / 397.1 | zstd -3 | 2.04 | 266.9 / 498.7 | **-21.2%** |
| `eco` | 2.79 | 13.3 / 382.5 | zstd -9 | 2.20 | 87.8 / 491.5 | **-21.0%** |
| `hc` | 2.87 | 4.9 / 387.7 | zstd -19 | 2.41 | 5.0 / 451.1 | **-16.0%** |
| `ratio` | 3.15 | 4.3 / 60.7 | xz -9 | 2.77 | 3.7 / 76.4 | **-12.2%** |

The rivals are their command-line tools (lz4, zstd, xz) on the same machine and files. In short: each kolmo mode is
9.8–21.2% smaller than the rival of its class; it compresses 0.6–1.2× as fast as that rival (eco 0.15×) and decompresses
0.8–1.0× as fast. Blocks are independent, so with more threads both directions scale with cores; these figures are
single-thread only. Figures for the 1.x line (measured with its library coders, on other data) are in
[`benchmark/BENCHMARKS.md`](benchmark/BENCHMARKS.md), marked as such. How we weigh a ratio gain against a speed loss is
in [`benchmark/TRADEOFFS.md`](benchmark/TRADEOFFS.md).

## How it works

| Data | Model |
|---|---|
| Tables (CSV/TSV) | Columns become typed streams. Numeric columns are exact decimals at a common scale, predicted per block by raw, delta, delta within an entity (e.g. country), **delta-of-delta** and **third-order** within an entity (for counters and steady trends), or a discovered **formula of other columns** (`y = k·a/b`, change and % change): 51 formulas on the OWID energy table. Quoted fields, short rows and odd values are lossless. |
| Logs | Lines are split into **templates and variables**; timestamps, IPs and IDs become integer columns ("skeleton" columns). |
| Fixed-width text | Lines that come in a few fixed lengths (chemistry and geometry files, fixed-width reports) are stored **column by column per line length**: a line scan and a transpose, several times cheaper than templates. |
| Prose and markup | `balanced`: blocks of up to 6 MiB, each coded with KLZH2 over the whole block. `eco`: KLZH2 level 7 (block sorting of the plain text with `-DKOLMO_ECO_TEXT_BWT=1`). `hc`: frequent words become 2-byte codes from byte values the block never uses, then KLZH2 level 9 over blocks of up to 128 MiB. |
| Decimal doubles | Float64 arrays whose values were written in decimal (`.npy` tables, raw arrays, the columns inside pickle, `.npz` and `.mat` files) are stored as **integers over a power of ten per column**, with missing values as a tag and anything else kept verbatim. |
| Scientific binary | **Fixed records** (period found by self-similarity; fields of 1/2/4/8 bytes stored raw, as deltas, as dictionary tokens, or as multiples of a discovered quantum with exact bit corrections) and **16-bit images** (byte order, row stride and predictor found automatically). |
| Executables, archives | x86 call/jump transform; each tar member gets its own model. |
| Coders | All Kolmo's own (no zstd, Brotli, LZ4, liblzma or xxHash): KLZ (`rt`), KLZH2 (`fast`, `eco`, `hc`), BWT (LZP + libsais-derived suffix sorting + QLFC, in C) and KLZMA (`max`), khuf Huffman leaves, and an integer **context-mixing** coder (`ultra`: orders 1–7, words, columns, indirect contexts, bit histories, neural mixing, APMs), best per block. |
| Container | Independent blocks with an index, so any byte range decodes alone. a 64-bit checksum per block (`kolmo_khash64`, Kolmo's own) and CRC32C over the header and index: corrupt input is reported, never returned as data. Blocks compress and decompress in parallel, so throughput scales with cores. |

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
`-x -l 2` goes further for ratio at the cost of decode speed: numeric graph leaves may also use context mixing when
that is smaller (measured −11% to −15% on integer series such as sensor and ECG samples, about −2% on a mixed
numeric corpus), and those blocks then decode at context-mixing speed. Plain `-x` is unchanged; any decoder reads both.
At `-x` and `-x -l 2`, a FASTQ file of up to 128 MiB is also coded by stream family (names, bases, qualities, each
with its own model; `src/experimental/KFAM.md`) and that coding is kept when smaller: 25% smaller than `-x` before
on three Illumina runs, and such files decode in seconds (7 to 25 MB/s) instead of at context-mixing speed. Any
0.0.x decoder reads them.

`--native-leaf` (with `balanced`; C API: `transforms |= KOLMO_T_NATIVE_LEAF`) codes integer-series residuals with
Kolmo's native plane-context leaves when that is smaller: measured −11.3% on 180 MB of ECG and numeric columns from
the holdout sources (ECG −11.9%, power −9.8%), and such files decode about 1.5–2.6× slower than default `balanced`
(2.3× on that data, about 580 MB/s on Apple silicon). Any 0.0.x decoder reads
them; files that do not use it are unchanged.

Advanced settings, for tuning between the three:

| Mode | Block | Coder | Use |
|---|---|---|---|
| `rt` | adaptive (≤4 MiB) | KLZ | decode speed first |
| `fast` | adaptive (≤4 MiB; plain data up to 6 MiB) | KLZH2 level 3 | default (`-q`: quick analysis, cheap transforms only) |
| `eco` | 16 MiB (text up to 32 MiB) | KLZH2 level 7 | lowest storage cost at moderate CPU |
| `hc` | 16 MiB (text 128 MiB) | KLZH2 level 9 | archive with fast reads |
| `max` | 128 MiB | BWT for text; KLZMA otherwise, or BWT where it is at least 3% smaller (`-l 1`: BWT only) | cold archive |
| `ultra` | 128 MiB | best of context mixing (with an image model for 8-bit images), KLZMA, BWT (`-l 1`: context mixing model 1 only, faster; `-l 2`: maximum ratio, numeric graph leaves may also use context mixing, slower decode) | coldest archive, best ratio |

`ultra` sizes its context-mixing tables to the block: about 16 times the block (a 10 MB file decodes in about 200 MiB;
`--x-tables=full` restores rc2's tables of up to 5.4 GiB, slightly smaller on text). On Linux the tables use transparent
huge pages.

### Memory

`-M SIZE` / `--memlimit SIZE` (or `KOLMO_MEMLIMIT`) sets one limit for compressing and decompressing: `512M`, `8G`,
`50%` of RAM, `auto` (the default: 50% of RAM, or of the container's limit) or `max`. Kolmo adapts how much runs at once
to the memory that is actually free (it waits instead of swapping, and speeds up again when memory frees), but **the
output never depends on it**: the same input and settings give the same bytes on every machine. Only an explicit size
changes the output, so that it decodes within that size: `ratio` and `-x` then use smaller blocks and `-x` smaller
tables. Every file declares what it needs to decode; a decoder with less stops before allocating anything, with the
figure (`needs 4.7 GiB to decode (limit 2.0 GiB); run with --memlimit 5G or --memlimit max`). Floors: speed and
balanced 64 MiB, eco and hc 256 MiB, ratio 1 GiB, `-x` 256 MiB. Details: `docs/MEMORY-MODEL.md`.

### Columnar decode (tables)

A table block can be decoded straight to **typed columns** — numeric columns as 64-bit integers at a common scale
(plus a validity flag per cell), text columns as offset/length into the block's bytes — skipping CSV row reassembly
and decimal formatting (`tbl2_columns` / `exp/colbench.c`). An analytical reader (Arrow, DuckDB, pandas) that would
otherwise decompress to CSV and re-parse reaches typed values about **3× faster**, and the columnar path is 1.2–1.4×
faster than full row decode on its own. The default lossless round trip is unchanged.

### Dictionaries and trained profiles: not in this build

Dictionaries (`-D`, `-t`) and trained profiles (`kolmo train`, `-P`) were built on the zstd and Brotli dictionaries of
earlier versions. This build has only Kolmo's own coders (container version 12), so they are not available: the
options and the C, Python and Rust calls report `KOLMO_ERR_UNSUPPORTED` ("not available in this build: dictionaries
and trained profiles need library codecs").

### Targets (`--target`)

Instead of a mode, say what matters; Kolmo picks the setting per 16 MiB chunk that is cheapest under it, from trial
compressions of the chunk and a table of calibrated speeds:

```bash
./build/kolmo --target 'decode>=1GB/s' in out        # smallest output that decodes at 1 GB/s per core
./build/kolmo --target 'cost:cloud=12' in out        # storage for a year + CPU, benchmark/TRADEOFFS.md model 3
./build/kolmo --target 'cost:link=100Mbit' in out    # compress + transfer + decompress time
./build/kolmo --target 'cost:size/1e9*0.02 + enc*1e-5' -v in out   # any formula of size, n, enc, dec
./build/kolmo calibrate                              # measure this machine (~/.config/kolmo/calibration)
```

The output is an ordinary frame. How close the choices come to the best fixed setting is measured in
`benchmark/rc3/features-a/RESULTS.md`.

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
original size, raw block size and block count of a frame (each zero field disables that limit; exceeding one returns
`KOLMO_ERR_LIMIT`) and the memory (`max_memory`: 0 = auto, `KOLMO_MEM_MAX` = none; a frame that declares more returns
`KOLMO_ERR_MEMORY` before anything is allocated, `kolmo_decode_memory` gives its figure). `kolmo_check_frame` runs the
same checks before the caller allocates the output buffer. Calls without limits use auto.

## Install

| | Install | Package |
|---|---|---|
| CLI (macOS, Linux) | `brew install adnaneben/kolmo/kolmo` | [adnaneben/homebrew-kolmo](https://github.com/adnaneben/homebrew-kolmo) |
| Python 3.8+ | `pip install kolmoc` | [kolmoc on PyPI](https://pypi.org/project/kolmoc/) |
| Rust | `cargo add kolmo` | [kolmo on crates.io](https://crates.io/crates/kolmo) |

- **Homebrew** builds the `kolmo` command from source in a few seconds. It needs nothing else: Kolmo has
  no third-party codec library.
- **Python:** the package is published as `kolmoc` because `kolmo` was already taken on PyPI, but you still
  `import kolmo`. Prebuilt wheels cover Linux (x86-64, aarch64) and Apple silicon Macs
  (macOS 11 or later). Other platforms, Intel Macs included, build the sdist, which needs a C compiler and
  takes about a minute.
- **Rust:** the crate compiles the C core in `build.rs` and links it statically. It needs a C compiler at
  build time.
- **No codec libraries.** From 0.0.1 every coder is Kolmo's own (KLZ, KLZH2, KLZMA, khuf, BWT, context mixing,
  kimg and the audio, video and FASTQ coders). The packages need only a C compiler.

Usage for each binding is in [`bindings/python`](bindings/python/README.md) and
[`bindings/rust`](bindings/rust/README.md). To build the CLI and library from source, see below.

## Build and use

The examples below run the source build (`./build/kolmo`). After `brew install`, the same commands work
with plain `kolmo`.

```bash
make                                    # build/kolmo, build/kolmobench, build/kolmotest (Linux, macOS; gcc or clang)
make check                             # resource failures, concurrent callers, round trips, ranges and corruption
./build/kolmo -m ratio in.csv out.kolmo     # compress: -m speed|balanced|ratio (-x: smallest of all), -T threads, -M memlimit; advanced: -m rt|fast|eco|hc|max|ultra, -l level, -q
./build/kolmo -d out.kolmo back.csv         # decompress (auto-detects one-shot vs streamed input)
./build/kolmo -s huge.csv huge.czs        # streaming compress: bounded memory, any size (-S N for N-MiB segments)
./build/kolmo -r 1000000:4096 out.kolmo x   # read 4 KiB at offset 1,000,000 without decoding the rest
./build/kolmo -V                            # version and container version
./build/kolmobench -o results.tsv file      # in-memory benchmark of kolmo's modes (compare rivals with their own CLIs)
```

Needs only a C compiler. There is no third-party codec library and no `vendor/` directory: every coder is in
`src/algo` and `src/codecs`. The BWT coder is ported from libbsc and its suffix sorter is taken over from libsais;
both are Apache-2.0 and listed in `NOTICE`.

On a Mac: `bash mac/setup.sh` once, then `bash mac/bench.sh` for the full benchmark and `bash mac/tune.sh` for an
overnight genetic search of the build-time settings (`tune/ga.py`).

## Status

0.0.2, the second release of the renumbered line (0.0.1 had the same code; 0.0.2 corrects the package
documentation). The 1.x line (1.0.0 and 1.0.1, now yanked) used library coders (zstd, Brotli, LZ4, liblzma); 0.0.x
uses only Kolmo's own and writes container version 12 (`SPEC.md`). Output is byte-identical across x86-64 and Apple
silicon. The decoder is fuzzed (harnesses in `fuzz/`, run under ASan/UBSan in CI); corrupt input is reported, never
returned as data. `RELEASING.md` describes how to cut a release.

### Files written by Kolmo 1.x

0.0.x does not read them: it reports `KOLMO_ERR_LEGACY` ("written by an earlier Kolmo (1.x or a release candidate)
that used library codecs") rather than returning wrong data. To move such a file, decompress it with kolmoc 1.0.1,
which stays installable from PyPI by its exact version (pip warns that it is yanked; that is expected), then
compress it again:

```bash
python3 -m venv kolmo1 && kolmo1/bin/pip install kolmoc==1.0.1
kolmo1/bin/python -c 'import kolmo,sys; open(sys.argv[2],"wb").write(kolmo.decompress(open(sys.argv[1],"rb").read()))' old.kolmo plain
kolmo -m balanced plain new.kolmo
```

Dictionaries (`-D`), `kolmo train` and trained profiles are not available in 0.0.x (`KOLMO_ERR_UNSUPPORTED`): they
relied on library coders.

## Layout

```
src/kolmo.h        the public API      src/kolmo.c   the library: includes the parts below (docs/LAYOUT.md)
src/api, core      entry points; container, blocks, thread pool, encoder, decoder, streams
src/plan           analysis and plan per block      src/transforms   table, log, lines, sci, word, graph, ...
src/codecs         one file per block codec (kolmo_codec table)    src/algo, arch   algorithms; CPU kernels by ISA
src/cli/cli.c      command line          bench/kolmobench.c   benchmark          tests/kolmotest.c   tests
tune/ga.py         genetic tuning        mac/              Mac scripts
bindings/python    PyPI package kolmoc   bindings/rust     crate kolmo        packaging/homebrew  brew formula
```

## Licence

Apache-2.0 (see LICENSE). Code ported from libbsc and libsais keeps its original copyright and licence (see NOTICE).
