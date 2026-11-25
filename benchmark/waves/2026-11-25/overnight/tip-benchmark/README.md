# All-options build against historical and qualified main

The four arms are historical main `4db0425b361012523d8074ca6f2a760d9466e9f4`, qualified main `a1d39559686a9ec17c8614b9154a64d7b5b98672`, a repeat of the same qualified-main executable (A/A), and the opt-in integrated build `8d23095b2f09d5c9e3c3425123bea63641ca5d71`. Source revisions and build options were supplied from build records. The harness measured the actual executable digests and the source and input byte hashes, which are in manifest.json.

The recorded combined build uses `-O3` `-mcpu=native` `-ffp-contract=off` `-DKOLMO_FAST_TABLE_TRIAL` `-DKOLMO_FAST_TYPED_TABLE_TRIAL` `-DKOLMO_AUDIO_SCREEN` `-DKOLMO_GEAR_ASM`. The runner set `KOLMO_AUDIO_SCREEN=1`. The learning tool was outside the timed CLI path.

Each of the 12 workload, level and thread settings had one warmup and 7 scored rounds per arm. raw.csv contains 384 timed encode/restore rows (48 warmup and 336 scored). The runner wrote each warmup row after an additional byte-exact readback with qualified main, giving 48 additional successful checks. These additional readbacks have no separate timing rows. Archive trials also required the saved file mode and modification time on timed restores.

The archive workload is the complete enwik8 ZIP as one member. Ordinary file inputs are the full OWID CO₂ table, the first 65,536 bytes of the supplied prose file, and the whole Pydub WAV. Only the listed levels and requested thread counts were tested.

| Input | Level | Threads | Stored B pre / main / combined | Encode MB/s pre / main / combined | Decode MB/s pre / main / combined | Combined MB/s pre / main / combined | Paired combined gain vs main / AA | A/A change |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| enwik8.zip | 1 | 1 | 36,492,173 / 36,492,173 / 36,492,173 | 563.94 / 582.41 / 619.37 | 2276.97 / 2253.63 / 2253.85 | 451.51 / 464.15 / 490.99 | +9.8% / +11.4% | -2.8% |
| enwik8.zip | 1 | 4 | 36,492,173 / 36,492,173 / 36,492,173 | 687.53 / 732.24 / 812.52 | 2567.14 / 2407.11 / 2653.31 | 535.11 / 556.85 / 613.67 | +16.4% / +16.5% | +0.1% |
| owid-co2.csv | 1 | 1 | 6,724,159 / 6,724,159 / 2,210,685 | 427.91 / 432.47 / 334.66 | 1348.65 / 1335.04 / 817.68 | 322.25 / 317.63 / 224.50 | -27.6% / -26.7% | -1.4% |
| owid-co2.csv | 1 | 4 | 6,724,159 / 6,724,159 / 2,210,685 | 891.89 / 953.15 / 622.55 | 1976.54 / 1991.72 / 1358.48 | 632.36 / 651.94 / 428.73 | -30.6% / -28.6% | -2.5% |
| owid-co2.csv | 2 | 1 | 4,393,006 / 4,393,006 / 1,927,296 | 347.42 / 367.55 / 300.39 | 994.76 / 990.55 / 701.45 | 258.34 / 264.48 / 210.15 | -19.0% / -19.0% | +0.1% |
| owid-co2.csv | 2 | 4 | 4,393,006 / 4,393,006 / 1,927,296 | 808.33 / 797.47 / 644.39 | 1564.83 / 1491.49 / 1235.76 | 537.03 / 519.63 / 422.08 | -19.3% / -16.4% | -4.7% |
| prose-64k | 3 | 1 | 19,211 / 19,211 / 19,211 | 13.49 / 13.47 / 12.79 | 17.93 / 18.03 / 18.16 | 7.78 / 7.67 / 7.57 | +3.8% / +0.0% | -1.5% |
| prose-64k | 3 | 4 | 19,211 / 19,211 / 19,211 | 13.38 / 13.32 / 12.97 | 17.98 / 18.06 / 15.32 | 7.64 / 7.59 / 7.02 | -5.7% / -5.5% | +0.7% |
| pydub.wav | 7 | 1 | 232,286 / 232,286 / 232,286 | 4.52 / 4.21 / 31.74 | 77.10 / 75.24 / 75.07 | 4.26 / 3.99 / 22.31 | +461.6% / +421.6% | +4.0% |
| pydub.wav | 7 | 4 | 232,286 / 232,286 / 232,286 | 4.57 / 4.87 / 28.47 | 69.94 / 71.39 / 76.44 | 4.28 / 4.56 / 20.54 | +359.1% / +346.7% | -1.5% |
| pydub.wav | ultra | 1 | 223,523 / 223,523 / 223,523 | 1.93 / 2.13 / 12.89 | 14.56 / 14.90 / 14.47 | 1.67 / 1.87 / 6.79 | +265.7% / +282.1% | -0.2% |
| pydub.wav | ultra | 4 | 223,523 / 223,523 / 223,523 | 2.38 / 2.11 / 12.82 | 15.47 / 15.26 / 15.28 | 2.05 / 1.85 / 6.87 | +266.4% / +263.4% | -0.3% |

Decimal MB/s uses original bytes and wall time including CLI startup and file I/O. Combined speed uses the median of each observed encode-plus-decode duration. paired.csv reports median within-round gains and win counts. score.csv retains per-arm phase rates and peak RSS medians. These are warm-cache measurements on a shared host. Worker occupancy, p95 latency and native x86 performance were outside this run.

Use the shared [replay](../all-options/replay.py) with --reference pointing at this manifest, --verify-reference, the three binary paths, --archive, --csv, --prose, --audio and a fresh --out directory. Run the shared [scorer](../all-options/score.py) with --data-dir on that directory.
