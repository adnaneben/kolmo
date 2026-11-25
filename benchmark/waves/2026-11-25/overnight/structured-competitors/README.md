# B25 · Typed numerical data and weather files

Measured the exact pinned-main CLI and the opt-in integration on the Apple M5 against OpenZL 0.3.0 (50f8789a) in a separate, matched CLI run. Every measured result includes a complete frame, full decode and an untimed SHA-256 check. The randomized records include one warmup and five scored observations. Both phases include process launch, file I/O and, for a trained encoder, reading its graph. Combined throughput uses the median of the observed encode-plus-decode wall times; the old panel’s independent best-phase timers cannot recover this statistic. This is a shared host, warm filesystem cache and requested Kolmo T1. OpenZL uses its normal CLI configuration. Flag numbers identify each codec’s own quality levels; equal flag numbers do not guarantee matching ratios or speed.

| Profile and file | Arm | Original B | Complete frame B | One-time encoder graph B | Encode MB/s | Decode MB/s | Combined MB/s |
|---|---|---:|---:|---:|---:|---:|---:|
| USW00012839-tmax | main-6 | 113,960 | 15,877 | 0 | 1.23 | 17.47 | 1.15 |
| USW00012839-tmax | main-7 | 113,960 | 15,877 | 0 | 1.01 | 16.35 | 0.94 |
| USW00012839-tmax | main-9 | 113,960 | 15,748 | 0 | 2.39 | 18.72 | 2.14 |
| USW00012839-tmax | openzl-6 | 113,960 | 17,761 | 0 | 12.15 | 14.08 | 6.36 |
| USW00012839-tmax | openzl-7 | 113,960 | 13,340 | 0 | 12.11 | 7.44 | 4.65 |
| USW00012839-tmax | openzl-9 | 113,960 | 13,340 | 0 | 13.13 | 17.21 | 7.42 |
| USW00012839-tmax | openzl-trained | 113,960 | 13,313 | 1,032 | 11.87 | 13.89 | 6.53 |
| air2023-npy | main-9 | 61,390,208 | 10,439,792 | 0 | 24.63 | 31.43 | 13.35 |
| air2023-npy | openzl-9 | 61,390,208 | 11,935,555 | 0 | 46.90 | 451.16 | 42.95 |
| air2023-npy | openzl-trained | 61,390,208 | 11,935,555 | 3,912 | 79.60 | 518.69 | 68.29 |
| miami-csv | experiment-1 | 10,134,321 | 1,619,419 | 0 | 646.99 | 1014.83 | 393.84 |
| miami-csv | main-1 | 10,134,321 | 1,619,419 | 0 | 596.99 | 923.25 | 362.56 |
| miami-csv | main-6 | 10,134,321 | 362,656 | 0 | 11.18 | 568.06 | 10.89 |
| miami-csv | main-9 | 10,134,321 | 308,081 | 0 | 7.20 | 287.87 | 7.04 |
| miami-csv | openzl-6 | 10,134,321 | 444,756 | 0 | 76.03 | 187.95 | 53.70 |
| miami-csv | openzl-9 | 10,134,321 | 400,887 | 0 | 31.19 | 244.63 | 27.66 |
| miami-csv | openzl-trained | 10,134,321 | 240,234 | 36,560 | 59.15 | 178.35 | 45.52 |

## What the typed integer transfer shows

Derived a defined product from each station CSV: one signed little-endian int32 TMAX observation per data row, in order, using INT32_MIN for an empty field. The local input filenames carry the .u32 suffix, while both serialization and OpenZL profile are signed le-i32. Fixing this representation removes a parsing ambiguity. This lane verifies and compresses the derived observations only; missing flags, dates and other CSV columns are not recoverable from that vector. The full CSV lane separately verifies every byte of the original file. Four previously enumerated training stations and four different test stations share NOAA GHCN as producer. Their hashes and names are in splits.json. This is a station holdout, not an unseen-producer holdout. The same split had previously been used for a full CSV benchmark. The new le-i32 encoder has never trained on test station bytes.

| Test station | Kolmo L9 B | OpenZL untrained L9 B | OpenZL trained B | Trained + 1 full encoder graph B | Kolmo / untrained / trained combined MB/s |
|---|---:|---:|---:|---:|---:|
| USW00012839-tmax | 15,748 | 13,340 | 13,313 | 14,345 | 2.14 / 7.42 / 6.53 |
| USW00013874-tmax | 24,197 | 21,294 | 21,253 | 22,285 | 1.99 / 8.03 / 8.00 |
| USW00023183-tmax | 19,989 | 17,041 | 17,030 | 18,062 | 2.22 / 7.25 / 8.01 |
| USW00024233-tmax | 18,756 | 16,324 | 16,306 | 17,338 | 2.23 / 6.73 / 7.32 |

## Storage and workload boundaries

All OpenZL test frames decoded using only the built-in decoder (no profile, graph or dictionary argument). Decoder-side bytes beyond the complete frame were zero. The serialized compressor is a one-time encoder deployment cost, not part of the archived frame: CSV 36,560 B, NumPy 3,912 B, fresh signed int32 1,032 B. Shared binary code is not added to either codec’s frame size; executable digests and lengths are recorded. The trained CSV and NumPy graphs were reused from the original profile-specific run. Recorded training durations were 941.57 and 1,234.74 seconds. The fresh int32 graph trained in 10.33 seconds. The timing CSV excludes these offline training times. Warm invocations include graph loading. Across K files sharing one encoder, charge the graph once to the sum of complete frames.

In the historical v0.0.2 comparison, charging the 36,560-byte CSV graph in full per station changes the GHCN size advantage from 19.90–23.75% frame-only to 7.45–13.31%. Across four complete held-out stations it is 19.03% with one deployment. Across four 2023 NCEP grids the old trained NumPy frames plus one graph are collectively 11.12% larger than historical Kolmo L9. Relative humidity was the lone winning grid. These historical sizes are labeled separately from the current-main rows. NCEP training used 2022 and tests use 2023; variables and producer overlap. Fresh whole-file NumPy speed is for the air grid only.

Replay prepare.py with BENCH_PAGE and OUTPUT, then compare.py with BENCH_PAGE MAIN_KOLMO EXPERIMENTAL_KOLMO OUTPUT. Run score.py with the original results/panel.json to reproduce the historical size correction. Corpus retrieval, source revisions and data split recipe are in bench/repro on main. The two cached original encoder graphs and the fresh integer graph accompany the evidence. Re-training may generate a different graph; for exact speed/size replay use these binary artifacts by hash.

## Decision on typed integer routing

Across all four held-out TMAX vectors, Kolmo L9 used 78,690 bytes, OpenZL untrained L6 used 91,928 bytes, and both untrained L7 and L9 used 67,999 bytes. The trained frames used 67,902 bytes. Training saved only 97 further bytes on the four transferred vectors; charging its 1,032-byte encoder once yields 68,934 bytes, 935 more than the untrained L7/L9 total. The untrained L7 numeric graph is therefore the better model-free research target for this typed representation. Prototype explicit type admission and bounded numeric transforms, then test other signed/unsigned widths, missingness and unrelated producers. One weather field and four stations cannot qualify a shipping default.
