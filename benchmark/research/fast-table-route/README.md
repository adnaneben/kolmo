# Fast levels on delimited tables

Decision on 25 November 2026: keep both routes experimental. The literal transpose failed the size gate on all eight 2 MiB smoke inputs (audio used the whole shorter file). Typed columns consistently reduced stored bytes on energy, CO₂ and Backblaze. Encodes and stock decodes were slower. The severe cost on the 133 MB whole Backblaze file rules out unconditional use in fast presets. Numeric size savings transferred to whole files. The GHCN CSV kept its original frame.

## Whole-file single-thread results

Decimal MB/s. Each throughput is the median across seven timed CLI observations. Combined throughput uses each observation's original bytes divided by encode + decode wall time. D is the pinned-main decoder, for all Kolmo rows. Arm A represents one control; results.csv retains the second control, both thread counts, paired deltas and RSS.

| File | Level | Main → typed bytes | Main → typed encode | Main → typed decode | Main → typed combined |
|---|---:|---:|---:|---:|---:|
| energy | 1 | 3,215,517 → 1,205,594 | 419.2 → 278.8 | 941.4 → 597.8 | 290.3 → 196.8 |
| energy | 2 | 1,988,777 → 1,052,607 | 354.5 → 245.7 | 753.1 → 573.9 | 241.1 → 175.0 |
| co2 | 1 | 6,724,159 → 2,210,685 | 439.3 → 345.3 | 1418.8 → 844.2 | 330.1 → 243.8 |
| co2 | 2 | 4,393,006 → 1,927,296 | 345.7 → 288.9 | 967.2 → 687.2 | 254.5 → 201.7 |
| backblaze | 1 | 24,538,148 → 10,288,524 | 1164.0 → 298.5 | 3331.7 → 571.9 | 847.0 → 188.1 |
| backblaze | 2 | 14,654,756 → 7,437,226 | 871.3 → 268.3 | 1943.0 → 510.8 | 579.5 → 173.2 |
| ghcn | 1 | 387,420 → 387,420 | 591.0 → 553.2 | 643.2 → 691.1 | 309.6 → 295.8 |
| ghcn | 2 | 225,251 → 225,251 | 473.2 → 470.8 | 637.0 → 632.9 | 269.7 → 270.3 |

For context, on whole Backblaze LZ4 -1 produced 24,024,207 bytes at 935.0 MB/s combined and zstd -1 produced 13,305,317 bytes at 665.8 MB/s. On CO₂ the figures were 7,334,043 at 383.5 MB/s and 4,112,930 at 329.5 MB/s. Typed L1 produced the smallest frames among these four on both files and had the lowest combined throughput. The [competitor follow-up](competitor-results.md) adds zstd settings 1 through 22 on energy and CO₂, the OpenZL CSV profile and four source-disjoint station tests using a trained OpenZL model. Two [country-disjoint validations](trained-owid/README.md) add comparisons with learned OpenZL models and preserve the exact model files and strict-mode outcomes. The whole Backblaze level sweep and native x86 follow-up remain open.

## Reproduction and limits

Use an isolated checkout of a1d39559686a9ec17c8614b9154a64d7b5b98672 for the baseline. Build with make build/kolmo at both revisions. For the experimental candidate set CFLAGS to -O3 -mcpu=native -DKOLMO_FAST_TABLE_TRIAL -DKOLMO_FAST_TYPED_TABLE_TRIAL. Omit the second definition to replay the literal pilot and both to restore ordinary output. Use separate builds or force a clean rebuild when changing definitions. Tested on the shared Apple ARM64 host with Apple clang 21.0.0, Darwin 27.2.0, LZ4 v1.11.1 and Zstandard v1.5.7. Source data paths are relative to the November review corpus. Run replay.py with the two binaries and corpus root. Round 0 is warmup and is excluded from results.csv. Run summarize.py results.csv scored.jsonl whole.jsonl to regenerate it. Repetitions use a seeded shuffled A/A2/B order, warm cache, file-to-file timings including process creation and normal output publication. The macOS wait4 peak RSS unit is bytes. CPU time is user + system. Rates and RSS are exploratory on this shared host; exact bytes and readbacks are reproducible.

The 2,016 initial timed-matrix arms (1,568 prefix/control and 448 whole-file, including warmups and rival controls) all matched their input. Every candidate Kolmo frame was decoded by the unmodified main binary. Two smoke passes added 128 arms. The final preset-guard adjustment was checked in 48 whole-file A/A2/B arms, whose archive hashes matched the corresponding measured build for all cases. An additional 48 edge cases × 2 builds recovered exactly. The L3 controls and below-threshold files were byte-identical. 357 frozen v12 frames were also decoded in the candidate test. These counts include repeated comparisons of the same data, not distinct corpora.

The tested typed implementation was efa457c5c068bbd9e9fdca30f2c233be660ed950. Main CLI SHA-256 b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f. Final guarded CLI SHA-256 bc0a01c0bb4da1d83dae820927fa97a0d75eb9e8f93c53bc36ec6fba035252e8. The second build with the preset guard was verified for frame identity. The timing receipts refer to the tested implementation. A future selector must explicitly incorporate the time and RSS cost of both the sample and end-to-end transform, especially on wide sparse tables. Do not publish this as a faster default or an independent x86 result.
