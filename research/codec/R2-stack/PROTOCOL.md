# R2-stack: interactions, not sums of isolated percentage gains

Registered 2026-11-12 before the new timing/factorial runs. Development experiment on already observed data, NOT new holdout validation. User request: investigate stackable gains and contextual amplification of provisional small improvements. Write only to fix/planner-record-ownership-isolated-20261112; PR #7 stays draft and unmerged. Preserve concurrent Claude work and all frozen experiments. No production source, format, default, release-target or other-branch changes.

## Scope and comparators

Reproduce R1's decoder-compatible leaf construction using exact pinned HUF/rANS leaf functions and the pinned KG_INT inverse from production reference 2f3308a3380a140ad0f6873ae228de7715e5eef5 (core blob e06467c8872d494b97270811bedf6fd0bac2aa31; graph 91ad067dc583b9d9e6f060f214a0f7c402bb4a31). Use the 58 archived E03 inputs/captures without changing prediction, GCD, seeds, residual order or source data. Source archive artifact 13929641752; input artifact 13961345681. Verify their archived hashes and frame identities. E02C/E02D are observed DEVELOPMENT; household power overlaps the repository's release-holdout source. Do not claim new generalization.

Primary factor matrix, all eight combinations:
- H: align Huffman chunks at existing residual byte-plane boundaries.
- R: align rANS chunks at those same boundaries.
- S: permit the singleton normalized frequency table in a research-only rANS encoder; old decoder unchanged.
- Each cell executes actual stored/Huffman/rANS competition, retaining the smallest with deterministic ties (stored, then HUF, then rANS). Alignment-disabled calls the original chunk schedule. Single-plane inputs are retained as no-op controls. Tables and chunk headers count.

A ninth policy is a costed no-expansion control: execute the original native baseline plus the fully combined H+R+S alternative, reuse an identical trial where possible, and keep the smaller complete frame. Charge every attempted encoder call, losing trial, copy, allocation and cleanup. This is a reference policy, not a proposed fast selector. No per-plane codec mixing is implied: the chosen leaf remains one existing codec. Where a candidate fails its coding trial, record the failure and stored fallback; do not relabel it as successful rANS compression.

An explicitly offline broader candidate-set diagnostic may include the already archived E03 packing output. This is alternative selection, NOT sequential composition; show overlap in saved bytes and do not call it an implemented joint encoder. Do not execute the old bitpacker and plane alignment in an invalid order simply to claim a stack.

## Reconstruction and cost boundary

Construct complete research E2C1 frames (same 64-byte framing and unchanged parameters/rest as E03) containing actual candidate payloads. Decode with unchanged pinned leaf readers and KG_INT inverse; verify original length/checksum and full bytes. Preserve headers/tails and existing limits. This is not an integrated CZN1 encoder or a claim about every historical decoder.

Encoding starts from a frozen captured residual (not re-running unchanged type detection/prediction); count all leaf trials, actual selection and frame formation. Report as residual-to-frame component time, not full-product encode throughput. Decoding is complete research-frame decoding and verified-input reconstruction, with no encoder immediately before timing. Report the scope explicitly.

Measurements: 7 rounds per case/policy, then 5 confirmation rounds, balanced rotation of policy order with a recorded shuffle seed. Short-input batching uses a deterministic size-based batch count, not performance-based tuning. Warm repeated decoding and cache-displaced decoding are separate regimes. Cache displacement uses a fixed 32 MiB buffer outside the measured interval; do not call that disk-cold or guaranteed LLC-cold. Preconstruct all candidate frames before the decode phase. Exact identical-frame pairs are noise controls. Record wall/CPU times, batch counts, absolute ns and ratios. RSS is process high-water including all held frames/runtime, not isolated per-policy scratch; do not rank policies by shared process RSS.

Correctness: original baseline size/hash agreement; deterministic re-encode; old leaf/inverse reconstruction; bounded lengths/coder IDs; GCC release and Clang ASan/UBSan/leak tests; complete candidate outputs checked by independent existing numeric reference where applicable. Vendor instrumentation limits remain explicit. No security-proof language.

## Interaction analysis

For stored size C, report conditional gains, not sum(A% + B%). On a common baseline, additive byte interaction I(A,B) = C_A + C_B - C_AB - C_0; positive means joint savings exceed the sum of isolated byte savings. Also report multiplicative residual-size null C_A*C_B/C_0 and its difference from C_AB. For the three factors report each factor's contribution with both other factors present, pair interactions at both levels of the third factor, and all joint sizes. Interaction is a measured descriptive result on these cases, not a significance claim from timing repetitions.

Separate C real-derived, D full-stream, D nested-prefix and synthetic groups. Report each source family and multi-byte/single-byte applicability. Describe failed or non-applicable cells rather than deleting them. A singleton fix can look useless alone but enable aligned rANS; HUF and rANS improvements can instead compete for the same winning leaf. Both possibilities are explicitly tested.

All emitted bytes count; percentage benefits on a leaf are weighted by the fraction of full-file cost affected. Source families, not repeated timings or correlated columns, are units for generalization. Keep family-weighted and byte-pooled summaries separate. Do not pool with hybrid zstd studies, derived end-to-end timings, or different framing.

## Decision and follow-up

Preserve previous registered failures. R2 may retain a new combination for future validation even when a component failed alone, but may not rename old outcomes. Report Pareto size/time/cost evidence without changing TARGETS.md or inventing a release pass. No new default/format integration follows from development results. Before validation freeze the complete combined pipeline, profile budget and source-level eligibility on at least two applicable new families plus dense/no-op controls; audit all benchmark/holdout manifests and the observed-source register. Negative development patterns stay as regression controls, not fresh holdout.

Next queued combinations, not implemented in this phase: compatible alignment plus shared histogram/scratch work; zero-map/run coding plus separate nonzero magnitude streams; patched low bits plus separate position/high-part streams; small-record batching plus amortized models. Each needs its own compatibility/cost boundary. Table reuse or arbitrary codec mixing may require new signalling and must not be smuggled into old leaf IDs.
