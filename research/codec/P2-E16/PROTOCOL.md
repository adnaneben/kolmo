# P2-E16 protocol (frozen before measurement): encode-time features for an out-of-family speed selector

Question (P2-E15): can a near-free per-input signal from the shared type-first pass predict when a second leaf
(`tb_best`'s zstd-3 leaves, `tp_pent`'s rANS) beats the guarded Huffman frame within zstd-3's complete-CLI cost?

- Runner job: rebuild the P2-E10 cohort with the unchanged registered scripts, then run `p2e13 feat` on every input.
  The features are typed/forward status, width, predictor order, signedness, output width, planes, gcd, residual and
  rest bytes, zero fraction and per-plane order-0 entropy. The job also records its own extraction wall time.
  It does no compression timing and writes no frames.
- Analysis (local, `ranker.py`), over P2-E14's committed records for the same 61 inputs:
  - Train on 10 families and test on the held-out family (leave-one-family-out).
  - Models:
    - (a) depth-1 and depth-2 decision rules over the features;
    - (b) a 1-nearest-neighbour baseline.
  - Choice set: {`tp_phuf_pg16`, `tb_best`, `tp_pent`, `tb_lazy`}.
  - Score: fits and smaller, and bytes, with an infeasible choice charged zstd-3's bytes. The baseline is always
    `tp_phuf_pg16`.
  - Inference cost: the measured extraction time is reported against zstd-3's median round-trip time.
- Interpretation: development only. A positive out-of-family result justifies a preregistered fresh-source
  validation; it is not acceptance.
- Budget: workflow timeout 20 minutes, expected about 6 runner-minutes.
