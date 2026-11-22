# Kolmo without its rivals' libraries: census, prototypes and plan (rc3 wave 2)

**Done in container version 12 (rc4-native):** the libraries are gone. Every encoder path writes Kolmo's own ids
(KLZ 28, KLZH2 32, KLZMA 24, khuf 27), the probes use KLZH2 (`kprobe_size`, `src/core/common.h`), every hash is
`kolmo_khash64`, dictionaries and trained profiles are not available (`KOLMO_ERR_UNSUPPORTED`), and frames of versions
1-11 are refused (`KOLMO_ERR_LEGACY`). The census below describes the tree before that change.

Written by the native lane on 17 November 2026 (`claude/rc3w2-native-20261117`).

The owner does not want Kolmo to depend on the libraries of the tools it competes with: zstd (including the HUF and FSE
coders and the ZDICT trainer inside it), LZ4, Brotli, liblzma, and xxHash. This document has three parts:

1. **Census:** every call into those libraries, the modes and paths that reach it, and its share of time and of
   output bytes per mode.
2. **Prototypes:** Kolmo's own coders, measured against the library each would replace.
3. **Plan forward:** for each library, keep, replace or drop; the order; the expected ratio and speed; the effort and
   the risk.

The numbers, commands and raw tables are in `benchmark/rc3w2/native/RESULTS.md`.

**Machine:** a cloud VM with an Intel Xeon at 2.10 GHz (4 vCPUs, AVX-512), Linux 6.18, GCC 13.3.

## Summary

| library | what it does in Kolmo today | Kolmo's prototype | where the prototype stands (same role, same blocks) | verdict for now |
|---|---|---|---|---|
| LZ4 | `speed` (codec 1); a planner probe | **KLZ**, codec 28 | **−5.9% to −6.9% size**; compress 0.68–0.90×; decode 0.60–0.77× | replace early, once decode ≥ 0.9× |
| zstd, levels 1–9 | `balanced` blocks and graph leaves; planner and transform probes | **KLZH**, codec 29 | KLZH1 against zstd -1: −4.7% on Silesia (compress 0.65×, decode 0.94×) but +9.9% on the structured set. Against zstd -3: KLZH3 is +2.4% / +3.8% at 0.76× compress; KLZH5 −2.8% / −1.6% at 0.23× | replace after tuning (2–3 weeks) |
| zstd, levels 19/22 | `hc` | KLZH8, or KLZMA6 (`strong`) | KLZH8 is **+13.7% (Silesia) / +20.8% (structured)**: no optimal parse yet. KLZMA6 is −7.4% / −7.2% but decodes 9× slower | **cannot be matched soon at zstd's decode speed**: an optimal parse for KLZH is the main work item |
| zstd's HUF | Kolmo's Huffman leaves (codec 13) | **khuf**, codec 27 | +0.09% to +0.16% size; compress 0.83–0.93×; decode 0.77–1.02× at 1 thread (1.02× on Kolmo's own leaf chunks) | replace |
| zstd's FSE | not called by Kolmo (only inside zstd) | — | — | goes with zstd |
| ZDICT | `-D`/`-t` dictionaries, `kolmo train` profiles | none | — | keep until a trainer and dictionary support exist in KLZH |
| Brotli | `eco` (codec 4); profiles | KLZH7, or KLZMA3 (`strong`) | KLZH7 is **+8%** (no context modelling, no static dictionary) but compresses 1.6× and decodes 1.5–3× faster; KLZMA3 is **−12% to −18%** but decodes 4.6× slower | **cannot be matched soon at Brotli's point**; KLZMA3 is a different trade-off |
| liblzma | `ratio` binary blocks (codec 6), `max`/`-x` trials, CM-mode leaves | **KLZMA**, codec 24 | **+0.08% (Silesia), +0.31% (structured)**; compress 1.1–1.3×; decode 0.70–0.83× | replace (parse and decoder refinements, 1–2 weeks) |
| xxHash (XXH3) | block checksums, dictionary ids, dedup chunk ids, log-key hashing | **khash64** | same collision and avalanche statistics in every test; speed 0.43–0.55× of XXH3 in cache (≥ 1 KiB), 0.8× from memory, equal at ≤ 64 B | replace when its SIMD kernels exist (a format decision) |
| xxHash (XXH64, zstd's copy) | checksums of container versions 1–2, codec 5's payload check, a sampling hash | khash64 | — | drop with the old formats |

Modes that could ship library-free first: **`ratio`** (KLZMA, BWT, CM and the graph coder are Kolmo's own) and
**`speed`** (KLZ). Both still need two shared items first: the planner's zstd and LZ4 probes, and the block checksum.
Then **`balanced`**, once KLZH matches zstd -3. **`hc`** and **`eco`** come last; for those two, the owner decides
whether KLZMA's slower decode is acceptable.

## 1. Census

### 1.1 Every call site

Kolmo's own code in `src/` calls these entry points (`grep` of `src/` and the bindings; the benchmark tool
`bench/kolmobench.c` uses zstd, LZ4, Brotli and liblzma as rivals and is not part of the library).

| library | entry points | file | path and modes | what replacing it needs |
|---|---|---|---|---|
| zstd | `ZSTD_compressCCtx`, `ZSTD_decompressDCtx`, `ZSTD_compress_usingDict`, `ZSTD_decompress_usingDict`, `ZSTD_compressBound`, contexts | `codecs/zstd.c` | codec 3: `balanced` (level 3), `hc` (19, 22 for text), graph leaves of `balanced`/`hc` (levels 1, 3, 9, 19), table-column segments (`transforms/table_cols.c`), `-D` dictionaries | KLZH (codec 29) at the zstd -3 and zstd -19 points; prefix dictionaries for `-D` |
| zstd | `ZSTD_compressCCtx` (12 call sites), `ZSTD_compressBound` | `plan/planner.c` | planner scoring in every mode: levels 1, 3 and 9 on samples, and the graph-candidate scores | a cost probe of Kolmo's own (KLZH level 1, or an entropy estimate); changes plans, so it needs retuning and a determinism update |
| zstd | `ZSTD_compressCCtx` (`zcost`) | `transforms/sci.c` | cost of the scientific-number transform's field layouts (probe only) | same as the planner |
| zstd | `ZSTD_compress` (level 3) | `transforms/graph_typed.c` | decision probe of the typed-Huffman route (`--typed-huf`, opt-in) | same as the planner |
| zstd | `ZSTD_compressCCtx`, `ZSTD_decompressDCtx` | `codecs/hufzstd_leaf.c` | codec 22 (Huffman then zstd) graph leaves, format 2, `balanced`/`hc` when smaller | khuf then KLZH, or drop (it wins on few files) |
| zstd | `ZSTD_compressCCtx` | `plan/frontier.c` | research build only (`make frontier`) | not part of the release |
| zstd | the CDict/DDict API, `ZSTD_compress2`, block API, `ZSTD_CCtx_setParameter` | `plan/profile.c` | `kolmo train` profiles and `-P` (features-a lane) | KLZH with a prefix dictionary, and the block-level API Kolmo needs |
| zstd (ZDICT) | `ZDICT_trainFromBuffer`, `ZDICT_optimizeTrainFromBuffer_cover` | `api/api.c`, `plan/profile.c` | `kolmo -t` dictionary training, `kolmo train` | a dictionary trainer of Kolmo's own (a COVER-style segment selection is a few hundred lines) |
| zstd (HUF) | `HUF_compress4X_repeat`, `HUF_decompress4X_hufOnly_wksp`, `ZSTD_cpuid` | `codecs/huf.c` | codec 13 leaves (`huf_compress_all`), called from `transforms/graph.c`, `codecs/graph_leaf.c` (plane-aligned leaves), `codecs/nplane.c` (`--native-leaf`), `transforms/graph_typed.c` (`--typed-huf`), and inside codec 22 | khuf (codec 27) |
| zstd (FSE) | none | — | Kolmo never calls FSE; it is inside zstd only | goes with zstd |
| zstd (XXH64) | `XXH64` (zstd's copy, `ZSTD_` namespace) | `core/container.h`, `codecs/bwt.c`, `transforms/graph_typed.c` | checksums of container versions 1–2 (decode of old files); codec 5's payload check (rc2 BWT frames, read only); a sampling hash in the typed route | dropped with the old formats (format 2 does not read them); khash64 for the sampling hash |
| LZ4 | `LZ4_compress_fast`, `LZ4_compress_HC_extStateHC`, `LZ4_decompress_safe`, `LZ4_compressBound` | `codecs/lz4.c` | codec 1 (`speed`, `rt`), codec 2 (LZ4-HC: a sub-codec the per-plane leaf accepts; no mode writes it) | KLZ (codec 28), levels 1 and 3 |
| LZ4 | `LZ4_compress_default` | `plan/planner.c` | a 32 KiB compressibility probe | the same cost probe as zstd's |
| LZ4 | streaming dictionary API | `plan/profile.c` | profiles in `speed` | KLZ with a prefix dictionary |
| Brotli | `BrotliEncoderCompress` (quality 8, window 24), `BrotliDecoderDecompress` | `codecs/brotli.c` | codec 4 (`eco` blocks and their graph leaves) | KLZH or KLZMA (below: neither matches Brotli's point) |
| Brotli | prepared-dictionary API | `plan/profile.c` | profiles in `eco` | as for zstd |
| liblzma | `lzma_raw_buffer_encode` (9e), `lzma_raw_buffer_decode`, `lzma_lzma_preset` | `codecs/lzma.c` | codec 6: `ratio` binary blocks (the better of LZMA2 and BWT), trials in `max` and `-x`, graph leaves when the block codec is CM (`-x`, `ultra`) | KLZMA (codec 24) |
| xxHash (XXH3) | `kolmo_xxh3` (`arch/xxh3.c`) | `core/container.h` (block checksums), `core/blocks.c` (rt payload sums), `core/decode.c`, `api/api.c` (dictionary ids), `core/dedup.c` (chunk ids, checks), `transforms/table_cols.c` (segment checks), `transforms/log.c` (key hashing), `plan/profile.c` (profile ids) | every mode: one checksum per block on compress and decompress | khash64; checksums and ids are a format decision |

The CLI, the bindings and the tests reach these only through the library.

### 1.2 Time and output bytes per mode

Measured with `benchmark/rc3w2/native/libprof.c`, the deps lane's link-time profiler with two corrections:

- **Compressed bytes.** Each decoder call also counts the compressed bytes it reads. That is the part of the output
  file coded through that library.
- **zstd's own Huffman calls.** zstd's literal coder calls `HUF_compress4X_repeat` and `HUF_decompress4X_hufOnly_wksp`
  across object files, so `--wrap` caught those calls too. **The deps lane's table in `docs/DEPENDENCIES.md` counted
  them as Kolmo's Huffman leaves, and so counted that time twice.** They are now part of zstd, and Kolmo's own leaves
  are counted apart. The same applies to codec 22, whose Huffman stream is the output of a zstd decode.

Each cell gives three shares: compress time / decompress time / output bytes. They are sums over the files of one
compress process and one decompress process per file (1 thread, process start and I/O included).

**Silesia, library build (default)**

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | HUF c / d / bytes | LZ4 c / d / bytes | Brotli c / d / bytes | LZMA2 c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|---|---|---|---|
| speed | 99,938,346 | 0.69 | 0.36 | 0.1% / 0.0% / 0.0% | — | 71.0% / 28.0% / 98.9% | — | — | 0.5% / 1.4% / 0.0% |
| balanced | 58,487,839 | 1.58 | 0.64 | 73.6% / 46.9% / 100.0% | — | — | — | — | 0.7% / 1.4% / 0.0% |
| eco | 50,773,162 | 21.29 | 1.21 | 1.7% / 0.0% / 0.0% | — | — | 91.3% / 60.8% / 100.0% | — | 0.2% / 1.5% / 0.0% |
| hc | 47,871,778 | 81.65 | 0.73 | 98.2% / 41.0% / 100.0% | — | — | — | — | 0.0% / 2.0% / 0.0% |
| ratio | 40,924,097 | 65.76 | 6.58 | 0.6% / 0.0% / 0.0% | — | — | — | 80.7% / 17.9% / 62.2% | 0.1% / 0.3% / 0.0% |

**Structured set, library build (default)**

| mode | output bytes | compress s | decompress s | zstd c / d / bytes | HUF c / d / bytes | LZ4 c / d / bytes | Brotli c / d / bytes | LZMA2 c / d / bytes | XXH3 c / d / bytes |
|---|---|---|---|---|---|---|---|---|---|
| speed | 13,888,171 | 0.14 | 0.07 | 0.2% / 0.0% / 0.0% | — | 61.1% / 22.3% / 99.0% | — | — | 0.4% / 0.7% / 0.0% |
| balanced | 3,181,407 | 0.50 | 0.14 | 15.8% / 12.1% / 93.4% | 0.2% / 0.1% / 2.6% | — | — | — | 0.3% / 0.9% / 0.0% |
| eco | 2,974,328 | 2.04 | 0.21 | 3.6% / 0.0% / 0.0% | — | — | 40.3% / 36.0% / 93.0% | — | 0.2% / 1.2% / 0.0% |
| hc | 2,913,190 | 4.18 | 0.16 | 79.1% / 13.2% / 92.8% | 0.0% / 0.1% / 2.8% | — | — | — | 0.1% / 1.6% / 0.0% |
| ratio | 2,750,996 | 12.49 | 0.39 | 0.5% / 0.0% / 0.0% | — | — | — | 89.6% / 36.7% / 88.1% | 0.0% / 0.7% / 0.0% |

What the census says:

- **The library cost is the mode's own setting, not overhead.** Share of Silesia compress time spent in the library:

  | mode | library | share |
  |---|---|---|
  | `speed` | LZ4 | 71% |
  | `balanced` | zstd -3 | 74% |
  | `eco` | Brotli quality 8 | 91% |
  | `hc` | zstd -19/-22 | 98% |
  | `ratio` | LZMA2 9e | 81% |

- **Decode time in the libraries is smaller:** 28% (`speed`), 41–47% (`hc`, `balanced`), 61% (`eco`), and 18% (Silesia)
  to 37% (structured) in `ratio`. On the structured set, every share is lower, because the graph coder and the
  transforms do more of the work.
- **Output bytes:**
  - On Silesia, 99–100% of the output of every mode but `ratio` goes through one library.
  - In `ratio`, 62% (Silesia) and 88% (structured) of the bytes go through LZMA2. The rest is block sorting, which is
    Kolmo's own.
  - On the structured set, the library takes 93% of `balanced`, `eco` and `hc`; the graph coder's rANS leaves, raw
    planes and headers take the rest.
- **Kolmo's direct Huffman leaves (codec 13) are rare:** 2.6–2.8% of the bytes of `balanced` and `hc` on the
  structured set, and none on Silesia.
- **Checksums (XXH3) cost 0.3–2% of decode time.**
- **Probes:** the planner's zstd probes are 0.1–3.6% of compress time. They write no bytes, but they decide plans.
- **ZDICT, XXH64 and the profile APIs** are not reached by these runs. Only `-D`/`-t`, `kolmo train`/`-P` and old
  formats reach them.

With `KOLMO_NATIVE=all` (section 2), what is left of the libraries in the same runs:
- **Decoding:** only XXH3.
- **Encoding:** the planner and transform probes (zstd, 0.2–5.9% of compress time; the LZ4 probe is too small to show)
  and XXH3.

`benchmark/rc3w2/native/RESULTS.md` has the native and `strong` census tables.

## 2. Prototypes

Kolmo's own coders are compiled on their own, like the block-sorting files:

| id | name | file | role | design |
|---|---|---|---|---|
| 24 | KLZMA | `src/algo/klzma.c` | LZMA2 in `ratio`; the strong option for `hc` and `eco` | adaptive binary range coder (11-bit probabilities); 12-state history; literal trees matched against the last distance; length, slot and reverse-tree distance coders; four repeat distances. Price-based optimal parse over windows of up to 4 KiB: forward dynamic programming that carries state and repeat distances per node, with a "literal then repeat" transition. Binary-tree match finder with 2- and 3-byte heads. Integer prices only, so the output is the same on every CPU. |
| 27 | khuf | `src/algo/khuf.c` | zstd's HUF in the graph leaves; the entropy stage of KLZH | canonical, length-limited Huffman codes (Moffat–Katajainen lengths, Kraft repair to 11 bits, always a complete code); four streams; least-significant-bit-first output; a branch-free 64-bit refill; a two-symbol decode table for chunks ≥ 4 KiB |
| 28 | KLZ | `src/algo/klz.c` | LZ4 in `speed` | byte-aligned sequences: a token (3 bits of literal length, 4 bits of match length, 1 bit for a 3-byte offset); 16 MiB window; hash table with LZ4-style skipping (level 1), a larger table (2), chains with lazy evaluation (3) |
| 29 | KLZH | `src/algo/klz.c` | zstd in `balanced` (and `hc`/`eco` until an optimal parse exists) | the same LZ77 with three repeat offsets. Five streams: literals; literal-length, match-length and offset codes (all khuf); extra bits. Levels 1–3: two hash tables (8- and 5-byte keys), greedy, a repeat check one byte ahead. Levels 4–9: hash chains with lazy evaluation. |
| — | khash64 | `src/algo/khash.c` | XXH3-64 in checksums and ids | NH-style 32×32→64 products over 64-byte stripes in eight lanes, with the raw word added (so a zero product still counts); the key rotated per stripe; a scramble every KiB; 128-bit multiply folds; written in generic vector C so compilers vectorise it |

All five are written for Kolmo from the published algorithms. They contain no code from zstd, LZ4, Brotli, liblzma, the
LZMA SDK or xxHash.

They stay **opt-in**: `KOLMO_NATIVE=lz,huf,lzma` (or `all`, plus `strong`) makes the encoder use them where it would
have called the library. Everything else stays the same: the same planner, the same trials, levels mapped. The encoder
writes the new id, and the frame is version 11. Without the variable, the output is byte-identical: the determinism
check passes with 122 frames. Decoders need no opt-in. `src/codecs/native.c` holds the opt-in and the codec tables.
The hooks in existing files are the codec registry, the five places that write a codec id, the leaf whitelist, and the
decoder's version gate.

### Against the library, same role (component level, `natbench`)

"size" is the native size against the library's (negative = native smaller); speeds are the native coder's against
the library's, at 1 and 4 threads. The speed, balanced and Huffman rows are from pass 2, after the decoder work (KLZ
fast path, khuf two-symbol table). The full table, with both sets, is in RESULTS.md section 2. Silesia, 1 thread:

| role | set | library → native | threads | library size | native size | size | compress | decompress |
|---|---|---|---|---|---|---|---|---|
| speed | silesia | lz4 → klz1 | 1 | 100,934,019 | 93,972,375 | -6.90% | 0.79× (386 / 487 MB/s) | 0.77× (1954 / 2530 MB/s) |
| speed | silesia | lz4 → klz2 | 1 | 100,934,019 | 90,320,404 | -10.52% | 0.69× (337 / 487 MB/s) | 0.74× (1884 / 2530 MB/s) |
| balanced | silesia | zstd1 → klzh1 | 1 | 73,302,233 | 69,843,373 | -4.72% | 0.65× (211 / 325 MB/s) | 0.94× (946 / 1005 MB/s) |
| balanced | silesia | zstd3 → klzh2 | 1 | 66,360,059 | 68,981,648 | +3.95% | 0.88× (187 / 212 MB/s) | 0.90× (799 / 888 MB/s) |
| balanced | silesia | zstd3 → klzh3 | 1 | 66,360,059 | 67,934,827 | +2.37% | 0.76× (161 / 212 MB/s) | 0.88× (784 / 888 MB/s) |
| balanced | silesia | zstd3 → klzh5 | 1 | 66,360,059 | 64,516,432 | -2.78% | 0.23× (48.0 / 212 MB/s) | 1.01× (901 / 888 MB/s) |
| huf | silesia | huf → khuf | 1 | 131,930,366 | 132,141,179 | +0.16% | 0.90× (666 / 740 MB/s) | 0.97× (1036 / 1064 MB/s) |
| leafchunks | kolmo | huf → khuf | 1 | 162,656 | 162,856 | +0.12% | 0.93× (810 / 868 MB/s) | 1.02× (1335 / 1307 MB/s) |
| eco | silesia | brotli8 → klzh7 | 1 | 57,101,231 | 61,784,605 | +8.20% | 1.60× (12.8 / 8.0 MB/s) | 3.04× (858 / 282 MB/s) |
| eco | silesia | brotli8 → klzma3 | 1 | 57,101,231 | 50,231,737 | -12.03% | 0.28× (2.2 / 8.0 MB/s) | 0.22× (61.7 / 282 MB/s) |
| hc | silesia | zstd19 → klzh8 | 1 | 53,260,623 | 60,549,563 | +13.69% | 2.79× (5.3 / 1.9 MB/s) | 1.08× (647 / 598 MB/s) |
| hc | silesia | zstd19 → klzma6 | 1 | 53,260,623 | 49,331,820 | -7.38% | 0.89× (1.7 / 1.9 MB/s) | 0.11× (66.7 / 598 MB/s) |
| ratio | silesia | lzma9e → klzma8 | 1 | 49,186,892 | 49,226,023 | +0.08% | 1.14× (1.6 / 1.4 MB/s) | 0.76× (67.7 / 89.5 MB/s) |

### Inside Kolmo (end to end, the same planner)

| set | mode | threads | `KOLMO_NATIVE` | output, library | output, native | size | compress s (library → native) | decompress s (library → native) |
|---|---|---|---|---|---|---|---|---|
| Silesia | speed | 1 | all | 99,938,346 | 93,467,859 | -6.47% | 0.69 → 0.80 (1.16×) | 0.36 → 0.33 (0.91×) |
| Silesia | speed | 4 | all | 99,938,346 | 93,467,859 | -6.47% | 0.28 → 0.31 (1.10×) | 0.18 → 0.20 (1.14×) |
| Silesia | balanced | 1 | all | 58,487,839 | 60,160,766 | +2.86% | 1.58 → 1.70 (1.08×) | 0.64 → 0.60 (0.94×) |
| Silesia | balanced | 4 | all | 58,487,839 | 60,160,766 | +2.86% | 0.60 → 0.89 (1.48×) | 0.27 → 0.28 (1.03×) |
| Silesia | eco | 1 | all | 50,773,162 | 54,704,671 | +7.74% | 21.29 → 15.64 (0.73×) | 1.21 → 0.79 (0.65×) |
| Silesia | eco | 4 | all | 50,773,162 | 54,704,671 | +7.74% | 12.14 → 9.70 (0.80×) | 0.76 → 0.52 (0.69×) |
| Silesia | eco | 1 | all,strong | 50,773,162 | 45,862,036 | -9.67% | 21.29 → 86.39 (4.06×) | 1.21 → 3.68 (3.04×) |
| Silesia | eco | 4 | all,strong | 50,773,162 | 45,862,036 | -9.67% | 12.14 → 49.96 (4.11×) | 0.76 → 2.30 (3.02×) |
| Silesia | hc | 1 | all | 47,871,778 | 53,754,879 | +12.29% | 81.65 → 38.20 (0.47×) | 0.73 → 0.78 (1.07×) |
| Silesia | hc | 4 | all | 47,871,778 | 53,754,879 | +12.29% | 44.28 → 23.67 (0.53×) | 0.52 → 0.53 (1.02×) |
| Silesia | hc | 1 | all,strong | 47,871,778 | 45,348,556 | -5.27% | 81.65 → 102.79 (1.26×) | 0.73 → 3.77 (5.13×) |
| Silesia | hc | 4 | all,strong | 47,871,778 | 45,348,556 | -5.27% | 44.28 → 53.97 (1.22×) | 0.52 → 2.23 (4.32×) |
| Silesia | ratio | 1 | all | 40,924,097 | 40,969,885 | +0.11% | 65.76 → 62.83 (0.96×) | 6.58 → 7.85 (1.19×) |
| Silesia | ratio | 4 | all | 40,924,097 | 40,969,885 | +0.11% | 40.29 → 38.07 (0.94×) | 3.16 → 3.62 (1.15×) |
| structured | speed | 1 | all | 13,888,171 | 13,060,561 | -5.96% | 0.14 → 0.16 (1.14×) | 0.07 → 0.08 (1.14×) |
| structured | speed | 4 | all | 13,888,171 | 13,060,561 | -5.96% | 0.08 → 0.07 (0.96×) | 0.05 → 0.05 (0.94×) |
| structured | balanced | 1 | all | 3,181,407 | 3,273,273 | +2.89% | 0.50 → 0.38 (0.75×) | 0.14 → 0.12 (0.85×) |
| structured | balanced | 4 | all | 3,181,407 | 3,273,273 | +2.89% | 0.21 → 0.24 (1.14×) | 0.07 → 0.08 (1.18×) |
| structured | eco | 1 | all | 2,974,328 | 3,237,564 | +8.85% | 2.04 → 1.16 (0.57×) | 0.21 → 0.22 (1.04×) |
| structured | eco | 4 | all | 2,974,328 | 3,237,564 | +8.85% | 1.18 → 0.88 (0.75×) | 0.17 → 0.15 (0.85×) |
| structured | eco | 1 | all,strong | 2,974,328 | 2,821,898 | -5.12% | 2.04 → 3.86 (1.89×) | 0.21 → 0.32 (1.49×) |
| structured | eco | 4 | all,strong | 2,974,328 | 2,821,898 | -5.12% | 1.18 → 3.26 (2.78×) | 0.17 → 0.29 (1.67×) |
| structured | hc | 1 | all | 2,913,190 | 3,151,770 | +8.19% | 4.18 → 1.28 (0.31×) | 0.16 → 0.20 (1.21×) |
| structured | hc | 4 | all | 2,913,190 | 3,151,770 | +8.19% | 3.44 → 1.04 (0.30×) | 0.15 → 0.13 (0.85×) |
| structured | hc | 1 | all,strong | 2,913,190 | 2,797,816 | -3.96% | 4.18 → 5.73 (1.37×) | 0.16 → 0.33 (2.03×) |
| structured | hc | 4 | all,strong | 2,913,190 | 2,797,816 | -3.96% | 3.44 → 5.03 (1.46×) | 0.15 → 0.29 (1.92×) |
| structured | ratio | 1 | all | 2,750,996 | 2,753,031 | +0.07% | 12.49 → 9.26 (0.74×) | 0.39 → 0.45 (1.15×) |
| structured | ratio | 4 | all | 2,750,996 | 2,753,031 | +0.07% | 10.54 → 7.35 (0.70×) | 0.30 → 0.35 (1.17×) |

How the table was measured: times are process wall times summed over the files (process start included), at 1 thread
(the census runs) and at 4 threads (`e2e.py`, best of 2). The planner is the library's, so each native coder runs
inside a plan tuned for the library it replaces.

What it shows, per mode:
- **`speed` (KLZ):** 6–6.5% smaller. Time is within the noise of process start and I/O, which dominate these totals.
- **`balanced` (KLZH):** +2.9% on both sets. Decode is equal; compress is up to 1.5× slower at 4 threads on Silesia.
- **`eco` and `hc` with KLZH:**
  - +8% to +12% larger;
  - compress 1.4–3.3× faster (`hc`'s zstd -19 search is slow);
  - decode about equal.
- **`eco` and `hc` with `strong` (KLZMA):**
  - 4–10% smaller than Brotli 8 and zstd -19;
  - compress 1.2–4× slower;
  - decode 1.5–5× slower.
- **`ratio` (KLZMA):** +0.07% to +0.11%. Compress is 4–30% faster; decode is 15–19% slower. KLZMA decodes at 0.7–0.8×
  of liblzma, and LZMA2 is 18–37% of `ratio` decode time.

## 3. Plan forward

### Per library

| library | decision | order | Kolmo's replacement | expected against the library (after the work) | effort | risk |
|---|---|---|---|---|---|---|
| xxHash (XXH3) | **replace** | 1 | khash64, AVX2 and NEON kernels through `arch/` | same collision quality (measured); 0.8–1.0× speed (today 0.43–0.55× in cache, which costs ≤ 2% of decode time) | 3–5 days, plus a format-2 frame change | low: a checksum, tested here for collisions, avalanche and corruption |
| xxHash (XXH64, zstd's copy) | **drop** | 1 | none (old formats) / khash64 (sampling hash) | — | hours | none: format 2 does not read versions 1–2 or codec 5 |
| zstd and LZ4 as planner probes | **replace** | 1 | KLZH level 1 or an entropy-plus-match estimate | plans of equal quality after retuning; probe time ≈ 1.5× today's (KLZH1 compresses at 0.65× zstd -1) or less with an estimate | 1 week | medium: changes plans in every mode; needs the benchmark sets to retune |
| liblzma | **replace** | 2 | KLZMA (codec 24) | size ±0.1% (today +0.08% Silesia, +0.31% structured); compress 1.1–1.3× (measured); decode 0.8–0.95× (today 0.70–0.83×) | 1–2 weeks | low-medium: the parse is done; the decoder needs a hand-tuned bit loop |
| LZ4 | **replace** | 3 | KLZ (codec 28) | −6% to −7% size (measured); compress 0.8–0.9×; decode ≥ 0.9× (today 0.60–0.77×) | days | low |
| zstd's HUF | **replace** | 4 | khuf (codec 27) | +0.1–0.2% size (table description); compress 0.9×; decode 1.0× on Kolmo's leaf chunks (measured) | days | low |
| zstd, levels 1–9 (`balanced`, leaves, codec 22) | **replace** | 5 | KLZH (codec 29), tuned | size within ±1% of zstd -3 (today +2.4% to +3.8% at the same speed point; −2.8% at 0.23× speed); compress 0.8–1.0×; decode 0.85–1.0× | 2–3 weeks | medium: zstd -3 is a well-tuned point; the structured set favours zstd today |
| zstd, levels 19/22 (`hc`) | **replace, but not soon at zstd's point** | 6 | KLZH with an optimal parse (or KLZMA if the owner accepts its decode) | first version within 2–4% of zstd -19 at zstd's decode speed (today +13.7% to +20.8%); KLZMA today −7% at 0.1× decode | 3–4 weeks | high: zstd's btultra parse with FSE is years of tuning |
| Brotli (`eco`) | **keep for now** | 7 | KLZH with literal contexts, or KLZMA3 | KLZH7 today +8% (decode 1.5–3×); KLZMA3 today −12% to −18% (decode 0.2×) | 4–6 weeks for context-modelled KLZH | high: Brotli's context modelling and static dictionary have no Kolmo counterpart yet |
| zstd's ZDICT, the dictionary and profile APIs | **keep for now** | 8 | a COVER-style trainer; prefix dictionaries in KLZ, KLZH and KLZMA | dictionary gains within a few percent of ZDICT | 2 weeks | medium |
| zstd's FSE | goes with zstd | — | — | — | — | — |

### Order

1. **The shared prerequisites, about 1 week.**
   - khash64 SIMD kernels: an AVX2 kernel and a NEON kernel through the `arch/` dispatch, with the C version as the
     reference.
   - The block checksum, the dictionary id and the dedup id switch to khash64 (format 2).
   - A cost probe of Kolmo's own replaces the planner's zstd and LZ4 probes: KLZH level 1 on the same samples, or an
     order-0 entropy plus match-coverage estimate. This changes plans. The determinism file is regenerated in its own
     commit, and the planner thresholds are re-tuned on the benchmark sets.
   - Without these two items, no mode is library-free.
2. **`ratio`, 1–2 weeks.** KLZMA replaces LZMA2.
   - Size is already +0.08% (Silesia) and +0.31% (structured), with faster compression.
   - Remaining: the "match, literal, repeat" transition, and Kolmo's lc/lp/pb choice per block (already passed
     through).
   - Decoder: branch-free normalisation, then an x86-64 and AArch64 bit-decode kernel. liblzma 5.6+ ships inline
     x86-64 assembly there.
   - Expected: equal size, 1.1–1.3× compress, decode 0.8–0.95× of liblzma 5.8.1.
   - With that, `ratio` is library-free: BWT, CM and the graph coder are already Kolmo's.
3. **`speed`, days.** KLZ replaces LZ4.
   - Already 6–7% smaller. Needs decode at ≥ 0.9× LZ4: the fast path is written; a 4-bit literal-length field is the
     remaining format question.
   - Compress speed is already 0.8×; a 2^12 hash table recovers part of the gap.
4. **Leaves, days.** khuf replaces HUF in codec 13.
   - +0.1–0.2% size: table description of ~130 bytes against FSE-coded weights.
   - Codec 22 becomes khuf then KLZH, or is dropped.
5. **`balanced`, 2–3 weeks.** KLZH replaces zstd at levels 1–9. Remaining gap to close:
   - KLZH at level 1 is already 4.7% smaller than zstd -1.
   - At the zstd -3 point, KLZH3 is +2.4% at 0.85× compress, and KLZH5 is −2.8% at 0.21× compress.
   - Work items: a 5-byte minimum match for the fast parse; sequence codes in the 128 KiB blocks zstd uses, so tables
     adapt locally; rANS (Kolmo's own) or tANS for the code streams; the decode loop.
   - Expected: size within ±1% of zstd -3, compress 0.8–1.0×, decode 0.85–1.0×.
6. **`hc`, 3–4 weeks.** An optimal parse for KLZH. Reuse KLZMA's binary-tree finder and dynamic programming, with
   Huffman code lengths as prices, in two passes.
   - **Honest expectation:** within 2–4% of zstd -19 at zstd's decode speed after the first version. Matching
     zstd -19/-22 (btultra2, FSE, 8+ MiB windows) is not in reach soon.
   - The alternative is KLZMA (`strong`): 4–7% smaller than zstd -19 today, but decode at 0.1× of zstd. That is a
     mode decision for the owner, not a coder decision.
7. **`eco`, open.** **Brotli quality 8 cannot be matched soon at its decode speed.**
   - KLZH7 is 8% larger: Brotli has literal context modelling, block-split entropy codes and a static dictionary.
   - KLZMA3 is 12–18% smaller, but compresses at 0.25× and decodes at 0.2×.
   - Options: (a) keep Brotli for `eco` longest; (b) redefine `eco` as KLZH with order-1 literal contexts (4–6 weeks,
     risky); (c) map `eco` to KLZMA3 and accept the slower decode.
8. **Dictionaries and profiles, 2 weeks.**
   - A COVER-style trainer replaces ZDICT, and KLZH gets a prefix dictionary.
   - Until then, `-D`/`-t`/`kolmo train` keep zstd, LZ4 and Brotli. The opt-in already leaves dictionary and profile
     paths on the libraries.

### Risks

- **Format.** Every replacement changes the bytes, and the checksum switch changes every frame. All of these belong
  together in format 2 (backward compatibility is not required). Each needs its own determinism regeneration and a
  fuzz pass over its decoder (`fuzz/`; the decoders are bounds-checked and pass corrupt-input tests under ASan/UBSan).
- **Speed regressions.** The native coders win on size where they stand today, and lose on decode speed. They should
  go default only when the decode speed of their mode is within 10%.
- **Maintenance.** About 2,000 lines of Kolmo code replace about 150,000 lines of vendored code. The libraries have
  years of fuzzing that these coders do not.
