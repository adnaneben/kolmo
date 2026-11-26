# Small-input benchmark

Kolmo levels 1-9 at one and ten threads against zstd, Brotli and LZ4 on items of exactly 64 B, 256 B, 1 KiB, 4 KiB,
16 KiB and 64 KiB. The items are cut from six kinds of data, and each one is compressed on its own: the case of a
key-value store, a message bus or a log shipper. Review 40, claims #173 (the benchmark), #90, #92 and #12. Results:
[RESULTS.md](RESULTS.md).

    make build/libkolmoarch.a                           # in each tree to measure
    benchmark/small/run.sh TREE OUT.tsv [options]       # builds smallbench against TREE's sources, runs it
    benchmark/small/compare.sh BASE NEW OUTDIR -T 1,10  # both trees, interleaved per kind and size
    python3 benchmark/small/summarize.py OUTDIR/base.tsv OUTDIR/new.tsv

`smallbench.c` options: `-q` (sizes and memory only), `-s 64,256,...` (sizes), `-l 1,3` (Kolmo levels), `-T 1,10`
(Kolmo threads), `-c kolmo,zstd,brotli,lz4` (coders). Timed runs go through the review workspace's `tools/timed.sh`,
one timed workload at a time. `compare.sh` alternates the two builds at every (kind, size), in swapped order each
step, so that load from other work on the machine falls on both alike. Two sequential runs differed by 2-4x on
settings whose code did not change.

## What is measured

Each row of the TSV is one (kind, size, coder, level, threads) setting over all the items of that kind and size.

- **Size:** the sum of the output bytes, and the ratio. The column `hash` is an FNV-1a hash of every output in order:
  two builds wrote the same bytes when their hashes match.
- **Time:** microseconds per call, compress and decompress. Each figure is the best pass of at least three (and at
  least 0.15 s), after a warm-up pass. Kolmo uses its one-shot buffer API. The rivals use their usual one-shot calls
  with a reused context (zstd CCtx/DCtx, LZ4F contexts), which is their fastest setting for small items. Brotli has no
  context reuse, so it gets a fresh instance per call, as `BrotliEncoderCompress` does.
- **Memory:** the heap working set of one call, the peak of live heap bytes during it above what was live before.
  - Rival contexts are created and freed inside the measured call, so their size counts as Kolmo's per-call
    allocations do.
  - Kolmo is compiled from source for this. `run.sh` compiles `src/kolmo.c` and every C source of `libkolmoarch.a` to
    assembly and renames their `malloc`, `calloc`, `realloc` and `free` calls to the harness's counters (Mach-O
    symbol names, so macOS only). The rivals are counted through their custom-allocator hooks.
  - Not counted: thread stacks and mmap. No route these inputs take maps memory.
- **Round trip:** every item is decoded and compared with `memcmp`. A failure stops the run.
- **Container:** `frame_bytes` is the part of Kolmo's output that is container: 32 bytes per frame and 20 per block.

## Data

All from the review workspace (`$V`; the list is `sets.txt`), public sources:

| kind | file | cut |
|---|---|---|
| json | `review40b/work/R21/data/gh_test.ndjson` (GH Archive 2015-01-01 hour 19 events) | at a line start |
| log | `review40b/work/R21/data/nasa_test.log` (NASA HTTP, Aug 1995) | at a line start |
| csv | `review40b/work/R21/data/ghcn_test.csv` (GHCN daily rows) | at a line start |
| kv | `corpus-holdout/osdb` (MySQL table file, Silesia) | at a multiple of the size |
| code | `corpus-holdout/samba` (source tarball, Silesia) | at a line start |
| binary | `corpus/ooffice` (x86 executable, Silesia) | at a multiple of the size |

Each kind and size has min(256, 4 MiB / size) items, but at least 16, spread evenly over the file and not
overlapping. A cut keeps the item at exactly the stated size, so a 64 B item is part of a line.
