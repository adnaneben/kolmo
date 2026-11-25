# Draft PR #107 on the #106 base

[Draft PR #107](https://github.com/adnaneben/kolmo/pull/107) contains one FASTQ commit, `31fd3acd922490143059ac2438e988e38d89be5f`. Its sole parent is the #106 head `8d23095b2f09d5c9e3c3425123bea63641ca5d71`. Fresh ordinary builds of both commits produced byte-identical native executables on the Apple M5. Their SHA-256 is `c40c238ab3e023364b7b14c6fa9c3fae3e06e7dda91bb9d9a2cf1586554bacc7`. The option-enabled head build has SHA-256 `76cc1bd8f8e0958b939b9d010ff6afb96afec5565ce8bd4a3380dd06428b694f`. [Build identities](validation.json)

| Input | Block size | Fixed → aligned bytes | Encode ms | Stock decode ms | Combined MB/s |
| --- | --- | ---: | ---: | ---: | ---: |
| SRR1770413 | 1 MiB | 471,176 → 434,740 | 2,015.9 → 202.3 | 2,175.5 → 228.7 | 0.562 → 5.475 |
| SRR2584863 | 1 MiB | 514,763 → 501,487 | 2,105.2 → 222.2 | 2,322.0 → 263.2 | 0.529 → 4.861 |
| SRR1770413 | Automatic | 405,778 → 405,778 | 268.3 → 282.0 | 295.2 → 293.8 | 4.114 → 4.163 |
| SRR2584863 | Automatic | 485,938 → 485,938 | 324.4 → 311.2 | 348.8 → 352.0 | 3.339 → 3.474 |

The rows use one encoder thread and three alternating paired repetitions per arm on a shared host. Both forced-block cases changed three codec-7 blocks into three codec-16 blocks. Automatic Ultra wrote one identical codec-16 block in both arms on each input. Level 3 also wrote identical frames. [Raw explicit-block timings and file hashes](raw.csv) · [Raw automatic timings](automatic.csv) · [Scorecard](summary.csv)

The unmodified #106 reader restored every timed frame, and the released v0.0.2 reader independently restored each first-pair variant. Eight extra regression cases cover variable widths and unchanged behavior for mixed or malformed records, large records, ordinary text and level 3. The measured source windows and full offset digests are the same as the initial #104 experiment. Run `python3 ../score.py .` here to verify the receipts. [Source windows and offset digests](inputs.json) · [Fallback and stock-readback data](regression.jsonl)


## Available source length

Each of the three local public-run prefixes ends at exactly 100,000 complete records. SRR1770413 has 63,177,790 B, SRR2584863 has 36,637,534 B and SRR835775 has 22,877,790 B. Unconstrained Ultra selects 134,217,728 B per block, so a genuine two-block input needs at least 71,039,939 additional bytes for SRR1770413 or 97,580,195 for SRR2584863. The current local sources cannot measure a natural crossing at that setting. The native codec-family ASan/UBSan suite passed on the #107 source with the option compiled and enabled. [Complete source hashes, extents and boundary digests](source-capacity.json) · [Exact compiled block-target probe](block-target.c) · [Probe output](block-target.txt) · [Sanitizer output](fqfam-sanitizer.log)

### Automatically selected blocks under a 256 MiB memory limit

A second run on longer, contiguous original slices used `-U -T1 --memlimit 256M` with no `--block-size`. The unmodified #106 encoder chose an 8 MiB boundary in each slice. The option retained a complete record at 8,388,444 B and 8,388,327 B respectively. Both blocks switched from codec 7 to 16. Two repetitions per arm alternated order.

| Input | Fixed → aligned archive B | Encode ms | Stock decode ms | Combined MB/s |
| --- | ---: | ---: | ---: | ---: |
| SRR1770413, 8,651,356 B | 1,658,579 → 1,351,098 | 8,709.1 → 734.2 | 9,746.4 → 934.9 | 0.469 → 5.183 |
| SRR2584863, 8,651,069 B | 1,856,619 → 1,671,261 | 11,072.3 → 1,024.4 | 11,117.6 → 1,402.3 | 0.390 → 3.565 |

These are 18.54% and 9.98% size reductions. The #106 reader restored all eight larger timed archives to the original SHA-256, and v0.0.2 independently restored each first-repeat variant. Unconstrained Ultra selects a 128 MiB boundary. The complete local public-run prefixes are 63,177,790 B and 36,637,534 B, so that unconstrained crossing remains unmeasured. The existing FASTQ codec-family ASan/UBSan suite passed with the option enabled.

[Raw capped phase timings and archive hashes](capped-large.csv) · [Larger original input hashes and record counts](capped-large-inputs.json) · [Capped scorecard](capped-score.csv) · [Sub-boundary negative control](capped-small.csv) · [Capped verification script](../capped_score.py)
