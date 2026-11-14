# P2-E15: realizable speed-mode rules on the recorded P2-E14 runs (analysis only, no new measurement)

`selector.py` reads P2-E14's committed records and writes `selector.json`. A rule picks ONE production-frame pipeline
per input using only information that is free at encode time. If the chosen pipeline does not fit zstd-3's cost on
every pair, the input is charged zstd-3's bytes, since a deployed rule would have to fall back.

| Rule (first run / confirmation) | Fits and smaller, of 61 | Bytes vs zstd-3 |
|---|---|---|
| per-input oracle (not realizable) | 58 / 57 | −14.8% / −15.0% |
| always `tp_phuf_pg16` | **49 / 47** | −11.9% / −12.5% |
| always `tp_phuf` | 45 / 46 | −12.5% / −12.6% |
| always `tb_lazy` | 44 / 46 | −11.9% / −12.6% |
| always `tb_best` | 35 / 32 | −7.5% / −6.7% |
| best size-threshold switch, chosen leave-one-family-out | ≤ 49 / ≤ 50 | ≈ −11.9% / −12.6% |

Findings:
- A fixed guarded per-plane Huffman rule already captures about 80% of the oracle byte gain (−12% of −15%).
- Using file size as the only extra feature does not close the gap out of family.
- The remaining ~3 points need a per-input signal that predicts when the zstd-3 or rANS leaves are worth their extra
  cost: the second-leaf choice of `tb_best`/`tp_pent`. P2-E13 showed that probing for it costs as much as it saves.
  It would need a near-free learned ranker over features the forward pass already has (order, gcd, output width,
  plane entropies), trained and validated **out of family** with inference charged.

These are development inputs only. Fitting a rule on them and reporting it as general would be tuning on observed
data, so this is not done here.
