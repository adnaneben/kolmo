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

## Unreleased (on main, not tagged)

- **Budget rule and a typed order in the balanced planner (branch `claude/dec64`).** A change is now judged by
  whether a mode's output gets smaller while its cost stays within its competitor's (zstd -3 for `speed` and
  `balanced`, OpenZL for `hc` and `ratio`): `benchmark/accept/budget.py`, rule and status in
  `benchmark/accept/TARGETS.md`. First step in the planner: at `balanced` the typed trials for binary data (float,
  integer, decimal, 8-bit image) run before the record search, and the search is skipped when a typed plan already
  saves a fifth. Planning time over 79 files falls 29% (24.5% to 20.8% of encode); one of 83 files changes (3.7%
  smaller).
- **`ratio`: float and decimal arrays code their planes with block sorting (branch `claude/dec64`).** The budget rule
  applied to the largest single cost found in `ratio`: LZMA2 at its extreme preset over the byte planes. On float
  arrays (model weights) block sorting gives the same size four to six times faster (a 10 MB safetensors file 5.4 to
  0.8 s). On decimal arrays LZMA2 was 10 to 12% smaller at 25 to 30 times the encode time (a 24 MB table: 8.0 s for
  1.26 MB against 0.3 s for 1.40 MB), which is 18 KB saved per CPU second where the cost model breaks even at 54
  (data kept a year) or 652 (30 days). With block sorting that table costs less than OpenZL (CPU and storage) and
  is 23% smaller; with LZMA2 it cost 3.3 times OpenZL. Decimal files at `ratio` are 2 to 12.6% larger than at the
  previous commit and still 5 to 43% smaller than with 1.0.1 (eight files checked); `-x` is unchanged. Integer arrays keep LZMA2.
  `hc`: float planes at zstd 9 instead of 19 (same size, up to 16 times faster); decimal planes keep zstd 19 (zstd 12
  was 3 to 36% larger).
- **More encode time off `balanced`, output unchanged (branch `claude/dec64`).** Measured by instruction counts
  (callgrind) and CPU time; every change below leaves all 83 test files byte-identical at `balanced`.
  - A plan marked "check each block" coded every block twice at the fast settings (the graph and plain zstd). The
    plain pass now runs only when three 64 KiB slices coded plain come within 35% of the graph's rate: a 31 MB
    decimal table goes from 185 to 109 ms.
  - Delimited tables (graph path): fields are found eight bytes at a time, numbers are read in one pass, and tag and
    scale streams are reserved once per block: a 316-column table 54 to 48 ms.
  - Planner: the integer trial looks at four of its sixteen sample runs first and stops a width whose estimate is a
    quarter above plain coding; plane entropies read one byte per value. On mozilla (no plan wins) total
    instructions fall 8.4%.
  - DEC64 writes only the byte planes a column reaches.
  - Planner: the 8-bit image trial runs first at `balanced`, and an image layout that saves a fifth ends the typed
    trials (the integer trial was a quarter of the encode of a 0.8 MB photograph): instructions on that file -41%.
- **Graph leaves are coded faster at `balanced` (branch `claude/dec64`).** The byte count that decides whether a
  leaf tries Huffman used one table, which stalls on runs of one byte value (most of a number plane); it now uses
  four. Huffman is not tried where it cannot win: a prefix code spends at least one bit per byte, and at `balanced`
  a leaf whose samples put Huffman a tenth behind zstd skips it. Output is byte-identical on 83 files at
  `balanced`, `hc -l 9` and `ratio -l 1`. Encode CPU on decimal arrays: 118 to 75 ms (24 MB), 323 to 205 ms (31 MB);
  a 316-column table 87 to 76 ms. With the planner order below, `balanced` encode CPU over 79 files is 13% lower
  (5,506 to 4,793 ms).
- **DEC64: second difference (branch `claude/dec64`).** A column or segment may store the difference of successive
  deltas (flag bit 4), chosen when it is 5% shorter again: running totals and steady trends. A 31 MB table with
  cumulative columns (`co2.npy`, float64 or float32): 10.6% smaller at `balanced`, 11.4% at `ratio`; other tables
  within 0.7%.
- **Graph leaves: the Huffman-only shortcut no longer misfires on sparse leaves (branch `claude/dec64`).** At
  `balanced`, three 32 KiB samples decide whether a large leaf is coded with Huffman alone. When all three fall in the
  busy part of a leaf that is mostly one byte value, Huffman was chosen and spent one bit per byte on the rest (found
  by the E02C research harness on another branch: 164 KB against 3 KB on one plane). Both coders are now tried when
  one value fills half the leaf. A 31 MB decimal table (`co2.npy`): 2,397,150 → 1,963,547 bytes (18.1% smaller); nine
  other decimal files 1.5 to 3.5% smaller; no other output in the 81-file set changed.
- **Planner: the record-plan leak is fixed the way `fix/planner-record-ownership` fixes it** (one owner, freed at the
  end), replacing this branch's site-by-site frees, and the image model's block codec moves from 15 to 17, so the two
  branches and `feat/native-residual-leaf` (graph leaf codec 15) can be brought together without clashes.
- **Decimal mapping for float32 (branch `claude/dec64`).** The DEC64 node takes 4-byte words (a per-column flag):
  binary32 values that were written in decimal are stored as the integer `m` with value = `(float)(m / 10^e)`.
  `.npy` `<f4` tables and float32 arrays inside containers. Four holdout tables converted to float32: `balanced`
  5.63 MB → 2.59 MB, `ratio` 3.79 MB → 2.11 MB (per file 3 to 58% smaller). Float64 output is unchanged.
- **`ultra` model 3: structure contexts and a better match choice.** Five more contexts for text: the key a value
  belongs to (`"key":`, `key=`, the tag after `<`) and the field of a delimited line with the byte above it in the same
  field; the match model now takes, of the last 16 places a context was seen, the one that agrees for longest. On
  top of the first model 3: CSV 4.4% smaller, JSON 2.4%, XML 2.3%, logs 2.0%. Against model 2 in all: JSON 6.7%, CSV
  5.4%, XML and logs 4.7%, Polish prose 3.4%, English prose 0.3%, at about 35% more time. Model 3 uses about 0.8 GB
  more memory than model 2, plus 4 bytes per input byte.
- **`ultra`: context-mixing model 3 for text (branch `claude/dec64`).** Model 2 plus four contexts (orders 12 and 20,
  and the byte the match model expects with orders 1 and 2), finer length classes for long matches, and a match that
  carries on past a one-byte difference. Used for text blocks; binary blocks keep model 2, where model 3 gained
  nothing (-2.2% to +0.06% on seven binary files). JSON 142,217 → 135,936 bytes (4.4% smaller), Polish prose 3.4%,
  logs 2.8%, XML 2.5%, CSV and source code about 1%; about 20% slower than model 2. Streams carry an extended header
  (flags 8, then model and table size); an older reader stops with an error.
- **`ultra`: an image model for the context mixer (block codec 17; branch `claude/dec64`).** For 8-bit images the
  mixer's hashed contexts are the pixel's neighbours (left, above, the diagonals, two back), the earlier channels of
  the same pixel, and gradient, median and inter-channel predictions with the local activity, in place of the
  preceding bytes; the mixers are selected by the prediction and the neighbours. A 512x512 grey photograph: 132,883 →
  116,664 bytes (paq8px -8: 113,869); a 512x512 RGB photograph: 404,896 → 323,467 (paq8px -8: 308,539). Encode about
  150 KB/s, as the mixer elsewhere. Files that use it need this version; an older reader stops with an error.
  `make check-cmimg` round-trips generated images and decodes damaged streams under AddressSanitizer.
- **`ratio` and `ultra`: image graphs try block-sorted leaves; `ultra` tries a transformed block plain.** Colour
  photographs are 5 to 6% smaller at `ratio` with block-sorted leaves in the image graph. At `ultra` a transformed
  block is also coded plain when plain block sorting is within 2% of it, which removes a case where `ultra` was
  larger than `ratio` (a 1 MB image array: 276,549 → 203,747; `ratio` 245,200).
- **Integer columns inside containers (branch `claude/dec64`).** The array finder of the DEC64 node also takes runs
  of small int64 values (a per-segment flag), and a whole-file numeric plan that the probes mark gives way to the
  segment plan. The int64 pickle that regressed on main: 129,908 → 98,236 bytes at `balanced` (1.0.1: 129,908), 76,398
  → 71,804 at `ratio`; a mixed pickle 8.5% smaller. Other outputs within 0.1%.
- **Regression ledger (`benchmark/accept/REGRESSIONS.md`, branch `claude/dec64`).** Every file of the corpus and
  the holdout sets measured against 1.0.1 and main at three settings: what got slower or larger, what was fixed, what
  is open, and which costs are accepted for what gain. From it: the plain attempts at `ratio` are now made only when
  a block-sorted slice says they can win (ratio encode over 81 files: 384 s → 228 s, main 262 s), and DEC64 must
  beat the next plan by 7%.
- **`ratio`: a transformed block is also tried plain (branch `claude/dec64`).** Transforms are chosen by their zstd
  score; under block sorting or LZMA2 the plain bytes are sometimes smaller. A transformed block now tries block
  sorting of the plain bytes when a slice from the middle is within reach, blocks up to 2 MiB also try plain LZMA2,
  and a plain result is kept when it is at least 3% smaller. Silesia `nci`: 1,533,927 → 1,225,386 bytes (20%
  smaller; its decode goes from about 120 to 67 MB/s, the block-sorting rate). Silesia tar: 40,996,509 → 40,687,968
  (19.34% → 19.20%), encode time unchanged, decode about 10% slower on the cloud VM. A 1 MB image array was 16%
  smaller. On 81 corpus and holdout files plain coding now beats the `ratio` plan by more than 1% on none (before: 2).
- **Mixed inputs: a numeric plan is checked away from its trial slice (branch `claude/dec64`).** A numeric graph or
  byte shuffle is chosen on one slice from the middle of the input. Four 32 KiB probes elsewhere now test that the
  rest is the same kind of data; where the plan codes a probe over 10% larger than plain coding, each block is coded
  both ways and the smaller kept. Fixes a regression of the `balanced` quick analysis that is on main: a pandas
  pickle of int64 columns followed by strings coded 69% larger than 1.0.1 (219,892 against 129,908 bytes; now
  129,908). A table of full-precision doubles that every version shuffled is 20% smaller at `balanced` and 26% at
  `ratio`. Both files are outside the test corpus; on the 81 corpus and holdout files, plain coding now beats the
  chosen plan by more than 1% on none (before: these two). No corpus output changed.
- **Decimal doubles: a new kolmo graph node (DEC64, node id 7; branch `claude/dec64`, not on main yet).** Binary
  float64 values that were written in decimal (prices, measurements, counts, most CSV data loaded into arrays) are
  stored as the integer `m` with value = `m / 10^e`: one exponent per column, the integers as zigzag byte planes with
  an optional delta, one tag byte per value. A column's most common other pattern (usually NaN for missing) is a tag;
  anything else (NaN payloads, -0.0, full-precision values) is kept verbatim as an exception, so every input round
  trips exactly. A row-major `.npy` table uses its shape for the columns. Any other input is searched for arrays of
  doubles at any byte alignment (a raw dump, a column-major `.npy`, and the columns inside a pandas pickle, an
  uncompressed `.npz` or a MATLAB `.mat` file); each array is cut into segments of 4,096 values that choose their
  own exponent, and everything between the arrays stays as plain bytes. On a holdout set built after the design was fixed (OWID CO2, three OpenZL sample tables, a
  nuclear mass table, scikit-learn digits and a telemetry table, each as `.npy` and as a raw column-major array; 14
  files, 95 MB): `balanced` 11.9 MB → 5.8 MB (every file 25 to 57% smaller), `ratio` 7.2 MB → 4.6 MB (0 to 49%
  smaller; xz -9: 8.3 MB). The three control files without decimal values are byte-identical. On the affected files
  encode takes 1.4 to 2.4 times the CPU and decode 1.3 to 1.8 times (still 120 to 240 MB/s and 420 to 770 MB/s on the
  cloud VM); other files pay one 64-value sample. At `ratio` and `ultra` the plan is scored with zstd, so each such
  block is also coded plainly and the smaller result kept. Files that use the node need this version or later; an
  older reader stops with an error at the unknown node. The inverse checks every tag and count before it writes
  (`make check-dec64`: round trips with awkward values and 59,000 damaged payloads under AddressSanitizer).
  A second holdout of containers (the same tables as pandas pickle, `.npz` and `.mat`; 12 files, 123 MB):
  `balanced` 15.5 MB → 7.8 MB, `ratio` 8.9 MB → 6.6 MB, no file larger (xz -9: 10.6 MB). Not covered yet: integer
  arrays inside containers (a pickle of int64 columns is unchanged), and doubles interleaved with other fields.
- **Line classes: a new transform for fixed-width text (block flag 12, container version 8).** Text whose lines come
  in a few fixed lengths (chemistry and geometry files, fixed-width reports and dumps) is stored column by column
  per line length. `balanced` takes it where it codes a trial slice at least as small as the template transform; it
  costs a line scan and a transpose instead of tokens and templates. Silesia `nci` at `balanced`: 2,038,889 →
  2,027,737 bytes, encode 254 → 92 ms, decode 57 → 36 ms, peak memory 54 → 14 MB. Files that use it carry version 8
  and need this version or later to decode; every other file is written as before (version 3 to 7). The decoder
  checks every length and count (`make check-lines`: round trips and 51,200 damaged inputs under AddressSanitizer).
- **`balanced` plans binary data with the quick analysis.** Trial slices of 256 KiB scored at zstd level 1, shuffle
  trials only at the byte width whose planes are most predictable and only where one is, the remaining layout trials
  skipped once a 16-bit image layout saves a fifth, and the x86 check first on code. The text trials and the integer
  trial are unchanged. Sizes on the 132-file test corpus are unchanged except one weights file (0.02% smaller).
- **Template transform: 25% less encode time on repetitive logs.** A line with the same shape as the last line of its
  length (digits in the same places, every other byte equal) reuses that line's tokens and template; runs of spaces
  are not scanned; the template table starts small. Output is byte-identical.
- Silesia as one tar, one thread, in memory, same process as zstd 1.5.7 (cloud VM, best of seven): `balanced` goes
  from 124 to 176 MB/s round trip (encode 146 → 221, decode 851 → 867) at 27.48% of the input; zstd -3 is 189 MB/s
  at 31.20%. The 11.9% size advantage is kept and the round trip is at 93% of zstd -3, up from 66%. `fast -l 2` is
  8.7% smaller than zstd -3 and 6% faster on the round trip (200 MB/s). What remains for `balanced` is planning time
  (about 130 ms on 212 MB) and the inverse transforms on decode; the LZ pass is zstd's in both.
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
