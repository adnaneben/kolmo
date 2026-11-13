# P2-E17 protocol (frozen before measurement): archive-mode frontier in a single run

Question: P2-E10 measured production `kolmo -m max` with research containers, and P2-E14 measured production-format
type-first frames without `max`. Within ONE run, on the same 61 development inputs, which complete pipeline is
smallest inside OpenZL's typed level-9 archive cost on every pair? Every candidate here is decodable by the
unmodified production CLI.

- Inputs: rebuilt as in P2-E10 and P2-E14 (the hashes must equal P2-E14's prepared manifest).
- Harness: N1 `baselines.py` unchanged, via `run_e17.py`. Two runs (seeds 20261112 and 20261113) and `confirm`.
- Policies (frozen):
  - `openzl_<profile>_9` (rival);
  - `zstd_3` (reference);
  - production `kolmo` fast, hc and `-m max`;
  - research `p2e13 enc {tb_best, tp_pent, tp_arch19}`, decoded by the production `kolmo -d -T1`.
- Gate: ASan/UBSan/LSan encoder, production CLI decode and `cmp` for every input and research policy.
- Report:
  - per family and in total: the smallest feasible pipeline (all candidates);
  - the same with production-only candidates;
  - the byte-weighted frontier vs OpenZL-9 (informative).

  This is development data, with no acceptance.
- Budget: workflow timeout 75 minutes. Expected about 30–45 runner-minutes, because `max` is slow.
