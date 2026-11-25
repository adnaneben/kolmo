# Public bounds and empty-policy export stacked on the integrated candidate

The executable source is `8ef0c6a53d8b8ff3fe8ca7801576f417d0350fe0`, tree `5ec4b746292153fef37a0c93b245e334bcee7a24`. The control is the measured #100 source `5af8c465307315cc835492c54e31567cf4b8ac59`. The stack contains B46 and the empty-policy export with its frozen experiment. The archive conflict resolution retains #100’s thread-safe initialization, name-length checks and reference-allocation checks. All 348 tracked bundled native source and header files matched their canonical versions in this tree.

Fresh, separate Apple ARM64 checkouts used Apple Clang 21.0.0, with identical optimization settings within each comparison. SHA-256 identifies the native files actually loaded.

| Build | CLI SHA-256 | Shared library SHA-256 |
|---|---|---|
| #100 default | 13859a0e2e2aa39413b2da6353a08c53191bb71b307eed509e085557dfac4393 | 3b0a92554333f591f03c14462ce3e71a859eb3e81f3a42be2f2d9a6e6cb93902 |
| Stack default | 552bd577b79b0cb48561747f613dd2ff8ee82d9c1eefba9b2e2a653f277a259f | bbd8103e081d8c3643ced8f50e40bc97961c3a4a5062bc35d1ff354dd5d9f4cc |
| Stack GEAR_ASM=1 | 37bcd85dfc9356e6222c37250638537a896b7d78c5ef0d1114841c186ad147c6 | 3a1c7b74313b3b032d450b4c13befc92baa826c4b0c95c3a3f0be3dd2daa165f |

## Capacity, archive and complete-file regressions

Both stack builds independently ran the 144 valid plain/indexed configurations and the 18 valid archive configurations against #100, with caller-provided exact bounds, complete frame equality and cross-restores. Both builds reproduced all 12 CLI archive cells including bytes, member mode, mtime, names and contents, at levels 1/3/9, T1/T4 and 1/4 KiB chunks. Each also passed 24 selected-member cases. The three generated tables were identical across build settings and identical to the original receipts. `matched-tables.json` records their paths, counts and hashes.

`files.py` exercised 28 configurations over six complete files, covering levels 1–9 and ultra, T1/T2/T4 and input sizes from 64,777 to 3,737,294 bytes. All three encoders emitted identical complete frames and all three readers restored the original file. `files.csv` records every input, setting, stored size and hash. The matrix samples executable, prose, NumPy, image bytes and JSON inputs. The 144-row public API matrix separately covers gzip, USTAR, structured tables and block/memory controls.

The default feature suite passed 3,714 checks, its ASan/UBSan variant passed 3,744, and its CLI tests and deterministic all-level suite passed. Both stack CLIs decoded and tested 274 frozen frames including streams. Both optimized and sanitized native assembly checks matched 28,784 positions and continuation hashes plus 257 guarded tails. The default and assembly Rust bindings each passed nine unit tests, two integration tests and one doctest. The Python binding round-tripped a complete 64,777-byte specification at ten levels and four block settings through each stack library.

## Empty policy deployment

All 14 policy tests passed, including the real-binary integration test. The original hash-verified experiment again exported an exact copy of its pinned stock executable, SHA-256 `b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f`, with released 0.0.2 reader `5c116ba0adb839757a3bfbcd53c334df354e804ab5fd9b2281ee1163e26221a5`. The replay verified 96 whole-file restores. Every original identity, trial order, frame hash and stored byte count was reproduced. See `export-replay.json` for the observations from this checkout and `benchmark/learning-export-ppp` for the original controlled speed study.

An explicit attempt to deploy this policy with the newly built stack executable was refused for its changed fingerprint. The destination was absent afterwards. The checked-in learning evidence therefore authorizes only the original executable and reader pair. A policy intended for a new binary needs validation with that binary.

The compact CM test agreed with both reference models and the legacy stream. The ASan/UBSan CM4 suite passed. The library decoded 357 frozen frames, and the frozen SHA-256 checks verified all 416 files.

## Matched archive speed on the complete optimized configurations

The exact #100 executable from its original performance receipt had SHA-256 `364ed476153f4973f3e53bbe33887b8870ae50c93cd41fe94b78aa35bf141800`. The stack executable from a separate fresh checkout had SHA-256 `c9f785f48f428731a8a38c01d35d2ae05c5450096fc554575b9ba596a5c91ba5`. Both were built with `make -j2 GEAR_ASM=1 CFLAGS='-O3 -mcpu=native -DKOLMO_FAST_TABLE_TRIAL -DKOLMO_FAST_TYPED_TABLE_TRIAL' build/kolmo`. The Makefile appends `-ffp-contract=off`.

Each of 22 seeded, shuffled triplets created a level-1, four-thread, single-member archive of the complete 36,445,475-byte enwik8.zip and fully extracted it. The first triplet warmed caches. Every timed restore matched the input, mode and mtime. The #100 CLI independently restored all three warmup archives. All 66 archives were exactly 36,492,173 bytes, SHA-256 `53632efd7160ae49830e512ce1aed8769a149e73a370dba4d3fc9d278e9cf19d`, the same as the earlier #100 receipt. Input SHA-256 was `547994d9980ebed1288380d652999f38a14fe291a6247c157c3d33d4932534bc`. This was a shared Apple M5; 1-minute system load averages ranged from 11.57 to 11.62 during these trials.

| Tested executable | Encode MB/s | Decode MB/s | Combined MB/s |
|---|---:|---:|---:|
| #100 | 692.60 | 2,272.67 | 523.33 |
| #100 same-binary A/A | 694.53 | 2,442.47 | 536.85 |
| Stack | 684.30 | 2,450.18 | 529.01 |

Rates use original decimal MB divided by the median wall time, charging process startup and filesystem I/O. Combined rates use each round’s encode-plus-decode time. The median paired combined throughput change was −0.15% against #100 (10/21 wins), −3.40% against the A/A arm (8/21), and +1.88% for the same-binary A/A arm against #100 (12/21). This control does not resolve an incremental improvement from B46 or the policy exporter. `archive-speed.csv` includes per-round hashes, phase times, RSS and system load. Replay using `archive-speed.py --base OLD --stack NEW --input enwik8.zip --output fresh.csv`, then `archive-score.py fresh.csv`.

A separate complete 14,377,942-byte OWID CO₂ CSV check (SHA-256 `7f78e2b218ce4bb8c538bbec04fdc9a7982e8d40bff972e650df603899edd5f6`) exercised the two typed trial flags at L1/T1. The two optimized executables each wrote the same 2,210,685-byte frame, SHA-256 `ba6b23d0e8a65a2c782db3c7a712dcc929a0747f2fe352611d1715965774bdb6`. Both and the released v0.0.2 reader restored the complete input. This file check measured byte compatibility only.
