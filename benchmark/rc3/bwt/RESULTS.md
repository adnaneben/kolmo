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

BENCH_TABLES

bzip3 was not measured: building it from its sources was refused by this session's permission policy (downloaded code
may not be built and run here). bsc is libbsc 3.4.0 driven as the `bsc` tool drives it (`bscdrv.c`: 25 MiB blocks,
LZP 15/72, static QLFC, OpenMP over blocks and inside libbsc when threads > 1), built from the vendored sources in git
history, since building the downloaded bsc tool was refused too.

## Tests run before each push

`make check`, `make check-rc` (includes the new `check-bwt`), `make check-sanitizers`, `make experimental-check
GPU_CHECK=`, `make kfam-check`, `benchmark/determinism/check.py`. The determinism set is regenerated in its own commit
for codec 18 (format change on purpose).
