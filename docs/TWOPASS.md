# Reading the whole file first: what pays

> **Since 0.0.1 (container version 12)** Kolmo has no library coders. This document was written before that, and its
> coder names map as follows in 0.0.x (`src/codecs/native.c`): LZ4 → KLZ level 1 and LZ4-HC → KLZ level 3 (28); zstd
> level 1-2 / 3-5 / 6-9 / 10-15 / 16-18 / 19+ → KLZH2 (32) level 1 / 3 / 5 / 6 / 8 / 9; Brotli → KLZH2 level 7; LZMA2 →
> KLZMA (24); zstd's Huffman (HUF) → khuf (27); XXH3/XXH64 → `kolmo_khash64`. Dictionaries
> and trained profiles are removed. Figures below were measured with the library coders.

Status: rc3 wave 2, twopass lane, 17 November 2026. Measurements and commands: `benchmark/rc3w2/twopass/RESULTS.md`
(cloud VM, x86-64, 4 vCPU). The question: is it worth reading the whole file first, to build an encoding specific to
that file (codec and transform choice, the context-mixing networks' settings and weights, static models), and at which
level and file size?

## Answer in one table

| technique | what it costs | what it gains | decode | verdict |
|---|---|---|---|---|
| 1a. full-file transform trials instead of today's slices | 0.6–7.7 s per 100 MB of planning (vs ≤ 0.09 s) | **nothing**: the same plan in all 101 file × level cases measured | same | not worth it; slices stay |
| 1b. a plan per homogeneous segment (whole-file scan, 8 KiB probe per 64 KiB) | ≤ 0.02 s per 100 MB | **4.2–13.3% on files that mix kinds of data** (balanced, hc, ratio); 0 on homogeneous files (identical frames) | per part: hc and ratio encode faster, ratio decodes slower | **implemented** (`src/plan/segment.c`) |
| 1c. `-x`: the plain-bytes CM trial on numeric graph blocks gated by a CM slice instead of a BWT slice | one 256 KiB CM slice | **33.6% on 10 MB of doubles** (the graph was larger than `ratio`'s output) | CM speed where CM now wins (0.4 MB/s instead of 170 MB/s), as for every other `-x` block | **implemented** (`src/core/blocks.c`) |
| 2a. full search of the CM learning settings per file (49 trials) | 49× the CM time | 0.04–1.3% without a prior; 0.6–5.6% with it | same | too slow; 2b gets nearly all of it |
| 2b. a race of 4 presets on a 256 KiB prefix (streams up to 16 MiB) | 4 CM codings of 256 KiB (in parallel when threads are free): `-x` 2–4× slower to encode 100 KB–1 MB at 1 thread, 1.1–1.4× at 4; 0–12% at 10 MB | 0.1–2.2% at 1 MB, 1.6–5.4% at 100 KB, 0.1–0.8% at 10 MB (with 3a) | same | **implemented** for `-x`; above 16 MiB a prefix mispredicted (+1.2% on 100 MB), so long streams take the prior alone |
| 2c. table size per file | — | < 0.05% | — | not worth it |
| 3a. a shared prior: mixer weights pre-trained once per model, compiled in | 2 × 1.3 KB of constants | 1.1–4% at 100 KB, 0.13–1.1% at 1 MB, never worse | same | **implemented** (prior id in the stream) |
| 3b. two-pass: pass 1 trains, the mean rows go in the header | a second CM pass; 315–630 header bytes | −0.7% to +4.9% (header cost wins at 100 KB; logs and tables lose) | same | not worth it; 3a is better and free |
| 4. semi-static order-1/2 models with rANS | tables in the header | loses to zstd -3 on every kind (text +25–40%, binary +38%) | 10× slower than zstd | not worth it |
| 5. encoder-only search | — | 1b, 1c, 2b, 3a all leave the decoder's work unchanged | — | see each row |

## 1. Codec and transform choice

**Sampling already finds the right plan.** `benchmark/rc3w2/twopass/planoracle.c` codes every file with every plan
that any level's analysis picks, with the analyses re-run with every trial slice set to the whole file, and with no
transform, each through the target level's real coders on the whole file. Over text, logs, tables, binary, doubles,
an image and a mixed file at 100 KB to 100 MB and all five levels (101 cases), the full-file analysis chose exactly
the plan the sampled one chose, every time; it cost up to 7.7 s per 100 MB against at most 0.09 s.

The oracle does find bigger gains, but they are the levels' deliberate trade-offs, not sampling errors:

- `speed` (minimal analysis) on images and doubles: 32–53% smaller with balanced's plan, at 4× the decode time
  (still about 250 MB/s); on text, 12–16% smaller with the word transform, at 3–4× the encode time.
- `balanced` on text: 1–4.8% smaller with the word transform (hc's plan), 3.5× slower to encode.

These are level choices; nothing here says sampling misses them.

**Where it does pay: heterogeneous files.** A file that holds different kinds of data gets one plan, chosen on one
slice. Coding each part with its own plan was 10–13% smaller at `balanced`, 6–8% at `hc` and 5–8% at `ratio` on a file
of prose, an executable, a CSV export and float64 values (and faster, since each part's analysis is simpler). `-x`
gains nothing there: context mixing adapts on its own and splitting costs it context (−0.2%). Implemented as content
segments: the whole file is classified window by window (text, a delimited table with its delimiter and field count,
or binary), runs under 1 MiB are absorbed, and each run becomes a segment with its own analysis, exactly as tar members
already do. No format change. A 32-probe check returns at once for files of one kind, so they pay nothing and their
frames are unchanged. End to end: mixed 10 MB −10.0% / −5.4% / −4.2%, mixed 100 MB −13.3% / −7.1% / −12.7% (balanced /
hc / ratio). A numeric-array class (byte planes more predictable than bytes) was tried and removed: on 8 KiB probes it
flickered on homogeneous doubles and split them (+1.9% at ratio on 100 MB).

**`-x` had one sampled decision that was badly wrong.** On a numeric graph block, `-x` tried context mixing over the
plain bytes only when a block-sorting slice came within 2% of the graph. On 10 MB of float64 sensor values block
sorting never does, but plain context mixing is 33% smaller than the graph (and the graph at `-x` was larger than
`ratio`'s output). The probe is now a 256 KiB context-mixing slice (within 10% of the graph's rate): the decision is
still sampled, but by the coder in question.

## 2. Per-file tuning of the context-mixing networks

Codec 7's learning constants became per-stream settings (two header bytes, 2 bits per field): mixer learning rate,
its floor and decay period, counter adaptation limit, APM rate, match-model rate, state-map count limit, and the final
weights of mixer and APMs. Defaults are the old constants; a stream with other settings is marked by model nibble 14
or 15 (SPEC.md).

- **Full search** (coordinate descent, 49 codings of the whole file): 0.04–1.3% at 1 MB and 100 KB without the prior,
  0.6–5.6% with it. Table size (one bucket per byte today): ±1 or 2 bits changes the size by under 0.05%.
- **The race** (the 6-bit study's idea): four presets that the full search kept finding, raced on the first 256 KiB.
  It reaches the full search's result within 0.0–0.2 points at 1 MB (text 0.607% vs 0.606%, logs 1.38% vs 1.58%) at
  4 × 256 KiB of coding instead of 49 × the file. A 128 KiB prefix chose worse on 2 of 6 files. A prefix cannot see
  how the rates do over tens of MB: on 100 MB of doubles the prefix's winner was 1.2% larger than the default rates, so
  streams over 16 MiB are not raced and take the prior with the default rates.

## 3. Weight initialisation

- **Two-pass means** (pass 1 trains; the mean trained row of each mixer is stored, 8 or 16 bits per weight, and every
  row starts from it): saves up to 1.1% of the coded bits, but the 315–630 header bytes eat that at 100 KB, and on logs
  and tables the means are a worse start than the uniform weights (+0.9–4.9%).
- **Shared prior** (warm start from a fixed pre-trained model, shipped like a dictionary): the same mean rows,
  trained once on 1 MB samples of other text (model 3) and other binaries (model 2), compiled in
  (`src/codecs/cm_prior.inc`). A general prior beat per-kind priors (a log-trained prior gained 0–0.8% on logs, the
  text-trained one 2.7%). It is never worse on the test files and costs nothing at encode or decode time.

## 4. Semi-static models

Whole-file counts per context (order 0, order 1, hashed order 2 with 4096 contexts), stored as frequency tables, and
static rANS: on raw bytes of every kind they are larger than zstd -3 (text 442 KB vs 349 KB at 1 MB; binary 809 KB vs
652 KB) and decode at 50–100 MB/s against zstd's 700–2000 MB/s. Byte contexts cannot replace matches; Kolmo's graph
leaves already use static Huffman and rANS planes where the transform has removed the structure. Not a middle tier.

## 5. What keeps decoding unchanged

The race and the prior change initial values and rates, not the work per bit: decode time and memory are unchanged
within noise. Content segments change only which plan each part gets; decode speed then follows that part's plan
(on the mixed file: hc decodes faster, ratio slower). The `-x` probe lets plain context mixing win where it is smaller,
and such a block decodes at CM speed, as every other `-x` block does. Encode time grows only where the encoder
searches: the race at `-x`.

## Recommendation, by level and size

| level | 100 KB | 1 MB | 10 MB | 100 MB |
|---|---|---|---|---|
| speed | nothing (its minimal analysis is the point) | same | same | same |
| balanced, hc, ratio | nothing | content segments (mixed files) | content segments | content segments |
| `-x` | prior + race on the whole stream | prior + race on 256 KiB | same | prior only (no race above 16 MiB) |

End-to-end numbers: `benchmark/rc3w2/twopass/RESULTS.md`.

## Model 4 (w4)

Since w4, `-x` writes codec 7's model 4 (`benchmark/w4/cm/RESULTS.md`), which is not raced: on 1 MB samples the race
gained it 0.15% on text and 0.05% on binary data, for twice the encoding time of a 1 MB stream at one thread (the four
presets run one after another). Model 4 always takes the race's fourth preset (`b0 b1` = `dc b4`), which came within
0.02% of the race on the same samples, and its own shared priors (`cm_prior4`, trained the same way on model 4: tune byte
2 = 1 for the text contexts, 2 for the binary ones; 0.11–0.15% smaller than model 3's prior mapped onto model 4's
inputs). A coordinate search of the eight settings on the training samples found 0.04% more and was not kept. The
planner's CM samples (`ultra_keep_transform`) also code with model 4, on 256 KiB instead of 1 MiB.

