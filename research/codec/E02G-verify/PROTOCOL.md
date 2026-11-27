# E02G-verify — decode-only, noise-controlled timing of the E02G frames

Registered 2026-11-12, before any timing in this protocol was taken. A **verification** of E02G's two decode-ratio
misses (Backblaze fast 1.075×, OWID hc 1.18×). It **cannot change E02G's registered verdict**; reversing that would need
a fresh validation. Branch and PR rules as before; production `2f3308a` and the frozen harness are unchanged; the timing
tool (`dtime.c`) includes the frozen harness unmodified.

## What is measured

For every E02G input and both modes, the tool rebuilds the frames exactly as E02G did: production `kolmo_compress`
(one thread), then the residual leaves for the control (`exist+prod3` fast / `exist+prod19` hc) and the candidate
(`planes+prod3g` fast / `planes+prod19` hc). It then times **decoding only**, with no encoder in the process loop:

- `P`: production frame, production decoder (`decode_block` path)
- `C`: control frame; `C2`: a second, byte-identical copy of the control in separate memory (**identical-frame noise
  control**)
- `K`: candidate frame

Each timed sample is a sequential single-thread decode of every block, including the production KG_INT inverse and the
block checksum. The output is compared byte for byte with the input **outside** the timed interval.

**Regimes.** *Warm*: one untimed decode, then a timed batch of `B = max(1, ceil(8 MiB / input bytes))` decodes; the
per-decode time is reported. *Displaced*: a 32 MiB buffer is written outside the timed interval before each single
timed decode. This is not guaranteed cold LLC or cold storage.

**Order.** 15 rounds per input and mode. The four policies rotate (round r starts at policy r mod 4) and are then
shuffled with seed 20261118 + r. Wall and CPU time are recorded for every sample; all samples are kept.

## Estimators and outcome rules (fixed now)

For each input, regime and policy: the median over rounds. Family ratio = Σ medians(K) / Σ medians(C), over the same
eligible files ≥ 256 KiB that E02G's bar used. Noise band = the 5th–95th percentile of the per-round family ratio
C2 / C, over all rounds.

For each of the two E02G misses, separately per regime:

| outcome | rule |
|---|---|
| **Reproduced** | family ratio K/C > 1.05 and above the noise band's 95th percentile |
| **Within noise** | family ratio K/C inside the noise band |
| **Not reproduced** | family ratio K/C ≤ 1.05 and above the band (a real but small cost) |

The other two target cells and the controls are reported the same way, descriptively. Results are reported per file,
per family and per regime with absolute microseconds. They are never pooled across families or with other
experiments' timings.
