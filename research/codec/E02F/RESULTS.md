# E02F results — per-plane KG_INT leaves on multi-byte residuals

**Registered outcome: H1 (fast) PROVISIONAL, H2 (hc) PROVISIONAL — each met in one family only (Wikimedia pageviews).**
Rules, sources and thresholds: [`PREREGISTRATION.md`](PREREGISTRATION.md), applied by `decide.py` unchanged.

## Execution

| | |
|---|---|
| Run | GitHub Actions **43260123241** (`e02f-validation.yml`), head `bdc1de1`, ubuntu-latest, AMD EPYC 9V45, one thread |
| Registration | committed and pushed (`e0989f0`) before the workflow existed; the run started from the next commit |
| Downloads | all registered files (4 EDF, 8 pageview JSON, FSDD zip); SHA-256 in `results/DOWNLOAD.sha256` |
| Inputs | 16 of 16 prepared, 0 missing entries |
| Frozen harness / production | `run.sh` verified `FROZEN.sha256` (identical to E02E's) and production identity with `2f3308a` |
| Checks inside the run | prepare unit tests, sanitizer selftest, golden vectors (two builds), Python reference; every complete input reconstructed for every configuration |
| Evidence | `results/` recovered from the job log, each file checked byte for byte against the run's own `MANIFEST.json` (`ARTIFACT_MANIFEST.json`); raw records `raw.jsonl.xz` (uncompressed SHA-256 `2ac796b0…`). `decide.py` re-run locally: identical decision |

**Calibration** passed: `exist+prod3` +0.004% vs production fast, `exist+prod19` +0.002% vs production hc.

**Phenotype** (files with INT blocks of output width ≥ 2): fast — EEG 4/4, pageviews 8/8, speech 3/4; hc — EEG 4/4,
pageviews 5/5 (3 pageview files had no INT block in hc), speech 3/4. All three families were exercised in both modes.

## Decisions as registered

| family | H1 fast: pooled | meets? | H2 hc: pooled | meets? |
|---|---|---|---|---|
| F1 EEG (4 × 1.25 MB) | +0.08% (worst +0.32%); decode +0.005 ns/B, ratio 1.003 | no (size) | −0.02%; decode +0.26 ns/B, large-file ratio **1.178** | no (size, decode) |
| F2 pageviews (8 / 5 × 14.7 KB) | **−9.57%**; decode −0.21 ns/B | **yes** | **−7.48%**; decode −0.34 ns/B | **yes** |
| F3 speech (3 × 2.8–4.1 MB) | −0.16% (worst −0.12%); ratio 0.996 | no (size) | −0.47%; ratio 0.841 | no (size, −0.47% > −0.50%) |
| **status** | | **provisional** | | **provisional** |

Pooled over every file with INT blocks: fast −0.15% (13,463 B of 8.81 MB), hc −0.37% (31,722 B of 8.63 MB).

## What the numbers say (descriptive)

- The phenotype condition worked as designed: multi-byte residuals were present in all three families, so the
  failures in EEG and speech are real negatives, not structural no-ops.
- **Gains follow sparse high planes, not multi-byte width.** Pageview counts (order 1, `ow` 2–4) gain −7.3% to −12.1%
  per file (fast). Dense signals with multi-byte residuals gain almost nothing (EEG −0.06% to +0.32%; speech −0.65% to
  0.00%). This matches E02E, where GHCN precipitation (mostly zero) gained −16% and temperatures little.
- Absolute savings on pageviews are small (about 400–800 B per 6 KB frame); the pooled saving across all files is
  0.15–0.37%.
- EEG hc decode ratio 1.18 on 1.25 MB files: an extra zstd -19 frame per block costs more than its bytes save there.

## Conclusion across E02E and E02F

Two preregistered validations, each **provisional in one family**. Per-plane residual leaves are **not supported as a
general KG_INT layout**, and the `KG_SPLIT` format change is not justified. The evidence points to a narrow phenotype
— residuals whose high byte planes are sparse (spiky counts, sparse precipitation) — where gains are 7–16%. Exploiting
it would need a per-block decision (a selector), which is new work, not a validation of this layout.

## Limitations

- Pageview files are small (14.7 KB) and eight articles share one API; EEG subjects share one protocol; speech is four
  speakers of one dataset.
- Shared CI runner timings.
- E02F's decode rule was set after E02E (stated in the registration); it did not decide any outcome here: pageviews pass
  either rule, and EEG/speech fail on size.
