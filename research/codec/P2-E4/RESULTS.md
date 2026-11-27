# P2-E4 — zero-run residual leaf (N3 sparse/run layer), development

`p2e4.c` is a copy of P2-E2's harness with one added research leaf (`zrl.inc`, framing `0xB0`): the type-first
residual becomes zero-run lengths (LEB128 bytes) plus the nonzero values as byte planes, each Huffman-coded or stored;
the decoder rebuilds the production-layout residual and runs `fi_inverse`. `tf_zrl_auto` uses it when at least 25%
of residual values are zero (one scan, no probe), else per-plane Huffman. The 25% cut is a development choice.

Local 2-vCPU, in memory, 20 development inputs, 5 paired rounds, vs zstd-3 at 1 Gbit/s (`results/summary.txt`, raw
records `results/records.tar.xz`). Every pipeline reconstructs every input; ASan/UBSan/LSan clean on all 20 inputs.

| Pipeline | Fits | Fits and smaller | Bytes where it matters |
|---|---|---|---|
| `tf_planes_huf` | 14 | 8 | mri −15.9%, fs_inode −48.3%, fs_mtime +340% |
| `tf_zrl` (unfused) | 4 | 4 | mri −28.9%, fs_inode −75.2%, fs_mtime +84% |
| `tf_zrl_auto` (unfused) | 12 | 8 | as zrl on sparse residuals, as planes elsewhere |
| `tf_zrlf` (fused encoder and run-aware inverse) | 4 | 4 | same bytes |
| `tf_zrlf_auto` (fused) | 12 | 8 | same bytes; fs_inode feasible at −75.2% (cost −55%) |

Second run (this table): the fused variants write runs/nonzero planes in one pass and invert straight from runs
(zero runs become value repetition / linear extrapolation), checked byte-exact end to end and under sanitizers.

**Verdict: byte-positive; cost-neutral overall with the fused decoder (PROVISIONAL, reopenable).** Fusing cut mri
from +70% to +45% of zstd-3's cost, still over budget there (its cost is mostly encode-side analysis), and made
fs_inode feasible at −75.2% bytes; the auto rule's extra scan costs a few small audio inputs their feasibility.
Original note on the unfused version: The representation recovers much of
zstd's advantage on sparse residuals without an LZ stage, but this first decoder materialises the residual and parses
runs value by value, so on mri the auto variant costs +84% vs zstd-3. A fused run-aware inverse (write output values
directly while walking runs) is the obvious next step; until then it is not a P2 candidate. fs_mtime remains a loss
(+84%): its advantage for zstd is repeated non-adjacent values, which no zero-run layer captures.
