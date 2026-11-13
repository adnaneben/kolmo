# P2-E21: archive mode for text-heavy CSV, with a preregistered fresh check (frozen before any plotly data is read)

**Motivation (exploratory, P2-E20 follow-up).** On the now-observed FiveThirtyEight CSVs, a local CLI timing
(`/tmp`-only, indicative) put the production table graph with BSC leaves (`tt_bsc0`) inside OpenZL `csv`-9's archive
cost and smaller on 18/23, at −18.9% bytes. These were written as genuine `CZN1` frames by the research encoder
`ttcli.c` and decoded by the production CLI. Production `kolmo -m eco` gave 17/23 and −4.8%. P2-E20 did not include
`eco`. Those numbers come from **development data**, which motivates the fresh check below.

`tt_bsc0` uses existing wire semantics only: a KG FIELDS/DECIMAL table program from production `kg_encode_table`, with
leaves coded by the existing `C_BSC` codec. Locally, ASan/UBSan/LSan `ttcli` gave 72/72 on the 24 CSVs and 6/6 on edge
CSVs, all decoded by production `kolmo -d`.

## Inputs (`prepare_e21.py`)

| Set | Inputs | How chosen |
|---|---|---|
| Development | the 24 FiveThirtyEight CSVs of P2-E20 | re-fetched and verified against P2-E20's recorded hashes |
| **Fresh** (preregistered) | first 24 kept | `plotly/datasets` @ `0c447c47b757ad74edecab31f0d72f849d2e67c2`, paths only. All `*.csv` files whose lower-cased path contains none of the frozen exclusion terms in `prepare_e21.py`, ordered by `sha256('p2e21|' + path)`, keeping files of 100,000–5,000,000 bytes |

The exclusion terms rule out every source used before in this repository. The plotly *package's* Gapminder dataset
was used in E02C, so any Gapminder path is excluded. The plotly datasets repository itself is not referenced.

## Policies (`run_e21.py`, frozen)

N1 harness unchanged; two runs (seeds 20261112 and 20261113); `confirm`.

| Role | Pipelines |
|---|---|
| Rival | `openzl_csv_9` |
| Reference | `zstd_3` |
| Production | `kolmo` fast, eco, hc |
| Production-format research frames (production CLI decodes) | `tt_bsc0`, `tt_zstd12`, `tt_brotli9` |

## Decision (`analyze_e21.py`, frozen)

- **Primary.** On the fresh plotly CSVs, `tt_bsc0` fits OpenZL `csv`-9's archive cost on every pair and is smaller.
  **Pass** if the one-sided 95% Clopper–Pearson lower bound on the success proportion exceeds 0.5 in both runs.
  Inputs where OpenZL fails are excluded from n and listed.
- **Secondary**, reported and not tested: `kolmo -m eco` and the other frames, and the development set.

Budget: timeout 60 minutes, started before 04:00 UTC. This is development research only, with no acceptance,
default or format change.
