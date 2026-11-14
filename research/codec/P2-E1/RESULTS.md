# P2-E1 results (development plumbing — not P2 evidence)

Local run, 2-vCPU Intel Xeon @ 2.10 GHz, gcc 13.3, one thread, 5 paired rounds per input, 20 inputs of the E02C
development corpus, 21 pipelines (`p2e1.c`). Raw records and the analysis are in `results/` (hashes in
`SHA256SUMS`, tool hashes and HEAD in `ENV.txt`). Every pipeline reconstructed every input byte for byte, before
and after every timed sample. ASan/UBSan/LSan: clean on four inputs in both `run` and `mem` modes. Shared 2-vCPU
machine: timings are indicative; this run decides what to build and measure next, nothing else.

## Policy evaluator (unchanged `policy.py`)

- Speed modes (rival zstd-3): `NO_FEASIBLE_CANDIDATE` on all 20 inputs, because the evaluator correctly rejects
  every record: production pipelines are not typed before planning, and type-first/stacked pipelines use research
  or emulated containers. It would also reject them as development data.
- Archive modes: `BLOCKED_MISSING_COMPETITOR` on all 20 (OpenZL not measured).

## Development feasibility (same `run_cost` model and paired runs; plumbing only)

Cost = encode + decode + stored bytes / 125 MB/s (1 Gbit/s), every one of 5 paired runs checked against zstd-3.

| Pipeline | Cost vs zstd-3 (range over inputs) | Fits every pair | Smaller than zstd-3 | Both |
|---|---|---|---|---|
| `kolmo_fast` (production default) | +34% … +876% | 0 / 20 | 17 / 20 | **0** |
| `kolmo_fast_Q` (minimal analysis) | −51% … +202% | 4 | 8 | 4 |
| `kolmo_speed` (rt + minimal) | −56% … +133% | 11 | 2 | 2 |
| `typed_fast` (declared type first, production INT encoder) | −36% … +412% | 2 | 16 | 2 |
| `typed_stack_fast` (+ per-plane leaves, emulated) | −19% … +732% | 1 | 17 | 1 |

Best feasible-and-smaller pipeline per input (5 of 20 inputs have one): `fs_inode` typed −80.7%, `syn_walk` typed
−56.7%, `syn_sparse_outliers` minimal −28.3%, `fs_size` minimal −15.1%, `dem_jacksboro` minimal −3.9%.

## What this shows

1. **Under P2 the production fast setting is not a candidate on this corpus.** It is smaller than zstd-3 on 17/20
   inputs (up to −80.7%) but its encode is 3× to 20× zstd-3's, so it never fits the round-trip budget at 1 Gbit/s.
   The binding cost is the **encoder's analysis**, not leaf decoding.
2. **Typing first is the largest single cost lever found so far.** With the type read from the input's own header,
   the same production INT encoder reaches the planner's bytes (often a little smaller) at a fraction of its cost
   (e.g. audio_curve +443% → +78%, fs_inode +96% → −24%, syn_walk +74% → −36%).
3. **Typing needs an applicability guard.** On `fs_mtime` the forced INT graph is +130% vs zstd-3; the planner was
   right not to use one there.
4. **The E02G per-plane stack costs more than it saves at 1 Gbit/s when the base is zstd-coded residuals.** Its
   bytes saved are worth microseconds of transfer; its extra encode/decode is worth milliseconds. The decode misses
   E02G-verify confirmed are real but are not what blocks P2 feasibility.
5. Remaining type-first overrun on 16-bit audio/images is 0.5–2.5 ms per file. Profile (callgrind, audio_soft):
   about 44% of typed encode is leaf coding (zstd on the residual), 34% three generic predictor/GCD passes with a
   64-bit division per value, 22% predictor choice; 72% of decode is the generic integer inverse.

## Next (P2-E2, development)

A type-first fast integer path whose parameters and residual stream must be byte-identical to production's, with
specialised passes and inverse, cheaper per-plane leaves (Huffman per plane instead of zstd on the whole residual),
a cheap sampled zstd guard for inputs like `fs_mtime`, and a real serialized research container. Then the OpenZL
archive rival and fresh preregistered validation.
