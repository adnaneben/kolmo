# P2-E22 results: diverse fresh TidyTuesday CSVs, primary **FAIL**

Run: Actions **43357063053** at `81388fe`, 03:09–03:23 UTC (~13 runner-minutes).

Integrity:
- The frozen files' hashes on the runner equal the preregistration commit.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- Sanitizer gate: **72/72**.
- `confirm`: IDENTITIES_MATCH, 191/192. OpenZL `csv`-9 failed on `dod_tbi_2006.csv`, which is excluded, so n = 23.

Inputs: 24 CSVs from 24 different TidyTuesday weekly datasets (414 candidate weeks; 42 size-checked). Examples:
IKEA, surveys, crop yields, movies, tweets, R downloads, F1 standings, CalFire, rocket launches, horror movies, WHO
TB, bird counts, CRAN packages.

| Pipeline (fits OpenZL `csv`-9 archive cost and smaller; rule bytes vs OpenZL-9) | Run 1 | Run 2 |
|---|---|---|
| **`tt_bsc0` (primary)** | **12/23** (LB 0.335; −14.3%) | **12/23** (LB 0.335) |
| `tt_zstd12` (secondary) | 17/23 (LB 0.549; −7.5%) | 17/23 |
| production `kolmo -m eco` | 13/23 (−6.9%) | 14/23 (−7.7%) |
| `tt_brotli9` | 11/23 | 11/23 |
| production `kolmo fast` / `hc` | 2 / 0 | 2 / 0 |

## Reading

- On genuinely diverse fresh CSVs, the BSC-leaf table frame is smaller than OpenZL wherever it fits (−14% overall
  under the rule). But it fits OpenZL's archive cost on only about half, so the P2-E21 pass did not generalize: its
  fresh set was mostly one generator.
- The zstd-12 table frame and production `eco` each fit and are smaller on 57–74%. `tt_zstd12` would have cleared
  the bar, but it was secondary here, so that is **not** a validated claim. It would need its own preregistered
  check on new files.
- Overall picture for text-heavy CSV vs OpenZL `csv`-9: Kolmo can be clearly smaller (by 5–20%). Staying inside
  OpenZL's archive cost needs the cheaper leaves (zstd-12) or `eco`, and the margin is input-dependent.
