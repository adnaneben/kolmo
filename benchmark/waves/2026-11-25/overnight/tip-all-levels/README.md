# PR #106 ordinary presets at 8d23095b

On a shared arm64 Mac, the ordinary CLI produced **identical compressed bytes in all 80 settings**. The median paired change in combined encode/decode throughput across settings was **+0.28% against main and +0.24% against the repeat of main**. The two main runs differed by +0.14%. These measurements do not establish an ordinary-preset speed improvement.

The reference was pinned main `a1d39559686a9ec17c8614b9154a64d7b5b98672`. The candidate was PR #106 at `8d23095b2f09d5c9e3c3425123bea63641ca5d71`, built with the experimental options compiled in. The ordinary commands requested only level and thread count for compression. The two main arms used the same binary, SHA-256 `b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f`. The candidate binary was `dbd92f4a170e36c9701aec15236648cd7b78e9f90a180fc8ffd79661aebbb6f2`. Both digests were independently matched to local executables. [Revisions, inputs and run settings](manifest.json)

## Ordinary CLI measurements

The grid covered L1 through L9 and Ultra, with one or four requested threads. The four inputs were prefixes of prose (16,384 bytes), Python source (65,536) and Gaia CSV (262,144), plus a complete ESC-50 clip (441,044). A recheck of the original files and measured slices matched all eight manifest hashes. The initial screen recorded 240 warmup and 720 scored round trips across three arms. Each of the 80 candidate warmup frames also passed a separate stock-main restore. Two selected settings received another six warmups and 66 scored round trips. All 1,032 recorded restores matched their input hashes. The 82 scored rows' phase medians, paired wall-time changes and frame identity were independently checked against the observations. [Ordinary observations](rounds.csv) · [Size, encode, decode, combined and CPU results for every setting](score.csv)

| Setting | Initial vs main | Confirmation vs main | Confirmation vs repeat main | Repeat main vs main in confirmation | Confirmation combined MB/s, main / repeat / candidate |
|---|---:|---:|---:|---:|---:|
| ESC-50 L1, 4 threads | −8.96% | −1.86% | −2.13% | −1.72% | 104.04 / 100.18 / 100.09 |
| Prose 16 KiB L3, 4 threads | +9.72% | −1.95% | −1.52% | +0.18% | 4.27 / 4.23 / 4.02 |

The largest qualifying initial slowdown narrowed on repetition. The strongest initial gain reversed. The screen scored three rounds per arm per setting and the confirmation scored eleven. Combined MB/s divides original bytes by the median wall time of a full encode and decode. Percentage changes use the median paired ratio within a setting, giving each setting equal weight in the grid summary. Timings include process launch and warm-input file I/O. Effective worker counts were unmeasured. The grid covers four samples on one machine. [Selection](confirmation-selection.json) · [Confirmation observations](confirmation-rounds.csv) · [Confirmation scores](confirmation-score.csv)

## Exact-tip opt-in comparison

A separate run used the same candidate binary, targeted experimental options and `KOLMO_AUDIO_SCREEN=1`. It covered 12 settings on four workloads with four arms, totaling 48 warmups and 336 scored round trips. Independent recalculation from the raw timings confirmed the size, encode, decode and combined entries for all 48 arm/setting combinations. The ZIP measurement includes archive creation and complete restore.

| Input and setting | Stored bytes, main → candidate | Combined MB/s, main → candidate | Paired change vs main / repeat main |
|---|---:|---:|---:|
| Complete enwik8 ZIP, L1 / 4 threads | 36,492,173 → 36,492,173 | 556.85 → 613.67 | +16.4% / +16.5% |
| Complete OWID CO₂ CSV, L1 / 1 thread | 6,724,159 → 2,210,685 | 317.63 → 224.50 | −27.6% / −26.7% |
| Prose 64 KiB, L3 / 4 threads | 19,211 → 19,211 | 7.59 → 7.02 | −5.7% / −5.5% |
| Complete Pydub WAV, L7 / 1 thread | 232,286 → 232,286 | 3.99 → 22.31 | +461.6% / +421.6% |

The CSV option stored 67.12% fewer bytes in the illustrated setting and took longer. At equal size, WAV and ZIP improved while the prose control slowed. The [exact-tip PDF](../tip-benchmark/all-options-performance.pdf) has SHA-256 `b1d8fa95ad94a091dc801b69a032536d060ee8060ac1f3043b3fbe5ddaf1502c`. [Opt-in settings and provenance](../tip-benchmark/manifest.json) · [All 12 settings](../tip-benchmark/score.csv) · [Paired changes](../tip-benchmark/paired.csv) · [Raw observations](../tip-benchmark/raw.csv)
