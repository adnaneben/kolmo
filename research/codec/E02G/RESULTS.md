# E02G results — stacked per-plane leaves + sparse-leaf guard on count/telemetry data

**Registered outcome: H1 (fast) PROVISIONAL, H2 (hc) PROVISIONAL.** The candidate is smaller in **all four** target ×
mode cells (−0.67% to −2.00%). Each mode has one family that misses only the large-file decode-ratio bar (≤ 1.05×).
Controls passed. The guard amplifier (ST-1) reproduced on fresh data. Rules: [`PREREGISTRATION.md`](PREREGISTRATION.md),
applied by `decide.py` unchanged.

## Execution

| | |
|---|---|
| Run | GitHub Actions **43272770322** (`e02g-validation.yml`), head `dd7b241`, ubuntu-latest, AMD EPYC 9V74, one thread |
| Registration | pushed (`343096a`) before the workflow existed |
| Downloads | Backblaze `data_Q1_2023.zip` (819 MB; member `data_Q1_2023/2023-01-01.csv`), OWID `owid-covid-data.csv` (98 MB), eegmmidb S005–S008 R01; SHA-256 in `results/DOWNLOAD.sha256` and `results/manifest.json` |
| Inputs | 17 of 17 prepared, none missing |
| Checks inside the run | frozen-harness and production-identity checks, prepare tests, sanitizer selftest, golden vectors, Python reference; every complete input reconstructed for every configuration |
| Evidence | `results/` recovered from the job log, every file byte-identical to the run's own manifest; raw `raw.jsonl.xz` (uncompressed SHA-256 `bc0e059f…`); local re-decision identical |

**Calibration** passed: +0.007% fast, +0.004% hc. **Eligibility:** 3 of 6 files per target family in both modes. The
planner chose no INT graph for the others (e.g. `capacity_bytes`, `smart_5_raw`, `new_deaths`), so they are unchanged.

## Decisions as registered

| family | H1 fast: `planes+prod3g` vs `exist+prod3` | H2 hc: `planes+prod19` vs `exist+prod19` |
|---|---|---|
| F1 Backblaze | −1.51%, worst −0.55%; decode +0.064 ns/B, **large-file ratio 1.075** → misses | **−1.17%**, worst −0.39%; ratio 0.959 → **meets** |
| F2 OWID | **−0.67%**, worst +0.09%; ratio 0.977 → **meets** | −2.00%, worst +0.86%; decode +0.128 ns/B, **large-file ratio 1.180** → misses |
| C1 dense EEG (control) | −0.33% → ok | −0.03% → ok |
| C2 single-byte (control) | 0.00% → ok | 0.00% → ok |
| **status** | **provisional** | **provisional** |

## H3 — guard inside the stack (descriptive)

The guard fired on two EEG control files (S006, S008: mixed 1- and 2-byte blocks whose split planes became sparse
leaves ≥ 256 KiB). Without it, per-plane leaves were **+6.91% and +14.42%** on those files and **+1.76%** pooled over
every INT file in fast mode; with it, **−0.73%** pooled and 0.00% on both files (98,204 B saved). This is the ST-1
amplifier from development data, reproduced on sources it was not tuned on. Per-plane leaves must not ship without it.

## Descriptive observations (not new rules)

- **The parameter phenotype protects dense data's decode.** hc, EEG: size −0.01% to −0.10%, sequential decode
  2.6–2.7 → 3.6 ms (+33%). EEG is not eligible under ST-2, so an encoder applying the phenotype as a gate would skip it.
- **The decode misses are near the bar, on a timer that is not noise-controlled.** Absolute added decode is small
  (0.06–0.13 ns per input byte). R2-stack measured identical-frame timing spreads up to 2.5× on shared runners, and
  E02G's sequential decode loops have no identical-frame control. The registered verdict stands; whether the misses are
  real needs a separate, noise-controlled measurement of these exact frames.
- **The planner decides eligibility as much as the data does.** Half the target files never reach the INT graph
  (ST-7).

## Across E02E, E02F and E02G (no pooling)

| validation | fast | hc | limiting factor |
|---|---|---|---|
| E02E | provisional (1 family) | not supported | MIT-BIH single-byte; GHCN hc decode 1.105× |
| E02F | provisional (1 family) | provisional (1 family) | EEG/speech size ≈ 0 |
| **E02G** | **provisional — size met in both families** | **provisional — size met in both families** | **one decode ratio per mode (1.075×, 1.18×)** |

With the guard and the phenotype, the size criterion now holds in every exercised target family. The remaining question
is decode cost on large eligible files, which needs noise-controlled timing before any further validation.

## Limitations

Two target families, three eligible files each; shared CI runner; decode timing as above; OWID is a single CSV
(columns correlated); Backblaze one day of one quarter.
