# Changelog

## 1.1.0 (2026-11-10)

### Ratio
- **`ultra`: context-mixing model 2.** Two sparse contexts, three record contexts (a detected fixed record length:
  the bytes one and two records back, and the column within the record), four mixers combined by a learned final
  mixer whose learning rate decays over the block, and an order-2 APM. Silesia under lzbench: 17.91% → **17.01%**
  (5.58× → 5.88×); zpaq -5 is 19.09%. enwik8: 19.57 MB → 18.96 MB (5.27×). Files written by 1.0.0 (model 1)
  still decode.
- **`ultra -l 1`**: model 1 only, without the LZMA2/BWT alternatives: 17.99% at 0.43 / 0.45 MB/s on Silesia —
  ahead of zpaq -5 (19.09%, 0.39 / 0.39 MB/s) on ratio and on both speeds.
- **`max`**: binary blocks keep the better of LZMA2 and BWT (Silesia 19.68% → 19.34%).
- **`max -l 1`**: BWT for every block — 20.93% at 21.6 MB/s on Silesia, ahead of bsc, kanzi -7 and bzip3 -5 on
  ratio, compression and decompression speed.
- **JSON**: JSON lines and pretty-printed JSON are detected; keys and punctuation form templates and every value
  becomes a typed column (OWID data as JSON lines: −51% at `fast`, −30% at `max`). Same block format as the log
  transform, so 1.0.0 decodes these files. **Minified JSON** (one line of records) is split into records first
  (block flag 10, container version 6): OWID records as one JSON array, −51% at `fast`, −37% at `max`.
- **Integer arrays (kolmo graph INT node, branch `claude/graph`)**: 16/32/64-bit arrays, with or without a `.npy`
  header, are coded as residuals: per block the order (value, delta, delta-of-delta), signedness, common divisor and
  output width are chosen, and the residuals are stored as byte planes. Millisecond timestamps (int64): 10.6× →
  24.5× at `fast` (9.3 MB/s compress+decompress) and 27.1× at `max`; OpenZL 0.3.0 reaches 20.6× at 5.2 MB/s.
  Population counts (int64): 3.80× at `fast` (12.8 MB/s) vs OpenZL's 3.27× (10.8 MB/s). Other inputs are unchanged
  (51 reference outputs byte-identical).

- **Float arrays (graph FLOAT node + Huffman-only leaves)**: float32 words are rotated so the exponent fills one byte,
  and each byte plane becomes its own leaf, coded by Huffman alone when that is smallest (the near-random mantissa
  planes are stored). Model weights (safetensors, ONNX, GGUF): 1.17× → 1.20× at `fast`, level with OpenZL 0.3.0.
- **Merge of Codex's `codex/lossless-frontier`**: blockwise file I/O, lower fast-file memory, legacy-compatible
  wide-table lanes and the opt-in frontier build. Codex's frontier codecs keep ids 8–11; the graph codec is 12.

- **8-bit images (graph IMG8 node)**: LOCO-I median prediction per channel, row length from a `.npy` header or detected
  among common widths × 1/3/4 channels. img_camera 1.62× → 1.82× at `fast`, 1.97× at `max` (OpenZL 0.3.0: 1.75×);
  astronaut 1.63× → 1.84× at `fast`.
- **16-bit floats (bf16/fp16)** in the FLOAT node: a BF16 LLM shard 1.28× → 1.50× at `fast` (its entropy bound).
- **rANS leaf codec (14)** at hc/max/ultra: model weights 1.2007× → 1.2036× (OpenZL's best ratio).
- **Wide tables at hc/max/ultra**: the graph is kept only when it also wins with the setting's strong codec on a slice
  from the middle of the file. Fixes a regression against 1.0.0 on the UCI electricity table at `max` (8.37× → 11.26×,
  equal to 1.0.0).

### Speed
- **Fast tier, per TRADEOFFS.md (unchanged output may not be more than 5% slower than 1.0.1):** graph trials at `fast`
  use 256 KiB and are compared by their gain over plain coding of the same slice; image detection is a lag scan over
  4096 sampled bytes; float detection samples 8192 words; graph leaves are coded from three samples (stored, Huffman
  only or zstd). Unchanged binary files went from up to 40% slower than 1.0.1 to level or faster; model weights are
  now both smaller (1.169× → 1.201×) and faster (+12% combined) than 1.0.1 at `fast`.
- **CLI:** writing over an existing file replaces it atomically (rename) instead of copying the staged output over it
  — decompression onto an existing file is about 30% faster; symlinks and hard-linked files keep the copy.
- Graph decoder: table numbers print straight into rows (no column text), leaf parts are views instead of copies,
  two-digit number printing; minified JSON: newline removal by runs. Minified JSON decodes 1.8× faster, wide tables
  1.65× faster (in memory).
- Fast/hc planner: text-transform trials on 256 KiB instead of 1 MiB (same decisions on every file tested), the log
  trial reuses the JSON trial's result; wide-table column analysis at `fast` samples runs of lines (+0.05% size,
  about 1.3× faster to compress). `json_split` copies runs (1.5× faster, byte-identical).
- **Quick analysis (`-q`, `transforms = 2`)**: only the cheap structure transforms, scored with zstd -1 on a 256 KiB
  slice. `fast -l1 -q` (30.33% at ~350 MB/s) beats zstd -2; `fast -l2 -q` (28.89% at ~300 MB/s) and `fast -q`
  (27.87% at ~220 MB/s) beat zstd -3.
- **Context-mixing tables on transparent huge pages** (Linux): ultra back to 1.0.0's speed with model 2.
- `ultra` tries LZMA2/BWT on a 1 MiB slice before coding a large block with them (same output on Silesia, up to 12%
  faster).
- Log transform: one-pass tokenizer for short tokens.

### Verification
- The full test suite (round trips in every mode, range reads, corruption fuzzing, JSON and minified-JSON cases) and
  Codex's resource and parallel suites pass; the full suite is also clean under AddressSanitizer and UBSan.
- Every change that is not meant to alter output was checked against 51 reference outputs (byte-identical).

### Format
- CM streams flag model 2 in the top nibble of the size word (`f & 8`); 1.0.0 decoders reject them as corrupt input.
  Minified-JSON blocks (tflag 10) raise the container to version 6, which 1.0.0 rejects. Every other output added
  here (JSON columns, `max` choices, quick analysis, `ultra -l 1`) is readable by 1.0.0.
- `max` level 1 now means "BWT for every block" (it was LZMA2 preset 1); `ultra` level 1 selects model 1.

### Memory
- `ultra` (model 2) needs about 4.5 GB at the largest block size (1.0.0: 3.8 GB); `ultra -l 1` keeps 1.0.0's 3.8 GB.

### Benchmarks
- `benchmark/lzbench`: kolmo enters lzbench as levels 0–12, each an ordinary CLI setting; results in
  `benchmark/lzbench/results/`.

### Naming
- **The old `cz` / `czn` name is gone from the source.** `src/czn.c` and `src/czn.h` are now `src/kolmo.c` and
  `src/kolmo.h`; the C API is `kolmo_*` (was `czn_*`) and constants are `KOLMO_*` (was `CZN_*`); the static library
  is `build/libkolmo.a`; tests and the benchmark tool are `kolmotest`, `kolmoparallel`, `kolmoresource` and
  `kolmobench`. This breaks C callers of the 1.0.x `czn_*` names; the CLI, the Python package and the Rust crate
  keep their interfaces. The on-disk magic bytes (`CZN1`, `CZNS`) are unchanged, so every existing file still
  decodes and output is byte-identical to before the rename.

## Unreleased (candidate branch `claude/next`)

- **Three modes.** `-m speed`, `-m balanced` (default) and `-m ratio`; `-x` selects the smallest output of all
  (`ultra`). The older names (`rt`, `fast`, `eco`, `hc`, `max`, `ultra`) remain as advanced settings.
- **`max` no longer gives up decode speed for almost nothing.** Block sorting replaces LZMA2 on a binary block only
  when it is at least 3% smaller, and is not tried at all on x86 code or where LZMA2 is above 35% of the input
  (where it lost in every test). One weights file had dropped from 508 to 29 MB/s decode for a 0.2% gain.
- **Cheaper planning.** Byte histograms in the 16-bit image detector use four counters (same result, a third less
  time); integer and image graph trials are screened on a short run first; the strong-codec check for wide tables
  uses 384 KiB.
- `benchmark/accept/`: release targets and the script that checks a results file against them.
- **Quick analysis (`-q`) is quick again.** Record layouts are searched only when the record period is clear, and the
  integer and image graph trials are skipped or shortened unless a `.npy` header names the type. Planning time on a
  browser tarball falls from 331 to about 100 ms; sizes on the test files are unchanged.
- **Faster command line at every size.** The file path with reused block buffers and staged output, which `fast`
  used for inputs of 8 MiB and more, now serves `speed` and `balanced` compression of any size and decompression in
  every mode. Output bytes are unchanged. On the Linux test machine: `speed` encodes 14 to 62% and decodes 33 to 73%
  faster; `balanced` encodes 7 to 17% and decodes 18 to 29% faster on files under 8 MiB.
- **Planning skips two searches that never paid off.** Record layouts are searched only when the record period
  stands at least 4x above the median lag, and the x86 transform is tried only where call/jump opcodes are at least
  1 in 1000 bytes. Outputs are byte-identical across the test corpus; planning time at `balanced` falls from 435 to
  about 150 ms on a browser tarball and by a third on record and database files.
- **Speed mode is speed first.** `-m speed` now uses minimal analysis (`-Q`): only the byte-plane shuffle is tried,
  and text is coded as it is. The table, record and image transforms, which made some files 2 to 4x slower to encode,
  stay in `balanced` and in the advanced `rt` setting. `-Q` also works with `fast`.
- **`max` on wide tables encodes ten times faster.** Column leaves take BWT like other text at `max`; LZMA2 on the
  leaves was 3% smaller and ran at about a tenth of the speed. Against 1.0.1 the file is 32% smaller and faster in
  both directions.
- **Speed mode plans in a fraction of a millisecond.** The entropy screen runs before any coder is set up, and a
  shuffle is skipped where LZ4 finds the input incompressible (it would trade most of LZ4's pass-through speed for
  ratio; `balanced` still takes that trade).
- **`hc` keeps its fast reads on model weights.** rANS leaves, which decode about four times slower than Huffman, are
  taken with the zstd family only when at least 3% smaller. Weights grow 0.24 to 0.35% and decode 40 to 80% faster.
- **Small arrays never lose to 1.0.1.** For blocks under 256 KiB the block's own coder on the plain bytes is compared
  with the graph, and the smaller is kept (a 93 KB integer file at `eco` had grown from 660 to 717 bytes).
- **`max` skips hopeless block-sorting attempts on large blocks.** A slice from the middle is coded both ways first;
  outputs are unchanged on the test corpus and a 21 MB source tarball encodes 9% faster.
- **`max` on minified JSON encodes nine times faster.** The record streams go straight to BWT, which had been winning
  over LZMA2 anyway; the LZMA2 pass is no longer run first. Output is unchanged.
- Smaller planning costs at the slow settings: the wide-table trial uses half a MiB from `eco` up, block sorting is
  not attempted on typed table columns at `max`, and the command-line file path applies from 8 MiB as in 1.0.1.
  Outputs are unchanged on the test corpus.
- `max`: JSON token streams (one record per line or minified) go straight to BWT, and block sorting is not attempted
  on log token streams, where it lost by 4 to 12%. JSON lines encode nine times faster at `max`; outputs unchanged.
- The minified-JSON trial runs only when the first records share a few key layouts, so a nested JSON document no
  longer pays for a trial it cannot win.
- Speed mode asks for stronger evidence before trying a shuffle, which cuts its planning on tarballs by two thirds.
- macOS builds link Homebrew's liblzma statically when the archive is present (less start-up time per run).
