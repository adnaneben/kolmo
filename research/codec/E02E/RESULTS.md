# E02E results — per-plane KG_INT residual leaves on source-disjoint data

**Registered outcome: H1 (fast) PROVISIONAL — supported in one family only. H2 (hc) NOT SUPPORTED. H3 (guard) never
triggered.** No format change is justified by this result. Rules, sources and thresholds: [`PREREGISTRATION.md`](PREREGISTRATION.md),
applied by `decide.py` unchanged.

## Execution

| | |
|---|---|
| Run analysed | GitHub Actions **43255531577** (`e02e-validation.yml`), head `c10adf2`, ubuntu-latest, Intel Xeon Platinum 8370C @ 2.80 GHz, 4 vCPU, one thread |
| Earlier run | **43253243675** (head `5f4a5f0`): fetch and every measurement completed, then `analyze.py` failed on Python 3.11 (nested same-quote f-string). Fixed in `c10adf2` (quote characters only; output byte-identical on 3.11 and 3.13), then the registered protocol was re-run. Its measurements were not analysed; its artifact (`e02e-validation-43253243675-1`, sha256 `67b2b138…fd24`) is retained by GitHub |
| Downloads | all 11 registered files fetched; SHA-256 identical in both runs (`results/DOWNLOAD.sha256`) |
| Frozen harness | `run.sh` verified `FROZEN.sha256` (e02c.c `3171cd94…`, selftest.inc `d85f4429…`, reference.py `230f314a…`, golden.txt `c9bcac15…`); production `src vendor Makefile tests TARGETS.md` identical to `2f3308a` |
| Checks inside the run | prepare unit tests, sanitizer selftest, golden vectors (two builds), Python reference: passed; every complete input reconstructed for every configuration |
| Inputs | 17 of 17 prepared, none missing (`results/manifest.json`) |
| Evidence | `results/`: decision, manifest, env, tables, raw records (`raw.jsonl.xz`, uncompressed SHA-256 `34ef353d…`), artifact manifest; files recovered from the job log and checked byte for byte against the run's own `MANIFEST.json`. `decide.py` re-run locally on the recovered raw records gives an identical decision |

## Calibration gate — passed

`exist+prod3` vs production fast frames: **+0.052%**; `exist+prod19` vs production hc frames: **+0.008%** (limit +0.10%).

## Decisions as registered

| hypothesis | F1 GHCN-Daily (9 files) | F2 MIT-BIH (8 files) | status |
|---|---|---|---|
| H1 fast: planes+prod3 vs exist+prod3 | **−4.23%** pooled; worst file 0.00%; decode 1.018× — meets | not exercised (planner chose no INT blocks) | **provisional** |
| H2 hc: planes+prod19 vs exist+prod19 | −3.72% pooled; worst 0.00%; decode **1.105×** — fails the 1.05× bar | 0.00%; decode 0.998× — fails the −0.50% bar | **not supported** |
| H3 guard (fast) | 0 files changed | — | never triggered |

Against unmodified production frames: GHCN fast −4.18%, hc −3.67%; MIT-BIH hc +0.004% (framing).

## What the numbers mean (descriptive, not new rules)

- **MIT-BIH is structurally out of scope, not a counter-example.** 12-bit ECG residuals fit in one byte (`ow1` in
  every block), so the residual has a single plane and splitting changes nothing (0.00% in both regimes, typed
  included). In fast mode the production planner chose the image transform (plan 5) over the integer graph for all
  eight signals. The two-family rule therefore could not be met by a layout that only acts on multi-byte residuals.
- **GHCN gains are concentrated on precipitation.** PRCP (order 0, `ow2`, mostly zeros) gains −16.1% to −16.8% in fast
  mode and −5.3% to −8.1% in hc; temperatures gain −0.1% to −1.2% (fast) and −1.4% to −3.8% (hc); TMIN at Los Angeles
  has `ow1` residuals (0.00%).
- **The hc decode failure is a fixed per-leaf cost on small files.** GHCN files decode in 0.13–0.45 ms; the extra zstd
  frame adds 15–57 µs per file (one file was faster). On the larger development files (E02C-harness §9) the same
  layout cost about +1%. The registered bar is applied as written: H2 fails.
- **Guard.** No large leaf in this data met the misfire condition, so the guard cost nothing and changed nothing.

## Conclusion and decision

Per-plane leaves remain a research candidate only for multi-byte KG_INT residuals; the validated signal is limited to
one family (GHCN, fast). This does **not** justify the `KG_SPLIT` format change. If pursued, the next validation must
be preregistered fresh, on new sources chosen so that both families have multi-byte residuals (a phenotype condition
fixed before data, not tuned to these results), and must state in advance how small-file decode overhead is judged.

## Limitations

- Two families; one structurally inapplicable. GHCN streams are small (0.12–0.23 MB) and three stations share format.
- Shared CI runner timings; decode ratios on sub-millisecond files are sensitive to fixed overheads.
- Artifact storage was unreachable from the analysing session; evidence was recovered from the job log and hash-checked.
