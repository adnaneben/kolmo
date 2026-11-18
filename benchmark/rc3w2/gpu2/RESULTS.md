# rc3 wave 2, gpu2 lane: where the GPU adds value, measured on the M5

The owner asked: in which scenarios does the GPU add value, alone or beside the CPU threads, for each file type and
level? Wave 1 (`benchmark/rc3/gpu/RESULTS.md`) measured the one GPU job that fits today's Kolmo (the BWT suffix sort)
and found no gain. This lane built the GPU jobs that the 6-bit study proposed for a "speed tier" (sections 7 and 11 of
`research/FINDINGS.md` on `claude/dazzling-maxwell-p8y4u0`), each with a CPU reference and byte-identical output, and
timed GPU alone, ten CPU threads alone, and both together.

**Short answer.** The GPU is useful on this Mac in three places, all on large inputs (64 MB and up) and none in
today's default modes:

1. **GR6+'s formula search (`-x` on wide tables): 2.3x faster than ten CPU threads at 130 columns, 1.7x at 79, a tie
   at 64, slower below.** But ten CPU threads alone already take it from 7.5 s to 1.3 s (5.8x); that CPU change is
   the one to make first.
2. **Wide interleaved rANS (a format Kolmo does not have): decode 3.1-3.9x faster than Kolmo's own rANS on ten
   threads, encode 1.45-2.1x, on 236-256 MB; the GPU wins encode from about 64 MB and decode from about 16 MB.** Ratio within 0.1-1.1% of Kolmo's rANS.
3. **The GPU as an eleventh worker on a data-parallel stage: the fused table front end runs 1.32-1.48x faster with
   the GPU taking chunks beside the ten threads** (and 1.15-1.36x when the GPU does the front end while the threads
   run the coder). This is the first measured "CPU and GPU together" gain on this Mac; wave 1 found none for the BWT.

It does not help for planner histograms, 4K video prediction, batches of small files, or BWT blocks of 64-256 MB
(the GPU is 1.2x to 4.6x slower than ten threads there). Nothing here justifies Metal in the default build: no
Kolmo level spends its time in a stage the GPU is faster at, and linking Metal costs every process start 0.8 ms
(wave 1). The GPU code stays research code (`benchmark/rc3w2/gpu2/build.sh`); the default build is unchanged.

Machine: Apple M5 (4 performance + 6 efficiency cores, 10-core GPU, 24 GB unified memory), macOS 27.2. The owner's
leaders benchmark ran before these measurements; every timing here ran with the benchmark lock held by this lane
(nothing else benchmarking; load average about 2 from the desktop). Every number is the median of 5 interleaved rounds
(variants alternate, order flips every round; 3 rounds for the BWT and the batch) after a warm-up, in one process
(`gpu2bench`), so Metal set-up is outside the timed calls; a GPU call includes command encoding, submission and
waiting, and its inputs and outputs live in zero-copy shared memory. Raw results: `results.jsonl` (final) and
`results-run1.jsonl` (first run: the krw numbers there use a slower CPU coder, superseded).

## How the work was checked

| kernel (`src/arch/metal/gpu2.metal`) | CPU reference (`gpu2_ref.c`) | checked |
|---|---|---|
| table front end: line index, fused field split + FNV-1a hash + decimal parse, same-as-above flags + deltas | `kg2_ref_*` | 120 random CSV/JSON-lines inputs (quotes, escapes, ragged rows, every number shape) + 4 real tables, and every chunk of every timed file: GPU, CPU 1 and CPU 10 threads identical |
| per-segment histograms + Q16 integer entropy | `kg2_ref_hist_segs` | 60 random segmentations (empty, single-byte, unaligned starts) + every timed file |
| order-1 histogram | `kg2_ref_hist_o1` | same |
| krw: wide interleaved rANS, 12-bit tables per segment, 32-1024 lanes, 32-bit states, 16-bit words | `krw_ref_encode_seg`, `krw_ref_decode_seg` (and `krw_fast_*`, the CPU-organised coder, same bytes) | 60 random cases x lanes 32/64/256/1024, single-symbol and empty segments; GPU encode = CPU encode byte for byte; both decoders exact; every timed file |
| GR6+ formula search map (screen, score with exact early exits) | `kg2_ref_formula_*`, and `g6_formulas` itself (`g6wrap.c` includes `src/kolmo.c`) | the formulas found equal `g6_formulas`' on owid-co2, owid_test, energy_test, wide_test and energy_k8..k64; the GPU's per-candidate screen and scores equal the CPU's |
| kvid strip prediction (wave 1's `kvid.metal`, zero-copy host) | kvid's NEON strip coder (`vidref.c`) | every timed clip |
| BWT (wave 1's `sa.metal`) | libsais (`src/algo/sais`) | every timed block |

`gpu2bench check` runs the first five rows (360 cases, 0 failures); every timing command re-checks its own inputs
before timing. 128-bit arithmetic of the formula search (`__int128` in `gr6.c`) is done with 64-bit halves on both
sides (`kg2_fpred`), with a native 64-bit fast path.

## (a) Fused table front end (CSV and JSON lines)

Field split (quote-aware), FNV-1a hash, decimal parse, same-as-above flag and delta per field, in one pass per line
plus one pass per field; output column-major. Files: `owid8.csv` (owid-co2-data.csv rows 8 times, 115 MB, 79
columns, 31.9 M fields), `energy16.csv` (energy_test.csv 16 times, 111 MB, 130 columns), `owid.jsonl` (owid-co2 as JSON
lines, 114 MB). Processed in line-aligned chunks of 16 MB (64 MB where marked).

| file | CPU 1 thread | CPU 10 threads | GPU | GPU + CPU 10 (shared chunks) | gain of GPU+CPU over CPU 10 |
|---|---:|---:|---:|---:|---:|
| owid8.csv | 235 ms (489 MB/s) | 39.8 ms (2.89 GB/s) | 38.4 ms (2.99 GB/s) | 28.4 ms (4.05 GB/s) | 1.40x |
| owid8.csv, 64 MB chunks | 228 ms | 37.4 ms (3.08 GB/s) | 36.6 ms (3.14 GB/s) | 25.2 ms (4.56 GB/s) | 1.48x |
| energy16.csv | 256 ms | 45.4 ms (2.44 GB/s) | 47.8 ms (2.32 GB/s) | 34.3 ms (3.23 GB/s) | 1.32x |
| owid.jsonl | 114 ms | 23.3 ms (4.92 GB/s) | 26.3 ms (4.35 GB/s) | 16.4 ms (7.00 GB/s) | 1.42x |

With a coder stage after it (zstd level 1 of every column's flags and residuals on the ten threads), the GPU doing
the front end of chunk i + 1 while the threads code chunk i:

| file | CPU 10: front end then zstd | GPU front end, then CPU zstd (no overlap) | GPU front end overlapped with CPU zstd | gain |
|---|---:|---:|---:|---:|
| owid8.csv | 63.9 ms | 65.5 ms | 50.0 ms | 1.28x |
| owid8.csv, 64 MB chunks | 46.5 ms | 40.1 ms | 35.0 ms | 1.33x |
| energy16.csv | 56.9 ms | 57.5 ms | 49.6 ms | 1.15x |
| owid.jsonl | 27.2 ms | 24.2 ms | 20.0 ms | 1.36x |

Break-even (single files of one chunk):

| input | CPU 10 | GPU | GPU + CPU 10 |
|---|---:|---:|---:|
| owid 1 MB | 0.80 ms | 1.42 ms | 0.99 ms |
| owid 4 MB | 1.71 ms | 2.97 ms | 1.79 ms |
| owid 16 MB | 5.72 ms | 12.47 ms (best 5.48) | 5.78 ms |
| owid 115 MB (7 chunks) | 39.8 ms | 38.4 ms | 28.4 ms |

The GPU alone equals ten threads at about 100 MB and loses below; together they tie on one 16 MB chunk and win on
seven (115 MB); sizes in between were not measured.
One effect dominates the small sizes: **the GPU clocks down when idle**. A single 16 MB call takes 12.5 ms median and
5.5 ms at best, while the same 16 MB inside a stream of chunks takes 4.4 ms of GPU time. A compressor that uses the
GPU in short bursts pays this every time.

Context: Kolmo itself on the original owid-co2-data.csv (14.4 MB, `-T 10`, process wall time including I/O) runs
1.9 GB/s at `speed`, 0.79 GB/s at `balanced`, 7 MB/s at `ratio`. The prototype front end alone on ten threads (2.4-4.9
GB/s) is faster than either level end to end, so a GPU front end only matters for a future speed tier whose coder is
faster than the front end, on inputs of 64 MB and more. (Kolmo's numbers on the replicated `owid8.csv` are not
comparable: its long-range matcher removes the copies, `results.jsonl` has them.)

## (b) GR6+ formula search (`-x` on tables)

GR6+'s `g6_formulas` screens every candidate (target t, inputs a, b, type, scale) on up to 64 rows and scores the
survivors on up to 8000, one thread, with an early exit that depends on the best formula so far. The map-reduce form:
screen all candidates (map), score the best screened candidate of each target fully (the seeds), score every survivor
with an exact early exit against the seed's rate (map), take per target the first candidate with the best rate in
`g6_formulas`' order (reduce). Same formulas as `g6_formulas` on every file.

| table | columns | candidates | `g6_formulas` (1 thread) | map-reduce CPU 1 | CPU 10 | GPU | GPU, row-major sample | GPU + CPU 10 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| energy_test.csv | 130 | 7.19 M | 7466 ms | 8088 ms | 1286 ms | 550 ms | 547 ms | 577 ms |
| owid-co2-data.csv | 79 | 1.31 M | 1226 ms | 1427 ms | 261 ms | 152 ms | 148 ms | 148 ms |
| energy, first 64 columns | 64 | 841 K | 398 ms | 340 ms | 70.8 ms | 84.2 ms | 74.4 ms | 72.6 ms |
| energy, first 32 columns | 32 | 101 K | 136 ms | 126 ms | 35.6 ms | 60.1 ms | 50.3 ms | 50.8 ms |
| energy, first 16 columns | 16 | 11 K | 16.8 ms | 13.6 ms | 7.8 ms | 11.4 ms | 11.2 ms | 10.3 ms |
| energy, first 8 columns | 8 | 984 | 3.5 ms | 3.0 ms | 3.0 ms | 3.0 ms | 3.0 ms | 3.0 ms |

(Under 1024 candidates every variant runs the map on one CPU thread; the sample parse, 1-20 ms, is the same in all.)
Stages on energy_test.csv: screen 2025 / 303 / 165 ms and score 6035 / 1031 / 365 ms (CPU 1 / CPU 10 / GPU).

In Kolmo, `-x` on energy_test.csv takes 14.3 s, of which the formula search is 7.5 s. Ten CPU threads would make that
about 8.1 s; the GPU about 7.4 s. The GPU's share of the win is 0.7 s of 7 s, against Metal's set-up (16-330 ms per
process, depending on the shader cache) and link cost. The GPU + CPU split (half the candidates each) gives nothing
over the GPU alone: the CPU half finishes later than the GPU half.

## (c) Wide interleaved rANS for planes and leaves (krw)

A new format, not Kolmo's: per segment (1 MiB, or 256 KiB) one 12-bit frequency table and L interleaved lanes (symbol j
to lane j mod L), 32-bit states, 16-bit renormalisation; the GPU runs one thread per lane, the CPU coder advances all
lanes together over rows (same bytes). For context, Kolmo's own rANS (`src/codecs/rans.c`: 1 MiB chunks, four 64-bit
states, 16-bit tables) on the same input cut into ten parts. `mix` = the benchmark files concatenated; `bf16x5` = the
47 MB bf16 weights five times.

| input | krw encode CPU 10 | krw encode GPU | Kolmo rANS encode CPU 10 | krw decode CPU 10 | krw decode GPU | Kolmo rANS decode CPU 10 | krw size vs Kolmo rANS |
|---|---:|---:|---:|---:|---:|---:|---:|
| mix 1 MB | 3.08 ms | 3.93 ms | 0.38 ms | 2.89 ms | 1.90 ms | 0.32 ms | +0.4% |
| mix 16 MB | 9.03 ms | 10.25 ms | 4.32 ms | 4.80 ms | 3.31 ms | 3.85 ms | +1.1% |
| mix 64 MB | 29.0 ms | 15.0 ms (4.5 GB/s) | 17.9 ms (3.8 GB/s) | 19.7 ms | 7.1 ms (9.4 GB/s) | 16.8 ms (4.0 GB/s) | +0.6% |
| mix 256 MB | 137.7 ms | 39.4 ms (6.8 GB/s) | 73.4 ms (3.7 GB/s) | 103.8 ms | 16.1 ms (16.6 GB/s) | 63.1 ms (4.3 GB/s) | +0.4% |
| mix 256 MB, 1024 lanes | 127.0 ms | 33.4 ms (8.0 GB/s) | 65.1 ms | 94.7 ms | 15.0 ms (17.9 GB/s) | 56.6 ms | +1.3% |
| mix 256 MB, 256 KiB segments | 123.1 ms | 32.0 ms (8.4 GB/s) | 65.8 ms | 91.6 ms | 14.9 ms (18.0 GB/s) | 56.8 ms | +0.1% |
| bf16 weights 236 MB | 121.5 ms | 35.7 ms (6.6 GB/s) | 51.9 ms (4.5 GB/s) | 101.4 ms | 14.9 ms (15.8 GB/s) | 45.9 ms (5.1 GB/s) | +0.3% |

GPU + CPU 10 on encode (half the segments each): 5.0 ms at 16 MB (better than either alone), 17-77 ms from 64 MB
(worse than the GPU alone: the CPU half is slower). Against Kolmo's rANS on ten threads, the GPU wins encode from
about 64 MB (1.2x, rising to 1.45-2.1x at 236-256 MB) and decode from about 16 MB (1.16x, rising to 3.1-3.9x). A krw
leaf would be a new codec and format; Kolmo's leaves are a few MB per block (wave 1), below the break-even.

## (d) Histograms and entropy estimates for the planner

Byte histogram + Q16 order-0 entropy of every 64 KiB block, and the order-1 histogram of the whole input (`mix`):

| input | blocks: CPU 1 | CPU 10 | GPU | order-1: CPU 1 | CPU 10 | GPU |
|---|---:|---:|---:|---:|---:|---:|
| 1 MB | 0.22 ms | 0.08 ms | 0.24 ms | 0.30 ms | 1.31 ms | 0.87 ms |
| 16 MB | 6.1 ms | 0.91 ms | 3.1 ms | 20.9 ms | 4.0 ms | 3.8 ms |
| 64 MB | 20.0 ms | 3.3 ms | 5.5 ms | 42.6 ms | 8.1 ms | 7.7 ms |
| 256 MB | 71.9 ms | 12.2 ms | 13.7 ms | 140 ms | 26.4 ms | 27.1 ms |
| 512 MB | 147 ms | 23.8 ms | 18.9 ms | 300 ms | 56.0 ms | 37.4 ms |

Ten threads reach 20-22 GB/s on block histograms; the GPU only passes them at 512 MB (1.26x), and the planner looks at
samples of 64 KiB to 1 MiB, where the GPU is 3-10x slower (wave 1). No.

## (e) Image and video prediction at 4K

kvid's strip prediction (three predictors per 16-row strip, the cheapest kept, zigzag residuals), wave 1's kernel with
zero-copy buffers, against kvid's NEON coder on ten threads. `v4k.y4m`: 16 frames of 3840x2160 4:2:0 (199 MB; the
mobile CIF clip upscaled with fixed-seed noise: the work per pixel does not depend on the content).

| clip | CPU 1 (NEON) | CPU 10 (NEON) | GPU | CPU 10 predict + zstd | GPU predict overlapped with CPU zstd |
|---|---:|---:|---:|---:|---:|
| CIF, 300 frames (45.6 MB) | 17.7 ms | 3.6 ms | 6.6 ms | 55.2 ms | 50.5 ms |
| 1080p, 16 frames (49.8 MB) | 10.7 ms | 2.1 ms | 6.8 ms | 16.0 ms | 17.9 ms |
| 4K, 16 frames (199 MB) | 40.6 ms | 7.7 ms (26 GB/s) | 35.3 ms | 50.2 ms | 55.6 ms |

The GPU is 1.8-4.6x slower than ten NEON threads. The kernel does about 120 integer operations per sample (three
predictors with edge cases, then the residual), about 24 G operations for the 4K clip, more than 10 ms of the GPU's
peak integer rate even if fully occupied; ten NEON threads do it in 7.7 ms. The one pipelined gain (CIF, 1.09x) comes
from a weak CPU baseline (each 8-frame group gives zstd one 1.2 MB job). Kolmo's `balanced` on the 4K clip runs at
0.59 GB/s; prediction on ten threads is about 2% of that time. No.

## (f) Many small files in one batch

The 1,341 files of this repository's `src`, `vendor`, `tests`, `research`, `docs` and `bindings` (24.8 MB, 218 B to
620 KB): histogram + entropy and a 32-lane krw encode per file.

| CPU 1 | CPU 10 | GPU, one batch | GPU, one call per file |
|---:|---:|---:|---:|
| 89.1 ms | 14.2 ms | 17.6 ms (best 8.1) | 787 ms |

One batched GPU call is close to ten threads (and beats them when the GPU is already clocked up); one call per file
costs 0.29 ms per call and is 55x slower. A GPU only fits if the archiver gathers files into large batches, and then
it is not faster than the threads. No.

## (g) BWT suffix sort on large blocks

Wave 1's exact GPU suffix sort (prefix doubling) against libsais (`src/algo/sais`) on 1 and 10 threads, prefixes of
`text256.bin` (distinct text, source, DNA, logs, JSON from the benchmark set):

| block | libsais 1 thread | libsais 10 threads | GPU | doubling rounds | GPU memory |
|---|---:|---:|---:|---:|---:|
| 16 MB | 164 ms | 150 ms | 222 ms | 12 | 0.8 GB |
| 64 MB | 784 ms | 680 ms | 870 ms | 13 | 3.1 GB |
| 128 MB | 1387 ms | 1172 ms | 2622 ms | 22 | 6.2 GB |
| 256 MB | 2956 ms | 2563 ms | 5344 ms | 22 | 12.5 GB (over the 9.5 GB budget) |

The GPU does not catch up on large blocks: the number of doubling rounds grows with the longest repeat (the DNA and
logs here need 22), each round is several passes of random-access memory traffic, and the memory need (44 bytes per
byte) passes the working-set budget at 256 MB. libsais' induced sorting stays near 100 MB/s on one core. No.

## Energy

`powermetrics` needs root, so GPU power is not measured. Each line of `results.jsonl` has the CPU energy of the
process (`ri_energy_nj` from `proc_pid_rusage`, no root needed), the GPU's busy time (command buffer timestamps) and
the device utilisation sampled every 5 ms from the IOAccelerator registry. Examples: the table front end on owid8.csv
spends 978 mJ of CPU energy on ten threads and 2 mJ of CPU energy with the GPU (plus 35 ms of GPU time at about 40%
device utilisation); the energy_test formula search 32.8 J on ten threads, 0.26 J of CPU plus 0.5 s of GPU time. The
GPU's own energy is missing from these, so they show that the CPU is idle, not that the whole machine saves energy.

## Recommendation

- **Default build: no GPU.** No Kolmo level spends its time in a stage the GPU does faster; the gains above are in
  stages that do not exist in Kolmo (wide rANS, a fused front end for a speed tier) or are better taken by CPU threads
  first (formula search). Keep `src/arch/metal` and this harness as research code.
- **Do now, on the CPU:** run GR6+'s formula search on the thread pool (map-reduce as in `g6wrap.c` + `gpu2_ref.c`,
  same formulas): `-x` on energy_test.csv 14.3 s -> about 8.1 s, on owid-co2 1.2 s less. That is a change for the gr6
  lane (`src/codecs/gr6.c`), not made here.
- **If a speed tier is built** (static-table transform + wide rANS, research/FINDINGS.md section 11): design its
  front end and entropy coder so that a GPU can take whole chunks as one more worker (1.3-1.5x on 100 MB inputs here)
  and decode wide rANS on the GPU (3.1-3.9x on 236-256 MB), behind runtime detection, only above 64 MB, with the same bytes as
  the CPU path. Below 16-64 MB the GPU's clock ramp and per-call cost make it slower than ten threads.

## Routing map section (for docs/ROUTING-MAP.md)

```
GPU (Apple M5, Metal; research build only, same bytes as the CPU path)
  type        level     GPU helps above              by                      otherwise
  tables      -x        64 columns (formula search)  2.3x at 130 cols vs 10 threads (CPU threads first: 5.8x)
  tables      speed*    64 MB, as an 11th worker     1.3-1.5x on the front end (*a future speed tier)
  planes/     speed*    64 MB encode, 16 MB decode   wide rANS (new format): decode 3.1-3.9x, encode 1.45-2.1x
  leaves                                             vs Kolmo's rANS on 10 threads
  any         planner   never (512 MB: 1.26x)        histograms of samples: CPU
  video/img   balanced  never (4K: 4.6x slower)      kvid prediction: NEON threads
  many files  any       never (batched: 0.8x)        per-call cost 0.29 ms
  text        ratio     never (256 MB BWT: 2.1x slower than libsais on 10 threads, 12.5 GB)
  all         speed/balanced/ratio today: no stage where the GPU is faster; Metal costs 0.8 ms per process start
```

## Reproduce

```
make build/libkolmoarch.a build/libvendor.a && sh benchmark/rc3w2/gpu2/build.sh
./build/gpu2/gpu2bench check [tables.csv ...]                      # exactness, 360+ cases
python3 -m venv venv && venv/bin/pip install numpy                  # only for the 4K/1080p clips
venv/bin/python -I benchmark/rc3w2/gpu2/mkdata.py DATA cif.y4m OUT  # inputs
GPU2_LOCK=path/BENCH.lock python3 -I benchmark/rc3w2/gpu2/run.py build/gpu2/gpu2bench OUT results.jsonl
```

Each subcommand of `gpu2bench` (`table`, `formula`, `krw`, `hist`, `video`, `batch`, `sa`) checks its input first
and prints one JSON line per variant; `GPU2_CHECK_ONLY=1` checks without timing.
