# Production change proposals (NOT applied; each needs explicit authorization of a `src/` and `tests/` change)

## 1. `decoder-fast-int-inverse.patch`: faster KG_INT inverse in the decoder (same wire format)

**What it changes.** `kg_decode` calls P2-E2's `fi_inverse` instead of `kg_int_inv`. `fi_inverse` is specialised by
element width, output width, planes and order, and falls back to `kg_int_inv` for anything else. The patch applies
cleanly to the repaired core (`git apply --check`); it is **not applied**.

**Evidence.**

| Check | Result |
|---|---|
| Output identity vs `kg_int_inv` (P2-E2) | identical on 19/19 inputs |
| Differential fuzzing (P2-E2) | 3.3 M inverse cases |
| Decode tests (P2-E28) | 540/540 research frames plus production `fast` frames |
| In-process ASan/UBSan/LSan (P2-E28) | 240/240 |
| Inverse speed | 3.0× faster on audio, 5.2× on numeric arrays (local) |
| Audio speed mode vs zstd-3 (P2-E28, development) | 24 → 35 of 40 clips fit and are smaller |
| Fresh audio test | **P2-E29 (MS-SNSD) FAIL**: 22/26 of 40; misses there come from guard and encoder throughput, not the decoder |

**Required with it:**
- conformance vectors in `tests/` covering every specialised path and the fallback;
- the full production correctness gate: 36 planner cases, 45 frame pairs, 90 cross-decodes, sanitizers.

## 2. CSV regularity picker (P2-E25–E27), not yet written as a patch

**What it would do.** For the eco/hc CSV path: if the first 64 KiB have line regularity > 0.9916, encode with the
existing table program and zstd-12 leaves; otherwise use the current `eco` path.

**Evidence.** Fresh, preregistered, Bonferroni-adjusted: 33/35 (P2-E26) and 26/28 (P2-E27, second aggregator)
inside OpenZL `csv`-9's archive cost.

**What it would affect.** Mode semantics change, though the wire format does not. It needs a decision on which mode
(`eco` or `balanced`) adopts it, plus tests.
