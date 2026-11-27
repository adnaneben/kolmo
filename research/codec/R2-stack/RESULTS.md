# R2-stack — measured interactions and contextual amplification

12 November 2026. **Development experiment complete; fresh validation NOT completed.**

Protocol: `b2f3c160f36462672ace9c58929aa147756f514b`. Executed implementation, verified by content hash: `be41cc58185712ae660dbf3b7e7a93959625b8e9`. All writes remain on `fix/planner-record-ownership-isolated-20261112`; PR #7 remains draft/unmerged. Production source, formats/defaults, other branches and all frozen prior experiments are untouched.

## Decision

Retain the **combination** of compatible plane-aligned HUF/rANS chunks and singleton-capable rANS as the next native-model candidate. It reconstructs complete research inputs with unchanged pinned leaf readers and the unchanged KG_INT inverse. Do not replace this with another expensive exhaustive selector: the costed baseline-preserving union produces the same output here but pays 1.63–1.67x residual-encoding duration on the full-stream group. No speed, release, M5 or whole-Kolmo superiority claim follows.

Provisional changes must be tracked both individually and in combinations. A component with no standalone size benefit may enable a useful second component. Conversely, two successful changes may consume the same redundancy and provide little incremental value together. This experiment demonstrates both, rather than assuming gains add.

## What actually ran

Three binary factors, eight cells, plus one costed safety control:

| Factor | On | Off |
|---|---|---|
| H, bit 1 | HUF chunks aligned to existing residual planes | Original HUF chunk boundaries |
| R, bit 2 | rANS chunks aligned to those planes | Original rANS chunk boundaries |
| S, bit 4 | Research encoder permits a one-symbol frequency table | Original encoder's singleton rejection |

Every cell actually tries stored/HUF/rANS and keeps the smallest complete frame. It is not an offline oracle. Policy 8 additionally tries the original baseline and the H+R+S alternative and retains the smaller. An aligned leaf remains ONE codec; this is not arbitrary per-plane codec mixing or a new KG node.

The data are the unchanged 58 archived E03 inputs/captures: seven C real-derived files, eleven D full streams, twenty-two nested D prefixes and eighteen synthetic inputs. Each pass has **522 configurations**. Two release passes use seven and five rounds, separately for residual-to-frame encoding, warm verified-input decoding and 32 MiB-displaced verified-input decoding: **18,792 timed batches**, not independent datasets. A third Clang sanitizer pass checks all 522 configurations without treating sanitizer execution as performance data.

All 522 frame sizes/hashes, inputs, residuals, choices and failure/fallback records match across all three passes. All 58 original native frames are reproduced byte-for-byte. Independent Python integer reconstruction matches each original input hash; the entropy decoders are not independently reimplemented.

## 1. Gains overlap on the full real streams

Complete E2C1 frame totals, including metadata, chunk models, header/tail and checksums:

| Combination | Complete bytes | Saving vs native baseline |
|---|---:|---:|
| Original native baseline | 337,195 | 0% |
| H only | 322,880 | 4.245% |
| R only | 319,712 | 5.185% |
| S only | 337,195 | 0% |
| H + R | 319,539 | 5.236% |
| H + S | 322,880 | 4.245% |
| R + S | 319,712 | 5.185% |
| H + R + S | 319,539 | 5.236% |
| Costed original-or-HRS safety control | 319,539 | 5.236% |

H independently saves 14,315 bytes and R saves 17,483 bytes; their combined saving is 17,656, NOT 31,798. The overlap is **14,142 bytes**. With R+S already present, H adds only **173 bytes**; with H+S present, R adds **3,341 bytes**. S adds zero on this real group because other coding choices already handle the affected constant cases.

Do not discard H solely because its conditional contribution is small: the native policy already evaluates HUF, so the relevant question is its marginal chunk/model cost, not another entire candidate search. Equally, do not advertise its standalone 4.245% as an additional saving on top of R.

## 2. A zero-gain helper enables another change

On the synthetic mechanism controls:

| Combination | Bytes |
|---|---:|
| Baseline | 628,234 |
| R only | 641,236 (2.07% larger) |
| S only | 628,234 (unchanged) |
| R + S | 595,667 (5.18% smaller) |
| H only | 597,535 |
| H + R + S | 591,902 (5.78% smaller) |

R can expose constant planes that the original rANS encoder rejects. S removes that encoder restriction, making R useful. The additive-byte interaction for R/S is **45,569 bytes**: `C_R + C_S - C_RS - C_0`. With H already enabled, S still contributes **5,633 bytes** when R is present.

This is a demonstrated mechanism on synthetic controls, NOT evidence of a 5.78% gain on new real data. S remains a compatibility enabler requiring mixed/constant-boundary coverage; it is not promoted as a standalone real-workload improvement.

## 3. A diluted aggregate can conceal a useful applicable population

Only four of the eleven D full streams have multi-byte residuals. Their baseline output is **133,799 bytes**, and HRS uses **116,143 bytes: 13.20% smaller**. They account for 39.68% of the baseline's compressed bytes, explaining the 5.24% whole-cohort effect. The seven single-byte controls remain unchanged.

Family totals, all full streams retained:

| Family | Baseline | HRS | Saving |
|---|---:|---:|---:|
| Bicycle demand | 61,040 | 50,749 | 16.86% |
| Traffic | 87,108 | 79,743 | 8.46% |
| Household electricity | 177,334 | 177,334 | 0% |
| Appliance energy | 11,713 | 11,713 | 0% |

Equal-family mean saving is **6.33%**, different from the byte-pooled 5.24%. The multi-byte subset is an applicability view already specified in the protocol; it must not replace the full-cohort headline or become a new post-hoc claim of validation.

C's seven real-derived inputs go from **51,436 to 50,786 bytes (1.26%)**. Their three multi-byte cases go from **6,761 to 6,111 bytes (9.61%)**, only 13.14% of baseline bytes. The two D prefix groups save 2.87% and 4.04% respectively, but prefixes are nested, not independent source families.

## 4. Packing and compatible alignment have complementary target inputs

A POST-HOC offline alternative-choice diagnostic takes the smaller of the actually emitted R2 safety-control frame and the archived E03 optional-packing frame for each file. On D full streams it reaches **309,548 bytes (8.20% below 337,195)**, versus R2's 319,539 and E03's 319,859.

The extra 9,991-byte opportunity beyond R2 is almost entirely one household stream where packing still helps; alignment helps bicycle/traffic. This is **portfolio complementarity, NOT bitpacking followed by plane alignment**, NOT an implemented combined encoder and NOT a free selector. Blindly chaining the transforms can destroy the byte-plane meaning. Retain the diagnostic as a ceiling within these tested alternatives and measure candidate rejection costs before implementing any combined policy.

The result remains larger than the earlier unchanged complete-Kolmo balanced/hc size controls. It does not establish a complete-product win.

## 5. Costs are real; small speed differences remain uncertain

D full-stream duration ratios, sums of per-input medians, first / confirmation:

| Policy vs native baseline | Residual-to-frame encode | Warm verified full-input decode | Displaced verified full-input decode |
|---|---:|---:|---:|
| HRS | 0.997 / 0.938 | 0.972 / 1.026 | 0.978 / 0.973 |
| Costed original-or-HRS | **1.665 / 1.630** | 0.975 / 1.004 | 0.973 / 0.980 |

HRS's fixed construction is materially less costly than retrying both representations on this runner. The smaller apparent time differences are NOT reliable speedups: 328 identical-frame comparisons per pass still show substantial timing spread. Warm identical-frame ratios range 0.761–1.485 in the first pass and 0.695–2.571 in confirmation; medians are approximately one. Small messages and shared-host effects remain important even after decoupling encode and decode.

Measurement boundaries:
- Encode begins with verified frozen residuals and pre-existing parameters/original-input checksum. It includes actual leaf trials, allocations, copies, frame checksum, selection and cleanup; it excludes unchanged prediction/capture, type detection, I/O and process startup. Do not compare its absolute milliseconds with E03's wider encode scope.
- Decode uses the unchanged leaf readers and exact staged KG_INT inverse, allocation/free and software CRC32C of both frame and original bytes. The common software-CRC/inverse work can mask smaller entropy-decoder differences. No entropy-kernel-only speed claim is made.
- Warm batches use deterministic input-size batching; displacement sweeps a fixed 32 MiB buffer outside the timing interval. This is not guaranteed cold LLC or cold storage.
- `entropy_calls` in the records counts whole-leaf candidate arms, NOT internal per-plane/chunk encoder invocations. All internal runtime and model construction are nevertheless inside the measured interval.
- No isolated per-policy memory or allocator-failure result was established. Process-wide RSS with all frames and the displacement buffer would not prove a memory win.

## 6. Verification, not fresh validation

Executed locally against the hash-verified pinned source distribution:
- GCC release and Clang ASan/UBSan/leak builds pass **351 inherited R1 checks plus 144 new deterministic round trips, 1,710 rejected truncations, 3,456 mutation probes and 12 single-plane identity checks per build**.
- All **522 archived configurations** pass in each release pass and in the sanitized pass.
- Audit checks **1,566 retained candidate frame hashes** and **6,264 reported wall/CPU medians**; all 611 entries in the source evidence archive are verified.
- Seven arithmetic unit tests cover independent, synergistic, overlapping and enabling effects.
- Production core/graph and exact executed research source blobs are recorded in EVIDENCE.json. Vendor objects remain uninstrumented; finite tests are not a security proof.

Protocol was published before measurements; code was run locally and its exact file hashes were matched to the published implementation. A failed interactive container invocation was rejected before executing anything; the noninteractive build then succeeded. No failed measurement or inconvenient input was discarded.

All cohorts are already observed development. Household overlaps the repository holdout source. These results do not satisfy independent fresh-data validation, historical-container compatibility, full production integration, M5 acceptance or release gates.

## 7. Decision and next task

Keep a combination ledger (COMBINATIONS.md), not only a standalone leaderboard. Do not add isolated percentage gains. For joint cost C_AB use both conditional gains and `C_A+C_B-C_AB-C_0`; multiplicative independence would predict `C_A*C_B/C_0`, which is a diagnostic, not a guarantee. For sequential 3% and 4% proportional savings the independent result would be 6.88%, not 7%; actual codec interactions can be stronger or weaker.

**Next substantive task:** define and preregister a complete native-compatible combined policy with a cheap, explicitly costed applicability/expansion guard and shared-work accounting, then validate on audited new multi-byte families plus dense/single-byte/constant controls. Do not rerun unconditional graph-plane experiments or reinstate the rejected exhaustive packing selector. Preserve R2 as development controls, quantify the overhead introduced by the guard, and keep any later packing alternative separate until its selection cost earns the additional opportunity.
