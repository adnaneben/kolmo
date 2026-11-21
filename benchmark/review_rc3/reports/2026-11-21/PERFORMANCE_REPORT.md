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

---

# Addendum, 2026-11-21 (overnight) — candidate C2 and the domain specialists

C2 = 25bedda8 (C1 + KOLMO_NATIVE v11 fix + W4 docs encoder tuning + brotli mmap allocator + W4 cm). The release binary is `final-builds/c2/build/kolmo`, sha256 31b9e558…. The method is the same as above: strict T1, one warmup plus 5 accepted trials, byte-verified decodes, the preregistered quiet rule, and one suite at a time.

## C2 vs C1 — holdout (21 unseen files; `report-holdout-c2/holdout-c2`; 1,050 accepted, 14 rejected)

| preset | size vs C1 | encode | decode | memory |
|---|---|---|---|---|
| speed | identical | tie | tie | — |
| balanced | **−0.14%** (4 W / 14 T / 3 L) | tie | tie | — |
| eco | **−4.39%** (5 W / 16 T) | 1.05× | **0.72×** (ALTERNATIVES A6) | **encode peak 1,079 → 233 MiB** |
| hc | **−1.04%** (4 W / 17 T) | 0.975× | 0.98× | encode peak 262 → 242 MiB |
| ratio | identical | tie | tie | — |

eco's decode slows on 4 text files: ld30 0.15×, reymont 0.19×, FASTQ 0.10×, samba 0.51×. Eco's total decode is 135 MiB/s, against 62 MiB/s for ratio.

## Native lane, opt-in — holdout, measured with its rivals (`report-holdout-c2/holdout-native`)

| setting | vs rival | size | encode | decode |
|---|---|---|---|---|
| speed + KOLMO_NATIVE=all | lz4-1 | **4.42% smaller** (17 W / 4 L) | 0.94× | 1.02× (tie) |
| speed + KOLMO_NATIVE=all | zstd-1 | 23.6% larger | 1.20× | 1.37× |
| hc + all,strong | zstd-19 | **20.5% smaller** (21/21) | 0.85× | **0.33×** |
| balanced + all | zstd-3 | 20.1% smaller | 0.66× | 0.89× |

balanced + all is **1.9% larger than default balanced**, so native LZ does not help balanced. speed + all is 4.2% smaller than default speed.

## Domain specialists (`report-domain-c2/*`; normalised inputs, see ALTERNATIVES A4)

| domain (files) | best rival | C2 ratio | C2 hc | C2 balanced | notes |
|---|---|---|---|---|---|
| audio (8 ESC-50 WAV) | flac -8 | **−12.4%** (enc 3.9, dec 9.2 MiB/s vs flac 31/32) | −9.1% | **−9.1%** (enc 13, dec 28 MiB/s) | balanced beats flac on size at about 0.9× its decode |
| image (6 PPM) | cjxl -d0 -e9 | **+20.5% (worse)** | +33.5% | +50.4% | the clear weakness; image-codec work in progress (branch review/c3-image-J62kQD) |
| numeric (2 raw arrays) | pcodec | **−37.3%** | −35.9% | −30.4% (and 3.1× faster decode) | strong |
| FASTQ (2, samtools-normalised) | CRAM 3.1 archive | +1.7% (enc 8.6× faster, dec 0.54×) | +15.1% (dec 7×) | +51% | CRAM's size varied by 1 byte between runs (larger size counted) |
| video (3 CIF y4m) | FFV1 level 3 | **−14.9%** (dec 0.84×) | **−11.5%** (dec 5.2×) | −6.6% (enc 2.6×, dec 5.2×) | FFV1 runs 4 threads (worker+async I/O), so it is not strict T1 |

**Still not measured:** OpenZL, Kanzi, fqzcomp5, cmix and ZipNN. Audio is measured only on 5-second mono clips; longer or stereo music is not covered.

## Ultra (-x) on C2 — timed (`report-ultra-c2/ultra-c2`; 8 screening ultra inputs; 120 accepted, 3 rejected)
| comparison | size | encode | decode | peak RSS enc/dec |
|---|---|---|---|---|
| C2 -x vs C1 -x | 0.28% larger (0 W / 3 T / 5 L) | **1.98×** | **1.87×** | **226 / 185 MiB vs 3,046 / 2,535 MiB** |
| C2 -x vs zpaq -m5 | **7.59% smaller** (7/8) | 1.93× | **7.46×** | about equal |
This reproduces the W4 cm screening result on the integrated C2 binary.

## Image codec kimg (C3 = cbb40b5d, branch review/c3-image-J62kQD, not in C2) — timed (`report-domain-c3/dom-image`)
| C3 ratio vs | size | encode | decode |
|---|---|---|---|
| C2 ratio | **−12.92%** (6/6) | 0.54× | 0.59× |
| cjxl -e7 | +4.24% | 0.53× | 0.59× |
| cjxl -e9 | +4.95% | 1.86× | 0.61× |
The gap to JPEG XL lossless narrows from +20.5% to +4.2%.

### kimg round 2 (f1f3f1c3, kimg stream v2), timed (`report-domain-c3r3/dom-image`; 360 accepted, 6 rejected)
| f1f3f1c3 ratio vs | size | encode | decode |
|---|---|---|---|
| C2 ratio | **−14.07%** (6/6) | 0.76× | 0.69× (14.8 vs 23.6 MiB/s) |
| cjxl -e7 | +2.87% | 0.72× | 0.69× |
| cjxl -e9 | +3.57% | **2.56×** | 0.72× |
