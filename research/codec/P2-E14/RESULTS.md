# P2-E14 results: production-format type-first frames on the 61-input P2-E10 cohort, production CLI decode

Run: Actions **43338093009** at `e7d4449`, 00:34–00:48 UTC (~14 runner-minutes).

Setup:
- Inputs: the registered E02E/E02F/E02G sources were rebuilt by their own scripts plus `P2-E10/prepare_e10.py`.
  Download hashes are in `results/DOWNLOAD_*.sha256`. That gives **61 development inputs from 11 families**.
- Encoder and decoder: every research frame is written by `p2e13 enc` as a standard `CZN1` frame and **decoded by
  the unmodified production `kolmo -d -T1`**.

Evidence:
- Recovered from the job log: 17/17 printed files match `MANIFEST.json`.
- Sanitizer gate (ASan/UBSan/LSan encoder, then a production CLI decode and `cmp`): **427/427**.
- N1 `confirm`: **IDENTITIES_MATCH, 610/610 configurations, 0 failures**.

## Speed mode: rival is the zstd-3 CLI; a pipeline must fit on every pair (first run / confirmation; of 61)

| Pipeline (all production frames) | Fits and smaller | Fits |
|---|---|---|
| `tp_phuf_pg16`: guarded per-plane Huffman | **49 / 47** | 51 / 49 |
| `tp_phuf`: per-plane Huffman | 45 / 46 | 46 / 47 |
| `tb_lazy`: Huffman, plus the zstd-3 leaves only if Huffman saves < 30% | 44 / 46 | 47 / 48 |
| `tp_prod3`: production zstd-3 leaves via the fast path | 40 / 35 | 40 / 35 |
| `tb_best`: one typed pass, both leaves, keep the smaller | 35 / 32 | 37 / 34 |
| `tp_pent`: per-plane rANS or Huffman, the smaller | 33 / 25 | 33 / 25 |
| `tp_arch19` (archive pipeline) | 2 / 1 | 2 / 1 |
| production `kolmo fast` | **0 / 0** | 0 / 0 |

**Smallest feasible speed pipeline per input** (an oracle choice per input, not a realizable rule):

| Winner | Inputs (first / confirmation) |
|---|---|
| `tb_best` | 27 / 22 |
| `tp_pent` | 22 / 18 |
| `tb_lazy` | 4 / 10 |
| other research frames | 5 / 7 |
| zstd-3 itself | 3 / 4 |

Byte-weighted, that oracle frontier is **−14.8% / −15.0%** below zstd-3. Median per family (first run):

| Family | Median vs zstd-3 |
|---|---|
| MIT-BIH | −35.7% |
| GHCN | −33.6% |
| EEG | −28.2% |
| pageviews | −26.3% |
| dense control | −21.2% |
| speech | −18.2% |
| FSDD whole WAVs | −17.1% |
| Backblaze | −8.4% |
| OWID | −6.0% |
| single-byte control | −2.9% |
| whole Backblaze CSV (fallback frame) | −1.4% |

**Boundary matters.** In memory, P2-E13 found trial-coding both leaves cost-negative against zstd-3 (1/20 fit). On
the complete-CLI boundary, process start-up and I/O are paid by every pipeline, so `tb_best` and `tp_pent` fit on
about half to 60% of inputs and are the most frequent per-input winners. Claims must stay boundary-specific.

## Archive mode: rival is OpenZL's typed `le-*` profile at level 9 on the whole file

Smallest pipeline inside OpenZL-9's archive cost on every pair (first run / confirmation):

| Winner | Inputs |
|---|---|
| `tb_best` | 15 / 15 |
| `tp_pent` | 13 / 14 |
| `tp_arch19` | 9 / 8 |
| production `kolmo fast` | 7 / 7 |
| `tp_prod3` | 1 / 1 |
| **OpenZL itself** | **16 / 16** (GHCN 7/9, Backblaze 2/6, OWID 2/6, pageviews 2/8, dense control 2/4, speech 1/4) |

Production `kolmo -m max`, `hc` and the research containers were not in this run. P2-E10 measured them on the same
cohort.

OpenZL caveat (as P2-E10): the whole-file `le-*` profile reads the NPY/WAV header bytes as values. The N1 exact-NPY
comparator (run 43334484927, recorded by its owner) is the header-exact control.

## Interpretation (development, observed sources; no acceptance, no format change)

1. The whole type-first stack now exists as **ordinary production frames**, and the unchanged decoder reads them:
   - per-plane Huffman;
   - per-plane rANS with R1's singleton normalisation in the encoder only;
   - production zstd leaves.

   On 61 inputs, a single realizable rule (`tp_phuf_pg16`) is smaller than zstd-3 within its complete-CLI cost on
   49/47. Production Kolmo fits on 0.
2. `tp_pent`'s singleton `C_RANS` chunks depend on the decoder accepting a frequency of `RANS_M`. That is existing
   decoder behaviour (R1 evidence, 360/360 local stress), but it still needs the separate wire review that AGENTS.md
   requires before any production encoder emits it.
3. The next step is a realizable selector that approaches the oracle frontier (−15% byte-weighted) without trial
   coding. Candidates are a declaration/family rule or a near-free supervised ranker, with its inference charged.
   It needs a preregistered, source-disjoint validation.
