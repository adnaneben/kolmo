# P2-E26 results: CSV picker vs OpenZL csv-9 on 36 new TidyTuesday datasets — **PASS**

Run: Actions **43403830702** at `0870527`, 09:44–09:55 UTC (~11 runner-minutes).

Integrity:
- The frozen files' hashes on the runner, including `pickcli.c`, equal the preregistration commit. The analysis
  reproduces from the committed records.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- Sanitizer gate (`tt_zstd12` and `pick`, production decode): 72/72.
- `confirm`: IDENTITIES_MATCH, 215/216. OpenZL `csv`-9 failed on `week7_starwars.csv`, which is excluded, so
  **n = 35**.

Inputs: 238 candidate weeks after excluding every P2-E22/E23/E24 directory; 83 size-checked; 36 kept.

| Pipeline (fits OpenZL `csv`-9 archive cost on every pair and smaller) | Run 1 | Run 2 |
|---|---|---|
| **`pick` (primary)** | **33/35** (Bonferroni LB 0.781; rule bytes **−16.0%**) | **33/35** |
| production `kolmo -m eco` | 29/35 (−13.1%) | 30/35 |
| `tt_zstd12` | 26/35 (−10.6%) | 26/35 |
| production `kolmo fast` | 14/35 | 14/35 |

**Decision: PASS** in both runs. The one-sided 99% lower bound is 0.781 > 0.5, Bonferroni-adjusted over the five
sequential CSV tests. The unadjusted bound also passes.

Paired comparison (reported, not tested): `pick` − `eco` = **+4 / +3** inputs.

## Supported claim (this boundary, P2 archive cost model)

The near-free picker, every output of which is decoded by the unmodified production CLI, is smaller than OpenZL
`csv`-9 and inside its archive cost on 33/35 unseen real-world CSV datasets, with 16% fewer bytes under the rule. It
does better than production `eco` alone.

The picker sends regular CSVs (first-64-KiB line regularity > 0.9916) to the production table program with zstd-12
leaves, and everything else to production `eco`.

## Limitations

- The rule was tuned on TidyTuesday and FiveThirtyEight data. These files are unseen datasets from the same
  aggregator.
- The CSV test sequence (P2-E21–E26) is reported in full in `P2-SUMMARY`.
- OpenZL's `csv` profile errors on some real files (1 of 36 here); those are excluded, not counted as wins.
- No default, format or release change. Wiring the picker into the production encoder would need authorization.
