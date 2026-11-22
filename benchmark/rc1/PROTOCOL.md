# Kolmo 1.2.0-rc1 against the leaders: protocol

Written on 14 November 2026, before the first timed run. Changes made after that date are listed at the end with
their reason.

## The rule

Every program is measured on the same machine, on one thread, in the same session. No timing is taken from a
cache, from an earlier day or from another machine. Where a rival cannot be run here, the report says so and gives
no timing for it.

## Machine and builds

- MacBook Pro (Mac17,2), Apple M5, 4 performance and 6 efficiency cores, 24 GiB of memory, mains power, low power
  mode off. macOS cannot pin a process to a core.
- Kolmo: commit `831b980` (version 1.2.0-rc1), built with the release flags, `make ARCH= lib` (`-O3`, no `-mcpu`).
  A second build of the same commit with `-DKOLMO_FORCE_GENERIC_INT_INV` is the control for the decoder's integer
  fast path.
- Rivals: the versions inside lzbench 2.4.1 (commit `9be0e25`), built with lzbench's default flags (`-O3 -DNDEBUG`,
  no `-mcpu`). Same compiler on both sides (Apple clang 21.0.0), and neither side is tuned for this CPU.
- `benchmark/lzbench/setup_macos.sh` adds Kolmo to lzbench as two ordinary codecs, `kolmo` and `kolmo_ctl`. Each
  loads its own shared library, which exports only `kolmo_*`, so Kolmo never runs lzbench's copy of zstd, lz4,
  brotli, libbsc or liblzma, and no rival runs Kolmo's.

## Tier A: one program times everything (lzbench)

lzbench reads the file into memory, times each compression and each decompression call, checks that the decoded
bytes equal the input, and reports the fastest iteration. Speeds are in MB/s with 1 MB = 1,000,000 bytes. Kolmo
runs with `threads = 1` and its default block size; every rival runs with lzbench's defaults, which use one
thread. The whole file is one buffer (`-b0`, the default).

`run_lzbench.py` starts one lzbench process per file and setting and holds the full lists. In short:

| Tier | Input | Minimum time (compress, decompress) | Settings |
|---|---|---|---|
| A1 | `silesia.tar`, 211,948,032 bytes, the twelve Silesia files in alphabetical order | 1 s, 2 s | 41: every Kolmo mode and every rival named below |
| A2 | 35 files of 14 data types, 497.1 MB | 1 s, 1 s | 22 per file |
| A3 | 18 typed numeric arrays (NumPy files and raw little-endian integers) | 1 s, 1 s | 14 per file, plus OpenZL's numeric profile on raw arrays |

Minimum number of iterations: three each way for fast and medium settings; one compression and three
decompressions for settings that compress slowly and decompress quickly (zstd 19 and 22, xz, lzma, brotli 11,
Kolmo `hc`, `ratio` and `max -l 1`); one each way for settings that are slow both ways (context mixing, PPM).

Kolmo's lzbench levels:

| Level | Command line | Level | Command line |
|---|---|---|---|
| 0 | `-m rt` | 9 | `-m hc` |
| 1 | `-m fast -l 1 -q` | 10 | `-m ratio` |
| 2 | `-m fast -l 2 -q` | 11 | `-x -l 1` |
| 3 | `-m fast -q` | 12 | `-x` |
| 4 | `-m balanced` (the default) | 13 | `-x -l 2` |
| 5 | `-m hc -l 9` | 14 | `-m balanced --native-leaf` |
| 6 | `-m hc -l 12` | 15 | `-m balanced --typed-huf` |
| 7 | `-m max -l 1` | 16 | `-m speed` |
| 8 | `-m eco` | 17 | `-m fast -l 1 -Q` |

### Checks on every row

1. **One thread.** The driver reads the lzbench process's CPU time when it ends. CPU time above elapsed time (by
   more than 3% plus 0.05 s) marks the row `threads`.
2. **Quiet machine.** Nothing else of mine runs during a tier. The nightly watcher's benchmark (`bench_mac3`) is
   looked for before and after every row; a hit marks the row `busy`. Any other process that used at least half
   a core just before a row (the system's Spotlight indexer, for example) is written down with the row.
3. **Round trip.** A row whose decoded bytes differ from the input, or that fails, is an error.
4. Marked and failed rows are run again. A row that stays marked is reported as marked, not dropped.
5. **Drift.** `lz4`, `kolmo,16`, `zstd,3` and `kolmo,4` are measured first and again at the end of each tier.
6. **Same bytes as the command line.** `check_wrapper_sizes.py` compares the size from `kolmo`, `kolmo_ctl` and
   `kolmo -T1` for all 18 levels on seven files. This is not timed.

## Comparisons fixed in advance

The Kolmo setting compared with each leader was chosen before measuring. Other settings may be shown beside it,
labelled as such.

| # | Area | Leader | Kolmo setting | Input |
|---|---|---|---|---|
| 1 | Lowest latency | lz4; also lzav 1, zxc 1 and 3, snappy, lzfse | `-m speed` | A1, A2 |
| 2 | Fast general purpose | zstd 1 | `-m fast -l 1 -Q` | A1, A2 |
| 3 | Balanced default | zstd 3 | `-m balanced` | A1, A2 |
| 4 | DEFLATE | libdeflate 6; also zlib-ng 6 | `-m balanced` | A1, A2 |
| 5 | Brotli | brotli 5; brotli 11 | `-m balanced`; `-m hc` | A1, A2 |
| 6 | High compression, quick decompression | zstd 19; also zstd 22 | `-m hc` | A1, A2 |
| 7 | LZMA2 | xz 9; also lzma 9 | `-m ratio` | A1, A2 |
| 8 | Strong practical compressors | bsc, bzip3, kanzi 9; also kanzi 7, ppmd8 | `-m ratio` | A1, A2 (bsc, bzip3) |
| 9 | Extreme ratio | zpaq 5 here; paq8px and cmix cannot run in lzbench | `-x`; also `-x -l 2` | A1 |
| 10 to 22 | By data type: CSV, JSON, logs, numeric arrays, float arrays, images, audio, model weights, DNA text, executables, text | the best general rival in A2 or A3, and the specialist of tier S | `-m balanced`, `-m ratio`, `-x`; opt-in settings labelled | A2, A3, S |

Words used in the report: **ahead** means smaller and not slower; **behind** means larger, or the same size and
slower. Sizes within 1% and speeds within 10% count as the same. Every verdict is printed with its numbers.

For a group of files, the size is the sum of the sizes and the speed is the total bytes divided by the total of
the per-file fastest times.

## Tier S: specialists that lzbench does not contain

pcodec (numeric arrays), FLAC (audio), JPEG XL lossless (images) and OpenZL's `zli` with its CSV profile and with
trained compressors (tables). Each is called from one Python process that also calls Kolmo through its own Python
binding with one thread. CPU time and elapsed time are read around every call, so a specialist that uses more than
one thread is seen and either limited to one thread or reported as marked.

## Tier B: whole process (optional)

The command-line tools as a user runs them, each limited to one thread, timed with `wait4`. It adds process
start, file reading and writing, and peak memory per tool. It is run only if time remains after A and S, and only
for what A cannot show (for example 7-Zip with its executable filters).

## Not measured here

- paq8px and cmix: not in lzbench. paq8px `-8` runs at about 20 KB/s on this Mac and was measured here on 13
  November 2026 on 23 files; those sizes may be quoted, labelled with their date. cmix needs more memory than this
  Mac has. Public table figures are labelled as such.
- Other processors, Linux, more than one thread, small-file and dictionary use.

## Changes after freezing

Tier A ran as frozen. The entries below concern tiers S and B only. All were written on 14 November 2026 between
19:00 and 19:35, after tier A1 and before any timed run of tier S or B. The smoke tests that led to them ran on one
file per data type between tiers; none of their numbers is used.

1. **OpenZL's `zli` is measured as a whole process, not inside Python.** `zli` is a command-line program, so it
   cannot be called from the Python process as the tier S text says. It is compared with the Kolmo command line in
   `run_cli.py`: both are started as processes, and each time includes process start and reading and writing the
   file. `wait4` gives the CPU time and the peak memory. `zli` has no thread option; the CPU-time check shows
   whether it stayed on one thread. On files under about 1 MB these times are mostly process start.
2. **More `zli` profiles than the CSV one.** The OpenZL entries inside lzbench accept raw integer arrays only, so
   `zli` also runs with its profiles for 16-bit samples (`le-u16`, `be-u16`), 8-bit samples (`u8`), integers
   (`le-i32`, `le-i64`), NumPy, pickle, ONNX, GGUF, safetensors and the star catalogue (`sao`, and `sddl2` with
   OpenZL's own example description of that file). Levels 6 and 9 of each.
3. **Timing loop of `run_inprocess.py`** (pcodec, FLAC, JPEG XL, Kolmo). Each call is repeated until it has run 3
   times and for 0.25 s in total, or for 3 s, or 5,000 times; the fastest run is kept. Kolmo is timed twice:
   through its Python binding, which copies the result, and as the bare library call with the buffers allocated
   beforehand. FLAC is timed in memory and through a file, because the audio library's in-memory path goes through
   Python callbacks. JPEG XL runs with the plugin's thread pool switched off for encoding and decoding.
4. **Timing loop of `run_cli.py`.** The fastest of up to three rounds, within 20 s per setting. `-x` runs only on
   files up to 16 MiB there; tier A covers `-x` on every file.
5. **The word "Trade".** `report.py` prints "Trade" where Kolmo is smaller but more than 10% slower. The frozen
   text only defined "ahead" (smaller and not slower) and "behind" (larger, or the same size and slower).
6. **7-Zip rows (tier B).** The test files have no file extension, and 7-Zip then applies no executable filter
   unless it is told to look inside the file. Three rows replace the two planned ones: `7zz a -t7z -mx9 -myx9
   -mmt1` (7-Zip analyses the file and picks its filter; with the BCJ2 filter it uses a little more than one
   thread and the row is marked), the same with `-mmtf=off` (the filter runs on the one thread), and `-mf=off`
   (plain LZMA2). 7-Zip's xz container was dropped: it used two threads whatever `-mmt` said. Plain LZMA2 is in
   tier A as `xz -9` and `lzma -9`.

The entry below was written on 14 November 2026 at about 21:40, after every timed run had finished. It changes no
measurement; it concerns what the tables show.

7. **A section "Further comparisons" in the tables.** `report.py` also prints, from the same rows: the data-type
   pairs of areas 10 to 22 one per line with their standing; the totals of the settings measured on all 35 files
   of A2; the specialist pairs of tier S one per line (pcodec at its levels 8 and 12 on subsets of the numeric
   arrays, JPEG XL, FLAC; the lines against pcodec level 12 were added at about 21:55, from the same rows);
   and the time of one call in milliseconds on the inputs under 1 MB. It also prints a table "Pairs not fixed
   before measuring": `-m fast -q` and `-m balanced` against zstd 9, `-m hc -l 9` against zstd 19, `-m max -l 1`
   against bzip3 5, kanzi 7 and xz 9, and `-m ratio` against zpaq 5. Those pairs were chosen after the results
   were seen. They show where a setting lands and are not part of the comparisons fixed in advance. The same holds
   for the extra settings shown beside the fixed pair in areas 1 to 9 (`-m rt`, `-m fast -l 1 -q`, `-m fast -q`,
   `-m eco`, `-m hc -l 9`, `-m hc` against xz 9, `-m max -l 1` against bsc, `-x -l 1`, and `-x` against kanzi 9).
   In the tables, "S1" is tier S as run inside one Python process (`run_inprocess.py`) and "S2" is the
   whole-process part (`run_cli.py`): OpenZL's `zli` (change 1) and the 7-Zip rows of tier B.
