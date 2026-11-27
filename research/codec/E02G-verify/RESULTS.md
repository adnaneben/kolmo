# E02G-verify results: decode-only timing of the E02G frames

Run: GitHub Actions 43280075566 (`.github/workflows/e02g-verify.yml`, head `ca0f0b7`), protocol committed first in
`144e1ae`. Evidence recovered from the job log and checked against the run's `MANIFEST.json` (all 4 files match);
copies in `results/` (`SHA256SUMS`). The regenerated inputs are byte-identical to E02G's (`manifest.json` equal to
`E02G/results/manifest.json`, 17 inputs). All four policies reconstructed exactly before timing; the ASan/UBSan/LSan
correctness pass ran first and the run completed successfully.

This is verification only. **It does not change E02G's registered verdict** (provisional, both modes).

## Registered outcomes (K/C = candidate decode time ÷ control decode time, family pooled)

| Mode | Family | Regime | K/C | C2/C noise band [p5, p95] | Added time, family total | Outcome |
|---|---|---|---|---|---|---|
| fast | Backblaze (registered miss) | warm | **1.087** | [0.994, 1.004] | +391 µs | **reproduced** |
| fast | Backblaze (registered miss) | displaced | **1.090** | [0.933, 1.058] | +405 µs | **reproduced** |
| hc | OWID (registered miss) | warm | **1.198** | [0.991, 1.007] | +1,444 µs | **reproduced** |
| hc | OWID (registered miss) | displaced | **1.206** | [0.983, 1.016] | +1,515 µs | **reproduced** |
| fast | OWID | warm / displaced | 0.974 / 0.968 | — | −121 / −151 µs | (not a miss) |
| hc | Backblaze | warm / displaced | 0.957 / 0.963 | — | −168 / −144 µs | (not a miss) |
| fast | dense control (EEG) | warm / displaced | 1.038 / 1.042 | — | +500 / +550 µs | control |
| hc | dense control (EEG) | warm / displaced | **1.328 / 1.321** | — | +2,981 / +2,948 µs | control |
| both | single-byte control | both | 0.994 – 1.003 | — | ≈ 0 | control |

Both E02G decode misses are **real**: they exceed 1.05 and the identical-frame noise band, in both the warm and the
cache-displaced regime. They are not a timing artefact of E02G's encode+decode harness.

## Per-file pattern (warm; files with INT blocks; size change = candidate vs control frame)

| Mode | File | K/C | Size change | Added |
|---|---|---|---|---|
| fast | backblaze_smart_242 | 1.218 | −2.60% | +231 µs |
| fast | backblaze_smart_241 | 1.156 | −0.55% | +168 µs |
| fast | owid_total_vaccinations | 1.154 | −1.36% | +106 µs |
| fast | eeg_S005 | 1.109 | −0.42% | +398 µs |
| fast | owid_people_vaccinated / owid_total_cases / backblaze_smart_9 | 0.91 – 1.00 | −0.61% to +0.09% | faster |
| hc | eeg_S006 / S007 / S008 | 1.32 – 1.34 | −0.01% to −0.10% | ≈ +1 ms each |
| hc | owid_total_cases | 1.295 | −5.02% | +771 µs |
| hc | owid_new_cases | 1.290 | **+0.87%** | +657 µs |
| hc | backblaze_smart_9 | 0.904 | −0.56% | faster |
| hc | other Backblaze, owid_total_deaths | 0.99 – 1.01 | −0.39% to −2.00% | ≈ 0 |

Observations (not tested hypotheses):

1. The decode cost is concentrated in a few files, and is **not proportional to the size gain**. In hc, the three EEG
   files pay about 1.33× for ≤ 0.10% smaller frames; `owid_new_cases` is both larger and 1.29× slower.
2. Several files decode **faster** with the candidate (up to 0.90×), so planes are not inherently slower to decode.
3. The cause is not attributed here: dtime records time per frame, not per leaf coder. Plausible mechanisms
   (more, smaller leaves; a different coder chosen per plane at hc) are unverified.

## Consequences for the register

- ST-1 stays provisional in size. Its decode cost is confirmed and **binding** in hc and on some fast files.
- ST-2 (context gate) changes status: it was "not needed as a size gate". Now it matters as a **decode-cost gate**.
  A rule that keeps the control leaf when the per-block size gain is small would have removed the worst hc cost (EEG)
  and the one hc loss (`owid_new_cases`) on these files. That is a hindsight observation on E02G data: it must be
  developed on development data and validated on fresh sources, never tuned here.
