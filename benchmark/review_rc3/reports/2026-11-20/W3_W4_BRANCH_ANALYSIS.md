# W3 / W4 branch analysis (read-only, preservation-first)

Review clone: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/repo`, checkout left at RC3
`5071f388d31e5cae307e28e9ec5a68f3ed8113e5`. Only read-only git commands were used (log, show, diff, merge-base,
grep, ls-tree). Nothing was built or run; every statement below comes from patch content at the pinned SHAs.
"file:line" citations are at the SHA named in front of them (`SHA:path:line`).

Categories: KEEP (preserve as is), TEST (needs verification before any decision), PROPOSE_FOR_INTEGRATION,
HISTORICAL (record/study; never means delete).

## 0. Topology

| branch | pinned SHA | merge-base with 5071f388 | own changes measured as |
|---|---|---|---|
| W4 entropy | 5f24d6ae | 5071f388 (also contains decmap 59f117f1 via merge b61b8059) | `b61b8059..5f24d6ae` |
| W4 lz | 3a027805 | 5071f388 (also contains decmap via merge 2debef2a) | `2debef2a..3a027805` |
| W4 cm | ca81739e | 5071f388 | `5071f388..ca81739e` |
| W4 bwt | 5ce318ad | 5071f388 | `5071f388..5ce318ad` |
| W4 docs | fba28558 | 5071f388 | `5071f388..fba28558` |
| W4 multi | fdb99760 | 5071f388 | `5071f388..fdb99760` |
| W3 decmap | 59f117f1 | 58f7cee8 | `58f7cee8..59f117f1` |
| W3 phrase | cee3b121 | 58f7cee8 | `58f7cee8..cee3b121` |
| W3 prior | ae00b26f | 58f7cee8 | `e42eff70..ae00b26f` (e42eff70 merges an older twopass tip 274ab0e8, which rc3 already contains) |
| local merge | a17d6657 (`refs/review-local/heads/claude/rc3w2-integration`) | 5071f388 | rc3 + decmap (e17142c4) + phrase |

`git merge-base --all 5f24d6ae 3a027805` returns two bases (5071f388 and 59f117f1): both W4 entropy and W4 lz merged
the same decmap commit, so decmap is not a duplicated patch, it is a shared ancestor.

a17d6657 check: the src/tests delta of `5071f388..a17d6657` is identical to the src/tests delta of
`58f7cee8..59f117f1` (compared hunk by hunk); bindings were resolved to rc3's synced copies plus decmap's delta
(10 binding files changed, not the 36 that `5071f388..59f117f1` shows, because 59f117f1 carries pre-rc3 binding copies).

---

## 1. W4 entropy — `claude/w4-entropy-20261118` @ 5f24d6ae

- **Merge-base:** 5071f388; includes decmap (59f117f1) through b61b8059.
- **Own commits:** f3d34922 (khuf v2, codec 30, QLFC decode, four QLFC parts), 183f8845 (one-symbol kernel, thresholds,
  table checks), 5f24d6ae (test-bwt link, results).
- **Files (src/tests, own):** `src/algo/khuf.c` (rewrite, +535/-), `src/algo/khuf.h`, `src/algo/qlfc.c`,
  `src/algo/qlfc_model.h`, new `src/arch/x86_64/khuf_x86_64.S` (BMI2), `src/codecs/{bwt_stream.c,codec.c,codec.h,
  hufzstd_leaf.c,native.c}`, `src/core/{blocks.c,decode.c}`, `src/transforms/graph.c`, `Makefile`, `SPEC.md`,
  `benchmark/determinism/expected.sha256` (18 lines), `tests/test_asm.c`, `tests/test_native.c`, bindings copies.
- **Features:** khuf v2 block format (package-merge, up to 12-bit codes, MSB-first, 3-mode table, 1 or 4 streams);
  multi-symbol decode tables; x86-64 BMI2 assembly loops (CPUID-gated, `5f24d6ae:src/algo/khuf.c:405-415`); new
  codec 30 KHZ (khuf then zstd); **graph leaves now write 27/30 by default instead of 13/22**
  (`5f24d6ae:src/codecs/native.c:46`, `5f24d6ae:src/transforms/graph.c:573`); `KOLMO_NATIVE=libhuf` restores 13/22;
  QLFC decoder SSE2 APM/MTF/run paths (`src/algo/qlfc_model.h`, `src/algo/qlfc.c`, claimed bit-exact); codec-18 blocks
  of 4-16 MiB coded as four QLFC parts (`5f24d6ae:src/codecs/bwt_stream.c:238-239`).

### Format impact (HIGH)
1. **Codec id 27 reused with a changed layout.** `5f24d6ae:src/algo/khuf.h:23-24`: "Wave 4 changed the block format
   ... rc3 codec 27 and 29 frames written before it do not decode." `KHUF_MAXLEN` 11 -> 12 (`khuf.h:30`). New table
   header byte 0 = `L | mode<<4` (`5f24d6ae:src/algo/khuf.c:160-164`) versus rc3's `count-1` byte
   (`5071f388:src/algo/khuf.c:99-114`). No version byte or new id distinguishes old from new.
2. **Codec 29 (KLZH) changes implicitly**: KLZH stores its four streams as khuf chunk streams
   (`5f24d6ae:src/algo/klz.c:289,322`), so every KLZH frame changes layout with khuf v2 even though `klz.c` is not
   edited by this lane.
3. **New codec id 30** (`5f24d6ae:src/codecs/codec.h:8`, `hufzstd_leaf.c:65-66`), graph-leaf-only (rejected as a block
   codec at `5f24d6ae:src/core/decode.c:71`); also permitted inside codecs 21/23 via `codec_is_native`
   (`native.c:54`).
4. **Default output changes** (determinism hashes regenerated): any graph block whose leaf picks Huffman is now 27/30
   and the frame becomes version 11 (`5f24d6ae:src/transforms/graph.c:698`, `codec_is_native -> used_f2`).
5. QLFC four-part split: the part count is self-described in the stream (`5071f388:src/algo/qlfc.c:788-804`), so rc3
   decodes it. No format change there.

**OLD rc3 decoder on NEW frames:**
- codec 30 leaf: `codec_get(30)` is out of range of rc3's table -> NULL -> `codec_decompress` returns -1
  (`5071f388:src/codecs/codec.c:15-17,30-31`); inside a planes leaf, `codec_is_native(30)` is false in rc3
  (`5071f388:src/codecs/planes_leaf.c:35`). **Safe rejection.**
- codec 27 (and 29) leaves/blocks: rc3 parses the new bytes as the old layout. New table bytes 0x01-0x2C all fall in
  rc3's "sparse" branch; rc3 then requires distinct symbols, lengths 1..11, Kraft sum exactly 1
  (`5071f388:src/algo/khuf.c:105`, `kh_codes`) and exact stream-length consumption per stream (end of
  `kolmo_khuf_decompress`). Most inputs are rejected, but **acceptance with wrong output is not excluded by any
  version/id check**. The container's per-block checksum over decoded bytes (`5071f388:src/core/decode.c:87-129`)
  would catch it, **except in `rt` frames** where `psum` makes the checksum cover the stored payload
  (`5071f388:src/core/decode.c:73-74`). Whether `rt` graph leaves can select Huffman: UNKNOWN.
- Classification: mostly safe rejection; theoretical silent mis-decode only in psum (`rt`) frames.

**NEW decoder on OLD rc3 frames:** codecs 13 and 22 are still read (`hz_decompress` kept, `hufzstd_leaf.c:23-33`). rc3
frames written with opt-in `KOLMO_NATIVE=huf`/`lz` (codec 27/29) are **not readable** (stated at `khuf.h:24`); new
reader rejects `mode > 2` or `L` outside 1..12 (`khuf.c:164`), otherwise same theoretical risk as above. Whether any
such rc3 opt-in frames exist outside test runs: UNKNOWN.

- **Thread impact:** 4-16 MiB codec-18 blocks now have four QLFC parts, so `-T4` decodes them in parallel; `-T1`
  unchanged. Assembly is x86-64 only; Apple ARM runs the C loops.
- **Tests:** `tests/test_asm.c` (asm vs C reference for dec4/dec1, random tables), `tests/test_native.c`
  (`test_khuf_garbage`, new sizes around 1024/16384). No test that rc3-written 27/29 frames are rejected or decoded.
- **Claims / evidence:** `5f24d6ae:benchmark/w4/entropy/RESULTS.md:13` khuf decode 1.03-1.76x Silesia (geomean
  1.22x), smaller on 12/14 inputs; `:15` QLFC decode 0.90-1.73x bsc at 1 thread; CHANGELOG: dickens ratio 4T decode
  0.256 -> 0.159 s at +0.05%, reymont 0.196 -> 0.072 s at +0.23%. Results:
  `benchmark/w4/entropy/results/{ebench_T1,hbench_T1}.txt`, `leaf_T{1,4}.jsonl`, `ratio_T{1,4}.jsonl`, `*_tables.md`.
- **Platform:** x86 Linux cloud VM, 4 vCPU Xeon (AVX-512), gcc 13 (`RESULTS.md:25-26`); "Apple M5 numbers come from
  the integration session" (`:9`) — none in branch.
- **Risk:** HIGH (codec-id reuse with incompatible layout; default output change).
- **Category:** TEST (PROPOSE_FOR_INTEGRATION only after the id question is decided: e.g. new ids for khuf-v2 leaves
  and KLZH-v2 instead of reusing 27/29, or an explicit owner decision that rc3 opt-in 27/29 frames are abandoned).

---

## 2. W4 lz — `claude/w4-lz-20261118` @ 3a027805

- **Merge-base:** 5071f388; includes decmap via 2debef2a.
- **Own commits:** 0dc7a8b1 (KLZ new stream format + decoder fast loop), d9e86fa5 (KLZH level-1 parse, repeats,
  `__builtin_memcpy`), 776ac3c3 (bindings, SPEC, changelog, scripts), 75d8a512 / 3a027805 (results).
- **Files (own):** `src/algo/klz.c` (+374/-), `src/algo/klz.h`, `src/algo/khuf.c` (+1: `#define memcpy
  __builtin_memcpy`), `SPEC.md`, `research/CODECS.md`, `CHANGELOG.md`, `benchmark/w4/lz/*`, bindings.
- **Features:** KLZ (28) four-stream layout (literals | tokens | 16-bit offsets | extensions), 4+4-bit tokens,
  budgeted fast decode loop; KLZH (29) encoder parse changes only (level-1 single table, rep offsets); FORTIFY-safe
  memcpy in khuf/klz.

### Format impact (MEDIUM; opt-in only)
- **Codec id 28 reused with an incompatible layout.** `3a027805:src/algo/klz.h:6` "the rc3 wave-2 format ... is not
  read any more"; `3a027805:SPEC.md:201` same. Window 2^24-1 -> 0xFFFF (`klz.h:31`). New decoder
  `3a027805:src/algo/klz.c:372-407` begins with three uv counts; rc3's decoder (`5071f388:src/algo/klz.c`,
  `kolmo_klz_decompress`) parses an interleaved token stream and requires `op == oe && ip == ie` at the end, so
  accidental acceptance is improbable but not excluded by an id/version check.
- **KLZH (29) wire format is unchanged by this lane:** `kolmo_klzh_decompress` is byte-identical between 2debef2a and
  3a027805 (diffed). (It still changes when combined with W4 entropy, see section 1.)
- **OLD rc3 decoder on NEW KLZ frames:** almost certainly rejects; silent mis-decode is only possible in `rt` frames
  (psum checksum over stored bytes), and **KLZ is exactly the `rt`/`speed` replacement** (`codec_native_id`: LZ4 ->
  KLZ). Opt-in `KOLMO_NATIVE=lz` only.
- **NEW decoder on OLD rc3 KLZ frames:** not readable (by design).
- Claim conflict: `3a027805:benchmark/w4/lz/RESULTS.md:285` says switching `speed` to codec 28 "needs no format
  work: codec 28 is already a version-11 codec id" — true for the id, but it ignores that rc3 readers carry the old
  KLZ layout under the same id.
- **Thread impact:** none.
- **Tests:** no new test files; RESULTS.md table cites `make check-native` round trips, corrupt/truncated payloads
  under ASan/UBSan, `KOLMO_NATIVE=all` at 1 and 4 threads (claim only; not re-run here).
- **Claims / evidence:** `research/CODECS.md` rows: KLZ vs LZ4 -1 −5.87% size, decode 1.05x/1.06x, compress
  0.98x/0.95x; KLZH1 vs zstd -1 −1.02%, decode 1.06x (1T)/0.94x (4T). Evidence:
  `benchmark/w4/lz/RESULTS.md`, `benchmark/w4/lz/results/{speed,balanced,ratio}.tsv`. "No default output changed
  (determinism: same hashes)" (`RESULTS.md:5`); `benchmark/determinism/expected.sha256` is not touched.
- **Platform:** cloud VM, Intel Xeon 2.10 GHz, 4 vCPUs, Linux 6.18, GCC 13.3 (`RESULTS.md:7`). No ARM numbers.
- **Risk:** MEDIUM (opt-in id reuse; KLZH numbers become stale once khuf v2 lands).
- **Category:** TEST (keep opt-in; re-measure KLZH after W4 entropy; decide id policy for 28).

---

## 3. W4 cm — `claude/w4-cm-20261118` @ ca81739e

- **Merge-base:** 5071f388. Commits 9c883acb (research model 5), b38ba316 (codec 7 model 4 for -x), 7a90996b
  (determinism digests), ca81739e (results).
- **Files:** `src/codecs/cm.c` (+458/-), `cm_par.c`, `cm_prior.inc` (`cm_prior4`), `src/algo/mixer.{c,h}`,
  `src/arch/{cpu.c,kernels.h,x86_64/kernels_x86.h,x86_64/mixer_avx2.c}`, `src/core/blocks.c`,
  `src/plan/parallel.c`, `docs/{MEMORY-MODEL,TWOPASS}.md`, `SPEC.md`, `tests/test_cm4.c` (new),
  `tests/test_kernels.c`, determinism.
- **Features:** CM model 4: subset of model 3's contexts selected by a u32 mask, 64-byte lines of four 16-byte
  buckets, 16-bit mixer, tables sized by stream length, prior `cm_prior4` (text/binary); `-x` writes model 4 by default
  (not raced); planner CM sample 1 MiB -> 256 KiB (`src/plan/parallel.c`).

### Format impact (HIGH for -x default output; backward-safe)
- New CM stream header: model nibble 9 + 3 tune bytes + context mask u32 (`ca81739e:SPEC.md:120-135,263`); decode
  limits `in[7] > 2` and mask subset checks (`ca81739e:src/codecs/cm.c:1137,1173`); CM model state/table layout is new.
  Prior tables `cm_prior4` and the mask constants are now part of the format (changing them breaks frames).
- Frame memory metadata: `cm_stream_need` knows nibble 9 (`ca81739e:src/codecs/cm.c:1171-1175`), `cm_need_bound`
  includes model 4 (`:1187-1191`), version 11 via `cm_stream_tuned` (`:1125`).
- **OLD rc3 decoder on NEW frames:** nibble 9 is not in rc3's accepted set
  (`5071f388:src/codecs/cm.c:921-922` returns -1; `cm_stream_need` returns 0 at `:950-951`). **Safe rejection**
  (SPEC states `KOLMO_ERR_CORRUPT`, `ca81739e:SPEC.md:135`).
- **NEW decoder on OLD rc3 frames:** nibbles 3/5/6/7/14/15 still accepted (`ca81739e:src/codecs/cm.c:1144-1147`). Reads
  rc3 -x frames.
- **Thread impact:** model 4 is not raced (fewer race threads at -x); codec 25 parts still one thread each
  (`cm_par.c`).
- **Tests:** `tests/test_cm4.c` (new, 91 lines), `tests/test_kernels.c` (16-bit mixer kernels).
- **Claims:** `ca81739e:benchmark/w4/cm/RESULTS.md:12` vs zpaq -m5: Silesia −7.1%, structured −55.5%, numeric −21.8%,
  text −4.7% (target −5% missed); `:13` speed 1.00-3.9x zpaq; `:15` memory 0.48-1.58x zpaq. Evidence:
  `benchmark/w4/cm/RESULTS.md`, `benchmark/w4/cm/summarize.py`.
- **Platform:** cloud VM, 4 vCPUs Intel Xeon 2.1 GHz (`RESULTS.md:3`); "Apple M5 numbers are left to the integration
  session" (`:5`). The `:13` baseline "was 0.73x / 0.83x on text, M5" mixes an M5 baseline with VM results.
- **Risk:** MEDIUM-HIGH (new default -x format; collides internally with W3 prior's "model 4").
- **Category:** PROPOSE_FOR_INTEGRATION after TEST (ARM bit-exactness of the 16-bit mixer, determinism across CPUs).

---

## 4. W4 bwt — `claude/w4-bwt-20261118` @ 5ce318ad

- **Merge-base:** 5071f388. Commits 598a80d2 (code), 5ce318ad (results).
- **Files:** new `src/algo/bigmem.h` (mmap + `MADV_HUGEPAGE`, Linux), `src/algo/lzp.c` (unrolled encoder +
  `lzp_encode_block_ref`), `src/codecs/bwt_stream.c` (in-place encode/decode), `src/core/blocks.c` (skip discarded
  `word_encode`), `tests/test_bwt.c`, `CHANGELOG.md`, `research/CODECS.md`, `benchmark/w4/bwt/*`.
- **Format impact:** none claimed (`5ce318ad:benchmark/w4/bwt/RESULTS.md:7` "Every frame is byte-identical to rc3");
  determinism file untouched. The header is now written after QLFC into the same bytes (code inspected:
  `kolmo_bwt_compress` builds the identical header fields). Decoder now QLFC-decodes directly into `out` and inverts
  in place; rc3 frames decode with the same parser. Frame memory metadata for codec 18 is unchanged
  (`blk_dec_need`: `base + 5*slack`), now an over-estimate of the 4n working set (conservative, not a hazard).
- **Thread impact:** none new; huge pages Linux-only (`KOLMO_NO_HUGEPAGE=1` disables).
- **Tests:** `tests/test_bwt.c` `lzp_fast_vs_ref` (fast vs reference LZP, in-place vs copy codec-18 encoder).
- **Claims:** `RESULTS.md:15` inverse BWT +33% (1T) / +40% (4T) enwik8; `:25` enwik8 compress RSS 664 -> 600 MiB;
  CHANGELOG: enwik9 decompress 788 -> 666 MiB, LZP 253 -> 357+ MB/s. Evidence: `benchmark/w4/bwt/rows/t{1,4}.jsonl`.
- **Platform:** VM Xeon 2.1 GHz, 4 vCPU, gcc 13, THP `madvise` (`RESULTS.md:44`); M5 left to integration (`:219`).
  Huge-page gains do not apply on macOS (malloc path).
- **Risk:** LOW-MEDIUM (in-place buffer arithmetic; QLFC now decodes into `out` without the previous `n+64` slack
  buffer when LZP is off — W4 entropy's SSE run store is guarded by `n - *i >= 16`, so compatible, but worth an ASan
  run on the combination).
- **Category:** PROPOSE_FOR_INTEGRATION (after ASan/determinism on the merged tree).

---

## 5. W4 docs — `claude/w4-docs-20261118` @ fba28558

- **Merge-base:** 5071f388. Commits 47f04156, 1bc1b59a, fba28558.
- **Files:** `src/codecs/zstd.c` (8 MiB window "zwide"; hc text level 19 + windowLog 27), `src/codecs/codec.h`
  (`zwide` field), `src/core/blocks.c` (eco text -> codec 18; hc text level id 21), `src/core/compress.c` (6 MiB
  balanced blocks, segment merging, eco 32 MiB text blocks), `src/core/decode.c` (writer thread),
  `src/plan/detect.c` (`looks_text8`), `src/plan/tunables.h` (`CZ_HC_TEXT_LEVEL` 22 -> 21),
  `src/transforms/table.c` (prefetch), determinism (eco hash), results.
- **Format impact:** no new ids or fields; encoder choices change (eco text now codec 18, a version-11 codec rc3
  reads; zstd windows ≤ 2^27, within zstd's default decoder limit). rc3 decodes these frames; new decoder reads rc3
  frames. Memory metadata: `blk_dec_need` for C_ZSTD is `base + 2 MiB` (`fba28558:src/core/memneed.c:57`); zstd is
  decoded one-shot into the block buffer, so the larger window does not add a window buffer (inference from code;
  not measured here). eco 32 MiB codec-18 blocks raise per-block decode need (`5*slack`).
- **Thread impact (CONFIRMED):** decode with one worker starts a second OS thread for writes:
  `fba28558:src/core/decode.c:391` `J.async_io = nt == 1;`, used at `:271-273` (`awriter_start` when writing to a file,
  more than one block, no range clip), thread created at `:242-246` (`pthread_create(... awriter_main ...)`), worker
  loop `:217-235`. So `-T1` decode is no longer single-threaded; it also adds one block buffer (`rb2`, `:272`).
  Claimed `RESULTS.md:35-37`: "enwik8 balanced decode 0.40 -> 0.22 s; +1 block of decode memory". This conflicts with
  the project's one-thread fair-benchmark rule and must be disclosed or gated.
- **Tests:** none added (only determinism hash for `-m eco`).
- **Claims:** `RESULTS.md` summary table: balanced vs zstd -3 text −0.3%, source −3.7%; eco vs brotli -11 text
  −14.6% at 24-32x its compress speed but decode "block-sorting speed", 10x slower than brotli (`:216`); hc text
  memory 5.02x -> 2.05x. Evidence: `benchmark/w4/docs/results/*.jsonl`.
- **Platform:** cloud VM Xeon 2.10 GHz, 4 vCPU (`RESULTS.md:3-4`); M5 by integration session.
- **Risk:** MEDIUM (hidden extra thread at -T1; eco decode speed regression is a product decision).
- **Category:** TEST (gate the writer thread behind an explicit option or `-T>=2` before using any -T1 numbers).

---

## 6. W4 multi — `claude/w4-multi-20261118` @ fdb99760

- **Merge-base:** 5071f388. Commits 2cba7356 (machine-wide pool), 299c1049, fdb99760 (results).
- **Files:** new `src/core/jobs.c` (368 lines: flock'd token slots, per-process records), `src/core/pool.c`
  (caller role, fork handler, private helpers), `src/algo/par.{c,h}` (`kolmo_par_take/give` hooks),
  `src/cli/cli.c` (`--jobs-shared`, status), `src/codecs/cm.c`, `cm_par.c`, `src/core/{blocks,compress,decode,memory,
  trials}.c`, `src/plan/parallel.c`, `src/kolmo.h` (+27), `src/kolmo.c`, docs, `tests/test_jobs.c`,
  `tests/stress_jobs.py`, bindings (Rust `lib.rs`, Python test).
- **Format impact:** none (claims "never changes output", `fdb99760:benchmark/w4/multi/RESULTS.md:9`; tokens only
  change how many threads run). **API/ABI change:** `kolmo_params` gains `int jobs_shared`
  (`fdb99760:src/kolmo.h:37`) — callers compiled against the old header pass a smaller struct (ABI break for
  shared-library users); new public functions `kolmo_jobs_shared`, `kolmo_jobs_proc`/status struct.
- **Thread impact:** default ON per user (`docs/CONCURRENCY.md:12,138`): helpers take a token per block job
  (`fdb99760:src/core/blocks.c:149`, `src/core/decode.c:230`), the caller never waits (`src/core/pool.c:87`); CM race
  threads and codec-25 parts take tokens (`cm.c` race loop, `cm_par.c`). Concurrent Kolmo processes (including
  parallel benchmark harnesses such as prior's `sz.sh` "run four at a time") now throttle each other. Writes files
  under `$XDG_RUNTIME_DIR/kolmo-jobs` or `$TMPDIR/kolmo-jobs-<uid>` (`src/core/jobs.c` header comment).
- **Tests:** `tests/test_jobs.c` (214 lines), `tests/stress_jobs.py`, bindings test.
- **Claims:** `RESULTS.md` tables: ratio N=4..16 wall 0.81-0.82x with pool; balanced N=16 peak 1005 -> 522 MiB; -x N=2
  wall 1.25x slower. Evidence: `benchmark/w4/multi/results.json`, `results-first-run.json`.
- **Platform:** Linux VM, 4 cores no SMT (`RESULTS.md:5`); macOS path "same code but were" not measured (`:78`).
- **Risk:** MEDIUM-HIGH (default-on cross-process behaviour; ABI; touches every worker loop).
- **Category:** TEST (consider default OFF until measured on macOS/M5).

---

## 7. W3 decmap — `claude/rc3w3-decmap-20261118` @ 59f117f1

- **Merge-base:** 58f7cee8. Single commit.
- **Files (src):** `src/algo/khuf.c` (constant bit-reversal table, one 11-bit table when n >= 4096),
  `src/algo/klz.c` (KLZH: one validation pass, refills on demand), new `src/arch/x86_64/word_avx2.c` (AVX2 word
  decoder, CPUID-gated), `src/kolmo.c`, `src/transforms/word.c`, `tests/test_arch.c`; research/CHANGELOG/results;
  bindings (re-synced from a pre-rc3 state).
- **Format impact:** none; decoder-only. Checked: KLZH range check `ml >= 72 || mo > 26` is equivalent to rc3's two
  checks (`59f117f1:src/algo/klz.c:328-330`); refill-on-demand is safe because after `kz_refill` at least 56 bits are
  buffered and the largest extra-bit counts are 30 (LN[71]) and 23 (offsets), with refills guarded by
  `br.c < LN[b]` / `br.c < h` (`:335-338`). khuf single table relies on replicated entries (`:172-175`). rc3 frames
  decode identically by construction (claim in `RESULTS.md`, not re-run).
- **Thread impact:** none.
- **Tests:** `tests/test_arch.c` (AVX2 vs C word decoder through `kolmo_force_c`).
- **Claims:** `59f117f1:benchmark/rc3w3/decmap/RESULTS.md:15` text eco/hc decode +14.8% (1T)/+12.1% (4T);
  `:19` `KOLMO_NATIVE=all` frames +16.3%/+14.2%. Evidence: `results/ab_T1.jsonl`, `ab_T4.jsonl`, `table.md`,
  `natbench_klzh.txt`, `callgrind_top.txt`.
- **Platform:** cloud VM 4 vCPU x86-64 (`RESULTS.md:25`). The AVX2 kernel is x86-only; on Apple ARM the NEON kernel
  already existed, so the +14.8% does not apply to the M5.
- **Superseded parts:** W4 entropy rewrites `kolmo_khuf_decompress`; only decmap's `KH_REV8` survives
  (`5f24d6ae:src/algo/khuf.c:95-105`); the "one table" change is replaced.
- **Risk:** LOW. **Category:** KEEP / PROPOSE_FOR_INTEGRATION (already in a17d6657).

## 8. W3 phrase — `claude/rc3w3-phrase-20261118` @ cee3b121

- **Merge-base:** 58f7cee8. Commits 87ced628, cee3b121.
- **Files:** only `benchmark/rc3w3/phrase/**` (tools, `phr.c`, results) and `research/CODECS.md` (+5); no `src/`,
  no bindings, no determinism change.
- **Format / thread impact:** none.
- **Claims:** `cee3b121:benchmark/rc3w3/phrase/RESULTS.md:5` "No variant gains 0.5% on any group against -x"; best
  JSON −0.38%. Evidence: `results/full/*.jsonl`, `results/s1m/*.jsonl`, `t1.jsonl`, `t4.jsonl`.
- **Platform:** Intel Xeon 2.10 GHz, 4 vCPUs, Linux VM (`RESULTS.md:56`).
- **Risk:** NONE. **Category:** HISTORICAL (measured, not adopted; already in a17d6657).

## 9. W3 prior — `claude/rc3w3-prior-20261118` @ ae00b26f

- **Merge-base:** 58f7cee8; merges twopass at 274ab0e8 (e42eff70). rc3 5071f388 contains 274ab0e8 **and later twopass
  commits** (a45247c2, 6f8761b7, 0ae4fdfe, ...) that this branch does not have.
- **Own commits:** cf4caf80 (CM decode working set for nibbles 14/15), b9e6adf3 (per-kind priors, model 4 token
  classes), 1afee825 (docs/results), ae00b26f (race threads only with memory granted).
- **Files:** `src/codecs/cm.c` (+293/-), new `src/codecs/cm_kprior.inc` (12,142 lines, LZMA2-packed prior tables,
  "388 KB" per CHANGELOG), new `src/algo/texttype.c`, `src/kolmo.c`, `Makefile`, `SPEC.md`, `docs/{PRIORS,LAYOUT}.md`,
  `tests/test_priors.c`, determinism (30 lines), large `benchmark/rc3w3/prior/raw/*`.
- **Format impact (HIGH, backward-safe):** model 4 signalled by nibble 15 with bit 7 of the prior byte
  (`ae00b26f:src/codecs/cm.c:1066,1093`); prior id 2..9 in tune byte 2 (`:392`). The 388 KB compiled-in prior tables
  become format (a built-in dictionary requirement: changing them breaks frames). rc3 rejects these streams because it
  refuses `in[7] > 1` (`5071f388:src/codecs/cm.c:925`, `:952`) — **safe rejection**. New decoder still reads rc3
  nibble 14/15 streams.
- **Memory metadata:** `cm_stream_need` maps bit 7 to v2=4 (`ae00b26f:src/codecs/cm.c:1093`) but `cm_need_bound` for
  graph leaves still uses model 3 (`:1100`); model 4 here is blocks-only per SPEC, so a mismatch only matters if leaves
  ever use it (UNKNOWN).
- **Duplicate fixes:**
  - cf4caf80 duplicates what rc3 already has (5071f388 `cm_stream_need` accepts nibbles 14/15,
    `5071f388:src/codecs/cm.c:950-952`).
  - ae00b26f ("race trial threads start only when their memory is granted") is an independent re-implementation of
    rc3's 6f8761b7 ("-x preset race reserves its extra threads ... passes the memory context"). Both add
    `kmem_try_reserve`/`kmem_ctx` and rewrite `cm_compress_raced`: certain textual conflict, keep rc3's version.
- **Thread impact:** model 3/4 race adds trial threads (memory-gated).
- **Tests:** `tests/test_priors.c`.
- **Claims:** CHANGELOG (`ae00b26f:CHANGELOG.md:14`) 10 KB −5.8% to −14.7%, 1 MB −1.3% to −4.5%, decode 1.0-1.1x.
  Evidence: `benchmark/rc3w3/prior/RESULTS.md`, `raw/sizes.txt`, `raw/timing*.jsonl`, `raw/model4_1m.txt`.
- **Platform:** cloud VM Intel Xeon 4 vCPU, Linux 6.18, gcc 13 (`RESULTS.md:11`).
- **Risk:** HIGH for integration (stale base, duplicate fixes, internal "model 4" collision with W4 cm).
- **Category:** HISTORICAL + TEST (re-port onto W4 cm as a distinct model number if adopted).

---

## 10. Overlap matrix (own changes; bindings and CHANGELOG/CODECS excluded unless noted)

| | entropy | lz | cm | bwt | docs | multi | decmap | phrase | prior |
|---|---|---|---|---|---|---|---|---|---|
| **entropy** | — | khuf.c, SPEC.md | blocks.c, SPEC, Makefile, determinism | **bwt_stream.c**, blocks.c | codec.h, blocks.c, decode.c, determinism | blocks.c, decode.c, Makefile | khuf.c (supersedes) | — | SPEC, Makefile, determinism |
| **lz** | | — | SPEC | — | — | — | khuf.c, klz.c (built on it) | — | SPEC |
| **cm** | | | — | blocks.c | blocks.c, determinism | **cm.c, cm_par.c**, blocks.c, parallel.c, MEMORY-MODEL.md, Makefile | — | — | **cm.c**, SPEC, Makefile, determinism |
| **bwt** | | | | — | blocks.c | blocks.c | — | — | — |
| **docs** | | | | | — | **decode.c**, blocks.c, compress.c | — | — | determinism |
| **multi** | | | | | | — | kolmo.c | — | **cm.c**, kolmo.c, LAYOUT.md, Makefile |
| **decmap** | | | | | | | — | — | kolmo.c |
| **phrase** | | | | | | | | — | CODECS.md only |

Every pair also touches `CHANGELOG.md` (top-of-file insertions: trivial conflicts) and most touch `research/CODECS.md`.

### Predicted textual conflicts (reasoned from diffs; merge-tree was not run)
1. **entropy x bwt — `src/codecs/bwt_stream.c` `kolmo_bwt_compress`:** entropy inserts `mb` (`5f24d6ae:...:238-239`) and
   edits the three `kolmo_qlfc_compress_min(..., p->min_block, ...)` calls; bwt restructures the same lines (in-place
   `qbuf`, two calls). Certain conflict; resolution: compute `mb` before bwt's QLFC call and pass it to both of bwt's
   calls. Semantic check: QLFC multi-part output still fits `asz >= lz + 64`.
2. **entropy x lz — `src/algo/khuf.c` top:** lz inserts `#define memcpy(...) __builtin_memcpy(...)` between the
   includes; entropy's hunk `@@ -1,7 +1,8` rewrites the comment with those includes as context. Likely conflict;
   keep both. Semantic: entropy's khuf.c lacks the FORTIFY fix; lz's KLZH measurements are against khuf v1.
3. **entropy x lz — `SPEC.md` codec table rows 27/28/29/30:** adjacent-line edits; likely conflict.
4. **docs x multi — `src/core/decode.c`:** both change the `djob_t` struct line (`int async_io` vs `int kj`), both edit
   the `decompress_worker` loop, both add a line after `J.fqthreads` in `run_decode_p`. Certain conflict. Semantic: the
   writer thread is not counted by the machine pool.
5. **cm x multi — `src/codecs/cm.c` race (`cm_race_job`, `cm_compress_raced`) and `cm_par.c`:** cm adds `mask` to
   `cm_race_job` and changes the part model; multi adds token take/give in the same loop. Likely conflict; tokens must
   wrap model-4 trials.
6. **cm x prior — `src/codecs/cm.c`:** both define an internal model `v2 == 4` with different meanings (W4 cm: masked
   model 3 with 16-byte buckets, nibble 9; prior: model 3 + 2 token contexts, nibble 15 + bit 7). Wire signals differ,
   but `CM_NMOD`, `cm_apply_prior`, `cm_set_tune`, `cm_stream_need`, and the race are edited by both; each decoder
   rejects the other's streams (W4 cm refuses `in[7] > 1` for nibble 15, `ca81739e:src/codecs/cm.c:1147`). Heavy
   conflict; requires renumbering one model.
7. **prior x rc3 itself — `cm_compress_raced`:** duplicate deadlock fix (ae00b26f vs 6f8761b7). Certain conflict.
8. **`benchmark/determinism/expected.sha256`:** changed by entropy, cm, docs, prior. Do not hand-merge; regenerate on
   the merged tree after review.
9. **`src/core/blocks.c`:** entropy (~554 newmodes), docs (~327-338, ~411), bwt (~222 word skip), multi (~149, ~567),
   cm (~301). Mostly different hunks; textual conflicts unlikely, but docs' new eco->BWT path discards the word
   transform for eco while bwt's skip condition covers only CM/LZMA: missed optimisation, not a bug.

### Duplicate versions of the same fix
- rc3 6f8761b7 vs prior ae00b26f (race threads and memory reservation).
- rc3 `cm_stream_need` nibbles 14/15 vs prior cf4caf80.
- decmap's khuf table build vs W4 entropy's rewrite (entropy supersedes everything but `KH_REV8`).
- decmap merged into both W4 entropy and W4 lz as the same commit (not a duplicate patch).

## 11. Recommended safe merge order (proposal only; nothing was merged)

Start from **a17d6657** (rc3 + decmap + phrase), which both W4 entropy and W4 lz already contain as ancestors.

1. **W4 bwt** (no format change; small conflict surface).
2. **W4 docs**, with the `-T1` writer thread gated (option or `nt >= 2`) and its -T1 numbers re-taken.
3. **W4 cm** (new -x model; rc3 rejects safely). Regenerate determinism.
4. **W4 entropy**, only after an owner decision on codec ids 27/29 (new ids recommended so old opt-in frames remain
   distinguishable); resolve `bwt_stream.c` with step 1. Regenerate determinism.
5. **W4 lz** (opt-in), after entropy; resolve khuf.c/SPEC rows; decide id policy for KLZ 28; re-measure KLZH on
   khuf v2.
6. **W4 multi** last (touches pool, all worker loops, cm race, decode.c from step 2); consider default OFF and fix the
   `kolmo_params` ABI note.
7. **W3 prior**: do not merge directly (stale base, duplicate fixes, model-4 collision). Preserve as HISTORICAL; port
   per-kind priors onto W4 cm's model under a distinct model id if wanted.

## 12. Measurement platform summary

All results files in these nine branches were measured on **x86-64 Linux cloud VMs** (Intel Xeon ~2.1 GHz, 4 vCPU,
gcc 13, `-march=native`). None contain Apple ARM (M5) measurements; several defer M5 numbers to an "integration
session". x86-only kernels (decmap AVX2 word decoder, entropy BMI2 khuf loops) mean their decode gains do not transfer
to the M5 as stated.

## 13. UNKNOWN

- Whether `rt` (psum) frames can contain Huffman graph leaves (affects the theoretical silent mis-decode of codec 27).
- Whether rc3 frames with opt-in codecs 27/28/29 exist outside test runs.
- ARM/M5 bit-exactness and speed of W4 cm's 16-bit mixer and W4 entropy's C decode loops (not measured in branch).
- Whether zstd 8 MiB/128 MiB windows need more decoder memory than `blk_dec_need` declares in streaming paths (one-shot
  path inferred safe; not measured).
- Exact git conflict hunks (merge-tree was intentionally not run).
