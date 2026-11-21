# Night 2 summary — 2026-11-21, 15:00 → 23:15 (+03), Apple M5

The detailed log is `NIGHT2_LOG.md`; the plan forward is `PLAN_FORWARD.md`. Nothing was merged to main, tagged, published or yanked, and nothing was deleted. Every timed figure is strict single thread, with byte-verified decodes and the preregistered quiet rule.

## 1. rc4: ready for your review

**Draft PR [adnaneben/kolmo#11](https://github.com/adnaneben/kolmo/pull/11)**, branch `review/rc4-J62kQD`, head 5a3ebe2c. It is C2 plus your decisions: A6 eco text revert, A8 kimg, A7 native kept opt-in, W4 cm accepted, and the label 1.2.0-rc4.

| gate | result |
|---|---|
| Phase E (17 steps) | 17/17 PASS |
| GitHub CI | 5/5 PASS: x86-64 Linux, arm64 Linux, macOS, sanitizers, fuzz |
| Compatibility matrix (16 generations) | 19,136 decodes: 0 misdecodes or crashes |
| kimg compatibility, native-version check, byte identity | clean; 126/126 identical frames |
| Timed holdout (1,575 trials) | see below |

**rc4 on the holdout (21 unseen files):**

| preset | vs C2 | vs nearest rival |
|---|---|---|
| speed | same bytes | lz4 -1: tie in size, decode 1.15× |
| balanced | same bytes | zstd -3: **−21.6%**, encode 0.73×, decode 0.94× |
| eco | +4.26%, **decode 1.36×** (A6) | zstd -9: **−20.5%**, encode 0.30×, decode 0.67× |
| hc | same bytes | zstd -19: **−16.9%**, encode 1.09×, decode 0.95× |
| ratio | −0.17% (kimg) | xz -9: **−11.5%**, encode 1.35×, decode 0.66× |

**When to merge:** whenever you have read the PR and are happy with it. Every gate is green. Merge with a merge commit, then push main. Tag only when the 0.0.1 release steps are ready (`PLAN_FORWARD.md` §2).

## 2. Native codec tuning (Kolmo's own coders)

| change | branch / commit | result (tuning set unless noted) |
|---|---|---|
| KLZ decoder fast path, offsets 8–15 | `review/native-tune-J62kQD` 0a15b0e2 | decode +6%, same format |
| **KLZH2 = codec 32**: KLZH plus a new rANS coder (KRANS) for the sequence codes | d25da20f | −0.85% vs KLZH at level 3, decode ~1.0×; KRANS decode 0.62 ns/symbol after 4 rounds (1.37 at first) |
| **KLZH optimal parse** (levels 7–9) and lazy parse with per-block minimum match (levels 5–6), by a subagent | `review/native-klzh-opt-J62kQD` b0403731, merged as 2e62763c | klzh9 **+12.5% → +2.5%** vs zstd -19, decode as fast |
| Combined (KLZH2 + optimal parse) | 13eed5bb | klzh2_9 **+1.5%** vs zstd -19 at the same encode speed; klzh2_3 +0.9% vs zstd -3 |
| **KLZH2 format** (subagent): minimum match 3, offsets up to 2^30 − 1, Huffman tables per 256 KiB of input | `review/native-klzh2-fmt-J62kQD` 1a74a603; `review/native-tune-J62kQD` fast-forwarded to it | klzh2_9 **+0.77%** vs zstd -19; end to end, native hc **−0.32%** vs zstd hc on the screening corpus; KLZH2 decode 4–7% slower; KLZH (29) bytes unchanged |
| **0.0.1 defaults prototype** | `review/native-defaults-J62kQD` d16cde21 | below |

**Prototype of 0.0.1 defaults** (KOLMO_NATIVE unset: speed → KLZ, ratio → KLZMA, Huffman leaves → khuf):
- **Timed holdout:** speed is **−4.18% vs rc4** (17 wins / 1 loss), encode 0.97×, decode 0.96×; **−4.42% vs lz4 -1**, decode 1.04×.
- **Screening corpus, size:** balanced +0.01%, eco 0.00%, ratio +0.04%. All round trips exact.
- **rc4's decoder reads every frame** of this prototype.
- **Native hc measured and switched off** (decision D9):
  - It was +1.56% vs zstd hc on the holdout: nci +9.1% (KLZH offsets stop at 16 MiB), mozilla +6.8% and sao +3.4% (zstd's minimum match of 3).
  - Both are format limits of KLZH2, which is unreleased and still free to change, so they were fixed tonight (row above).
  - **Retested on the holdout** with the new format (315 accepted): **+0.55%** vs zstd hc (4 wins / 13 losses), encode 0.83×, decode 0.95×, encode peak 347 vs 242 MiB.
    - The worst files are nci (+4.9%), mozilla (+3.3%) and sao (+3.0%).
    - D9 stands: native hc stays off until it reaches parity on the holdout.

## 3. Decisions I made (alternatives for you)

| # | decision | alternative |
|---|---|---|
| D1 | KLZH2 as a **new codec id (32)**, with rANS only where it saves ≥3% of a stream | change KLZH (29) itself; no: it is readable by rc3, so its format is fixed |
| D2, D6 | protected timed runs. Stopped one prototype run that Spotlight indexing stalled (4 trials in 50 min) and reran it later | run with noise; no: the numbers would not be fair |
| D4 | kept the optimal parse at KLZH level 7: smaller, but 3× slower to encode | level 7 back on the lazy parse: ~3× faster encode, ~6% larger |
| D5 | did not touch system processes (Spotlight); moved scratch files under `.noindex` | — |
| D7 | `KOLMO_NATIVE` maps zstd -19 to KLZH level 9: zstd -19's encode time, 1% smaller than level 8 | level 8 |
| D8 → D9 | made hc native in the prototype, measured it, then **switched it off** (still +0.55% after the format work) | ship native hc at +0.55% size, 0.83× encode, about 100 MiB more encode peak |
| D10 | accepted the KLZH2 format work, including 256 KiB Huffman spans: smaller, decode 4–7% slower | 512 KiB spans: probably faster decode, slightly larger (not measured) |
| — | rc4 PR opened as a **draft** | — |

## 4. For you to decide

1. **Merge rc4** (PR #11): my recommendation is yes, after your read.
2. **0.0.1 defaults:** speed → KLZ, ratio → KLZMA, Huffman leaves → khuf. My recommendation is yes: −4.2% on speed, no loss elsewhere, readable by rc4.
3. **The 0.0.1 renumbering steps** (`PLAN_FORWARD.md` §2):
   - Homebrew `version_scheme 1`;
   - yank 1.0.0 and 1.0.1 on PyPI (`kolmoc`) and crates.io (`kolmo`) **after** 0.0.1 is up;
   - next versions 0.1.0, 0.2.0 (cargo treats every 0.0.x as incompatible with the next).
4. **The native branches:** merge `review/native-tune-J62kQD` (KLZ decoder, KLZH2, optimal parse) into the rc line after its own compatibility matrix. It adds codec 32 (opt-in only).

## 5. Compatibility of the native branch
The compatibility matrix on `review/native-tune-J62kQD` was run twice, at 13eed5bb and at the final 1a74a603:
- 22,984 decodes each time, **0 misdecodes or crashes**;
- it reads everything rc4 reads;
- every frame it writes decodes exactly;
- rc4, C2 and rc3 refuse its codec-32 frames cleanly.

Unit tests, ASan/UBSan, natbench and end-to-end round trips pass.

## 6. Not done
- Native hc's remaining holdout gap (nci, mozilla, sao). The subagent found that sequence and literal counts match zstd's closely, so what is left is probably entropy-stage detail.
- The encoder memory estimate for KLZH2 (`memneed.c`) is still zstd's figure. It is used for scheduling only and never changes output.
- A real-music audio set, 1080p video, JPEG/PNG recompression: planned only (`PLAN_FORWARD.md` §4–6).
