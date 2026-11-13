# P2-E23: preregistered fresh check of the zstd-12 table frame (frozen before any new TidyTuesday file is read)

**Why.** In P2-E22, the secondary pipeline `tt_zstd12` fit OpenZL `csv`-9's archive cost and was smaller on 17/23
diverse fresh CSVs. It was secondary, so that is not a validated claim. This run makes `tt_zstd12` the **primary**
on files not read before.

`tt_zstd12` is the production table graph (`kg_encode_table`) with leaves coded by production zstd at level 12. It is
written as a standard `CZN1` frame by the research encoder `P2-E21/ttcli.c` and decoded by the production CLI.

## Inputs (`prepare_e23.py`)

- Same repository and commit as P2-E22 (`rfordatascience/tidytuesday` @ `54ea581e…`). This is the same aggregator
  repository, but **disjoint datasets**: every weekly directory P2-E22 size-checked (42) is excluded, as are
  P2-E22's exclusion terms.
- Selection by path only, with salt `p2e23|`:
  - one file per weekly directory;
  - directories ordered by hash;
  - files of 100,000–5,000,000 bytes;
  - the first 24 kept.

## Policies, boundary and analysis

Policies and boundary are identical to P2-E21/E22. The analysis (`analyze_e23.py`) is identical to P2-E22's except
for the primary.

**Primary.** `tt_zstd12` fits OpenZL `csv`-9's archive cost on every pair and is smaller. **Pass** if the one-sided
95% Clopper–Pearson lower bound exceeds 0.5 in both runs. Inputs where OpenZL fails are excluded, and fewer than 12
qualifying inputs means **inconclusive**. Everything else is reported only.

Budget: timeout 60 minutes, started before 04:00 UTC. Development research only, with no acceptance, default or
format change.
