# A cross-project dictionary for small Python source files

An 8,192-byte zstd dictionary reduced the individual compressed frame size of **all 72 held-out files** across NumPy, Pydantic and SymPy. It saved 7,206 frame bytes in total. Including one dictionary copy and its ZIP directory entry cost 8,300 bytes, so the self-contained dictionary archive was **1,094 bytes larger than plain zstd-3**. On this batch, ordinary Kolmo L3 gave up 12,154 archive bytes to plain zstd-3 and ran 13.3% faster in median paired encode-plus-restore throughput.

## Archive sizes

Each source has an independent compressed frame inside a real ZIP_STORED archive. Entry names, timestamps, permissions, directory layout and member count match across codecs, except for the additional dictionary entry where indicated. All members can be extracted separately. Each cohort has 36 files. The final row is an actual single 72-member source archive, with one additional entry when the dictionary is embedded.

| Cohort | Source bytes | Kolmo L3 | zstd-3 | zstd-3, dictionary already present | zstd-3, dictionary embedded once |
|---|---:|---:|---:|---:|---:|
| Tiny, 64–2,048 B | 22,793 | 20,165 | 15,955 | 14,142 | 22,442 |
| Mid, 4,096–32,768 B | 456,747 | 140,638 | 132,694 | 127,301 | 135,601 |
| All 72 | 479,540 | 160,781 | 148,627 | 141,421 | 149,721 |

The dictionary saved 2,472 frame bytes across the 24 NumPy files, 2,424 across Pydantic and 2,310 across SymPy. At this mix, a dictionary already present at the reader saves 100.08 bytes per file on average. A single embedded copy costs 115.28 bytes per file at 72 files. The projected break-even against plain zstd is 83 files at that average savings, or 165 tiny files or 56 mid-sized files at their respective averages. These are extrapolations. If a reader retains the dictionary across two equally sized batches with the same savings, one transfer would leave a net savings of 6,112 bytes. [Per-file frame lengths and hashes](frames.csv) · [Whole archive sizes and hashes](combined-archives.csv)

## Encode and restore

One warmup and five scored passes per arm and cohort ran on a shared arm64 Mac. The test shuffled arm and cohort order on each pass. The measurements include one CLI launch per member per phase, ZIP creation and reading, dictionary extraction for the dictionary arm, and writing restored files. Throughput uses source MB divided by median seconds, and combined uses encode plus restore time from the same pass. The all-file timing sums the two independent cohort runs within each pass. The single-copy 72-member archive was measured for size only.

| Arm | Tiny combined MB/s | Mid combined MB/s | All encode MB/s | All decode MB/s | All combined MB/s |
|---|---:|---:|---:|---:|---:|
| Kolmo L3 | 0.117 | 2.293 | 2.433 | 2.413 | 1.205 |
| zstd-3 | 0.097 | 2.027 | 2.028 | 2.080 | 1.040 |
| zstd-3, embedded dictionary | 0.099 | 1.998 | 2.108 | 2.046 | 1.038 |

The dictionary's median paired combined speed change against plain zstd was −0.21% overall. Five shared-host passes with per-member process launches provide little evidence about small speed differences or in-process library performance. Dictionary training happened once, offline, before benchmarking. [Raw paired timings](rounds.csv)

## Training and verification

NetworkX 3.7, Pillow 12.1.1 and pytest 9.2.0 supplied 161 whole development files totaling 1,279,775 bytes. zstd 1.5.7 FastCover trained only on those files, with a fixed ID, fixed parameters and one thread. Repeating training reproduced dictionary SHA-256 `7944cb4a33362f3915602d053fb3d44d14dc85478b916393d4b9b6e283cd8f8a`. The test retained 12 files per size band from each of NumPy 2.5.5, Pydantic 2.12.5 and SymPy 1.14.0. Selection sorted package-relative names by SHA-256, excluded designated test/vendor directories and rejected identical file contents globally. All 233 file hashes were distinct. Only the chosen size bands and Python source were tested. [Input inventory](inputs.csv) · [Training parameters and binary hashes](manifest.json)

Ordinary Kolmo used PR #106 at `8d23095b2f09d5c9e3c3425123bea63641ca5d71`, with `-3 -T1` and all Kolmo environment overrides cleared. Every candidate warmup frame also restored with pinned main `a1d39559686a9ec17c8614b9154a64d7b5b98672`. Every timed arm restored all original bytes, covering 1,296 member round trips plus 72 main-reader checks. Dictionary archives restored using the dictionary extracted from that archive. Frame and archive hashes remained identical across repetitions. The results retain hashes and lengths. The reproduction reads already installed projects and trains the dictionary in a temporary directory. [Reproduction script and CLI settings](replay.py)

For self-learning, the size data supports tracking the cost of delivering a dictionary and the number and sizes of future members before choosing it. Retaining independent frames preserves per-file access. These measurements qualify this zstd competitor and identify a size target for Kolmo on Python files. A learned Kolmo encoder, codec selection on new languages, and a production deployment decision would need separate evidence.
