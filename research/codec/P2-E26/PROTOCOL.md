# P2-E26: preregistered fresh test of the CSV picker (frozen before any new TidyTuesday file is read)

## Rule under test

The rule was found on development data in P2-E25 (`selector_csv.py`, using runner records of P2-E21–E24 only;
105 CSVs). `pickcli` computes one near-free feature: the **line regularity** of the first 64 KiB, meaning the share of
complete lines whose comma count equals the modal count.

| Regularity | Encoder used |
|---|---|
| > 0.9916 | production table program with zstd-12 leaves (P2-E21 `tt_zstd12`, `CZN1` frame) |
| otherwise, or if the table encoder declines | production `kolmo -m eco -T1` |

Every output is decoded by the unmodified production `kolmo -d`.

Development evidence (P2-E25):
- Leave-one-experiment-out, the rule fit OpenZL `csv`-9's archive cost and was smaller on **82/105**, against 75
  for `eco` alone; the per-file oracle reaches 96.
- Locally, `pick`'s bytes equal `tt_zstd12` on 54/55 table-branch files and CLI `eco` on 53/53 eco-branch files. The
  one exception is the CR-only file, where the table encoder declines and `eco` is used, by design.

## Inputs (`prepare_e26.py`)

- Same TidyTuesday commit. Every weekly directory size-checked by P2-E22, P2-E23 or P2-E24 is excluded.
- Selection by path only, with salt `p2e26|`:
  - one file per week;
  - files of 100,000–5,000,000 bytes;
  - the first 36 kept.
- Limitation, stated in advance: the rule was tuned on TidyTuesday and FiveThirtyEight files. The new files are
  unseen datasets from the same aggregator, not a new aggregator.

## Policies

These are frozen in `run_e26.py`, with the N1 harness unchanged, two runs and `confirm`.

| Role | Pipelines |
|---|---|
| Rival | OpenZL `csv`-9 |
| Reference | `zstd_3` |
| Production | `kolmo` fast, eco |
| Research frames | `tt_zstd12`, `pick` |

## Decision (`analyze_e26.py`)

- **Primary.** `pick` fits OpenZL `csv`-9's archive cost on every pair and is smaller. **Pass** if the one-sided 99%
  Clopper–Pearson lower bound (Bonferroni over **five** sequential CSV tests) exceeds 0.5 in both runs. Fewer than 18
  qualifying inputs means inconclusive.
- Reported, not tested: the paired count difference `pick` − `eco`, and every other pipeline.

Development research only, with no acceptance, default or format change.
