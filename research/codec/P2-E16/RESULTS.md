# P2-E16 results: out-of-family selectors over encode-time features (negative, development)

Run: Actions **43340533639** at `d75c5a4`, 00:52–00:55 UTC (~3 runner-minutes). `p2e13 feat` was run on the 61
P2-E10 cohort inputs. Their hashes are **identical** to P2-E14's prepared manifest, so the features and P2-E14's
outcomes refer to the same bytes. There are 183 feature lines (3 repetitions), and 6/6 printed files match
`MANIFEST.json`.

Analysis: `ranker.py` writes `results/ranker.json`. It uses leave-one-family-out over 11 families. The choice set is
{`tp_phuf_pg16`, `tb_best`, `tp_pent`, `tb_lazy`}. An infeasible choice is charged zstd-3's bytes.

| Selector (first / confirmation) | Fits and smaller, of 61 | Bytes vs zstd-3 |
|---|---|---|
| always `tp_phuf_pg16` (baseline) | **49 / 47** | −11.9% / −12.5% |
| depth-1 rule, out of family | 44 / 43 | −11.0% / −14.4% |
| depth-2 rule, out of family | 38 / 38 | −10.5% / −12.6% |
| 1-nearest-neighbour, out of family | 38 / 31 | −10.6% / −9.0% |

The in-sample depth-2 rules split on the minimum plane entropy and on the residual ratio or zero fraction. They differ
between the two runs.

## Findings

1. **No learned selector beats the fixed guarded rule out of family in both runs.** Gains in one run reverse in
   the other: many second-leaf choices sit at the cost boundary (the timing noise is the same as in P2-E10 for GHCN
   and FSDD).
2. **The features are not free.** The extra entropy pass costs a median of 1.2 ms in process, about 10% of zstd-3's
   median complete-CLI round trip (12 ms). Any selector must pay this, which is part of why P2-E13's `tb_pick` failed.
3. Conclusion for now: under the zstd-3 speed rival, ship-shaped research should use the fixed guarded per-plane
   Huffman frame (`tp_phuf_pg16`). Second leaves belong to archive mode, where `tb_best`, `tp_pent` and `tp_arch19`
   win against OpenZL (P2-E14). A learned ranker would need more families and a cheaper feature set before it is
   worth a preregistered validation.

This is development data only, with no acceptance and no format change.
