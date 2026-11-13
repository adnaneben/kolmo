# S1 codec ordering and placement — executed development results

13 November 2026. **35 fixed configurations, 14 inputs, two timed passes and a sanitizer pass completed locally.** The eight real-derived inputs are previously exposed NPY originals from CollegeMsg and Covertype; six generated inputs test mechanisms only. Every original byte is reconstructed. This is not fresh validation, production integration or a globally best codec claim.

## Main decision

Keep **per-plane Huffman -> zstd-3** and **per-plane Huffman -> LZ4** as conditional layered candidates. Retain standalone codecs: the combination is not the best choice on every input. Reject unconditional post-zstd entropy coding, the tested rANS/zstd combinations and whole-frame double compression as balanced-mode defaults on this evidence.

"Amplification" here means an additional size or cost improvement beyond the relevant standalone control on the same inputs and representation. It does not mean adding standalone percentages or a proof of universal super-additivity. A->B means encode with A, then compress that coded byte stream with B; decode in reverse. H=Huffman, R=singleton-capable rANS, Z=zstd-3, L=LZ4. Both contain complete codec payloads, not an LZ token stream with its entropy stage replaced.

## Order and placement results on the eight real-derived files

The comparator is the smaller aggregate standalone constituent at the SAME placement. Costs include complete KST1 research frames, all stage headers, original header/tail bytes, parameters, tables, CRC, typing, prediction/GCD, copies, allocation and cleanup. Ratios are sums of per-input medians in first / confirmation passes, not continuous-stream throughput. Cost is the active 1-Gbit/s round-trip model. Less than 1 is lower duration/cost.

| Sequence / placement | Standalone control | Stored bytes | Size change | Encode ratio | Decode ratio | Modeled cost ratio |
|---|---|---:|---:|---:|---:|---:|
| H->Z, one joined plane stream | joined Z | 817,471 | +0.96% | 0.90 / 0.87 | 1.06 / 1.07 | 0.97 / 0.95 |
| Z->H, one joined plane stream | joined Z | 809,910 | +0.02% | 1.02 / 1.01 | 1.02 / 1.04 | 1.01 / 1.00 |
| **H->Z, independently per plane** | **per-plane Z** | **789,949** | **-2.37%** | **0.82 / 0.81** | **1.00 / 1.00** | **0.90 / 0.91** |
| Z->H, independently per plane | per-plane Z | 809,490 | +0.04% | 1.03 / 1.04 | 1.00 / 0.99 | 1.02 / 1.02 |
| R->Z, one joined plane stream | joined Z | 986,941 | +21.89% | 1.22 / 1.16 | 1.36 / 1.38 | 1.26 / 1.23 |
| Z->R, one joined plane stream | joined Z | 813,055 | +0.41% | 1.15 / 1.11 | 1.14 / 1.17 | 1.11 / 1.09 |
| R->Z, independently per plane | per-plane Z | 816,319 | +0.89% | 1.08 / 1.07 | 1.32 / 1.32 | 1.15 / 1.16 |
| Z->R, independently per plane | per-plane Z | 816,604 | +0.92% | 1.18 / 1.19 | 1.12 / 1.12 | 1.14 / 1.14 |
| L->H, one joined plane stream | joined H | 879,584 | -11.45% | 1.05 / 1.07 | 0.99 / 0.98 | 0.98 / 0.98 |
| H->L, one joined plane stream | joined H | 849,391 | -14.49% | 1.03 / 1.02 | 1.04 / 1.02 | 0.99 / 0.96 |
| L->H, independently per plane | per-plane H | 872,477 | -8.76% | 1.09 / 1.08 | 1.01 / 0.99 | 1.03 / 1.00 |
| **H->L, independently per plane** | **per-plane H** | **819,778** | **-14.28%** | **1.03 / 1.05** | **1.03 / 1.01** | **0.98 / 0.99** |
| L->Z, one joined plane stream | joined Z | 890,763 | +10.01% | 1.04 / 1.03 | 1.04 / 1.05 | 1.05 / 1.04 |
| Z->L, one joined plane stream | joined Z | 813,000 | +0.40% | 1.01 / 1.00 | 1.00 / 1.02 | 1.00 / 1.00 |
| L->Z, independently per plane | per-plane Z | 884,013 | +9.25% | 0.99 / 0.98 | 0.98 / 0.99 | 1.01 / 1.01 |

Interpretation: putting H->Z at the independent-plane boundary changes a +0.96% size regression into a -2.37% size gain versus its matched Z-only control. In this implementation Huffman reduces the amount of data sent into zstd enough to offset the additional work in the aggregate. This is an inference about the measured pipeline, not an isolated CPU attribution or optimal kernel claim.

Z->H demonstrates wasted work directly: all 807,474 source bytes presented to its second Huffman stage across the real files were internally stored, not entropy-coded. No real input got smaller. Its additional headers remain in the result. H->L and L->H improve on the H/L constituents, but they must ALSO face the stronger zstd alternative; a win over a weak constituent is not automatic mode acceptance.

## Where an apparently small gain amplifies, and where it reverses

For H->Z per plane, compare against the better of H-only and Z-only independently for EACH input. This is a diagnostic comparison, not a free online selection policy.

| Input | Best standalone bytes | H->Z bytes | Change |
|---|---:|---:|---:|
| CollegeMsg source IDs | 79,457 | 78,941 | -0.65% |
| CollegeMsg destination IDs | 82,799 | 82,853 | +0.07% |
| CollegeMsg timestamps | 70,699 | 69,530 | -1.65% |
| Covertype cover class | 22,603 | 17,071 | **-24.47%** |
| Covertype elevation | 168,224 | 157,101 | **-6.61%** |
| Covertype fire distance | 98,303 | 103,640 | **+5.43%** |
| Covertype hillshade | 180,152 | 175,017 | -2.85% |
| Covertype road distance | 103,626 | 105,796 | **+2.09%** |

Thus five of eight get smaller than both constituent controls; three do not. The 24.47% cover-class saving still costs about 1.03x its Z-only modeled cost in the first pass. A large byte gain is not automatically a cost gain.

Generated controls show additional contextual effects, NOT generalization evidence. On repeated categorical runs, joined L->Z is 280 bytes versus 392 for the best tested per-plane standalone coder; on sparse outliers, per-plane R->Z is 36,070 bytes versus 36,284 for per-plane R. Dense bounded values retain H-only as the smallest tested option; full-width noise and all-zero whole files favor direct zstd after all metadata is counted. These controls are retained even when they contradict the real-cohort ranking.

## Placement alone can matter more than a second codec

Keeping zstd-3 fixed, moving predicted residuals from interleaved values to a joined byte-plane layout reduces complete size **16.07%** (964,717 -> 809,734). Splitting those planes into independent Z streams saves only another **0.074%** (809,134) while increasing modeled cost **3.2-4.5%**. It is not an unconditional win.

With rANS, independent plane models reduce size **17.31%** versus the joined stream (986,699 -> 815,875), at approximately unchanged/slightly lower measured encode/decode duration. With Huffman, the same placement change saves **3.73%** (993,348 -> 956,284). Different coders therefore need different stream boundaries.

Adding prediction before zstd but leaving residuals interleaved reduces size versus whole-input Z in this harness, yet increases modeled cost. Representation, coding and implementation cost must be evaluated jointly, not as independent wins that are added together.

## Outer-frame compression: smaller bytes, worse balanced cost

Zstd over a COMPLETE per-plane-Huffman KST1 frame yields **787,794 bytes**, the smallest tested aggregate. It is only **0.273% smaller** than per-plane H->Z's **789,949 bytes**, but takes **1.23-1.24x encode**, **1.28-1.29x decode**, and **1.20-1.22x modeled cost** relative to that alternative. It also wraps away the inner stream boundaries, rather than preserving independent access to them. No random-access benchmark was performed.

An outer Z layer over per-plane rANS instead enlarges size **0.061%** and raises modeled cost **18.8-21.4%**. Reject both outer wrappers as balanced defaults on this evidence; the narrow byte frontier alone is insufficient.

## Competitor-relative boundary

Direct zstd-3 totals **1,354,554 bytes**. Per-plane H->Z is **41.68% smaller**, but aggregate modeled cost is **1.006x / 1.003x** direct zstd, not a strict aggregate cost win. Four individual inputs (both CollegeMsg IDs, elevation, hillshade) pass the observed smaller-and-every-pair-cost screen in both runs. H->L per plane totals **819,778 bytes**, with modeled cost **0.969x / 0.970x** direct zstd, but only three files pass that same strict per-file screen. No universal selector is implemented or accepted.

These small cost differences are sensitive to measured noise. Duplicate zstd controls span 0.85-1.17x encode, 0.89-1.13x decode and 0.93-1.08x modeled cost per real input; duplicate H controls also vary. Do not convert observed paired outcomes into a statistical guarantee, precise target-machine SLA or unconditional win. No arbitrary percentage saving threshold was introduced, and none of these observations changes earlier N2 verdicts.

No same-run OpenZL measurement was made. **Archive acceptance is NOT_EVALUATED**, regardless of a small stored-size result. Direct zstd retains checksum-off defaults, while KST1 performs an original-byte CRC32C: this is not equal-integrity-feature dominance. The reference implementation deliberately materializes intermediate buffers; every copy is timed, but no optimized fused-decoder or isolated scratch-memory benefit is claimed.

## Verification and preservation

- New native framing and layered paths: **420 selftest round trips**, **2,079 rejected truncations** and **2,376 mutation probes**, in each GCC release and Clang ASan/UBSan/LSan build.
- **490 real/synthetic input-configuration cases** under sanitizers; all complete reconstructions and deterministic frames passed.
- Same **490 cases in each timed pass**, 7 then 5 rounds, **11,760 timed batches**, with every timed output checked. All frames match across sanitizer/first/confirmation executions.
- Separate Python KST1 parser and numeric inverse reconstructed **all 490 first-pass frames**, using pinned entropy readers as subprocesses. This is independent framing/numeric reconstruction, not a separately implemented entropy decoder.
- The auditor checked completeness, **1,470 frame instances**, **11,760 timing rows** and recomputed **2,940 encode/decode/cost medians**. Eleven runner regression tests passed. A standalone preparation script reproduced all fourteen input bytes and metadata exactly.
- Frozen-source build receipts identify 250 dependencies plus new source/binary identities. Included core/bridge/driver were instrumented; the vendor archive was not. Finite checks are not a security proof, and comprehensive allocation-failure injection was not performed.
- An initial combined build call timed out during sanitizer compilation; the final separate build completed. An audit attempted before confirmation completed refused the missing completion receipt; final audit passed. Neither incident discarded a data point or changed the matrix.

The source and protocol are pinned; source hash equality was checked after staging the Git blobs. Compact results, reproduction code and hashes are Git-tracked. The delivered evidence archive retains original inputs, all frames, raw timings, stage accounting, logs and full audits; that large archive is separate from Git and is not represented as permanent Git storage. No new benchmark workflow, paid resource, automation, production change, merge or release was initiated.

## Next substantive task

Freeze a bounded **per-plane conditional layer policy** on development data: retain H-only, Z-only, H->Z and H->L as alternatives, and test applying the second stage only to planes where a cheap, charged statistic predicts benefit. Compare with the fixed S1 controls and prohibit a free hindsight selector. First eliminate avoidable staging copies without changing output; record that optimization separately. Then validate the frozen complete policy on genuinely new sources, with matched zstd/OpenZL budgets and decoder costs. Do not stack every codec indiscriminately.
