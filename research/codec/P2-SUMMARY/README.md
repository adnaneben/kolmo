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

11. **Leaf selection must be cheap (P2-E13, local):** sharing one typed pass and trial-coding both leaves (or
   predicting with probes) is cost-negative against zstd-3 in memory; choose the leaf from declaration-level facts.
12. **The archive winner now exists in today's format (P2-E13):** per-plane rANS leaves (`C_RANS`, R1 singleton
   normalisation, P-18) decoded by the unmodified production CLI match P2-E10's `tf_planes_ent19` within 0.1%;
   `tp_arch19` adds production zstd-19 leaves. Correctness: 60/60 corpus, 360/360 random NPY stress.
13. **P2-E14 (Actions 43338093009, 61 inputs, production frames, production CLI decode):** guarded per-plane Huffman
   fits zstd-3 and is smaller on 49/47; production `kolmo fast` 0/0. The per-input oracle frontier of production-frame
   pipelines is −14.8%/−15.0% byte-weighted vs zstd-3; on the CLI boundary `tb_best` and `tp_pent` fit far more
   often than in memory (boundary-specific claims only). Archive: a Kolmo production frame is smallest within
   OpenZL-9 typed cost on 45/61, OpenZL on 16/61.

14. **Selection (P2-E15/E16):** a fixed guarded per-plane Huffman frame captures ~80% of the oracle speed-mode byte
   gain; size rules, decision rules and 1-NN over encode-time features do not beat it out of family (and the features
   cost ~10% of zstd-3's round trip). Second leaves (zstd-3/rANS/zstd-19) are archive-mode tools.

15. **Archive frontier in one run (P2-E17, Actions 43341163635):** inside OpenZL typed-9's archive cost, some Kolmo
   pipeline (all decodable by the production CLI) is smallest on 47/61 inputs; production modes alone 37–39/61.
   `max` holds audio/pageviews, `tp_pent` takes MIT-BIH/EEG; OpenZL keeps GHCN daily weather (7/9).

## Correctness evidence behind these claims

Identity checks against production (19/19 typed inputs: choice, parameters, residual, rest, inverse); 3.3 M
differential inverse fuzz cases; 140 k mutated containers; 300 CSV edge cases + 40 SWAR stress files; typing
agreement with an independent oracle (310/310 + 40/40); ASan/UBSan/LSan on every research pipeline before every
timing run (runner gates: P2-E3 all inputs × 7 policies, P2-E7 50/50).

## What is needed next (in order)

1. **Preregistered, source-disjoint validation** of the fixed rule `tp_phuf_pg16` (speed vs zstd-3) and of the
   archive pipelines `tp_pent`/`tb_best` beside production `max` (vs OpenZL typed-9), on whole original files
   registered before reading (candidates not referenced in the repository: ESC-50 16-bit WAV clips,
   FiveThirtyEight CSVs; **fetching them needs the user's approval**).
2. **Wire review** of singleton `C_RANS` chunks (existing decoder behaviour, used by `tp_pent`) before any
   production encoder emits them; and review of the proposed fast order-1 entropy leaf for GHCN-like series (P2-19).
3. Header-exact OpenZL control: the N1 exact-NPY run (43334484927) is recorded by its owner.
4. User decision P2-D1 (research containers for typed CSV).

Runner use by this workstream tonight (approx. minutes): P2-E3 13, P2-E7 13, P2-E8 19, P2-E10 48, P2-E12 11,
P2-E14 14, P2-E16 3, P2-E17 32, P2-E18 1 = ~154 (plus N1 ~11 and the N1 exact-NPY run by the parallel workstream).
Decision P2-D1 remains useful for research containers (typed CSV) but no longer blocks arrays.
