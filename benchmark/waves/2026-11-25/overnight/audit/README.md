# Review reconciliation and missing-output recovery

**25 November 2026.** Source used for fresh controls: main `a1d39559686a9ec17c8614b9154a64d7b5b98672`, native Apple arm64 build. The [original evidence and review closure](https://github.com/adnaneben/kolmo/tree/847f518313cd59565315b9374d0cf9e9c3de3c1e/benchmark/waves/2026-11-25) were matched to the published Git blob IDs. Both the ZIP CRC and all 17 internal SHA-256 receipts in the closure archive passed. The ZIP SHA-256 is `8b41c66f268d66cf149145c86f4ad57ec172fb43abc4eb9e5c1e4d3dc4c18bb6`.

## Every assignment and claim

- **123 assignment rows.** `assignment-audit.csv` ties 112 A/B records to original line and content fingerprints, plus 11 C records to their named receipt pack or memo. All 40 first-wave source-reference sets resolved at the original pinned commit. PR IDs, states and exact heads were independently read for all linked rows. The 123 assignments cover 68 group labels, with **33 labels shared across multiple rows**. A linked draft is evidence of source work only within its scope. Its presence cannot establish a performance improvement or that one identified worker produced it.
- **31 A/B rows originally cite no implementation PR.** `reopened-gates.csv` traces every one to a source file on current main and names the smallest qualification experiment still needed. B59 has a later negative experiment with 504 exact trials, which closes the proposed broad small-first default. The other 30 are open for qualification. Some have partial related implementation elsewhere. None should be counted as delivered fixes on the basis of the assignment.
- **178 historical Claude claim IDs.** Each slug is present in its corresponding independently hash-checked report or reassessment; `legacy-claim-audit.csv` includes lines, report hashes, the prior disposition, and PR ancestry. The evidence comprises **30 full reports and ten reassessments**. All 16 rows associated with previously merged PRs have an actual merge ancestor of current main. Another 19 have partial support and 143 have historical evidence only. Presence in a report does not verify a fix on current main. The later campaign's 178 claim records duplicate the earlier ones after removing the campaign annotation; its review directory contains zero new reports.
- **Live PR identities.** `pr-snapshot.json` contains the directly fetched metadata. #52/#92 resolve to the same merged commit. #87 and #89 are review branches; #47 records their separately ported qualified scopes. Historical branch prefixes distinguish Claude-authored from Codex-authored work. The reviewed ledgers and PR snapshot do not name a current individual owner for each future task. This audit preserved Claude-authored branches and confined new work to research and validation.
- **Receipt integrity.** `receipt-index.csv` fingerprints 119 pack entries and indexes 8,221 JSONL records, inclusive of repeated attempts, controls and failures. There are 90 rows with an explicit false `ok` in two early rival files (36 and 54); the archived comparison report explains the 7-Zip suffix, early-close filter, and misplaced option problems. Corrected attempts remain separate. Verify raw success rows per workload before crediting speed.
- **Two-wave performance evidence.** `wave-receipt-checks.json` confirms six observations per setting (two arms × three repeats), exact input hash agreement with the manifest and identical stored size and frame hashes within each of the 90 settings in each wave. The replay script matched the digest in both manifests; it checked the decoded hash before recording each trial. This is a receipt audit, not a fresh rerun of all 1,080 historical trials. Each wave covered 12 files with only 13 distinct file/thread combinations, despite representing levels 1–9 and Ultra. Do not generalize the measurements to a full file×size×level×option matrix.
- **Execution attribution.** R11 and R13 explicitly left no original finished report or product patch. The ten historical reassessments include reports for both. Assignment IDs and final documents alone do not reveal elapsed effort, invoices, independent authorship, stopped-session IDs, or whether every assigned process emitted output. The checks above validate what survives, without inferring unobserved work.

## Fresh R11 evidence: independent records versus complete archives

Built pinned main and used its actual level-3, T1 DDP1 archive writer and extractor. For each of five historic real-data groups, 24 names taken at evenly spaced positions in the second half of the sorted group formed one complete named archive; the control encoded the same names individually as 24 complete archives. Every named file and its contents were recovered in both arms. SHA-256 and exact bytes are in `replacements.json`. Grouping changes retrieval granularity and filesystem/process overhead. This is directly deployable batching evidence. No learned prior, new codec, independent retrieval promise, or timing claim is implied.

| Family | 24 individual archives (bytes) | One archive (bytes) |
|---|---:|---:|
| SSH log | 17,383 | 2,822 |
| Hadoop log | 20,916 | 2,337 |
| OpenStack log | 28,588 | 9,484 |
| Samba C | 119,157 | 96,191 |
| Samba HTML | 178,203 | 120,306 |

## Fresh R13 evidence: reversible gzip recipe and transfer failures

`gzip_recipe.py` implements an **external research format** for a narrow single-member gzip input. Before accepting a file it reconstructs the complete original member from its header, inflated bytes, nominated deflate level and current zlib runtime, and checks equality. It records the original digest, length, verbatim header and runtime version along with a stock Kolmo frame of the inflated data. If any check or the full stored-size comparison fails, it keeps the ordinary Kolmo encoding of the input. Accepted recipes need this wrapper and the same zlib implementation; stock v12 alone cannot reconstruct them. Different libz builds with the same version string can differ, so successful output reconstruction is checked by length and SHA-256. Use only trusted research inputs at this stage. This is separate from a general bitstream-reconstruction transform and the release.

All 102 attempts restored the exact original bytes. The candidate includes the baseline encode cost, full recompression eligibility check, optional second encode, actual wrapper bytes and decode-time recompression. Shared-host Apple ARM64 timing, three interleaved repeats, warm cached file data, process calls plus in-process wrapper work, no p95 claim. Sizes contain all headers and the inner frame. `gzip-summary.csv` includes phase and combined throughput, and `gzip-trials.csv` preserves the complete trial rows.

| Known development gzip | Main L5 frame bytes | Recipe bytes | Size reduction | Main combined MB/s | Recipe combined MB/s |
|---|---:|---:|---:|---:|---:|
| dickens, gzip 6 | 3,871,698 | 2,896,113 | 25.20% | 96.53 | 1.36 |
| XML, gzip 9 | 658,969 | 496,781 | 24.61% | 47.06 | 1.05 |
| osdb, gzip 1 | 4,076,455 | 3,296,874 | 19.12% | 90.56 | 2.15 |

The first four sorted system and first four Homebrew manpage inputs gave six distinct hashes. Only one unique file benefited (29,213→28,044 B, 4.0%). Duplicate manpages were preserved in the receipt and counted once when stating the transfer result. The other five distinct inputs retained the original frame size. Concatenated input, trailing bytes, wrong compression hint, tiny files and incompressible input exercised fallback. The test corpus and hash lists are independent of the encoder, but all measurements share the same machine/zlib. The three development members were selected because earlier measurements exposed an opportunity. No claim of representative gzip coverage or throughput leadership is supported. **Reject as a default.** Keep only as an experiment for archival cases where a narrow, reversible external format and much slower retrieval are acceptable. Further work needs publisher-disjoint larger inputs, different native libz builds and explicit archival cost targets.

## Reproduction

Run in an isolated copy with Python 3.9+ and a native Kolmo build. Inputs from the private historic reviewer work directory were used locally. This evidence handoff contains hashes and decisions rather than copies of those inputs or private reports.

```sh
python3 reconcile.py "$repo" "$original_zip" "$reviewer_closure" "$historical_review_parent"
python3 recheck-gaps.py "$exact_main_source"
python3 wave_verify.py "$wave_receipts_dir"
python3 replace-missing.py "$exact_main_cli" "$historical_review_parent/review40b/work"
python3 gzip_screen.py "$exact_main_cli" "$historical_review_parent/review40b/work/assess-R13/data" gzip-trials.csv
```

`reconcile.py` expects the supplied `pr-snapshot.json`. The wave receipt directory contains wave1/wave2 raw JSONL, manifests and their matching replay script. The manpages in the transfer screen are sampled from the local installation; compare the recorded input hashes before treating a rerun as the same corpus.
