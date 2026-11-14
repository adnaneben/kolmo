# P2-E24: preregistered fresh test of production `kolmo -m eco` vs OpenZL csv-9 (Option A, user-approved 12:23 Riyadh)

**Context.** This is the **fourth sequential** preregistered fresh CSV test:

| Test | Primary | Result |
|---|---|---|
| P2-E21 | `tt_bsc0` | pass, but the sample was mostly one generator |
| P2-E22 | `tt_bsc0` | 12/23, fail |
| P2-E23 | `tt_zstd12` | 12/24, fail |
| **P2-E24** | production `kolmo -m eco` | this test |

In P2-E22/E23, `eco` was the best secondary pipeline, at 31–32 of 47 pooled. Choosing the primary after looking at
earlier secondaries inflates false positives. This test therefore uses a **Bonferroni-adjusted bar for 4 tests**
(alpha = 0.0125) and more inputs (n = 36). The whole sequence is reported with the result.

## Inputs (`prepare_e24.py`)

- Same TidyTuesday commit as P2-E22/E23. Every weekly directory size-checked by P2-E22 or P2-E23 is excluded, so no
  file seen before is reused.
- Selection by path only, with salt `p2e24|`:
  - one file per weekly dataset directory;
  - files of 100,000–5,000,000 bytes;
  - the first **36** kept.

## Policies and boundary

Identical to P2-E21–E23 (`run_e24.py`, docstring aside): N1 harness unchanged, two runs, `confirm`.

| Role | Pipelines |
|---|---|
| Rival | OpenZL `csv`-9 |
| Reference | `zstd_3` |
| Production | `kolmo` fast, **eco**, hc |
| Production-format frames | `tt_bsc0`, `tt_zstd12`, `tt_brotli9` |

## Decision (`analyze_e24.py`)

- **Primary.** Production `kolmo -m eco` fits OpenZL `csv`-9's archive cost on every pair and is smaller. **Pass** if
  the one-sided **98.75%** Clopper–Pearson lower bound on k/n exceeds 0.5 in **both** runs.
- With fewer than 18 qualifying inputs (OpenZL failures excluded), the result is inconclusive.
- The unadjusted 95% bound is reported alongside; it does not decide.
- Everything else is reported only.

A pass supports one claim on this boundary: Kolmo's existing `eco` mode is usually smaller than OpenZL `csv`-9 within
its archive cost on diverse real-world CSVs. No default, format or release change follows from it.
