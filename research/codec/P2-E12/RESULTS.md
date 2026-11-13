# P2-E12 results: production-format type-first frames on N1's complete-CLI boundary, decoded by the production CLI

Run: Actions **43326268053** at `2be503d`, 23:19–23:30 UTC (~11 runner-minutes). The research encoder `tpcli`
(P2-E11) writes standard `CZN1` frames, and the **unmodified production `kolmo -d -T1` decodes them**. The comparison
uses N1's unchanged harness (`baselines.py`), the same 10 observed development inputs as N1 and P2-E3, and pinned
zstd 1.5.7 and OpenZL v0.3.0.

The evidence was recovered from the job log. All 13 printed files match the run's `MANIFEST.json`; the per-run logs
in the manifest remain only in the expiring Actions artifact.

Correctness and repeatability checks:
- Sanitizer gate: ASan/UBSan/LSan `tpcli` encode, then a production CLI decode and `cmp`. It passed **40/40** (10
  inputs × 4 frames).
- N1 `confirm`: **IDENTITIES_MATCH**, 104/120 configurations complete. The 16 failures are N1's `openzl_numpy_6/9`
  rows: OpenZL v0.3.0 has no numpy profile, the same result as in N1 and P2-E3.

## Speed mode: rival is the zstd-3 CLI; a pipeline must fit on every pair (first run / confirmation)

| Pipeline | Fits and smaller | Fits |
|---|---|---|
| `tp_phuf` (type-first per-plane Huffman, production frame) | **9 / 9** | 9 / 9 |
| `tp_phuf_pg16` (guarded) | 9 / 9 | **10 / 10** |
| `tp_prod3` (production KG_INT program via the fast path, production frame) | 7 / 6 | 8 / 8 |
| `tp_table3` (forced typed table, production frame) | 1 / 1 | 7 / 8 |
| production `kolmo` fast / fast -q | 1 / 1 (Covertype CSV only, as in P2-E7) | 1 / 1 |
| production `kolmo -m hc` | 0 / 0 | 0 / 0 |

Smallest feasible pipeline per input, which is the same in both runs except where noted:

| Input | Winner |
|---|---|
| Covertype elevation | `tp_prod3`, −48.9% |
| Covertype road / fire | `tp_prod3`, −44.9% / −45.1% (confirmation: road `tp_phuf`, −24.0%) |
| Covertype hillshade | `tp_prod3`, −37.1% |
| Covertype cover | zstd-3 itself (the guard routes to whole-input zstd) |
| Covertype CSV | production `kolmo fast`, −72.6% |
| CollegeMsg timestamp | `tp_prod3`, −63.9% |
| CollegeMsg target / source | `tp_phuf`, −5.8% / −0.4% |
| CollegeMsg CSV | `tp_phuf` fallback, −0.5% |

These results confirm P2-E3 and P2-E11 in today's frame format. The fast path writes the production integer program
and stays inside zstd-3's round-trip cost on most typed columns. The production CLI pays its analysis cost and does
not fit. The forced-table frame (`tp_table3`) is smaller than zstd-3 on the CollegeMsg CSV (−34.3%) but is over
cost there. On Covertype it fits, but production `fast` is smaller (570,082 bytes against 635,046).

## Archive mode (informative): rival is the best available OpenZL configuration

The smallest pipeline inside that OpenZL configuration's archive cost is:
- production `hc` on 6/10 inputs;
- production `fast` on 2/10;
- `tp_prod3` on Covertype cover;
- OpenZL on CollegeMsg source.

The OpenZL rival here is `serial` or `csv`; no typed NPY profile was measured. The OpenZL typed and exact-NPY
comparisons are P2-E10 and the N1 exact-NPY work.

## Status

This is development data only. No acceptance, default, format or threshold change follows from it. The frames use
existing wire semantics (`T_KG`, `C_GRAPH`, `C_HUF`) and are decoded by the unmodified decoder, so no new wire IDs
are involved.
