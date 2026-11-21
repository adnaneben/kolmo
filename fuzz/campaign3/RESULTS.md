# Kolmo rc3 coverage-guided fuzzing, campaign 3 (17–18 Nov 2026)

Branch under test: the merged rc3, `claude/release-rc3-20261116` at `1536036`. Results and harnesses are on
`claude/rc3w2-verify-20261117`. Campaign 2's harnesses, the fuzz-only `KOLMO_FUZZ_NO_CM` guard and its corpora
(`claude/rc2-fuzz2-20261116`, `fuzz/campaign2/RESULTS.md`) never reached rc3. They are brought into the rc3 layout
here, with new targets for every decoder rc3 added.

## Summary

- **No crashes, ASan/UBSan reports, leaks, harness-invariant traps or out-of-memory kills in 16 targets.**
  - Every target ran at least 65 minutes in libFuzzer fork mode. The ones that matter most ran longer: frame decoder
    2.9 h, codec decoders 4.0 h, extensions 2.6 h.
  - In all, 226 M executions.
- **3 timeouts and 2 slow units, all in `fuzz_decompress` (the build with context mixing).**
  - All 5 are valid context-mixing frames: codec 25 at -x, and codec 7 of an rc2 frame.
  - The optimised CLI decodes them correctly in 1.0–1.6 s.
  - Under ASan+UBSan with comparison tracing, the harness's 4 decodes take 30–339 s.
  - Replayed without a time limit in the sanitizer build, all 5 finish with exit 0 and no report.
  - Classification: *slow under sanitizers, not a hang*, as in campaign 2. No fix is needed.
- **Every rc3 decoder is reached.** The coverage replay covers every decode function of codecs 16, 17, 18, 19, 20,
  21, 22, 23, 25 and 26, the KEXT, IDX1 and DDP1 readers, KPF1, profile frames and `--target`, with two exceptions
  (see *Not reached*).
  - Over the files rc3 added or rewrote: 64.2% of regions, 77.6% of functions, 65.5% of lines. The rest is mostly
    encoders, which these harnesses do not call.
  - Over the whole library: 61.4% of regions, 73.8% of functions, 64.7% of lines, 53.9% of branches.
- **Two coverage gaps found during the campaign and closed:**
  - the FASTQ family decoder (codec 16, `kf_fastq_decompress`) had no seed and no entry in `fuzz_codecs`;
  - column queries on JSON lines were never asked for.
  Each got its harness change, seeds and an extra run (batches G and H below).
- **The memory-model work (lane `memdyn`) has not landed** in rc3 at the time of this campaign:
  - `docs/MEMORY-MODEL.md` is approved, but its implementation is not on the branch;
  - no branch of that lane exists on `origin`;
  - `src/core/stream.c:108` still allocates a frame's claimed original size, the finding of campaign 2.
  The stream and limits targets of campaign 2 were run against rc3 as they are, and are clean. The run against the
  memory model waits for that lane.
- **C++:** only the fuzz binaries link a C++ runtime (libFuzzer's libstdc++). The library, CLI, wheel and crate do
  not; `benchmark/rc3w2/verify/RESULTS.md` has the `ldd`/`nm` evidence.

## Machine and tools

| | |
|---|---|
| CPU | Intel Xeon Processor @ 2.30 GHz, 4 vCPU (cloud VM), AVX2 + AVX-512 |
| Memory | 15 GiB, no swap |
| OS | Ubuntu 24.04.5 LTS, kernel 6.18.44 |
| Compiler | Ubuntu clang 18.1.3; libFuzzer, ASan and UBSan from `libclang-rt-18-dev` 1:18.1.3-1ubuntu1 |
| Coverage | llvm-profdata / llvm-cov 18 |
| Vendored libraries | `build/libvendor.a` (gcc 13.3, not instrumented): zstd, Brotli, LZ4, the vendored liblzma subset |

Build flags (`fuzz/campaign3/build.sh`): `-g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined
-fno-omit-frame-pointer -ffp-contract=off`, with the CI defines.

**Differences from campaign 2:**
- The parts of the library compiled on their own are instrumented too. These are the Makefile's `ARCHSRC`: LZP, QLFC,
  the libsais subset, `bwt_stream.c`, the dispatched kernels and the `.S` kernel. Codec 18's whole pipeline lives there.
- No system liblzma is linked, and no libbsc.
- The coverage build is `MODE=cov`.

## Harnesses

| target | builds | what it calls | invariants (trap) |
|---|---|---|---|
| `fuzz_decompress` | CM, nocm | `kolmo_decompress`, bounded and exact buffers, 1 and 2 threads | – |
| `fuzz_limited` | nocm | `kolmo_decompress_limited` (generous and tight limits), `kolmo_read_range` | range = full decode; success with the wrong length |
| `fuzz_dict` | nocm | `kolmo_decompress_dict`: the right, a wrong and no dictionary | success with the wrong length |
| `fuzz_stream` | nocm | `kolmo_stream_decompress` (memfd); frame lengths accepted up to 1 GiB | – |
| `fuzz_stream_limited` | nocm | `kolmo_stream_decompress_limited`, tight and 16 MiB limits | output above max_output_bytes; tight ≠ generous |
| `fuzz_roundtrip` | CM, nocm | compress (every mode; nocm: no ultra, quick/full analysis, native-leaf, typed-Huffman) then decompress | lossy round trip |
| `fuzz_nplane` | – | codec 15 leaf: decode, round trip, damaged round trips | lossy round trip |
| **`fuzz_codecs`** (new) | CM, nocm | codecs called directly (list below), round trips with damage | lossy round trip |
| **`fuzz_ext`** (new) | nocm | KEXT, IDX1, DDP1 (details below) | indexed ≠ unindexed answer; member ≠ `extract_all`; size ≠ stat |
| **`fuzz_profile`** (new) | nocm | KPF1, profile frames, `--target` (details below) | lossy round trip with a loaded profile |
| `fuzz_krc`, `fuzz_kbcx`, `fuzz_fastq` | – | the experimental research decoders, as in campaign 2 | wrong length |

The new harnesses in detail:

- **`fuzz_codecs`** calls each decoder directly, with no container or checksum in the way:
  - codec 18 through `codec_decompress`, and through `kolmo_bwt_decompress` directly;
  - codecs 19, 20 (the kvid inverse layout), 21, 22, 23, 25, 26 and 17;
  - codec 5 through the C port;
  - the graph block with every format-2 leaf allowed;
  - codec 16 (added in batch G).

  Round trips with damage run for codecs 16, 18, 19, 21, 22, 23 and 26.
- **`fuzz_ext`** covers:
  - the KEXT section walk;
  - `kolmo_index_info`, and `kolmo_query_run` with ranges, `#N`, names and JSON keys, fixed, word and regex
    patterns, `LINENO`/`COUNT`/`ICASE` and 1 or 2 threads;
  - DDP1 `count`, `stat`, `extract` and `extract_all`.

  An option recomputes the CRCs (`fuzz_fixup.h`). The first two invariants are checked only when the CRCs were not
  recomputed: SPEC.md allows a forged index with a valid CRC to change answers.
- **`fuzz_profile`** covers:
  - `kolmo_profile_load`, with the id and CRC recomputed on request;
  - `describe`;
  - CZN1 and CZC1 profile frames;
  - a round trip with the loaded profile;
  - the `--target` parser, the calibration table and the cost formula evaluator on edge values.

`KOLMO_FUZZ_NO_CM` makes `cm_decompress` and `cm_decompress_img` return an error at once. Their model setup costs
about 300 ms per call (src/codecs/cm.c, cm_image.c), and the guard keeps the fuzzer off it. With the guard, codec 25
exercises only its part table, and the CM builds cover the rest. `kolmo.o` is byte-identical with and without the
guard.

## Seeds

`fuzz/campaign3/make_seeds.sh` and `seedgen/` regenerate them:
- `gen_inputs.py` makes raw inputs from fixed random seeds: WAV, Y4M, long-form and wide CSV, line records, logs, JSON
  lines, text, integer and float series, PGM, FASTQ, and sets of similar CSV files.
- `build/kolmo` compresses them in every mode and route: 9 modes, -x on 8 KiB slices, and -x on whole tables, line
  records, the PGM and the FASTQ.
- `codec_seeds.c` cuts each codec payload out of the frames.

Further seeds:
- indexed files at `-B 0.0625`, plain and hc;
- `kolmo a` archives;
- profiles trained with `kolmo train` (balanced and hc), each with its frames;
- `--target` expressions.

Seeds slower than 600 ms per input in the sanitizer build were dropped: 72, listed in `logs/slow_seeds_dropped.txt`.
GR6+ frames were kept anyway.

Campaign 2's corpora (rc2 frames, which rc3 still reads) were read in place as extra corpora.

**Why the frame decoders ran at about 20 exec/s, against campaign 2's 280–1,450:**
- Every input is decoded 4 times: bounded and exact buffers, each at 1 and 2 threads.
- The rc3 routes are heavy under ASan+UBSan with comparison tracing:
  - a GR6+ model setup costs about 150 ms optimised, before anything is decoded;
  - the audio decoder's NLMS cascade is costly;
  - frames that engage the transforms decode 0.15–1.2 MB.
- The 4 vCPUs were shared with the test matrix for the first hours.

So the frame decoders' depth (75–172 k executions per target) is shallow. The codec targets compensate by calling each
decoder directly.

## Runs

`fuzz/campaign3/run.sh` runs every target with `-fork=1 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1
-rss_limit_mb=4096 -timeout=25`, at most 4 MiB per input (1 MiB for the round trip, nplane and codec targets). The
sanitizer settings are `ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

The container restarted twice during the campaign. Each batch it interrupted was run again in full on its corpus. The
interrupted parts count in the totals below; `logs/timeline.txt` has the times.

| batch | UTC | targets (four at a time on 4 vCPUs) |
|---|---|---|
| warm-up | 17 Nov 18:16–18:52 | decompress_nocm, codecs_nocm, ext_nocm, profile_nocm. Stopped and re-seeded (slow GR6+, audio and -x seeds) |
| A | 18:52–19:34, restart, 19:35–20:41 | decompress_nocm, codecs_nocm, ext_nocm, profile_nocm |
| B | 20:41–21:47 | limited_nocm, dict_nocm, stream_nocm, stream_limited_nocm |
| C | 21:47–22:10, restart, 18 Nov 03:06–04:12 | roundtrip_nocm, codecs (CM), decompress (CM), nplane |
| D | 04:12–05:18 | roundtrip (CM), krc, kbcx, fastq |
| E | 05:18–06:24 | codecs_nocm with inputs of at most 64 KiB |
| F | 05:25–05:56 | ext_nocm with JSON key columns |
| G | 06:24–07:30 | codecs_nocm with codec 16, decompress_nocm with codec-16 seeds |
| H | 07:48–08:09 | ext_nocm with JSON-column seeds |

Totals per target (all runs; `fuzz/campaign3/logs/`; `cov` is libFuzzer's edge count at the end of the last run):

| target | wall time | executions | cov | oom / timeout / crash |
|---|---:|---:|---:|---|
| fuzz_decompress_nocm | 10,400 s | 171,788 | 9,590 | 0 / 0 / 0 |
| fuzz_codecs_nocm | 14,243 s | 71,984 | 23,621 | 0 / 0 / 0 |
| fuzz_ext_nocm | 9,484 s | 2,213,547 | 6,749 | 0 / 0 / 0 |
| fuzz_profile_nocm | 6,478 s | 30,508,240 | 9,540 | 0 / 0 / 0 |
| fuzz_limited_nocm | 3,984 s | 74,530 | 6,434 | 0 / 0 / 0 |
| fuzz_dict_nocm | 3,973 s | 85,364 | 7,003 | 0 / 0 / 0 |
| fuzz_stream_nocm | 3,936 s | 24,424,079 | 3,424 | 0 / 0 / 0 |
| fuzz_stream_limited_nocm | 3,938 s | 3,776,658 | 3,089 | 0 / 0 / 0 |
| fuzz_roundtrip_nocm | 5,315 s | 170,081 | 33,095 | 0 / 0 / 0 |
| fuzz_codecs (CM) | 5,257 s | 25,141 | 19,488 | 0 / 0 / 0 |
| fuzz_decompress (CM) | 5,253 s | 6,576 | 8,933 | 0 / **3** / 0 (slow, below) |
| fuzz_nplane | 5,289 s | 338,186 | 3,542 | 0 / 0 / 0 |
| fuzz_roundtrip (CM) | 3,903 s | 8,683 | 34,269 | 0 / 0 / 0 |
| fuzz_krc | 3,923 s | 81,424,592 | 223 | 0 / 0 / 0 |
| fuzz_kbcx | 3,926 s | 25,791,651 | 668 | 0 / 0 / 0 |
| fuzz_fastq | 3,922 s | 57,077,073 | 802 | 0 / 0 / 0 |

The warm-up (35 minutes, 4 targets, 18 M executions) was also clean, apart from 3 slow units of `fuzz_codecs_nocm`:
GR6+ round trips whose seeds were then made smaller.

## Findings

| kind | count | details |
|---|---|---|
| crash (ASan, UBSan) | 0 | – |
| harness invariant (`__builtin_trap`) | 0 | – |
| leak | 0 | – |
| out of memory (4 GiB) | 0 | – |
| timeout (> 25 s) | 3 | `fuzz_decompress`; slow under sanitizers, see below |
| slow unit (> 10 s) | 2 (+3 in the warm-up) | the same class |

The timeouts and slow units are in `fuzz/campaign3/slow_units/`:

| input | content | CLI (optimised, 1 thread) | sanitizer build, no time limit |
|---|---|---|---|
| timeout-204d157b… (77 KB) | v11, 1 block, codec 25, 827 KB original | decodes, 1.57 s | exit 0, 339 s, no report |
| timeout-237a50f4… (51 KB) | v11, 1 block, codec 25, 236 KB original | decodes, 1.03 s | exit 0, 145 s, no report |
| timeout-6027e631… (702 B) | v3, 1 block, codec 7 (CM), 345 KB original (campaign 2 corpus) | decodes, 0.96 s | exit 0, 57 s, no report |
| slow-unit-d676e4a6… | CM frame | – | exit 0, 30 s |
| slow-unit-e200a220… | CM frame | – | exit 0, 47 s |

The harness decodes each input 4 times, and the sanitizers and comparison tracing slow a context-mixing decode by
roughly 50 to 200 times (1.0 s optimised against 57–339 s for the 4 decodes here). Together that takes a 1 s decode
past 25 s. These inputs are not hangs: the decoder's work is bounded by the frame's original size.

Outside the fuzzer, the CI and test runs found three defects (missing binding file, a duplicate symbol in a test, an
extra file mapping). They are reported in `benchmark/rc3w2/verify/RESULTS.md`.

## Coverage

`MODE=cov fuzz/campaign3/build.sh` then `fuzz/campaign3/coverage.sh`. The replay covers every final corpus, the seeds
and campaign 2's corpora. Reports are in `coverage/`: `report.txt` per file, `report_rc3.txt`, `functions.txt`, and
`zero_functions.txt`, which lists the functions no binary executed.

Whole library (all of `src/` except `experimental/`, `arch/metal`, `arch/aarch64`, plus the six research decoders):
**61.4% regions, 73.8% functions, 64.7% lines, 53.9% branches.**

Files rc3 added or rewrote:

| file | regions | functions | lines | branches |
|---|---:|---:|---:|---:|
| codecs/bwt.c | 92.3% | 100% | 100% | 66.7% |
| codecs/bwt_stream.c | 79.5% | 100% | 95.3% | 61.1% |
| algo/lzp.c | 75.4% | 88.9% | 81.8% | 67.5% |
| algo/qlfc.c | 83.9% | 86.4% | 87.6% | 79.7% |
| algo/sais/sais.c | 53.6% | 69.3% | 54.5% | 44.9% |
| algo/par.c | 89.0% | 100% | 100% | 79.7% |
| codecs/audio.c (19) | 91.5% | 99.0% | 95.9% | 86.9% |
| codecs/video.c (20) | 94.4% | 100% | 98.5% | 87.0% |
| codecs/planes_leaf.c (21) | 93.6% | 100% | 100% | 86.5% |
| codecs/hufzstd_leaf.c (22) | 64.8% | 100% | 78.6% | 53.9% |
| codecs/zrun_leaf.c (23) + algo/zerorun.c | 92.6% / 96.3% | 100% / 100% | 94.9% / 100% | 81.8% / 85.3% |
| codecs/cm_par.c (25) | 56.3% | 55.6% | 60.6% | 51.1% |
| codecs/gr6.c (26) | 88.8% | 95.7% | 93.9% | 82.4% |
| codecs/fastq.c (16) | 65.4% | 80.6% | 77.7% | 53.7% |
| core/extension.c (KEXT) | 80.6% | 66.7% | 72.9% | 73.4% |
| core/index.c (IDX1) | 56.9% | 78.8% | 70.0% | 47.3% |
| core/dedup.c (DDP1) | 47.1% | 50.0% | 52.8% | 37.1% |
| transforms/table_cols.c | 57.6% | 85.7% | 70.2% | 44.4% |
| plan/profile.c (KPF1) | 63.1% | 64.1% | 63.5% | 55.0% |
| plan/target.c | 58.8% | 83.3% | 62.9% | 57.2% |
| core/decode.c | 85.7% | 100% | 90.4% | 77.0% |
| core/stream.c | 55.6% | 80.0% | 64.4% | 52.0% |
| **total of these files** | **64.2%** | **77.6%** | **65.5%** | **57.0%** |

These figures come from the replay before batch H. `ix_json_field` and `ix_json_value` were checked separately with the
JSON-column seeds: 70.5% and 52.3% of regions.

**Not reached.** Every unreached function in these files is an encoder or trainer, except the following:
- `qlfc_dec_task` (QLFC decode on threads), `qlfc_split_blocks` and `par_main`: they need coder blocks of at least
  4 MiB, larger than any fuzz input.
- 135 functions of the libsais subset: the recursion on 32-bit alphabets and the OpenMP-shaped region variants, which
  only very large inputs or more threads reach.
- In `graph.c`, nine `fi_inv_*` integer-inverse specialisations, `kg_int_inv` and unfused `kg_decimal_inv`. The encoder
  never writes them; campaign 2 found the same.

The encoder and trainer functions left unreached are:
- `kolmo_archive_create` and its chunker;
- `kolmo_compress_indexed` and the filter builder;
- `kp_train`;
- `kt_compress_setting`;
- `cmp_compress_*`;
- `kolmo_stream_compress`;
- `tblc_compress`.

The round-trip targets reach the encoders only through `kolmo_compress`, and inputs of at most 1 MiB do not engage
these routes.

## Files

| path | content |
|---|---|
| `fuzz/fuzz_*.c`, `fuzz/fuzz_fixup.h`, `fuzz/fuzz_dict_data.h` | harnesses |
| `fuzz/campaign3/build.sh`, `run.sh`, `make_seeds.sh`, `seedgen/` | build, runner, seeds |
| `fuzz/campaign3/coverage.sh`, `zero_functions.py`, `summarize.py` | coverage replay and reports, log summary |
| `fuzz/campaign3/corpus/<target>/` | the minimised (`-merge=1`) final corpora, files of at most 4 KiB. A file kept in `<t>_nocm` is not repeated in `<t>`, nor a file of `fuzz_decompress_nocm` in `fuzz_limited_nocm` and `fuzz_dict_nocm`, nor one of `fuzz_stream_nocm` in `fuzz_stream_limited_nocm`, so pass the sibling directory as a second corpus. Larger inputs are regenerated by `make_seeds.sh` and a run |
| `fuzz/campaign3/logs/` | every libFuzzer log by batch, the timeline, the dropped slow seeds |
| `fuzz/campaign3/coverage/` | llvm-cov reports |
| `fuzz/campaign3/slow_units/` | the 3 timeouts and 2 slow units |

## Reproduce

```sh
sudo apt-get install libclang-rt-18-dev                  # libFuzzer and the sanitizer runtimes for clang 18
make -j4 build/kolmo && fuzz/campaign3/build.sh          # build/fuzz/*
W=/tmp/c3 fuzz/campaign3/make_seeds.sh                   # $W/seeds/<target>
W=/tmp/c3 EXTRA=<campaign 2 corpus dir> SECS=3900 TARGETS="fuzz_codecs_nocm fuzz_ext_nocm" fuzz/campaign3/run.sh
MODE=cov fuzz/campaign3/build.sh && W=/tmp/c3 OUT=/tmp/cov EXTRA=<...> fuzz/campaign3/coverage.sh
```
