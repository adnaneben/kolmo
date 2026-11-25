# Archive ratio and combined throughput

Measured on 25 November 2026, Apple M5 macOS arm64, warm filesystem cache. The three native binaries were built on this host with Apple clang, -O3 -mcpu=native. The external products were bsdtar 3.5.3 with libarchive 3.7.4, LZ4 1.11.1 or zstd 1.5.7. Source and binaries used for the fresh comparisons:

| Arm | Revision | SHA-256 of executable |
| --- | --- | --- |
| main | 4db0425b361012523d8074ca6f2a760d9466e9f4 | cf23250a9d1fd43f6fb0328f26803ebb4a47ba6332ce98a9053b70728c8abfdf |
| PR73 | dcbf483184e77e465c02e33bd1aae674ca26b5bc | b5c968708777f3bce882ceeddea7107dde46bbde86d9959cefe31acaa280f9f3 |
| PR90 | f1ea02dff9616e91ca8d0b9ed7fc50333bc925d1 | dd0cb9d63b9932c12249f2fd95115c2cdd1c9b174c78e91aef2bbb0014157de5 |

## Inputs and commands

Silesia 12 files contains every member of the 211,947,520-byte silesia.tar. Silesia gzip member encloses the complete tar compressed with Apple gzip -n -6, a single member above the direct-store threshold. The codec's own counters verify every chunk was unique for each run. The 768 small members are deterministic 8 KiB files derived from intervals in the first 4 MiB of the same tar, with seeded content suffixes. This synthetic layout tests extraction overhead. These are three layouts of one source family. Input manifest records SHA-256, length, mode and mtime. Every source file has mode 0644 and a fixed mtime. All products receive the same sorted list of explicit ordinary file paths.

Kolmo uses archive -1 -T N --stats and extract -T N -C. The tar products pipe bsdtar -cf - into lz4 -1 -B7 or zstd -1; restored output pipes each tool's standard decoding into bsdtar -xf -. T1 explicitly requests --single-thread on the external encoder. T4 requests -T4. With pipelines, the tar reader/writer can run concurrently with the codec, so these are product-level configurations, not strictly identical core budgets. The standalone external decoders use their normal decoder modes. The process wall includes input reads, creation, output writes, process startup, and full extraction. Filesystem cache is warm and no explicit fsync is requested. CPU sums both processes in each pipeline. Peak RSS is the largest per-process high-water mark in each phase on macOS; for tar pipelines this does not measure simultaneous combined RSS.

For each repetition, the workload/thread order and five arms within each workload/thread are deterministically shuffled. There is one unscored warmup per cell and seven scored repetitions. A phase timer starts immediately before spawning the first process and stops after all workers exit. The output descriptor for external pipelines is prepared immediately before the timer and tar formats carry different framing/metadata. Every restore is checked against the entire input manifest. All Kolmo arms must produce the same compressed SHA-256 for each cell or execution aborts. The raw CSV retains warmups and timings. SHA verification and cleanup are outside the timers.

Ratio is uncompressed bytes divided by archive bytes. End-to-end speed is input bytes divided by elapsed create plus full restore time. MB/s uses decimal megabytes. Each wall figure is the median of observations for that metric. Median encode and decode times do not necessarily sum to the median total. Individual phase speeds use their phase median. The paired table computes baseline-time/candidate-time within each repetition, giving speedup for each phase or total. Its bootstrap intervals for the median use 10,000 draws, descriptive given the small sample size and multiple comparisons. One shared host with no control over unrelated host processes or OS page-cache eviction. No p95 inference is made from seven pairs.

Run replay.py with --silesia-tar, --main, --wave2 and --candidate pointing to the pinned input and local builds. The replay regenerates synthetic members and the gzip control. Run score.py after the replay to generate paired.csv. Use the distinct named-member measurements from the original #87 review when assessing partial recovery.

## Atomic extraction regression retest

The stage-raw.csv file contains a separate 192 complete restores (176 scored) from the isolated #59 atomic staging patch and its byte-identical main control executable. Every restore was checked for member names, SHA-256, mode and mtime. The eight cells cross 64 and 256 files, 512 B and 32 KiB generated payloads, and T1/T4. The archive was created once per dataset with the baseline. The order of contexts and the paired binaries is randomly shuffled before each round. Eleven scored pairs per cell produced slower extraction in 87/88 comparisons with the private stage per file. Median paired slowdown per cell spans 2.13–2.88x. Main and #73/#90 provide different failure/publish semantics. The private-directory-per-file implementation requires a performance repair; atomic replacement remains a requirement. Files stage-summary.csv and retest_stage.py preserve the outcomes and replay.

## Direct-main backport chart

The clean-* table is an independent randomized run for the three chart layouts plus a second source family, complete enwik8 recompressed as one gzip member. It directly compares a clean main + #87/#89 backport (arm direct-main), its exact main base and the same two external pipeline competitors. Seven scored rounds plus one warmup; new order seed 202611251. Supply --enwik8 and --clean-candidate, --arms main direct-main tar+LZ4 tar+zstd, --workloads 'Silesia 12 files' 'Silesia gzip member' '768 small members' 'enwik8 gzip member', --seed 202611251 --label clean-. Run score.py clean- direct-main for paired summaries. Peak RSS and all verification rules above apply. The full extraction code in the clean arm retains the original main publication behavior. Its two scoped optimizations apply only to one large unique member at level 1, or to listing/partial extraction operations.

The direct-main executable SHA-256 is b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f. Relative to main at 4db0425b, the only modified src/ files have these SHA-256 digests.

| Source | SHA-256 |
| --- | --- |
| src/cli/features.c | 48f14a63a89efc4f865db31c333ff6202ec9ee2977eff55189f13cde53045eb4 |
| src/core/features.h | fe4b587270798f31ca40585ae7661e1ea7bf6943500dd5cfc0e98a57cfdd6a83 |
| src/core/dedup.c | 2d9ba48fb52178d9fddb3fddb30e958ef24326844423b6c2eab12b27bb42b589 |

The input Silesia tar used in both fresh matrices has SHA-256 a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2.

The gzip input for enwik8 is generated from the full 100,000,000-byte original (SHA-256 2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8). The four eligible input/thread combinations in the primary clean run show 3.0–5.4% median paired combined throughput improvement and 35–65 MiB lower encoder RSS. The seven-pair interval on enwik8/T4 includes unity, and the separate-arm median total on Silesia gzip/T4 slightly favors main, despite six of seven matched runs being faster on the candidate. Raw run order and phase metrics are available for independent evaluation. The fresh retest keeps this original result.

## Size and time in a delivery lane

transfer-model.csv evaluates a single serialized transfer on top of measured median local creation and full restore. If A and B produce different sizes, their crossing solves T_A+B_A/v = T_B+B_B/v for a positive bandwidth v. Full Silesia T4 has a modeled 97.24 MB/s crossover between clean Kolmo and tar+LZ4. The small-member T4 tree has a modeled 11.58 MB/s crossover with tar+zstd. CPU/I/O overlap, storage tiers, network protocol overhead, repeated decodes and seeking change this model. These are conditional arithmetic projections, not measured network results.

## Confirmatory paired retests

The confirm-* files retain a separate 15-triplet confirmation with the clean candidate and two independently ordered executions of the same main binary. This run covers Silesia gzip at T4, enwik8 gzip at T4 and 768 small files at T1. Replay uses --arms main main-AA direct-main --contexts 'Silesia gzip member@4' 'enwik8 gzip member@4' '768 small members@1' --reps 15 --seed 761251 --label confirm-. score.py confirm- direct-main computes the paired candidate receipt; score.py confirm- main-AA computes the same-binary control. Rename the generated confirm-paired.csv between scoring calls.

| Case | Candidate combined vs main | Candidate combined vs replicated main | Two identical main executions |
| --- | --- | --- | --- |
| Silesia gzip, T4 | 1.0377x, 14/15 faster | 1.0362x, 13/15 faster | 1.0045x |
| enwik8 gzip, T4 | 1.0406x, 14/15 faster | 1.0456x, 15/15 faster | 1.0026x |
| 768 small files, T1 | 1.0035x, 8/15 faster | 0.9902x, 5/15 faster | 1.0160x |

The original suspect eligible-cell results are recorded alongside the confirmations. Both same-binary controls give the scoped large-member optimization a consistent combined gain, with each of the four 95% descriptive paired-median intervals above 1. The unrelated small-file control changes sign between replicas; its original -1.2% paired point is inside the observed same-binary variation. There is no qualified combined gain on the small-file layout.

## Repaired private staging trial

The patched extractor (SHA-256 8a82dcfca546e45bf51bbc6e519865dc979b88e77d5f6b4335e9c82e19e321e1) reused a private stage for sibling files. Run retest_stage.py --baseline MAIN --original-atomic PR59 REPLACEMENT to replay. The repaired-stage-raw and repaired-stage-summary tables repeat the varied member count, size and T1/T4 tests with the main baseline, original #59 and candidate. All 288 full restores verified. Across 8 contexts, the repair removed 31.6–41.9% of paired elapsed extraction time vs #59 and won 87/88 scored pairs. It remained 32.0–68.1% slower than direct-write main, which does not provide the same atomic publication behavior. These figures apply to flat trees of sibling files.

Across all five timing experiments 1,120 complete output trees were verified: 240 stacked comparison, 256 clean backport comparison, 144 clean A/A confirmation, 192 isolated original staging, and 288 repaired staging. Of those, 1,009 were scored. All datasets are from one M5 machine.

## Clean backport and external peers at four requested threads

| Layout | Kolmo combined MB/s | Tar+LZ4 combined MB/s | Tar+zstd combined MB/s | Size comparison |
| --- | ---: | ---: | ---: | --- |
| Full Silesia | 674.44 | 970.78 | 978.39 | Kolmo 91,041,469 B; LZ4 100,369,162 B; zstd 73,259,152 B |
| 768 small files | 79.25 | 59.79 | 62.18 | Kolmo 3,684,604 B; LZ4 4,413,766 B; zstd 3,432,061 B |
| Silesia gzip member | 750.16 | 904.44 | 1,140.61 | Kolmo 68,289,672 B; LZ4 68,217,946 B; zstd 68,155,971 B |
| enwik8 gzip member | 728.82 | 804.94 | 1,133.95 | Kolmo 36,595,528 B; LZ4 36,556,851 B; zstd 36,551,328 B |

These throughputs are input bytes divided by each arm's own median total. Use the paired tables for changes between versions; wall clock varies within and between rounds. See clean-summary.csv for all separate create and restore figures.
