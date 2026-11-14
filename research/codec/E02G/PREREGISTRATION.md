# E02G preregistration — stacked per-plane pipeline (planes + sparse-leaf guard) on count/telemetry data

Registered 2026-11-12, before any E02G data was downloaded, parsed or compressed. Validates the stack ST-1 with the
ST-2 phenotype from `../STACKING.md`, on the production-connected boundary (complete Kolmo frames). E02G is a
validation set, not a final holdout. Earlier decisions (E02E, E02F, R2-stack) are not revised by it.

## Frozen pipeline and comparators

- **Fast profile:** candidate `planes+prod3g` — one residual leaf per byte plane, production's fast leaf rule per leaf,
  plus the sparse-leaf guard (a leaf ≥ 256 KiB that the rule sends to Huffman-only is also coded with zstd -3; the
  smaller is kept, both charged). Control `exist+prod3` (calibrated equal to production fast frames).
- **hc profile:** candidate `planes+prod19`; control `exist+prod19`. The guard is not applicable at hc (no sampling
  shortcut there).
- Also reported: `planes+prod3` (no guard), to measure the guard's marginal contribution inside the stack. Other
  harness configurations are exploratory.
- Code: production `2f3308a` unchanged; harness files frozen by content hash (`FROZEN.sha256`, identical to E02E/E02F);
  `run.sh` refuses to run on any difference. Branch `fix/planner-record-ownership-isolated-20261112` only; PR #7 draft.
- Boundary: complete-planner Kolmo frames with only the KG_INT residual leaf replaced (experimental leaf framing; not
  CZN1, and not the parallel workstream's E2C1 envelope). Per-plane leaves would need a separately reviewed format
  change to integrate (`PROVISIONAL.md` P-01).

## Sources (fixed; no substitution)

Checked at registration against every repository benchmark/holdout manifest and protocol (`benchmark/`), and against
the observed-corpora list (`PROVISIONAL.md` D-02). NYC TLC, GH Archive, Divvy, CitiBike-style bike-trip data and every
earlier family were excluded for that reason.

| family | role | publisher file | streams |
|---|---|---|---|
| F1 Backblaze drive stats | target | `https://f001.backblazeb2.com/file/Backblaze-Hard-Drive-Data/data_Q1_2023.zip`; the first `.csv` member in sorted name order (one day of all drives) | `capacity_bytes`, `smart_9_raw` (power-on hours), `smart_241_raw` (LBAs written), `smart_242_raw` (LBAs read), `smart_5_raw` (reallocated sectors), `smart_197_raw` (pending sectors) |
| F2 Our World in Data COVID-19 | target | `https://raw.githubusercontent.com/owid/covid-19-data/master/public/data/owid-covid-data.csv` | `total_cases`, `new_cases`, `total_deaths`, `new_deaths`, `total_vaccinations`, `people_vaccinated` |
| C1 dense multi-byte control | non-target regression control | PhysioNet eegmmidb 1.0.0 `S005R01.edf` … `S008R01.edf` (database already observed in E02F; these subjects not) | per file, all data signals, channel-major int16 (E02F's conversion) |
| C2 single-byte control | no-op control | the F1 CSV member | `smart_194_raw` (temperature) |

Conversion (`prepare.py`, unit-tested on synthetic fixtures before any download): CSV fields in file row order; blank
tokens skipped (counts recorded); every kept token parsed with `Decimal` and required to be an exact integer within
int64; written as little-endian int64 `.npy` v1.0. No sorting, scaling, rounding, resampling, filling or outlier
editing. EDF as in E02F. A missing field or file is reported as a coverage gap, never replaced. SHA-256 of every
download (and of the chosen zip member) and every derived input is recorded before the first codec call.

**Exposure.** After this run, these files become observed: Backblaze Q1 2023 (the whole archive, as one family), the
OWID COVID-19 file, and eegmmidb S005–S008. The parallel workstream's planned native-compatible validation (R2-stack
`NEXT_TASK.md`) must not use them as fresh data.

## Protocol

`fetch.sh DIR`, then `run.sh DIR OUT`: frozen-hash and production-identity checks; prepare tests; build;
`make -f research/codec/E02C-harness/Makefile all check`; `prepare.py`; `run_matrix.py --reps 7 --seed 20261117`;
`analyze.py`; `decide.py`. One thread. Regime B (complete planner) is primary.

**Calibration gate** (voids the run on failure): pooled over all files with INT blocks, `exist+prod3` and `exist+prod19`
each within +0.10% of the production fast and hc frames.

## Eligibility (ST-2 phenotype), per mode

A file is **eligible** if its production frame has at least one INT block with residual output width ≥ 4, or with
predictor order 0 and width ≥ 2. A target family with fewer than 2 eligible files in a mode is **not exercised**.
Control files are evaluated whatever their phenotype.

## Hypotheses and decision rules

- **H1 (fast):** `planes+prod3g` vs `exist+prod3` on eligible target files.
- **H2 (hc):** `planes+prod19` vs `exist+prod19` on eligible target files.
- **H3 (guard within the stack, descriptive):** guard firings (leaves changed) and bytes saved, `planes+prod3g` vs
  `planes+prod3`, over all files.

A target family **meets** H1/H2 when, over its eligible files:
1. pooled complete-input bytes change ≤ **−0.50%**;
2. no eligible file ≥ 64 KiB is larger by more than **+1.00%**;
3. added median sequential decode time ≤ **0.50 ns per input byte**, and the pooled decode ratio over files ≥ 256 KiB is
   ≤ **1.05**.

**Control check:** pooled over each control family (all its files), the candidate must not be larger than its control by
more than **+0.50%**. A control failure caps the hypothesis at **provisional**, whatever the target families show.

| status | requirement |
|---|---|
| **Supported** | both target families meet, and no control failure |
| **Provisional** | exactly one target family meets, or both meet with a control failure |
| **Not supported** | ≥ 1 target family exercised, none meets |
| **Inconclusive** | no target family exercised, or calibration failed |

Reported regardless: family and pooled totals, per-file changes, change against unmodified production, eligible and
ineligible files separately, encode cost, decode times, and typed-regime results. A Supported result is a research
signal only: no format change, default, selector or release follows.

## Integrity

No outcome-driven change to sources, conversion, eligibility, configurations, thresholds or statistics after data is
read. Infrastructure fixes are versioned with their effect stated. Raw records and hashes are retained; downloads and
derived inputs are not committed.
