# Public benchmark: one-command reproduction

Everything behind the benchmark page (`site/index.html`) comes from here. One command fetches the data, builds
the tools at pinned versions, runs every measurement, writes JSON and regenerates the page:

```sh
python3 -m venv .venv && .venv/bin/pip install -r bench/repro/requirements.txt
PYTHON=.venv/bin/python sh bench/repro/run.sh ../kolmo-bench                  # full run (~3 h on an Apple M5)
PYTHON=.venv/bin/python sh bench/repro/run.sh ../kolmo-bench --quick          # smoke test (minutes)
PYTHON=.venv/bin/python sh bench/repro/run.sh ../kolmo-bench --steps panel,site   # one part
```

`--ref <tag|commit|WORKTREE>` picks the Kolmo version to measure (default: the tag of `VERSION`).
The work directory needs about 6 GB. Re-running resumes where it stopped.
Set `KOLMO_BENCH_TIMED` to a lock command if other benchmarks share the machine; every build and timed step runs
under it.

## Steps

| Step | Script | Writes | What it measures |
|---|---|---|---|
| fetch | `bench/lzbench/corpora.py`, `fetch.py` | `data/`, `FETCHED.json` | Silesia, enwik8, Canterbury (rebuilt byte for byte as in the Mac run); holdout sets and model files from their source URLs, each checked by sha256 (`datasets.json`) |
| build | `build.sh` | `tools/`, `tools.json` | Kolmo (`make ARCH=`), lzbench 2.4.1 at `9be0e25` with Kolmo added by `bench/lzbench/install.py`, OpenZL `zli` at `50f8789a` |
| lzbench | `lzbench_run.py` | `lzbench/*.csv`, `lzbench.json` | ~50 codec settings in memory, one thread, on the three corpora and on Silesia file by file |
| panel | `panel.py` + `suite.json` | `panel.json` | Kolmo (no configuration) vs OpenZL untrained **and trained**, zstd, xz; ZipNN on weights |
| mt | `mt_run.py` | `mt.json` | Command line on one thread and on all cores, with peak memory |
| dfloat11 | `dfloat11_tool.py` | `dfloat11.json` | DFloat11's size on the bf16 models, computed from its method (its own decoder needs CUDA) |
| site | `site/build.py` | `site/results.json`, `site/index.html` | The page, with every verdict computed from the JSON |

## The OpenZL rematch

The 0.0.2 report compared Kolmo with OpenZL's *untrained* profiles, on files that overlap Kolmo's development
data. `panel.py` answers both caveats:

- **Holdout data, new to Kolmo**, from public sources, each split into a training half and a test half:

  | Set | Source | Train | Test | OpenZL profiles |
  |---|---|---|---|---|
  | `ghcn-csv` | NOAA GHCN-Daily station CSVs (cut before 2026-01-01) | 4 stations | 4 other stations | `csv` |
  | `gharchive-json` | GH Archive event JSON, 2015-01-01 | hours 12–15 | hours 16–19 | `serial`, `lz` |
  | `nasa-logs` | NASA Kennedy HTTP logs, 1995 | 1–4 July | 4–7 August | `serial`, `lz` |
  | `ncep-npy` | NCEP/NCAR reanalysis surface fields, float32 `.npy` | 2022 | 2023 (same fields) | `numpy` |
  | `tlc-parquet` (optional) | NYC TLC trips, rewritten as plain uncompressed Parquet | Jan–Feb 2024 | Mar–Apr 2024 | `parquet` |

- **OpenZL trained**: `zli train -p <profile> <train dir>` once per set and profile (default greedy trainer,
  capped at 20 minutes), then `zli compress -c <trained compressor>` on the test files. The training files are
  never measured. Training time and the compressor's size are recorded.
- **OpenZL untrained**: the same profiles at `-l 6` and `-l 9`, as in the report.
- **Kolmo**: `kolmo -3 / -5 / -7 / -9` (and `-U` up to 12 MB), never trained, never told the format.

## ML weights

Three public model files from Hugging Face, pinned by revision: GPT-2 (f32, 548 MB), SmolLM2-135M (bf16,
269 MB), Qwen2.5-0.5B (bf16, 988 MB). Kolmo -1…-9, zstd, xz, OpenZL's `safetensors` profile and ZipNN
(`zipnn_tool.py`: whole file, ZipNN's defaults, one thread, timed in-process). DFloat11 decodes only on NVIDIA
GPUs; `dfloat11_tool.py` computes the size its method gives (sign and mantissa kept, exponent Huffman-coded) and the
page labels that column as computed.

## Rules

- One thread per tool, except the explicitly all-core rows of `mt`.
- Same compiler options: Kolmo with `make ARCH=` (the release flags); every lzbench codec with lzbench's flags.
- Every round trip compared byte for byte; failures are recorded as error rows, not dropped.
- Best of 3 for command-line runs (1 above 60 MB or for the slowest settings).
- Inputs are fetched from their sources and checked by sha256; the repository redistributes none of them.
  GHCN station files grow daily, so only their date-cut derived files are pinned.
- The Python packages (numpy, h5py, pyarrow, zipnn) go in a virtual environment, never system-wide.
