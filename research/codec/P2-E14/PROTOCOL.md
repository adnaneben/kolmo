# P2-E14 protocol (frozen before measurement): production-format type-first frames on the P2-E10 cohort

Question: do the P2-E12 results (type-first frames written in today's `CZN1` format and decoded by the unmodified
production `kolmo -d`) hold on P2-E10's 61-input, 11-family development cohort, and does sharing one typed forward
pass while trying both leaves (`tb_best`) or building the second leaf lazily (`tb_lazy`, P2-E13) fit zstd-3's
complete-CLI cost once process start-up is included? P2-E13 found both cost-negative in memory.

- Inputs: rebuilt on the runner by the registered E02E/E02F/E02G fetch/prepare scripts plus `P2-E10/prepare_e10.py`.
  These inputs are unchanged and are observed development data.
- Harness: N1 `baselines.py` unchanged (timing, affinity, rlimits, byte-exact checks, determinism, cost formulas),
  through `run_e14.py`. There are two runs, with seeds 20261112 and 20261113, and an N1 `confirm` step.
- Policies (frozen): `zstd_3` (speed rival), `openzl_<profile>_9` (archive rival, profile per input as P2-E10),
  production `kolmo_fast` (control), and research encoders `p2e13 enc {tp_phuf, tp_phuf_pg16, tp_prod3, tb_best,
  tb_lazy}`. All research frames are decoded by the production `kolmo -d -T1`.
- Sanitizer gate before timing: the ASan/UBSan/LSan `p2e13` encoder, a production CLI decode and `cmp`, for every
  input and research policy.
- Report:
  - Speed: fits zstd-3 on every pair, and fits and smaller, per family and in total, for each run.
  - Archive (informative): the smallest pipeline inside OpenZL-9's archive cost.
  - No pooled claims. No acceptance: this is development data.
- Budget: workflow timeout 60 minutes. Expected about 25 runner-minutes.
