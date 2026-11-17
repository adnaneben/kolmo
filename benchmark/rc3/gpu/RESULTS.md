# rc3 gpu lane: CPU and GPU at the same time

The owner asked: "Can we not execute GPU and CPU in parallel so that we have 20 physical threads rather than just 10
CPU or GPU ones?" Short answer, measured on the owner's Mac: **no, not for Kolmo's work.** The M5's GPU is not ten
more cores of the same kind. For the one part of Kolmo that a GPU can do with the same output bytes (the suffix sort of
the BWT codec), it runs at 0.3 to 0.8 times the speed of *one* CPU performance core on text and repetitive data, and
at 1.05 to 2.2 times on high-entropy binary data that Kolmo does not block-sort. Run beside all ten CPU threads it made
`-m ratio` 2 to 9% *slower* (shared memory bandwidth, one more thread on the cores, a slower straggler block). Where
it looked faster (1.09x at `-T 2`, 1.11x to 1.52x with ten one-thread processes), the gain came from the GPU worker
being one more CPU thread: given that thread instead, the CPU alone was faster (the GPU version ran at 0.92x and
0.96x). The rest of Kolmo's time is serial by nature (LZ match finders whose
exact output depends on their search order, context mixing and arithmetic coding) and cannot move to the GPU without
changing the output.

What did gain is CPU parallelism the profile exposed: kvid's prediction ran on one thread; on ten threads kvid's
`balanced` compression is 1.66 to 1.75 times faster (same bytes). And the profile shows the bigger lever for the slow
modes: `ratio`, `hc` and `-x` keep one core busy on any file smaller than their block (128 MiB), so 9 of the 10 cores
idle there. That is CPU work for the bwt lane (threaded suffix sorting) and the xmode lane, not GPU work.

Machine: Apple M5 (4 performance + 6 efficiency cores, 10-core GPU, 24 GB unified memory), macOS. **The machine was
shared** with other local work during every measurement (load average 2 to 3 before the runs; at one check another
program kept the GPU 32% busy, just before the kernel timings of section 2). Every end-to-end comparison is interleaved A/B (A and B
alternate, the order flips every round) and reports medians. GPU power could not be measured: `powermetrics` needs
root (`sudo` asks for a password), so only the GPU's device utilisation (from `ioreg`) is reported.

Branch `claude/rc3-gpu-20261116`, from `claude/release-rc3-20261116` (`2207da1`), with `claude/rc3-structure-20261116`
(`9ef225d`) merged. Measurements before the merge used the same code in the old layout; the tests in section 7 ran
after it.

## 1. Where the time goes (all ten threads, CPU only)

`kolmo -T 10` on single files of the benchmark set; `cores` is (user + system) / wall; hot spots from `sample`.

| file | MB | balanced s | hc s (cores) | ratio s (cores) |
|---|---:|---:|---:|---:|
| dickens | 10.2 | 0.01 | 1.36 (1.0) | 0.25 (1.0) |
| webster | 41.5 | 0.03 | 4.80 (2.4) | 0.80 (1.0) |
| mozilla | 51.2 | 0.09 | 1.95 (4.5) | 2.58 (4.8) |
| samba | 21.6 | 0.09 | 2.46 (1.3) | 5.22 (1.0) |
| owid-co2-data.csv | 14.4 | 0.05 | 0.43 (1.0) | 2.11 (1.0) |
| x-ray | 8.5 | 0.02 | 0.20 (1.0) | 0.95 (1.0) |
| llm_bf16_47mb.safetensors | 47.2 | 0.10 | 3.71 (2.8) | 15.46 (1.0) |
| chr22.fa | 51.8 | 0.04 | 7.94 (2.2) | 0.92 (1.0) |

| mode | where the time goes | GPU-able with the same bytes? |
|---|---|---|
| `balanced` | zstd 3 and the graph transforms; 0.3 to 1.4 GB/s with 10 threads, 10 to 100 ms per file | no: a GPU round trip costs 0.1 ms and Metal set-up 65 ms; the work is already done by then |
| `hc` | `ZSTD_btGetAllMatches` (zstd's binary-tree match finder): about 90% of busy samples | no: the matches found depend on the tree's insertion order and search limits |
| `ratio`, binary, CSV, weights | liblzma's bt4 match finder (`bt_find_func`, `bt_skip_func`): 60 to 80%; `lzma_lzma_optimum_normal` about 15% | no, for the same reason |
| `ratio`, text | libbsc: libsais suffix sort 55 to 65% of the codec's time, QLFC most of the rest, LZP 3 to 5% (table below) | **the sort, yes**: the suffix array is unique |
| `-x` | `cm_compress_m`: 97% | no: bit-serial context mixing and arithmetic coding |
| kvid `balanced` | prediction 39 ms of 67 ms (one thread), then Kolmo | yes, and also on CPU threads |
| kfam (`-x` on FASTQ) | first the CM trial on the whole block (`cm_compress_m`, 97% of the first seconds), then kfam: `kf_dm_code` 69%, `kf_qm_code` 18%, `kf_code16` 13% (model + binary arithmetic coder per symbol); standalone kfam on 64,779 reads (40.9 MB, SRR1770413 prefix) takes 3.87 s at `-T 10` with 2.5 cores busy | no: bit-serial |

libbsc's parts, one thread (`bscprof`, Kolmo's parameters):

| file | bsc total s | LZP s | libsais BWT s |
|---|---:|---:|---:|
| dickens | 0.181 | 0.005 | 0.089 |
| webster | 0.764 | 0.025 | 0.486 |
| samba | 0.309 | 0.014 | 0.165 |
| nci | 0.360 | 0.039 | 0.270 |
| chr22.fa | 0.828 | 0.022 | 0.424 |
| mozilla | 1.045 | 0.041 | 0.489 |

So even an infinitely fast GPU sort would make a `ratio` text block at most 1.6 to 2.9 times faster, and in `ratio`
on a file under 128 MiB that block is the only work running.

## 2. The GPU suffix sort (built, exact, slower than one core)

`src/arch/metal/sa.metal`, `sa_metal.m`: prefix doubling over a stable LSD radix sort of 64-bit keys, written for
Metal (no libcubwt code; only its C API is mirrored, so libbsc's own GPU hook calls it). First sort by eight bytes,
then rounds on the unsorted groups with keys (rank of i, rank of i + h) packed into as few bits as n needs; radix
passes on digits that are equal in every key are skipped. It produces libsais' suffix array and BWT bit for bit:
`tests/test_gpu_sa.c` checks 2146 random and edge-case inputs (runs of one byte, two symbols, periodic text, zero
bytes, up to 3 MB) and `ratio`/`-x` frames with and without the GPU worker.

Speed against libsais on one core, fastest of 3 (`build/test-gpu-sa -t FILE`), whole files:

| file | MB | libsais MB/s | GPU MB/s | GPU / CPU | doubling rounds |
|---|---:|---:|---:|---:|---:|
| dickens | 10.2 | 115.6 | 89.8 | 0.78 | 12 |
| webster | 41.5 | 81.2 | 65.3 | 0.80 | 12 |
| samba | 21.6 | 101.4 | 50.6 | 0.50 | 13 |
| nci | 33.6 | 101.6 | 32.8 | 0.32 | 13 |
| chr22.fa | 51.8 | 98.2 | 30.2 | 0.31 | 22 |
| mozilla | 51.2 | 90.0 | 73.1 | 0.81 | 14 |
| xml | 5.3 | 114.9 | 41.8 | 0.36 | 12 |
| x-ray | 8.5 | 77.2 | 166.3 | 2.16 | 4 |
| ooffice | 6.2 | 104.5 | 110.3 | 1.05 | 13 |
| reymont | 6.6 | 105.5 | 74.9 | 0.71 | 8 |
| osdb | 10.1 | 92.3 | 80.0 | 0.87 | 5 |
| sao | 7.3 | 97.7 | 143.4 | 1.47 | 2 |
| mr | 10.0 | 116.6 | 91.8 | 0.79 | 12 |
| owid-co2-data.csv | 14.4 | 101.5 | 85.2 | 0.84 | 6 |
| loghub_all.log | 4.5 | 116.0 | 53.5 | 0.46 | 12 |
| api.json | 3.7 | 118.0 | 35.9 | 0.30 | 14 |
| llm_bf16_10mb.safetensors | 10.5 | 85.9 | 168.2 | 1.96 | 2 |

Why it is slow on this machine:
- The M5's GPU reaches 129 GB/s on a coalesced copy and 143 GB/s on reads, but 13 GB/s on a random scatter
  (`bw.m` calibration). Prefix doubling moves about 32 bytes per suffix per radix pass, 6 to 8 passes per round, and
  text needs 12 rounds (repetitive data 13 to 22), with random rank lookups between rounds. libsais's induced sorting
  is linear and cache-friendly; one P-core does 80 to 120 MB/s.
- Kernel profile on dickens (10.2 MB): the first round (eight passes over all suffixes) takes 50 ms; in isolation the
  eight histogram passes take 5.6 ms, the eight scatters 17.6 ms (about 110 GB/s, close to the bandwidth), the scans
  0.7 ms; the rest is the per-round rank, flag, compaction and key kernels, which are random-access.
- Tried and kept: packing both rank parts into the fewest bits (7 -> 6 radix passes per round on dickens) together
  with a scatter that ranks per simdgroup without a barrier per element round: round 1 59.0 -> 47.8 ms, round 2
  46.6 -> 32.0 ms, dickens 0.71x -> 0.78x of libsais. Tried and gave nothing: staging the tile in threadgroup memory
  for coalesced writes (round 1 47.8 -> 50-53 ms), one atomic per distinct digit and simdgroup in the histogram (no
  change).
- Memory: about 44 bytes per input byte on the GPU (2 GB for a 41 MB block), against 5 for libsais.
- The GPU wins only where eight bytes already separate almost every suffix (x-ray, model weights, sao: 2 to 4 rounds).
  Kolmo's `ratio` sends those files to LZMA2 or the graph, not to the BWT, so the GPU's wins land where it is not used.

## 3. The heterogeneous scheduler and what "CPU and GPU in parallel" bought

`src/core/hetero.c`: one GPU worker thread joins Kolmo's CPU pool in the modes that block-sort (`ratio`, `-x`) when a
file has more blocks than threads. It pulls blocks from the same counter as the CPU workers, but only while the CPU
workers have a backlog (remaining blocks > CPU threads), so the GPU never holds the last blocks while cores wait. Its
BWT sorts go to the GPU through libbsc's own GPU hook (`bwt.cpp` built with `LIBBSC_CUDA_SUPPORT`, served by
`src/arch/metal/cubwt_metal.c`; `vendor/libbsc` unchanged); any GPU error, a block under 1 MiB or above half the
device's working set falls back to libsais. Every other part of the block (LZP, QLFC, the LZMA2 trial) runs on the GPU
worker's CPU thread. The output is the same bytes whichever worker codes a block.

Interleaved A/B, 5 rounds each, median wall time, same output bytes and a checked round trip every time
(`benchmark/rc3/gpu/ab.py`). `textcorpus` = webster + dickens + nci + reymont + xml + chr22.fa (149 MB); `silesia12` =
the twelve Silesia files concatenated (212 MB).

| input | args | CPU only s | with GPU worker s | speed | GPU sorts | GPU busy | CPU cores busy (A / B) |
|---|---|---:|---:|---:|---:|---:|---|
| textcorpus | `-m ratio -T 10 -B 4` | 0.47 | 0.51 | **0.93x** | 2 (8.3 MB) | 44% | 7.05 / 7.47 |
| textcorpus | `-m ratio -T 10 -B 8` | 0.58 | 0.63 | **0.91x** | 1 (8.4 MB) | 35% | 7.00 / 7.03 |
| textcorpus | `-m ratio -T 4 -B 8` | 0.78 | 0.82 | 0.96x | 2 (16.7 MB) | 42% | 3.54 / 3.71 |
| silesia12 | `-m ratio -T 10 -B 8` | 5.40 | 5.50 | 0.98x | 2 (16.7 MB) | 6% | 7.91 / 7.87 |
| webster | `-m ratio -T 10` (one block) | 0.79 | 0.81 | 0.98x | 0 | 1% | 0.99 / 0.98 |
| webster | `-m ratio -T 2 -B 4` | 0.37 | 0.34 | **1.09x** | 2 (8.4 MB) | 42% | 1.89 / 2.03 |
| dickens | `-x -T 4 -B 1` | 8.64 | 8.71 | 0.99x | 0 | 3% | 2.82 / 3.20 |

GPU busy is the device utilisation of the whole GPU sampled every 50 ms during B runs (A runs: 1 to 3%, other
programs). The GPU sorted 8.4 MB in 0.21 s inside a 10-thread run (40 MB/s) against 65 to 90 MB/s alone: the CPU
threads and the GPU share the same memory bandwidth. The one-block run is 2% slower because `--gpu` compiles the Metal
kernels at start (65 ms or more). Default `ratio` and `-x` on a file under 128 MiB is one block, so the GPU worker
never starts there.

Many files, one process per core (`benchmark/rc3/gpu/many.py`): ten `kolmo -m ratio -T 1` processes at once on ten
files of the set (dickens, webster, nci, reymont, xml, chr22.fa, loghub_all.log, api.json, owid-co2-data.csv, samba),
so every core is busy and the GPU is otherwise idle; median of 4 to 6 interleaved rounds, wall time until the last
process ends, same bytes in every round:

| A | B | A s | B s | B speed | GPU sorts per round |
|---|---|---:|---:|---:|---:|
| `-T 1 -B 4`, CPU only | `-T 1 -B 4 --gpu` | 3.32 | 3.00 | 1.11x | — |
| `-T 1 -B 8`, CPU only | `-T 1 -B 8 --gpu` | 6.07 | 4.00 | 1.52x | 7 |
| `-T 2 -B 8`, CPU only | `-T 1 -B 8 --gpu` | 3.86 | 4.01 | **0.96x** | 7 |
| webster `-T 2 -B 4`, CPU only | webster `-T 2 -B 4 --gpu` | 0.40 | 0.37 | 1.09x | 2 |
| webster `-T 3 -B 4`, CPU only | webster `-T 2 -B 4 --gpu` | 0.33 | 0.36 | **0.92x** | 2 |

The ten-process gain is a thread count, not the GPU: the slowest process (chr22.fa, 7 blocks) finishes when the other
processes have left cores free, and its GPU worker is a second thread that takes blocks; the GPU sorted only 7 blocks
in the whole round (3 of them chr22.fa's, at a third of a core's speed). Giving every process a second CPU thread
instead is 4% faster than the GPU worker.

**Answer in numbers:** on this M5, CPU + GPU together gave 0.91x to 0.98x of CPU-only `-T 10` on block-sorted data,
0.92x to 0.96x of a CPU-only run given the same number of threads in every other setting, and nothing on every other
mode (their work cannot run on the GPU with the same bytes). The GPU behaves like less than one extra core for the
sort, and it slows the CPU cores it shares memory with. "20 threads" is not on offer: the 10 GPU cores are not cores
in that sense (each is a wide SIMD unit that only pays off on data-parallel work), and Kolmo's slow modes spend 75 to
97% of their time (`hc`, `ratio` on binary data, `-x`) in work that is serial per block; the one data-parallel part
(the suffix sort, 55 to 65% of a `ratio` text block) is faster on one CPU core than on the GPU.

## 4. kvid: prediction on CPU threads and on the GPU

kvid's strip prediction (three predictors per 16-row strip, the cheapest kept) was single-threaded. Frames are
independent at encode time (the previous frame is the input's), so it now runs on `-T` threads; `--gpu` runs it as a
Metal kernel (`src/arch/metal/kvid.metal`, one threadgroup per strip). All three give the same bytes
(`benchmark/rc3/gpu/kvid_ab.py`, 3 interleaved rounds of `kvid -b`, fastest; `balanced`, Kolmo stage on 10 threads
except `cpu1`):

| clip (45.6 MB) | transform, 1 thread | transform, 10 threads | transform, GPU (kernel) | compress, 10 threads | compress, GPU + 10 threads |
|---|---:|---:|---:|---:|---:|
| akiyo_cif | 39.0 ms | 6.5 ms | 12.3 ms (about 6) | 36.7 ms | 43.0 ms |
| foreman_cif | 39.0 ms | 6.5 ms | 12.7 ms (6.1) | 39.8 ms | 44.8 ms |
| mobile_cif | 45.5 ms | 7.5 ms | 12.7 ms (6.1) | 38.0 ms | 43.7 ms |

Against kvid before this change (prediction on one thread, Kolmo on 10), interleaved, 3 rounds of `kvid -b`:

| clip | before | after (prediction on 10 threads) | speed | same bytes |
|---|---:|---:|---:|---|
| akiyo_cif | 64.9 ms | 39.0 ms | 1.66x | yes |
| foreman_cif | 67.1 ms | 38.9 ms | 1.73x | yes |
| mobile_cif | 70.4 ms | 40.3 ms | 1.75x | yes |

The GPU kernel alone (6 ms) only matches ten CPU threads, and copying 45 MB in and out of Metal buffers adds 6 ms.
kvid's layout groups residuals by plane over all frames, so Kolmo cannot start before the last frame is predicted:
there is no stage for the GPU to overlap with.

## 5. Planner-sized work and fixed costs

`benchmark/rc3/gpu/hist_bench.m` (byte histogram + order-0 entropy, as the planner computes on its samples):

| work | CPU, one core | GPU end to end (kernel) |
|---|---:|---:|
| empty command buffer, round trip | — | 0.097 ms |
| Metal set-up (device, compile from source, pipelines) | — | 64.9 ms per process |
| 64 KiB | 0.022 ms | 0.235 ms (0.055) |
| 1 MiB | 0.236 ms | 0.336 ms (0.078) |
| 16 MiB | 4.607 ms | 2.270 ms (1.142) |

The planner's samples are 64 KiB to 1 MiB: the GPU loses 1.4x to 10x there before its 65 ms set-up is counted.
Linking Metal and Foundation alone costs 0.8 ms per process start (2.70 -> 3.52 ms median over 200 interleaved runs of
`-m balanced` on a 94 KB file, `startup.py`), 30% on small files. That is why the GPU worker is built only with
`make GPU=1`.

LZ match finding and wide rANS were not built: the match finders' output depends on their sequential search state
(zstd's binary tree, liblzma's bt4 with depth limits), so a GPU finder gives different bytes; a GPU-friendly encoder
(matches from the suffix array, for example) would be a new format and a new codec. krans' 32-way interleaved rANS was
already slower with NEON than scalar on the M5 (`research/CODECS.md`); it is a decode-side coder whose leaves are a few
MB, below the GPU's break-even.

## 6. What is in the branch

| change | where | default build | verdict |
|---|---|---|---|
| kvid prediction on `-T` threads (same bytes) | `src/experimental/kvid.c` | yes (research tool) | **kept**: 1.66-1.75x faster kvid `balanced` |
| GPU suffix sort and BWT, exact | `src/arch/metal/sa.metal`, `sa_metal.m`, `cubwt_metal.c` | no (`GPU=1`) | kept as research: slower than one core on text |
| GPU worker beside the CPU pool | `src/core/hetero.c`; hooks under `KOLMO_GPU` in `codecs/bwt.c`, `core/blocks.c`, `core/compress.c`, `api/api.c` | no (`GPU=1`, then `--gpu` / `KOLMO_GPU=1`) | kept off: 0.91-0.98x at 10 threads, 0.92-0.96x against one more CPU thread |
| kvid prediction on the GPU (`--gpu`) | `src/arch/metal/kvid.metal`, `kvid_metal.m` | no (`GPU=1`) | kept as research: slower than 10 threads |
| `kolmo_set_gpu()`, `--gpu` | `src/kolmo.h`, `src/cli.c` | yes (returns 0 without `GPU=1`; `--gpu` then says so) | API stub |
| tests | `tests/test_gpu_sa.c`, `make GPU=1 check-gpu` (part of `check-rc`) | prints "built without" otherwise | — |

The default build's output and start-up are unchanged (determinism check, section 7). With `GPU=1` the frames are the
same bytes with and without `--gpu` (`test_gpu_sa`, `ab.py`, and the determinism check run with `KOLMO_GPU=1`).

## 7. Tests

After merging `claude/rc3-structure-20261116` (`9ef225d`), on the M5, with other local test runs on the machine:

| build | `make check` | `check-rc` | `check-sanitizers` | `experimental-check` | `kfam-check` | determinism |
|---|---|---|---|---|---|---|
| default (no GPU code linked) | pass | pass | pass | pass | pass | 110 frames OK |
| `GPU=1` | pass | pass (includes `check-gpu`: 2146 exact sorts, ratio frames identical with 2 GPU sorts) | pass | pass | pass | 110 frames OK, and 110 OK with `KOLMO_GPU=1` |

The first `GPU=1` `check-rc` failed to link `test-dispatch-asan` (it compiles `src/kolmo.c` without the arch archive);
fixed in the Makefile and re-run.

## Reproduce

```
make GPU=1 all build/test-gpu-sa build/kvid
./build/test-gpu-sa                                   # exactness
./build/test-gpu-sa -t FILE...                        # GPU sort vs libsais
python3 benchmark/rc3/gpu/ab.py build/kolmo 5 out.jsonl corpus -m ratio -T 10 -B 8
python3 benchmark/rc3/gpu/kvid_ab.py build/kvid 3 out.jsonl balanced clip.y4m...
clang -O2 -fobjc-arc benchmark/rc3/gpu/hist_bench.m -framework Metal -framework Foundation -o build/hist-bench
python3 benchmark/rc3/gpu/startup.py nogpu/build/kolmo build/kolmo small-file 200 -m balanced
```
