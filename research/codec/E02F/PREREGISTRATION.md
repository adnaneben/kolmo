# E02F preregistration — per-plane KG_INT leaves on multi-byte residuals

Registered 2026-11-12, before any E02F data was downloaded, parsed or compressed. E02F follows E02E
(`../E02E/RESULTS.md`), which was provisional in fast mode (one family) and not supported in hc mode. E02F is a new
experiment designed with knowledge of E02E's outcome; E02E's recorded decision stands unchanged and is not re-judged
here. E02F is a validation set, not a final holdout.

## What changed relative to E02E, and why (decided before E02F data)

1. **Phenotype condition.** E02E showed the layout is a no-op when the residual fits in one byte (MIT-BIH). E02F judges
   the candidate only on files where the unmodified production planner emits at least one INT-graph block whose
   residual output width is ≥ 2 bytes (`ow ≥ 2`). Single-byte files are reported but are not evidence either way.
2. **Sources** chosen from domain knowledge (signal amplitude, counts), not from any compression result, as likely to
   produce multi-byte residuals. Whether they actually do is measured; a family whose files fail the phenotype is
   **not exercised**, and no replacement source is sought.
3. **Decode-cost rule restated per input byte.** E02E's ratio rule penalised a fixed per-leaf cost on sub-millisecond
   files. E02F's rule (below) bounds added decode time per input byte and keeps a ratio bar for large files. This rule
   would have passed E02E's GHCN hc result; that is stated openly, and E02E's decision is not revised by it.

## Frozen code and isolation

- Branch `fix/planner-record-ownership-isolated-20261112` only; PR #7 stays draft; nothing else is touched.
- Production `2f3308a` (`src/kolmo.c` `e06467c8…`); `src vendor Makefile tests benchmark/accept/TARGETS.md` unchanged.
- Harness: the same frozen files as E02E, by content hash (`FROZEN.sha256`, identical to `../E02E/FROZEN.sha256`).
  `run.sh` refuses to run on any difference. Reporting scripts (`E02C-harness/run_matrix.py`, `analyze.py`) as at the
  commit of this registration.
- Decision configurations: `exist+prod3` → `planes+prod3` (fast), `exist+prod19` → `planes+prod19` (hc). Others are
  exploratory.

## Sources (fixed; no substitution)

None appears in earlier corpora on this branch (E02C-harness, parallel E02C, E02D, E02E) or in the repository's holdout
and benchmark manifests (checked at registration).

| family | publisher | exact files | derived inputs |
|---|---|---|---|
| F1 EEG | PhysioNet EEG Motor Movement/Imagery Database 1.0.0 | `https://physionet.org/files/eegmmidb/1.0.0/S00{1,2,3,4}/S00{1,2,3,4}R01.edf` | 4: per file, all data signals (excluding `EDF Annotations`) as int16, channel-major |
| F2 Pageviews | Wikimedia REST API, daily per-article views, en.wikipedia, all-access, agent `user`, 2016-01-01 to 2025-12-31 | articles `Earth, Moon, Mathematics, Water, London, Music, Albert_Einstein, Python_(programming_language)` | 8: one int32 series per article |
| F3 Speech | Free Spoken Digit Dataset, `https://github.com/Jakobovski/free-spoken-digit-dataset/archive/refs/heads/master.zip` | `recordings/{digit}_{speaker}_{index}.wav` for speakers jackson, nicolas, theo, yweweler | 4: per speaker, all recordings concatenated in (digit, index) order, int16 |

16 inputs. Conversion (`prepare.py`, unit-tested on synthetic fixtures before any download):
- **EDF**: parse the header (record count, signal count, labels, samples per record); data records are little-endian
  int16; for each non-annotation signal, its samples from every record in order; signals concatenated in header order.
  The data length must equal records × Σ samples × 2.
- **Pageviews**: the API's `items` in returned order; timestamps must be strictly increasing; views must be integers
  in [0, 2³¹ − 1]. Gaps are kept as gaps (no filling).
- **FSDD**: each WAV must be PCM mono 16-bit 8000 Hz; frames concatenated in the stated order; any missing index is
  recorded, not replaced.
- Every input becomes a NumPy `.npy` v1.0 file. No rounding, scaling, sorting, resampling or windowing. SHA-256 of every
  download and derived input is recorded before the first codec call. Coverage gaps are reported, not filled.

## Protocol

`fetch.sh DIR` then `run.sh DIR OUT`: verify frozen hashes and production identity; prepare unit tests; build;
`make -f research/codec/E02C-harness/Makefile all check`; `prepare.py`; `run_matrix.py --reps 7 --seed 20261115`;
`analyze.py`; `decide.py`. One thread. Regime B (complete planner) is primary; regime A (typed) is descriptive.

**Calibration gate** (voids the run on failure): pooled over all files with INT blocks, `exist+prod3` within +0.10% of
production fast frames and `exist+prod19` within +0.10% of production hc frames.

## Hypotheses and decision rules

Phenotype population, per mode: files whose production frame has ≥ 1 INT block with `ow ≥ 2`. A family with fewer than
2 phenotype files in a mode is **not exercised** in that mode.

- **H1 (fast)**: `planes+prod3` vs `exist+prod3`.
- **H2 (hc)**: `planes+prod19` vs `exist+prod19`.

A family **meets** a hypothesis when all of the following hold over its phenotype files:

1. pooled complete-input bytes change ≤ **−0.50%**;
2. no phenotype file of ≥ 64 KiB is larger by more than **+1.00%**;
3. decode: (sum of per-file median sequential decode times, candidate − control) ÷ (sum of the files' input bytes)
   ≤ **0.50 ns per input byte**; and, if the family has phenotype files ≥ 256 KiB, the pooled decode ratio over those
   files is ≤ **1.05**.

| status | requirement |
|---|---|
| **Supported** | ≥ 2 families meet |
| **Provisional** | exactly 1 family meets |
| **Not supported** | ≥ 1 family exercised, none meets |
| **Inconclusive** | no family exercised, or calibration failed |

Also reported: per-family and pooled totals, per-file changes, change against unmodified production, encode cost,
memory, the single-byte (non-phenotype) files, and typed-regime results. A Supported result is a research signal only:
it does not authorise a format change, default, selector or release; `KG_SPLIT` would need its own review and a fresh
holdout.

## Integrity

No outcome-driven change to sources, conversion, phenotype, configurations, thresholds or statistics after data is
read. Infrastructure fixes are versioned and their effect stated. Raw records and hashes are retained; downloads and
derived inputs are not committed.
