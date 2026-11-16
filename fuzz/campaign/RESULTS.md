# Kolmo rc2 coverage-guided fuzzing campaign (15 Nov 2026)

Branch under test: `claude/release-rc2-20261115` at `fd66a97`. Results on `claude/rc2-fuzz-20261115`.
This was the project's first coverage-guided campaign. Until now there was only CI `fuzz-smoke`: 20,000 random
inputs per target, gcc ASan.

## Summary

- **No crashes, ASan/UBSan reports, leaks or out-of-memory kills** in any of the 9 targets. That is about
  162 million executions over 4 phases (≈ 9.3 h of target wall time on 4 cores).
- **Timeouts (6 unique): all are valid, unmodified `-x` (ultra, context-mixing) frames that decode slowly.** They
  are not hangs. With the optimized CLI they decode at 0.12–0.17 MB/s, for example 688 KB in 5.9 s. Under
  ASan+UBSan -O1 the harness runs each one 2–4 times, which takes 21–82 s and goes past `-timeout=25`. They are
  under `timeouts/`. Classification: *timeout, slow valid input*. There is no file/line defect to report.
- **Caveat on depth:** the three CZN1 frame-decoder targets (`fuzz_decompress`, `fuzz_limited`, `fuzz_dict`) ran
  at only 1–2 exec/s. Context-mixing model setup costs about 300 ms per call even at -O3 for a 4 KiB frame, and
  many corpus inputs reach it. Together these three targets did only ~54k executions, so their result is a
  shallow pass, not a deep one. The research decoders, the stream decoder and fastq ran at 2k–20k exec/s.
- Coverage of the frame decoders is limited by what the seed frames exercise. The integer/decimal graph inverse,
  the table, x86 (e8e9), lines, 16-bit image and record transforms were never decoded (see the 0% list). The
  CLI did not emit those transforms for any seed I made.

## Machine and tools

| | |
|---|---|
| CPU | Intel Xeon @ 2.30 GHz, 4 vCPU (cloud VM) |
| Memory | 16 GB (15.7 GiB), no swap. Lowest available memory seen during the campaign: 11.1 GB |
| OS | Ubuntu 24.04.5 LTS, kernel 6.18.44 |
| Compiler | Ubuntu clang 18.1.3 (1ubuntu1). libFuzzer/ASan/UBSan runtimes from `libclang-rt-18-dev` 18.1.3-1ubuntu1 (apt: `clang llvm libclang-rt-18-dev liblzma-dev`; `clang`/`llvm` alone lack the libFuzzer runtime on 24.04) |
| Coverage tools | llvm-profdata / llvm-cov 18 |
| Vendor libs | `build/libvendor.a` built by `make ARCH= build/libvendor.a` with gcc 13.3.0, not instrumented (as in CI), so coverage and sanitizers cover Kolmo's own code. liblzma 5.4.5 |

Build flags (`fuzz/campaign/build.sh`): `clang -g -O1 -fsanitize=fuzzer,address,undefined
-fno-sanitize-recover=undefined -ffp-contract=off` plus the CI `fuzz-smoke` defines and includes
(`-DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc
-Ivendor/brotli/include`). Links `build/libvendor.a -llzma -lpthread -lm -lstdc++`. Coverage build: the same with
`-fsanitize=fuzzer -fprofile-instr-generate -fcoverage-mapping` (`MODE=cov`).

## Targets

| target | API under test | harness |
|---|---|---|
| fuzz_decompress | `kolmo_decompress` (existing) | `fuzz/fuzz_decompress.c` |
| fuzz_roundtrip | `kolmo_compress` → `kolmo_decompress`, 6 modes (existing) | `fuzz/fuzz_roundtrip.c` |
| fuzz_nplane | native plane-context leaf, codec 15 (existing) | `fuzz/fuzz_nplane.c` |
| fuzz_limited | `kolmo_decompress_limited` (generous and tight limits) + `kolmo_read_range`, checks range == full decode | `fuzz/fuzz_limited.c` (new) |
| fuzz_stream | `kolmo_stream_decompress` (src/stream.inc) via memfd → /dev/null | `fuzz/fuzz_stream.c` (new) |
| fuzz_dict | `kolmo_decompress_dict`: correct, wrong and no dictionary; fixed 2 KiB dictionary `campaign/dict.bin` | `fuzz/fuzz_dict.c` (new) |
| fuzz_krc | `krc_decompress`, `krc_decode_threads` | `fuzz/fuzz_krc.c` (new) |
| fuzz_kbcx | `kbc_decompress` and `kbx_decompress` on every input | `fuzz/fuzz_kbcx.c` (new) |
| fuzz_fastq | `frontier_fastq_decode` through a KFQ1 frame walk mirroring `fastq_cli.c` (and raw payloads) | `fuzz/fuzz_fastq.c` (new) |

No new harness trusts a size from the input above 16 MiB. Claimed original sizes, and CZNS frame lengths in the
stream harness, are checked against the cap before allocation. Inputs above the cap are skipped or decoded only
into a fixed small buffer.

## Seeds

`fuzz/campaign/make_seeds.sh` builds the seeds from 56 raw inputs. They are the files in `tests/` plus generated
slices of 4–128 KB: prose, CSV, JSON, logs, int16/32/64, float32/64, PPM/PGM/RGBA images, an ELF slice, random
and zero bytes, DNA, FASTQ, tar, NPY (int16/int32/uint8/float32) and mono/stereo 16-bit WAV. Each is compressed
with `build/kolmo` at `-m speed`, `-m rt`, `-m balanced`, `-q`, `-Q`, `-n`, `-m eco`, `-m hc`, `-m ratio`,
`-m max -l 1`, `-m max -l 10`, `-x`, `-x -l 1`, `-x -l 2`, `--native-leaf`, `-T 1` and `-B 0.0625`. NPY and WAV
files also get `-m fast -T 1 --typed-huf`. That is 958 frames. There are also:

- 28 CZNS streams (`-s`, `-S 1`, `-s -m rt`, `-s -m hc`), one of them multi-segment.
- 36 dictionary frames (`-D dict.bin` × 6 modes).
- Research frames from `build/krc-cpu`, `build/kbc`, `build/kbx -l 0/1/2` and `build/kolmo-fastq`. These were
  built with `make ARCH= build/krc-cpu build/kbc build/kbx build/kolmo-fastq`, the CLI targets of
  `experimental-check`; the full check was not run.

roundtrip/nplane seeds are the raw inputs with a mode or plane-count prefix byte.

Seed adjustments during the campaign:

1. 46 `-x` frames of large inputs took more than 2 s per run under ASan. They stalled the first start of the
   frame-decoder targets (see phase 1) and were filtered out with `fuzz/campaign/filter_slow.sh`.
2. Only one FASTQ input yielded a compressed (non-stored) KFQ1 block. Phase 3 added 12 varied FASTQ files
   (lengths 36–3000, N-rich, Illumina headers, CRLF, `+name`, binned and full-range qualities, duplicates,
   truncated).
3. Phase 4 added 30 frames of 200–690 KB text, log and JSON inputs. These are the only CLI outputs that reach
   the word-dictionary and log/JSON decoders.

## Runs

Every target ran in libFuzzer fork mode (`fuzz/campaign/run.sh`) with these options: `-fork=1 -ignore_crashes=1
-ignore_ooms=1 -ignore_timeouts=1 -rss_limit_mb=4096 -timeout=25`. `-max_len` was 1048576 for frame decoders and
131072 for roundtrip/nplane, whose inputs are raw data compressed per execution. All targets ran at the same
time, 9 processes on 4 cores, so each had about 0.45 core. Environment: `ASAN_OPTIONS=detect_leaks=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

| phase | time (UTC) | targets |
|---|---|---|
| 1 | 16:18–17:58 (6000 s) | all 9. decompress/limited/dict were stopped at 16:35 (stalled on slow seeds, 0 exec/s, logs in `logs/phase1_stalled/`) and restarted 16:36–18:17 with filtered seeds |
| 2 | 18:00–18:46 (2700 s) | decompress, limited, dict (second process, same corpora, using the cores freed by phase 1) |
| 3 | 18:18–18:48 (1800 s) | fastq with the extra FASTQ seeds |
| 4 | 18:56–19:27 (1800 s) | decompress, limited, dict with the large text/log/JSON seeds |

Per target and phase. `cov` is libFuzzer edge coverage of the instrumented code, and `cov@Nm` is coverage N
minutes into that phase. A phase that starts from an existing corpus shows its starting coverage at the first
stat line.

| target | phase | wall s | executions | exec/s | cov | ft | corpus | oom/timeout/crash | cov@1m | @5m | @10m | @30m | @60m | @90m | @100m |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fuzz_decompress | 1 | 6079 | 7,251 | 1 | 3531 | 3254 | 437 | 0/0/0 | – | 3270 | 3363 | 3491 | 3511 | 3523 | 3530 |
| fuzz_decompress | 2 | 2763 | 3,812 | 1 | 4075 | 3299 | 395 | 0/0/0 | | | 3221 | 4007 | | | |
| fuzz_decompress | 4 | 1817 | 2,857 | 1 | 4577 | 3816 | 432 | 0/0/0 | | | 3781 | 4575 | | | |
| fuzz_limited | 1 | 6060 | 12,084 | 2 | 3640 | 3693 | 545 | 0/0/0 | – | 3375 | 3416 | 3480 | 3521 | 3637 | 3640 |
| fuzz_limited | 2 | 2718 | 7,032 | 2 | 4592 | 3759 | 497 | 0/0/0 | | | 3676 | 4582 | | | |
| fuzz_limited | 4 | 1803 | 4,280 | 2 | 4904 | 4194 | 512 | 0/0/0 | | | 4559 | 4904 | | | |
| fuzz_dict | 1 | 6009 | 10,138 | 1 | 3735 | 4150 | 705 | 0/0/0 | 2582 | 3596 | 3656 | 3706 | 3722 | 3734 | 3735 |
| fuzz_dict | 2 | 2706 | 4,242 | 1 | 4974 | 4218 | 672 | 0/0/0 | | | 4073 | 4939 | | | |
| fuzz_dict | 4 | 1828 | 2,496 | 1 | 5548 | 4713 | 707 | 0/1/0 | | | 4652 | 5548 | | | |
| fuzz_stream | 1 | 6019 | 86,040,132 | 14,294 | 1035 | 1216 | 333 | 0/0/0 | 870 | 911 | 930 | 1009 | 1016 | 1029 | 1035 |
| fuzz_roundtrip | 1 | 5997 | 9,506 | 1 | 11386 | 14639 | 1317 | 0/0/0 | – | 10336 | 10605 | 11123 | 11184 | 11225 | 11386 |
| fuzz_nplane | 1 | 6073 | 131,528 | 21 | 2052 | 2998 | 887 | 0/0/0 | 1967 | 2011 | 2027 | 2033 | 2047 | 2051 | 2052 |
| fuzz_krc | 1 | 6018 | 36,364,324 | 6,042 | 206 | 210 | 48 | 0/0/0 | 200 | 201 | 205 | 206 | 206 | 206 | 206 |
| fuzz_kbcx | 1 | 6063 | 13,437,972 | 2,216 | 672 | 667 | 94 | 0/0/0 | 666 | 668 | 669 | 672 | 672 | 672 | 672 |
| fuzz_fastq | 1 | 6009 | 29,714,826 | 4,945 | 231 | 333 | 96 | 0/0/0 | 210 | 229 | 229 | 229 | 229 | 231 | 231 |
| fuzz_fastq | 3 | 1833 | 37,191,677 | 20,290 | 578 | 654 | 171 | 0/0/0 | 555 | 572 | 577 | 578 | | | |

Totals per target: decompress 13,920 executions, limited 23,396, dict 16,876, stream 86.0 M, roundtrip 9,506,
nplane 131,528, krc 36.4 M, kbcx 13.4 M, fastq 66.9 M. The krc and kbcx decoders plateaued within 30 minutes,
and so did fastq in each of its phases.

Minimised final corpora (`-merge=1` of seeds and every phase's corpus, coverage build) are in `corpus/<target>/`,
40 MB in total:

| target | files | KB |
|---|---|---|
| fuzz_decompress | 429 | 3,792 |
| fuzz_limited | 513 | 4,056 |
| fuzz_dict | 709 | 4,992 |
| fuzz_stream | 276 | 1,700 |
| fuzz_roundtrip | 993 | 12,820 |
| fuzz_nplane | 593 | 9,976 |
| fuzz_krc | 45 | 500 |
| fuzz_kbcx | 91 | 752 |
| fuzz_fastq | 152 | 1,880 |

## Crashes, leaks, timeouts, OOMs

| kind | count | details |
|---|---|---|
| crash (ASan: OOB read/write, UAF, double free; UBSan: overflow, shift, misaligned …) | 0 | – |
| harness invariant (`__builtin_trap`: lossy round trip, success with wrong length, range ≠ full decode) | 0 | – |
| leak (LeakSanitizer) | 0 | – |
| out-of-memory (`-rss_limit_mb=4096` / malloc limit) | 0 | – |
| timeout (>25 s) | 6 unique | all valid `-x` frames, below |

The timeout files are in `fuzz/campaign/timeouts/`. Each one is the seed itself (byte-identical), so there is
nothing to minimise. The phase-1/4 fuzzing also produced `slow-unit` mutants of `x__sci_200k.txt.kolmo`, which
ran for 10–25 s and are not committed.

| file | raw size | O3 CLI decode, 1 thread | ASan harness |
|---|---|---|---|
| timeout-6b2ca499d1d9__x__prose_128k.txt.kolmo | 131,072 | 0.78 s | 29 s |
| timeout-48750fe1c923__xl2__f32_96k.bin.kolmo | 98,304 | 1.69 s | 28 s |
| timeout-6fee4b132511__x__lines_200k.txt.kolmo | 246,355 | 1.57 s | >25 s |
| timeout-b76dd537674e__x__access_300k.log.kolmo | 284,014 | 1.71 s | >25 s |
| timeout-fbd00b167840__x__prose_400k.txt.kolmo | 382,070 | 3.04 s | 81.5 s |
| timeout-a6ca74064d50__x__app_600k.log.kolmo | 687,953 | 5.87 s | >25 s |

Stack: none, because these are not sanitizer reports. The time is spent in the context-mixing decoder
(`src/cm.inc`). Classification: *hang? no; slow valid input*. Decode speed of the ultra tier is about
0.12–0.17 MB/s on this CPU.

### Observations that are not findings of this campaign (harness-filtered, by design)

- `src/stream.inc:91–92` `kolmo_stream_decompress` accepts a frame length up to 2^34 (16 GiB) and `malloc`s it
  before reading the frame. `src/stream.inc:98` allocates the frame's claimed original size. A 20-byte
  CZNS input can make it request 16 GiB. The stream harness skips inputs whose frame length or claimed size
  exceeds 16 MiB, as the campaign rules required, so the fuzzer never exercised this path. It needs a
  decision, not a fix from this campaign.

## Coverage (llvm-cov, replay of the final corpora)

`fuzz/campaign/coverage.sh` replays the corpora through the coverage builds. **All targets** combines the 9
profiles, so it includes the encoder, which `fuzz_roundtrip` and `fuzz_nplane` exercise. **Frame decoders only**
combines `fuzz_decompress`, `fuzz_limited`, `fuzz_stream` and `fuzz_dict`. Denominators include encoder-only code
in the same file. Full reports are in `coverage/`.

| file | lines (all targets) | branches (all targets) | functions entered | lines (frame decoders only) | branches (frame decoders only) |
|---|---|---|---|---|---|
| src/kolmo.c | 1518/2412 **62.94%** | 2180/4160 **52.40%** | 96/123 | 523/2412 21.68% | 697/4160 16.75% |
| src/cm.inc | 491/494 **99.39%** | 651/726 **89.67%** | 29/30 | 410/494 83.00% | 515/726 70.94% |
| src/kg_int_fwd.inc | 110/110 **100.00%** | 142/146 **97.26%** | 10/10 | 0% (encoder) | 0% |
| src/kgraph.inc | 622/1016 **61.22%** | 929/2032 **45.72%** | 44/57 | 173/1016 17.03% | 315/2032 15.50% |
| src/lines.inc | 21/106 **19.81%** | 23/140 **16.43%** | 1/2 | 0/106 0% | 0% |
| src/log.inc | 300/511 **58.71%** | 284/726 **39.12%** | 6/14 | 158/511 30.92% | 153/726 21.07% |
| src/nplane.inc | 152/152 **100.00%** | 273/302 **90.40%** | 12/12 | 64/152 42.11% | 118/302 39.07% |
| src/sci.inc | 1/436 **0.23%** | 1/638 **0.16%** | 1/31 | 0% | 0% |
| src/stream.inc | 49/81 **60.49%** | 39/86 **45.35%** | 3/4 | 49/81 60.49% | 39/86 45.35% |
| src/tbl2.inc | 678/1153 **58.80%** | 733/1678 **43.68%** | 22/37 | 201/1153 17.43% | 192/1678 11.44% |
| src/typed_int_encoder.inc | 0/244 **0.00%** | 0/462 **0.00%** | 0/17 | 0% | 0% |
| src/word.inc | 168/171 **98.25%** | 195/262 **74.43%** | 6/8 | 54/171 31.58% | 58/262 22.14% |
| src/experimental/krc.c (fuzz_krc) | 60/141 42.55% | 121/280 43.21% | 11/19 | | |
| src/experimental/kbc.c (fuzz_kbcx) | 139/205 67.80% | 120/228 52.63% | 15/21 | | |
| src/experimental/kbx.c (fuzz_kbcx) | 190/276 68.84% | 170/312 54.49% | 19/26 | | |
| src/experimental/frontier_fastq.c | 140/279 50.18% | 188/402 46.77% | 8/19 | | |
| src/experimental/frontier_entropy.c | 75/156 48.08% | 76/144 52.78% | 4/7 | | |
| src/experimental/frontier_runs.c | 16/46 34.78% | 19/62 30.65% | 2/4 | | |

### Functions that stayed at 0% (no target entered them)

Decoder-side, so these are untested decode paths:

- kolmo.c: `tbl_decode` (T_TBL, tflag 3), `e8e9_inv` (x86 filter, tflag 8: the ELF slices never selected it),
  `tpq_inv_wide`, `read_block`, `kolmo_decompress_to_fd`, `kolmo_decompress_file` (fd APIs, not fuzzed)
- kgraph.inc: `kg_fields_inv`, `kg_dec_field`, `kg_dec_done`, `kg_decimal_inv`, `kg_put_num` (decimal/field graph
  inverse); `fi_inv_2_1_0 … fi_inv_8_8_0` (12 specialised field inverses); `kg_int_inv` (only the fallback for
  big-endian/control builds, expected 0)
- lines.inc: `lines_decode` (T_LINES). log.inc: `skel_parse` (log_decode itself was reached in phase 4, 31% of
  the file's lines). word.inc: none on the decode side beyond the `_pub` wrappers
- sci.inc: the whole file: `img16_decode`, `img16_inv`, `rec_decode`, `rec_field_dec`, `planes_get`,
  `pfor_get` and every encoder function (16-bit image / record transforms, tflag 6/7: no seed and no
  round-trip input selected them)
- tbl2.inc: `tbl2_decode_pub` and 14 encoder/analysis helpers

Encoder or API-only, so 0% is expected for a decoder campaign:

- kolmo.c: `kolmo_train_dict`, `kolmo_compress_dict`, `kolmo_compress_from_fd`, `kolmo_compress_file`,
  `kolmo_strerror`, `tbl_encode_pub`, `e8e9_fwd`, `e8_mask16`, `tpq_fwd`, `tpq_wide_fwd`, `tpq_wide_pass`,
  `json_split`, `write_block`, `fmt_run`, `fmt_dec`, `scan1`, `exact_div_p10`, `array_resize`, `tls_destroy`,
  `default_threads`
- `cm_compress`, `kolmo_stream_compress`, `kgf_scan_8_0`, `kgf_write_8_0`
- typed_int_encoder.inc: all 26 (`--typed-huf` is never selected by `fuzz_roundtrip`, which does not set the bit)
- krc/kbc/kbx/frontier: all `*_encode`/`*_compress`/plan functions

Full per-function tables: `coverage/functions_all_targets.txt`; lists: `coverage/zero_functions_*.txt`.

## Reproduce

```
apt-get install -y clang llvm libclang-rt-18-dev liblzma-dev
make -j4 ARCH= build/libvendor.a build/kolmo build/krc-cpu build/kbc build/kbx build/kolmo-fastq
fuzz/campaign/build.sh                  # build/fuzz/*   (MODE=cov -> build/fuzzcov/*)
RAW=<raw inputs> SEEDS=<out> BIG=<1.2 MB text> fuzz/campaign/make_seeds.sh
W=<work> SEEDS=<seeds> SECS=6000 fuzz/campaign/run.sh   # TARGETS="fuzz_x ..." for a subset
W=<work> fuzz/campaign/sample.sh &                       # coverage/memory timeline every 5 min
python3 fuzz/campaign/summarize.py <work>/logs/*.log
CORPUS=fuzz/campaign/corpus OUT=<dir> fuzz/campaign/coverage.sh
build/fuzz/fuzz_decompress -runs=0 fuzz/campaign/corpus/fuzz_decompress   # replay under ASan
```

Logs: `logs/phase1`, `logs/phase1_stalled`, `logs/phase2`, `logs/phase3`, `logs/phase4`, and `logs/timeline.txt`
(5-minute samples of phase 1).
