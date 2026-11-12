# P2-E3 — type-first research pipelines on N1's complete-CLI boundary (development)

Status: **development** (observed inputs). Registered before any P2-E3 measurement; frozen with this commit.

## Question

On the matched complete-CLI boundary N1 built (pinned OpenZL v0.3.0 `32246b48…`, zstd 1.5.7 CLI from the same
dependency, repaired Kolmo CLI, one assigned CPU, startup and I/O included, independent randomized encode and decode
rounds, child CPU/max-RSS, byte-exact verification), where do the P2-E2 type-first pipelines stand against the
designated rivals — zstd-3 for speed modes, OpenZL for archive modes — and against Kolmo's own CLI settings?

## Method

- `run_e3.py` imports `N1-baselines/baselines.py` unchanged and replaces only `policies()`/`command()` to add seven
  research policies via `research/codec/P2-E2/tfcli.c`:
  speed family `tf_planes_huf`, `tf_huf`, `tf_planes_huf_pg16`, `tf_prod3`; archive family `tf_prod19`,
  `tf_planes_prod19`, `tf_planes_ent19`. N1's own eight policies are re-measured in the same run.
- Inputs: N1's prepared observed R3 inputs (8 NPY uint32 arrays and 2 derived CSV views; correlated, report separately).
  `tfcli` types the NPY arrays from their own header; CSV views are opaque to it (whole-input zstd at the level).
- Build: N1's flags (`-O3 -DNDEBUG -fno-plt -ffp-contract=off`) for every tool, including `tfcli`.
- Gate before timing: ASan/UBSan/LSan round trip of every research policy on every prepared input.
- Two runs (seeds 20261112 and 20261113) and N1's `confirm` (identical sizes and frame hashes).
- Cost formulas: N1's `cost()` (P2 defaults: speed = encode + decode + bytes / 125 MB/s; archive = CPU at
  $(3.89/72)/h with 3.3 reads + $0.023/GB-month for 30 days).

## Interpretation limits

Observed development data; no P2 acceptance. Research container (`KTF1`), not a production frame. CLI timings are not
poolable with P2-E1/E2's in-memory timings. A research policy that beats a rival here motivates a frozen,
source-disjoint validation; it is not that validation.
