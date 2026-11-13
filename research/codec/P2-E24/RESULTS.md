# P2-E24 results: production `kolmo -m eco` vs OpenZL csv-9 on 36 new TidyTuesday datasets — **PASS**

Run: Actions **43401044132** at `b77bba8`, 09:24–09:37 UTC (~13 runner-minutes).

Integrity:
- The frozen files' hashes on the runner equal the preregistration commit. The analysis reproduces from the committed
  records.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- Sanitizer gate: 108/108.
- `confirm`: IDENTITIES_MATCH, 287/288. OpenZL `csv`-9 failed on `regularite-mensuelle-tgv-aqst.csv`, which is
  excluded, so **n = 35**.

Inputs: 36 new weekly datasets (330 candidate weeks after excluding every P2-E22/E23 directory; 92 size-checked).

| Pipeline (fits OpenZL `csv`-9 archive cost on every pair and smaller) | Run 1 | Run 2 |
|---|---|---|
| **production `kolmo -m eco` (primary)** | **27/35** (Bonferroni LB 0.575; rule bytes −7.3%) | **27/35** |
| `tt_zstd12` (zstd-12 table frame) | 28/35 (−6.7%) | 28/35 |
| `tt_bsc0` (BSC table frame) | 20/35 (−14.7%) | 20/35 |
| `tt_brotli9` | 19/35 | 17/35 |
| production `kolmo fast` / `hc` | 4 / 2 | 4 / 2 |

**Decision: PASS.** In both runs the one-sided 98.75% Clopper–Pearson lower bound (Bonferroni over the four
sequential CSV tests) is 0.575, above 0.5. The unadjusted 95% bound also passes.

## What this supports, and what it does not

- **Supported (this boundary, P2 archive model):** Kolmo's existing production `eco` mode is usually smaller than
  OpenZL `csv`-9 and inside its archive cost on diverse real-world CSVs. The observed rate is 77% (27/35), about
  7% fewer bytes under the rule.
- This is the fourth test in a sequence, reported in full:

  | Test | Primary | Result |
  |---|---|---|
  | P2-E21 | `tt_bsc0` | pass, low-diversity sample |
  | P2-E22 | `tt_bsc0` | fail |
  | P2-E23 | `tt_zstd12` | fail |
  | **P2-E24** | `eco` | **pass** at the Bonferroni-adjusted bar |

- `eco` had already shown 13–14/23 and 18/24 as a secondary in P2-E22/E23. Across all three diverse fresh samples
  it is 58–59 of 82 (71–72%).
- Not supported: any default, release or format change (none is made), and any claim for other CSV regimes. Two are
  open: CR-only line endings (P2-E20), and the files where OpenZL's CSV profile errors.
