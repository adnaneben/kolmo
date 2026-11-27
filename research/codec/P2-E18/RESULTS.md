# P2-E18: why OpenZL keeps GHCN daily weather (local analysis on observed inputs; no runner measurement)

Setup:
- Export: Actions **43346396613** (~1 runner-minute) printed the nine observed `E02E__ghcn_*` NPY inputs. Their
  SHA-256 hashes (`results/ghcn.sha256`) equal P2-E14's prepared manifest. The data itself is not committed; the
  workflow regenerates it from NOAA.
- Rival build: the pinned OpenZL v0.3.0 (`32246b48…`) and zstd were built locally with N1's flags.
- Timings are local and indicative only (`results/local_cost.txt`, 5 repetitions, median CLI wall time).

## What the data is

Nine int32 series of 30,000–57,000 daily values:
- TMAX and TMIN have 50–133 distinct values each.
- PRCP is 65–90% zeros.

Order-0 entropy of the values is about the same as that of the deltas. The conditional entropy H(x | previous x) is
15–20% lower: the series is strongly first-order Markov in value space. That structure is what OpenZL exploits and
Kolmo's fast leaves do not.

## Production-decodable alternatives (`gf.c`: KG_INT order 0, bias = min, 16-bit output, existing wire semantics)

| Leaf on the 16-bit offset values | Bytes vs OpenZL-9 (temperatures) | Local archive cost vs OpenZL-9 |
|---|---|---|
| `C_CM` (production context mixing), interleaved | **−4% to −15%** on 5/6 (e.g. 14739 TMAX 22,293 vs 26,169) | **15–25× over** (≈ 150–370 ms per direction) |
| `C_BSC` (BWT), interleaved | −1% to −10% on 4/6 | **+5–16% over** (decode 4–6 ms vs 2.5–3 ms) |
| `C_LZMA`, `C_ZSTD` 19 | larger than OpenZL | — |
| production `kolmo fast` | +2% to +18% | about equal (inconsistent locally) |

All frames written by `gf.c` decode with the unmodified production `kolmo -d` (16/16 checked).

## Findings

1. **The GHCN gap is modelling, not framing.** A context model in today's format (`C_CM` on order-0 offset values)
   beats OpenZL's bytes on most temperature series. Its CPU, however, is far outside P2's archive cost at these file
   sizes (120–230 KB), where storage is only about 75% of the rival's modeled cost.
2. **BWT (`C_BSC`) is the nearest existing leaf.** Its bytes are smaller on temperatures, but decode is about 1.5–3
   ms slower per file, which puts it 5–16% over cost. This was local only; a runner measurement was not done.
3. Closing GHCN within cost needs a **fast order-1 entropy leaf**: rANS or tANS with the previous value bucket as
   context, decoding at Huffman-like speed. That is **new wire semantics** (a new codec ID). Per AGENTS.md it needs
   separate review before any implementation, so it is recorded as a proposal (PROVISIONAL P2-19), not built.

This is development data only, with no acceptance and no format change.
