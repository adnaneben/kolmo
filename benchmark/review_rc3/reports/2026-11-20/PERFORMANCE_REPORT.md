# PERFORMANCE_REPORT — RC3 review, fresh Mac results (2026-11-19 UTC)

This report covers only fresh measurements from this review. The earlier reports (benchmark/rc3/*, kolmo-rc3-report.md) are history; `claims/CLAIMS_LEDGER.md` and `claims/ERRATA_2026-11-20.md` say which of their claims hold.

## Setup

- **Machine.** Apple M5 (arm64), macOS (Darwin 27.2), on AC power with caffeinate. One workload at a time.
- **Harness.** `benchmark/review_rc3/audit.py` at review commit 2bdbbb72. Each Kolmo trial runs as a fresh process.
- **Strict T1.**
  - Kolmo runs with `-T 1` for encode and decode, plus the T1 environment variables.
  - zstd runs with `--single-thread`, xz with `-T1`, 7zz with `-mmt=1`, and bzip3 with `-j 1`.
  - Sampled running threads are recorded for every trial (`max thr e/d`).
  - lz4 1.11 and 7zz show a second sampled thread: an I/O or helper thread. They are labelled "worker + async I/O" in the tool inventory and are not strict T1. Their speed figures are therefore favourable to the rival.
- **Trials.** One warmup plus 5 accepted timed trials per file × codec. Every decode is checked byte for byte against the input.
- **Quiet rule (preregistered).** Other processes may use at most 0.5 cores, judged over a 0.3 s pre-window before encode and before decode. Rejected trials are kept and run again.
- **Measures.** Complete compressed file size; median wall time; time-average RSS; kernel peak RSS.
- **Binaries.**
  - RC3: `screen-builds-1/rc3/build/kolmo`, sha256 a8a30a0c….
  - C1: `final-builds/c1/build/kolmo`, sha256 badac9ea…, code 302e31a6.
  - Both are built with `-O3 -mcpu=native`.

## 1. Holdout (unseen corpus, decisive for C1)

21 files totalling 364,219,430 bytes; none share a sha with the screening corpus (`corpus-holdout/MANIFEST.json`):
- 7 Silesia files;
- CSV, text, a bf16 weights file and FASTQ;
- 2 Y4M videos;
- 4 PPM images;
- 4 ESC-50 WAV clips.

The run accepted 1,785 trials and rejected 64, with no missing cells. All 357 cells are deterministic; 0 decode mismatches. The report is `report-holdout/holdout-main/{report.json,per_file.csv,scoreboard.html}`, summarised in `report-holdout/SUMMARY.md`.

| preset | total size | ratio | enc MiB/s | dec MiB/s | enc peak max | dec peak max |
|---|---:|---:|---:|---:|---:|---:|
| c1/speed | 241,931,382 | 1.505 | 469 | 905 | 17 MiB | 15 MiB |
| c1/balanced | 139,896,825 | 2.603 | 217 | 438 | 137 MiB | 118 MiB |
| c1/eco | 131,876,301 | 2.762 | 20.1 | 285 | **1,079 MiB** | 143 MiB |
| c1/hc | 126,787,509 | 2.873 | 6.2 | 430 | 262 MiB | 117 MiB |
| c1/ratio | 116,731,731 | 3.120 | 4.9 | 62 | 640 MiB | 188 MiB (rc3 223) |

**C1 vs RC3** has identical bytes on all 21 files for every preset. Encode and decode times are statistical ties on 21/21 files for every preset (geometric-mean ratios 0.976–1.020). The only significant difference is ratio decode peak RSS: 8 wins, 13 ties, 0 losses (W4 bwt).

**C1 vs rivals (holdout):**

| comparison | size | encode | decode | memory |
|---|---|---|---|---|
| speed vs lz4-1 | 0.25% smaller total; per-file 6 W / 15 L (a tie) | 0.97× | **1.14×** | lower RSS |
| balanced vs zstd-3 | **21.5% smaller**, 19/21 | 0.72× | 0.92× | higher RSS (137 vs 7 MiB peak) |
| hc vs zstd-19 | **16.0% smaller**, 15/21 | 1.13× | 0.96× | higher RSS |
| ratio vs xz-9 | **11.3% smaller**, 18/21 | 1.43× | 0.67× | — |
| ratio vs 7zz-9 | **11.2% smaller**, 18/21 | 1.26× | 0.51× | — |
| ratio vs bzip3 | **10.6% smaller**, 21/21 | 0.71× | 2.13× | — |
| ratio vs bsc | **12.3% smaller**, 20/21 | 0.25× | 1.10× | — |

**Generalisation note.** On the screening corpus, ratio beat xz-9 by 21.5% and hc beat zstd-19 by 19.6%. On the holdout the margins are 11.3% and 16.0%. Quote holdout figures for unseen data. The screening corpus contains several files Kolmo routes to specialised codecs.

## 2. Screening (RC3 baseline and branches; 21 files, `report-screening/SUMMARY.md`)

| RC3 vs rival | total size | enc | dec |
|---|---|---|---|
| ratio vs xz-9 | 21.5% smaller | 1.59× | 0.73× |
| ratio vs 7zz-9 | 21.4% smaller | 1.54× | 0.57× |
| ratio vs brotli-11 | 23.3% smaller | 10.0× | 0.34× |
| ratio vs bzip3 | 11.9% smaller | 0.82× | 2.05× |
| ratio vs bsc | 13.8% smaller | 0.29× | 1.09× |
| hc vs zstd-19 | 19.6% smaller | 1.26× | 0.94× |
| eco vs zstd-9 | 22.9% smaller | **0.36×** | **0.66×** |
| balanced vs zstd-3 | 22.3% smaller | 0.68× | 0.94× |
| speed vs lz4-1 | 0.26% **larger** (6 W / 15 L) | 0.97× | 1.13× |
| speed vs zstd-1 | 44.6% **larger** | 1.25× | 1.41× |
| x vs zpaq-m5 (8 files) | 7.85% smaller | 0.98× | 4.07× |

Branch results are summarised in `FEATURE_DECISIONS.md`.

## 3. Scoreboard — strict T1 (GREEN = better than reference beyond CI, RED = worse, GRAY = tie or not measured)

| objective | reference | size | encode | decode | memory |
|---|---|---|---|---|---|
| C1 vs RC3, all presets (holdout) | rc3 | GRAY (identical) | GRAY | GRAY | GREEN (ratio decode peak) |
| speed vs lz4-1 | lz4-1 | GRAY | GRAY | GREEN | GREEN |
| speed vs zstd-1 | zstd-1 | RED | GREEN | GREEN | GRAY |
| balanced vs zstd-3 | zstd-3 | GREEN | RED | RED (marginal) | RED |
| eco vs zstd-9 | zstd-9 | GREEN | RED | RED | RED (1.08 GiB enc peak) |
| hc vs zstd-19 | zstd-19 | GREEN | GREEN | GRAY | RED |
| ratio vs xz-9 / 7zz-9 | xz / 7zz | GREEN | GREEN | RED | mixed |
| ratio vs bzip3 / bsc | bzip3 / bsc | GREEN | RED | GREEN | GREEN on decode |
| x vs zpaq-m5 | zpaq | GREEN | GRAY | GREEN | RED (3.0 GiB peak) |
| audio vs FLAC, image vs JPEG XL, video vs FFV1, FASTQ vs CRAM/fqzcomp, arrays vs Pcodec/OpenZL | specialists | **GRAY: not measured** | | | |

Per-file HTML scoreboards (green/red/gray): `report-holdout/holdout-main/scoreboard.html` and `report-screening/<suite>/scoreboard.html`.

## 4. Separate scoreboards (not strict T1)

- **W4 docs.** Decode runs 2 threads at -T1. Balanced is 0.60% smaller on 4 docs files; decode RSS is worse on 3/4. Research only.
- **Multithread / governor (W4 multi) and GPU=1.** Not measured in this campaign.

## Coverage and limits

- Domain specialists are **unavailable** on this Mac: flac, ffmpeg/FFV1, cjxl, Pcodec, OpenZL, Kanzi, fqzcomp/CRAM and cmix (`tool-inventory/TOOL_INVENTORY.md`). Kolmo's domain claims (audio vs FLAC and the others) are therefore **not re-verified**. Only general-purpose rivals were measured.
- Measurements are arm64 only. x86 paths (decmap AVX2) and Linux huge pages (W4 bwt) were not exercised.
- Short decodes (under about 50 ms) carry ±10% CI from process start-up, so small speed claims on small files are unresolvable with the cold CLI.
