# rc3 features-a: trained profiles (`kolmo train`) and `--target`

Lane report, 16 November 2026. Branch `claude/rc3-features-a-20261116` (from `claude/release-rc3-20261116`, with
`claude/rc3-structure-20261116` merged). Code: `src/plan/profile.c`, `src/plan/target.c`, `src/api/features.c`,
`src/cli/train.c`; formats in `SPEC.md` (*Trained profiles (KPF1)*, *Profile frames*, *Target frames*); tests in
`tests/test_profile.c` (`make check-profile`, part of `make check-rc`).

## Summary

- **Profiles beat `zstd --train` on every dataset family measured**, at the same zstd level (`balanced` against
  zstd 3, `hc` against zstd 19) and on held-out files of the same sources: single messages 2.4–11% smaller
  (3.5–4.7% at `hc`), CSV and JSON exports 5–11%, Hadoop and OpenStack logs 13–27%, SSH logs and numeric files
  43–71%. Against untrained Kolmo the gain is 64–79% on single
  messages, 39–50% on Hadoop logs, 8–10% on OpenStack logs, and nothing (±0.2%) on large CSV, JSON and numeric files,
  where the analysis already finds the structure and a dictionary has little left to give.
- **Messages also got faster:** untrained Kolmo spends most of its time on per-frame overhead for 100-byte to 1 KB
  inputs; with a profile they compress 3.6–4.0× and decompress 3.3–4.3× faster than untrained Kolmo, as fast as zstd
  with a prepared dictionary or faster (`balanced`).
- **`--target` picks the right setting** (median *choice* regret 0% on every target but one, against the best fixed
  setting measured after the fact), and the decode floors are met on 15 of 16 runs with 0% median size regret. **The
  cost targets pay too much for their trial compressions**: trial time is part of the cost, and on the 0.3–23 MB files
  here the total cost is 9.5–39% (median) above the best fixed setting. Details and the next step below.
- OpenZL's trained compressors were **not re-measured**: building OpenZL (and the zstd command line) from a downloaded
  source tree was refused by this session's permission policy. The zstd baseline uses the vendored libzstd's
  `ZDICT_trainFromBuffer`, which is what `zstd --train` runs.

## Machine, builds, protocol

- Cloud VM, Intel Xeon @ 2.10 GHz, 4 vCPU, Linux 6.18, gcc 13.3, `make` defaults (`-O3 -march=native`).
- Every program on **one thread** unless the table says 4 threads (`benchmark/rc2/PROTOCOL.md` rule 5: never mixed).
  In memory, in one process (`build/kpbench`, `build/tgbench`), so process start-up does not swamp small messages.
  Each file is compressed and decompressed on its own and every round trip is checked byte for byte; fastest of
  3 passes over the whole test set (1 pass for the `ratio` and `-x` settings in `tgbench`).
- zstd baseline: vendored libzstd 1.5.7, the dictionary from `ZDICT_trainFromBuffer` over the training files cut into
  16 KiB pieces (`zstd --train -B16K`; `zstd --train` rejects fewer than five samples, and three families have one to
  four training files), 112,640 bytes (zstd's default), frames as the zstd command line writes them (checksum, content
  size, dictionary id), with prepared dictionaries (`ZSTD_CDict`/`ZSTD_DDict`) as a server would keep them.
- Profiles: `kolmo_profile_train` with defaults (the same 112,640-byte dictionary budget), trained on the training
  files only.

## Data

`prepare.py` downloads and cuts every set (`python3 -I prepare.py DATA_DIR`); downloads are treated as untrusted.
Each family is split by time or by file: the test files are later months, later chunks or other files of the same
source, never seen in training.

| family | source | train files | train MB | test files | test MB | test set SHA-256 (first 16) |
|---|---|---:|---:|---:|---:|---|
| csv-quakes | USGS catalogue CSV, Jan 2020 / Feb–Jun 2020 | 1 | 3.24 | 5 | 15.89 | 4c089c73ea91894e |
| json-quakes | USGS GeoJSON, Jan / Feb–Mar 2020 | 1 | 12.54 | 2 | 20.31 | 34428cbf8b3f6959 |
| log-hadoop | Loghub Hadoop, one file per container log; every 5th file trains | 196 | 14.78 | 782 | 33.82 | 2eef5ba2cf656b30 |
| log-openstack | Loghub OpenStack, 256 KiB chunks; first 10% train | 23 | 6.03 | 212 | 55.42 | a7d3a152b9e9bc3d |
| log-ssh | Loghub OpenSSH, 1 MiB chunks (rotations); first 4 train | 4 | 4.19 | 20 | 20.97 | 97811698aa489f0f |
| num-power | UCI household power, `;` table per month; 2 months train | 2 | 4.32 | 12 | 33.77 | e1c807a5ad176b56 |
| num-power-f32 | the same months as float32 NPY (7 columns) | 2 | 1.87 | 12 | 14.72 | 2edef6385395204c |
| msg-geojson | single GeoJSON features (~0.7 KB) | 2000 | 1.43 | 6000 | 4.28 | b828f6dcebe5ed8b |
| msg-openstack | single OpenStack log lines (~300 B) | 2000 | 0.59 | 3000 | 0.89 | 2b0f0833e4b113c8 |
| msg-ssh | single OpenSSH log lines (~120 B) | 2000 | 0.23 | 3000 | 0.36 | d8c273184f4ff95c |

## 1. Trained profiles

### What a profile holds and what a profile frame saves

A profile (`SPEC.md`, *Trained profiles (KPF1)*) is a versioned, CRC-checked file of sections: the setting (`PARM`),
pinned plans (`PLAN`), a zstd dictionary (`ZDIC`), a raw dictionary for Brotli and LZ4 (`RDIC`), the largest leaf
that uses them (`DLIM`), a training summary (`INFO`), and a reserved critical section for context-mixing priors
(`CMPR`; the xmode lane has not provided priors, so there is no writer yet). A frame written with it records the
32-bit profile id (container version 11, header flag bit 2); one-block frames up to 1 MiB use the compact form
(`CZC1`): 14–16 bytes of framing for a small message instead of 52, and zstd leaves up to 64 KiB are raw zstd blocks,
without the 5–6 bytes of zstd frame and block header that the container already makes redundant.

Training (`kolmo train`, `kolmo_profile_train`) analyses every sample as the planner would and pins a plan for each
file shape (the first line with digits and letters normalised, plus a size band) when the planner chose it for at
least 90% of the bytes of that shape; trains the dictionaries; and, with five or more samples, measures on a held-out
fifth to choose the dictionary trainer (zstd's fastCover, as `zstd --train`, or COVER), the leaf-size limit for the
dictionaries (64 KiB to unlimited) and, with `--target`, the setting.

### Results

Ratio = original bytes / compressed bytes over the whole test set; "vs" columns compare compressed sizes (negative =
the profile is smaller); MB/s = test-set bytes per second, compress / decompress. Raw lines: `results/kpbench.jsonl`.

**`speed`, 1 thread** (zstd level 1; ratio = original / compressed; MB/s compress / decompress)

| family | files | MB | untrained | profile | zstd | zstd + dict | profile vs untrained | profile vs zstd + dict | untrained MB/s | profile MB/s | zstd + dict MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| csv-quakes | 5 | 15.9 | 2.146 | **2.147** | 3.486 | 3.490 | -0.0% | +62.5% | 424 / 2318 | 426 / 2353 | 327 / 963 |
| json-quakes | 2 | 20.3 | 5.202 | **5.202** | 8.886 | 8.893 | +0.0% | +70.9% | 763 / 2865 | 771 / 2978 | 643 / 1346 |
| log-hadoop | 782 | 33.8 | 8.634 | **11.013** | 13.775 | 19.709 | -21.6% | +79.0% | 1340 / 4244 | 1208 / 3677 | 909 / 1909 |
| log-openstack | 212 | 55.4 | 6.630 | **6.858** | 12.440 | 13.303 | -3.3% | +94.0% | 1150 / 3990 | 899 / 3361 | 772 / 1765 |
| log-ssh | 20 | 21.0 | 8.865 | **8.867** | 16.582 | 16.637 | -0.0% | +87.6% | 1657 / 5096 | 1613 / 4992 | 940 / 2111 |
| msg-geojson | 6000 | 4.3 | 1.155 | **3.905** | 1.621 | 5.511 | -70.4% | +41.1% | 507 / 2542 | 571 / 2685 | 382 / 972 |
| msg-openstack | 3000 | 0.9 | 0.907 | **2.708** | 1.270 | 3.565 | -66.5% | +31.6% | 335 / 2234 | 507 / 2262 | 309 / 782 |
| msg-ssh | 3000 | 0.4 | 0.697 | **1.755** | 0.986 | 1.689 | -60.3% | -3.8% | 98 / 740 | 236 / 834 | 78 / 245 |
| num-power | 12 | 33.8 | 3.546 | **3.546** | 6.734 | 6.729 | +0.0% | +89.8% | 625 / 2117 | 657 / 2173 | 403 / 959 |
| num-power-f32 | 12 | 14.7 | 2.406 | **2.407** | 3.045 | 3.047 | -0.0% | +26.6% | 462 / 2371 | 488 / 2453 | 338 / 764 |

`speed` (LZ4 class) against LZ4 with the same dictionary (its last 64 KiB, as `lz4 -D`). The LZ4 rows are bare blocks (no frame, no checksum: a lower bound); the `lz4` command line adds a 19-byte frame per file (magic, descriptor, block size, end mark, content checksum), shown as *framed*. Their compress speed is not shown: this harness re-hashes the dictionary for every message.

| family | profile | lz4 | lz4 + dict, bare | lz4 + dict, framed | profile vs bare | profile vs framed | profile decode MB/s | lz4 + dict decode MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| csv-quakes | **2.147** | 2.151 | 2.152 | 2.152 | +0.2% | +0.2% | 2353 | 2464 |
| json-quakes | **5.202** | 5.219 | 5.221 | 5.221 | +0.4% | +0.4% | 2978 | 3331 |
| log-hadoop | **11.013** | 8.731 | 10.861 | 10.810 | -1.4% | -1.8% | 3677 | 3577 |
| log-openstack | **6.858** | 6.638 | 6.857 | 6.854 | -0.0% | -0.1% | 3361 | 3764 |
| log-ssh | **8.867** | 8.868 | 8.875 | 8.874 | +0.1% | +0.1% | 4992 | 5282 |
| msg-geojson | **3.905** | 1.256 | 4.254 | 3.821 | +8.9% | -2.1% | 2685 | 2839 |
| msg-openstack | **2.708** | 1.069 | 3.139 | 2.612 | +15.9% | -3.5% | 2262 | 3177 |
| msg-ssh | **1.755** | 0.984 | 2.158 | 1.605 | +22.9% | -8.6% | 834 | 1712 |
| num-power | **3.546** | 3.550 | 3.550 | 3.550 | +0.1% | +0.1% | 2173 | 2064 |
| num-power-f32 | **2.407** | 2.408 | 2.409 | 2.409 | +0.1% | +0.1% | 2453 | 2472 |

**`balanced`, 1 thread** (zstd level 3; ratio = original / compressed; MB/s compress / decompress)

| family | files | MB | untrained | profile | zstd | zstd + dict | profile vs untrained | profile vs zstd + dict | untrained MB/s | profile MB/s | zstd + dict MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| csv-quakes | 5 | 15.9 | 3.947 | **3.950** | 3.550 | 3.541 | -0.1% | -10.3% | 141 / 370 | 144 / 350 | 186 / 882 |
| json-quakes | 2 | 20.3 | 9.274 | **9.276** | 8.852 | 8.848 | -0.0% | -4.6% | 236 / 397 | 215 / 372 | 452 / 1275 |
| log-hadoop | 782 | 33.8 | 15.974 | **26.017** | 13.787 | 19.520 | -38.6% | -25.0% | 144 / 1090 | 144 / 1196 | 743 / 1780 |
| log-openstack | 212 | 55.4 | 15.550 | **16.826** | 11.197 | 12.329 | -7.6% | -26.7% | 99 / 1366 | 278 / 1438 | 431 / 1608 |
| log-ssh | 20 | 21.0 | 49.352 | **49.957** | 14.982 | 14.733 | -1.2% | -70.5% | 214 / 1087 | 257 / 1062 | 568 / 1721 |
| msg-geojson | 6000 | 4.3 | 1.454 | **5.847** | 1.612 | 5.705 | -75.1% | -2.4% | 105 / 324 | 381 / 1064 | 341 / 916 |
| msg-openstack | 3000 | 0.9 | 1.056 | **3.727** | 1.274 | 3.601 | -71.7% | -3.4% | 64 / 188 | 259 / 806 | 222 / 715 |
| msg-ssh | 3000 | 0.4 | 0.715 | **1.957** | 0.987 | 1.742 | -63.5% | -11.0% | 37 / 143 | 133 / 496 | 130 / 480 |
| num-power | 12 | 33.8 | 16.072 | **16.072** | 6.013 | 5.960 | +0.0% | -62.9% | 226 / 630 | 253 / 689 | 303 / 862 |
| num-power-f32 | 12 | 14.7 | 9.042 | **9.058** | 3.512 | 3.519 | -0.2% | -61.1% | 93 / 767 | 142 / 760 | 173 / 739 |

**`hc`, 1 thread** (zstd level 19; ratio = original / compressed; MB/s compress / decompress)

| family | files | MB | untrained | profile | zstd | zstd + dict | profile vs untrained | profile vs zstd + dict | untrained MB/s | profile MB/s | zstd + dict MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| csv-quakes | 5 | 15.9 | 5.165 | **5.165** | 4.559 | 4.587 | +0.0% | -11.2% | 4 / 372 | 4 / 368 | 2 / 808 |
| json-quakes | 2 | 20.3 | 14.416 | **14.416** | 13.075 | 13.125 | +0.0% | -9.0% | 5 / 1076 | 5 / 1167 | 2 / 1359 |
| log-hadoop | 782 | 33.8 | 17.872 | **35.906** | 16.294 | 26.670 | -50.2% | -25.7% | 7 / 1112 | 8 / 1219 | 3 / 2030 |
| log-openstack | 212 | 55.4 | 16.439 | **18.277** | 14.534 | 15.928 | -10.1% | -12.9% | 13 / 1348 | 24 / 1438 | 3 / 2164 |
| log-ssh | 20 | 21.0 | 59.014 | **60.596** | 25.061 | 25.698 | -2.6% | -57.6% | 24 / 1052 | 35 / 1098 | 3 / 1962 |
| msg-geojson | 6000 | 4.3 | 1.478 | **7.028** | 1.641 | 6.699 | -79.0% | -4.7% | 10 / 316 | 3 / 1116 | 3 / 1031 |
| msg-openstack | 3000 | 0.9 | 1.067 | **3.875** | 1.290 | 3.740 | -72.5% | -3.5% | 12 / 177 | 4 / 770 | 4 / 698 |
| msg-ssh | 3000 | 0.4 | 0.724 | **2.026** | 1.008 | 1.950 | -64.3% | -3.8% | 10 / 105 | 7 / 540 | 7 / 561 |
| num-power | 12 | 33.8 | 17.860 | **17.860** | 10.036 | 10.270 | +0.0% | -42.5% | 16 / 709 | 16 / 728 | 2 / 989 |
| num-power-f32 | 12 | 14.7 | 10.068 | **10.069** | 4.559 | 4.662 | -0.0% | -53.7% | 7 / 719 | 8 / 753 | 3 / 770 |

**`balanced`, 4 threads** (zstd level 3; ratio = original / compressed; MB/s compress / decompress)

| family | files | MB | untrained | profile | zstd | zstd + dict | profile vs untrained | profile vs zstd + dict | untrained MB/s | profile MB/s | zstd + dict MB/s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| csv-quakes | 5 | 15.9 | 3.947 | **3.950** | 3.550 | 3.541 | -0.1% | -10.3% | 316 / 747 | 406 / 998 | 189 / 918 |
| log-hadoop | 782 | 33.8 | 15.974 | **26.017** | 13.787 | 19.520 | -38.6% | -25.0% | 152 / 1297 | 160 / 1287 | 778 / 1894 |
| log-openstack | 212 | 55.4 | 15.550 | **16.826** | 11.197 | 12.329 | -7.6% | -26.7% | 105 / 1489 | 284 / 1463 | 421 / 1741 |
| msg-geojson | 6000 | 4.3 | 1.454 | **5.847** | 1.612 | 5.705 | -75.1% | -2.4% | 112 / 355 | 378 / 1060 | 365 / 1006 |
| msg-ssh | 3000 | 0.4 | 0.715 | **1.957** | 0.987 | 1.742 | -63.5% | -11.0% | 32 / 140 | 110 / 322 | 133 / 485 |
| num-power-f32 | 12 | 14.7 | 9.042 | **9.058** | 3.512 | 3.519 | -0.2% | -61.1% | 118 / 913 | 207 / 944 | 181 / 732 |

### What made the difference (measured during development, `balanced`, one thread)

| change | measured on | before | after | kept |
|---|---|---:|---:|---|
| Dictionary on every zstd/Brotli/LZ4 leaf | csv-quakes | 4,027,001 (untrained) | 4,055,123 (+0.7%) | no: replaced by the leaf limit |
| Leaf limit chosen on held-out samples (`DLIM`) | csv-quakes; log-hadoop | +0.7%; 256 KiB limit 1,468,674 | −0.1%; unlimited chosen 1,449,335 | yes |
| Pinned plans by majority (> 50% of bytes) | log-hadoop, no dictionary | 2,117,037 (untrained) | 2,397,615 (+13.3%) | no |
| … keyed also by size band, pinned at ≥ 90% agreement | log-hadoop, no dictionary | 2,397,615 | 2,154,872 | yes |
| … and the field count of lines 1–32 checked (quote-aware) | log-hadoop, no dictionary | 2,154,872 | 2,091,899 (−1.2% vs untrained) | yes |
| Compact frame (`CZC1`) + magicless zstd frames | msg-geojson | 2,945,080 (untrained) | 762,666 | yes |
| Raw zstd blocks for leaves ≤ 64 KiB | msg-geojson; msg-ssh | 762,666; 212,162 | 732,402; 196,743 | yes |
| COVER dictionary when it wins on the held-out fifth | msg-ssh; log-hadoop | 196,743; 1,398,253 | 182,489 (−7.2%); 1,299,869 (−7.0%) | yes (OpenStack lines: COVER was 23% larger, so fastCover is kept there) |
| Direct path for inputs under 4 KiB (no block machinery) | msg-ssh compress | 67–75 MB/s | 117–157 MB/s | yes (same bytes; checked by the test) |
| Plan pinning: analysis skipped | log-openstack (256 KiB files) compress | 99 MB/s | 278 MB/s | yes |

One bug found on the way: the profile codec path computed "not mine" for leaves above the leaf limit but still
compressed them before handing them back, so those leaves were coded twice (CSV compress at 82 instead of 134 MB/s).
Fixed before any measurement in this report.

### Against OpenZL's trained compressors (not re-measured)

OpenZL could not be built in this session (see *Summary*). The existing comparison in `benchmark/openzl`
(`results.json`, OpenZL 0.3.0 CLI, trained on a quarter of the OWID rows) had untrained Kolmo `fast` already 45–49%
smaller than trained OpenZL on the OWID CSVs (owid 1,063,753 vs 1,937,463 bytes; energy 735,707 vs 1,435,353; wide
1,173,625 vs 1,591,244) and trained OpenZL 46% smaller than Kolmo `fast` on plain Parquet (1,164,877 vs 2,172,919).
Profiles change large CSV files by ≤ 0.1% (above), so on those files the picture would not change; Parquet was not
in this lane's data. A like-for-like run belongs to the integration lane on the Mac, where OpenZL builds.

## 2. `--target`

### What it does

`kolmo --target T` cuts the input into 16 MiB chunks (text at line ends; `-B` changes the size), classifies each chunk
(text, table, log, JSON, numeric, binary), and for each of the nine settings (speed, balanced without analysis,
balanced, eco, hc, ratio -l 1, ratio, -x -l 1, -x) predicts the encode and decode time from a calibration table
(setup + bytes / rate, per setting and class) and the size from trial compressions of the chunk (whole up to 256 KiB,
three 64 KiB slices above). Floors (`decode>=`, `encode>=`) remove settings; the objective is the smallest size, or
the smallest value of a cost formula (`cost:cloud[=months]` is `benchmark/TRADEOFFS.md` model 3, `cost:link=R` model 2,
or any formula of `size`, `n`, `enc`, `dec`). Settings are tried cheapest first and skipped when their time alone, plus
the price of the trial itself, already costs more than the best so far. Chunk frames are joined into one ordinary
frame (`SPEC.md`, *Target frames*).

The built-in table (`src/plan/target_table.inc`, from `calibration-vm.txt`) was measured by `kolmo calibrate` on this
VM on built-in synthetic data, 64 KiB and 512 KiB per class (`-x` runs at 0.1–0.3 MB/s here, so those rows are
measured on small inputs). `kolmo calibrate FILES` measures on the user's own data instead.

### How well it meets the targets

Eight files: seven single files of the families above and `mixed.bin` (23 MB: a CSV month, an NPY month, OpenStack
chunks, a GeoJSON month, a Hadoop log and an SSH chunk, concatenated, in 2 MiB chunks). Every fixed setting was
measured on each file (`-x` only up to 3 MB); the best fixed setting under each target, measured after the fact, is
the oracle. *Choice regret*: the chosen setting's own cost (or size, for floors) against the oracle, for files coded
as one chunk; *total regret*: the target run itself, trials included. For floors, regret is in size and "met" checks
the real speed.

Median and worst regret over the eight files, for each configuration measured (the last one is what the branch ships):

| target | A: first version (1 MiB / 256 KiB trials, trials not priced) | B: trials priced | C: B with a table calibrated on the training files | **D: B with small trials (shipped)** |
|---|---|---|---|---|
| `decode>=1GB/s` (size regret) | 0.0% / +4.0% / +63.8% | same as A | −2.1% / −2.1% / +2.6% (2 floors missed) | **0.0% / +4.0% / +63.8%** (met 8/8) |
| `decode>=500MB/s` (size regret) | 0.0% / 0.0% / +2.6% | same as A | 0.0% / 0.0% / +190.8% | **0.0% / 0.0% / +2.6%** (met 7/8) |
| `encode>=100MB/s` (size regret) | 0.0% / 0.0% / +66.7% (met 6/8) | same as A | 0.0% / 0.0% / +66.7% | not re-run (same code path as A) |
| `cost:cloud` | 0.0% / +75.6% / +229.4% | 0.0% / +45.7% / +187.9% | 0.0% / +24.7% / +311.2% | **+10.5% / +38.6% / +122.2%** |
| `cost:cloud=12` | 0.0% / +16.4% / +50.3% | 0.0% / +15.5% / +48.2% | 0.0% / +42.9% / +184.5% | **0.0% / +18.1% / +49.4%** |
| `cost:link=100Mbit` | 0.0% / +48.4% / +106.9% | 0.0% / +26.8% / +50.3% | 0.0% / +20.1% / +74.0% | **0.0% / +23.3% / +42.2%** |
| `cost:link=1Gbit` | 0.0% / +11.0% / +95.9% | 0.0% / +10.4% / +76.8% | 0.0% / +17.9% / +59.7% | **0.0% / +9.5% / +19.8%** |
| `size` (two files ≤ 1.1 MB) | 0.0% / 0.0% / 0.0% | — | — | — |

Each cell: median choice regret / median total regret / worst total regret. Raw lines: `results/tgbench-*.jsonl`.

**The shipped configuration** (built-in table, trials priced, whole chunk up to 256 KiB, else three 64 KiB slices), per file:

| file | MB | target | chosen (chunks) | size | best fixed setting | its size | met | choice regret | total regret |
|---|---:|---|---|---:|---|---:|---|---:|---:|
| chunk004.log | 1.0 | `decode>=1GB/s` | hc x1 (1) | 26994 | hc | 26994 | yes | +0.0% | +0.0% |
| chunk004.log | 1.0 | `decode>=500MB/s` | hc x1 (1) | 26994 | hc | 26994 | yes | +0.0% | +0.0% |
| chunk004.log | 1.0 | `cost:cloud` | balanced-n x1 (1) | 79698 | balanced | 31756 | — | +112.8% | +122.2% |
| chunk004.log | 1.0 | `cost:cloud=12` | eco x1 (1) | 27929 | eco | 27929 | — | +0.0% | +18.1% |
| chunk004.log | 1.0 | `cost:link=100Mbit` | balanced-n x1 (1) | 79698 | balanced-n | 79698 | — | +0.0% | +22.1% |
| chunk004.log | 1.0 | `cost:link=1Gbit` | speed x1 (1) | 128915 | speed | 128915 | — | +0.0% | +6.2% |
| chunk0100.log | 0.3 | `decode>=1GB/s` | balanced x1 (1) | 17565 | hc | 16883 | yes | +4.0% | +4.0% |
| chunk0100.log | 0.3 | `decode>=500MB/s` | hc x1 (1) | 16883 | eco | 16525 | yes | +2.2% | +2.2% |
| chunk0100.log | 0.3 | `cost:cloud` | balanced x1 (1) | 17565 | eco | 16525 | — | +1.1% | +3.4% |
| chunk0100.log | 0.3 | `cost:cloud=12` | ratio x1 (1) | 14887 | ratio | 14887 | — | +0.0% | +49.4% |
| chunk0100.log | 0.3 | `cost:link=100Mbit` | balanced-n x1 (1) | 24626 | balanced-n | 24626 | — | +0.0% | +16.2% |
| chunk0100.log | 0.3 | `cost:link=1Gbit` | speed x1 (1) | 41016 | speed | 41016 | — | +0.0% | +14.0% |
| 2007-02.npy | 1.1 | `decode>=1GB/s` | speed x1 (1) | 484877 | speed | 484877 | yes | +0.0% | +0.0% |
| 2007-02.npy | 1.1 | `decode>=500MB/s` | hc x1 (1) | 119563 | hc | 119563 | yes | +0.0% | +0.0% |
| 2007-02.npy | 1.1 | `cost:cloud` | balanced x1 (1) | 133194 | balanced | 133194 | — | +0.0% | +67.2% |
| 2007-02.npy | 1.1 | `cost:cloud=12` | hc x1 (1) | 119563 | eco | 123012 | — | +1.4% | +15.2% |
| 2007-02.npy | 1.1 | `cost:link=100Mbit` | balanced-n x1 (1) | 338902 | balanced | 133194 | — | +30.9% | +35.3% |
| 2007-02.npy | 1.1 | `cost:link=1Gbit` | speed x1 (1) | 484877 | speed | 484877 | — | +0.0% | +19.8% |
| 2020-02.csv | 2.3 | `decode>=1GB/s` | speed x1 (1) | 1108923 | balanced-n | 677038 | yes | +63.8% | +63.8% |
| 2020-02.csv | 2.3 | `decode>=500MB/s` | hc x1 (1) | 465439 | balanced-n | 677038 | no (379 MB/s) | -31.3% | -31.3% |
| 2020-02.csv | 2.3 | `cost:cloud` | ratio-l1 x1 (1) | 404092 | balanced | 604583 | — | +2.4% | +16.3% |
| 2020-02.csv | 2.3 | `cost:cloud=12` | ratio-l1 x1 (1) | 404092 | ratio-l1 | 404092 | — | +0.0% | +1.5% |
| 2020-02.csv | 2.3 | `cost:link=100Mbit` | balanced-n x1 (1) | 677038 | balanced-n | 677038 | — | +0.0% | +23.3% |
| 2020-02.csv | 2.3 | `cost:link=1Gbit` | speed x1 (1) | 1108923 | speed | 1108923 | — | +0.0% | +9.5% |
| 2007-02.txt | 2.6 | `decode>=1GB/s` | speed x1 (1) | 747718 | speed | 747718 | yes | +0.0% | +0.0% |
| 2007-02.txt | 2.6 | `decode>=500MB/s` | hc x1 (1) | 152587 | hc | 152587 | yes | +0.0% | +0.0% |
| 2007-02.txt | 2.6 | `cost:cloud` | eco x1 (1) | 166873 | balanced | 169109 | — | +10.5% | +30.4% |
| 2007-02.txt | 2.6 | `cost:cloud=12` | eco x1 (1) | 166873 | hc | 152587 | — | +5.5% | +8.8% |
| 2007-02.txt | 2.6 | `cost:link=100Mbit` | balanced x1 (1) | 169109 | balanced | 169109 | — | +0.0% | +14.7% |
| 2007-02.txt | 2.6 | `cost:link=1Gbit` | speed x1 (1) | 747718 | speed | 747718 | — | +0.0% | -10.3% |
| application_144514442372 | 5.0 | `decode>=1GB/s` | hc x1 (1) | 13433 | ratio | 12811 | yes | +4.9% | +4.9% |
| application_144514442372 | 5.0 | `decode>=500MB/s` | eco x1 (1) | 13140 | ratio | 12811 | yes | +2.6% | +2.6% |
| application_144514442372 | 5.0 | `cost:cloud` | balanced-n x1 (1) | 38691 | balanced | 21899 | — | +28.2% | +38.6% |
| application_144514442372 | 5.0 | `cost:cloud=12` | eco x1 (1) | 13140 | eco | 13140 | — | +0.0% | +27.1% |
| application_144514442372 | 5.0 | `cost:link=100Mbit` | balanced-n x1 (1) | 38691 | balanced-n | 38691 | — | +0.0% | +42.2% |
| application_144514442372 | 5.0 | `cost:link=1Gbit` | speed x1 (1) | 96143 | speed | 96143 | — | +0.0% | +11.3% |
| 2020-02.json | 9.0 | `decode>=1GB/s` | balanced-n x1 (1) | 1027033 | balanced-n | 1027033 | yes | +0.0% | +0.0% |
| 2020-02.json | 9.0 | `decode>=500MB/s` | hc x1 (1) | 631261 | hc | 631261 | yes | +0.0% | +0.0% |
| 2020-02.json | 9.0 | `cost:cloud` | balanced-n x1 (1) | 1027033 | eco | 703247 | — | +17.0% | +26.2% |
| 2020-02.json | 9.0 | `cost:cloud=12` | ratio x1 (1) | 541558 | ratio | 541558 | — | +0.0% | +1.0% |
| 2020-02.json | 9.0 | `cost:link=100Mbit` | balanced-n x1 (1) | 1027033 | balanced-n | 1027033 | — | +0.0% | +4.2% |
| 2020-02.json | 9.0 | `cost:link=1Gbit` | speed x1 (1) | 1733789 | speed | 1733789 | — | +0.0% | +3.4% |
| mixed.bin | 23.0 | `decode>=1GB/s` | speed x2, balanced-n x7, hc x2 (11) | 3449251 | balanced-n | 2779521 | yes | — | +24.1% |
| mixed.bin | 23.0 | `decode>=500MB/s` | eco x1, hc x10 (11) | 1894171 | balanced | 2634049 | yes | — | -28.1% |
| mixed.bin | 23.0 | `cost:cloud` | balanced-n x8, balanced x1, eco x1, ratio-l1 x1 (11) | 2456770 | balanced | 2634049 | — | — | +54.9% |
| mixed.bin | 23.0 | `cost:cloud=12` | balanced-n x1, eco x1, hc x1, ratio-l1 x4, ratio x4 (11) | 1696002 | ratio-l1 | 1613448 | — | — | +31.0% |
| mixed.bin | 23.0 | `cost:link=100Mbit` | speed x1, balanced-n x9, balanced x1 (11) | 2779693 | balanced-n | 2779521 | — | — | +27.7% |
| mixed.bin | 23.0 | `cost:link=1Gbit` | speed x11 (11) | 4633772 | speed | 4623246 | — | — | +7.1% |

| target | files | median choice regret | median total regret | worst total regret |
|---|---:|---:|---:|---:|
| `decode>=1GB/s` | 8 | +0.0% | +4.0% | +63.8% |
| `decode>=500MB/s` | 8 | +0.0% | +0.0% | +2.6% |
| `cost:cloud` | 8 | +10.5% | +38.6% | +122.2% |
| `cost:cloud=12` | 8 | +0.0% | +18.1% | +49.4% |
| `cost:link=100Mbit` | 8 | +0.0% | +23.3% | +42.2% |
| `cost:link=1Gbit` | 8 | +0.0% | +9.5% | +19.8% |


### Findings

- **Floors work.** With the built-in table, `decode>=500MB/s` was met on 7 of 8 files with no size given up in the
  median (worst +2.6%); the miss is the earthquake CSV, where `hc` decoded at 399 MB/s against a predicted 650
  (synthetic tables decode faster than this CSV with its quoted place names). `decode>=1GB/s` was met on all 8; on
  the same CSV the table under-predicts `balanced -n`, so it fell back to `speed` (+64% size).
- **The choice is right, the trials are the cost.** For the cost targets the chosen setting is the best fixed one in
  the median case (choice regret 0%, except `cost:cloud` with small trials: +10.5%). The total regret (11–39% in the
  median for the shipped configuration) is the CPU time of the trials themselves, which the cost formula charges like
  any other CPU time: at the default cloud prices one second of CPU costs as much as storing 0.65 MB for a month.
  Pricing the trials and making them smaller halved the median for `cost:cloud` and `cost:link=100Mbit` and cut the
  worst cases by up to 4×.
- **Calibration on the user's files is not yet reliable.** A table measured on the training files (C) helped two
  cost targets and hurt the rest: two floors were missed and `-x` was chosen for an NPY file under `cost:cloud=12`.
  The two-point fit (64 KiB and 512 KiB per class) is too noisy, above all for `-x`, whose 0.1–0.3 MB/s rate is
  measured on very little data. Next step: fit on more and larger points, and weight classes by the bytes of each.
- **What would remove most of the remaining regret:** predicting the size of the slow settings from the trial of a
  fast one (a per-class ratio learned by `kolmo calibrate`, or stored in a trained profile, where `kolmo train
  --target` already measures every candidate on held-out data), so only one or two settings are trial-compressed.


## Format changes and why

| change | why | cost to other readers |
|---|---|---|
| container version 11, header flag bit 2 + profile id in bytes 6–9 | a frame must say which profile decodes it; an older reader refuses version 11 at the header instead of failing inside a block | none for frames without a profile (their bytes are unchanged; the determinism check covers this) |
| compact frame `CZC1` for one-block profile frames ≤ 1 MiB | 36–38 bytes less per message: the difference between losing and winning against zstd with a dictionary on 100 B–1 KB messages | a reader without profile support reports `KOLMO_ERR_PROFILE` (it needs the profile anyway) |
| zstd leaves of profile frames: raw blocks ≤ 64 KiB, magicless frames above, no content size, dictionary id or checksum | the container records sizes and checks the decoded bytes; 5–6 bytes per leaf | profile frames only |
| `KPF1` profile file | new file type | — |

Final numbering (version 11, codec ids) is the integration lane's decision (`research/RC3-PLAN.md`).

## Limitations and what failed

- **Speed mode on messages.** With LZ4 (the speed tier), profiles make messages 60–70% smaller than untrained Kolmo
  and 2–9% smaller than `lz4 -D` with its frame, but 9–23% larger than bare LZ4 blocks with the same dictionary (no
  framing at all), larger than zstd −1 with a dictionary (LZ4 against zstd), and they decode SSH lines at half the
  speed of bare LZ4 (the per-message checksum and header). Train with `--target` to let the profile pick a zstd
  setting for such data.
- **`hc` on messages** compresses at 3–7 MB/s with or without a dictionary (zstd 19's prepared-dictionary set-up per
  message dominates); the zstd baseline is the same.
- **Training time with COVER**: up to 27 s on 15 MB of samples (one thread, deterministic). `kolmo train --quick`
  keeps zstd's fastCover only (0.1–1 s).
- **zstd raw block API**: the 64 KiB raw-block leaves use `ZSTD_compressBlock`/`ZSTD_decompressBlock`, which zstd
  marks deprecated (still present in 1.5.7). If the deps lane trims zstd, these two functions (and
  `ZSTD_compressBegin_usingCDict`, `ZSTD_decompressBegin_usingDDict`) must stay, or the leaves fall back to magicless
  frames (+5 bytes per message) with a new format revision.
- **Command line**: `-P` and `--target` read the whole input into memory (no `-s` streaming or file-descriptor path
  yet). The Python and Rust bindings do not expose the new API (their `native/` copies are synced by the structure
  and integration lanes).
- **Targets**: the trial overhead above; the calibration on synthetic data mispredicts some real data by 2× (CSV under
  `ratio -l 1`, `hc` decode of CSV near 500 MB/s); `size` on large inputs means `-x` everywhere (slow by design).
- **Pins** are not made for fixed-record plans (they carry per-file field tables) or experimental frontier plans.
- **Context-mixing priors**: room reserved (`CMPR`), no implementation (no priors from the xmode lane yet).

## Reproduce

```sh
python3 -I benchmark/rc3/features-a/prepare.py DATA                 # data (about 90 MB downloaded)
make build/kpbench build/tgbench build/kolmo
./build/kpbench -r 3 -T 1 -z 3 -m balanced DATA/sets/log-hadoop/train DATA/sets/log-hadoop/test   # one family
./build/tgbench -r 3 FILE 'decode>=1GB/s' 'cost:cloud' ...           # one file, any targets
python3 benchmark/rc3/features-a/summarize.py kpbench.jsonl tgbench.jsonl
./build/kolmo calibrate -o table.txt [FILES]; python3 benchmark/rc3/features-a/make_table.py table.txt > src/plan/target_table.inc
```

`benchmark/rc3/features-a/results/` holds the raw JSON lines of every run in this report.
