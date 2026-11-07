# Changelog

## Unreleased (branch `claude/perf`)

### Ratio
- **`ultra`: context-mixing model 2.** Two sparse contexts, three record contexts (a detected fixed record length:
  the bytes one and two records back, and the column within the record), four mixers combined by a learned final
  mixer whose learning rate decays over the block, and an order-2 APM. Silesia under lzbench: 17.91% → **17.01%**
  (5.58× → 5.88×); zpaq -5 is 19.09%. Files written by 1.0.0 (model 1) still decode.
- **`ultra -l 1`**: model 1 only, without the LZMA2/BWT alternatives: 17.99% at 0.43 / 0.45 MB/s on Silesia —
  ahead of zpaq -5 (19.09%, 0.39 / 0.39 MB/s) on ratio and on both speeds.
- **`max`**: binary blocks keep the better of LZMA2 and BWT (Silesia 19.68% → 19.34%).
- **`max -l 1`**: BWT for every block — 20.93% at 21.6 MB/s on Silesia, ahead of bsc, kanzi -7 and bzip3 -5 on
  ratio, compression and decompression speed.
- **JSON**: JSON lines and pretty-printed JSON are detected; keys and punctuation form templates and every value
  becomes a typed column (OWID data as JSON lines: −51% at `fast`, −30% at `max`). Same block format as the log
  transform, so 1.0.0 decodes these files.

### Speed
- **Quick analysis (`-q`, `transforms = 2`)**: only the cheap structure transforms, scored with zstd -1 on a 256 KiB
  slice. `fast -l1 -q` (30.33% at ~350 MB/s) beats zstd -2; `fast -l2 -q` (28.89% at ~300 MB/s) and `fast -q`
  (27.87% at ~220 MB/s) beat zstd -3.
- **Context-mixing tables on transparent huge pages** (Linux): ultra back to 1.0.0's speed with model 2.
- Log transform: one-pass tokenizer for short tokens.

### Format
- CM streams flag model 2 in the top nibble of the size word (`f & 8`); 1.0.0 decoders reject them as corrupt input.
  Every other output added here (JSON columns, `max` choices, quick analysis, `ultra -l 1`) is readable by 1.0.0.
- `max` level 1 now means "BWT for every block" (it was LZMA2 preset 1); `ultra` level 1 selects model 1.

### Memory
- `ultra` (model 2) needs about 4.5 GB at the largest block size (1.0.0: 3.8 GB); `ultra -l 1` keeps 1.0.0's 3.8 GB.

### Benchmarks
- `benchmark/lzbench`: kolmo enters lzbench as levels 0–12, each an ordinary CLI setting; results in
  `benchmark/lzbench/results/`.
