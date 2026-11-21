# Wave 4, BWT lane: block-sorting pipeline of `ratio` (codec 18)

Branch `claude/w4-bwt-20261118`, base rc3 `5071f388`. Scope: LZP, forward BWT (suffix sort), inverse BWT, block
buffers and threading. QLFC (the entropy stage) belongs to the entropy lane. Nothing in `src/algo/qlfc.c` changed; the
only interface assumed is `qlfc` coding from one buffer into another of capacity `cap`, as before.

**No format change.** Every frame is byte-identical to rc3. The sweep below checks this for each file, and the
determinism hashes are unchanged (`benchmark/determinism/check.py` passes without regeneration).

## Changes

1. **Huge pages for the suffix array and the inverse's index array** (`src/algo/bigmem.h`, new). Arrays of 4 MiB or
   more are `mmap`ed and `madvise(MADV_HUGEPAGE)`d on Linux (malloc elsewhere, or with `KOLMO_NO_HUGEPAGE=1`).
   Suffix sorting and the inverse are random access over 4n bytes, so TLB misses dominate them.
   - Inverse BWT: +33% (1 thread) and +40% (4 threads) on enwik8.
   - Suffix sort: about +15% at 1 thread on 100 MB blocks.
   - Used by codecs 5 and 18.
2. **Codec 18 works in 4n instead of 5n+.**
   - Encoder: when the caller's output slot has room (always, for `ratio` blocks), LZP writes into that slot past the
     header and the BWT runs in place there. QLFC codes into the (now free) suffix array, then the header and payload
     are copied to the front of the slot. Otherwise (small `cap`), the old copy path runs; the test checks that both
     paths write the same bytes.
   - Decoder: QLFC decodes straight into the output and the inverse runs in place there. Its only extra buffer is the
     index array P (4n).
   - enwik8, 1 thread: compress RSS 664 → 600 MiB.
   - enwik9: compress RSS 1857 → 1740 MiB (1 thread), 3888 → 3546 MiB (4 threads); decompress 788 → 666 MiB (1 thread).
3. **Unrolled LZP encoder** (`src/algo/lzp.c`). It follows libbsc's large-input fast path: four positions per 8-byte
   load, one rolling hash, far compare before near compare. It is used for hash sizes ≤ 17 (codec 18 uses 17/96);
   the old loop stays as `lzp_encode_block_ref`.
   - Output is identical to the reference. `tests/test_bwt.c` compares the two over 10 data kinds × 9 sizes ×
     10 (hash, min) pairs × 1 and 4 threads; they were also compared on 16 real files × 3 parameter sets.
   - Speed at 17/96: enwik8 253 → 357–403 MB/s, dickens 216 → 580–660 MB/s. At 15/72 (libbsc's default):
     363 → 505 MB/s.
4. **No discarded word transform in `ratio`** (`src/core/blocks.c`). The planner ran `word_encode` on text blocks
   whose codec (CM, or LZMA at the levels that drop it) then threw the result away. That was 10% of `ratio`'s encode
   time on dickens in callgrind. It is now skipped, and the frames are identical (checked on dickens, enwik8, nci
   and enwik9).

The Python and Rust bindings' `native/` copies are synced (`sync-native.sh`). NOTICE and the libsais/libbsc headers
are unchanged.

## Method

- VM: Xeon @ 2.1 GHz, 4 vCPU, gcc 13, `-march=native`, Linux THP policy `madvise`. The VM is noisy: about ±20% run
  to run on sub-second files.
- The three tools alternate per file inside each round. Each figure is the best of 3 rounds (1 round for enwik9).
  1-thread and 4-thread runs are separate passes. Peak RSS comes from `wait4`.
- Scripts: `run_all.sh`, `bench.py`, `run1.py`, `table.py`. Raw rows are in `rows/t1.jsonl` and `rows/t4.jsonl`.
- Tools:
  - `base`: rc3 at `5071f388`.
  - `new`: this branch.
  - `bsc`: libbsc 3.4.0, the copy that was vendored in this repo before `818d3acc` (Apache-2.0), driven by
    `bsclike.c` as `bsc e -m0 -e2 -b25`: LZP 15/72, BWT, adaptive QLFC, OpenMP over the blocks. It is a test tool
    only and is not linked into Kolmo. Build:
    `g++ -O3 -march=native -fopenmp -DLIBBSC_OPENMP_SUPPORT -DLIBSAIS_OPENMP -DNDEBUG` over libbsc's sources plus
    `bsclike.c`.
- **bzip3 was not measured.** Building downloaded bzip3 sources (and installing the PyPI binding) was refused by
  this environment's policy, so the only external reference is libbsc. The rc3 copy of libbsc is the only one that
  could be built here.
- Stage timings: `stages.c` (Kolmo, in memory, one block per file, `HUGE=1` for the huge-page arrays, as the codec
  now allocates them) and `bscstages.cpp` (libbsc, same block). Build stages with
  `gcc -O3 -march=native -Isrc benchmark/w4/bwt/stages.c build/libkolmo.a -lpthread -lm`.

Note on block size: `ratio` codes text in blocks of up to the whole file (100 MB for enwik8, 1 GB split by the
memory model for enwik9). bsc `-b25` codes 25 MB blocks. bsc's speed and memory below therefore come from 4× (or
more) smaller blocks, which is also why Kolmo's files are 6.8–7.3% smaller on enwik8 and enwik9. The same-block
comparison is in the stage table and in the enwik8 `-b100` row.

## Whole files, 1 thread

| file | MB | tool | bytes | vs bsc | compress MB/s | decompress MB/s | compress RSS MiB | decompress RSS MiB |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| dickens | 10.2 | base | 2,254,558 | -0.20% | 10.4 | 16.3 | 103 | 54 |
| dickens | 10.2 | new | 2,254,558 | -0.20% | 11.5 | 15.9 | 79 | 55 |
| dickens | 10.2 | bsc | 2,259,136 | | 16.5 | 17.3 | 53 | 53 |
| mozilla | 51.2 | base | 13,909,331 | -12.95% | 2.2 | 78.2 | 238 | 33 |
| mozilla | 51.2 | new | 13,909,331 | -12.95% | 2.2 | 94.0 | 215 | 33 |
| mozilla | 51.2 | bsc | 15,977,818 | | 13.8 | 13.4 | 142 | 124 |
| mr | 10.0 | base | 1,989,774 | -9.80% | 2.0 | 18.9 | 156 | 34 |
| mr | 10.0 | new | 1,989,774 | -9.80% | 2.2 | 18.8 | 156 | 35 |
| mr | 10.0 | bsc | 2,206,014 | | 20.3 | 18.7 | 42 | 40 |
| nci | 33.6 | base | 1,199,630 | -5.27% | 8.0 | 58.9 | 324 | 145 |
| nci | 33.6 | new | 1,199,630 | -5.27% | 7.1 | 46.9 | 328 | 149 |
| nci | 33.6 | bsc | 1,266,324 | | 29.3 | 54.6 | 115 | 113 |
| ooffice | 6.2 | base | 2,091,923 | -18.26% | 2.0 | 79.9 | 80 | 15 |
| ooffice | 6.2 | new | 2,091,923 | -18.26% | 2.0 | 64.1 | 80 | 16 |
| ooffice | 6.2 | bsc | 2,559,110 | | 13.3 | 9.6 | 33 | 33 |
| osdb | 10.1 | base | 2,206,574 | -0.82% | 10.7 | 17.1 | 69 | 54 |
| osdb | 10.1 | new | 2,206,574 | -0.82% | 9.0 | 15.7 | 74 | 54 |
| osdb | 10.1 | bsc | 2,224,782 | | 14.3 | 15.0 | 52 | 52 |
| reymont | 6.6 | base | 978,208 | -0.42% | 15.1 | 24.1 | 50 | 36 |
| reymont | 6.6 | new | 978,208 | -0.42% | 15.1 | 23.1 | 51 | 37 |
| reymont | 6.6 | bsc | 982,366 | | 20.9 | 20.1 | 36 | 36 |
| samba | 21.6 | base | 3,975,940 | +1.69% | 3.9 | 42.1 | 124 | 28 |
| samba | 21.6 | new | 3,975,940 | +1.69% | 3.9 | 33.6 | 128 | 29 |
| samba | 21.6 | bsc | 3,909,930 | | 17.2 | 18.9 | 90 | 86 |
| sao | 7.3 | base | 2,184,759 | -53.31% | 5.2 | 103.6 | 62 | 20 |
| sao | 7.3 | new | 2,184,759 | -53.31% | 5.3 | 96.7 | 62 | 20 |
| sao | 7.3 | bsc | 4,679,766 | | 10.1 | 7.2 | 39 | 40 |
| webster | 41.5 | base | 6,439,824 | -3.22% | 14.9 | 26.3 | 242 | 208 |
| webster | 41.5 | new | 6,439,824 | -3.22% | 13.7 | 24.1 | 256 | 209 |
| webster | 41.5 | bsc | 6,654,030 | | 17.4 | 23.6 | 129 | 130 |
| x-ray | 8.5 | base | 3,616,667 | -3.72% | 3.1 | 53.0 | 141 | 31 |
| x-ray | 8.5 | new | 3,616,667 | -3.72% | 3.1 | 54.3 | 140 | 31 |
| x-ray | 8.5 | bsc | 3,756,586 | | 9.5 | 9.8 | 45 | 45 |
| xml | 5.3 | base | 385,428 | -1.97% | 20.6 | 46.1 | 56 | 23 |
| xml | 5.3 | new | 385,428 | -1.97% | 21.3 | 38.5 | 37 | 25 |
| xml | 5.3 | bsc | 393,182 | | 34.9 | 34.3 | 24 | 23 |
| silesia.tar | 212.0 | base | 40,464,096 | -14.64% | 3.1 | 34.3 | 788 | 291 |
| silesia.tar | 212.0 | new | 40,464,096 | -14.64% | 3.1 | 32.6 | 747 | 249 |
| silesia.tar | 212.0 | bsc | 47,402,944 | | 14.3 | 15.7 | 158 | 152 |
| enwik8 | 100.0 | base | 20,798,338 | -6.75% | 10.1 | 16.8 | 664 | 496 |
| enwik8 | 100.0 | new | 20,798,338 | -6.75% | 11.0 | 15.4 | 600 | 497 |
| enwik8 | 100.0 | bsc | 22,304,136 | | 13.7 | 16.7 | 158 | 153 |
| enwik9 | 1000.0 | base | 180,020,338 | -7.33% | 9.1 | 17.4 | 1857 | 788 |
| enwik9 | 1000.0 | new | 180,020,338 | -7.33% | 12.1 | 17.4 | 1740 | 666 |
| enwik9 | 1000.0 | bsc | 194,268,404 | | 15.6 | 17.8 | 158 | 153 |
| owid-co2-data.csv | 15.2 | base | 1,223,833 | -61.27% | 3.0 | 99.6 | 200 | 32 |
| owid-co2-data.csv | 15.2 | new | 1,223,833 | -61.27% | 3.0 | 103.0 | 199 | 32 |
| owid-co2-data.csv | 15.2 | bsc | 3,159,706 | | 15.6 | 18.0 | 70 | 69 |
| owid-energy-data.csv | 9.2 | base | 823,712 | -57.25% | 2.8 | 144.2 | 120 | 21 |
| owid-energy-data.csv | 9.2 | new | 823,712 | -57.25% | 2.9 | 99.2 | 120 | 21 |
| owid-energy-data.csv | 9.2 | bsc | 1,926,740 | | 17.5 | 17.4 | 45 | 45 |
| logs.log | 5.1 | base | 83,513 | -23.16% | 23.8 | 317.7 | 36 | 12 |
| logs.log | 5.1 | new | 83,513 | -23.16% | 21.9 | 338.9 | 36 | 12 |
| logs.log | 5.1 | bsc | 108,690 | | 51.3 | 68.7 | 20 | 18 |
| **total** | 1553 | base | 284,646,446 | -9.93% | 6.3 | 20.5 | 1857 (max) | 788 (max) |
| **total** | 1553 | new | 284,646,446 | -9.93% | 7.0 | 20.2 | 1740 (max) | 666 (max) |
| **total** | 1553 | bsc | 316,039,664 | | 15.4 | 17.4 | 158 (max) | 153 (max) |

## Whole files, 4 threads

| file | MB | tool | bytes | compress MB/s | decompress MB/s | compress RSS MiB | decompress RSS MiB |
|---|---:|---|---:|---:|---:|---:|---:|
| dickens | 10.2 | base / new / bsc | as above | 12.3 / 15.9 / 16.4 | 33.3 / 32.4 / 50.5 | 105 / 81 / 54 | 56 / 56 / 62 |
| mozilla | 51.2 | base / new / bsc | | 5.2 / 5.3 / 25.1 | 174.8 / 227.6 / 21.6 | 361 / 356 / 232 | 54 / 57 / 146 |
| mr | 10.0 | base / new / bsc | | 1.8 / 1.9 / 16.5 | 18.3 / 24.3 / 54.2 | 156 / 156 / 43 | 34 / 36 / 49 |
| nci | 33.6 | base / new / bsc | | 8.7 / 7.9 / 34.5 | 115.3 / 176.6 / 73.3 | 324 / 334 / 147 | 147 / 151 / 121 |
| ooffice | 6.2 | base / new / bsc | | 1.9 / 2.0 / 11.0 | 66.9 / 66.9 / 33.1 | 84 / 86 / 34 | 16 / 16 / 42 |
| osdb | 10.1 | base / new / bsc | | 12.3 / 10.8 / 12.1 | 34.7 / 29.3 / 40.8 | 69 / 78 / 53 | 55 / 55 / 62 |
| reymont | 6.6 | base / new / bsc | | 16.2 / 16.7 / 15.1 | 26.4 / 28.9 / 77.1 | 51 / 52 / 37 | 37 / 38 / 46 |
| samba | 21.6 | base / new / bsc | | 7.7 / 8.0 / 18.7 | 91.9 / 93.5 / 63.5 | 206 / 201 / 91 | 56 / 56 / 97 |
| sao | 7.3 | base / new / bsc | | 4.5 / 5.1 / 10.7 | 76.3 / 87.4 / 23.4 | 62 / 62 / 40 | 20 / 20 / 48 |
| webster | 41.5 | base / new / bsc | | 23.7 / 19.5 / 24.5 | 61.1 / 78.7 / 37.4 | 244 / 268 / 202 | 210 / 210 / 134 |
| x-ray | 8.5 | base / new / bsc | | 3.3 / 2.9 / 9.4 | 47.1 / 51.0 / 25.9 | 141 / 140 / 46 | 31 / 31 / 54 |
| xml | 5.3 | base / new / bsc | | 18.4 / 25.9 / 24.0 | 52.4 / 63.6 / 104.8 | 62 / 37 / 25 | 24 / 26 / 28 |
| silesia.tar | 212.0 | base / new / bsc | | 5.5 / 5.7 / 40.0 | 78.1 / 71.3 / 55.0 | 1169 / 1242 / 443 | 390 / 338 / 434 |
| enwik8 | 100.0 | base / new / bsc | | 16.6 / 19.2 / 47.4 | 47.2 / 61.0 / 54.5 | 665 / 624 / 476 | 498 / 498 / 378 |
| enwik9 | 1000.0 | base / new / bsc | | 34.2 / 41.4 / 54.3 | 60.8 / 63.9 / 67.1 | 3888 / 3546 / 616 | 2152 / 2124 / 495 |
| owid-co2-data.csv | 15.2 | base / new / bsc | | 3.0 / 2.8 / 16.4 | 134.9 / 112.1 / 50.0 | 200 / 200 / 72 | 32 / 32 / 78 |
| owid-energy-data.csv | 9.2 | base / new / bsc | | 2.9 / 3.0 / 16.9 | 131.8 / 106.1 / 49.4 | 120 / 120 / 46 | 21 / 21 / 54 |
| logs.log | 5.1 | base / new / bsc | | 16.7 / 21.2 / 25.3 | 299.0 / 254.1 / 99.7 | 36 / 36 / 21 | 12 / 12 / 23 |
| **total** | 1553 | base / new / bsc | -9.93% / -9.93% / 0 | 13.4 / 14.3 / 40.6 | 62.5 / 66.4 / 57.5 | 3888 / 3546 / 616 | 2152 / 2124 / 495 |

Sizes are the same at 4 threads as at 1 thread for every tool.

Reading the rows:
- The files where `ratio` compresses at 2–5 MB/s (mozilla, mr, ooffice, samba, sao, x-ray, the OWID CSVs, most of
  silesia.tar) spend their time in what `ratio` tries outside this lane: LZMA2 and CM trials and table transforms.
  This lane does not change those rows except through noise. Those files are also where Kolmo is 10–61% smaller.
- The text files that do go to codec 18 (dickens, webster, enwik8, enwik9, reymont, xml) carry this lane's changes.
- The slower decode on nci, samba and owid-energy at 1 thread is noise. In 8 interleaved rounds:
  - nci: base 0.51–0.93 s, new 0.48–0.79 s.
  - samba: base 0.61–0.81 s, new 0.65–0.83 s.
  - owid-energy: best of 3 rounds, 0.062 s vs 0.065 s.

## Same block, per stage (excluding QLFC)

Each file is one block in memory: Kolmo codec 18 stages with LZP 17/96 vs libbsc with LZP 15/72. Figures are MB/s of
input. `enc-noq` is LZP + BWT; `dec-noq` is unBWT + unLZP.

| file | T | Kolmo LZP | bsc LZP | Kolmo BWT | bsc BWT | **enc-noq Kolmo / bsc** | Kolmo unBWT | bsc unBWT | **dec-noq Kolmo / bsc** | Kolmo QLFC enc / dec | bsc QLFC enc / dec |
|---|---|---:|---:|---:|---:|---|---:|---:|---|---|---|
| enwik8 | 1 | 403 | 843 | 25.3 | 20.7 | 23.8 / 20.2 = **1.18×** | 103.5 | 70.4 | 94.2 / 65.9 = **1.43×** | 30.4 / 25.9 | 29.7 / 24.2 |
| dickens | 1 | 660 | 988 | 32.0 | 33.6 | 30.6 / 32.5 = **0.94×** | 120.9 | 84.8 | 107.0 / 79.5 = **1.35×** | 28.3 / 23.2 | 35.9 / 24.1 |
| webster | 1 | 310 | 919 | 31.3 | 31.8 | 28.4 / 30.7 = **0.93×** | 134.9 | 90.4 | 118.6 / 84.4 = **1.41×** | 40.4 / 33.4 | 52.4 / 30.3 |
| enwik8 | 4 | 1102 | 1288 | 46.8 | 26.9 | 44.9 / 26.4 = **1.70×** | 370.2 | 246.0 | 335.7 / 223.7 = **1.50×** | 45.1 / 79.9 | 113.9 / 92.8 |
| dickens | 4 | 1511 | 2133 | 59.9 | 31.5 | 57.7 / 31.1 = **1.86×** | 426.9 | 294.2 | 380.8 / 263.8 = **1.44×** | 32.9 / 35.4 | 82.7 / 78.4 |
| webster | 4 | 1059 | 1131 | 46.0 | 25.3 | 44.1 / 24.7 = **1.79×** | 480.5 | 249.5 | 421.8 / 224.0 = **1.88×** | 54.0 / 104.4 | 82.0 / 110.3 |

LZP is slower than bsc's because 17/96 uses a 4× larger table than 15/72 (more cache misses) and finds more
matches. At 15/72, Kolmo's LZP is now 505 MB/s on enwik8 vs bsc's 672 MB/s in the earlier run.

At 1 thread, the forward suffix sort is equal to libbsc's on dickens and webster and 22% faster on enwik8 (huge pages).

## Results against the targets

| target | result |
|---|---|
| `ratio` compress ≥ 1.2× bsc | **Not met.** Whole files vs bsc `-b25`: 1 thread enwik8 0.80×, enwik9 0.78×, dickens 0.70×; 4 threads enwik8 0.41×, enwik9 0.76×. At the same block size (enwik8, bsc `-b100`: 7.71 s at 1 thread, 5.94 s at 4): 0.85× at 1 thread, **1.14×** at 4 threads. Without QLFC (stage table): 0.93–1.18× at 1 thread, **1.70–1.86×** at 4 threads. QLFC encode is ~45% of codec 18's time at 1 thread and scales 30 → 45 MB/s from 1 to 4 threads (bsc: 30 → 114). That stage belongs to the entropy lane (see below). On the non-text Silesia files and the CSVs, `ratio`'s time is in other codecs' trials, outside this lane. |
| decode ≥ 1.0× bsc excluding QLFC | **Met.** unBWT + unLZP: 1.35–1.43× at 1 thread, 1.44–1.88× at 4 threads. Whole files, QLFC included: 1.16× (1 thread) and 1.15× (4 threads) over all files; enwik8 0.92× / 1.12×, enwik9 0.98× / 0.95×, dickens 0.92× / 0.64×. The dickens gap at 4 threads is QLFC decode (35 vs 78 MB/s). |
| peak memory ≤ bsc at the same block size | **Codec met, process not.** Codec 18's own working set is now 4n: the suffix array or index array, with the block in the caller's output slot. bsc's is about 5n (enwik8 `-b100`: 476 MiB compress and decompress). Kolmo's process peak on enwik8 is 600 MiB compress (6.0n) and 497 MiB decompress (5.0n), because Kolmo keeps the whole input (compress) or the whole output (decompress) mapped beside the block slot. bsc streams blocks. Reaching ≤ 5n at compress means coding in place in the input or streaming the input, a container-level change outside this lane. |
| size unchanged or smaller | **Met.** Every frame is byte-identical to rc3; total -9.93% vs bsc. |

## Measured and dropped

- **Multi-stream (interleaved) inverse BWT as a format change.** Not needed. libsais's inverse already follows up to
  16 chains per thread from the sampled indexes codec 18 stores (one per 2^rlog bytes), and huge pages lifted it to
  1.35–1.9× bsc's inverse. A format-2 change would buy nothing measurable here.
- **More sampled indexes per block for the inverse.** The stored count already gives each thread enough chains on
  4 threads. No gain within noise, so no change.
- **Huge pages for the QLFC scratch buffers.** No gain. `qlfc.c` was restored to rc3.
- **Compiler flags for `sais.c`** (`-O2`, `-funroll-loops`, no vectorize): all within noise of `-O3`. Clang may be a
  few percent faster on the sort, which is not actionable for a gcc build.
- **Suffix-sort asm, SIMD or prefetch kernels.** Not written. libsais's hot loops already prefetch. Profiles of the
  sort show it bound on random 4-byte loads over 4n, which huge pages addressed. No hand kernel would be
  byte-identical *and* faster without a different algorithm. So there is no new `.S` file in this lane, and the
  NO_ASM build is unaffected (it passes `make NO_ASM=1 check`).
- **32-bit vs 64-bit index paths.** Codec 18 blocks are < 2 GiB, so the 32-bit path is already the only one used.

## Notes for other lanes

- **Entropy lane: QLFC encode thread scaling.** With 23 balanced coder blocks of ~4 MiB (0.15 s each at 1 thread),
  QLFC encode goes only from 30 to 45 MB/s on 4 threads (bsc: 30 → 114 on its smaller blocks). Each thread's model is
  ~2.5 MB, larger than L2. QLFC is now ~45% of codec 18's encode at 1 thread and more than half at 4 threads, so it
  decides the compress target.
- **Block threading leaves threads idle.** `src/core/compress.c:176` gives each block `threads / nb` threads, so 3
  blocks on 4 threads leave one thread idle. Not changed here, since it changes which blocks run where, not the bytes.
- Apple M5 speed is left to the integration session, as the brief says. Nothing here is x86-specific apart from
  `MADV_HUGEPAGE`. On macOS the arrays are plain `malloc`, so the huge-page gains will not show there.
