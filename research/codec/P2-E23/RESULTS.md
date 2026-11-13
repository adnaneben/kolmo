# P2-E23: NOT RUN (build step hung; cancelled by its owner; no inputs were read)

Actions **43358797920** at `4c78ea9` started at 03:25 UTC. The step "Build pinned rivals, Kolmo and ttcli" never
finished: after 47 minutes it was still in `gcc`/`cc1`, where the identical step took about 5 minutes in P2-E21 and
P2-E22. The cause is unknown. The log has no compiler error; `cancelled-run-log-tail.txt` holds the end of the
step.

This session cancelled its own run at 04:12 UTC to stop it wasting runner minutes; nobody else's job was touched.
It used about 47 runner-minutes and produced no result.

- **The input selection step never ran.** No new TidyTuesday file was read, so the preregistration (`PROTOCOL.md`,
  `prepare_e23.py`, `analyze_e23.py`) remains valid for a future run.
- It was **not relaunched**, because of the overnight plan's 04:00 UTC (07:00 Riyadh) cut-off for new jobs.
- The claim that `tt_zstd12` beats OpenZL `csv`-9 within cost stays **unvalidated**. P2-E22 gave 17/23 there, but as
  a secondary result.

## Re-run (user-approved, 13 Nov 09:46 Riyadh): **completed; primary FAIL**

Actions **43381239112** at `b20835b`, 06:48–06:58 UTC (~10 runner-minutes).

What changed: only the workflow's infrastructure. Each build command got `timeout 900` and the build step a 25-minute
limit, plus a step that prints the build logs if the build fails. The frozen protocol, selection and analysis files
were unchanged, and their runner hashes equal the preregistration commit. The build finished normally, so the
earlier hang was runner-specific: locally the same compiles take 23 s and 13 s.

Inputs and checks:
- 24 new TidyTuesday datasets: 372 candidate weeks after excluding P2-E22's 42; 42 size-checked.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- Sanitizer gate: 72/72.
- `confirm`: IDENTITIES_MATCH, **192/192** (OpenZL succeeded on every file this time).

| Pipeline (fits OpenZL `csv`-9 archive cost and smaller; rule bytes vs OpenZL-9) | Run 1 | Run 2 |
|---|---|---|
| **`tt_zstd12` (primary)** | **12/24** (LB 0.319; −4.5%) | **12/24** |
| production `kolmo -m eco` | 18/24 (LB 0.565; −11.3%) | 18/24 |
| `tt_bsc0` | 16/24 (−13.6%) | 15/24 (−12.3%) |
| `tt_brotli9` | 8/24 | 7/24 |
| production `kolmo fast` / `hc` | 7 / 2 | 7 / 1 |

Decision: **FAIL** (12/24; the lower bound of 0.319 is below 0.5).

## Sequence so far

Three successive preregistered fresh CSV tests:

| Test | Primary | Result |
|---|---|---|
| P2-E21 | `tt_bsc0` | pass, but the sample was mostly one generator |
| P2-E22 | `tt_bsc0` | 12/23, fail |
| P2-E23 | `tt_zstd12` | 12/24, fail |

Each test's winning *secondary* pipeline did not hold up as the next test's primary. Rankings among these
near-boundary pipelines vary from sample to sample, so promoting the best secondary to primary is unreliable.
Across the two diverse fresh samples (exploratory pooling, 47 files):

| Pipeline | Fits and smaller |
|---|---|
| production `eco` | 31–32/47 |
| `tt_zstd12` | 29/47 |
| `tt_bsc0` | 27–28/47 |

No fixed pipeline has been validated as beating OpenZL `csv`-9 within cost on diverse real-world CSVs.
