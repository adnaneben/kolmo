# Research: 6-bit encoding, bit-level neural coding, mathematical reduction, data-structure models

Status: exploratory prototypes, not part of the kolmo format. Everything here was measured on a 4-core Linux VM
(single thread per codec), every output decoded and compared with the input. Python-binding timings for
zstd/brotli/xz/lz4 are in-memory; kolmo timings are whole-process (file I/O and start-up included, which penalises
the small files). Speeds were measured while other experiments ran on other cores, so read them as ±20%.

Corpora: OWID CO2 (15.2 MB CSV, 79 columns), COVID countries-aggregated (5.5 MB CSV), ggplot2 diamonds (2.8 MB CSV),
Titanic (60 KB CSV), Loghub HDFS_2k / Apache_2k logs, tiny-shakespeare (1.1 MB text).

## 1. The 6-bit alphabet (k6)

**Spec note:** 33 alphabet codes + 10 digits = 43, which leaves 21 free codes, not 31. The prototype uses 26 letters
(case folded) + 10 digits = 36, which leaves 28 free codes.

Three ways to use the free codes were built (`sixbit/k6.c`, `sixbit/bench6.py`):

| table | codes 36-63 |
|---|---|
| K6-fixed | 25 punctuation/whitespace codes (`space , \n . - " : / _ ' ( ) ; \t + = \r [ ] @ # % & * !`) + SHIFT, CAPS-LOCK, ESC |
| K6-adaptive | the 61 most frequent bytes of the file (no case folding) + ESC |
| K6-tokens | letters+digits with SHIFT/LOCK, and the 25 free codes go to the file's most valuable single bytes **or multi-byte strings** (`,,`, `0.0`, words...) |

ESC + two codes carries any other byte, so every input round-trips.

### Ratio and speed (ratio = original / compressed)

| file | K6-fixed | K6-adaptive | K6-tokens | huff0 (static Huffman) | zstd -1 | zstd -19 | xz -9e | brotli -11 | kolmo balanced | kolmo ratio |
|---|---|---|---|---|---|---|---|---|---|---|
| owid-co2.csv | 1.32 | 1.33 | **2.05** | 2.35 | 3.64 | 4.36 | 4.83 | 4.83 | 10.94 | 12.45 |
| covid.csv | 1.29 | 1.33 | 1.52 | 1.84 | 4.96 | 7.26 | 8.42 | 8.34 | 4.16 | 5.18 |
| diamonds.csv | 1.24 | 1.33 | 1.60 | 1.85 | 4.03 | 5.81 | 6.22 | 6.01 | 6.24 | 9.10 |
| titanic.csv | 1.23 | 1.33 | 1.49 | 1.59 | 2.46 | 2.97 | 3.21 | 3.28 | 2.63 | 3.51 |
| hdfs.log | 1.26 | 1.33 | 1.42 | 1.53 | 5.67 | 6.72 | 6.80 | 7.02 | 6.85 | 7.94 |
| apache.log | 1.29 | 1.33 | 1.41 | 1.60 | 16.6 | 23.9 | 25.4 | 25.5 | 38.4 | 44.2 |
| shakespeare.txt | 1.28 | 1.33 | 1.48 | 1.66 | 2.30 | 3.03 | 3.06 | 3.09 | 2.58 | 3.76 |

| codec | encode MB/s | decode MB/s |
|---|---|---|
| k6 (scalar C prototype) | 65–170 | 355–615 |
| huff0, 4 streams | 1170–1400 | 1650–1950 |
| lz4 | 300–1550 | 1900–3550 |
| zstd -1 | 260–1000 | 785–2180 |

### What this shows

* **On its own, k6 is capped at 1.333** (8/6). Static Huffman, the classic symbol code, beats it on every file
  (1.53–2.35) and runs about 8× faster. A fixed-width code cannot follow skewed symbol frequencies; Huffman can. The
  "25% saved" is real only against uncompressed ASCII. Every standard compressor already saves 35–96%.
* **Packing to 6 bits damages any compressor run after it.** xz on packed k6 vs xz on the raw file: OWID 3.72 vs 4.83,
  COVID 5.36 vs 8.42, Apache 13.8 vs 25.4. Packing four 6-bit symbols into three bytes misaligns them, so the same
  string shows up as four different byte patterns and LZ matching fails.
* **The mapping itself adds no information.** Run the same compressors on k6 symbols kept one per byte and you get the
  same ratio as on the raw file (±1%). A bijective relabelling of symbols can't reduce entropy. Only modelling can.
* **Best use of the free codes:** multi-byte tokens (K6-tokens). Standalone ratio goes from 1.32 to 1.42–2.05 (2.05 on
  wide numeric CSV, where `,,`, `0.0`, `.000` repeat). That makes it a byte-pair-encoding tokenizer, which is useful
  as a front end for a modelling coder, not as the compressor itself.

### Recommendation for the free codes, if a 6-bit alphabet is kept

1. Mandatory controls: ESC (raw byte), SHIFT, CAPS-LOCK (3 codes).
2. Structural delimiters for CSV/logs: `space , \n . - " : / _ ' ( )` (≈12 codes).
3. The rest are per-file **dynamic tokens** chosen by the encoder and sent in the header: frequent n-grams, empty
   fields `,,`, decimal patterns. For CSV, a **DITTO** code ("same field value as the row above") is the most promising
   untested candidate. Section 3 shows how much same-as-previous structure CSV columns carry.

Where 6 bits *does* pay off is as the alphabet of a bit-level model (section 2).

### Dense layout: a-z, 0-9, A-Z in sequence, absent symbols pruned (`sixbit/bench_dense.py`)

Codes follow a-z, 0-9, A-Z (code 36, the 37th code, is `A`). Letters and digits the file never uses are removed, so
the rest pack densely, and uppercase has its own codes, so no SHIFT is needed. Ratio = original / packed (below 1
means the output is larger):

| file | letters+digits present (of 62) | other distinct bytes | non-alnum share | 36 + 3 controls | dense + ESC | dense + PAGE | dense + fill freed slots | frequency choice (63 + ESC) |
|---|---|---|---|---|---|---|---|---|
| owid-co2.csv | 62 | 9 | 41.3% | 0.727 | 0.731 | 0.944 | 1.067 | **1.332** |
| covid.csv | 61 | 9 | 21.5% | 0.909 | 0.933 | 1.098 | 1.238 | **1.329** |
| diamonds.csv | 37 | 5 | 41.0% | 0.702 | 0.733 | 0.946 | **1.333** | **1.333** |
| titanic.csv | 61 | 10 | 31.3% | 0.779 | 0.820 | 1.016 | 1.139 | **1.329** |
| hdfs.log | 52 | 12 | 17.3% | 0.954 | 0.991 | 1.137 | **1.333** | **1.333** |
| apache.log | 39 | 13 | 27.2% | 0.845 | 0.864 | 1.048 | **1.333** | **1.333** |
| shakespeare.txt | 53 | 12 | 23.7% | 0.882 | 0.905 | 1.078 | **1.333** | **1.333** |

ESC = the next 2 codes carry a raw byte (18 bits per escaped byte); PAGE = the next code indexes a second table of 64
bytes (12 bits); "fill" gives the slots freed by pruning (plus the 26 never used) to the most frequent other bytes.
Encode 125–225 MB/s, decode 200–525 MB/s for the scalar prototype; all round trips verified.

* **Letters and digits alone can't carry CSV, logs or text.** Space, comma, newline, `.` and `-` are 17–41% of all
  bytes. With only 36 + 3 controls (or with 62 dense letters and digits and nothing else), each of those costs 18 bits
  and the "compressed" file grows by 5–43%.
* **Pruning works when the file uses few letters.** Diamonds uses 37 of the 62, the Apache log 39, Shakespeare 53. The
  freed slots take the punctuation, and dense+fill reaches the full 1.333 with zero escapes.
* **It does not work for tables with country or person names.** OWID, COVID and Titanic use 61–62 of the 62, because
  names bring in nearly every uppercase letter. Pruning frees 0–1 slots, a rare `Q` or `X` keeps a code while the comma
  (8–12% of all bytes) is escaped, and the ratio drops to 1.07–1.24.
* **Pick symbols by frequency, not by being a letter.** Choose the 63 most used bytes of the file (rare uppercase
  letters escaped) and every file reaches 1.329–1.333, with escapes under 0.3% of symbols. The order of codes (a-z
  first, `A` = 37th) doesn't change the size of a fixed-width code; only which symbols get a code does.
* Every file here has at most 71 distinct bytes, so 64 codes are always enough. Two (diamonds with 42, Apache with 52)
  fit entirely and need no ESC at all.

**Answer to "do we need more than 26 + 10 + 3 controls":** yes, at least space, comma, newline, `.` and `-`, and in
practice the file's most frequent punctuation (10–13 bytes). We never need to go beyond 64 codes: one control (ESC)
is enough, because uppercase gets its own codes and SHIFT/LOCK disappear. The best rule is: rank all bytes by
frequency, give the top 63 a code, ESC the rest. This doesn't change the overall conclusion: the ceiling stays 1.333,
below Huffman's 1.53–2.35 on the same files.

## 2. Bit-level neural network coding (`neural/nncm.c`)

A real coder with an arithmetic encoder and decoder. Each symbol is coded as binary decisions. Hashed context models
(orders 0–6, word, CSV column and the same column in the row above) feed a single-layer logistic network (online
gradient descent), then an APM refines the output. Level 0 is order 0 with no network, level 1 adds orders 1–4 and the
mixer, level 2 adds the word and column contexts.

| file | huff0 | xz -9e | nncm L0 | nncm L1 | nncm L2 (8-bit) | nncm L2 on k6 symbols (6-bit) | kolmo -x (its CM tier) |
|---|---|---|---|---|---|---|---|
| diamonds.csv | 1.85 | 6.22 | 1.81 | 8.47 | **9.38** · 1.20 MB/s | 9.28 · **1.78 MB/s** | 10.91 · 0.20 MB/s |
| covid.csv | 1.84 | 8.42 | 1.91 | 7.07 | 8.09 · 1.03 MB/s | 7.88 · **1.39 MB/s** | 15.41 · 0.35 MB/s |
| hdfs.log | 1.53 | 6.80 | 1.56 | 7.01 | 7.17 · 0.77 MB/s | 7.05 · **1.16 MB/s** | 9.53 · 0.12 MB/s |
| shakespeare.txt | 1.66 | 3.06 | 1.63 | 3.52 | 3.56 · 0.72 MB/s | 3.52 · **1.02 MB/s** | 4.10 · 0.09 MB/s |

(6-bit ratios and speeds are relative to the original file bytes. Decode speed ≈ encode speed, as for any symmetric CM.)

* A ~300-line bit-level network beats xz -9e on 3 of 4 files at ~1 MB/s. The network is where ratio comes from:
  L0 → L1 is 4–5×.
* **The 6-bit alphabet works as a speed lever for neural coders.** Six binary decisions per symbol instead of eight
  gives 35–50% faster coding in both directions, for 1–3% larger output. ESC/SHIFT add symbols, and case folding
  splits contexts. This is the one place the 6-bit idea pays off.
* kolmo's existing CM tier (`src/cm.inc`, mature lpaq-style with two mixers, a match model and record contexts) is
  ahead on ratio but 5–10× slower. The gap on covid (15.4 vs 8.1) comes from kolmo's table transforms running
  before the network. Transform first, then model: that is how a neural network should be used here.
* "Neural networks in the transform": learned transforms such as autoencoders and VQ are lossy, so they don't fit a
  lossless format. The network belongs in the probability model, as here, where it is lossless by construction
  because encoder and decoder run the same deterministic updates. A deployable version needs integer-only arithmetic
  (as cm.inc has). The prototype uses floats, which is safe only on one machine.

## 3. Mathematical reduction functions (`mathred/tabmath.py`)

For each CSV column the cheapest model wins: text, dictionary ids, or fixed-point integers with a predictor (none,
delta, delta-of-delta, delta / delta-of-delta within a group such as country). Then **derived-column formulas**:
v − round(F(a, b)·10^d) with F ∈ {a/b·k, a − lag(a), 100·(a/lag(a) − 1), …} over columns already decoded. Every column
is decoded back to its exact text before its size counts.

| file | xz -9e whole file | xz per column | math models + xz | kolmo ratio | kolmo -x |
|---|---|---|---|---|---|
| covid.csv | 8.42 | 12.11 | **17.79** | 5.18 | 15.41 |
| diamonds.csv | 6.23 | 7.70 | 9.08 | 9.10 | 10.91 |
| owid-co2.csv | 4.83 | 5.91 | 8.78 | 12.45 | — |
| titanic.csv | 3.22 | 3.46 | 3.55 | 3.51 | — |

* Cumulative series (COVID confirmed counts, OWID cumulative_co2, population) are closest to a straight line locally,
  so **delta-of-delta** wins them. The residuals are 40–50% smaller than the text.
* **Derived columns are the clearest new idea.** The search found 11 OWID columns that are exact or near-exact
  functions of others: every `*_per_capita` = X / population · 10⁶ and every `*_per_gdp` = X / gdp · 10⁹, matching
  72–99% of rows exactly with the rest ±1 in the last digit. Each shrinks 6–7× beyond delta coding.
* **Measured inside kolmo:** remove those 11 columns from OWID and add back the formula residual streams (43.7 KB with
  xz). kolmo balanced goes 10.94 → **11.47 (−4.6% size)** and kolmo ratio 12.45 → **13.44 (−7.3%)**.
  A `DERIVE` node in `kgraph` (inputs: decoded columns; params: formula id, constant, scale; output: residuals) would
  capture this. To be deterministic it must use exact integer arithmetic, e.g. a 128-bit rounded division, not floats.
* Found along the way: kolmo balanced/ratio score below xz on covid.csv, while this simple model reaches 17.8×.
  Filed as a separate task.

## 4. Data-structure models (`structures/dstruct.py`)

| model | result |
|---|---|
| **Column-major lists** instead of rows | xz gains 20–30% on every CSV (OWID 4.83→6.04, COVID 8.42→11.96, diamonds 6.23→7.78). Known, and kolmo already does it. It's the base layer the other wins sit on. |
| **Functional-dependency graph** between columns (A determines B, store only the mapping) | 0 bytes saved on all four CSVs. When rows are grouped (OWID by country), LZ already codes the dependent column almost for free. Only worth it for unsorted data. |
| **Word-transition graph** (rank of each word in the previous word's move-to-front successor list) | Rank entropy 3.06 bits/token vs 11.22 for word ids, but 67% of transitions are first-time, and their escapes cost more than xz on raw text (ratio 2.76 vs 3.06). An order-1 word graph loses to byte-level modelling; the neural coder (3.56) wins on the same file. |
| **Trie (front-coded sorted dictionary)** | 21% smaller after xz than a plain sorted list (27.3 KB vs 34.5 KB, shakespeare vocabulary). Worth using for the dictionary of any categorical column or token table. |
| **Functions as structure** (section 3) | The biggest win: a column described by a formula over other columns. |

## Overall conclusions

1. As a compressor, the 6-bit encoding is beaten on ratio and speed by Huffman alone, and it hurts whatever compressor
   runs after it. Don't ship it as a storage format.
2. The 6-bit alphabet is useful as the **symbol domain of a bit-level neural coder**: ~40% faster for 1–3% size.
3. The real gains for CSV come from modelling, in this order: column layout → per-column predictors (delta,
   delta-of-delta, per group) → **derived-column formulas** (new: +4.6% / +7.3% on top of kolmo on OWID) → a bit-level
   network to code the residuals.
4. Next step for kolmo: a `DERIVE` kgraph node with integer-exact formulas, searched over column pairs on a sample;
   then try a DITTO / dynamic-token alphabet as context for the CM tier.

## Reproduce

```
make build/kolmo                        # in the repo root
gcc -O3 -march=native -o k6 research/sixbit/k6.c
gcc -O3 -march=native -o nncm research/neural/nncm.c -lm
gcc -O3 -march=native -Ivendor/zstd -Ivendor/zstd/common -o huffbench research/sixbit/huffbench.c build/libvendor.a -lpthread
pip install zstandard brotli lz4 numpy
python3 research/sixbit/bench6.py ./k6 build/kolmo workdir data/*.csv data/*.log data/*.txt
./nncm c 2 8 file out ; ./nncm d 2 8 out back            # 8-bit; for 6-bit, code the .sym file that k6 writes
python3 research/mathred/tabmath.py file.csv [report.json]
python3 research/structures/dstruct.py layout|fdgraph file.csv ... ; python3 research/structures/dstruct.py wgraph|trie file.txt
```

## 5. Reading the numbers, and the recommended 6-bit model

**How to read a ratio.** Ratio = original size ÷ compressed size; "% of original" = compressed ÷ original.

| ratio | file becomes | saved |
|---|---|---|
| 1.333 | 75% of original | 25% |
| 1.53 | 65% | 35% |
| 2.35 | 42.5% | 57.5% |
| 4.83 | 20.7% | 79% |
| 12.45 | 8% | 92% |

**Why Huffman (1.53–2.35) beats fixed 6-bit (1.333).** A fixed code gives every symbol the same 6 bits. Huffman gives
frequent symbols short codes and rare ones long codes. In OWID, `,` is 28.8% of all bytes and ideally costs 1.8 bits,
`0` (20.1%) 2.3 bits and `.` 3.1 bits, while `Q` (0.01%) costs 13.8 bits. Most bytes are frequent ones, so the
average drops to 3.45 bits, against 6 for the fixed code.

### Experiments (all lossless, decoders included; `sixbit/gr6.py`, `sixbit/variants.py`)

* **radix**: the "42 symbols, then 43 rolls over" idea. Number the file's N distinct bytes 0..N−1 and write the text as
  one base-N number, so each character costs log2(N) bits (DEC's RADIX-50 packed 40 characters, 3 per 16-bit word).
* **bpe63**: the file's distinct bytes get codes 0..N−1, and codes N..62 go to the most frequent pairs and strings.
* **GR6**: a repeat model plus a graph map, then 6-bit units. Each token (a run of letters, digits, `.` or `-`, or
  one punctuation byte) is coded as: DITTO (same as the line above, same position), else its rank in the graph's list
  of tokens that followed the same context before, else its global id, else NEW plus its characters. Values are
  written as 6-bit units: 5 data bits and a continuation bit.

Size as % of original (ratio in brackets):

| file | distinct bytes | plain 6-bit | radix | bpe63 | GR6 → fixed 6-bit | GR6 → Huffman | GR6 → xz | GR6 → neural | xz -9e original | Huffman original |
|---|---|---|---|---|---|---|---|---|---|---|
| titanic.csv | 71 | n/a | 77.0% (1.30) | 75.3% (1.33) | 70.0% (1.43) | 54.5% (1.84) | 44.1% (2.27) | 39.7% (2.52) | 31.2% (3.21) | 62.9% (1.59) |
| hdfs.log | 64 | 75.0% (1.33) | 75.0% (1.33) | 75.0% (1.33) | 38.7% (2.58) | 23.6% (4.23) | 16.0% (6.25) | 15.8% (6.32) | 14.7% (6.80) | 65.5% (1.53) |
| apache.log | 52 | 75.0% (1.33) | 71.3% (1.40) | 61.2% (1.63) | 36.4% (2.74) | 12.3% (8.13) | 4.5% (22.1) | 5.5% (18.3) | 3.9% (25.4) | 62.5% (1.60) |
| diamonds.csv | 42 | 75.0% (1.33) | 67.4% (1.48) | 48.7% (2.05) | 45.5% (2.20) | 24.1% (4.14) | 19.3% (5.19) | 16.8% (5.94) | 16.1% (6.22) | 54.1% (1.85) |
| shakespeare.txt | 65 | n/a | 75.3% (1.33) | 75.0% (1.33) | 54.0% (1.85) | 46.2% (2.16) | 40.7% (2.46) | 36.4% (2.74) | 32.7% (3.06) | 60.2% (1.66) |
| covid.csv | 70 | n/a | 76.6% (1.31) | 75.2% (1.33) | 43.6% (2.30) | 25.9% (3.87) | 13.9% (7.18) | 12.2% (8.21) | 11.9% (8.42) | 54.5% (1.83) |
| owid-co2.csv | 71 | n/a | 76.9% (1.30) | 75.1% (1.33) | 53.8% (1.86) | 34.0% (2.94) | 25.9% (3.86) | 21.4% (4.67) | 20.7% (4.83) | 42.5% (2.35) |

GR6 token mix: DITTO 42–77% of tokens on CSV and logs, graph hit 7–61%, NEW 1–12%. GR6 ran at 0.25–1.3 MB/s in Python;
no C version exists yet, so it has no speed figure.

* **Radix** only helps when a file uses few distinct bytes: 42 → 5.39 bits/char (67%). Real files use 64–71 distinct
  bytes, and then it is no better than 6 bits.
* **Codes after the alphabet for pairs (bpe63)** is the best pure alphabet trick: diamonds 2.05 (49%), Apache 1.63.
  It needs free codes, so it does nothing for files that use 64+ distinct bytes.
* **Repeat + graph first, then 6-bit (GR6)** is the best fixed-width result: 36–70% of original (1.43–2.74). It beats
  Huffman on 5 of 7 files, while staying a simple stream of 6-bit units.
* Fixed 6-bit is still the weak final step. On covid, DITTO is 42% of units and carries about 1.2 bits of
  information, yet costs 6. Replace the final 6-bit stage with Huffman over the units and the size roughly halves
  again. A neural coder over the same units reaches xz -9e level (within 0.2–1.6 points on 5 of 7 files).

### Recommended model

1. **Transform**: tokens → DITTO (repeat from the line above) → graph map (rank among the successors seen in the same
   context) → global id → NEW literal. For tables, add the column predictors and formulas of section 3.
2. **6-bit units as the internal alphabet**: every decision becomes a value written in 6-bit units. This keeps the
   symbol alphabet at 64, which makes the final coder simple and fast. A bit-level network codes 6 decisions per unit
   instead of 8 (section 2: ~40% faster).
3. **Adaptive final coder, never fixed width**: Huffman over units for speed, the neural coder for size.

Stored as raw 6-bit units, the format is simple but gives up 30–60% of the achievable size. 6 bits is the right
*alphabet*, not the right *code length*.
