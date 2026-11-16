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
