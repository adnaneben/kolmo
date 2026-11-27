# P2-E21 results: text-heavy CSV archive vs OpenZL csv-9 (development + preregistered fresh plotly check)

Run: Actions **43355382170** at `b718bbf`, 02:54–03:07 UTC (~12 runner-minutes).

Integrity:
- The frozen files' hashes on the runner equal the preregistration commit.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`. The frozen analysis reproduces from the
  committed records.
- Sanitizer gate (ASan/UBSan/LSan `ttcli`, production `kolmo -d`, `cmp`): **144/144**.
- `confirm`: **IDENTITIES_MATCH**, 383/384. The one failure is OpenZL `csv`-9 on the CR-only `classic-rock` CSV,
  excluded from n.

## Preregistered primary: `tt_bsc0` on fresh plotly/datasets CSVs — **PASS**

`tt_bsc0` is the production table graph (`kg_encode_table`) with leaves coded by the existing BSC codec, written as a
standard `CZN1` frame and decoded by the unmodified production CLI. It fits OpenZL `csv`-9's archive cost on every
pair and is smaller on **23/24 in both runs**. The one-sided 95% lower bound is 0.817 against a 0.5 bar. Under the
rule, the byte total is **−15.3%** vs OpenZL-9.

| Pipeline (fits and smaller; rule bytes vs OpenZL-9) | Fresh plotly, 24 (run 1 / run 2) | Development FiveThirtyEight, 23 (run 1 / run 2) |
|---|---|---|
| `tt_bsc0` (primary) | **23 / 23** (−15.3%) | 17 / 16 (−20.1%) |
| `tt_zstd12` (table graph + zstd-12 leaves) | 24 / 24 (−13.4%) | 16 / 16 (−10.4%) |
| `tt_brotli9` | 14 / 14 | 10 / 11 |
| production `kolmo -m eco` | 4 / 4 (−0.8%) | 17 / 17 (−4.8%) |
| production `kolmo` fast, hc | 0 / 0 | 0 / 0 |

### Important limitation: low source diversity in the fresh set

The frozen plotly selection rule had no per-directory cap. **21 of the 24 fresh files are t-SNE embedding outputs
from one generator** (`dash-sample-apps/dash-tsne/…/data.csv`), which are near-identical numeric CSVs. Only three
come from other sources:

| File | `tt_bsc0` | `tt_zstd12` | `kolmo eco` |
|---|---|---|---|
| `_3d-line-plot.csv` | −16.9%, fits | −16.7%, fits | +3.3% |
| `df_IA_lat_lon.csv` | −5.7%, **over cost** | −5.1%, fits | −5.3%, fits |
| `olympic-winners.csv` | −20.4%, fits | −8.8%, fits | −6.5%, fits |

The preregistered test passed as written. However, its evidential weight is about **four distinct sources, not 24**.
Together with the development set (16–17/23 over 23 different FiveThirtyEight datasets), the honest summary is:

> The table graph with BSC (or zstd-12) leaves is usually smaller than OpenZL `csv`-9 within its archive cost on
> text-heavy CSVs. That is supported on development data from 23 sources plus a small fresh check. A
> diverse fresh confirmation, with one file per dataset, is still needed.

## Interpretation

- This is the first pipeline found that beats OpenZL's CSV profile within cost on real-world CSVs. It uses only
  production components (the table graph and the BSC codec) in today's frame format. What is new is the encoder's
  choice: production `eco`/`hc`/`max` pick other leaves, analysis or modes for these inputs.
- No production, default or format change. A production encoder that made this choice would need separate
  authorization.
