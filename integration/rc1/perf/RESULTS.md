# RC1 integrated-build performance pilot — executed

13 November 2026 UTC. Codec source ae3957ad80d134506ce37b59857af25fe8826062; CI-only checkpoint b627c1171d821deca1484f023c7eeda6a1e5bde1. Published comparison is v1.0.1 at a1d26f2075120f2a48e72e7026754d1a17bb2e8f. Protocol was frozen at 46efb019a5e7b00fe3686828821fd2d875b73cb4 before measurement. No release published, defaults promoted or main merged.

## Completed integration and gates
The combined source now retains the ownership fix and compatible memory/tar facilities; includes the checked fast inverse, opt-in native contextual leaf (version 9 only when emitted), optional ultra level-2 CM graph leaves, opt-in typed Huffman and CSV archive routes, CLI cleanup and synchronized Python/Rust native copies. See ../FEATURES.json. KST1 deep stacking remains research-only; the general type-first cross-column planner is not completed by the regularity-based CSV router.

CI 43472625541 and wheels 43472625565 passed on this source: Linux x86, ARM and macOS checks; full main sanitizer, combined inverse/option checks, native-leaf fuzz and binding/streaming gates. The old ownership-only workflow was skipped, not called passed. Locally rebuilt tests passed 2880 inverse cases/76012 malformed rejections and 119 option round trips/5108 header checks/36 forward differentials. All 53 archived pre-integration default frames matched.

## Pilot coverage and audit
31 fixed configurations across 35 inputs (33,381,545 original bytes): eight previously observed NPY arrays, two regular CSV views, four complete known CSV regression files, four complete small audio samples, four Loghub 2k-line logs, twelve one-MiB Silesia prefixes and one overlapping 64-KiB control. These are development sources, not a fresh holdout. Silesia prefix figures are NOT complete-corpus results.

842 applicable configurations completed exact generation/decode, 5052 timed operation batches and 1684 separate RSS batches. All 842 frames, original hashes, complete matrices, timing arithmetic and RSS integrals were independently audited. Five new audit tests passed. All 1980 matched workbook comparison formulas were checked; the workbook has 5760 formulas and zero formula-error cells.

88 ultra-family cases >=512 KiB were NOT_RUN_LIMIT under the registered 4-GiB host budget. One OpenZL CSV-9 generation on CNAE9 failed with an allocation error under the fixed 3-GiB virtual address cap. It is NOT counted as a Kolmo win. Matched CSV regression rows below use the same three successful inputs for both codecs.

## Matched results
Ratio is original/stored bytes. Speeds are MiB/s computed from total bytes and sums of per-file median seconds. Positive savings favor Kolmo; strict wins require smaller output and cost <= rival in every one of the three pairs. A favorable aggregate does not erase per-file losses.

| Workload / RC option | Rival | Coverage | Ratio ours / rival | Encode ours / rival | Decode ours / rival | Bytes saved | Modeled cost saved | Strict wins |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Numeric / typed-huf | zstd-3 | 8/8 | 6.317 / 4.401 | 469.95 / 317.85 | 1462.83 / 1333.97 | 30.32% | 29.35% | 7/8 |
| Numeric / native leaf | zstd-3 | 8/8 | 7.876 / 4.401 | 24.85 / 317.85 | 478.77 / 1333.97 | 44.12% | -635.35% | 0/8 |
| Numeric / hc | OpenZL whole-numeric-9 | 8/8 | 8.334 / 8.054 | 7.46 / 51.73 | 1583.89 / 814.29 | 3.36% | -47.38% | 0/8 |
| Numeric / max | OpenZL whole-numeric-9 | 8/8 | 8.823 / 8.054 | 4.07 / 51.73 | 220.49 / 814.29 | 8.72% | -98.57% | 0/8 |
| Regular CSV / eco | OpenZL CSV-9 | 2/2 | 8.957 / 5.666 | 31.67 / 18.87 | 159.11 / 156.36 | 36.74% | 34.97% | 2/2 |
| Regular CSV / csv-archive | OpenZL CSV-9 | 2/2 | 8.609 / 5.666 | 75.28 / 18.87 | 246.99 / 156.36 | 34.19% | 39.97% | 2/2 |
| CSV regressions / eco | OpenZL CSV-9 | 3/4 | 4.730 / 4.018 | 20.54 / 22.03 | 259.49 / 219.82 | 15.06% | 14.39% | 3/3 |
| CSV regressions / csv-archive | OpenZL CSV-9 | 3/4 | 4.261 / 4.018 | 46.28 / 22.03 | 318.82 / 219.82 | 5.69% | 12.75% | 2/3 |
| Logs / eco | OpenZL binary-9 | 4/4 | 11.994 / 7.807 | 31.40 / 84.54 | 670.12 / 1291.75 | 34.91% | 22.47% | 4/4 |
| Mixed prefixes / fast | zstd-3 | 12/12 | 3.053 / 2.495 | 89.17 / 219.32 | 896.02 / 1222.73 | 18.26% | -72.52% | 0/12 |
| Mixed prefixes / eco | OpenZL binary-9 | 12/12 | 3.297 / 2.599 | 12.92 / 53.31 | 324.29 / 432.73 | 21.16% | 10.15% | 5/12 |
| Audio / typed-huf | zstd-3 | 4/4 | 1.660 / 1.257 | 172.96 / 462.30 | 594.64 / 877.96 | 24.28% | -27.00% | 0/4 |

Native leaf saves 4.06% compared with RC fast on Numeric8 (789043 -> 756988 bytes), but encoding drops from 34.70 to 24.85 MiB/s and decoding from 1505.80 to 478.77 MiB/s. Keep it opt-in. Ultra level 2 reaches 1.808x on the four small audio samples versus default ultra 1.752x, but its decoding is about 0.2 MiB/s and measured peak RSS exceeds 1.1 GiB. These are small-input observations, not worst-case bounds.

On regular CSV, plain eco remains smaller than the route option (732519 versus 762125 bytes), while the route is faster. On all four complete regression CSVs, the route is 12.45% larger than eco (1218685 versus 1083790 bytes). It must not universally override eco.

## RSS examples (MiB; average / maximum)
| Workload / option | Encode RSS | Decode RSS |
|---|---:|---:|
| Numeric / typed-huf | 5.50 / 6.80 | 5.69 / 7.12 |
| Numeric / zstd-3 | 5.57 / 6.45 | 4.54 / 5.26 |
| Regular CSV / eco | 39.29 / 71.30 | 14.33 / 24.99 |
| Regular CSV / csv-archive | 20.78 / 39.52 | 13.14 / 23.43 |
| Regular CSV / OpenZL CSV-9 | 46.35 / 88.57 | 23.31 / 54.39 |

RSS is time-weighted process resident memory from separate >=50-ms batches, sampled about every millisecond. Cohort average is mean of per-file averages; peak is maximum reset post-exec VmHWM or sample on the matching files. Original/gold/output buffers, touched libraries and allocator retention count. Not isolated codec workspace and not worst-case memory. Average/peak encode and decode appear independently in the workbook.

## Relative to published v1.0.1
RC fast saves 9.37% on Numeric8 and decodes 32.97% faster, but encodes 5.66% slower. On twelve one-MiB mixed prefixes it saves 0.08%, encodes 40.29% faster and decodes 9.23% faster. On four audio samples it saves 6.36%, but encodes 47.6% slower and decodes 15.5% slower. This includes intervening base changes, not only the latest feature port; attribution requires a pre-integration reference and feature ablations. Do not describe the RC as uniformly better.

## Boundary and limitations
Native complete-input API, one pinned CPU on shared AMD EPYC 9V74, three independent processes per timing direction/configuration, >=25-ms measured work each. Typing, planning, rejected trials, contexts, checksums, copies, allocation/free and reconstruction count; file I/O/startup/original loading and external byte comparisons are outside timers. No old M1/S2 samples pooled. Both core builds use -O3 -DNDEBUG -fno-plt and -ffp-contract=off. Fourteen RELEASE helper symbols renamed solely for linking, algorithms/API unchanged.

The speed model is charged encode + decode + bytes at 1 Gbit/s. Archive uses the recorded 30-day/3.3-read CPU/storage model, not current price quotes. Direct zstd has checksum-off defaults, Kolmo/OpenZL integrity is on; raw LZ4 needs external original length. OpenZL v0.3.0 is pinned, untrained, with applicable profile/graph settings, not a trained leader frontier.

Identical zstd controls vary 0.843–1.208x encode, 0.848–1.161x decode and 0.886–1.129x modeled speed cost per file. Green/red means observed differences, not statistical significance or an SLA. No trained-OpenZL, full-scale corpus, M5, multithread, streaming/file-mapping, genomic or GPU leadership claim. Existing combined-code CI supplies sanitizer coverage; the new measurement driver has exact-valid-input and bookkeeping checks, not a new comprehensive sanitizer certification.

## Preservation and next task
Readable driver, runner, preparation, portable build helper, audit and tests are in source/. Their Git tree fc476502df9c526dfdd719ac4be0db5819b7887a matches local files exactly. The portable build helper was source-stage checked after measurement; exact executed build scripts are in the delivered source package. PACKAGE.json identifies workbook, source and raw evidence attachments; large raw data are not claimed as Git-tracked.

Next: matched pre-integration-versus-RC ablations on audio and CSV regressions, then freeze mode-specific routing before fresh-source and target-hardware validation. Keep costly choices opt-in. PR #10 remains draft/unmerged; no release published.
