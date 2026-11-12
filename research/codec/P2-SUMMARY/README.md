# P2 overnight summary (12/13 November 2026) — Claude workstream, development evidence only

Policy: P2 competitor-bounded (`benchmark/accept/ACTIVE_POLICY.json`). Nothing here is P2 acceptance: all data is
observed or derived, and every type-first result uses a research container that `policy.py` rejects (decision P2-D1
in `PROVISIONAL.md` §A4). Production source, format, defaults and other branches are unchanged.

## What was learned

1. **Production Kolmo `fast` is not a P2 speed-mode candidate on this evidence.** It is smaller than zstd-3 on 17/20
   development inputs but never inside zstd-3's round-trip cost (+34% … +876%, in memory), and on N1's CLI boundary
   it fits on 1/10 (P2-E1, P2-E3, P2-E7). The cost is encoder analysis, not leaf decoding.
2. **Typing first is the main lever, and it stacks with cost-cutting layers.** Declared type → identity-preserving
   fast KG_INT passes/inverse (byte-identical to production, P2-02) → byte planes → Huffman per plane. On N1's CLI
   boundary against the pinned zstd-3 CLI this is smaller and inside the cost on 7/8 typed NPY inputs (both runs,
   two independent workflow runs: P2-E3, P2-E7); production fast/quick/hc fit on 0/8.
3. **Tables work the same way.** Type-first CSV columns (INT, exact fixed-scale DEC, TEXT) are the smallest pipeline
   inside zstd-3's cost on both N1 tables (−73.0%, −36.1%; P2-E7).
4. **Archive mode:** against OpenZL's CSV profile at level 9, production `kolmo hc` (−41.1%, −29.9%) and typed CSV
   at level 19 (−40.4%, −29.1%) are both smaller at lower modeled archive cost (P2-E7). NPY archive comparison is
   blocked (OpenZL v0.3.0 has no `numpy` profile; adapter owned by the N1 workstream).
5. **A realizable rule** (declared array → guarded per-plane Huffman; typed CSV → typed CSV; else zstd) is inside
   zstd-3's cost on 10/10 N1 inputs and smaller on 9/10 in both runs (`portfolio.py`, `portfolio.txt`). In memory on
   small development inputs it fits far less often (arrays 8/20, CSV 2/10): fixed per-call costs dominate small inputs
   there, and the guard's own probe costs as much as it saves. Inputs < ~0.3 MB are the weak spot.
6. **Negative / not cost-effective under a speed rival:** zstd-coded residual leaves on large tables (archive-mode
   only), probe-based leaf selection (P2-08), the unfused zero-run leaf. E02G's decode misses are real but not what
   blocks P2 feasibility.

7. **The best type-first stack exists in today's format (P2-E11).** Written as a standard `CZN1` frame (one KG_INT
   graph block; per-plane Huffman as plane-aligned chunks inside one ordinary Huffman leaf), it is decoded by the
   unchanged production decoder and CLI (60/60 frames); in memory it fits zstd-3 on 13/20 and is feasible-and-smaller
   on 9/20 development inputs. Only the encoder is new, so decision P2-D1 no longer blocks the main result.
8. **Archive frontier (P2-E8):** inside OpenZL csv-9's archive cost the smallest pipelines are production `hc`
   (Covertype −41.1%) and `max` (CollegeMsg −30.6%); heavier modes are smaller but over cost at these sizes.
9. **Fused predictor choice (P2-E9)** is identical to production on 200,000 random arrays and trims small-input cost.
10. **Broader cohort (P2-E10, 61 inputs, 11 families, Actions 43318087650):** type-first per-plane Huffman fits zstd-3's
   speed cost and is smaller on 43 / 41 inputs (two runs); **production Kolmo (fast, -q, hc, max) on 0/61**. Archive vs
   OpenZL's typed `le-*` profile at level 9 on whole files: some Kolmo pipeline is smallest within OpenZL's cost on
   47/61; OpenZL keeps 14 (GHCN 7/9). The research per-plane best-entropy pipeline wins most often (23 / 25).

## Correctness evidence behind these claims

Identity checks against production (19/19 typed inputs: choice, parameters, residual, rest, inverse); 3.3 M
differential inverse fuzz cases; 140 k mutated containers; 300 CSV edge cases + 40 SWAR stress files; typing
agreement with an independent oracle (310/310 + 40/40); ASan/UBSan/LSan on every research pipeline before every
timing run (runner gates: P2-E3 all inputs × 7 policies, P2-E7 50/50).

## What is needed next (in order)

1. ~~P2-E11 frames on N1's boundary~~ **done (P2-E12, Actions 43326268053)**: production-format type-first frames,
   decoded by the unmodified production CLI, fit zstd-3 and are smaller on 9/10 inputs in both runs (best per input
   −37…−64% on typed columns); production `hc` 0/10. Next in the runner queue: the N1 owner's exact typed-NPY OpenZL comparator.
2. **Fresh, source-disjoint whole original files**, registered before reading (candidates not referenced anywhere in
   the repository: ESC-50 16-bit WAV clips, FiveThirtyEight data CSVs; fetching them needs approval).
3. OpenZL typed NPY adapter (N1 owner) to unblock archive mode for arrays.
4. Small-input cost: per-call fixed costs and a zero-probe guard.

Runner use by this workstream tonight: P2-E3 ~13, P2-E7 ~13, P2-E8 ~19, P2-E10 ~48, P2-E12 ~11 minutes (N1 ~11 by the
parallel workstream). Decision P2-D1 remains useful for research containers (typed CSV) but no longer blocks arrays.
