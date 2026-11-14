# P2-E9 — fused-order predictor choice (development)

The type-first path spends most of its time on small arrays (<= 16384 values) in predictor choice, which evaluates
the whole array for every order and sign. `fastchoose.inc` evaluates the three orders in the same sweep while keeping
each order's arithmetic, gcd early exit and the comparison order of production `kg_num_choose`.

Correctness: decision and returned cost identical to production on **200,000** random arrays (random, walk, line,
few values with gcd, runs, int64 extremes, quadratic; widths 2/4/8; lengths incl. the 16384 sampling boundary) plus
5,000 under ASan/UBSan (`results/diffchoose.jsonl`); on all 19 typed development inputs the choice, parameters,
residual, rest and inverse remain byte-identical to production (`results/identity.jsonl`).

Development timing (local 2-vCPU, in memory, 5 paired rounds; `results/summary.txt`): `tf_planes_huf` fits zstd-3's
cost on 15/20 and is feasible-and-smaller on 9/20 (P2-E2: 14 and 8); mri +8% and topobathy +21% (still over). A
correctness-safe cost reduction of modest size on this noisy machine; needs runner timing before any claim.
`p2e9.c`/`fastint9.inc` are copies of P2-E2's files with only the choice call changed.
