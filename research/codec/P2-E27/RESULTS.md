# P2-E27 results: cross-aggregator fresh test of the CSV picker — **PASS** (with a diversity caveat)

Run: Actions **43407137403** at `3667bf0`, 10:10–10:24 UTC (~14 runner-minutes).

Integrity:
- The frozen files' hashes on the runner equal the preregistration commit. The analysis reproduces from the
  committed records.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- Sanitizer gate: 56/56.
- `confirm`: IDENTITIES_MATCH, **168/168**; OpenZL succeeded on every file.

Inputs: 132 candidate dataset keys across the two new aggregators. Only **28** fell in the 100 KB–5 MB range: 27
from `selva86/datasets`, 1 from `datasciencedojo/datasets`.

| Pipeline (fits OpenZL `csv`-9 archive cost on every pair and smaller) | Run 1 | Run 2 |
|---|---|---|
| **`pick` (primary)** | **26/28** (LB at alpha 0.05/6 = 0.726; rule bytes −16.1%) | **26/28** |
| production `kolmo -m eco` | 26/28 (−16.6%) | 26/28 |
| `tt_zstd12` | 26/28 (−16.0%) | 26/28 |
| production `kolmo fast` | 12/28 | 12/28 |

**Decision: PASS** in both runs (Bonferroni over six sequential CSV tests). n = 28 ≥ 18, so not inconclusive.

## Caveats (stated plainly)

- **Effective diversity is lower than 28.** The normalised-stem rule did not merge train/test or variant files of one
  dataset. Pairs present: `adult_train`/`adult_test`, `bank_marketing_train`/`_test`, `har_train`/`_validate`,
  `Churn_Modelling`/`_m`, `norway_new_car_sales_by_model`/`_clean`/`_by_make`, `orders_45d`/`olist_orders_45d`.
  That leaves about 20 distinct datasets. Even counting one file per dataset, the picker wins on about 18–19 of
  them.
- The second aggregator contributed one file, so this is effectively one new aggregator.
- On this sample, the picker, `eco` and the table frame tie (paired `pick` − `eco` = 0). The picker's advantage over
  `eco` seen in P2-E26 (+3/+4) did not appear here; its pass comes from both branches already beating OpenZL.

## Cumulative CSV picture (fresh, preregistered)

| Test | Primary | Result |
|---|---|---|
| P2-E24 | `eco` | 27/35 PASS |
| P2-E26 | picker | 33/35 PASS |
| P2-E27 | picker | 26/28 PASS (new aggregator) |

Earlier failures, P2-E22 and P2-E23, are kept in the record. The picker is never worse than `eco` and sometimes
better.
