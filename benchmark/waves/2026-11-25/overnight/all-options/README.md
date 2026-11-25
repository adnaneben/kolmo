# All-options build against historical and qualified main

The four arms are historical main `4db0425b361012523d8074ca6f2a760d9466e9f4`, qualified main `a1d39559686a9ec17c8614b9154a64d7b5b98672`, a repeat of the same qualified-main executable (A/A), and the opt-in integrated build `ac10ce80048ab0664074f7ba031634055f4d2225`. Source revisions and build options were supplied from build records. The harness measured the actual executable digests and the source and input byte hashes, which are in manifest.json.

The recorded combined build uses `-O3` `-mcpu=native` `-ffp-contract=off` `-DKOLMO_FAST_TABLE_TRIAL` `-DKOLMO_FAST_TYPED_TABLE_TRIAL` `-DKOLMO_AUDIO_SCREEN` `-DKOLMO_GEAR_ASM`. The runner set `KOLMO_AUDIO_SCREEN=1`. The learning tool was outside the timed CLI path.

Each of the 12 workload, level and thread settings had one warmup and 7 scored rounds per arm. raw.csv contains 384 timed encode/restore rows (48 warmup and 336 scored). The runner wrote each warmup row after an additional byte-exact readback with qualified main, giving 48 additional successful checks. These additional readbacks have no separate timing rows. Archive trials also required the saved file mode and modification time on timed restores.

The archive workload is the complete enwik8 ZIP as one member. Ordinary file inputs are the full OWID CO₂ table, the first 65,536 bytes of the supplied prose file, and the whole Pydub WAV. Only the listed levels and requested thread counts were tested.

| Input | Level | Threads | Stored B pre / main / combined | Encode MB/s pre / main / combined | Decode MB/s pre / main / combined | Combined MB/s pre / main / combined | Paired combined gain vs main / AA | A/A change |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| enwik8.zip | 1 | 1 | 36,492,173 / 36,492,173 / 36,492,173 | 476.59 / 513.18 / 566.39 | 2191.08 / 1946.20 / 2184.30 | 382.97 / 411.04 / 441.21 | +11.0% / +8.6% | -2.1% |
| enwik8.zip | 1 | 4 | 36,492,173 / 36,492,173 / 36,492,173 | 578.01 / 629.77 / 748.43 | 2497.93 / 2447.71 / 2527.41 | 471.24 / 508.68 / 575.88 | +11.7% / +7.4% | +5.9% |
| owid-co2.csv | 1 | 1 | 6,724,159 / 6,724,159 / 2,210,685 | 355.72 / 367.34 / 262.80 | 1112.90 / 1078.96 / 648.09 | 270.33 / 281.64 / 187.92 | -31.9% / -29.0% | -7.6% |
| owid-co2.csv | 1 | 4 | 6,724,159 / 6,724,159 / 2,210,685 | 723.54 / 758.49 / 584.64 | 1695.35 / 1672.21 / 1238.41 | 507.12 / 515.91 / 393.53 | -21.4% / -23.1% | -2.0% |
| owid-co2.csv | 2 | 1 | 4,393,006 / 4,393,006 / 1,927,296 | 306.47 / 300.89 / 253.57 | 767.07 / 745.84 / 582.04 | 223.99 / 213.54 / 175.70 | -17.2% / -15.9% | +1.3% |
| owid-co2.csv | 2 | 4 | 4,393,006 / 4,393,006 / 1,927,296 | 617.19 / 627.43 / 569.84 | 1366.83 / 1307.30 / 975.75 | 431.76 / 428.10 / 361.47 | -18.8% / -23.1% | +4.2% |
| prose-64k | 3 | 1 | 19,211 / 19,211 / 19,211 | 10.28 / 11.08 / 9.52 | 12.42 / 12.52 / 12.78 | 6.04 / 6.03 / 5.91 | -4.2% / -5.2% | -10.4% |
| prose-64k | 3 | 4 | 19,211 / 19,211 / 19,211 | 9.05 / 11.02 / 8.71 | 11.78 / 16.11 / 11.75 | 5.12 / 5.59 / 4.81 | -13.5% / -1.3% | -14.4% |
| pydub.wav | 7 | 1 | 232,286 / 232,286 / 232,286 | 3.75 / 3.78 / 26.21 | 65.64 / 66.93 / 58.39 | 3.58 / 3.56 / 18.67 | +439.9% / +430.6% | -1.3% |
| pydub.wav | 7 | 4 | 232,286 / 232,286 / 232,286 | 4.28 / 4.51 / 26.52 | 56.83 / 56.59 / 55.99 | 3.97 / 4.16 / 19.71 | +369.4% / +363.6% | -0.1% |
| pydub.wav | ultra | 1 | 223,523 / 223,523 / 223,523 | 1.90 / 1.86 / 11.02 | 13.46 / 12.77 / 13.27 | 1.67 / 1.62 / 6.24 | +279.2% / +274.4% | -0.4% |
| pydub.wav | ultra | 4 | 223,523 / 223,523 / 223,523 | 1.92 / 1.87 / 11.26 | 14.23 / 12.88 / 13.25 | 1.69 / 1.62 / 6.09 | +278.2% / +276.0% | -1.7% |

Decimal MB/s uses original bytes and wall time including CLI startup and file I/O. Combined speed uses the median of each observed encode-plus-decode duration. paired.csv reports median within-round gains and win counts. score.csv retains per-arm phase rates and peak RSS medians. These are warm-cache measurements on a shared host. Worker occupancy, p95 latency and native x86 performance were outside this run.

To replay these inputs and executables, run replay.py with --verify-reference, the three binary paths, --archive, --csv, --prose, --audio and a fresh --out directory. Run score.py --data-dir on that directory.

A separate hash-pinned replay reproduced all 48 arm/setting frame hashes, stored sizes and codec counts. Its 96 timed restores and 48 additional qualified-main readbacks passed. The table above uses only the original seven-round measurements.
