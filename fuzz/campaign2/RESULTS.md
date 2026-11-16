# Kolmo rc2 coverage-guided fuzzing, campaign 2 (16 Nov 2026)

Branch under test: `claude/release-rc2-20261115` at `9d4496a` (stream fix: frames grow as bytes arrive;
`kolmo_stream_decompress_limited`). Results on `claude/rc2-fuzz2-20261116`. The starting point was campaign 1
(`claude/rc2-fuzz-20261115`, `fuzz/campaign/RESULTS.md`): its harnesses and scripts were copied here. Its corpora, logs
and coverage reports remain on that branch.

## Summary

- **No crashes, ASan/UBSan reports, harness-invariant traps, leaks, out-of-memory kills or timeouts** in 14 targets.
  The guarded targets ran about 17.6 M executions, campaign 1's frame decoders about 54 k. Every target ran at least
  65 minutes in libFuzzer fork mode. The minimised final corpora also replay clean under ASan+UBSan.
- **11 slow units** (libFuzzer's >10 s report, under the 25 s timeout). 10 are the 128×128 context-mixing image seed
  (codec 17) and checksum-failing mutants of it. 1 is an ultra round trip of 21 KB of text. Each takes 8–14 s under
  ASan at -O1 and 0.1–0.8 s with the optimised CLI. Classification: *slow under sanitizers, not a hang*.
  Details are under Findings below.
- **Depth fixed.** A fuzz-only define, `KOLMO_FUZZ_NO_CM`, stubs out the context-mixing decoders. With it the frame
  decoders ran at 280–1,450 exec/s on average, against campaign 1's 1–2 exec/s. That is 1.1–5.7 M executions per
  target, against 14–23 k. Three of the five guarded targets reached the 500 exec/s target; `fuzz_decompress_nocm`
  (283) and `fuzz_stream_nocm` (715 average, but less than 500 in the last half hour) are discussed under Runs.
- **Unreached decode paths are now reached.** Seeds made to drive `build/kolmo` into every transform enter all the
  functions on the brief except six `fi_inv_*` specialisations, `kg_int_inv` and unfused `kg_decimal_inv`. The
  encoder never emits those (see *Still at 0%*). The table transform needed hand-crafted frames, because no encoder
  writes it any more.
  - sci.inc lines: 0.2% → 94.3%
  - lines.inc: 19.8% → 100%
  - log.inc: 58.7% → 98.6%
  - kgraph.inc: 61.2% → 83.4%
  - kolmo.c: 62.9% → 82.8%
  - all files: 59.3% → 77.9% (lines), 48.1% → 64.9% (branches)
- **Stream fix confirmed.** Forged frame-length claims are rejected in under 0.13 s under ASan with about 2 MB peak
  RSS. That covers 1 GiB, 16 GiB, 2^63 and a forged frame after a valid one; the harness now accepts claims up to
  1 GiB. One related point remains, outside the fuzzer's reach by design: the unlimited `kolmo_stream_decompress`
  still allocates a frame's claimed original size without a bound (`src/stream.inc:108`). See Findings.

## Machine and tools

| | |
|---|---|
| CPU | Intel Xeon @ 2.10 GHz, 4 vCPU (cloud VM) |
| Memory | 16 GB (15.7 GiB), no swap |
| OS | Ubuntu 24.04.5 LTS, kernel 6.18.44 |
| Compiler | Ubuntu clang 18.1.3 (1ubuntu1), libFuzzer/ASan/UBSan from `libclang-rt-18-dev` 1:18.1.3-1ubuntu1 |
| Coverage | llvm-profdata / llvm-cov 18 |
| Vendor libs | `build/libvendor.a` (`make ARCH= build/libvendor.a`, gcc 13.3.0, not instrumented); liblzma 5.4.5 |

Build flags are those of campaign 1 (`fuzz/campaign/build.sh`): `-g -O1 -fsanitize=fuzzer,address,undefined
-fno-sanitize-recover=undefined -ffp-contract=off` and the CI fuzz-smoke defines. The coverage build is `MODE=cov`.

## The one library change: `KOLMO_FUZZ_NO_CM`

`src/cm.inc` has a guard at the top of `cm_decompress` and `cm_decompress_img`. When the define is set, both return
-1 at once. This is the only source change.

```c
#ifdef KOLMO_FUZZ_NO_CM   /* fuzz-only builds: skip context-mixing model setup (~300 ms per call) */
    (void)in; (void)cn; (void)out; (void)n; return -1;
#endif
```

Normal builds do not change. I ran `make ARCH= all` before and after the edit, and the five binaries (`kolmo`,
`kolmobench`, `kolmotest`, `kolmoparallel`, `kolmoresource`) are **byte-identical** (same sha256). `make ARCH= check`
passes (ALL PASSED).

`fuzz/campaign/build.sh` builds each frame-decoder target twice: `<t>` (context mixing included, as in campaign 1)
and `<t>_nocm` (with the define). Of the stream targets only `_nocm` builds are fuzzed, because the stream code calls
`kolmo_decompress(_limited)` per frame and the CM targets cover CM frames. A CM `fuzz_stream` is built for the
baseline replay. `fuzz_roundtrip_nocm` excludes ultra mode, whose decoder is stubbed out. It uses the other bits of
the mode byte to vary quick or full analysis and the native-leaf and typed-Huffman opt-ins.

## Targets

| target | build | API | harness |
|---|---|---|---|
| fuzz_decompress(_nocm) | both | `kolmo_decompress`, bounded and exact buffers, 1 and 2 threads | `fuzz/fuzz_decompress.c` |
| fuzz_limited(_nocm) | both | `kolmo_decompress_limited` (generous/tight) + `kolmo_read_range` (= full decode) | `fuzz/fuzz_limited.c` |
| fuzz_dict(_nocm) | both | `kolmo_decompress_dict`: right, wrong and no dictionary | `fuzz/fuzz_dict.c` |
| fuzz_stream_nocm | fast | `kolmo_stream_decompress`; **frame-length claims accepted up to 1 GiB** (was 16 MiB); claimed original size still capped at 16 MiB (see Findings) | `fuzz/fuzz_stream.c` (updated) |
| fuzz_stream_limited_nocm | fast | **new**: `kolmo_stream_decompress_limited` with tight limits picked by the last byte (max_output 1 B–16 MiB, max_block 1 B–1 MiB, max_blocks 1–4096) and with 16 MiB limits. No size is pre-filtered. Traps if a tight decode returns more than max_output_bytes, or if both succeed with different lengths or bytes | `fuzz/fuzz_stream_limited.c` |
| fuzz_roundtrip(_nocm) | both | compress → decompress must be lossless | `fuzz/fuzz_roundtrip.c` (nocm variant added) |
| fuzz_nplane, fuzz_krc, fuzz_kbcx, fuzz_fastq | as campaign 1 | | |

## Seeds (item 2 of the brief)

`fuzz/campaign2/make_seeds.sh` and `fuzz/campaign2/seedgen/` regenerate everything. Raw inputs are generated
(Python, fixed random seeds) and compressed with `build/kolmo` at `-m balanced|ratio|speed|hc|max|eco` and `-q`:

| input | what it drives |
|---|---|
| CSV/TSV 0.2–4 MB, integer + decimal + text columns, quoted, ragged rows, quoted fields with commas | tbl2 (tflag 4) incl. `num_decode` (ragged/quoted: non-aligned path); crafted tflag 3 |
| wide CSV (260–400 columns; `,` `;` with decimal comma, tab, `|`) | kolmo graph FIELDS/DECIMAL: `kg_fields_inv`, `kg_dec_field`, `kg_dec_done`, `kg_put_num` |
| wide text-only tables (270–300 columns) | `tpq_inv_wide` (T_TP wide) |
| CSV with `total = qty × price`, `sum = a + b`, grouped series | tbl2 formula columns `fm_int`, `fm_predict` (added after the runs, not fuzzed) |
| x86-64 ELF `/usr/bin/nasm`, `strace`, `tmux` (1.1–2.1 MB) | `e8e9_inv` |
| fixed-width text records, SQL lines, JSON array | `lines_decode`, `log_decode` |
| 10/12-bit images: PGM 16-bit (maxval 4095), raw LE/BE with header, raw without header | `img16_decode`, `img16_inv`. `img16_detect` only accepts samples below 8192, so 16-bit-range images do not qualify |
| fixed-length binary records (48/20/12-byte structs; doubles on a 0.01 / 0.125 grid) | `rec_decode`, `rec_field_dec`, `planes_get`, `pfor_get` (quantised doubles) |
| int16/32/64 series: walks, and smooth series with rare spikes of 2^15–2^62, signed and unsigned (crossing 2^31) | graph integer leaves: 18 `fi_inv_W_OW_SG` variants. Spikes force the wide residual planes |
| NDJSON, JSON arrays, access/app/syslog logs (0.6–4 MB) | log/JSON templates; `skel_parse` through `fuzz_roundtrip_nocm` |
| campaign 1's tests/legacy frames, `tests/legacy_frames.h` (v1–v5) | old container versions |

**Table transform (tflag 3, `tbl_decode`).** The current encoder never writes tflag 3. Only its planner trial calls
`tbl_encode`, and table blocks are written as tbl2, tflag 4. `fuzz/campaign2/craft_tbl3.c` takes a T_TBL frame from
`build/kolmo` and the raw CSV. It rewrites each table block as plan prefix | u32 length | zstd(`tbl_encode`(block))
with tflag 3, then recomputes the header CRC. The block checksum is over the decoded bytes, so it does not change.
Each crafted frame is decoded and compared with the raw input. 16 crafted frames were made from 7 CSVs.

**Pre-run coverage check.** `seedgen/funchk.sh` replays every frame once through `build/fuzzcov/fuzz_decompress_nocm`
and lists the target functions each one enters. I iterated (5 rounds of inputs) until every function on the brief
that the encoder can produce was entered. For each function the seeds keep the 2 frames with the smallest original
size that enter it. `coverage/seeds_prerun/` is the replay of all assembled seeds before fuzzing: 76.4% of lines
over all files.

**Context-mixing (`-x`) seeds, fewer and smaller.** The CM targets got up to 60 CM frames of 8 KiB or less from
campaign 1's corpus, plus `-x`, `-x -l 1` and `-x -l 2` frames of 24–64 KiB slices (CSV, log, JSON, wide CSV, int32,
ELF, fixed-width text, small images). Seeds slower than 4 s per run under ASan were removed. One 128×128 grey image
frame (codec 17, the only one that reaches `cm_decompress_img`, 10 s per run under ASan) was kept on purpose. The
result is 34/67/66 seeds for decompress/limited/dict, against 437–707 in campaign 1.

Streams: campaign 1's stream corpus, `-s`, `-S 1` and `-s -m hc` streams of 0.4–0.7 MB inputs, and forged CZNS
inputs. The forged inputs claim frame lengths of 2^30±1, 2^34(+1), 2^63 and 64 MiB(+1), alone and after a valid
frame. Round trip: campaign 1's corpus plus 0.15–1 MB slices of the new inputs with 6 mode bytes.

## Runs (item 4 of the brief)

Options for every target (`fuzz/campaign2/run.sh`): `-fork=1 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1
-rss_limit_mb=4096 -timeout=25 -max_len=4194304` (1 MiB for roundtrip/nplane). Sanitizer options were
`ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

| batch | time (UTC) | targets (each 3,900 s) |
|---|---|---|
| warm-up | 10:18–10:29 | first start of the 5 guarded targets + roundtrip_nocm. I stopped it after 11 min because mutants of a 2.3 MB-original lines frame were taking over the corpus. Seeds were then re-picked by smallest original size, streams of >0.7 MB inputs dropped, and roundtrip_nocm moved to batch B. Logs are in `logs/warmup/` |
| A | 10:29–11:35 | decompress_nocm, limited_nocm, dict_nocm, stream_nocm, stream_limited_nocm (5 processes / 4 cores) |
| B | 11:35–12:41 | decompress, limited, dict (CM), roundtrip, roundtrip_nocm, nplane, krc, kbcx, fastq (9 / 4 cores) |

`exec/s` is executions over wall time. `cov` is libFuzzer edges of that binary; `cov@Nm` is the value N minutes in.

| target | wall s | executions | exec/s | cov | ft | corpus | oom/timeout/crash | cov@1m | @5m | @10m | @30m | @60m |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fuzz_decompress_nocm | 3945 | 1,115,701 | 282 | 3276 | 3348 | 644 | 0/0/0 | 2800 | 2991 | 3128 | 3246 | 3266 |
| fuzz_limited_nocm | 3942 | 2,704,645 | 686 | 3282 | 3254 | 594 | 0/0/0 | 3000 | 3126 | 3140 | 3256 | 3279 |
| fuzz_dict_nocm | 3934 | 5,703,228 | 1,449 | 3136 | 3336 | 792 | 0/0/0 | 2927 | 3040 | 3083 | 3111 | 3132 |
| fuzz_stream_nocm | 3935 | 2,814,787 | 715 | 2024 | 2159 | 525 | 0/0/0 | 1866 | 1939 | 1945 | 2018 | 2023 |
| fuzz_stream_limited_nocm | 3938 | 5,281,395 | 1,341 | 2258 | 2686 | 699 | 0/0/0 | 1919 | 2011 | 2071 | 2220 | 2257 |
| fuzz_decompress (CM) | 3927 | 1,625 | 0.4 | 2150 | 1854 | 127 | 0/0/0 | – | 1407 | 1930 | 1971 | 2150 |
| fuzz_limited (CM) | 3931 | 2,014 | 0.5 | 3616 | 3085 | 227 | 0/0/0 | – | 2488 | 3295 | 3411 | 3615 |
| fuzz_dict (CM) | 3917 | 1,541 | 0.4 | 3154 | 2616 | 171 | 0/0/0 | – | 2193 | 2193 | 2811 | 3151 |
| fuzz_roundtrip (CM) | 3921 | 2,215 | 0.6 | 17957 | 14931 | 1008 | 0/0/0 | – | – | – | 14639 | 17586 |
| fuzz_roundtrip_nocm | 3959 | 12,876 | 3 | 16299 | 18764 | 2018 | 0/0/0 | – | 15750 | 16057 | 16233 | 16296 |
| fuzz_nplane | 3975 | 60,522 | 15 | 3371 | 3015 | 601 | 0/0/0 | 2989 | 3281 | 3300 | 3358 | 3369 |
| fuzz_krc | 3933 | 8,509,587 | 2,163 | 229 | 210 | 48 | 0/0/0 | 220 | 229 | 229 | 229 | 229 |
| fuzz_kbcx | 3971 | 6,106,266 | 1,537 | 671 | 658 | 89 | 0/0/0 | 671 | 671 | 671 | 671 | 671 |
| fuzz_fastq | 3931 | 16,367,593 | 4,163 | 780 | 671 | 155 | 0/0/0 | 659 | 711 | 711 | 768 | 777 |

Warm-up (≈620 s each, logs only): decompress_nocm 191,366, limited_nocm 140,992, dict_nocm 262,092, stream_nocm
108,134, stream_limited_nocm 78,806, roundtrip_nocm 5,136 executions. All were clean.

**Against the 500 exec/s target.**
- dict_nocm (1,449), stream_limited_nocm (1,341), stream_nocm (715) and limited_nocm (686) met it on average.
  stream_nocm's later rate was lower, about 190–250 exec/s, as the corpus filled with the 0.4–0.7 MB-original streams.
- decompress_nocm reached 282, against campaign 1's 1–2. Its harness decodes each input four times: a 64 KiB bounded
  buffer and the exact size, each with 1 and 2 threads, and the 2-thread calls start threads under ASan. Also, the
  frames that reach the new transforms decode 0.15–1.2 MB each, since most transforms only engage at 256 KiB or more.
- The context-mixing targets remain at about 0.5 exec/s. Under ASan at -O1 a CM decode costs 2–10 s even for 16 KB
  frames: the 128×128 image takes 10 s in `fuzz_decompress`, 24 s for an RGB one. They ran for coverage of the CM
  code. Their depth is still shallow, about 1.5–2 k executions each.

**Corpus minimisation.** For each target, `-merge=1` of its seeds and its corpus through the coverage build gives
the final corpus: 411 MB in total, of which 268 MB is round-trip inputs. That is too much to commit. The committed
corpus, `fuzz/campaign2/corpus/` (44 MB apparent size), contains:
- every minimised file of 32 KiB or less;
- the per-function seed frames (`fuzz_decompress_nocm/seed__*`, including the crafted tflag-3 frames, the legacy
  frames and the formula CSV frames).

Files that are identical between sibling targets are stored once. Pass the sibling directory as a second corpus:

| target | sibling directory |
|---|---|
| limited_nocm, dict_nocm | decompress_nocm |
| limited, dict | decompress |
| stream_limited_nocm | stream_nocm |
| roundtrip_nocm | roundtrip |

The larger minimised files (mutated multi-MB-original frames and round-trip inputs over 32 KiB) are regenerated
from the seeds by `make_seeds.sh` and a run. All 14 committed corpora replay clean under ASan+UBSan.

## Findings (item 5 of the brief)

| kind | count | details |
|---|---|---|
| crash (ASan / UBSan) | 0 | – |
| harness invariant (`__builtin_trap`): lossy round trip; wrong-length success; range ≠ full decode; limited stream output above max_output_bytes; limited ≠ generous stream output | 0 | – |
| leak | 0 | – |
| out-of-memory (4096 MB) | 0 | – |
| timeout (>25 s) | 0 | – |
| slow unit (>10 s) | 11 | below; slow under sanitizers |
| outside the harness: unbounded allocation in `kolmo_stream_decompress` | 1 | below; reproduced directly, not by the fuzzer |

### Slow units: `fuzz/campaign2/slow_units/`

| file | what it is | ASan harness | optimised CLI |
|---|---|---|---|
| fuzz_decompress__slow-unit-c9cc66e3… (also under fuzz_dict) | the seed itself: `-x` frame of a 128×128 8-bit PGM, one block, codec 17 (CM image model), 16,399 bytes | ~10 s | 0.20 s, decodes |
| fuzz_decompress__slow-unit-{169e84e1, 7125611d, 8be8c840, 9fc8598b, ca2a2e4c, d65ffaab, ec506daf, ee2ae80f} | mutants of that seed (1 to 7,239 bytes changed), same header | 8.9–14 s | 0.10–0.48 s, `checksum mismatch` |
| fuzz_roundtrip__slow-unit-156aa9ca… | mode byte 5 (ultra) + 21,625 bytes of C source | 7.9 s | 0.84 s compress + decompress, lossless |

There is no stack: these are not sanitizer reports. The time is spent in `cm_new`/`cm_dec_loop` (`src/cm.inc`),
model setup and per-bit mixing, made 10–60 times slower by ASan+UBSan at -O1. A corrupted CMI block is rejected only
after the full CM decode, by the block checksum (`src/kolmo.c` `decode_block`, checksum after
`codec_decompress`). This is inherent to a per-block checksum over decoded bytes. Classification: *slow input under
sanitizers; not a hang, not a defect*. Nothing to minimise: the mutants are the same size as the seed, and the
seed is valid.

### Stream: unbounded allocation of a frame's claimed original size (no limits)

- **File and line:** `src/stream.inc:108` (`kolmo_stream_decompress_limited` with `limits == NULL`, the path
  `kolmo_stream_decompress` takes):
  `if ((size_t)osz > ocap) { free(out); out = malloc(osz ? (size_t)osz : 1); ... }`. `osz` is
  `kolmo_original_size()` of the frame.
- **Input:** `fuzz/campaign2/findings/stream-claim-8GiB-original.czns` (38,533 bytes). It is a valid 38 KB table
  frame whose header `orig` (and block `raw`) were set to 8 GiB, with the header CRC recomputed.
- **Result:**
  - ASan: `requested allocation size 0x200000000 ... exceeds maximum supported size`, stack `malloc` ←
    `kolmo_stream_decompress_limited src/stream.inc:108:52` ← caller.
  - Without ASan: `malloc` of 8 GiB succeeds lazily (overcommit, peak RSS 2 MB). `kolmo_decompress` then returns
    `KOLMO_ERR_CORRUPT`. With 16 GiB−1 it returns `KOLMO_ERR_ALLOC`.
- **Classification:** unbounded allocation driven by input (a resource-exhaustion risk where overcommit is off or
  under a memory rlimit). No memory-safety error.
- **With limits:** `kolmo_stream_decompress_limited` with any `max_output_bytes` refuses the same input before
  allocating (`stream.inc:107`, verified under ASan).
- **Why the fuzzer did not find it:** `fuzz_stream` still skips frames whose claimed original size exceeds 16 MiB,
  following campaign 1's rule against trusting input sizes. This is the second half of campaign 1's stream
  observation. The 9d4496a fix addressed the frame length, not the original size. It needs a decision (cap, or
  require limits), not a fix from this campaign.

The first half is fixed. `fuzz/campaign2/findings/stream-forged-16GiB-frame-length.czns` (84 bytes, frame length
2^34) and the 1 GiB / 2^63 variants return `KOLMO_ERR_CORRUPT` in under 0.13 s under ASan, with 2 MB peak RSS. The
frame buffer grows in 64 MiB steps only as bytes arrive (`stream.inc:96–104`).

## Coverage (llvm-cov replay; `fuzz/campaign2/coverage.sh`)

*c1* is campaign 1's final corpora (`claude/rc2-fuzz-20261115:fuzz/campaign/corpus`), replayed through this
commit's coverage builds (fuzz_decompress/limited/dict/stream with CM, roundtrip, nplane, krc, kbcx, fastq). *c2* is
this campaign's full minimised final corpora (all 14 targets). *All* combines every target, including the round
trips, which run the encoder. *Dec* combines only the frame and stream decoders (c1: decompress, limited, dict,
stream; c2: those, both builds, plus stream_limited). Denominators include encoder code in the same file, so *dec*
percentages are low by construction.

| file | lines c1 | lines c2 | branches c1 | branches c2 | functions c1 → c2 | dec lines c1 → c2 | dec branches c1 → c2 |
|---|---|---|---|---|---|---|---|
| src/kolmo.c | 62.94% (1518/2412) | **82.79%** (1997/2412) | 52.40% | **68.61%** | 96 → 104 /123 | 21.68% → **30.18%** | 16.75% → **22.02%** |
| src/cm.inc | 99.40% | **99.40%** | 89.86% | **88.49%** ¹ | 33 → 33 /34 | 83.33% → **90.08%** | 71.23% → **81.51%** |
| src/kg_int_fwd.inc | 100.00% | **100.00%** | 97.26% | **97.26%** | 10 → 10 /10 | encoder | encoder |
| src/kgraph.inc | 61.22% (622/1016) | **83.37%** (847/1016) | 45.72% | **64.42%** | 44 → 55 /57 | 17.03% → **24.80%** | 15.50% → **21.60%** |
| src/lines.inc | 19.81% | **100.00%** | 16.43% | **80.71%** | 1 → 2 /2 | 0.00% → **48.11%** | 0.00% → **32.14%** |
| src/log.inc | 58.71% | **98.63%** | 39.12% | **86.09%** | 6 → 10 /14 | 30.92% → **31.31%** | 21.07% → **24.79%** |
| src/nplane.inc | 100.00% | **100.00%** | 90.40% | **91.39%** | 12 → 12 /12 | 42.11% → **42.11%** | 39.07% → **46.03%** |
| src/sci.inc | 0.23% (1/436) | **94.27%** (411/436) | 0.16% | **79.62%** | 1 → 27 /31 | 0.00% → **26.38%** | 0.00% → **21.32%** |
| src/stream.inc | 64.44% | **64.44%** ² | 44.90% | **52.04%** | 4 → 4 /5 | 64.44% → **64.44%** | 44.90% → **52.04%** |
| src/tbl2.inc | 58.80% | **65.13%** | 43.68% | **51.37%** | 22 → 23 /37 | 17.43% → **27.84%** | 11.44% → **22.82%** |
| src/typed_int_encoder.inc | 0.00% | **3.69%** | 0.00% | **1.30%** | 0 → 2 /17 | encoder | encoder |
| src/word.inc | 98.25% | **98.25%** | 74.43% | **82.44%** | 6 → 6 /8 | 31.58% → **31.58%** | 22.14% → **25.57%** |
| src/experimental/krc.c | 42.55% | 42.55% | 43.21% | 43.21% | 11 → 11 /19 | | |
| src/experimental/kbc.c | 67.80% | 67.80% | 52.63% | 52.63% | 15 → 15 /21 | | |
| src/experimental/kbx.c | 68.84% | 68.84% | 54.49% | 54.49% | 19 → 19 /26 | | |
| src/experimental/frontier_fastq.c | 50.18% | 50.18% | 46.77% | 47.01% | 8 → 8 /19 | | |
| src/experimental/frontier_entropy.c | 48.08% | 48.08% | 52.78% | 52.78% | 4 → 4 /7 | | |
| src/experimental/frontier_runs.c | 34.78% | 34.78% | 30.65% | 30.65% | 2 → 2 /4 | | |
| **TOTAL** | **59.30%** | **77.85%** | **48.07%** | **64.86%** | | 23.91% → **32.69%** | 18.44% → **25.95%** |

¹ `cm_decompress` and `cm_decompress_img` have two profile records each, the guarded and the full build, so
llvm-cov sees them twice. The CM figures come mostly from the CM builds.
² The 32 uncovered lines of stream.inc are `kolmo_stream_compress` (encoder). The whole decoder side is covered.

Note: campaign 1's own table (`fuzz/campaign/RESULTS.md`) was measured at `fd66a97`. The c1 column above replays the
same corpora on `9d4496a`, so the two runs are compared on one code base.

Reports: `coverage/final/` (c2), `coverage/c1corpora/` (c1), `coverage/seeds_prerun/` (seeds before fuzzing). Each
has `report.txt`, `report_decoders.txt`, `functions*.txt` and `zero_functions*.txt`.

### Newly reached (0% in c1, entered in c2)

`tbl_decode`, `e8e9_inv`, `lines_decode`, `tpq_inv_wide`, `kg_fields_inv`, `kg_dec_field`, `kg_dec_done`,
`kg_put_num`, `fi_inv_2_1_0`, `fi_inv_4_1_0`, `fi_inv_4_2_0`, `fi_inv_8_1_1`, `fi_inv_8_2_1`, `fi_inv_8_4_1`. Campaign 2
seeds also enter `fi_inv_2_2_1`, `2_4_0`, `2_4_1`, `4_2_1`, `4_4_0`, `4_4_1`, `4_8_0`, `4_8_1` and `8_8_1`. In sci.inc:
`img16_decode`, `img16_inv`, `rec_decode`, `rec_field_dec`, `planes_get`, `pfor_get` and the image/record encoders.
In log.inc: `skel_parse`, `put_column`, `put_ints`, `json_next_value`. In kolmo.c: `tpq_fwd`, `tpq_wide_fwd`,
`tpq_wide_pass`, `e8e9_fwd`, `e8_mask16`. Also `kg_encode_table` and the 2 typed-int functions that run.

### Still at 0%, and why

On the decode side:

- `kg_decimal_inv` (unfused): every DECIMAL node the encoder writes (`kg_encode_table`, `src/kgraph.inc:651`) reads a
  FIELDS column, so the decoder always takes the fused path (`kgraph.inc:1235`). Only a hand-built graph would reach
  it, and none was made.
- `fi_inv_2_8_0`, `fi_inv_2_8_1`: a 16-bit series cannot have a residual of 4 bytes or more (`kg_bw`,
  `kgraph.inc:670`, gives 8 only for residuals of 2^32 and above).
- `fi_inv_8_1_0`, `fi_inv_8_2_0`, `fi_inv_8_4_0`, `fi_inv_8_8_0`: 64-bit series are always coded signed
  (`kg_int_fwd.inc:60`, `sg = (w == 8)`).
- `kg_int_inv`: the generic inverse, used only when no specialisation applies (interleaved multi-byte residuals,
  big-endian host or `FI_USE` off). The encoder always writes planar residuals when `ow > 1` (`kgraph.inc:741`).
- `tbl2.inc` `col2_fvals`, `ncol_fvals`, `tbl2_columns`: formula columns on the non-aligned decode path, and the
  `tbl2_columns_pub` API. These are not on the brief. Formula seeds reaching `fm_int`/`fm_predict` were made after
  the runs and are in the committed corpus, but were not fuzzed.
- `read_block`, `kolmo_decompress_to_fd`, `kolmo_decompress_file`: the fd/file APIs, which no harness calls (as in
  campaign 1).

Encoder or API wrappers only, so 0% is expected: `*_pub` test wrappers, `kolmo_train_dict`, `kolmo_compress_dict`,
`kolmo_compress_from_fd`, `kolmo_compress_file`, `kolmo_stream_compress`, `cm_compress`, `kgf_scan_8_0`,
`kgf_write_8_0` (unsigned 64-bit, never chosen), 24 of 26 typed-int encoder functions, and the research encoders.

## Reproduce

```
apt-get install -y clang llvm libclang-rt-18-dev liblzma-dev
make -j4 ARCH= build/libvendor.a build/kolmo
fuzz/campaign/build.sh; MODE=cov fuzz/campaign/build.sh
git worktree add /tmp/c1 claude/rc2-fuzz-20261115
C1CORPUS=/tmp/c1/fuzz/campaign/corpus WORK=<work> SEEDS=<seeds> fuzz/campaign2/make_seeds.sh
W=<w> SEEDS=<seeds> SECS=3900 TARGETS="fuzz_decompress_nocm fuzz_limited_nocm fuzz_dict_nocm fuzz_stream_nocm fuzz_stream_limited_nocm" fuzz/campaign2/run.sh
W=<w> SEEDS=<seeds> SECS=3900 TARGETS="fuzz_decompress fuzz_limited fuzz_dict fuzz_roundtrip fuzz_roundtrip_nocm fuzz_nplane fuzz_krc fuzz_kbcx fuzz_fastq" fuzz/campaign2/run.sh
python3 fuzz/campaign/summarize.py <w>/logs/*.log
build/fuzzcov/<t> -merge=1 <final>/<t> <w>/corpus/<t> <seeds>/<t>          # per target
CORPUS=<final> OUT=<dir> fuzz/campaign2/coverage.sh
build/fuzz/fuzz_limited_nocm -runs=0 fuzz/campaign2/corpus/fuzz_limited_nocm fuzz/campaign2/corpus/fuzz_decompress_nocm
```

Logs: `logs/warmup/`, `logs/batchA/`, `logs/batchB/`, and `logs/timeline.txt` (5-minute coverage and memory samples).
