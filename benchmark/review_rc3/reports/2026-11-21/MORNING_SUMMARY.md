# Morning summary — overnight run 2026-11-20/21 (Apple M5, strict T1)

The detailed log is `NIGHT_LOG.md`. Decisions waiting for you are in `ALTERNATIVES_FOR_REVIEW.md`.

## Recommended candidate: C2 = 25bedda8 on `review/c2-integration-J62kQD`
It is pushed and CI is green on x86-64, arm64 Linux and macOS, with sanitizers and fuzzing. It is not merged, there is no PR, and there is no tag.

C2 is C1 plus four changes:
1. The KOLMO_NATIVE v11 fix. Plain frames are again readable by rc2 and 1.1.0.
2. W4 docs encoder tuning.
3. The brotli memory fix. The eco encode peak drops from 1.08 GB to 233 MiB.
4. W4 cm for `-x`. It is 2× faster with about 13× less memory, using a collision-free model id.

| gate | result |
|---|---|
| Phase E (17 steps) | PASS |
| compat (14,580 decodes) | 0 misdecodes |
| CI | PASS |
| timed holdout vs C1 | eco −4.39%, hc −1.04%, balanced −0.14%; speed/ratio identical |

## Where Kolmo stands against the competition (fresh, timed, byte-verified)

| domain | best rival | Kolmo C2 |
|---|---|---|
| general (holdout, unseen) | xz -9 / zstd -19 / zstd -3 / lz4 -1 | ratio −11.3% vs xz, hc −16% vs zstd-19, balanced −21.5% vs zstd-3, speed = lz4 (native opt-in −4.4%) |
| numeric arrays | pcodec | **−37%** (ratio), −30% balanced at 3× faster decode |
| audio | flac -8 | **−12.4%** ratio, −9.1% balanced |
| video | FFV1 | **−14.9%** ratio, −11.5% hc with 5× faster decode |
| FASTQ | CRAM 3.1 archive | +1.7% (tie), encode 8.6× faster, decode 0.54× |
| images | JPEG XL lossless (-e7/-e9) | C2: +20.5% (behind). **kimg branch: +2.9% vs e7, and 2.6× faster encode than e9** |

## Your decisions (details in ALTERNATIVES_FOR_REVIEW.md)
- A6: eco text block sorting. It is −4.4% in size, but eco text decode becomes 0.10–0.19×. Keep it, or revert only that rule? The revert is ready as branch `review/c2-ecoalt-J62kQD` (4566725d).
- A7: native KLZ as the speed default. It is −4.4% vs lz4 at the same decode, but it changes the format.
- W4 cm: output is 2.7% larger on one synthetic table case (codec 25 used to win there); accepted for now, options are listed.
- Version label: C2 changes default bytes and adds a `-x` generation, so I propose 1.2.0-rc4.

## Still open
- Image codec: merge decision (A8); validation results are below.
- Tools not measured: OpenZL, Kanzi, fqzcomp5, cmix, ZipNN. Audio has only been measured on 5-second clips.

## Image work and ultra timing
- **`-x` on C2 (timed).**
  - vs C1: 1.98× faster encode, 1.87× faster decode, peak memory 226 MiB instead of 3,046 MiB, for 0.28% larger output.
  - vs zpaq -m5: still 7.6% smaller, with 7.5× faster decode.
- **New image codec kimg** (codec 31, branch `review/c3-image-J62kQD`, head f1f3f1c3; **not in C2**). This is decision A8.
  - Size (timed, round 2): ratio images are **14.1% smaller** than C2.
  - Gap to JPEG XL -e7: down from +20.5% to **+2.9%**. It encodes 2.56× faster than cjxl -e9.
  - Speed cost: image decode is 0.69× of C2's (14.8 vs 23.6 MiB/s; djxl 21).
  - Compatibility: clean. Every older decoder refuses kimg frames, and C3 decodes all C2 frames identically.
  - Validation: Phase E 17/17 and CI 5/5 on cbb40b5d. The round-2 commits pass make check/check-rc/determinism/compat, but CI and sanitizers have not run on them yet.
