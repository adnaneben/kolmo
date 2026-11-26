# Performance graph: Kolmo snapshots against the field

One chart per data group, plus an overall chart:

- **x** = compression ratio (original / compressed);
- **y** = combined speed = original bytes / (encode s + decode s), decimal MB/s, log scale;
- every point is annotated with its encode MB/s (`e`) and decode MB/s (`d`).

Kolmo levels 1-9 and ultra are drawn for each snapshot with its own marker. A dashed
arrow joins the same level from one snapshot to the next, so each level's movement is visible.

| Snapshot | Build | Marker |
|---|---|---|
| A | tag `v0.0.2` (550c02fd), built from source with the repository Makefile | hollow circle |
| B | `main` a1d39559, labelled "main (pre-0.0.3)"; replaced by the 0.0.3 candidate once it is ready | filled square |
| C | post-review build (later) | triangle |

A and B are built the same way (`make build/kolmo`, default `-O3 -mcpu=native`), so the
comparison is between sources, not build settings. The B binary's SHA-256 (`b67bbddb…`) is the
same as the main arm in Codex's `tip-all-levels` run. `$V/bins-v010/kolmo-0.0.2` is not used: it is
not the tagged build.

Competitors, each on one thread: lz4 -1; zstd -1/-3/-9/-19/--ultra -22; brotli 1/5/9/11;
xz -6 and -9e; 7-Zip `7zz` LZMA2 -mx9 and PPMd -mx9; bzip3; bsc; zpaq -m4 and -m5.
Exact command lines are in [configs.py](configs.py) and in every row (`enc_argv`, `dec_argv`).
PAQ8PX is not run here.

## Method

Aligned with Codex's `benchmark/waves/2026-11-25/overnight/tip-benchmark` on
`codex/ppp-audit-and-results-20261125`: decimal MB/s over original bytes, wall time of the
CLI process including start-up and warm-input file I/O, and combined time = median of the
per-round encode+decode sums.

- Every tool runs on one thread: `kolmo -T 1`, `zstd -T1`, `xz -T1`, `lz4 -T1`, `7zz -mmt1`,
  `bzip3 -j 1`, `zpaq -t1`; brotli is single-threaded and the Homebrew bsc 3.4.0 is not linked
  with OpenMP.
- Each cell (configuration × input) runs 3 encodes and 3 decodes. Encode and decode speeds
  use the medians; peak RSS is from `wait4`; every decode is checked by SHA-256 against the
  input; compressed bytes and their SHA-256 are recorded for each run (a cell whose outputs
  differ between runs is rejected by the plotter).
- Each cell runs under `$V/tools/timed.sh` (the machine-wide timing lock) and only when the
  1-minute load is under 12. The load is read inside the lock at the start and the end of the
  cell and recorded in the row (`load1_before`, `load1_after`); a cell that ends at or above
  12 is discarded and re-run.
- The rows file is a checkpoint: a cell already present (same config, input SHA-256 and binary
  SHA-256) is skipped, so a run resumes where it stopped.

## Data

The inputs are the hashed files of the PR #48 impact corpus (`$V/impact/corpus/manifest.jsonl`,
mostly 4 MiB slices of public corpora). Every input's SHA-256 is checked against that manifest
before a run. The group lists are in [groups.json](groups.json). The `dev` set is charted. The
held-out set is kept apart: it runs only with `--set holdout` into its own rows file and charts,
never mixed with `dev`.

| Group | dev inputs |
|---|---|
| Text | dickens, Silesia xml |
| Source code | FLAC C sources |
| JSON and logs | api.json, Loghub |
| CSV and tables | energy 130-column CSV, GHCN CSV, GHCN rows TSV |
| Numeric arrays and weights | population i64, OWID f64 .npy, bf16 safetensors, Magika ONNX |
| Executables | ooffice (x86) |
| Images | retina PPM, Hubble PPM, x-ray (16-bit) |
| Mixed tar | Canterbury corpus tar |

Group points sum over their inputs: ratio = Σ original / Σ compressed, speeds = Σ original / Σ time.

## Running

```bash
V=/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD
python3 benchmark/perfgraph/run.py --corpus $V/impact/corpus --set dev \
  --out benchmark/perfgraph/rows/dev.jsonl \
  --kolmo A=path/to/kolmo-v0.0.2 --snapshot-label "A=v0.0.2" \
  --kolmo B=path/to/kolmo-main --snapshot-label "B=main (pre-0.0.3)" --competitors
```

```bash
python3 benchmark/perfgraph/plot.py benchmark/perfgraph/rows/dev.jsonl --out benchmark/perfgraph/charts/dev
```

Untimed size pass (any load, `nice -n 15`, one rep, every output still decoded and hashed):
add `--size-only`; those rows are marked `size_only` and `plot.py --sizes` turns them into
`sizes.md`/`sizes.csv`. They never feed a speed axis, and the plotter rejects a timed cell whose
compressed bytes differ from the size pass of the same binary.

When the whole run is already inside the timing lock (the coordinator's quiet-window queue wraps
each job in `timed.sh`), pass `--timed none` so the runner does not try to take the lock again;
it still waits for load < 12 before every cell.

The plotter needs matplotlib. A new snapshot (C, or B rebuilt on the 0.0.3 candidate) is one more
`--kolmo` run into the same rows file; the competitors' rows are reused. When B is rebuilt, its
binary hash changes, so its cells are re-measured; the plotter keeps the latest row per cell.
