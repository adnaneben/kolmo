# P2-E22: diverse fresh confirmation for text-heavy CSV archive (frozen before any TidyTuesday data is read)

**Why.** P2-E21's preregistered fresh check passed (`tt_bsc0` 23/24), but 21 of its 24 fresh files came from one
t-SNE generator. This run repeats the same primary test on a source chosen for diversity: **one file per dataset**.

## Source

| Field | Value |
|---|---|
| Repository | `rfordatascience/tidytuesday` @ `54ea581e1352f6c55b9343feb77df0fc29c78621` |
| Prior use | not referenced anywhere in the repository before this commit |
| Licence | per dataset, as stated in each week's readme; public research use. Inputs are not committed; hashes are recorded |

## Selection (`prepare_e22.py`, paths only)

- Candidates are `data/YYYY/YYYY-MM-DD/*.csv` files whose lower-cased path contains none of the frozen exclusion
  terms in `prepare_e22.py`. Those terms cover every earlier source and every P2-E20/E21 dataset name.
- Take **one file per weekly dataset directory**, choosing the smallest `sha256('p2e22|' + path)`.
- Order directories by `sha256('p2e22|' + dir)`.
- Keep files of 100,000–5,000,000 bytes until 24 are kept.

## Policies and boundary

These are identical to P2-E21 (`run_e22.py` is byte-identical apart from its docstring):

| Role | Pipelines |
|---|---|
| Rival | OpenZL `csv`-9 |
| Reference | `zstd_3` |
| Production | `kolmo` fast, eco, hc |
| Production-format frames (decoded by the production CLI) | `tt_bsc0`, `tt_zstd12`, `tt_brotli9` |

N1 harness unchanged; two runs; `confirm`.

## Decision (`analyze_e22.py`, the P2-E21 analysis with fresh ids `TT__*`)

- **Primary.** `tt_bsc0` fits OpenZL `csv`-9's archive cost on every pair and is smaller on the fresh files.
  **Pass** if the one-sided 95% Clopper–Pearson lower bound exceeds 0.5 in both runs. Inputs where OpenZL fails are
  excluded and listed.
- If fewer than 12 files qualify, the result is reported as **inconclusive** whatever the bound.
- Secondary, reported only: `tt_zstd12`, `kolmo -m eco`.

Budget: timeout 60 minutes, started before 04:00 UTC (07:00 Riyadh). Development research only, with no acceptance,
default or format change.
