# Plan forward — written 2026-11-21 (night 2)

Every figure is from this review's own measurements: strict single thread, Apple M5, byte-verified decodes. File references are in `evidence/`.

## 1. When to merge to main and push

**The right time is after you have read the rc4 report and the rc4 PR shows all of the following:**
1. GitHub CI is green on the rc4 head.
2. Phase E 17/17 passes. (Done.)
3. The compatibility matrix has 0 misdecodes. (Done: 19,136 decodes.)
4. The timed holdout figures are in the PR. (Running tonight.)

At that point:
- Merge the PR with a merge commit, not a squash, so that each lane's commits and their evidence stay traceable.
- Push main.
- **Do not tag yet.** Tag only when the release steps in §2 are ready, so the tag carries the final version number.

Native tuning (KLZH2, the KLZH optimal parse, faster KLZ) stays on its own branches. It goes in only after its own compatibility matrix and holdout. It changes what `KOLMO_NATIVE` writes, and it is a candidate for the defaults of the first renumbered release (§3).

## 2. Renumbering to 0.0.1 (your decision; plan only, nothing done)

**What is published today:**

| channel | package | versions |
|---|---|---|
| PyPI | `kolmoc` | 1.0.0, 1.0.1 |
| crates.io | `kolmo` | 1.0.0, 1.0.1 |
| Homebrew | own tap `adnaneben/homebrew-kolmo`, formula `kolmo` | 1.0.1 |

1.1.0 and the 1.2.0 release candidates were never published.

**How each channel treats a lower version** (this is why the steps below matter):
- **PyPI.**
  - `pip install kolmoc` picks the highest non-yanked version. Once 1.0.0 and 1.0.1 are yanked, new installs get 0.0.1.
  - `pip install -U kolmoc` on a machine that already has 1.0.1 will **not** downgrade: pip keeps 1.0.1. Those users must run `pip install kolmoc==0.0.1` or `pip install --force-reinstall kolmoc`.
  - Yanked files stay downloadable for anyone who pins `==1.0.1`. That is intended: it keeps old builds reproducible.
- **crates.io.**
  - Yanked versions are skipped for new lockfiles, but existing lockfiles keep them.
  - A dependency written as `kolmo = "1"` will never resolve to 0.0.1 (a different semver major). Users must change it to `kolmo = "0.0.1"`.
  - In cargo's semver, every 0.0.x release is incompatible with the next one: `0.0.1` and `0.0.2` do not upgrade into each other. Plan the next steps as 0.1.0, 0.2.0 and so on, or users will have to edit their manifest for every patch release.
- **Homebrew (own tap).**
  - `brew upgrade` will not move 1.0.1 to 0.0.1.
  - Add `version_scheme 1` to the formula. Homebrew then treats every version under the new scheme as newer than any under the old one, so `brew upgrade` moves users to 0.0.1.

**Release steps for 0.0.1:**
1. Merge rc4 to main (§1).
2. Change the version labels in one commit: VERSION, `pyproject.toml`, `Cargo.toml`, the formula with `version_scheme 1`, and the CHANGELOG heading "0.0.1 — first release of Kolmo's own codec family; supersedes 1.0.x".
3. **Keep decoding every 1.0.x file.** The container is versioned, and 0.0.1 reads every frame 1.0.x wrote; the compat matrix proves this for v1.0.1. State it in the README, so that "renumbered" is not read as "incompatible".
4. Tag `v0.0.1` and publish to all three channels.
5. Yank 1.0.0 and 1.0.1 on PyPI and crates.io, with the yank reason "superseded by 0.0.1 (renumbered)".
6. Add a README note on how to move from 1.0.x (the three commands above).
7. Make the CLI print its own version and its format generation (`kolmo -V`). Today `--version` is an illegal option. Bug reports then say which numbering they come from.

**Risk to note:** any downstream project pinned to `kolmoc>=1` or `kolmo = "1"` stops getting updates without any error. That is the cost of going below 1.0; the steps above keep it to one manual change per user.

## 3. The native codec family (the reason for the renumbering)

Kolmo still calls a library coder in every default preset:

| preset | library coder today | Kolmo's own replacement | where it stands (component level, tuning set) | what is left |
|---|---|---|---|---|
| speed | LZ4 | KLZ (28) | 8% smaller than lz4; decode 0.69× (4,010 vs 5,790 MB/s) after tonight's +6%; end to end (holdout) 4.4% smaller at the same decode speed | A7: make it the default in 0.0.1 |
| balanced | zstd -3 | KLZH2 (32, new tonight) | 0.9% larger than zstd -3 (was 1.7%) | KLZH2 low-level parse; planner retune for native |
| hc | zstd -19 | KLZH2 + optimal parse (in progress tonight) | KLZH9 was 12.5% larger than zstd -19 | optimal parse (subagent branch) |
| eco | Brotli | KLZH2 level 7 or KLZMA | KLZH7 +8% vs Brotli; KLZMA 12% smaller but decodes 4.6× slower | needs context modelling, or accept KLZMA's trade-off |
| ratio | LZMA2 | KLZMA (24) | same size (+0.08%); decode at parity (161.9 vs 163.7 MB/s) | ready |
| Huffman leaves | zstd HUF | khuf (27) | +0.16% size, decode 0.97× | ready |
| dictionaries, profiles | zstd and Brotli dictionaries | none | — | KLZH2 dictionary support |

**Measured tonight** (prototype `review/native-defaults-J62kQD`, timed holdout):
- Default speed → KLZ: **−4.18% vs rc4 speed**, encode 0.97×, decode 0.96×. Against lz4 -1 it is −4.42% with decode 1.04×.
- KLZMA and khuf as defaults: size change ≤ +0.04%.
- Native hc (KLZH2 + optimal parse) was **+1.56%** vs zstd hc. The losses come from KLZH's 16 MiB offset limit (nci +9.1%) and its minimum match of 4 (mozilla +6.8%, sao +3.4%).
- Both limits belong to the unreleased codec 32, so they can still change. Fixing them is the next native work item:
  - offset codes up to 2^30;
  - minimum match 3 in KLZH2;
  - a binary-tree finder over a larger window at level 9.

**Recommendation for 0.0.1 defaults:** switch to Kolmo's own coder where it is at parity or better:
- speed → KLZ;
- ratio → KLZMA;
- Huffman leaves → khuf.

Keep zstd and Brotli for balanced, hc and eco until KLZH2 and its optimal parse match them on the holdout. The renumbered release is the right moment for these default changes, because the version change already tells users the output is new. A branch with these defaults can be built and measured before the release; that is the next step after tonight's tuning lands.

## 4. Images

**Today:**
- rc4 includes kimg, at +2.9% vs JPEG XL lossless -e7.
- kimg encodes 2.6× faster than cjxl -e9.
- It decodes at 0.69× of C2's image speed (14.8 vs 23.6 MiB/s; djxl reaches 21).

**Next, in order of value:**
1. **Lossless JPEG recompression.** Most images on disk are already JPEG, and Kolmo gains nothing on them today. Recoding JPEG's Huffman-coded coefficients with a context model, and restoring the exact original file, saves about 20% (JPEG XL's JPEG transcoding and Lepton/brunsli show the range). This is likely the largest real-world gain left in images.
2. **PNG recompression.** Unpack the deflate stream, code the pixels with kimg, and store what is needed to rebuild the identical PNG ("precomp"-style).
3. **kimg v3, to beat cjxl -e7 on size.**
   - A weighted mix of several predictors and context modelling of the residuals per channel and per local gradient.
   - Palette and screenshot detection.
   - 16-bit samples.
4. **kimg decode speed.** SIMD for the predictor and the coder, with a target at or above djxl.
5. **A larger image test set.** Photos, screenshots, scans and medical images; only 6 PPMs so far.

## 5. Video

**Today:** raw CIF y4m only.
- Ratio is 14.9% smaller than FFV1 level 3.
- hc is 11.5% smaller, with 5× faster decode.

**Next:**
1. **Test at real sizes.** 1080p and 4K, 10-bit (yuv420p10), several minutes long.
2. **Measure against stronger lossless rivals.** x264 `-qp 0`, x265 lossless, and FFV1 multithreaded on a separate multithread scoreboard.
3. **Temporal prediction.** Motion-compensated prediction of each frame from the previous one; today the gain is mostly spatial.
4. **Frame-parallel decode** for playback-speed use.

Most video is stored lossy (H.264/HEVC/AV1). Recompressing those losslessly means re-coding CABAC and is a research item, not a near-term target. The near-term market is mastering and archival of raw or intra-only footage.

## 6. Audio

**Today:** on 8 five-second mono ESC-50 clips, ratio is 12.4% smaller than flac -8, and balanced is 9.1% smaller at about 0.9× flac's decode speed.

**Next:**
1. **A real music set.** 16-bit/44.1 kHz stereo CD tracks and 24-bit/96 kHz files, several minutes each.
2. **The stronger rivals.**
   - OptimFROG, Monkey's Audio (extra high), WavPack `-hhx` and MPEG-4 ALS.
   - They beat FLAC by about 3–8%, so they are the bar for a "best lossless audio" claim.
3. **Stereo decorrelation and a cascade of adaptive filters** (the approach of the top coders: sign-LMS stages after a long linear predictor).
4. **Decode speed.** At least flac's for balanced. Players need real-time decode with a large margin.

## 7. FASTQ / genomics

**Today:** ratio is 1.7% larger than CRAM 3.1 archive (a tie), with 8.6× faster encode and 0.54× decode.

**Next:**
1. Measure fqzcomp5 and Spring.
2. Quality-score models conditioned on position and previous qualities (fqzcomp's approach).
3. An optional read reordering mode (Spring-like) for archives where read order does not matter.
4. Decode speed.

## 8. Numeric and tabular data

**Today:** on raw integer and float arrays, ratio is 37% smaller than pcodec, and balanced is 30% smaller with 3× faster decode.

**Next:**
1. Measure OpenZL, which the plan named but which has not been run yet, plus fpzip/ZFP (lossless) for floats.
2. Time series with Gorilla/Chimp-style XOR coding.
3. Parquet and Arrow files, with their columns recoded.

## 9. General presets (holdout, rc4 = C2 on these bytes)

| preset | vs rival | size | weak point |
|---|---|---|---|
| speed | lz4 -1 | tie (−0.25%); with native KLZ −4.4% | about 29% larger than zstd -1: decide whether speed should aim at zstd -1's point |
| balanced | zstd -3 | −21.5% | encode 0.72×; peak memory 137 MiB vs 7 MiB |
| eco | zstd -9 | about −23% (screening) | encode 0.36×; decode much slower than zstd |
| hc | zstd -19 | −16% (C1), about −17% (C2) | memory |
| ratio | xz -9 | −11.3% | decode 0.67× |
| `-x` | zpaq -m5 | −7.6%, 7.5× faster decode | not yet measured against cmix, paq8px or Kanzi |

**Next:**
1. Peak memory of balanced: stream the planner trials instead of holding whole blocks.
2. Decode speed of ratio: KLZMA decode tuning, or a faster ratio point.
3. A multithread scoreboard, separate from strict T1.
4. The unmeasured rivals: Kanzi, cmix, paq8px, OpenZL, ZipNN (model weights).

## 10. Process

- **Keep the holdout untouched.** Tuning tonight used only the screening corpus.
- **Add test sets per domain.** Music, 1080p video, photos, JPEG and PNG files.
- **Put the fair-benchmark harness (`audit.py`) on main**, with its quiet rule, preflight and byte verification. Then every PR can carry a scoreboard produced the same way.
