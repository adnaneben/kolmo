# E02E preregistration — per-plane KG_INT residual leaves on source-disjoint data

Registered 2026-11-12, before any E02E data was downloaded, parsed or compressed, and before any E02D outcome was
inspected: at registration E02D had only its preregistration (`e6bf564`) and execution code (`2ad085f`) on the branch,
and none of its results were read. Only publisher documentation known in advance chose the sources. E02E is a
validation set, not a final holdout, and it stops being unseen once its results are read.

Sequence, for the record: the text of this registration was written while E02D had only its preregistration and
execution code on the branch. E02D's results commit (`91c4cda`, 2026-11-12 18:04 +03:00) arrived during registration;
its files (`E02D_RESULTS.md`, `E02D_EVIDENCE.json`) were not opened. The only edits after it arrived were the frozen
commit id and the content-hash note below; no source, conversion, configuration, threshold or rule changed.

## Question

Does coding the KG_INT residual stream as one leaf per byte plane — each leaf chosen by production's own leaf rule at
the production zstd level — shrink complete Kolmo output on new real integer data, without a decode penalty?
(Development result, `E02C-harness/RESULTS.md` §9: −1.94% fast, −0.64% hc on files with INT blocks.)

## Frozen code and isolation

- Writes only to `fix/planner-record-ownership-isolated-20261112`; PR #7 stays draft and unmerged; no other branch or PR
  is touched.
- Production code: `2f3308a` (`src/kolmo.c` blob `e06467c8…`); `src/`, `vendor/`, `Makefile`, `tests/`,
  `benchmark/accept/TARGETS.md` unchanged.
- Harness frozen at commit `1ed2a2b` (guard added in `122d5d0`, its selftest in `1ed2a2b`, both before any E02E data): `research/codec/E02C-harness/harness/{e02c.c, selftest.inc, reference.py,
  golden.txt}` with the SHA-256 values recorded in `FROZEN.sha256` next to this file. `run.sh` refuses to run if any
  differs. The content hashes are authoritative; commit ids can change if these unpushed commits are rebased before
  publication. Configurations used: `exist+prod3`, `planes+prod3`, `planes+prod3g`, `exist+prod19`, `planes+prod19`
  (ids 33–38). The other configurations run too (the harness runs all), but only these enter decisions; the rest are
  exploratory.
- `research/codec/E02C/` and `research/codec/E02D/` are not modified or read for decisions.
- A genuine harness defect found during E02E invalidates the affected result. It is documented; settings are not changed
  against this data.

## Sources (fixed; no substitution)

None appears in any earlier corpus on this branch (E02C-harness development corpus, the parallel E02C corpus, E02D's
UCI sources) or in the repository's holdout or benchmark manifests (`benchmark/holdout/inputs.json`,
`benchmark/frontier/*.json`), as checked at registration.

| family | publisher files | streams |
|---|---|---|
| F1 NOAA GHCN-Daily (NCEI) | `https://www.ncei.noaa.gov/pub/data/ghcn/daily/all/{USW00094728,USW00023174,USW00014739}.dly` (New York Central Park, Los Angeles Intl, Boston Logan) | per station: TMAX, TMIN, PRCP |
| F2 PhysioNet MIT-BIH Arrhythmia Database 1.0.0 | `https://physionet.org/files/mitdb/1.0.0/{100,101,200,201}.{hea,dat}` | per record: signal 0, signal 1 |

That makes 9 GHCN and 8 ECG streams, 17 inputs. Each stream is one input file. Streams from one family are correlated
and count as one family, not as independent sources.

Conversion (`prepare.py`, fixed at registration, unit-tested on synthetic fixtures before any download):
- **GHCN**: fixed-width `.dly` records in publisher order (station, year, month, element, 31 day slots; value columns
  22–26 + 8·(day−1)). Every value other than `-9999` is kept in file order, flags ignored, as little-endian int32 in a
  NumPy `.npy` v1.0 file. Missing days are skipped, not filled; the record of skipped slots stays outside the compression
  boundary.
- **MIT-BIH**: the header must declare 2 signals in format 212. Samples are decoded as 12-bit two's complement
  (`s0 = b0 | (b1 & 0x0F) << 8`, `s1 = b2 | (b1 & 0xF0) << 4`, minus 4096 when ≥ 2048), one little-endian int16 `.npy`
  per signal, in sample order. The `.dat` byte count must equal 3 × samples / 2 × signals.

These are lossless derived numeric files; E02E reconstructs those files, not the publishers' original bytes. No
rounding, scaling, sorting, resampling, windowing or outlier editing. If a download or parse fails, coverage is reported
as incomplete; no other source replaces it. SHA-256 of every downloaded file and every derived input is recorded in
`manifest.json` before the first codec call. A confirmation pass must reuse the same bytes.

## Protocol

`fetch.sh DIR` downloads exactly the files above. `run.sh DOWNLOAD_DIR OUT_DIR` (fixed at registration; tested before registration only on development files, as a mechanics dry run): verify the frozen hashes; `make -f
research/codec/E02C-harness/Makefile all check`; `prepare.py`; `run_matrix.py --manifest <E02E manifest> --reps 7
--seed 20261114`; `analyze.py`; `decide.py`. One thread. Both regimes run: B (complete planner, fast and hc) is primary;
A (typed `kg_encode_num`) is secondary and descriptive. Every complete input is reconstructed byte for byte for every
configuration (harness behaviour).

## Calibration gate (infrastructure check; failure voids the run)

In regime B, `exist+prod3` must be within +0.10% of the production fast frames and `exist+prod19` within +0.10% of the
production hc frames, pooled over files with INT blocks. (The development value was +0.02%.)

## Hypotheses and decision rules

Population: files on which the unmodified production planner chooses at least one INT-graph block, per mode. A family
with fewer than 2 such files in a mode leaves that mode's hypothesis **not exercised** for the family.

- **H1 (fast)**: `planes+prod3` vs `exist+prod3`.
- **H2 (hc)**: `planes+prod19` vs `exist+prod19`.
- **H3 (guard)**: `planes+prod3g` vs `planes+prod3` in fast mode. Descriptive: number of leaves changed and bytes saved.

For H1 and H2, separately per mode:

| outcome | requirement |
|---|---|
| **Supported** | in **both** families: pooled complete-input bytes over INT files change by ≤ −0.50%; no single file ≥ 64 KiB larger by more than +1.00%; median sequential end-to-end decode time ≤ 1.05× the `exist+prodN` control (pooled over the family's INT files) |
| **Provisional** | the same in exactly one family |
| **Not supported** | neither family meets it, while at least one family is exercised |
| **Inconclusive** | no family exercised, or the calibration gate failed |

Reported regardless: family and pooled totals, the equally weighted mean of per-family changes, the worst per-file
regression, encode time (residual kernel and derived end-to-end), decode time, peak memory, and the change against
unmodified production frames. Results on all 17 inputs (including files without INT blocks, where nothing changes)
are shown separately from the INT-file population. A Supported result is a research signal only. It does not authorise
a format change, default, selector or release; a format change would need its own review and a fresh holdout.

## Integrity

- No outcome-driven change to sources, conversion, configurations, thresholds or statistics after data is read.
  Infrastructure fixes are versioned, and their effect on results is stated.
- Raw records, SHA-256 hashes and failures are retained; large downloaded files and frames are not committed.
- The independent Python reference covers stored blobs and the KG_INT inverse, not the entropy decoders (unchanged
  limitation).
