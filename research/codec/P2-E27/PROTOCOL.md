# P2-E27: cross-aggregator fresh test of the CSV picker (frozen before any file from these sources is read)

**Why.** P2-E26 passed (33/35), but the picker was tuned on TidyTuesday and FiveThirtyEight files and tested on new
TidyTuesday datasets. This test uses **two aggregators never used before**:

| Aggregator | Pinned commit | Prior use in the repository |
|---|---|---|
| `selva86/datasets` | `c943fa8…` | none |
| `datasciencedojo/datasets` | `f0ccab6…` | none |

## Selection (`prepare_e27.py`, paths only)

- **One file per dataset key**: a sub-directory, or the normalised stem for top-level files.
- Keys ordered by `sha256('p2e27|' + key)`.
- Keep files of 100,000–5,000,000 bytes, skipping any whose normalised stem was already kept from either repository,
  so the same public dataset is not counted twice.
- Up to 36 files.

## Policies and pipeline

Policies are identical to P2-E26 (`run_e27.py`). The picker is P2-E25's `pickcli.c`, unchanged:

| Role | Pipelines |
|---|---|
| Rival | OpenZL `csv`-9 |
| Reference | `zstd_3` |
| Production | `kolmo` fast, eco |
| Research frames | `tt_zstd12`, `pick` |

The N1 harness is unchanged; two runs; `confirm`.

## Decision (`analyze_e27.py`)

- **Primary.** `pick` fits OpenZL `csv`-9's archive cost on every pair and is smaller. **Pass** if the one-sided
  Clopper–Pearson lower bound at alpha = 0.05/6 (Bonferroni over **six** sequential CSV tests) exceeds 0.5 in both
  runs. Fewer than 18 qualifying inputs (OpenZL failures excluded) means **inconclusive**.
- Reported only: `pick` − `eco` and the other pipelines.

Development research only, with no acceptance, default or format change.
