# Engine experiments

Research towards one own compression engine in place of the library coders. Nothing here is part of the product.

## Experiment 1 (11 Nov 2026): re-code what zstd's matches leave over

Question: with zstd's own match decisions kept, how much smaller does the output get if the leftover bytes
(literals) and the match descriptions (sequences) are coded with context, and what does that cost at decode?

Method: `exp1.c` takes the level-N parse from `ZSTD_generateSequences`, replaces the literal sections of the real
zstd frame with each scheme's bytes (Huffman streams built with zstd's own coder, table reuse across blocks as zstd
does), and estimates sequence coding by entropy (the estimate reproduces zstd's real sequence bytes within 1%).
Decode cost comes from one common sequence executor. Cloud VM, one thread, best of five. 12 Silesia files and 13
structured files; `exp1_*.csv` hold every number.

| Scheme (level-3 parse) | Silesia vs zstd -3 | Structured set | Decode cost |
|---|---|---|---|
| Literals split by previous-byte class, 8 or 16 Huffman streams | -2.8% | -3.0% | +30% (a load chain per literal) |
| Literals split by position in a detected record | -3.0% | -3.6% | +11% |
| Literal minus the byte at the last match offset | -1.2% | -0.2% | within noise |
| Best static literal scheme per file | -3.9% | -4.0% | |
| ... plus context-split sequence tables | -5.7% | -5.2% | not measured |
| Adaptive binary mix (order 1, order 2, match byte, position, field) | -10.4% | -4.9% | 17 MB/s of literals: about 8x zstd's decode time |
| ... plus adaptive sequences | -12.5% | -6.5% | |
| kolmo balanced today (transforms + zstd 3), for reference | -11.6% | -15.7% | |

Cheaper parses (Silesia, against zstd -3's 66.14 MB):

| Parse | zstd itself | Static contexts, all | Static, decode-cheap only | Adaptive mix |
|---|---|---|---|---|
| level 3 | 66.14 MB | 62.38 (-5.7%) | 62.95 (-4.8%) | 57.84 (-12.5%) |
| level 2 | 69.33 (+4.8%) | 64.10 (-3.1%) | 65.35 (-1.2%) | 57.44 (-13.1%) |
| level 1 | 73.23 (+10.7%) | 65.51 (-0.9%) | 68.54 (+3.6%) | 56.20 (-15.0%) |

Findings:

1. A level-2 parse with static context coding is 1 to 3% smaller than zstd -3, at a search cost zstd pays about 30%
   less for. A level-1 parse with all static contexts matches zstd -3's size. The size side of the bet holds.
2. The speed side is not shown. Previous-byte contexts cost a dependent load per literal (+30% decode on the
   level-3 parse, +70 to 120% on cheaper parses, which leave more literals). Positional contexts and the offset
   subtraction are cheap. The encode cost of the new entropy stage was not measured.
3. On text, literals are only 8 to 11% of zstd -3's output (dickens: 0.44 M literals against 1.22 M sequences), so
   literal coding moves text by under 1%. Text needs a different way to describe or find matches.
4. Contexts on the raw parse give far less than kolmo's transforms where a transform applies (sao: 87% of zstd -3
   against the transform's 42%), and help where no transform applies (mozilla -7%, ooffice -7%, osdb -5%).
5. Adaptive mixing is worth 10 to 15% but decodes about eight times slower than zstd: a ratio-tier tool only.

Prior art: clustered order-1 Huffman literals (brotli's context map; Charles Bloom's notes), literals minus the
byte at the last offset (Oodle's "sub" literals), binary context coding of literals (LZMA). Nothing in experiment 1
is new on its own; what is untested elsewhere, as far as we know, is driving these contexts from kolmo's structure
planner. OpenZL is the nearest system (a graph of codecs with zstd-class back ends).

Next (experiment 1b): a real encoder and decoder for the decode-cheap variant on a level-1/2-cost parse, timed end
to end against zstd -3, and the same coding applied to kolmo's transformed streams instead of raw input.
