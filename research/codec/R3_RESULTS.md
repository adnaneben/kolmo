# R3 — fresh validation of bounded compatible native selection

12 November 2026. **Execution and audit completed. Registered scientific verdict: PROVISIONAL — one source family meets every size/cost criterion, not both.** The earlier queued checkpoint in commit `46e6f76` is superseded by this result; its historical local evidence is retained.

## Decision

Keep compatible plane-aligned entropy coding, singleton support and shared scratch as a promising combination. The frozen candidate reduces complete native research-frame bytes by **14.70%** across the two new families with approximately baseline encoding duration. It passes the entire registered balanced-native screen on CollegeMsg. Covertype passes size, per-file regression, encoding and full verified-decoding criteria but **fails the separately measured entropy-kernel decode criterion**. Do not hide that failure behind the slower common checksum/inverse work or relax the threshold after seeing the result.

This is not superiority over complete Kolmo, a production integration, a new format or release acceptance. No production source/default/format, release target, other branch or frozen prior experiment was changed. Writes remain on `fix/planner-record-ownership-isolated-20261112`; PR #7 stays draft and unmerged. Parallel Claude E02G/verification work is preserved and not pooled with this native-component study.

## Registration, source and execution identity

- Preregistration: `c72628a6330e224a59a5a7e1d995a2a4439c7b51`, published before new-source payload download/evaluation.
- Frozen implementation: **`06db7f2f2849fa6ccdc207227cfaf4498d17870e`**. Nine file blobs are fixed in `R3-guard/FROZEN.json`.
- Pinned production: `2f3308a3380a140ad0f6873ae228de7715e5eef5`; core `e06467c8872d494b97270811bedf6fd0bac2aa31`; graph `91ad067dc583b9d9e6f060f214a0f7c402bb4a31`.
- GitHub Actions run **43276455708**, job **131386028141**, completed successfully at 18:22 UTC. A successful workflow means the registered measurements and decision ran, not that the scientific hypothesis passed.
- Artifact **13970918568**, SHA-256 **`00ae71bdc5e4f3612d017dfeabb0f5154742feda67bde2be92f1bfeefb50ebe1`**. It contains source receipts, exposure audit, frozen files, build logs, inputs, captures, all frames, raw timings and decision.
- No R3 source, thresholds, datasets, fields, windows or acceptance rule changed after evaluation began. The first real-source run completed; no failed real-source run or inconvenient field was discarded.

## Combined policy

The candidate makes one bounded choice between original and aligned stored/Huffman/rANS competition. For one plane, fewer than 1,024 values per plane or fewer than 16,384 residual bytes, it retains the original schedule. Otherwise it samples at most 1,024 evenly spaced values per plane and estimates pooled-versus-conditional byte entropy. It aligns only if predicted saving meets both the registered proportional and model-overhead margins. This estimate is a heuristic, not a compression bound.

Aligned coding reuses one rANS temporary buffer sized to the actual plane/chunk and permits a singleton table. The decoded byte sequence and pinned leaf readers/inverse stay unchanged. The candidate emits exactly the original or fixed-aligned control's frame according to its recorded decision. One existing leaf still selects one codec: no arbitrary per-plane codec mixing or table-reuse signalling is introduced.

Comparators are original native baseline, fixed HRS alignment, costed original-or-HRS safety, the gated/shared-scratch candidate and a duplicate original baseline for timing noise. The strict safety comparator pays for both schedules; the candidate does not. Raw stored fallback is guaranteed, but non-expansion against an uncomputed compressed baseline is not.

## New data and applicability

The exposure preflight scanned and hashed **229 repository text files**, including benchmark/holdout/protocol/registry content, and found no identifier matches for the registered sources before download. Skipped binary/large paths are recorded. This is an auditable identifier and provenance check, not a proof that no undocumented alias exists.

| Family | Derived streams | Selected values | Complete NPY bytes |
|---|---:|---:|---:|
| UCI Covertype | elevation, road distance, fire distance, hillshade control, cover-class control | 1,310,720 | 5,243,520 |
| SNAP CollegeMsg | source ID, destination ID, event timestamp | 179,505 | 718,404 |
| Total | 8 | **1,490,225** | **5,961,924** |

Conversions were fixed: the first 262,144 Covertype rows and all 59,835 CollegeMsg triples, original ordering, exact uint32 values, no sorting, imputation or synthetic outliers. Publisher archives/members and every derived input were hashed before capture. Complete original DERIVED NPY files are reconstructed; publisher text layout and other Covertype fields are not within this compression boundary. CollegeMsg contains anonymized edge identifiers/times, not message text.

Both source families had at least two multi-byte residual streams and were exercised. The gate aligned seven streams and retained the original single-plane cover-class control. Columns and rows within a family are correlated; eight streams and repeated timings are not eight or thousands of independent source families. All these inputs are now observed and must be excluded from future fresh validation.

## Complete native sizes

All parameters, seeds/GCD, original header/tail, entropy tables/chunk headers, payload and checksums count.

| Family | Original native baseline | Gated combined candidate | Saving |
|---|---:|---:|---:|
| Covertype | 680,310 | **582,508** | **14.38%** |
| CollegeMsg | 275,663 | **232,938** | **15.50%** |
| Byte-pooled total | 955,973 | **815,446** | **14.70%** |

Equal-family mean saving is 14.94%. No individual input expanded in this validation; the cover control was byte-identical. The candidate, fixed HRS and strict safety comparator produced the same frames here. This does not establish a universal no-expansion property for the sample gate. Nor is 14.70% added to R2's 5.24%: they are measurements on different cohorts and a common-definition baseline, not sequential percentage gains.

## Registered timing decision

Ratios are candidate/original native baseline, first / confirmation pass. Values over one mean slower. Each is a sum of per-input medians within a source family, not a single continuous stream.

| Family | Encode | Full verified warm decode | Entropy/buffer warm decode kernel | Verdict |
|---|---:|---:|---:|---|
| CollegeMsg | 1.034 / 1.005 | 0.938 / 0.924 | 0.868 / 0.957 | All registered screens met |
| Covertype | 1.010 / 1.003 | 1.060 / 1.063 | **1.558 / 1.550** | Kernel decode screen failed |

The registered limits were encode <=1.15x, verified decode <=1.10x and kernel decode <=1.10x, with size and regression gates, in both passes and both families. Duplicate-baseline encode and verified-decode noise ratios passed the predefined 0.90..1.10 range. That is a sanity check, not statistical proof of precise speedups. The outcome remains **provisional_one_family**.

For context, the strict safety policy took roughly 1.76-1.77x encoding duration on Covertype and 1.79-1.82x on CollegeMsg. The candidate avoids that extra full-schedule trial. Its gate/scratch work does not establish a universal free-selection result.

Encode begins at frozen captured residuals, including gate/model trials, allocation/copying, frame formation/checksum and cleanup; unchanged prediction/type detection/capture and the pre-existing original-input checksum are excluded. Full decode includes frame and output checksums, metadata, inverse and allocations. The entropy/buffer kernel assumes a prevalidated frame and is not a public safe decoder API. Warm batches and 32-MiB-displaced decoding are distinct; displacement is not guaranteed cold storage or cold LLC. No whole-product or target-machine throughput is claimed.

## What the kernel failure reveals

Post-hoc inspection of the already recorded chosen codecs and timings locates the main regression:

| Covertype stream | Original chosen codec | Candidate chosen codec | Kernel ratio, first / confirmation |
|---|---|---|---:|
| elevation | Huffman | rANS | **4.27 / 4.30x** |
| hillshade | Huffman | rANS | **4.02 / 3.86x** |
| road distance | rANS | rANS | about 0.99x in first pass |
| fire distance | rANS | rANS | about 1.01x in first pass |

This is consistent with a size-only codec-choice trade-off, not proof that every aligned rANS path is four times slower. The layouts and selected coders both changed; a fixed-layout HUF-versus-rANS ablation is the next controlled diagnostic. Common CRC/inverse work dilutes the two kernel regressions to approximately 6% for full-family verified decoding, so the separate kernel measurement prevented a misleading blanket promotion.

The selected high-plane rANS tables on the two slow streams contain 18 and 2 symbols, respectively: they are NOT singleton chunks. A constant-only decode shortcut therefore cannot explain or resolve this particular bottleneck by itself. A selected CollegeMsg timestamp rANS chunk DOES contain one symbol and 59,833 raw bytes, providing real-data coverage of the compatibility-enabling singleton path. Its isolated size contribution was not measured in R3 and is not invented here.

## Shared scratch: a separate implementation benefit

Output identity is unchanged by scratch reuse. Peak directly tracked requested heap during encode, compared with fixed R2 HRS:

| Input | Fixed HRS | Candidate |
|---|---:|---:|
| Covertype elevation | 6,422,768 bytes | **3,437,423 bytes** |
| CollegeMsg source IDs | 4,804,304 bytes | **928,671 bytes** |
| CollegeMsg timestamps | 5,282,960 bytes | **1,398,827 bytes** |

These counters cover the harness and staged leaf/inverse-function dynamic allocations, not vendor-internal allocation, allocator overhead, stack, process RSS or full-codec bounds. No-op paths deliberately retain the original implementation's memory behaviour. No shared-histogram/table optimization has been claimed; only the explicitly implemented workspace reuse is credited.

## Verification and independent audit

- Workflow verified frozen source blobs, full source identity and production-file preservation, then passed eight infrastructure tests and both GCC release and Clang sanitizer selftests.
- In each build: 351 inherited R1 cases; 144 R2 deterministic round trips, 1,710 truncations, 3,456 mutations and 12 single-plane identities; seven gate checks and 18 scratch-identity controls passed.
- All eight fresh inputs and five policies passed: **40 configurations in the sanitizer pass and 40 in each release pass**. Seven/five rounds across five timing regimes produced **2,400 timed batches**, not independent datasets.
- Fresh sanitizer execution exercised **48 directly tracked allocation-failure positions** with valid cleanup or exact fallback and no tracked leak.
- Downloaded archive digest matched GitHub; all **282 artifact manifest entries** verified. Independent audit checked every one of the **1,490,225 retained source values**, archive/member/input hashes, **120 frame hashes**, **800 reported medians**, all candidate-to-control decisions and all repeat identities. An independently written decision calculation reproduced the registered one-family verdict.
- A separate local Clang sanitizer replay of all **40 configurations** reproduced every cloud frame hash, stored size, captured residual and gate/codec choice. This is a second execution environment on now-observed data, not another independent source family or new validation sample.
- Vendor objects remain uninstrumented. The independent Python reconstruction does not independently implement the entropy decoders. These finite checks do not establish comprehensive security, historical full-container compatibility or M5 acceptance.

## Status and next action

R3 has answered the registered question without changing its rules: the combined policy has replicated size value and low selection cost, but only one family passes the full balanced-native profile. **Retain it provisionally; do not promote a universal default.** The previous local checkpoint and interrupted-setup history remain in the repository/evidence.

Next substantive task: a small **decode-cost-aware codec-choice ablation** on these now-observed inputs, holding predictor, chunking and bitstreams fixed. Compare aligned Huffman-only, aligned singleton-rANS, existing minimum-size native choice and a clearly bounded decode-oriented choice; both entropy candidates are already tried, so first assess whether selection can use those same outputs without another representation trial. Quantify the size premium of avoiding the two slow coder switches. Treat any new rule as development, then freeze it before a new audited validation set. Do not retune R3 thresholds or claim Covertype passed retrospectively.

New-source comparisons against complete Kolmo, an integrated production encoder/decoder, new format/defaults, merges, releases and M5 acceptance remain uncompleted and unauthorized by this report. Documentation-only CI skipping does not retroactively validate later commits; executed evidence is tied to `06db7f2`.
