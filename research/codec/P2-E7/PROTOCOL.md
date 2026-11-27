# P2-E7 — type-first portfolio on N1's complete-CLI boundary (development)

Registered before any P2-E7 measurement. Same boundary, pinned rivals (OpenZL v0.3.0 `32246b48…`, zstd 1.5.7 CLI),
Kolmo control build, flags, prepared N1 inputs (8 NPY uint32 arrays + 2 derived CSV views; observed, correlated) and
harness as N1/P2-E3: `run_e7.py` imports `N1-baselines/baselines.py` unchanged and replaces only `policies()` /
`command()`.

Research policies (frozen):
- NPY speed: `tf_planes_huf`, `tf_planes_huf_pg16` (`research/codec/P2-E2/tfcli.c`; re-measured for a joint view).
- CSV speed: `mcsv_planes_huf`, `mcsv_prod3`; CSV archive: `mcsv_prod19` (`research/codec/P2-E6/mcli.c`, file magic
  `KMC1`, the exact P2-E6 research container; typing contract in `P2-E6/RESULTS.md`). On NPY inputs `mcli` is opaque
  (whole-input zstd at the level) and `tfcli` is opaque on CSV; those cross cells are controls, not candidates.

Rivals: zstd-3 CLI (speed); OpenZL `csv` profile level 9 (archive, CSV views). OpenZL NPY remains BLOCKED (no
`numpy` profile; header-preserving `le-u32` adapter claimed by the N1 workstream).

Gate: ASan/UBSan/LSan round trip of all five research policies on every prepared input before timing. Two seeded
runs + N1 `confirm`. Cost formulas: N1's `cost()` (P2 defaults). Observed development data: no P2 acceptance.
