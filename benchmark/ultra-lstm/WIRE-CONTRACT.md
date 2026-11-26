# Proposal: an opt-in wire contract for model 4 with an LSTM input (claim 68)

**Status: proposal only. Nothing in this file is implemented in any Kolmo reader or writer.** The research container `KLS1`
written by `lstm_h.c` is not a Kolmo frame. No production code path emits it or accepts it. Read this file together with
[RESULTS.md](RESULTS.md). The candidate missed the acceptance gates, so this contract is **not** proposed for adoption.
It records what adoption would require, so that a later attempt starts from a written contract and not from a guess.

## Identifier

- The codec 7 extended header (`f = 8`) gets **model nibble 10**, the next free nibble under the registry in SPEC.md
  (*Model nibble registry*). Every reader from rc3 on refuses nibble 10 with `KOLMO_ERR_CORRUPT`, so frames written
  before this change keep their meaning, and old readers fail cleanly on new frames.
- A frame holding a nibble-10 stream needs a new frame version (13), because version 12 readers must not
  partially accept it. The writer emits nibble 10 only under an explicit opt-in, for example
  `-U --codec-level 3`, never by default and never from `-U` alone.
- No new block codec identifier is needed. Claim 66's codec id 40 (a GPU neural text codec) is a separate proposal,
  and this file does not cover it.

## Stream layout (nibble 10)

```
size u32 (flags 8 in the top nibble)
10 << 4 | (s - 2)            model 4's table-size shift, as nibble 9
b0 b1 b2                     model 4's learning settings, as nibble 9
mask u32 LE                  model 4's context mask, as nibble 9
H u8                         LSTM cells, 1..255 (the measured candidate: 128)
T u8                         BPTT horizon, 1..64 (candidate: 30)
lr u16 LE, lro u16 LE        Adam and output-layer learning rates, in units of 1e-4 (candidate: 50, 1000)
sel u8                       final-mixer weight-set selector (candidate 0: LSTM confidence octile × partial byte)
lrm u8                       final-mixer learning rate in the low nibble (candidate 4); high nibble 1 adds model 4's
                             five internal mixer outputs as inputs (tried on dev, worse, not part of the candidate)
coded bytes                  the codec 7 binary arithmetic coder, unchanged
```

The harness container `KLS1` carries the same fields in the same order behind a 4-byte magic and a plain u32 size.

## Deterministic reference decoding

The decoder must reproduce the encoder's probabilities exactly on every ISA. The candidate is designed for that:

1. **Model 4 part.** This is the existing integer model 4 (nibble 9), byte for byte. `cm_predict(m, 4)` gives `pc`.
2. **LSTM part, IEEE 754 binary32, round-to-nearest-even, no fused multiply-add, no flush-to-zero.** The order of every sum
   is fixed: the bias first, then index ascending (`lstm_forward`, `lstm_bptt`, `lstm_byte` in `lstm_h.c`). Builds
   must use `-ffp-contract=off` and must not use `-ffast-math`. Vectorized kernels may only use lane-parallel
   operations that preserve this order. A horizontal reduction tree is not allowed unless the contract is changed to
   name that exact tree.
   - `kexp(x)`: clamp to [−87, 87], `k = round_half_away(x·1.44269504)`, two-step Cody–Waite reduction with constants
     0.693145752 and 1.42860677e-6, the degree-6 Horner polynomial in `lstm_h.c`, then multiplication by `2^k` assembled from
     the exponent bits. No libm call is made.
   - `σ(x) = 1 / (1 + kexp(−x))` and `tanh(x) = 2σ(2x) − 1`. `sqrtf` and division are the IEEE correctly rounded operations.
   - Initial weights come from xorshift32 with seed 0x9E3779B9, scaled by `1/sqrtf(H+1)`. The forget-gate bias starts
     at 1, and the output layer starts at 0.
   - The bit probability is `pl = clamp(int(tree[2·node+1] / tree[node] · 4096), 1, 4095)`, where the float-to-int
     conversion truncates toward zero.
3. **Final mixer (integer).** Its inputs are `st(pc)`, `st(pl)` and 256 (model 4's exact stretch table). Weights are
   int32, starting at (65536, 0, 0). The dot product is computed in int64 and shifted right by 16, clamped to ±2047,
   then squashed and clamped to [1, 4095]. The update is `w += (x · err) >> 14` with
   `err = ((y << 12) − p) · lrm` (arithmetic shift).
4. **Reference decoder.** A slow, portable C reference built with `-O0 -ffp-contract=off` would serve as the conformance
   oracle on arm64 and x86-64. Golden frames would sit next to the v12 corpus. This follows R16's F8 recommendation that
   model 4 itself be written out first (FORMAT.md §24), because the LSTM stream inherits every model-4 bit.

## Resource declaration

The decoder working set is model 4's working set (`cm_mem4 + cm4_chain`) plus the LSTM. The LSTM has
`3·256·H + 3·H·(H+1) + 256·(H+1)` parameters, each stored as a weight, two Adam moments and a gradient (16 B per
parameter), plus `9·T·H` floats of window history. For the candidate (H = 128, T = 30) that is 180,864 parameters:
2.89 MB, plus 0.14 MB of history, on top of model 4's tables (for H = 32, T = 10: 0.59 MB). The frame's declared decode need (`blk_dec_need`) would include this, so `--memlimit` stays honest.

## What this contract does not settle

- **Speed.** The decoder runs at the measured candidate speed. RESULTS.md shows that this misses the archival gate by
  a wide margin.
- **Cross-ISA bit-exactness.** It is argued above but was not tested. The harness was only run on Apple M5 (arm64).
- **Larger LSTMs.** H = 64 or 128 changes nothing in the layout, but it multiplies the cost.
