# Entropy-coding and modelling micro-gains (the "last few percent" after transforms)

Scope note for the writer: all numbers below are copied from the cited page and carry that page's CPU, corpus and version. Percentages marked "(derived)" are my arithmetic on the cited figures. Items I know from background but could not open a source for in this session are placed under "Gaps" and labelled "recalled, unverified" - do not present them as sourced. Several primary sources were unreachable (listed in the last Gaps block).

Reference cost line for kolmo's fast mode (lzbench, silesia.tar, AMD EPYC 9554, single thread): zstd 1.5.6 -2 = 344 MB/s compress, 1246 MB/s decompress, 69,503,444 bytes; zstd -5 = 125 / 1197 MB/s, 63,040,310 bytes (level 3 itself is not in the table) - [lzbench README](https://github.com/inikep/lzbench).

---

## 1. Order-1 / order-2 static entropy coding at high speed (rANS, tANS/FSE, Huffman, SIMD interleaving)

### Takeaway
Static order-1 rANS is the best-documented cheap step up from kolmo's order-0 rANS: on two real byte streams it cut size 5.9% and 19.6% versus order-0 while still decoding at 366-478 MB/s scalar and about 1,000 MB/s with AVX2 32-way interleaving. Against that, replacing Huffman with an arithmetic-precision coder is only worth about 0.4-0.9% on order-0 text statistics, so the gain is in the context, not in the coder.

### Cited Findings
- Order-0 vs order-1 static rANS, same file, Intel i5-4570 3.2 GHz, gcc 6.2. "Q40" = 94,602,182 bytes of DNA quality values with about 40 distinct symbols; "Q8" = 73,124,567 bytes with about 8 distinct symbols - [rans_static benchmark tables (fork of Bonfield's rans_static)](https://github.com/da0ka/rans_static):

  | Coder | Q40 order-0 | Q40 order-1 | Q8 order-0 | Q8 order-1 |
  |---|---|---|---|---|
  | rANS_static4x8 size | 53,688,173 | 43,165,948 | 16,847,000 | 15,849,499 |
  | rANS_static4x8 enc / dec MB/s | 326.9 / 673.2 | 159.6 / 366.3 | 300.0 / 681.2 | 197.5 / 430.0 |
  | rANS_static4x16 size | 53,689,007 | 43,381,697 | 16,847,245 | 15,866,708 |
  | rANS_static4x16 enc / dec MB/s | 302.4 / 659.2 | 180.5 / 425.6 | 369.3 / 664.5 | 231.8 / 478.1 |
  | r32x16b_avx2 size | 53,696,527 | 43,391,024 | 16,853,107 | 15,872,840 |
  | r32x16b_avx2 enc / dec MB/s | 525.1 / 1415.0 | 234.5 / 985.9 | 534.7 / 1430.6 | 237.1 / 1039.4 |
  | arith_static (static arithmetic) size | 53,713,263 | 43,399,150 | 16,852,961 | 15,858,338 |
  | arith_static enc / dec MB/s | 246.0 / 144.0 | 127.5 / 97.7 | 240.2 / 146.5 | 175.9 / 131.3 |

- Derived from that table: order-1 vs order-0 = -19.6% (Q40, 4x8), -19.2% (Q40, 4x16), -5.9% (Q8, 4x8). Order-1 decode costs about 30-45% of order-0 scalar decode speed (659 to 426 MB/s on Q40 4x16; 665 to 478 MB/s on Q8). Encode speed roughly halves (327 to 160 MB/s). 16-bit-renormalising 4x16 loses about 0.5% to 4x8 at order-1 on Q40 (43,381,697 vs 43,165,948) - (derived) [rans_static](https://github.com/da0ka/rans_static).
- 32-way interleaved AVX2 rANS more than doubles scalar decode: 1,415-1,431 MB/s order-0 and 986-1,039 MB/s order-1 on a 2013 Haswell desktop; static arithmetic coding with the same model decodes at 98-147 MB/s, i.e. 4-10x slower for a size difference under 0.6% - [rans_static](https://github.com/da0ka/rans_static).
- CRAM 3.1's "rANS Nx16" codec is order-0/order-1 rANS with 4 or 32 interleaved states plus optional wrappers applied in a fixed order: STRIPE (every Nth byte to one of N separately coded streams, "for structured data" such as 32-bit integers), NOSZ, CAT (store raw when "the overheads of compressing are too high" for very short data), N32, ORDER, RLE, PACK - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- CRAM 3.1 order-1 tables: frequencies normalised to a power of two with 10- or 12-bit precision; the spec says the 1024 maximum "offers better cache performance for the large Order-1 tables and usually has minimal impact on compression ratio"; the order-1 frequency table may itself be compressed with order-0 rANS; in order-1 each interleaved state codes its own contiguous region so states are independent; 32-way "is not recommended on smaller blocks of data" because of size and initialisation costs - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- CRAM PACK: if the alphabet has at most 16 symbols, pack 2, 4 or 8 symbols per byte before entropy coding (1 symbol = constant stream, nothing stored; 2 symbols = 1 bit; 3-4 = 2 bits; 5-16 = 4 bits). CRAM RLE: run lengths are kept for a chosen subset of symbols only and stored as a separate metadata stream that can itself be rANS-compressed - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- CRAM 3.1 also has an adaptive arithmetic coder (order-0/order-1, frequency increment 16, halve when total exceeds 2^16-17, symbols bubble toward the front so frequent symbols are found first; optional RLE with run lengths split into parts of at most 3 coded in contexts keyed by the symbol; optional PACK, STRIPE, external bzip2) - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- End-to-end effect of that codec set: CRAM 3.1 is 7-16% smaller than CRAM 3.0 at equivalent profiles on HiSeq 2000 data "with similar speed except for small and archive decoding", and the 3.1 "normal" profile is 16% smaller than 3.0 on NovaSeq data (Xeon E5-2660, 12 threads). The move from 4 to 32 rANS states was made to permit SIMD - [Bonfield, CRAM 3.1 preprint](https://www.biorxiv.org/content/10.1101/2021.09.15.460485v1.full.pdf).
- Huffman vs FSE vs rANS vs range coder on book1 (768,771 bytes, order-0 entropy about 435,042.6 bytes): ryg_rans 435,604-435,626 bytes; 24-bit range coder 436,186; FSE 437,232; length-limited Huffman 438,768 (12-bit max code) / 438,505 (13-bit) / 438,400 (15-bit) / 438,385 (16-bit); FSE's Huffman mode 439,150 - [sserangecoding README](https://github.com/richgel999/sserangecoding).
- Derived: on this order-0 text distribution Huffman costs +0.64% (15-bit) to +0.73% (12-bit limit) over rANS, and FSE/tANS costs +0.37% (sizes include differing headers) - (derived) [sserangecoding README](https://github.com/richgel999/sserangecoding).
- Speeds quoted in the same README (mixed machines and MB vs MiB, so approximate): ryg_rans single-stream decode 207-223 MiB/s, 2-way interleaved 318-332 MB/s, SSE4.1 SIMD interleaved 513-566 MB/s; FSE 331 / 370 MB/s; FSE's Huffman 649 / 801 MB/s; vectorised 16-stream range decoder 552-619 MiB/s on Ice Lake (2.3-2.6 cycles/byte), 64-stream AVX2 port about 1,373 MiB/s. Range coding encode depends on fast division and the vectorised variant encodes at only 51.9 MiB/s. Licence: Unlicense / public domain - [sserangecoding README](https://github.com/richgel999/sserangecoding).
- Huff0 design: 4 independent segments decoded in parallel, single-, double- and quad-symbol decode tables selected by heuristics on table-build time, raw decode speed and quantised compression ratio; about 800-900 MB/s on 32 KB blocks (2015 hardware); single-symbol tables win at poor ratios and for 8 KB blocks, double-symbol in the main useful range, quad-symbol slightly ahead at 128 KB blocks. BSD-licensed in the FiniteStateEntropy repository - [Collet, Huffman revisited part 5](https://fastcompression.blogspot.com/2015/10/huffman-revisited-part-5-combining.html).
- The zstd 1.5.5 fast Huffman decoder depends on manually unrolled inner loops ("compilers don't consistently unroll the inner loops, which destroys performance"); restoring them changed kernel read time by -2.4% to -7.0% on Btrfs levels 1-9 and -9.1% on SquashFS level 1 (whole-read time, not the Huffman loop alone) - [LKML zstd Huffman backport patch](https://lkml.iu.edu/hypermail/linux/kernel/2311.2/04348.html).
- BitKnit's coder is a 2-way interleaved rANS with 15-bit probabilities, a 2^16 lower bound and 16-bit renormalisation over 64 KiB blocks; the encoder buffers symbols and codes them in reverse; raw bits are bypass-coded; an optional branchless renormalisation path exists; blocks fall back to stored - [rygorous BitKnit rANS gist](https://gist.github.com/rygorous/b434cf2916be5c9573796b5f96671cbe).
- A 2026 paper replaces the range-coder division by a shift by holding the adaptive total at a fixed power of two (ring buffer of the last 2^12 symbols: increment the new symbol, decrement the oldest) and decodes through a lookup table; removing the division saves about 20 clocks per symbol (40-50% of encode time, static uniform case). No comparison with rANS/FSE/Huffman and no code or licence are given - [Range-Coder with fast Adaptation and Table-Based Decoding, arXiv 2601.06120](https://arxiv.org/html/2601.06120v1).

### Inferences
- For kolmo's fast mode, scalar order-1 rANS (about 430-480 MB/s on 2013 hardware) sits well below zstd's roughly 1,200 MB/s Silesia decode, so it fits the zstd -3 budget only (a) with the 32-way AVX2/NEON variant, or (b) on streams that are a minority of output bytes. It is a clear archive-mode addition.
- The gain is strongly stream-dependent: about 20% where successive symbols are correlated and the alphabet is about 40 symbols, about 6% for an 8-symbol alphabet. Byte planes that have already been delta-coded or shuffled will sit nearer the low end or below; kolmo must select per chunk and include the table cost. The CRAM design choices that keep order-1 cheap are directly reusable: 10-bit frequencies for cache fit, compressing the table with order-0, a raw fallback for short inputs.
- Order-1 table cost matters at kolmo's 128 KiB chunk size: a 256x256 table is large relative to the chunk unless sparse-coded, which is why CRAM compresses the table and uses 10-bit precision. Larger chunks or a shared table across chunks would be needed for small alphabets to pay.
- Swapping Huffman for tANS/rANS at order-0 is worth under 1% on text-like distributions (more on very skewed ones, where Huffman's 1-bit floor bites); kolmo already has both, so nothing new there.
- PACK-before-entropy and per-symbol-subset RLE are two cheap pre-entropy steps kolmo's list does not include; they differ from the bit-packing kolmo dropped because the packed bytes are still entropy coded (packing is used to shrink the symbol count and lengthen contexts, not as the final coder).

### Gaps
- TurboRC / Turbo-Range-Coder and TurboANX benchmark tables: the repository page was blocked and a mirror page had no data. No numbers obtained.
- htscodecs' own BENCHMARKS.md (rANS-Nx16 with NEON/AVX512, arith, fqzcomp, tok3 speeds) was blocked; the table above comes from the older rans_static lineage. Licence of htscodecs not confirmed in session (recalled, unverified: BSD-3-Clause, with ryg_rans-derived parts public domain).
- Fabian Giesen's "rANS in practice" and "interleaved entropy coders" posts could not be opened; ryg_rans licence (recalled, unverified: public domain/CC0).
- No source found in session for order-2 static coding at speed, order-1 Huffman, "recoil" parallel rANS, or the 2022-2024 Huff0 assembly-loop speed figures (recalled, unverified: zstd 1.5.x release notes report large Huffman decode gains from hand-written x86-64 assembly loops).
- CRAM 3.1 paper Tables 1-3 (per-codec sizes and MB/s) were not extractable; the PMC copy returned a captcha.

---

## 2. Cheap adaptivity: adaptive binary/nibble coders, counters, SSE/APM, two-input mixing, "CM-lite" coders

### Takeaway
Adaptive nibble-alphabet rANS (Oodle LZNA) is the documented midpoint between static coding and bitwise coding: about 2.5x LZMA's decode speed at slightly better ratio, but that still means 75-87 MB/s, an archive-mode speed. Light CM back ends (kanzi levels 6-9, bcm, zpaq mid) buy 5-15% over BWT+simple coder or LZMA at 1-30 MB/s symmetric speed; none is near fast-mode speed.

### Cited Findings
- LZNA ("LZ-nibbled-ANS", Oodle 1.45): replaces LZMA's binary arithmetic coder with rANS and codes nibbles rather than bits ("up to 4x fewer coding operations"); probability updates are shift-based with a power-of-two total over a 16-symbol alphabet, by mixing cumulative distributions (credited to Giesen); needs SSE2 for full speed. Results: lzt99 (24,700,820 bytes) LZNA -z6 9,154,248 at 75.65 MB/s decode vs LZMA 9,329,925 at 29.47 MB/s vs LZHAM 10,140,761 at 93.74 MB/s; baby_robot_shell (58,788,904) LZNA 12,933,907 at 86.84 MB/s vs LZMA 13,525,659 at 42.49 MB/s vs LZHAM 15,594,877 at 100.97 MB/s. "Usually beats LZMA on binary data and is slightly worse on text" - [cbloom, Oodle LZNA](https://cbloomrants.blogspot.com/2015/05/05-09-15-oodle-lzna.html).
- Derived: LZNA is 1.9% (lzt99) and 4.4% (baby_robot_shell) smaller than LZMA at 2.0-2.6x the decode speed - (derived) [cbloom, Oodle LZNA](https://cbloomrants.blogspot.com/2015/05/05-09-15-oodle-lzna.html).
- Bloom's assessment: "LZMA is very Pareto" - swapping its statistical coder for ANS (faster) or CM (more compression) only moves along the trade-off; no modification is a clear win on all of ratio, speed and memory - [cbloom, some LZMA notes](https://cbloomrants.blogspot.com/2014/06/06-12-14-some-lzma-notes.html).
- Adaptive vs static arithmetic at the same order: CRAM's static arithmetic and static rANS differ by under 0.6% in size, so the choice is about speed - [rans_static](https://github.com/da0ka/rans_static).
- Adaptation-rate tuning matters by several percent. Bitwise adaptive counter with a count LIMIT, Calgary corpus: order-0 1,859,853 (limit 4) / 1,704,809 (16) / 1,680,784 (64, best) / 1,726,280 (1020); order-1 1,408,402 (4) / 1,304,204 (32, best) / 1,399,988 (1020); order-2 1,153,855 (4) / 1,089,660 (16, best) / 1,150,737 (1020). Stationary data (digits of pi) keeps improving to the largest limit - [Mahoney, Data Compression Explained](https://mattmahoney.net/dc/dce.html).
- Derived from the same table: order-0 to order-1 = -22.4%, order-1 to order-2 = -16.5% on Calgary with adaptive bitwise models at their best limits (raw text-like data, no prior transform) - (derived) [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Direct (probability) vs indirect (bit-history state to probability) models, Calgary: indirect is worse at order-0 (1,716,974 vs 1,680,784) but better from order-1 up: order-1 1,289,769 vs 1,304,204 (-1.1%), order-2 1,048,050 vs 1,089,660 (-3.8%), order-3 964,942 vs 1,017,354 (-5.2%). The state table has 219 states in one byte per context - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Logistic mixing: p = squash(sum of w_i * stretch(p_i)), update w_i += rate * (y - p) * stretch(p_i), rate about 0.01; choosing the weight set by a small context such as order-0 "can improve compression". An ISSE (2-input mixer keyed by a bit history, weights initialised to (1.0, 0.0)) is the cheapest mixing unit in ZPAQ. SSE/APM maps (context, quantised stretch(p)) to a refined probability with interpolation between 32 buckets; no isolated gain figure is given - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Small CM configurations on calgary.tar: lpaq1 682,512 bytes, 8 s, 195 MB; ZPAQ mid.cfg (8 components: order 0-5 ICM/ISSE chain, order-7 match model, order-1 mixer) 699,474 bytes, 8 s, 111 MB; ZPAQ max.cfg (22 components) 644,433 bytes, 20 s, 246 MB; versus ppmd J -o16 754,243 in 2 s, bsc 1.0.3 774,397 (1.1 s / 0.62 s), 7zip LZMA 824,573 (1.5 s / 0.16 s), bzip2 860,097 (0.60 s / 0.39 s) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Binary arithmetic coding cost alone, order-0 model on enwik8, 2.0 GHz T3200: 130 ns/byte compress, 112 ns/byte decompress (about 8-9 MB/s on that 2008 laptop); the binary-ANS ("ABC") variants were slower there (173-238 ns/byte decode). lpaq1 as a whole: 3,594 ns/byte decode - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- kanzi (Apache-2.0), silesia.tar 211,957,760 bytes, Ryzen 9950X, multi-threaded, encode ms / decode ms / size: -l 5 1814 / 685 / 53,863,205; -l 6 2300 / 840 / 49,472,110; -l 7 1840 / 1558 / 47,330,431; -l 8 5928 / 6340 / 42,962,926; -l 9 14693 / 15192 / 41,520,677. Same table: zstd 1.6.0 -19 11090 / 23 / 52,830,213; xz 5.8.1 -9 43611 / 931 / 48,802,580; bsc 3.3.11 -T16 1201 / 698 / 47,900,848; bzip3 1.5.1 -j16 2348 / 2218 / 47,260,281; bzip2 -9 8223 / 3453 / 54,588,597 - [kanzi README](https://github.com/flanglet/kanzi).
- kanzi single-thread (lzbench, silesia.tar, EPYC 9554), compress / decompress MB/s, size: -5 21.0 / 50.3, 54,050,463; -6 15.8 / 29.6, 49,517,568; -7 10.2 / 15.2, 47,308,205; -8 3.45 / 3.29, 43,247,149; -9 2.44 / 2.35, 41,807,652. Same table: bsc 3.3.5 -m0 -e1 16.6 / 24.1, 49,143,366; xz -9 2.57 / 123, 48,745,306; lzma 24.09 -9 4.01 / 93.0, 48,674,973; ppmd8 -4 13.2 / 12.0, 51,241,932; lzham -d26 -1 3.08 / 309, 54,740,589 - [lzbench README](https://github.com/inikep/lzbench).
- kanzi on enwik8 (Apple M3), encode ms / decode ms / size: -l 5 1546 / 812 / 26,521,279; -l 6 2165 / 1171 / 24,076,777; -l 7 2831 / 1850 / 22,817,366; -l 8 7397 / 7508 / 21,181,998; -l 9 10414 / 10675 / 20,035,687 - [kanzi README](https://github.com/flanglet/kanzi).
- Large Text Compression Benchmark, enwik8 / enwik9 bytes, compress / decompress ns per byte, memory: bsc 3.25 -b1000 -e2 20,786,794 / 163,884,462, 23 / 8, 5000 MB; bcm 2.03 -b1000x 20,738,630 / 163,646,387, 63 / 34, 4096 MB; bzip3 -b 511 20,749,611 / 169,990,721, 175 / 146, 3700 MB; kanzi TPAQX 19,098,186 / 161,690,495, 490 / 480, 3100 MB; zcm 0.93 -m8 19,572,089 / 159,135,549, 421 / 411; ccmx 1.30 20,857,925 / 174,142,092; nanozip 0.09a -cc 18,594,163 / 148,545,179, 1149 / 1141, 32 GB; ppmd J1 -o10 -m256 21,388,296 / 183,964,915, 880 / 895, 256 MB; 7zip PPMd o=10 mem=1630m 21,197,559 / 178,965,454, 503 / 546; xz 5.2.1 -9e dict=1GiB lc=4 pb=0 24,703,772 / 197,331,816, 5876 / 20; zstd 0.6.0 -22 25,405,601 / 215,674,670, 701 / 2.2; lzham -d29 -x 25,002,070 / 202,237,199, 1096 / 6.6; lzpm 0.11 (ROLZ) 26,501,542 / 229,083,971, 15395 / 57; bzip2 -9 29,008,736 / 253,977,839, 379 / 129 (timings are from different machines and years) - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).

### Inferences
- The speed ladder for one coded symbol stream, from the cited figures: static rANS order-0/1 (400-1,400 MB/s) > adaptive nibble rANS inside an LZ (LZNA, 75-90 MB/s whole-codec) > LZMA binary range coder (30-125 MB/s whole-codec) > BWT+adaptive coder (14-50 MB/s) > light CM (2-15 MB/s). Only the first rung is a fast-mode candidate; adaptive nibble models belong in a "balanced" tier.
- kolmo's CM coder (about 30 models) is in the kanzi -8/-9, lpaq/zpaq-max class. The cited data suggest a missing middle tier: an order-0-to-2 chain with ISSE-style 2-input mixing (zpaq "mid" style, 8 components) reached 699,474 on calgary.tar in the same time lpaq1 took for 682,512, i.e. small CMs are within 2.5% of lpaq1 but 7% better than PPMd -o16 and 15% better than LZMA on that corpus.
- Counter-rate selection is a cheap self-tuning knob for any adaptive coder kolmo ships: the Calgary table shows 1-7% between plausible limits, and the best limit falls as order rises. Two-rate counters averaged or mixed are the usual way to avoid choosing (no measured figure found; see Gaps).
- Indirect (state-machine) models only pay at order 1 and above, and then by 1-5%; for order-0 byte planes direct counters are both smaller and faster.

### Gaps
- No source opened for BitKnit's model internals and speed, LZNA's exact adaptation constants, or Oodle's later statements on why LZNA/BitKnit were retired in favour of Kraken/Leviathan static coding (recalled, unverified: BitKnit used adaptive 15-bit multi-symbol models updated by mixing toward a target CDF, two rates in LZNA, SSE2 nibble update).
- No isolated measurement found of SSE/APM gain as an add-on to a simple coder, or of two-speed counters vs single-rate (recalled, unverified practitioner lore: an APM stage on an order-1/2 coder gives roughly 1-3%; dual-rate counters about 1-2%).
- lpaq1, ccm, csc rows in LTCB lie in a part of the 680 KB page that was not retrieved. lzham's "LZMA-like with semi-adaptive Huffman" design and lzpm/"LZP + order-2 CM" figures beyond the LTCB row were not sourced.
- kanzi's per-level transform/entropy composition was not on the fetched page (recalled, unverified: L5 = BWT + RANK + ZRLT + ANS0; L6 = BWT + SRT + ZRLT + FPAQ; L7 = LZP + BWT + CM; L8 = TPAQ; L9 = TPAQX, each with TEXT/UTF pre-transforms).

---

## 3. PPMd (var.H / PPMd7, var.I / PPMd8) as a text back end

### Takeaway
PPMd is not a clear win over a modern BWT coder in the sources found: on enwik8/9 and on Silesia, bsc is both smaller and several times faster than PPMd, and PPMd decodes as slowly as it encodes. Its documented niche is small-to-medium text with modest memory, where PPMd -o16 beats bzip2 and LZMA by 9-12% on the Calgary corpus.

### Cited Findings
- lzbench, silesia.tar, single thread: ppmd8 24.09 -4 = 13.2 MB/s compress, 12.0 MB/s decompress, 51,241,932 bytes; bsc 3.3.5 -m0 -e1 = 16.6 / 24.1 MB/s, 49,143,366 bytes; lzma 24.09 -9 = 4.01 / 93.0 MB/s, 48,674,973 bytes; bzip2 -9 = 13.1 / 37.5 MB/s, 54,572,811 bytes - [lzbench README](https://github.com/inikep/lzbench).
- enwik8 / enwik9: ppmd J1 -o10 -m256 21,388,296 / 183,964,915 at 880 / 895 ns/byte in 256 MB; 7zip PPMd o=10 with 1,630 MB 21,197,559 / 178,965,454 at 503 / 546 ns/byte; bsc -b1000 -e2 20,786,794 / 163,884,462 at 23 / 8 ns/byte with 5,000 MB; xz -9e 1 GiB dictionary 24,703,772 / 197,331,816 - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Derived: on enwik9, single-block BWT (bsc) is 8.4% smaller than 7zip PPMd and 10.9% smaller than ppmd J at 256 MB; PPMd is 9.3% smaller than xz on enwik9 - (derived) [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Calgary (calgary.tar, time in seconds compress / decompress): ppmd J -o16 -m256 754,243 (1.4 / 1.4); ppmd J order-4 804,992 (0.57 / 0.67); ppms J -o6 773,141 (0.93 / 0.98); ppmonstr J 669,497 (8.5 / 8.1); bsc 1.0.3 774,397 (1.1 / 0.62); bzip2 -9 860,097 (0.60 / 0.39); 7zip LZMA 824,573 (1.5 / 0.16) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Lineage and memory behaviour: ppmd variant I (2002) is used in 7-Zip, FreeArc, WinZip, StuffIt; WinRAR uses older variant H; variant J (2006) compresses slightly better than I. When the context tree fills memory it is discarded and rebuilt; -r1 prunes instead, which compresses better but is slower. ppmonstr is the slower, stronger variant (674,704 vs 754,243 on calgary.tar at 8 s vs 2 s). SEE in ppmd distinguishes binary contexts (13-bit direct context), "nm" and "m" contexts - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Higher PPM order does not keep helping: ppmonstr J on enwik8 gave 19,062,555 (order 16), 19,084,964 (order 32), 19,098,634 (order 64) - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Structured text (XML): in a study of XML compressors, apart from gzip-based tools the spread between best and worst average ratio was "less than 5%"; PPM-based schemes (XMillPPM, XMLPPM, SCMPPM) gave the best ratios but were "much slower than the other compressors"; "none of the XML conscious compressors has achieved an outstanding compression ratio over its back-end", with 5% the best case - [Sakr, An Experimental Investigation of XML Compression Tools, arXiv 0806.0075](https://arxiv.org/pdf/0806.0075).
- Dictionary preprocessing helps both BWT and LZ77 "by forcing splits on word boundaries"; Mahoney attributes the dominance of dictionary pre-processing among top large-text results partly to memory limits. Preprocessed (paq8hp dictionary) input fed to ppmonstr -o64 gave enwik8 totals of about 18.27-18.40 MB vs 19.06 MB raw at order 16 - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html); [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Derived: a word-dictionary transform improved ppmonstr on enwik8 by roughly 3.5-4% (19,062,555 to about 18.3-18.4 MB) - (derived) [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).

### Inferences
- For kolmo's archive mode, PPMd is worth adding only as a per-stream candidate for small and medium text columns where BWT blocks are short (PPMd adapts within kilobytes; BWT needs block length). On large homogeneous text the cited figures favour libbsc, which kolmo already has, on ratio, speed and decode asymmetry.
- "PPMd on token streams after a word transform beats BWT" is not supported by anything retrieved. The only relevant evidence is that a dictionary transform improves PPM by about 4% and "helps both BWT and LZ77"; no head-to-head after the same transform was found.
- PPMd decode speed (about 12 MB/s on Silesia) rules it out of the fast and probably the balanced mode.

### Gaps
- No benchmark found comparing PPMd, BWT and LZMA specifically on JSON or log data. The XML study's numeric tables are in figures that could not be read and it predates LZMA/zstd/bsc comparisons.
- Shkarin's PPMII paper was not retrieved. Public-domain status of PPMd (stated in the task brief) was not re-verified in session (recalled, unverified: Shkarin's code is public domain; 7-Zip's Ppmd7/Ppmd8 C re-implementation by Igor Pavlov is public domain).
- PPMd memory per order for small inputs, and PPMd7 vs PPMd8 speed difference, not sourced.

---

## 4. BWT post-stage choices

### Takeaway
What follows the BWT is worth 5-10% of output: replacing bzip2's MTF + RLE + Huffman with a context-modelled or QLFC-style adaptive coder took calgary from 828,347 to 785,672 bytes (-5.2%), and dropping RLE in favour of an order 0-1 ISSE chain on raw BWT output saved 6.9% on enwik8. Among modern BWT coders (bsc, bcm, bzip3, bwtmix, kanzi -7) the spread is under 2% on text, so libbsc already captures most of it.

### Cited Findings
- bzip2 pipeline: RLE of runs of 4-255, BWT, MTF, RUNA/RUNB zero-run coding, Huffman with 2-6 tables switched every 50 symbols (the original bzip used arithmetic coding, dropped for patent reasons). BBB: order-0 indirect model followed by 6 SSE stages directly on BWT output. Calgary 14 files: bzip2 -9 828,347 (0.68 s / 0.42 s); bbb 785,672 (10.33 s / 1.46 s fast mode) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- ZPAQ-based BWT back ends at 32 MB blocks: "bwtrle1" (RLE then order-0 ICM) enwik8 24,091,153 / enwik9 210,224,876; "bwt2" (no RLE, order 0-1 ICM-ISSE chain) 22,440,353 / 195,887,789; compress time rose from roughly 264-313 to 446-514 ns/byte - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- BWT family on calgary.tar (compress s / decompress s): nanozip 0.07a -cc 743,351 (1.4 / 0.53); bsc 1.0.3 774,397 (1.1 / 0.62); bcm 0.10 783,884 (1.4 / 1.1); m03 785,541 (2.3 / 1.8); sbc 787,007 (1.6 / 0.72); dark 787,653; uhbc 791,446; bbb 798,705 (10.3 / 3.8); GRZipII (LZP+BWT) 794,255 (0.84 / 0.70); bzip2 -9 860,097 (0.60 / 0.39) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Derived: bsc is 10.0% smaller than bzip2 on calgary.tar at under 2x the time; the best BWT coder (nanozip) is 4.0% smaller than bsc - (derived) [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Large-block text: bwtmix 20,608,793 (enwik8) at 1794 / 690 ns/byte; bcm 20,738,630 at 63 / 34; bzip3 -b511 20,749,611 at 175 / 146; bsc -e2 20,786,794 at 23 / 8; bbb 20,847,290 at 4524 / 2619; bzip2 -9 (900 KB blocks) 29,008,736. On enwik9: bcm 163,646,387, bsc 163,884,462, bbb 164,032,650, bwtmix 167,852,106, bzip3 169,990,721 (bzip3's block is capped at 511 MiB, the others ran 1 GB blocks) - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Derived: with whole-file blocks the gap from the fastest (bsc -e2) to the strongest post-stage (bwtmix) is 0.86% on enwik8 for a 78x compress-time increase; bzip2's 900 KB blocks plus Huffman cost about 40% against any of them - (derived) [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Block size: "use the largest block possible", but separate different data types into independent blocks, because BWT suits stationary sources (sorted word lists compress poorly). A non-BWT illustration of block size: zpaq fast model on enwik8 went from 28,176,221 (1 MB blocks) to 24,837,482 (100 MB blocks) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html); [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- bzip3: BWT (libsais) + an "order-0 context mixing" entropy coder + an LZP/RLE pre-pass ("RLE with Lempel Ziv+Prediction"); about 17 MiB/s compression and 23 MiB/s decompression per thread (clang 13, x64), "heavily dependent on the compiler". Licence LGPL-3.0-only for the core; libsais and the LZP code Apache-2.0 - [bzip3 README](https://github.com/kspalaiologos/bzip3).
- bzip3 vs others on mixed data: Silesia bzip3 47,260,281 vs bsc 47,900,848 (bzip3 1.3% smaller, decode 2218 ms vs 698 ms multi-threaded) vs kanzi -l 7 47,330,431 - [kanzi README](https://github.com/flanglet/kanzi).
- bsc sort-transform alternatives (single thread, silesia.tar): -m0 (BWT) -e1 49,143,366 at 16.6 / 24.1 MB/s; -m4 (ST4) -e1 50,626,974 at 31.2 / 14.3 MB/s; -m5 (ST5) -e1 49,522,504 at 28.8 / 13.3 MB/s - [lzbench README](https://github.com/inikep/lzbench).
- Derived: ST5 costs 0.8% in size for about 1.7x faster compression but slower decompression than full BWT - (derived) [lzbench README](https://github.com/inikep/lzbench).
- libbsc: Apache-2.0; memory about 16 MB + 5x block size per parallel block; optional CUDA sort - [libbsc repository](https://github.com/IlyaGrebnov/libbsc).
- Inversion frequencies vs MTF: Arnavut (DCC 2000) reports IF "better compression gain than the recency ranking (MTF coder) for almost all the test data files"; Arnavut (2004) reports IF better on large files, images and DNA; IF needs more side information so is less attractive for small inputs. Deorowicz (2002, "Second step algorithms in the Burrows-Wheeler compression algorithm", introducing weighted frequency count) reached 2.25 bps on the Calgary corpus - [data-compression.info, Inversion Frequencies](https://www.data-compression.info/Algorithms/IF/index.html).
- LZP as a BWT pre-pass serves both ratio and sort speed; Mahoney lists LZP/LZ77 preprocessing among fixes for slow sorting of highly redundant data. Bijective BWT (BWTS) changes size by only a few bytes (book1 with fpaq0p 244,106 vs 244,096) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).

### Inferences
- kolmo's libbsc leaf is already within about 1% of the best BWT second stages on text. The remaining BWT-side levers are operational: (1) block = whole stream, never mixed types; (2) LZP pre-pass on or off per stream (bsc exposes it); (3) -e2 adaptive vs -e1 static QLFC as a ratio/speed option; (4) ST4/ST5 as a faster-encode option. None is new code.
- A bzip3/bcm-style "CM directly on BWT output" replacement for QLFC is worth roughly 0-1.3% in the cited comparisons and is 2-3x slower to decode; low priority.
- MTF-family choice (MTF vs IF vs distance coding vs WFC) is a pre-2005 question; every current BWT compressor in the tables uses QLFC or direct modelling.

### Gaps
- bsc's -e0/-e1/-e2 definitions and their measured size/speed differences were not on any retrieved page; LTCB lists only -e2 and lzbench only -e1 (recalled, unverified: -e1 static QLFC, -e2 adaptive QLFC, with -e2 about 1-3% smaller and slower; newer versions add a fast -e0).
- Abel's and Deorowicz's per-stage bps tables (MTF vs IF vs DC vs WFC) were not retrieved beyond the 2.25 bps headline. No measured figure for segmentation of BWT blocks or for LZP on/off in bsc.

---

## 5. LZ-family modelling gains

### Takeaway
The one hard number retrieved for parameter tuning is LZMA position-bit tuning on binary game data: lc0:lp2:pb3 was 1.84% smaller than default -mx9, free at decode time. LZMA's matched-literal coding is described by Bloom as a form of exclusion that works "significantly better" than the fuller alternative he tried and substitutes for much of an optimal parser's benefit.

### Cited Findings
- "Game testset" totals: LZMA -mx9 d26:lc0:lp2:pb3 58,884,562 vs LZMA -mx9 default 59,987,629 vs LZHAM -mx9 62,621,098 vs zip -9 88,436,013 (raw 167,495,105); Bloom states his best LZMA settings for binary data were lc=0, lp=2, pb=2 - [cbloom, Oodle LZNA](https://cbloomrants.blogspot.com/2015/05/05-09-15-oodle-lzna.html).
- Derived: position-bit tuning = -1.84% on that set; LZMA (tuned) is 6.0% smaller than LZHAM and 33% smaller than deflate there - (derived) [cbloom, Oodle LZNA](https://cbloomrants.blogspot.com/2015/05/05-09-15-oodle-lzna.html).
- For text the opposite direction is used: the LTCB xz entry runs lc=4, pb=0 with a 1 GiB dictionary (24,703,772 on enwik8) - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Parameter definitions: lc = number of high bits of the previous byte used as literal context (default 3); lp = low bits of position used in the literal context (default 0); pb = low bits of position used in the match/literal and length state (default 2); LZMA2 requires lc + lp <= 4 and pb <= 4. 11-bit probabilities; state machine of 12 states; last four distances kept for LONGREP[0-3] plus SHORTREP (a 1-byte rep0 match); distance slots of 14 and above code middle bits at fixed probability and the low 4 bits through a modelled reverse bit-tree ("align" bits) - [Wikipedia, LZMA](https://en.wikipedia.org/wiki/LZMA).
- Literal after a match: the byte at the last-used distance (the "match byte") selects contexts for the literal's bits while they agree. Wikipedia states the literal is not XORed and calls the XOR description incorrect; Mahoney's text describes it as XOR with the predicted byte; Bloom treats it as exclusion ("the next symbol cannot be the one that followed in the match") - [Wikipedia, LZMA](https://en.wikipedia.org/wiki/LZMA); described differently in [Mahoney, DCE](https://mattmahoney.net/dc/dce.html) and [cbloom, LZMA notes](https://cbloomrants.blogspot.com/2014/06/06-12-14-some-lzma-notes.html).
- Bloom's experiments on that coder: LZMA uses 3x256 probabilities for the matched literal instead of the 9x256 a full binary exclusion would need, and the reduced form "appears to work significantly better"; intermediate schemes using more exclude bits "all hurt". Literal-after-match exclusion with a simple flexible parse gives "almost identical compression" to no exclusion with a full optimal parse, the latter being "much much slower to encode" - [cbloom, LZMA notes](https://cbloomrants.blogspot.com/2014/06/06-12-14-some-lzma-notes.html).
- LZMA's other modelled fields: literals use order-1 plus low position bits; match length is context for the offset slot; rep offsets are held in a move-to-front queue; optimal parsing uses look-ahead so a shorter match or literal can be chosen when it enables a cheaper later match - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- BitKnit's parser cost notes show rep matches of length 2 are admitted (symbols per byte estimated 1.0 for literals and length-2 rep matches, 1.333 for length 3, 1.25 for length 4) - [rygorous BitKnit gist](https://gist.github.com/rygorous/b434cf2916be5c9573796b5f96671cbe).
- zstd advanced parameters (man page 1.5.7): minMatch - "Larger search lengths usually decrease compression ratio but improve decompression speed"; targetLength - for btopt/btultra/btultra2 "A larger targetLength usually improves compression ratio but decreases compression speed", reversed for the fast strategy; windowLog - larger "usually improves compression ratio" at memory cost for both sides; LDM - larger ldmHashLog "usually improve[s] compression ratio at the expense of more memory", ldmMinMatch "Larger/very small values usually decrease compression ratio", ldmHashRateLog larger is faster but far from default lowers ratio; overlapLog larger improves ratio and lowers speed (multithreaded only); --target-compressed-block-size "can decrease compression speed by up to ~10% at level 1"; dictionary training "greatly" improves small files similar to the samples, and only the first 128 KiB of large samples is used - [zstd(1) man page](https://man.archlinux.org/man/zstd.1.en.txt).
- Size steps available inside zstd on silesia.tar (single thread): -11 58,262,299 at 34.4 MB/s; -15 57,168,834 at 8.36 MB/s; -18 53,329,873 at 3.79 MB/s; -22 52,333,880 at 2.08 MB/s; decode 1,073-1,369 MB/s throughout - [lzbench README](https://github.com/inikep/lzbench).
- Derived: the jump from level 15 to 18 (where zstd moves to its optimal-parser strategies with price estimates) is 6.7% for 2.2x compress time and no decode cost; 18 to 22 is a further 1.9% - (derived) [lzbench README](https://github.com/inikep/lzbench).
- Decode-speed classes of LZ + entropy designs, single thread on silesia.tar: zstd 1,073-1,369 MB/s; libdeflate 860-919; brotli 352-477; zlib 323-348; lzham 233-309; xz 98-127; lzma 74-93; fast-lzma2 76-90 - [lzbench README](https://github.com/inikep/lzbench).
- Filters change which LZ back end wins: with shuffle + byte-delta on float data, "Zstd's ratio [improves] a lot" while LZ4 and BloscLZ improve "a little"; chunk sizes of 64 KB lost "maybe too large" a ratio, 1 MB judged a good balance, 4 MB lost practically nothing - [Aras Pranckevicius, Float Compression 8: Blosc](https://aras-p.info/blog/2023/03/02/Float-Compression-8-Blosc/).

### Inferences
- LZMA lc/lp/pb selection is a near-free archive-mode gain for kolmo's LZMA2 leaf: decode cost is unchanged and the one measured data point is about 1.8% on binary data. Candidate set suggested by the cited settings: default (3,0,2); (0,2,2) and (0,2,3) for 4- and 8-byte records; (4,0,0) for text. After kolmo's byte-plane split the record alignment is removed, so pb/lp should fall back to 0 on plane streams and stay relevant only on un-split record streams.
- For zstd leaves the cheap knobs are minMatch (raise to 5-7 on noisy numeric planes to stop spurious short matches; no measured figure found), targetLength, and windowLog/LDM on long streams. All are encode-side choices with no decode penalty.
- The matched-literal idea (context or subtraction by the byte at rep0) is the most transferable LZ modelling trick for a home-grown kolmo LZ: Bloom's result that it substitutes for most of an optimal parser's gain makes it attractive for the balanced tier.

### Gaps
- No measured gains found for: zstd minMatch/targetLength on numeric data; zstd block splitting and literal-compression mode; brotli literal context modes and static dictionary; Oodle "sub"/"lamsub"/"o1" literal modes (search returned nothing usable); LZMA fb/nice-length; rep-offset count (rep0 only vs rep0-3); offset low-bit alignment modelling in isolation; length/offset joint coding. These remain practitioner lore here (recalled, unverified: Oodle Kraken/Leviathan choose per chunk between raw literals and literal-minus-match-byte; Leviathan adds order-1 and position-context literal modes; zstd dictionaries help mainly under about 100 KB inputs).
- The two LZMA descriptions conflict on whether the matched literal is XORed; the LZMA SDK source was not consulted to settle it.

---

## 6. Parameter self-tuning at encode time

### Takeaway
The retrieved evidence supports trial-based selection of a handful of LZMA position-bit settings and of entropy order (0 vs 1) per stream; the gains on record are about 2% for LZMA parameters and 6-20% for order selection, paid only at encode time. No source measuring sample-based (partial) trials against full trials was found.

### Cited Findings
- Hand-tuned LZMA parameters beat defaults by 1.84% on a binary game-data set (58,884,562 vs 59,987,629) - [cbloom, Oodle LZNA](https://cbloomrants.blogspot.com/2015/05/05-09-15-oodle-lzna.html).
- Benchmark submitters routinely hand-pick these: xz with lc=4, pb=0 for enwik; ppmd order and memory chosen per file (durilca's best PPM order varied from 12 to 24 with memory and file) - [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- Huff0 selects among single-, double- and quad-symbol decoders automatically from table-build time, decode speed and compression ratio heuristics - [Collet, Huffman revisited part 5](https://fastcompression.blogspot.com/2015/10/huffman-revisited-part-5-combining.html).
- CRAM 3.1 codecs carry per-block flag bytes (order, RLE, PACK, STRIPE, CAT) so that the encoder can choose the combination per block, with a raw fallback for short blocks - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- Precomp recovers a deflate stream's parameters by trying 81 zlib option combinations, an example of brute-force parameter search being accepted in archive tools - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Adaptive-counter rate has a data-dependent optimum (limit 64 at order-0, 32 at order-1, 16 at order-2 on Calgary; the largest limit on stationary data) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).

### Inferences
- Order-0 vs order-1 static coding can be decided without trial coding: both sizes are computable from the order-0 and order-1 histograms (sum of -log2 p plus table cost), one pass over the chunk. That makes order selection cheap enough for fast mode even if the order-1 decoder itself is reserved for streams where it pays.
- For LZMA, a 3-4 candidate trial on a prefix sample is the natural design; with 7-Zip/xz encode speeds of 3-5 MB/s at high levels, full trials multiply an already dominant cost, so sampling or a fast-level proxy is needed. No published measurement of how well a sample predicts the best setting was found.

### Gaps
- No source found on xz preset internals, 7-Zip -mpb usage statistics, or optimiser tools (ECT, zopfli-style, "lzma-optimizer") with cost-versus-gain figures.
- CRAM's actual encoder heuristics (how htscodecs decides which flag combinations to try, and its trial budget) were not retrieved (recalled, unverified: htslib trials several methods on early blocks then re-trials periodically, with the candidate set depending on the compression profile).

---

## 7. Bit-level and small-alphabet tricks for numeric planes

### Takeaway
The best-documented cheap tricks for mostly-zero or small-alphabet planes are the CRAM 3.1 wrappers around rANS: PACK (alphabet of 16 or fewer packed 2-8 symbols per byte before entropy coding), RLE restricted to chosen symbols with run lengths in a separate entropy-coded stream, and CAT (raw) for tiny blocks. The spec gives no isolated gain figures; the aggregate effect of the new codec set is the 7-16% CRAM 3.0 to 3.1 reduction.

### Cited Findings
- PACK: 1 symbol = nothing stored; 2 symbols = 8 per byte; 3-4 symbols = 4 per byte; 5-16 symbols = 2 per byte; applied before the entropy stage so the entropy coder sees fewer, denser symbols; more than 16 symbols is not allowed - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- RLE in rANS Nx16: a list of symbols that carry run lengths (zero entries means all symbols), run lengths stored as a separate metadata stream that may itself be compressed; literals then go through order-0 or order-1 rANS - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- RLE in the adaptive coder: each symbol has its own run-length model of 4 values; runs are split into parts of at most 3, a part equal to 3 meaning "continue"; contexts are the symbol (first part), then two shared continuation contexts; example "ABBCCCCDDDDD" becomes A<0> B<1> C<3,0> D<3,1> - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- STRIPE: N-byte values split into N byte-position streams each coded with its own choice (the spec's example uses RLE on the first three streams of 32-bit integers and order-0 on the last); may be combined only with PACK and NOSZ - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- The paper's description of PACK in use: quality values quantised to 4 distinct values are packed 4 per byte before coding - [Bonfield, CRAM 3.1 preprint](https://www.biorxiv.org/content/10.1101/2021.09.15.460485v1.full.pdf).
- bzip2's zero-run stage: after MTF, runs of zeros are coded with two symbols RUNA/RUNB (bijective base-2 run lengths) before Huffman - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- Name tokeniser (tok3) as a model for string columns: each name is split into tokens against a chosen earlier name; token types include STRING, CHAR, DIGITS, DIGITS0 (leading zeros with stored width), DELTA and DELTA0 (0-255 increment over the previous name's numeric token), MATCH (same token as before), DUP (whole name repeated), NOP, END; each (token position, type) pair forms its own byte stream coded with rANS Nx16 or the adaptive coder; a stream that is all MATCH is dropped; a stream identical to an earlier stream is stored as a reference - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- fqzcomp quality model as a template for a cheap context-mixed-by-hashing numeric model: one 16-bit context built from the previous few symbols (qbits, shifted), position in record, a running count of changes so far, and a stored selector; lookup tables map each component before packing; 2^16 adaptive models - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs).
- Simple predictive filters before a weak order-0 coder: lena.bmp 786,486 raw-coded, 511,316 after one delta pass, 645,634 after two, 768,154 after three (over-differencing hurts); PNG 474,573 - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).

### Inferences
- For kolmo's top (mostly-zero) byte planes the cheapest untested candidates are: (a) per-symbol-subset RLE with zero as the only run symbol, runs to a side stream, literals to order-0 rANS; (b) PACK when a plane has 16 or fewer distinct values, followed by order-1 rANS, since packing turns an order-1 byte context into an order-2/4/8 symbol context at no decode cost beyond an unpack loop; (c) constant-plane elision (nsym = 1). All decode at memcpy-class or rANS-class speed, so they are fast-mode eligible.
- PACK + order-1 is the principled reason "bit-packing" might still pay in kolmo even though bit-packing as a final coder was dropped.
- The tok3 "stream is all MATCH so drop it" and "stream duplicates an earlier stream" checks are near-free additions to any columnar splitter.

### Gaps
- No source retrieved for: Elias-Fano on sorted integer sequences; sparse-plane bitmaps with packed non-zeros; nibble-split Huffman; exception lists (PFOR-style); adaptive Golomb with context (JPEG-LS/LOCO-I, FLAC) versus order-0/1 entropy coding of the same residuals. No numbers can be given for these.
- No isolated ablation of PACK, RLE or STRIPE was found; the per-codec tables in the CRAM 3.1 paper were not extractable.

---

## 8. Evidence on which gains survive stacking after delta / shuffle / dictionary transforms

### Takeaway
Direct published ablations are scarce. What exists points one way: after a structure-removing transform the choice of back end still matters, the stronger modelling back end gains more from the transform, and context order-1 keeps paying on residual-like streams with small alphabets (6-20% on quality values), while over-applied transforms (double delta) lose more than any coder upgrade returns.

### Cited Findings
- After shuffle + byte-delta on float arrays, zstd's ratio improves "a lot" while LZ4 and BloscLZ improve "a little"; shuffle + XOR-delta stayed under ratio 2.0 on every test - [Aras Pranckevicius, Float Compression 8: Blosc](https://aras-p.info/blog/2023/03/02/Float-Compression-8-Blosc/).
- Streams that are already the output of domain transforms (CRAM quality-value and other per-field series) still gain 5.9% (8-symbol alphabet) and 19.6% (40-symbol alphabet) from order-1 over order-0 - (derived) [rans_static](https://github.com/da0ka/rans_static).
- CRAM applies its transforms in a fixed stack - STRIPE, then per-stripe RLE and/or PACK, then order-0/1 rANS - and the whole 3.1 codec set yields 7-16% over CRAM 3.0, which had order-0/1 rANS 4x8, gzip, bzip2 and lzma only - [CRAM codec specification 3.1](https://cmdcolin.github.io/hts-specs-md/CRAMcodecs); [Bonfield, CRAM 3.1 preprint](https://www.biorxiv.org/content/10.1101/2021.09.15.460485v1.full.pdf).
- XML-aware front ends improve on their back end by at most about 5%, and the ranking of results follows the back end (PPM-based best, gzip-based worst) - [Sakr, arXiv 0806.0075](https://arxiv.org/pdf/0806.0075).
- Dictionary/word transforms help BWT and LZ77 and improved PPM by roughly 4% on enwik8 (derived), i.e. the transform and a strong model stack rather than cancel - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html); [Mahoney, LTCB](https://mattmahoney.net/dc/text.html).
- A second delta pass turned a 35% gain into an 18% gain on image data under an order-0 coder (511,316 to 645,634 bytes) - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).
- BWT needs type-homogeneous blocks: "separate data types into independent blocks", which is what a columnar/plane split provides - [Mahoney, DCE](https://mattmahoney.net/dc/dce.html).

### Inferences
- Ranked by evidence strength for kolmo (my ranking, from the cited figures):
  1. Order-1 static rANS with histogram-based order selection, AVX2/NEON 32-way: 6-20% on correlated small-alphabet streams; about 1,000 MB/s SIMD decode; archive and balanced modes certainly, fast mode per-stream.
  2. PACK / symbol-subset RLE / constant elision wrappers ahead of rANS: no isolated numbers, near-zero decode cost.
  3. LZMA lc/lp/pb candidate selection: about 1.8% on binary records, zero decode cost; relevant mainly on un-split record streams.
  4. A mid-weight CM tier (order 0-2 chain with 2-input mixing and optional SSE): the cited small CMs are 7-15% ahead of PPMd/LZMA on Calgary at about 10 MB/s-class speed.
  5. zstd encode-side parameters (minMatch, targetLength, LDM/window): direction documented, magnitudes not.
  6. PPMd as an extra candidate for short text columns: evidence is mixed-to-negative against libbsc on large inputs.
  7. Alternative BWT post-stages: under about 1.3% over libbsc.
- The stacking question for kolmo's own transforms can only be settled by kolmo's corpus; the published record does not contain an ablation of entropy order or adaptivity specifically after delta + byte-shuffle.

### Gaps
- No encode.su thread or paper was retrieved that measures order-1 or adaptive coding specifically on byte-shuffled, delta-coded numeric planes.
- Sources that could not be read in this session (blocked by the fetch permission layer, a captcha, or truncation), so the report writer knows what was not checked: github.com/powturbo/Turbo-Range-Coder; github.com/flanglet/kanzi-cpp; fgiesen.wordpress.com "rANS in practice"; github.com/samtools/htscodecs BENCHMARKS.md and LICENSE.md; the PMC copy of the CRAM 3.1 paper (captcha; the bioRxiv preprint was used, without its tables); the libbsc README body; the last third of the CRAM codec specification; most of the 680 KB LTCB page (only the first 200 KB was read); the charts in the maskray.me 2025 benchmark post and the Blosc post (numbers are in images).
