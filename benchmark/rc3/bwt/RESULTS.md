# rc3 bwt lane: libbsc in C, libsais owned, codec 18

Branch `claude/rc3-bwt-20261116`. Machine for every number here: cloud VM, Intel Xeon @ 2.1 GHz, 4 vCPUs (1 thread
per core), Ubuntu 24.04, gcc 13.3, `-O3 -march=native`. The VM is shared and noisy: speeds within about 10% are the
same. Sizes are exact.

## 1. libbsc ported to C, byte-identical, C++ removed

What Kolmo used of libbsc 3.4.0 (`bsc_compress`/`bsc_decompress`/`bsc_block_info` with LZP hash 15, minimum length
72, the BWT block sorter, static QLFC, fast mode, no OpenMP) is now C:

| file | from libbsc | what it is |
|---|---|---|
| `src/algo/lzp.c` | `lzp/lzp.cpp` | LZP: the x86-64/AArch64 encoder (8-byte compares and heuristic) on every CPU, bounds-checked decoder |
| `src/algo/qlfc.c`, `qlfc_model.h`, `qlfc_tables.h` | `coder/qlfc/*`, `coder/coder.cpp`, `coder/common/*` | QLFC rank transform, static and adaptive models, mixers, block split |
| `src/algo/rangecoder.h` | `coder/common/rangecoder.h` | binary range coder, 16-bit output |
| `src/codecs/bwt_stream.c` | `libbsc/libbsc.cpp`, `bwt/bwt.cpp`, `adler32/` | the libbsc block (header, adler32s, LZP/BWT/QLFC glue) and codec 18 |
| `src/algo/par.c` | — | fork-join helper and the thread crew that replaces OpenMP |

**Proof of identity.** `benchmark/rc3/bwt/equiv.sh` builds libbsc 3.4.0 (from `vendor/libbsc`, or from git history
after its removal) next to the port and compares `bsc_compress` with `kolmo_bsc_compress` byte for byte, then decodes
each output with the other library. Inputs: all 12 Silesia files, enwik8, the UCI household power CSV (133 MB), the
USGS quakes CSV, generated web logs (42 MB) and JSON lines (30 MB), each cut at 23 block sizes from 0 bytes to 128 MiB
(around every LZP and QLFC block-count threshold), with LZP 15/72 + static QLFC (Kolmo's setting), LZP 15/72 +
adaptive QLFC and no LZP, and the port at 1 and 4 threads; plus 648 synthetic edge cases (one symbol, noise, the LZP
flag byte 0xF2 everywhere, periodic, tiny alphabets, long runs, a half repeated, all 256 symbols), also with LZP 16/32.
Result: **2,826 comparisons, 0 differences.** `tests/test_bwt.c` keeps libbsc's output hashes for 63 generated inputs
so `make check-bwt` (part of `check-rc`) proves it without libbsc. The determinism check (110 frames) is unchanged by
the port and by the libsais takeover.

Two places where identity needed care: the LZP encoder's four-positions-per-step loop (the loop condition is checked
only between steps, so the port keeps the same step boundaries), and the encoders' capacity rule when blocks are coded
in parallel (libbsc codes them in order with capacity `min(size, n - written)`; the port codes them in parallel with
full capacity and replays the rule: LZP succeeds iff the unlimited size is below the capacity - 8, QLFC iff the last
position its overflow check saw is below the capacity - 16, both because the output pointer only grows).

**Bugs and hardening found on the way** (libbsc trusts its input; Kolmo hashed the stream first to stay safe):
LZP decoder without an output bound (a crafted match length writes past the buffer), QLFC decoder without bounds on
the decoded size, the run lengths and the exponent loops (a crafted stream indexes past the 32-entry model arrays),
the range decoder reading past its input, and the index table read before the header without a check. All are bounded
in the port; damaged streams are fuzzed under ASan in `check-bwt`.

**C++ removed.** `vendor/libbsc` deleted; no `CXX`, `-lstdc++` or `-lc++` in the Makefile, the Python and Rust
bindings (both rebuilt and tested: the wheel's `libkolmo.so` links no C++ runtime, `cargo test` passes), CI, `tune/`.
`fuzz/build.sh` still links with `$CXX` because libFuzzer itself is C++.

## 2. libsais: review, takeover, threads

See `src/algo/sais/README.md` for the functions reached, the review against SA-IS and Grebnov's BWT induced sorting,
the issues found and what changed. Summary: the subset Kolmo uses (8-bit BWT with sampled indexes and the inverse) is
in `src/algo/sais/sais.c`; OpenMP is replaced by a pthread crew (54 regions converted by script, checked by the
compiler); the inverse BWT now runs on threads from the indexes the format already stores. Identical output to libsais
at 1 to 6 threads on 1,728 generated cases (ASan/UBSan clean; TSan reports only libsais's own benign boundary writes).

| enwik8, 32 MB | BWT, 1 thread | BWT, 4 threads | inverse, 1 thread | inverse, 4 threads |
|---|---|---|---|---|
| Kolmo crew | 27 MB/s | 58 MB/s | 86 MB/s | 355 MB/s |
| libsais + OpenMP (active wait) | 26 MB/s | 59 MB/s | 91 MB/s | 310 MB/s |

Before rc3 Kolmo built libsais without OpenMP, so every BWT block was sorted and inverted on one thread.

Not done: an AVX2 copy with runtime dispatch (a baseline SSE2 build was about 7% slower forward, equal inverse, at the
edge of this VM's noise; left to the SIMD lane with the number); prefetch retuning (done upstream in 2.10.4).

## 3. Codec 18 and the planner

Codec 18 (`src/codecs/bwt.c`, layout in `src/codecs/bwt_stream.c`) replaces codec 5 for writing; codec 5 stays
readable, so rc2 frames still decode. Frames using it are container version 11.

| change | measured on | effect |
|---|---|---|
| adaptive QLFC (mixers) instead of static | 451 MB: Silesia, enwik8, power CSV, quakes, web log, JSON lines; whole files as one block | −1.11% (81,611,814 → 80,709,704 bytes); decode about 1.5× slower per thread |
| no libbsc header (28 B), no adler32 passes, no stream hash (4 B) | every BWT block and leaf | −32 bytes per block, two fewer passes over the data; the container's XXH3 covers the decoded block and every stage decoder is bounds-checked |
| LZP 17/96 instead of 15/72 | same set, adaptive | −0.05% (80,709,704 → 80,669,376) |
| QLFC coder blocks of at least 4 MiB | same set, adaptive, LZP 17/96 | −0.13% (→ 80,563,768); 2 MiB minimum: −0.08%, 8 MiB: −0.16% but half the decode parallelism, one block: +0.09% (the split also helps adaptation) |
| threaded LZP, QLFC and unBWT inside a block | — | uses the threads left over per block (`codec_ctx.threads`); see section 4 |
| `ratio`: skip the full LZMA2 pass when the slice says BWT wins clearly | Silesia osdb (10 MB) | compress 6.8 s → 0.8 s and 3.1% smaller (2,276,454 → 2,206,574); mr, samba, mozilla unchanged (their slices are above 0.90) |

LZP parameters tried (adaptive QLFC, same 451 MB): off +0.15%, 16/32 +0.32%, 18/48 +0.31%, 16/128 −0.05%, 20/64
+0.01%, 17/96 −0.05%, against 15/72.

## 4. Size and speed against bsc, bzip3 and xz

Whole files, `benchmark/rc3/bwt/bench.sh` (wall time of whole processes, one run each; 451 MB: the 12 Silesia
files, enwik8, the first 64 MB of the UCI power CSV, the USGS quakes CSV, the generated web log and JSON lines). Kolmo
runs `-m ratio` (the mode that uses block sorting); "rc3 base" is commit 2207da1 (libbsc, codec 5), "codec 18" this
branch. All round trips verified.

**1 thread** (`-T 1`, `xz -T1`, bsc without OpenMP):

| tool | bytes | ratio | vs kolmo rc3 base | compress MB/s | decompress MB/s |
|---|---|---|---|---|---|
| kolmo-rc3base-ratio | 72,217,003 | 16.02% | +0.00% | 3.6 | 44.3 |
| kolmo-bwt-ratio | 71,813,739 | 15.93% | -0.56% | 3.9 | 30.9 |
| xz-9 | 88,759,444 | 19.69% | +22.91% | 1.1 | 69.3 |
| bsc | 83,323,574 | 18.48% | +15.38% | 20.9 | 35.8 |

**4 threads** (all of the VM; `-T 4`, `xz -T4`, bsc with OpenMP over blocks and inside libbsc):

| tool | bytes | ratio | vs kolmo rc3 base | compress MB/s | decompress MB/s |
|---|---|---|---|---|---|
| kolmo-rc3base-ratio | 72,217,003 | 16.02% | +0.00% | 4.5 | 47.6 |
| kolmo-bwt-ratio | 71,813,739 | 15.93% | -0.56% | 5.0 | 68.0 |
| xz-9 | 88,759,580 | 19.69% | +22.91% | 1.1 | 69.6 |
| bsc | 83,323,574 | 18.48% | +15.38% | 12.7 | 78.8 |

Per file (sizes; identical at 1 and 4 threads except xz's few header bytes):

| file | rc3 base | codec 18 | change | xz -9 | bsc |
|---|---|---|---|---|---|
| dickens | 2,274,350 | 2,254,558 | -0.87% | 2,830,604 | 2,274,294 |
| mozilla | 13,835,767 | 13,835,767 | +0.00% | 13,374,160 | 16,183,060 |
| mr | 2,014,182 | 1,989,774 | -1.21% | 2,750,272 | 2,231,054 |
| nci | 1,225,386 | 1,199,630 | -2.10% | 1,738,884 | 1,283,606 |
| ooffice | 2,093,384 | 2,093,384 | +0.00% | 2,426,816 | 2,587,434 |
| osdb | 2,276,454 | 2,206,574 | -3.07% | 2,849,908 | 2,276,398 |
| reymont | 994,018 | 978,208 | -1.59% | 1,317,152 | 993,962 |
| samba | 3,739,524 | 3,739,524 | +0.00% | 3,763,616 | 3,968,402 |
| sao | 2,184,759 | 2,184,759 | +0.00% | 4,415,072 | 4,724,778 |
| webster | 6,492,700 | 6,439,824 | -0.81% | 8,385,868 | 6,698,930 |
| x-ray | 3,616,667 | 3,616,667 | +0.00% | 4,489,868 | 3,806,826 |
| xml | 399,262 | 385,428 | -3.46% | 453,260 | 399,206 |
| enwik8 | 20,944,968 | 20,798,338 | -0.70% | 24,865,244 | 22,463,202 |
| power64.txt | 3,326,232 | 3,326,232 | +0.00% | 5,715,572 | 5,976,184 |
| quakes.csv | 536,920 | 528,404 | -1.59% | 655,576 | 536,864 |
| web.log | 4,637,422 | 4,637,422 | +0.00% | 5,880,032 | 4,596,506 |
| events.jsonl | 1,625,008 | 1,599,246 | -1.59% | 2,847,540 | 2,322,868 |

Reading the numbers:
- codec 18 is smaller on every file where block sorting is used (text, XML, quakes CSV, JSON lines, nci, mr, osdb:
  −0.7% to −3.5%); files coded with LZMA2 or transformed away from block sorting (mozilla, samba, ooffice, sao,
  x-ray, the power CSV, the web log) are unchanged.
- bsc's compress at 4 threads is slower than at 1 on files of one 25 MiB block: OpenMP's active waiting inside
  libbsc on this 4-vCPU VM (the same effect as in section 2).
- Compress is faster overall (osdb: 7.4 s → 1.1 s from skipping LZMA2; the rest within noise).
- Decompress at 1 thread is about 30% slower: the adaptive QLFC model (mixers) costs about 1.5× the static one per
  symbol. At 4 threads it is 43% faster than the rc3 base (68 against 48 MB/s overall; enwik8 3.8 s → 1.8 s), because
  rc2 inverted the BWT and decoded QLFC on one thread per block.
- bsc is much faster to compress (no planner, no trials) and 15% larger; xz -9 is 23% larger.

**BWT stage alone, enwik8 as one 100 MB block** (`m18.c`/`bscdrv.c`, in-memory, times exclude I/O):

| | size | compress 1 thr | decompress 1 thr | compress 4 thr | decompress 4 thr |
|---|---|---|---|---|---|
| libbsc 3.4.0 (bsc settings, one block) | 20,944,912 | 14.8 MB/s | 28.6 MB/s | 18.3 MB/s (OpenMP passive; 4.4 MB/s with OpenMP's default active wait) | 105 MB/s |
| Kolmo port, codec 5 settings (static QLFC) | 20,944,892 | 14.0 MB/s | 26.9 MB/s | 26.7 MB/s | 90.8 MB/s |
| Kolmo codec 18 (adaptive, LZP 17/96, ≥ 4 MiB coder blocks) | 20,798,286 (−0.70%) | 11.3 MB/s | 17.5 MB/s | 23.4 MB/s | 62.2 MB/s |

The port runs at libbsc's single-thread speed within this VM's noise; with threads it compresses 1.5× faster than
libbsc+OpenMP and decodes within 15% of it.

bzip3 was not measured: building it from its sources was refused by this session's permission policy (downloaded code
may not be built and run here). bsc is libbsc 3.4.0 driven as the `bsc` tool drives it (`bscdrv.c`: 25 MiB blocks,
LZP 15/72, static QLFC, OpenMP over blocks and inside libbsc when threads > 1), built from the vendored sources in git
history, since building the downloaded bsc tool was refused too.

## Tests run before each push

`make check`, `make check-rc` (includes the new `check-bwt`), `make check-sanitizers`, `make experimental-check
GPU_CHECK=`, `make kfam-check`, `benchmark/determinism/check.py`. The determinism set is regenerated in its own commit
for codec 18 (format change on purpose).
