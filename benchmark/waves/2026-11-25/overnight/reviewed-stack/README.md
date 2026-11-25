# All-options build against historical and qualified main

The four arms are historical main `4db0425b361012523d8074ca6f2a760d9466e9f4`, qualified main `a1d39559686a9ec17c8614b9154a64d7b5b98672`, a repeat of the same qualified-main executable (A/A), and the opt-in integrated build `c8fc5ac54aa302baf6abc66f210617901f9e6e2e`. Source revisions and build options were supplied from build records. The harness measured the actual executable digests and the source and input byte hashes, which are in manifest.json.

The recorded combined build uses `-O3` `-mcpu=native` `-ffp-contract=off` `-DKOLMO_FAST_TABLE_TRIAL` `-DKOLMO_FAST_TYPED_TABLE_TRIAL` `-DKOLMO_AUDIO_SCREEN` `-DKOLMO_GEAR_ASM`. The runner set `KOLMO_AUDIO_SCREEN=1`. The learning tool was outside the timed CLI path.

Each of the 12 workload, level and thread settings had one warmup and 7 scored rounds per arm. raw.csv contains 384 timed encode/restore rows (48 warmup and 336 scored). The runner wrote each warmup row after an additional byte-exact readback with qualified main, giving 48 additional successful checks. These additional readbacks have no separate timing rows. Archive trials also required the saved file mode and modification time on timed restores.

The archive workload is the complete enwik8 ZIP as one member. Ordinary file inputs are the full OWID CO₂ table, the first 65,536 bytes of the supplied prose file, and the whole Pydub WAV. Only the listed levels and requested thread counts were tested.

| Input | Level | Threads | Stored B pre / main / combined | Encode MB/s pre / main / combined | Decode MB/s pre / main / combined | Combined MB/s pre / main / combined | Paired combined gain vs main / AA | A/A change |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| enwik8.zip | 1 | 1 | 36,492,173 / 36,492,173 / 36,492,173 | 443.69 / 398.40 / 489.19 | 1791.99 / 1769.02 / 1735.46 | 351.72 / 326.04 / 381.62 | +24.4% / +8.8% | +13.8% |
| enwik8.zip | 1 | 4 | 36,492,173 / 36,492,173 / 36,492,173 | 403.15 / 468.78 / 577.20 | 2004.86 / 2251.77 / 1997.81 | 323.12 / 391.76 / 444.03 | +12.7% / +15.2% | -3.8% |
| owid-co2.csv | 1 | 1 | 6,724,159 / 6,724,159 / 2,210,685 | 308.92 / 295.56 / 243.64 | 786.35 / 1004.92 / 617.81 | 222.79 / 235.71 / 168.13 | -28.7% / -22.6% | -7.8% |
| owid-co2.csv | 1 | 4 | 6,724,159 / 6,724,159 / 2,210,685 | 632.67 / 623.33 / 462.53 | 1555.42 / 1392.67 / 1133.47 | 454.68 / 422.21 / 331.53 | -26.1% / -22.9% | -1.9% |
| owid-co2.csv | 2 | 1 | 4,393,006 / 4,393,006 / 1,927,296 | 256.73 / 188.79 / 191.47 | 628.85 / 656.97 / 458.28 | 196.83 / 140.07 / 129.84 | -13.6% / -25.5% | +16.2% |
| owid-co2.csv | 2 | 4 | 4,393,006 / 4,393,006 / 1,927,296 | 572.00 / 573.86 / 428.43 | 1107.13 / 1105.38 / 856.82 | 374.72 / 362.39 / 253.30 | -26.6% / -20.6% | -11.3% |
| prose-64k | 3 | 1 | 19,211 / 19,211 / 19,211 | 8.94 / 7.04 / 7.35 | 10.70 / 10.97 / 10.17 | 4.87 / 4.20 / 4.27 | +0.1% / +5.2% | +5.4% |
| prose-64k | 3 | 4 | 19,211 / 19,211 / 19,211 | 7.69 / 8.51 / 7.55 | 13.38 / 10.39 / 9.96 | 4.58 / 4.74 / 4.30 | +4.0% / +3.5% | +0.5% |
| pydub.wav | 7 | 1 | 232,286 / 232,286 / 232,286 | 2.74 / 3.28 / 20.24 | 47.93 / 49.38 / 53.16 | 2.55 / 3.11 / 14.54 | +404.4% / +433.0% | -6.4% |
| pydub.wav | 7 | 4 | 232,286 / 232,286 / 232,286 | 3.77 / 3.66 / 20.40 | 54.09 / 50.84 / 52.90 | 3.54 / 3.42 / 14.74 | +332.0% / +380.0% | -4.2% |
| pydub.wav | ultra | 1 | 223,523 / 223,523 / 223,523 | 1.42 / 1.40 / 9.29 | 10.08 / 11.21 / 10.40 | 1.27 / 1.26 / 4.94 | +315.0% / +362.8% | -10.2% |
| pydub.wav | ultra | 4 | 223,523 / 223,523 / 223,523 | 1.46 / 1.34 / 10.75 | 10.68 / 8.96 / 10.33 | 1.29 / 1.16 / 5.42 | +392.6% / +407.7% | -3.0% |

Decimal MB/s uses original bytes and wall time including CLI startup and file I/O. Combined speed uses the median of each observed encode-plus-decode duration. paired.csv reports median within-round gains and win counts. score.csv retains per-arm phase rates and peak RSS medians. These are warm-cache measurements on a shared host. Worker occupancy, p95 latency and native x86 performance were outside this run.

Use the shared [replay](../all-options/replay.py) with --reference pointing at this manifest, --verify-reference, the three binary paths, --archive, --csv, --prose, --audio and a fresh --out directory. Run the shared [scorer](../all-options/score.py) with --data-dir on that directory.
