# ALTERNATIVES_FOR_REVIEW — decisions taken overnight that the owner should validate (2026-11-21 morning)

Each entry gives the choice made, the alternatives not taken, and the evidence. Nothing here is merged to main.

## A1. W4 cm (`-x` CM model 4): the format id (step 3c; branch `review/c2-cm-J62kQD`, 2026-11-20)

**Chosen:** keep W4 cm's wire id, codec 7 extended header with model **nibble 9**. Make it collision-free by writing down a
nibble registry in SPEC.md: assigned 3, 5, 6, 7, 9, 14, 15; reserved and refused 0-2, 4, 8, 10-13; `b2` of nibbles 14/15
must be 0 or 1. The registry also reserves nibble **10** for any later CM model, such as W3 prior's token-shape model. New
tests in `tests/test_cm4.c` make every decoder in this tree refuse each reserved nibble, and nibble 14/15 streams with
`b2` in {2, 3, 0x80, 0x81, 0xff}.

**Why it is collision-free.**
- W4 cm's nibble 9 is refused by an explicit whitelist in every reader from rc3 on (rc3 and C1 accept only 3, 5, 6, 7,
  14, 15). Readers before rc3 refuse it by container version (a model-4 frame is version 11).
- No shipped generation ever wrote nibble 9.
- W3 prior's form (nibble 15 with `b2` bit 7) was never shipped. It was the scheme that changed the meaning of an
  assigned field: rc3 refused it only because `b2 > 1` happens to be checked.

**Alternatives not taken:**

- **Renumber W4 cm** (for example to nibble 10) and leave W3 prior's nibble-15 form as is.
  - Cost: rewriting W4 cm's SPEC, encoder, decoder, stream-need and determinism digests for no reader-side benefit.
  - It would keep the weaker design (a new meaning for an assigned field) alive in W3 prior.
- **Container version bump** (version 12 for model 4).
  - Gain: readers would refuse by version before parsing.
  - Cost: every model-4 frame would also become unreadable by readers that could otherwise handle all its other
    blocks. It is not needed, because the nibble whitelist already refuses model 4 explicitly.
- **A separate codec id** (for example codec 31 "CM4").
  - Gain: cleanest separation; frameinfo and planners could tell models apart without parsing the CM header.
  - Cost: a new codec id in the block table, the registry and the bindings, plus a planner change.
  - Worth considering if model 4 ever becomes a default rather than an `-x` opt-in.
- **Merge W3 prior as well.** Not done. It needs its own renumbering to nibble 10, plus a Mac screening, and W4 cm
  supersedes most of its gain.

**Owner to validate:** the nibble-9 / registry choice (versus a separate codec id), and whether W3 prior is retired or
renumbered to nibble 10.

## A1. KOLMO_NATIVE container version (finding 6) — branch review/c2-native-J62kQD, commit 91ae8cee
**Choice:** the encoder records whenever `codec_wire()` writes a native id. The record lives on the codec context and is passed back from trial threads. The frame becomes v11 only if a native id was written.

**Evidence:** `evidence/native-version-c2-2` (560 encodes, 0 misdecodes).
- 121 frames drop to v3/v7, and rc2 and 1.1.0 read them exactly.
- 10 frames stay v11 with no native id: a discarded trial set the flag. This is conservative and safe.

**Alternatives not taken:**
- **Scan the finished block payload for native ids.** It would be exact, but it duplicates every leaf and column parser.
- **Keep v11 whenever the variable is set** (the old behaviour). It is simple, but it locks out older readers for no reason.
- **One process-wide sticky flag.** Frame bytes would then depend on earlier calls in a library process, which breaks determinism.

## A2. Suspected dictionary + KOLMO_NATIVE trial-thread id mismatch — checked, not a bug
`evidence/dict-native-probe-c1`: 300 round trips with `-D` and KOLMO_NATIVE (lzma, lz, huf, all, all,strong; T1 and T4) were all exact on C1. No change was made.

## A3. `planner.c:158` -Wnonnull on memmem — not changed
The call is reached only after `hl <= n` has been checked (line 145 returns early otherwise). The warning is a false positive from the libc attribute. Silencing it, for example with an explicit `hl <= n` guard, is cosmetic; it is left for the owner.

## A4. Domain-suite input normalisation
- The FFV1 and CRAM round trips change bytes outside the payload: ffmpeg rewrites the y4m first line, and samtools drops read-name comments.
- So the domain corpus (`corpus-domain/`) uses inputs normalised once through those tools: `*_norm.y4m` with identical frame MD5s, and `*_norm.fastq`. Every codec compresses the same normalised file.
- **Alternative:** keep the original inputs and add a sidecar for the lost bytes to the rivals' sizes. That is more faithful to "complete file", but adapter processes break the direct-process memory measurement.

## A-mem. eco encode memory: macOS malloc's large cache, not brotli's working set (branch review/c2-mem-J62kQD)

**Cause.** Brotli is measured with a counting allocator in `scratch-mem/bmem*.c`:
- It encodes a 16 MiB block at quality 8, window 2^24, with about 300 allocations.
- Many of them are near 23 MiB, each a little larger than the last.
- Its live heap peaks at 106 MiB.

macOS libmalloc keeps freed large blocks resident in its large cache. RSS therefore reached 1,083 MB in the standalone test and 1,078 MiB for `kolmo -m eco -T 1` on SRR2584863.fastq. The brotli CLI behaves the same: 1,012 MiB on 16 MiB.

Proof: running with `MallocLargeCache=0` gives 133 MiB (FASTQ) and 190 MiB (foreman), with identical frames.

eco video blocks also reach brotli, through kvid's entropy back end, which calls `codec_compress`.

**Thread check.** `-m eco -T 1` runs exactly one thread, sampled with `ps -M`: 1 thread on all samples for FASTQ and foreman. This is not a T1 issue.

**Fix taken.** `src/codecs/brotli.c` now makes the same calls as `BrotliEncoderCompress`: the streaming API, the same quality, window, mode and size hint. The difference is an allocator that serves blocks of 1 MiB and up with mmap/munmap. The one-shot fallbacks (empty input, failed or oversized stream) still call `BrotliEncoderCompress`, so the output is byte-identical by construction.

Byte-identity check: `evidence/frame-identity-c2mem/` covers 191 frames — all 21 holdout files and the 13 native-matrix inputs in speed, balanced, eco, hc and ratio, plus `-x` on inputs of 1 MB or less. Result: 0 differing, 0 round-trip failures.

| file | mode | before (C1 portable) | after |
|---|---|---:|---:|
| SRR2584863.fastq | eco | 1,078 MiB | 157 MiB |
| foreman_cif.y4m | eco | 852 MiB | 187 MiB |
| mobile_cif.y4m | eco | 951 MiB | 199 MiB |

**Not changed.** ratio peaks of 617–640 MiB on the y4m files and 291 MiB on FASTQ. `MallocLargeCache=0` only takes ratio from 617 to 569 MiB, so that is a real working set (kvid / BWT), not allocator retention.

**Speed risk.** Fresh mmap pages are zero-filled, while recycled cached blocks were not. A /usr/bin/time spot check showed eco FASTQ at 3.30 s vs 3.05–3.23 s; this is not a benchmark. A timed eco comparison is needed before adopting.

**Options not taken (owner to choose):**
1. Re-exec the CLI with `MallocLargeCache=0` set. This is process-wide and also covers non-brotli large allocations, but it is hacky, macOS-only, and has no effect on library users.
2. A process-wide mmap allocator for every kolmo large buffer. Broader gain, but a larger and riskier change.
3. Patch vendor brotli to grow its arrays geometrically or pre-size them. This would cut allocation churn, but it diverges from upstream.
4. Lower the brotli window or block size for eco. This changes the output (format-neutral but not byte-identical) and needs a fresh size/speed screen.
5. Also give the brotli **profile** path (`src/plan/profile.c:310`) the same allocator. Left untouched because it serves `kolmo train` only.

**Finding from the C2-cm validation (owner to decide):**
- **Synthetic table (`tests/test_cmpar.c`):** `-x` with W4 cm codes the 282,679-byte table as one model-4 block of
  36,251 bytes. C1 picks codec 25 (one CM model per column) and writes 35,299 bytes, so model 4 is **2.7% larger**.
  The "halves" stream is also 1.0% larger (121,184 vs 119,943), because codec-25 parts now use model 4 too.
- **Cause:** W4 cm's planner compares the column path with model 4 on a 256 KiB sample (it was 1 MiB before W4 cm).
  This is consistent with the screening result for the ultra suite (W4 cm `-x` 0.28% larger than rc3 `-x`, with 2x
  speed and 13x less memory).
- **Options:**
  - **(a) Accept, as on the branch now.** The test's table case only requires 1/4-thread identity and a round trip.
  - **(b) Bias the decision.** Keep codec 25 when the columns come within 1-3% of model 4 on the sample.
  - **(c) Restore the 1 MiB sample** for tables only.
  - **(d) Let codec-25 parts race model 3 vs model 4.**
- Options (b) to (d) need an ultra-suite rerun to measure them. Nothing beyond (a) was done tonight.

## A6. eco text goes to block sorting (W4 docs tuning in C2) — trade-off for the owner
**Holdout, timed** (`evidence/report-holdout-c2`): C2 eco is 4.39% smaller in total than C1 eco. Encode speed is the same, and the encode peak drops from 1,079 to 233 MiB (with the brotli fix). But decode slows a lot on the text files:

| file | size | decode speed vs C1 |
|---|---|---|
| ld30.txt | −41% | 0.15× |
| reymont | −37% | 0.19× |
| SRR2584863.fastq | −27% | 0.10× |
| samba | −3% | 0.51× |

eco's total decode throughput is 135 MiB/s, against 283 MiB/s for C1.

**Choice made:** keep it in C2. This matches what eco is for: it sits between hc and ratio on size, and its decode stays faster than ratio's (62 MiB/s).

**Alternatives:**
- (a) Revert only the eco text→BWT rule. Keep the docs hc and balanced gains (−1.04% and −0.14%), and eco returns to its old size and decode speed.
- (b) Use BWT for eco text only when the block is at most N MiB. That bounds decode time.
- (c) Keep brotli for eco and give the size win to ratio instead. Ratio already uses BWT for text, so this changes nothing.

**Recommendation:** (a) if eco is meant as "fast-decoding archive"; keep the current choice if eco is "smaller than brotli -11 at much faster encode".

## A7. Make native KLZ the default for `speed`? — not done; owner decision
**Holdout:** speed + KOLMO_NATIVE=all is 4.42% smaller than lz4-1 (17 of 21 files) and 4.2% smaller than default speed. Decode is tied with lz4-1 (1.02×) and encode is 0.94×.

**Cost:** speed output becomes container v11, which rc2, 1.1.0 and v1.0.1 cannot read (they refuse it safely).

**Options:**
- (a) Keep it opt-in. This is the current state in C2.
- (b) Make it the default for speed in the next minor version, with a `--compat` flag that writes the old format.
- (c) Add a separate `-m speed2` preset.

**Recommendation:** (b) at the next format-generation release. The gain is real, but it is a format decision.

## A-img. kimg, a lossless 8-bit image codec (block codec 31) — branch review/c3-image-J62kQD @ cbb40b5d (on C2 25bedda8)
**Gap it targets:** in the timed dom-image run (C2, 6 PPM files), `-m ratio` was 19.7% larger than `cjxl -d 0 -e 7`. Ratio coded images as a predictor graph with block-sorted residuals (hubble: plain LZMA2). Only ultra used the slow CM image model.

**Design (as built):**
- **Where it runs:** at ratio and ultra (default levels, no dictionary or profile), on any 8-bit image with a header (PNM/PAM, NPY, BMP, TIFF via `meta_image8`). Each block holding pixels is also coded by kimg and the smaller coding is kept, the same pattern as kaud.
- **Prediction:** 10 predictors, weighted 2^44/s², where s is each predictor's error at W, N, NW and NE (integer only, so decoding is portable).
  - 7 spatial: gradient, MED, a smoothed gradient, N+NE−NNE, W+NE−N, N, W.
  - 3 inter-channel: the earlier channel's error under the same spatial predictor.
  - The blend is followed by a LOCO-style bias per (channel, error bucket, gradient signs).
- **Colour:** the encoder codes each transform and keeps the smallest: none, subtract-green, YCoCg-R. Samples become 9-bit.
- **Entropy coding:**
  - The residual is folded into the feasible range, then binarised: zero flag, unary bit length, first and second mantissa bits.
  - Each of those binary decisions mixes 4 context models logistically: (error bucket), (W and N residual magnitudes), (gradient sign texture × bucket), (earlier channel's residual, or NW, × NE). The models use adaptive-rate counters.
  - Lower mantissa bits use direct adaptive bits. The binary range coder is 32-bit and carry-less, with 12-bit probabilities.
- **Format:**
  - Stream: version byte 1, ich, transform, iw, phase, head, npx, the stored head and tail bytes, then the coded data.
  - Frame version 11. Codec 30 is left for W4 entropy's khuf v2.

**Sizes (deterministic; `-m ratio`, release build):**

| image | C2 ratio | **C3 kimg** | cjxl e7 | cjxl e9 |
|---|---:|---:|---:|---:|
| astronaut | 376,602 | **330,893** | 324,397 | – |
| chelsea | 175,854 | **148,663** | 142,016 | – |
| coffee | 392,200 | **338,303** | 335,092 | – |
| hubble_deep_field | 1,456,660 | **1,315,225** | 1,286,417 | – |
| immunohistochemistry | 353,870 | **286,131** | 270,395 | – |
| retina | 952,413 | **809,282** | 738,818 | – |
| **total** | 3,707,599 | **3,228,497 (−12.9%)** | 3,097,135 (C3 +4.2%) | 3,076,319 (C3 +4.9%) |

- Ultra still keeps the CM image model when it is smaller. For example, `-x` on chelsea is 141,884 B, which is below cjxl e7.
- hc is unchanged.

**Speed (release, `/usr/bin/time` medians of 3, not a timed suite):**

| | C3 | C2 | ratio |
|---|---:|---:|---:|
| ratio encode, 6 images | 5.85 s | 4.09 s | 1.43× |
| ratio decode, 6 images | 0.67 s | 0.26 s | **2.6×** |

The decode ratio is above the ~2× target: hubble's C2 frame was LZMA2, which decodes in 0.03 s. Excluding hubble, decode is 1.9× C2.

**Decode speed compared with JPEG XL** (domain run): djxl decodes these images at about 20 MiB/s at T1; kimg decodes about 10.7 MiB in 0.67 s, roughly 16 MiB/s. The tests against JXL decoding speed are pending.

**Validation:**
- `make all`, `make check` and `make check-rc` pass.
- Determinism: 122/122 frames unchanged; no expected-frame changes.
- `evidence/kimg-compat-c3`:
  - 972/972 C2 compatibility frames decode with C2's outcome;
  - 18/18 kimg frames round-trip (ratio, `-x`, `-B 1` multi-block with mid-row phase, PGM grey, PAM RGBA, PPM with trailing bytes);
  - every older generation (v1.0.1, 1.1.0, rc2, rc3, W3/W4 builds, C1, C2-cm, C2) refuses all 210 kimg decodes; 0 misdecodes, 0 crashes.
- Corruption: 60/60 CLI corruptions are refused by the block checksum. In an ASan/UBSan fuzz of the decoder, 286/300 were refused, 14 decoded to wrong bytes (the container checksum catches these), and there were no sanitizer errors.

**Not done:** a timed dom-image suite (main session); Phase E sanitizers on this branch; CI.

**Options not taken / negative results** (development copies in `evidence/kimg-dev/`):

| option | result |
|---|---|
| plain two-rate adaptive bits, no mixing (v0) | 3,563,373 |
| mixing, learning rate ×6 | 3,612,950 (worse) |
| mixing, learning rate ×1 | 3,507,796 |
| colour transforms | 3,246,144 |
| fractional learning rate | 3,237,707 |
| W/N error weight 1 instead of 2 | 3,228,185 (kept) |
| inverse-cube weights | 3,271,761 (worse) |
| 14 predictors | −0.1% for +4% time; not kept |
| richer residual context (NE, prev, ee) | ±0.03%; not kept |
| direct-coded second mantissa bit | +0.2% size, −2% time; not kept |
| 3 mixer inputs | +0.3%, no speed gain; not kept |

**Ideas left for later:**
- A JXL-style MA tree, or a learned context selection, for the last ~4%.
- An SSE/APM stage.
- An LZ77 or run mode for flat regions (retina has the largest gap, +9.5%).
- A faster decoder: profile the predictor blend and the int64 division.
- Offering kimg at hc as an opt-in. It is not offered at hc now, because hc's image decode is about 130 MiB/s and kimg's is about 16.

## A8. kimg (image codec 31) in default ratio and -x — timed result and decision request
Timed run: `evidence/report-domain-c3/dom-image`, strict T1, 6 PPM images, release build d8afcdce… at cbb40b5d.

| C3 ratio vs | size | encode | decode | memory |
|---|---|---|---|---|
| C2 ratio | **−12.92%** (6/6 files) | 0.54× | 0.59× (12.3 vs 23.4 MiB/s total) | decode RSS lower on 5/6 |
| cjxl -d0 -e7 | +4.24% | 0.53× | 0.59× | lower average RSS |
| cjxl -d0 -e9 | +4.95% | **1.86×** | 0.61× | — |

**Format effect:** ratio and `-x` image blocks become codec 31 in v11 frames. Every older decoder refuses them (210/210); C3 decodes all C2 frames identically.

**Choice made:** kept on its own branch (`review/c3-image-J62kQD`). It is **not merged into C2**: it changes default ratio output for images and slows image decode about 1.7×.

**Options:**
- (a) Merge into the next candidate as is.
- (b) Make it the ratio default only when it saves at least X%.
- (c) Use it for `-x` only.
- (d) Keep working toward JPEG XL parity first; candidates are a JXL-style weighted predictor and MA-tree contexts. The design notes are in section A-img.

**Recommendation:** (a) for rc4 if image ratio matters. It closes 80% of the gap to JPEG XL, and it is still smaller than every general-purpose rival.

### A-img round 2 (2026-11-21, 01:55–03:15 UTC) — branch review/c3-image-J62kQD: 0260a7aa, then f1f3f1c3
**Timed result for cbb40b5d** (main session, evidence/report-domain-c3/dom-image): ratio is 12.92% smaller than C2. Encode is 0.54× and decode 0.59× C2 (12.3 MiB/s, against 23.4 for C2 and 20.2 for djxl); size is +4.24% against cjxl e7.

**Profile** (`sample`, standalone): about 60% in the pixel loop (predictor blend, contexts) and about 38% in bit coding.
- Disabling the blend saved only 11%. Neither fixed-rate counters nor frozen mixer weights saved more than about 5–7%.
- So the time goes to the serial decoding chain: about 5 mixed binary decisions per sample.
- Reaching djxl's speed needs a structural change, not more tuning (see the ideas below).

**Committed:**

| commit | change | effect |
|---|---|---|
| 0260a7aa | same bitstream: interleaved error layout (the 10 error sums vectorise, absent neighbours read a zero entry); the coder always inlined with per-sample context rows; specialised encode and decode loops; colour transform chosen on 1/8 of the rows (bands of 16) from 256 KiB | CLI release, 6 images: decode 0.67 → 0.53 s, encode 5.87 → 4.75 s; bytes identical to cbb40b5d |
| f1f3f1c3 | stream version 2: weight offset 4 (was 1), WW errors added to the error sums, bias halved at 512 samples (was 64); version 1 still decoded | 3,228,497 → 3,186,034 B (−1.32%); speed unchanged |

**Sizes** (`-m ratio`, CLI): f1f3f1c3 against C2 and cjxl.

| image | C2 | f1f3f1c3 | cjxl e7 |
|---|---:|---:|---:|
| astronaut | 376,602 | 329,224 | 324,397 |
| chelsea | 175,854 | 146,579 | 142,016 |
| coffee | 392,200 | 336,260 | 335,092 |
| hubble_deep_field | 1,456,660 | 1,301,120 | 1,286,417 |
| immunohistochemistry | 353,870 | 283,844 | 270,395 |
| retina | 952,413 | 789,007 | 738,818 |
| **total** | 3,707,599 | **3,186,034 (−14.07%)** | 3,097,135 (f1f3f1c3 +2.87%) |

**Speed** (CLI release build, medians of 3 `/usr/bin/time` runs while Phase E ran in other worktrees):

| | C2 | f1f3f1c3 |
|---|---:|---:|
| decode, 6 images | 0.26 s | 0.53 s (2.0×) |
| encode, 6 images | 4.11 s | 4.75 s (1.16×) |

Expect the timed suite to show roughly 15 MiB/s decode against C2's 23.4 and djxl's 20.2.

**Validation of f1f3f1c3:**
- `make all`, `make check` and `make check-rc` pass.
- Determinism: 122/122 frames.
- `evidence/kimg-compat-c3-r3`: 972/972 C2 compatibility frames decode with C2's outcome; 18/18 kimg frames round-trip; older decoders refuse 210/210.
- All 18 version-1 frames from cbb40b5d decode exactly.
- The same checks passed for 0260a7aa (`evidence/kimg-compat-c3-r2`).

**Negative results** (code in `evidence/kimg-dev/round2/`):
- **Tree-coded bit length** (version 2 candidate): −0.03% size but +12% decode, because most residuals are short and unary stops early.
- **SSE/APM stage** on the mixed probability: at best −0.008%, at +18% decode.
- **Predictor-spread context** in place of model 3: −0.09%.
- **JPEG XL-style self-correcting predictors** (signed neighbour residuals fed back, 3 extra predictors): +0.2% to +0.6% (worse; extra predictors dilute the blend).
- **Weight offset:** 8 gave −0.33% and 16 gave +0.55%, against 4 at −0.57% alone.
- **Bias halving:** at 32, +0.6%.
- **Larger N weight:** +0.27%.

**Ideas not attempted (for the owner):**
- **Decode speed:**
  - one multi-symbol adaptive decode per sample (for example rANS with an adaptive frequency table per context), instead of about 5 binary mixed decisions;
  - or 2–4 interleaved independent streams (row bands), so the CPU overlaps the decoding chains at one thread.
  - Either would be a new kimg version.
- **Size:**
  - MA-tree-like context clustering learned per image (signalled in the header);
  - a run or LZ77 mode for flat regions (retina is still +6.8% against cjxl);
  - per-image predictor-set selection.

## A8 update — kimg round 2 timed (branch head f1f3f1c3)
The timed run gives ratio images −14.07% vs C2 and +2.87% vs cjxl -e7, with encode at 0.76× and decode at 0.69× of C2. kimg v1 frames (cbb40b5d) still decode.
The recommendation is unchanged: merge into rc4 if image ratio matters. Decode speed needs a redesign (multi-symbol coding), as noted in A-img round 2.

## A6 update — option (a) is ready as a branch: review/c2-ecoalt-J62kQD @ 4566725d (C2 + one switch)
The switch is `KOLMO_ECO_TEXT_BWT`, and it defaults to 0. eco text and JSON token streams keep brotli; everything else in C2 is unchanged.

| holdout eco total | bytes |
|---|---|
| this option | 131,458,743 |
| C2 | 126,086,585 |
| C1 | 131,876,301 |

So this option is 0.32% smaller than C1, from the window merges, and keeps C1-level eco decode on text.

**Checks:** make check passes, and determinism is 122/122 (text.txt eco returns to C1's frame). Sizes were measured untimed.

**Not run:** a timed eco run and Phase E on this branch. CI is green (run 44861578916).

**To choose:** take C2 for the smallest eco, or merge this branch for fast eco decode.
