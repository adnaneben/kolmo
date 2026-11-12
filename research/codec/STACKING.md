# Stacking and amplification map

Which provisional gains combine, which amplify in a particular context, and which to test, verify and validate next.
Companion to [`PROVISIONAL.md`](PROVISIONAL.md) (entry IDs P-…). Written 2026-11-12. All figures are on **observed
development data**; nothing here is validation.

Terms: a **stack** is two changes applied together in one pipeline; an **amplifier** is a change worth little alone
that raises another change's gain; **alternative selection** (choosing one of several representations per file or
block) is not a stack and is labelled separately. Percentages are never added: interactions are measured on a common
baseline, as `R2-stack/PROTOCOL.md` also requires.

## Ownership

- **Entropy-level stacks** — aligned Huffman × aligned rANS × singleton rANS (P-17, P-18), on E03's inputs and its
  E2C1 framing — are owned by the parallel **R2-stack** protocol (`R2-stack/PROTOCOL.md`, `b2f3c16`). Not duplicated
  here; its results feed rows ST-3 and ST-4.
- **Production-boundary stacks** — complete Kolmo frames with production's zstd leaf rule, levels, the sampling guard
  and the planner's own block choices — are covered here, with the production-connected harness.

## Map

| ID | Combination | Kind | Evidence so far | Verdict |
|---|---|---|---|---|
| **ST-1** | Per-plane leaves (P-01) + sparse-leaf guard (P-03) — **E02G: provisional both modes, size met in both families; guard reproduced** (`E02G/RESULTS.md`) | **Amplifier** | Guard alone: 0 bytes changed on every input, both regimes (`results-stack1`). With planes, typed regime: **+4.48% → −0.80%** (rescues one 161 KB sparse plane in `audio_concat`). Complete planner: never triggered (no sparse leaf ≥ 256 KiB in the planner's blocks) | **Validate** — and a safety requirement: any per-plane integration must carry the guard |
| **ST-2** | Per-plane leaves + **context gate from KG_INT parameters** (order 0, or residual width ≥ 4) | **Amplification by context** | Planes gain by context (`stack.json`): order 0 with 2-byte residuals −7.5% to −14.9%; 4-byte residuals −7.2% to −9.0%; 2-byte order-1 dense residuals −0.07% to −4.2%; single-byte 0.00%. A parameter-only gate keeps 60–90% of the gain on fewer files; an ideal gate adds no bytes beyond always-on planes | **Use as the validation phenotype.** As a size gate it is not needed; it only matters if decode cost is binding |
| ST-3 | Per-plane entropy-only coding (P-07) + aligned chunks in one leaf (P-17) + singleton rANS (P-18) | Stack | X-01: P-07 is −1.7% to −18.5% vs the native baseline; R1 feasibility for P-17/P-18 | **R2-stack** (in progress) |
| ST-4 | Per-plane layout + per-file choice of coder or packing (P-06, P-07, P-12, P-14) | Alternative selection | Level-matched offline ceiling vs production: −2.6% (dev fast), −7.2% (E02E fast), −2.7% (E02F fast); −0.65% to −4.4% in hc. Ideal choice, selection cost not charged | **Test only after R2-stack**, with a cheap cost-aware gate (R1 P-19); E03 showed full trials cost about 1.7× encode |
| ST-5 | Per-plane leaves + hc level (zstd -19) | Stack | Composes as expected: −0.64% (dev), −0.29% (E02E), −0.37% (E02F) | Covered by ST-1 validation |
| ST-6 | Packing (local/patched) + per-plane leaves on the same residual | Neither | Both transform the same residual stream, so they are alternatives, not a sequence | Do not claim a stack; handled as ST-4 |
| ST-7 | Leaf improvements + **planner feedback** (the planner chooses transforms using zstd trial costs; a cheaper INT leaf could change its choice) | Second-order stack, **untested** | Every measurement so far holds the planner's choices fixed; e.g. fs_mtime is not an INT graph in production | **Test (development)** after ST-1: re-plan with candidate leaf costs, report blocks whose plan changes |
| ST-8 | Small-message amplification (pageviews, 14.7 KB): per-plane −7% to −12% where absolute savings are 400–800 B | Context | Gains are large in percent, small in bytes | Weight by bytes in any decision; keep family-equal and byte-pooled summaries separate |

## Decision: what to test, verify, validate

1. **Validate ST-1 with ST-2 as the eligibility phenotype** — a single frozen stacked pipeline: `planes+prod3g` (fast)
   and `planes+prod19` (hc), on files whose INT blocks have order 0 or residual width ≥ 4, requiring blocks large enough
   for the guard to be exercised (leaves ≥ 256 KiB). This needs a fresh preregistration (E02G) with new sources of
   **large count/telemetry/timestamp series**, checked against every manifest and the observed-corpora list (D-02),
   with dense multi-byte signals as non-target controls. Freeze it before any data is read.
2. **Verify ST-1's mechanism** before E02G data: one constructed case already proves the guard fires and shrinks a
   sparse plane (`selftest guard_case`). E02G must also report how often the guard fires on real data.
3. **Test ST-7** (planner feedback) on development data: does a cheaper per-plane INT leaf change planner decisions, and
   by how much? It is the only route by which leaf gains reach blocks the planner currently codes otherwise.
4. **Wait for R2-stack** (ST-3) before ST-4; ST-4's ceiling is large but selection cost decides it.

## Sequences and locations (transform order)

A transform's gain depends on **where** in the pipeline it sits and **what runs before it**. The pipeline locations are:

| Loc | Stage | What can change there | Constraint now |
|---|---|---|---|
| L0 | Planner: block split, transform choice | which graph a block gets (INT, table, raw) | unchanged; ST-7 measures feedback only |
| L1 | Structural transform | column split, table fields, DECIMAL mantissa/exponent | existing transforms only |
| L2 | KG_INT predictor, GCD, bias | order 0/1/2 | **fixed** (AGENTS.md) |
| L3 | Residual layout | existing, byte planes, global/local/patched packing | research location |
| L4 | Leaf coder | stored, Huffman, rANS, zstd level, sparse-leaf guard | research location |
| L5 | Container | leaf chunk size, aligned chunks in one leaf (P-17) | R2-stack |

| ID | Sequence | Status | Evidence / reason |
|---|---|---|---|
| SQ-1 | L2 predictor → L3 planes → L4 coder | **Measured, provisional** | E02E/E02F/E02G; this is the ST-1 pipeline |
| SQ-2 | L3 planes → L4 guard (guard after layout) | **Measured** | Guard alone changes 0 bytes; after planes it rescues sparse planes (ST-1) |
| SQ-3 | L3 packing → L4 zstd | **Measured, negative** | Development result kept: packing before zstd generally enlarges (`E02C-harness/RESULTS.md` summary) |
| SQ-4 | L3 planes **before** L2 predictor (transpose raw values, then predict) | Not run; expected worse | Splitting bytes first removes the carry structure the predictor uses; test only as a cheap development check if a reason appears |
| SQ-5 | L3 planes → per-plane choice of L4 coder | Partly measured | `planes+ent` (best of Huffman/rANS per plane) is X-01's −1.7% to −18.5%; overlaps ST-3/ST-4 |
| SQ-6 | L1 column/field split → L2 INT → L3 planes | **Candidate, untested** | Planes only help where the planner already builds an INT graph; a split that creates more INT columns would carry the gain to more bytes. Same mechanism as ST-7 |
| SQ-7 | L1 DECIMAL mantissa → L2 INT → L3 planes | **Candidate, untested** | Reuses the existing DECIMAL transform; no new floating-point coding. Needs a check that it stays inside "no floating-point work yet" before any test |
| SQ-8 | L3 planes → L5 aligned chunks (P-17) + singleton rANS (P-18) | In R2-stack | Container-level stack; owned by the parallel workstream |
| SQ-9 | L2 predictor chosen by final coded cost (not residual entropy) | **Blocked** | Changes L2, which is fixed in this continuation; listed so it is not lost |

Order rules used here: transforms that **remove redundancy** (predictor, GCD) come before transforms that **re-arrange
bytes** (planes, packing), which come before **coders**; two re-arrangements of the same stream are alternatives
(ST-6), not a sequence. A stack is credited only when the combined pipeline is measured end to end on a common baseline.

**What to test next, in order:** (1) SQ-6/ST-7 together on development data: re-plan with the candidate leaf costs and
count bytes newly routed to INT+planes; (2) SQ-7 only after the constraint check; (3) SQ-4 only if (1) shows a reason.
SQ-9 waits for an authorization change.

## Reproduce

`python3 -I research/codec/E02C-harness/stack.py` recomputes the context, gate and oracle diagnostics from committed
records (`stack.json`). The guard-stacking run is in `E02C-harness/results-stack1/` (development corpus, frozen harness,
5 repetitions, seed 20261116).
