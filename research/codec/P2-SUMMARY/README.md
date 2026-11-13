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

16. **Fresh preregistered validation (P2-E20, Actions 43350744793; ESC-50 WAV + FiveThirtyEight CSV):** archive on
   fresh audio **passes** (Kolmo pipeline smallest within OpenZL `le-i16`-9 cost on 35/40 in both runs, −10.3% bytes);
   the speed rule beats zstd-3 within cost on 25/40 and 24/40 but **fails** the preregistered CP95 > 0.5 bar
   (production 0/40); fresh real-world CSVs **fail** in archive (OpenZL 23/23 within cost; Kolmo hc/max smaller by up
   to 43% but 1.05–2.1× its cost). **P2-D1 closed by test**: research containers not useful (4/24, 1/24).
17. **Wire review (13 Nov):** singleton `C_RANS` chunks are conformant existing-format use (4,500 mutants, 0 findings);
   the GHCN order-1 leaf (P2-19) would reach only OpenZL parity and is not pursued.

18. **Text-heavy CSV archive (P2-E21, Actions 43355382170):** the production table graph with BSC leaves, written as
   a production frame (`tt_bsc0`), fits OpenZL `csv`-9's archive cost and is smaller on 16–17/23 development CSVs
   (−20%) and on 23/24 fresh plotly CSVs (preregistered PASS, −15.3%) — but 21 of those 24 fresh files come from one
   t-SNE generator. **Diverse fresh confirmation (P2-E22, 24 TidyTuesday datasets) FAILED**: 12/23 (LB 0.335); the
   zstd-12 table frame (17/23) and production `eco` (13–14/23) were the more robust secondary pipelines.
   **P2-E23 (zstd-12 frame as primary, 24 new datasets) also FAILED: 12/24**; there `eco` was 18/24 and `tt_bsc0`
   15–16/24. Rankings among these near-boundary pipelines change between samples; nothing is validated for CSV.

19. **CSV archive validated for production `eco` (P2-E24, Actions 43401044132):** on 35 new diverse TidyTuesday
   CSVs, `kolmo -m eco` fits OpenZL `csv`-9's archive cost and is smaller on 27/35 in both runs (Bonferroni-adjusted
   LB 0.575 → PASS; −7.3% bytes). Fourth of four sequential CSV tests (E21 pass/low diversity, E22 fail, E23 fail).

20. **CSV picker validated (P2-E25 → P2-E26, Actions 43403830702):** line regularity of the first 64 KiB picks the
   production table program with zstd-12 leaves (regular) or production `eco` (otherwise); on 35 new TidyTuesday
   datasets it fits OpenZL `csv`-9's archive cost and is smaller on **33/35** in both runs (Bonferroni over five
   sequential CSV tests, LB 0.781 → PASS; −16.0% bytes), beating `eco` alone by 3–4 inputs.

## Correctness evidence behind these claims

Identity checks against production (19/19 typed inputs: choice, parameters, residual, rest, inverse); 3.3 M
differential inverse fuzz cases; 140 k mutated containers; 300 CSV edge cases + 40 SWAR stress files; typing
agreement with an independent oracle (310/310 + 40/40); ASan/UBSan/LSan on every research pipeline before every
timing run (runner gates: P2-E3 all inputs × 7 policies, P2-E7 50/50).

## What is needed next (in order)

1. Archive, CSV: the picker is validated (P2-E26, 33/35). Next: a cross-aggregator fresh check, then (if authorized)
   wiring the regularity choice into the production CSV path. (`hc`'s 1.3 s case is the CR-only file, P2-E20 follow-up.)
2. Speed rule on audio: understand the categories where `tp_phuf_pg16` misses (natural soundscapes,
   interior/domestic) using **development** data only; any revised rule needs a new preregistration and new fresh data.
3. If promotion is ever wanted: authorization for the one-line `rans_norm` change in `src/` plus conformance vectors
   in `tests/` (wire review §A), and the production CLI exit-time buffer leak.

Runner use by this workstream tonight (approx. minutes): P2-E3 13, P2-E7 13, P2-E8 19, P2-E10 48, P2-E12 11,
P2-E14 14, P2-E16 3, P2-E17 32, P2-E18 1, P2-E20 16, P2-E21 12, P2-E22 13, P2-E23 ~47 (hung build, cancelled) + 10 (re-run), P2-E24 13, P2-E26 11 = ~276 (plus N1 ~11 and the N1 exact-NPY run by the parallel workstream).
Decision P2-D1 remains useful for research containers (typed CSV) but no longer blocks arrays.
