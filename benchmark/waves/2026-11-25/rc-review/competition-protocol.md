# Kolmo competitive performance and release decisions

This is the continuing project brief for Codex and Claude Code. Its unit of evidence is a named workload on pinned source, hardware and data. Track the strongest measured lossless competitor in each lane and close the gap in both compression ratio and combined speed. Retain repeatable niche improvements, rejections and initial observations permanently.

## Score each workload

Let N be input bytes, C be complete compressed bytes including frames, indexes, side data, and chargeable dictionaries, E be encode seconds and D be decode seconds. Report R = N/C and V = N/(E+D), with decimal MB/s for throughput. Using separate phase rates e and d in identical units, V = ed/(e+d). For an optional single ranking use R × V = N²/[C(E+D)]. An implementation reducing C by fraction s can spend at most s/(1−s) additional combined time to break even on that ranking. The product can hide a ratio loss or an unacceptable decode delay. Use matched size, matched time and the two-dimensional Pareto frontier to adjudicate leadership. Keep explicit per-workload limits on each phase, actual RSS, first useful output and p95 latency. A shared-runner median does not establish p95.

Keep file-to-file, in-memory, directory, cold-start, named-member, full restoration, and indexed/range access lanes independent. For storage products, also evaluate one encode followed by k decodes and transfer time on each supported deployment. This supplementary deployment cost must keep its own labels and assumptions. Multiple rounds are paired on identical inputs. Report the median of observed E+D, not the sum of independently computed medians.

## Qualify, scope and retain

1. Specify candidate, comparator, intended file family and producer, size range, thread counts, CPU ISA, block layout, memory ceiling and objectives before testing. Record compiler, effective threads, corpus hashes, model or dictionary costs, and cache/IO conditions.
2. Keep raw results, source revisions, exact verification, negative pilots, and pre-declared sealed holdouts separated by producer. Verify both resource accounting and recovery with a shipping decoder wherever ordinary v12 compatibility is promised.
3. Randomize paired runs. Investigate each regression in its own setting, alternative sizes, thread levels and an independent producer. Use same-binary A/A controls for small/noisy differences. Preserve all observations, including those disproved. Reject a repeatable regression inside the stated scope. A narrower gate needs independent validation, plus an inexpensive and robust way for the encoder to identify it.
4. Separate correctness and security requirements from speed admission. Measure the cost of necessary protections, repair inefficient implementations and retain the security property. Give each independent source change a branch and a reproducible review record. Update the RC only with qualified changes. Publishing a tag, public advisory or registry artifact is a separate decision.
5. Keep learning opt-in and encoder-side. Begin with an immutable versioned format catalogue, reversible policy snapshots, deterministic fallback and an ordinary v12 decoder. Do not substitute benchmark file identities for deployable features. An offline chooser which sees all results is an upper bound. Test unseen producers, adversarial distribution shifts in normal valid data, catalogue/dictionary startup cost and the cost of a failed probe.

## Required comparisons and research

Record LZAV, LZ4 and fast zstd on short blocks; LZ4-HC, libdeflate, Brotli, zstd, xz, 7-Zip, PPMd and BWT-family codecs across the middle; OpenZL and ZipNN on comparable structured inputs; PAQ8PX, cmix, zpaq and applicable neural compressors in high-ratio lanes. Attach version, flags, hardware and full input for each. A published byte count is a size anchor. Independent native timing is required for a speed comparison. Count archive service features and dictionary shipping costs explicitly.

Evaluate algorithmic complexity alongside actual bytes and total CPU/wall. Compare bounded parsing, cache-aware data structures, floating-point and integer transforms, reset/history schedules and per-class coding loss. For assembly and intrinsics, require scalar/ISA output identity, actual ISA execution and an end-to-end gain at target file sizes. Use separate project lanes for encryption, release integrity and operational memory containment.

[Existing numerical workload targets](https://github.com/adnaneben/kolmo/blob/8485feecc5ab0890e8d68fa2e47bb461d6fa93f4/benchmark/waves/2026-11-25/acceptance.md) · [Live tracker](https://github.com/adnaneben/kolmo/issues/57)
