# Reversible transforms for text and semi-structured data (pre-coder stage)

Scope note: kolmo already has a word-dictionary transform (high-ratio setting), a line/log template transform, minified-JSON key/value splitting, a typed CSV transform with scaled-integer deltas and cross-column formulas, and BWT / LZMA2 / CM back ends. These notes concentrate on what is not in that list. "pp" = percentage points of original size; "%" = relative size reduction unless stated. Percentages marked "(computed)" are my arithmetic from the cited figures, not numbers the source states.

Sources that could not be read (blocked or bot-walled, so nothing from them is used): https://scispace.com/pdf/effective-asymmetric-xml-compression-25x2kpjwx3.pdf (Skibinski's XWRT XML paper), https://www.vldb.org/pvldb/vol13/p2649-boncz.pdf (FSST), https://pmc.ncbi.nlm.nih.gov/articles/PMC7336184 (Sequence Compression Benchmark, final version). Not searched at all for lack of budget: XMill SIGMOD 2000 primary paper, Logzip, LogReducer and CLP primary papers, RFC 7932, GeCo3, MFCompress, 2bit format docs, encode.su threads.

## 1. Word and dictionary transforms (WRT, XWRT, StarNT, LIPT, drt, paq8hp, cmix/phda, capital conversion, space/EOL coding, q-grams)

### Takeaway
A static-dictionary word transform is worth roughly 8 to 10% on PPM/CM and 12 to 27% on LZ77/BWT for large English text, and the sub-techniques (capital conversion, EOL coding, space handling, q-grams, dictionary ordering) are separable small gains; the back end decides the variant, because the LZ77-tuned variant gives almost nothing on CM (0.27% on Calgary) while the PPM-tuned variant gives 4.8 to 7.9%. Dictionary ordering by frequency plus syntactic/semantic/suffix grouping is the part most specific to CM and is how paq8hp, durilca, phda9 and cmix get their text results.

### Cited Findings

WRT (Skibinski, Grabowski, Deorowicz, "Revisiting dictionary-based compression", 2005)
- Sub-techniques: capital conversion with the flag placed before the word, and a space after the flag to help PPM find longer contexts; separate flag for all-uppercase words; codewords from bytes 128-255 split into disjoint sub-alphabets (43/43/42, up to 79,550 words; later 101/9/9/9 with up to four-byte codewords); dictionary sorted by frequency over a 3 GB corpus and then by suffix within small groups; q-gram replacement using the 26 uppercase byte values freed by capital conversion; EOL coding that turns only line ends surrounded by lowercase letters into spaces; a binary-data filter (after 20% non-text characters an escape copies the next 16 bytes verbatim); surrounding words with spaces (files over 1 MB only). — [Revisiting dictionary-based compression](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- Gain versus no preprocessing, Calgary corpus, PPM-oriented WRT: gzip 6.27%, UHARC 7.43%, bzip2 6.04%, UHBC 6.66%, PPMd 4.82%, PPMonstr 4.72%, PAQ6 4.83%. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- Gain versus no preprocessing, large text files, PPM-oriented WRT: gzip 15.57%, UHARC 17.86%, bzip2 12.07%, UHBC 10.37%, PPMd 9.71%, PPMonstr 9.14%, PAQ6 7.93%. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- LZ77-oriented variant (WRT-LZ77: 189-symbol codeword alphabet split 167/22/22, most word-separating spaces removed, no q-grams, no surrounding spaces), large files: gzip 27.32%, UHARC 21.43%, bzip2 14.20%, UHBC 8.47%, PPMd 8.24%, PPMonstr 7.06%, PAQ6 4.77%. Calgary: gzip 9.87%, UHARC 7.39%, bzip2 3.29%, UHBC 3.67%, PPMd 2.70%, PPMonstr 2.16%, PAQ6 0.27%. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- book1 in bpc (none / StarNT / WRT): PAQ6 2.090 / 2.032 / 1.894; PPMonstr 2.122 / 2.072 / 1.913; UHBC 2.223 / 2.139 / 1.971; bzip2 2.420 / 2.292 / 2.131; gzip 3.250 / 2.736 / 2.682. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- Large-file averages in bpc (none / StarNT / WRT): PAQ6 1.740 / 1.688 / 1.602; PPMonstr 1.784 / 1.719 / 1.621. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- Earlier transforms, as summarised in that paper: LIPT improves PPM or BWCA by about 4-5% and gzip by under 7%; StarNT improves on LIPT by 2-5% with PPM/BWCA and about 10% with gzip; WRT improves on StarNT by 5.1 to 6.6% on large files for every back end except gzip (0.37%). — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- EOL coding gave a large gain on book1 and book2; "try shorter word" matching gave a negligible gain. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- Dictionary size sensitivity (large files): 80,000 to 60,000 words costs under 0.15%; 40,000 words about 0.4% (0.68% for gzip); 20,000 words 2.72% for gzip and 1.30% for PPMonstr; no dictionary 34.43% for gzip and 6.66% for PPMonstr. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)
- Dictionary lookup cost: minimal perfect hashing 115 ns/word and 1,029 KB memory, ternary search tree 158 ns and 7,199 KB. An 80,000-word dictionary compresses to 200.8 KB with PPMonstr order-5 and to 151.0 KB when words are stored reversed. — [same](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)

Large Text Compression Benchmark (Mahoney), enwik8 unless stated
- WRT inside PAQ: paq8f (no WRT) 18,289,559 bytes versus paq8g (WRT added back) 17,817,246 bytes at -7, which is 2.58% smaller (computed). The WRT step there splits words into upper, lower and capitalised, replaces words with 1-3 byte codes, and codes longest prefix/suffix matches of at least 6 characters. — [LTCB](https://mattmahoney.net/dc/text.html)
- Same back end (ppmonstr), different preprocessors: none 19,098,634; xml-wrt 2.0 18,625,624; xml-wrt 3.0 18,494,374; paq8hp1 dictionary 18,322,077; paq8hp3 dictionary 18,197,797; paq8hp5 dictionary 18,154,921 (18,267,556 including the 111,935-byte compressed dictionary). So a semi-dynamic XWRT dictionary gives 3.2% and the tuned static paq8hp5 dictionary 4.9% (4.4% counting the dictionary) on ppmonstr (computed). — [LTCB](https://mattmahoney.net/dc/text.html)
- Dictionary ordering history: paq8hp2 sorts its 43K two- and three-byte-code dictionary alphabetically and groups the 80 most frequent words by syntactic type; paq8hp3 groups 2-byte-code words semantically or by common suffix and sorts the 40,960 three-byte-code words by last character in reverse alphabetical order; paq8hp5's dictionary is 411,681 bytes with 44,880 words. Sizes at -8: paq8hp1 17,397,023; paq8hp2 17,223,661; paq8hp3 17,085,021; paq8hp4 16,889,237; paq8hp5 16,761,044. — [LTCB](https://mattmahoney.net/dc/text.html)
- durilca 0.5 (PPM + SSE) -m700 -o12: 19,227,202 with text preprocessing off (-t0) versus 18,520,589 with the dictionary on (-t2), 3.7% smaller (computed). Its later dictionary is "organized in a similar manner to paq8hp2-paq8hp5". — [LTCB](https://mattmahoney.net/dc/text.html)
- drt (the dictionary preprocessor from lpaq9m): dictionary lpqdict0.dic is 465,210 bytes (88,759 compressed), drt.exe 15,548 bytes. A zpaq cmax4 config gives 18,986,507 alone and 18,400,773 with drt, 3.1% smaller (computed). — [LTCB](https://mattmahoney.net/dc/text.html)
- Table rows (enwik8 / enwik9 bytes): xwrt 3.2 -l14 (lpaq6 back end) 18,679,742 / 151,171,364; drt|lpaq9m 17,964,751 / 143,943,759; paq8hp12any 16,230,028 / 132,045,026; durilca'kingsize 16,209,219 / 127,376,595; phda9 1.8 15,010,414 / 116,544,849; cmix v21 14,623,723 / 107,963,380; paq8px_v206fix1 -12L 15,849,084 / 124,696,410; ppmonstr J 19,055,092 / 157,007,383; bsc 3.25 20,786,794 / 163,884,462; xz 5.2.1 24,703,772 / 197,331,816; zstd 0.6.0 -22 25,405,601 / 215,674,670. — [LTCB](https://mattmahoney.net/dc/text.html)
- cmix uses "dictionary preprocessing based on code from paq8hp12any and paq8l"; starlit "uses the dictionary preprocessor from phda9"; paq8px "doesn't preprocess enwik8/enwik9"; paq8pxd adds "dynamic dictionary preprocessing (based on XWRT)". — [LTCB](https://mattmahoney.net/dc/text.html)

paq8px (the comparison target)
- paq8px has no external dictionary preprocessing; instead the T flag pre-trains its text models from english.dic and english.exp, which must be present at decompression. Calgary total 573,198 bytes (-12L, v213) versus 550,861 (-12LT, v210), about 3.9% smaller; Canterbury 303,634 versus 290,559, about 4.3% smaller; the two figures come from different versions. — [paq8px readme](https://gh.r-mj.com/GotthardtZ/paq8px)
- paq8px text handling that works as a model, not a transform: English/French/German stemmers and language detection (Porter-based), a UTF-8 decoder DFA, word embeddings, a "linemodel" for structured text, an XML model, a record model, and EOL / RLE / Base64 / Base85 transforms. — [paq8px readme](https://gh.r-mj.com/GotthardtZ/paq8px)

Token-id transform with frequency ordering (2026 preprint)
- Method: BPE-tokenise (cl100k_base), renumber tokens so the most frequent get the smallest ids, write ids as LEB128 varints, then compress. enwik8, size as % of original, raw to transformed: zlib-9 36.48 to 29.40; LZMA 26.38 to 24.69; zstd-22 25.27 to 24.51; bz2 29.01 to 29.07 (worse); PPMd order-16 22.83 to 23.27 (worse). — [Frequency-Ordered Tokenization](https://arxiv.org/html/2602.22958v1)
- Ablation on a 10 MB subset: BPE alone hurts zstd (-0.38 pp), LZMA (-0.26 pp) and PPMd (-1.64 pp); the frequency reordering supplies the gain (+1.73 pp zstd, +1.76 pp LZMA, +0.14 pp PPMd). — [same](https://arxiv.org/html/2602.22958v1)
- Other data, gain in pp for zlib-9 / zstd-22 / LZMA: enwik9 6.13 / 1.20 / 1.57; Shakespeare 8.42 / 2.63 / 3.01; Python code (98 KB) 5.28 / 3.78 / 4.71; Chinese Wikipedia 5.36 / 0.93 / 0.91; Arabic Wikipedia 4.78 / 1.55 / 1.33. — [same](https://arxiv.org/html/2602.22958v1)
- Side effect: the varint stream is 41.6 MB for 100 MB of text, making zstd-22 3.1x and LZMA 2.4x faster end to end on enwik8 (1.9x and 1.7x on enwik9). The authors' own WRT reimplementation gave +4.74 pp (zlib), -0.08 pp (zstd), +0.87 pp (LZMA), and they say it omits some WRT features. — [same](https://arxiv.org/html/2602.22958v1)

### Inferences
- Gains fall as the back end gets stronger, in a consistent order: LZ77 (15-27%) > BWT (10-14%) > PPM (8-10%) > CM (5-8%) on large English text; on mixed small files (Calgary) everything roughly halves. For kolmo's CM tier a realistic expectation from a tuned static dictionary is 3 to 5% (ppmonstr +4.9%, durilca +3.7%, zpaq-CM + drt +3.1%, paq8 + WRT +2.6%).
- The variants conflict rather than stack: space removal and dense codeword alphabets help LZ77 and hurt PPM/CM, while flag-before-word and space-after-flag help PPM/CM. One transform per back end family is needed, chosen by the planner.
- Since kolmo already has the word dictionary, the unexploited pieces are probably (1) dictionary ordering by frequency then syntactic/semantic/suffix group for the CM tier, (2) EOL coding for hard-wrapped text, (3) q-gram codes in freed byte values, (4) capital conversion flag placement per back end, (5) a binary-run escape filter. Each is reported as a small separate gain and they are applied together in WRT, so they stack by construction.
- The Polish-novel gap (16%) is consistent with an English-only static dictionary: XWRT's semi-dynamic (two-pass, per-file) dictionary gave 3.2% on ppmonstr versus 4.9% for a tuned static English one, so a semi-dynamic dictionary recovers roughly two thirds of the static gain without any language assumption (computed from the LTCB ppmonstr rows; one data point).
- paq8px's own English advantage on text is a model-side prior (pre-training, stemming), about 4% on small corpora, not a transform; matching it means either shipping a dictionary or doing the same pre-training.
- The token-id result confirms the same rule from a different angle: a tokeniser alone harms context-based coders, and it is the ordering of ids that pays.

### Gaps
- No measured numbers found for TWRT, for XWRT + LZMA versus XWRT + PPMd on plain text (the benchmark page lists only the lpaq6 configuration), for phda9's dictionary in isolation, or for cmix with and without its dictionary.
- The paq8hp1 to paq8hp5 sequence (3.7% total, computed) mixes dictionary re-ordering with model changes, so the share due to ordering alone is not isolated in the source.
- No source found quantifying capital conversion, space stuffing or q-grams individually; the WRT paper gives them only as part of the whole.
- No measurements found on Polish or other inflected languages with a language-specific dictionary.

## 2. Punctuation and numbers in text; field recognition (timestamps, IPs, hex, UUID, base64); front coding

### Takeaway
Typed recognition of numeric-looking tokens (timestamps, IPs, fixed-width counters) with delta and variable-byte coding is the best-documented cheap gain: in log data it adds 6 to 17% on top of already-specialised log compressors and about 66% on top of a bare general-purpose coder. Base64/Base85 decoding before coding is established practice in paq8px.

### Cited Findings
- Denum splits tokens into pure numbers, numbers with special characters (timestamps like 03:03:06, IPs like 202.133.98.6) and alphanumeric "numeric variables" (MACs, hashes). Pure numbers of 2-15 digits are grouped by (digit length, leading digit) and delta-coded; timestamps are delta-coded; IPs are merged into one integer and stored as-is; short numbers, long numbers and hashes are stored as-is; all integers use a stop-bit variable-byte code ("elastic encoding", from LogReducer); the result is tarred and passed to LZMA. — [Denum, ASE 2024](https://arxiv.org/html/2408.05760v1)
- Ablation on six Loghub sets: adding numeric token parsing to the general-purpose compressor raises compression ratio by 65.8% on average, and adding string processing (dictionary + index of the tag-substituted lines) a further 22.8%. — [Denum](https://arxiv.org/html/2408.05760v1)
- Stacking evidence: plugging Denum's numeric module in front of LogReducer raises its average ratio from 60.928 to 74.240 (+17.05%), improving 14 of 16 sets (BGL -19.3% and Spark -15.3% regress because of irregular numbering); in front of LogShrink from 74.091 to 78.490 (+5.94%), improving 14 of 16 (Mac -8.5%, OpenStack -4.3%, Proxifier -5.9% regress). The paper's abstract quotes 11.8%, which does not match its own per-tool tables. — [Denum](https://arxiv.org/html/2408.05760v1)
- DeLog keeps the number format (for example zero padding) in the token's signature so values such as "2024-01" are restored exactly, and classes tokens as keyword, short pure numeric, long pure numeric (delta + elastic), complex numeric, alphanumeric. — [DeLog](https://arxiv.org/html/2601.15084)
- CLP codes each timestamp as a millisecond delta from the previous message in a variable-length integer (most deltas take 1-2 bytes), stores integers directly, and packs decimal floats into a 32-bit custom format (1 sign bit, 25 digit bits, 3 bits digit count, 3 bits decimal-point offset) that reproduces the text exactly; values that do not fit fall back to the dictionary. The float encoding gave a 5.37% compression improvement on Spark logs and was 2-3x faster than IEEE-754 conversion. — [Uber engineering blog on CLP](https://www.uber.com/blog/reducing-logging-cost-by-two-orders-of-magnitude-using-clp)
- XWRT has a special number-encoding model and dedicated containers for dates, times, page ranges and fractional numbers, and treats URLs and e-mail addresses as dictionary words. — [XWRT home page](https://xwrt.sourceforge.net/)
- paq8px detects Base64 (since 2016) and Base85 (v207, 2022) blocks and decodes them before modelling; it also has EOL and RLE transforms (2018). — [paq8px readme](https://gh.r-mj.com/GotthardtZ/paq8px)
- Front coding applied to a sorted word list: gzip on an 80,000-word dictionary gives 293.1 KB raw and 231.2 KB with front compression on reversed words (21.1% smaller); with PPMonstr, storing words reversed gives 151.0 KB versus 200.8 KB (24.8% smaller). — [Revisiting dictionary-based compression](https://www.academia.edu/5675934/Revisiting_dictionary_based_compression)

### Inferences
- The exactness trap for all numeric field packing is formatting: leading zeros, sign, digit count, trailing zeros, case of hex digits, separators. CLP's float layout and DeLog's signature both carry the format alongside the value; any kolmo field packer needs the same (value, format-class) split, with a verbatim fallback when the round trip does not reproduce the bytes.
- Grouping numbers by (digit count, leading digit) before delta is a very cheap way to get column-like behaviour without a template parser; it is the piece most likely to add to kolmo's existing template transform, given it added 6% even to LogShrink.
- The regressions in Denum's plug-in table (BGL, Spark, Mac) show delta on numeric tokens must be selected per group by measured cost, not applied blindly.
- Base64 decoding is likely relevant to the JSON API dump gap if the dump embeds tokens, hashes or binary blobs.

### Gaps
- No published measurement found for packing UUIDs, hex strings, MAC addresses or IPv4/IPv6 to binary as a standalone step before LZ/BWT/CM; Denum explicitly stores IPs and hashes without arithmetic because it finds no useful relationship in them.
- No measured numbers found for URL/path prefix sharing (incremental coding) on real URL lists in front of LZMA/BWT/CM; the only front-coding figures found are for a sorted word list.
- No measured gain found for the paq8px Base64/Base85 transforms.
- Unverified recall, not checked in this session: Brotli's built-in static dictionary (about 13k words with 121 word transforms, RFC 7932) and zstd trained dictionaries help mainly small inputs; FSST (VLDB 2020) builds a 255-entry table of 1-8 byte symbols for short strings. RFC 7932 was not searched; the FSST paper URL was blocked.

## 3. JSON and XML

### Takeaway
For XML, a word/tag dictionary plus container separation takes gzip from 1.499 to 0.964 bpc on the Wratislavia corpus, but container separation of the XMill kind is reported to help only bounded-context coders (gzip, PPMd-class, up to 15%) and to hurt unbounded-context ones (bzip2/BWT, PPM*). For JSON, the documented wins come from storing the schema once and grouping records by schema into typed columns (uSlope: 2.34x zstd's ratio), but the production systems doing this are not byte-exact.

### Cited Findings
XML
- XWRT techniques: start tags, URLs and e-mails as dictionary words; end tags replaced by one character, optionally merged with the following EOL; `="` and `">` replaced by single characters; spaceless word model; white-space preservation; input split into containers, with dedicated containers for dates, times, pages and fractional numbers; special number encoding; semi-dynamic dictionary of frequent words. Back ends: zlib (-l1..3), LZMA (-l4..6), PPMVC (-l7..9), FastPAQ8 / lpaq6 (-l10..14). — [XWRT home page](https://xwrt.sourceforge.net/)
- Wratislavia XML corpus average: gzip alone 1.499 bpc; XWRT + gzip 0.964; XWRT + LZMA 0.830; XWRT + PPM 0.775; XWRT + FastPAQ8 0.701 (XWRT 3.1). Per file (-l2 / -l6 / -l9 / -l11): dblp 0.865 / 0.747 / 0.690 / 0.597; enwikibooks 1.742 / 1.504 / 1.481 / 1.269; enwikinews 1.597 / 1.301 / 1.202 / 1.090; lineitem 0.276 / 0.243 / 0.243 / 0.226; shakespeare 1.481 / 1.349 / 1.204 / 1.185; SwissProt 0.475 / 0.388 / 0.363 / 0.313; uwm 0.315 / 0.278 / 0.240 / 0.228. — [XWRT home page](https://xwrt.sourceforge.net/)
- XMLPPM paper conclusions: XMill improves gzip and bounded-context ppmd+ by up to 15% but worsens bzip2 and ppm*; an "encoded SAX" byte stream (tags as dictionary bytes) improves all compressors and beats bzip2 on textual XML by about 2-4%; multiplexed hierarchical modelling (separate PPM models for element structure, attributes, strings, with the enclosing element injected as context) compresses textual XML about 5% better and structured data 10-25% better than the best prior method. — [XMLPPM paper, conclusions](https://xmlppm.sourceforge.net/paper/node8.html)
- XSAQCT / SXSAQCT separate XML into an annotated structure tree and per-path data containers ("similar paths glued together"), each compressed with a back end chosen by value type. — [Dagstuhl report](https://core.ac.uk/download/62913653.pdf)
- paq8px has had a dedicated XML model since 2017 (model inside the CM, not a transform). — [paq8px readme](https://gh.r-mj.com/GotthardtZ/paq8px)

JSON
- uSlope (OSDI 2024) stores each dataset's schema structure once instead of with every record and groups records sharing a schema into one well-structured table; reported ratios 21.9:1 to 186.8:1, 2.34x the ratio of Zstandard. — [uSlope, OSDI 2024](https://www.usenix.org/conference/osdi24/presentation/wang-rui)
- clp-s (the open implementation for JSON logs) accepts only valid ndjson, has a `--structurize-arrays` option that parses arrays fully into dedicated columns, and a `--timestamp-key` option that can improve compression ratio; it does not preserve event order or time-zone information. — [clp-s docs](https://docs.yscope.com/clp/main/user-docs/core-clp-s)
- LogLite on JSON log files: XOR/RLE against a same-length recent line then LZMA beats LZMA alone by 37.7% on average and PBC + LZMA by 8.1%; with zstd it beats uSlope by 17.3% on average. — [LogLite, VLDB 2025](https://arxiv.org/html/2507.10337v1)

### Inferences
- The XMLPPM finding matches kolmo's own gap pattern: separating content into containers removes the cross-field context a CM or BWT coder would otherwise use. For the CM tier the transferable idea is the opposite of splitting: keep one stream but supply structure as context (current path / enclosing key as a context for the value model), or split only when each container gets its own model with the path injected.
- For the LZMA2 tier, container separation by path plus a tag/key dictionary is the documented win (up to 15% for gzip-class in XMLPPM's test; 36% for XWRT + gzip, which also includes the word dictionary).
- Low-risk XML items not in kolmo's list: end-tag elision (the closing name is implied by the stack; needs an exception flag for mismatches), merging end tag + EOL, `="` / `">` digraph tokens, indentation implied by depth with an exception list.
- For the JSON API dump (33% behind), the documented levers beyond key/value splitting are schema-once storage, grouping records by schema, array-of-objects to typed columns, and array structurisation. Byte exactness needs side information the production systems drop: key order per record, whitespace style, number spelling (1.0 vs 1, exponent form), string escape spelling (`é` vs raw UTF-8, `\/`), and duplicate keys. A canonical-form check with verbatim fallback per record is the usual way to stay exact.
- LogLite's same-length XOR trick is essentially a cheap positional delta against a similar record; it stacks with a back end (it beat plain LZMA by 38% on JSON) and is much simpler than schema inference, so it is a candidate fast-tier JSON-lines transform.

### Gaps
- The XWRT XML paper (Skibinski, Grabowski, Swacha, "Effective asymmetric XML compression") could not be fetched, so there are no ablation numbers per technique or direct XWRT versus XMill / XMLPPM / SCMPPM figures here.
- XMill's own published numbers were not retrieved; the "up to 15%" figure is XMLPPM's assessment of XMill.
- No measured figure found for array-of-objects to column-major transposition or key dictionaries in front of BWT or CM specifically; uSlope and clp-s report against zstd only.
- No sources found on EXI-derived ideas, JSON tiles, Sinew, or string-escape normalisation with measured compression gains.
- The clp-s page does not say whether key order or exact bytes are preserved; it lists event order and time zones as not preserved, so it is not a lossless reference.

## 4. Logs

### Takeaway
Beyond the template/variable split kolmo already has, the gains in the literature come from (1) storing variables column-wise (36 to 103% smaller archive than row-wise in LogShrink's study), (2) typed numeric handling with delta and variable-byte coding (+6 to +17% on top of the best parsers), and (3) grouping tokens by a cheap shape-and-context signature instead of by parsed template (DeLog, +9.3% over Denum). Template accuracy itself is reported not to predict compression ratio.

### Cited Findings
- Loghub, 16 sets, average compression ratio with an LZMA back end: LogReducer 60.928, LogShrink 74.091, Denum 80.568, DeLog 88.056. General-purpose averages computed from Denum's table: gzip about 24.9, LZMA about 35.1, bzip2 about 30.2, PPMd about 37.0. — [Denum](https://arxiv.org/html/2408.05760v1); [DeLog](https://arxiv.org/html/2601.15084)
- Selected per-set ratios (LZMA / PPMd / LogReducer / LogShrink / Denum / DeLog): Android 18.857 / 19.370 / 20.776 / 21.857 / 32.494 / 30.354; Apache 25.186 / 31.688 / 43.028 / 55.940 / 58.517 / 59.648; BGL 17.637 / 18.927 / 38.600 / 42.385 / 41.804 / 45.682; HDFS 13.559 / 19.155 / 22.634 / 27.319 / 25.670 / 29.013; Hadoop 36.095 / 32.110 / 52.830 / 60.091 / 78.546 / 79.626; OpenSSH 18.918 / 31.977 / 86.699 / 103.175 / 101.654 / 106.012; Thunderbird 27.309 / 33.026 / 49.185 / 48.434 / 63.824 / 68.647; Windows 202.568 / 61.083 / 342.975 / 456.301 / 481.350 / 541.569. — [Denum](https://arxiv.org/html/2408.05760v1); [DeLog](https://arxiv.org/html/2601.15084)
- LogShrink: column-oriented storage reduced archive size by 36% to 103% versus row-oriented with lzma and gzip; it finds shared delimiters by delimiter-level longest common subsequence, replaces repeated string fields by dictionary indices, stores numeric fields with steady differences as deltas with an elastic encoder, and claims 16% to 356% higher ratio than baselines on average. — [LogShrink, ICSE 2024](https://ar5iv.labs.arxiv.org/html/2309.09479)
- Speed (MB/s, 4 threads): Denum C++ 26.2, LogReducer 10.7, LogShrink about 1.5, Logzip 0.69 per the Denum paper; DeLog paper reports compression DeLog 21.4, Denum 11.4, LogReducer 5.4, LogShrink 0.95 and decompression DeLog-L 177.6, DeLog 98.2, Denum 97.4, LogReducer 3.5, LogShrink 0.48. — [Denum](https://arxiv.org/html/2408.05760v1); [DeLog](https://arxiv.org/html/2601.15084)
- Block size matters: Denum's ratio on HDFS rises from 25.670 (100K-line chunks) to 30.405 (3M-line chunks), BGL 41.804 to 44.117, Spark 59.470 to 64.581, with speed falling. — [Denum](https://arxiv.org/html/2408.05760v1)
- DeLog assigns each token a signature from intrinsic features (digit/letter content, special characters, length) plus external context (last keyword, position), groups tokens by signature, and encodes each group (delta + elastic for long numerics, dictionary for most others); it states that "higher parsing accuracy does not guarantee a better compression ratio". Ablation: adding intrinsic structure to a keyword/variable split lifts Zookeeper from 93.86 to 123.34 and HealthApp from 24.19 to 36.63. On 10 ByteDance production sets it averages 26.297 versus 19.057 for Denum; LogReducer and LogShrink failed on 3 of the 10. — [DeLog](https://arxiv.org/html/2601.15084)
- DeLog's authors say search-oriented systems (CLP, Loggrep, uSlope) often have ratios below LZMA's and that DeLog's ratio is more than 2x theirs (comparison in supplementary material not seen). — [DeLog](https://arxiv.org/html/2601.15084)
- CLP: log-type dictionary, variable dictionary, encoded messages of (log-type id, variable ids, encoded timestamp, encoded numbers); average ratio 32.20 versus gzip 16.38 in the OSDI 2021 slides. At Uber, the row-wise intermediate form + zstd is only "5-8% better" than zstd on raw logs, while the full pipeline with dictionary de-duplication and columnar zstd is 2.16x better than zstd and 2.28x better than gzip. — [CLP OSDI 2021 slides](https://usenix.org/system/files/osdi21_slides_rodrigues.pdf); [Uber engineering blog](https://www.uber.com/blog/reducing-logging-cost-by-two-orders-of-magnitude-using-clp)
- LogLite (VLDB 2025), no parsing at all: keeps the last 8 lines per line length, replaces bytes equal to the best-matching earlier line of the same length with zeros, run-length codes the zeros, then zstd or LZMA. On text logs + LZMA it beats LZMA alone by 17.1% on average but stays behind LogShrink and LogReducer (Apache size fraction: LogShrink 0.019, LogReducer 0.023, LogLite-BL 0.036); it is about 15x faster than LogShrink and 2.7x faster than LogReducer. Ablations: reverse lookup +14.2% ratio, keeping original characters instead of XOR values +20.9%, byte-aligned RLE +28.8%; a larger window adds 28.8% on text and 16.6% on JSON. — [LogLite](https://arxiv.org/html/2507.10337v1)

### Inferences
- Windows is the outlier that shows where plain PPMd/CM-class models fail on logs (PPMd 61 versus LZMA 203 versus parsers 343-542): long-range exact repetition. Elsewhere PPMd beats LZMA, and the parsers beat both by 1.5 to 3x. kolmo's 20% log gap to paq8px is small relative to those factors, so the missing piece is probably variable-side modelling, not the template split.
- Ordered by evidence and cost, the not-yet-present candidates are: column-wise variable streams per template slot; delta + variable-byte for monotone numeric slots chosen by measured cost; timestamp field delta with format kept separately; per-slot variable dictionary; grouping stray numbers by (length, leading digit); larger blocks.
- The row-wise CLP figure (5-8% over zstd) versus the columnar one (2.16x) is the clearest published evidence that the template split alone gives little and the column layout gives most of the gain for LZ-class back ends.
- All log-paper numbers use LZMA (or zstd) as back end. None reports results with a CM or BWT back end, so how much of the columnar gain survives under kolmo's CM tier is untested in the literature.

### Gaps
- LogShrink's own per-dataset table, ablations and speed were truncated in the fetch; its figures here are as re-reported by Denum and DeLog.
- Logzip (ASE 2019), LogReducer (FAST 2021), CLP (OSDI 2021 paper), Loggrep and uSlope full papers were not read; no per-technique ablations from them.
- Denum's and DeLog's ablation figures are partly only in charts; exact per-set values were not available as text.
- No source found that tests these transforms in front of PPM/CM or BWT instead of LZMA.
- Whether each tool is byte-exact on irregular whitespace and malformed lines was not verified; DeLog and Denum state lossless operation, LogShrink and LogReducer failed on some sets.

## 5. CSV and tabular text

### Takeaway
Nothing was found in this session that measures CSV-specific text transforms beyond what kolmo already has; the nearest evidence is from log and XML work, where column-wise layout, per-column dictionary and delta coding account for most of the gain with LZ-class back ends.

### Cited Findings
- Column-oriented storage of parsed fields cut archive size by 36% to 103% versus row-oriented with lzma and gzip. — [LogShrink](https://ar5iv.labs.arxiv.org/html/2309.09479)
- Per-group delta on numeric fields has to be selective: applying Denum's numeric handling regressed 2 of 16 sets under LogReducer and 3 of 16 under LogShrink, by 4 to 19%. — [Denum](https://arxiv.org/html/2408.05760v1)
- XWRT gives dates, times and fractional numbers their own containers and encodings; on the table-like lineitem XML file the result is 0.276 bpc with gzip and 0.226 with FastPAQ8. — [XWRT home page](https://xwrt.sourceforge.net/)
- Decimal values can be packed exactly as (sign, digits, digit count, decimal offset) so the text is reproduced without storing it. — [Uber engineering blog](https://www.uber.com/blog/reducing-logging-cost-by-two-orders-of-magnitude-using-clp)

### Inferences
- Candidate additions not in kolmo's list, all reversible with an exception path: date/time columns to epoch integer + format descriptor; low-cardinality columns to a dictionary index stream; shared-prefix columns front-coded against the previous row; empty/null bitmap per column; quote stripping with a per-cell "was quoted" exception list; column reordering in the serialised output so correlated columns are adjacent (a permutation of columns is a fixed-size header, unlike row sorting).
- Row sorting needs a stored permutation of about log2(n!) bits, so it only pays when the sort saves more than roughly log2(n) bits per row; no published figure was found for this trade.

### Gaps
- No measured numbers found for column reordering, enum dictionaries, null bitmaps, unquoting or shared-prefix coding on CSV in front of LZ77/BWT/CM. This topic needs kolmo's own measurements or a separate search of the columnar-database literature (e.g. BtrBlocks, FSST, OpenZL), which was out of reach here.

## 6. Unicode and non-English text

### Takeaway
Feeding a context coder code points instead of UTF-8 bytes is worth about 6% on PPM and 12% on an LZW-class coder for non-Latin scripts, and nothing on ASCII; SCSU/BOCU-1 re-encoding in front of gzip/bzip2 is reported as not worthwhile.

### Cited Findings
- Decoding UTF-8 into tokens (code point, invalid byte, end of file), reversible for any byte sequence, with a Polya-tree base model for unseen symbols: PPM improves by 0.120 bits/byte on average over byte PPM on UTF-8 files (about 6.1%), LZW by 0.347 bits/byte (about 12.2%). — [Making compression algorithms for Unicode text](https://arxiv.org/pdf/1701.04047)
- Per language, bits per byte, byte PPM versus code-point PPM versus PPMII: Chinese 3.787 / 3.383 / 3.563; Russian 1.846 / 1.772 / 1.895; Arabic 1.104 / 0.928 / 1.268; Japanese 1.684 / 1.585 / 1.736. Code-point PPM beats PPMII on 9 of 12 UTF-8 files; on ASCII and binary files the token variants match the byte variants and PPMII wins. — [same](https://arxiv.org/pdf/1701.04047)
- The same paper cites Atkin and Stansifer: gzip and bzip2 outperform SCSU and BOCU-1 on longer texts, and SCSU preprocessing gives gzip only a slight gain. — [same](https://arxiv.org/pdf/1701.04047)
- Token-id transform on non-English text, gain in pp for zlib-9 / zstd-22 / LZMA: Chinese Wikipedia 5.36 / 0.93 / 0.91; Arabic Wikipedia 4.78 / 1.55 / 1.33. — [Frequency-Ordered Tokenization](https://arxiv.org/html/2602.22958v1)
- paq8px handles UTF-8 inside the model (a decoder DFA) and ships stemmers only for English, French and German. — [paq8px readme](https://gh.r-mj.com/GotthardtZ/paq8px)

### Inferences
- For Polish (Latin script with about 9 two-byte letters), the code-point effect should be smaller than the Russian figure (4% on PPM), since most characters stay one byte. A cheap equivalent is remapping the handful of frequent two-byte sequences to unused single bytes (with escape), which also makes word boundaries and case folding work across diacritics.
- Case folding with a case mask on Polish requires Unicode-aware lower-casing for the diacritic letters; byte-level capital conversion misses them.
- paq8px has no Polish stemmer or dictionary, so kolmo's 16% gap there is not from language-specific priors; it is more likely generic model strength (word/indirect contexts), which a semi-dynamic dictionary only partly addresses.

### Gaps
- No Polish, Greek or other Latin-with-diacritics measurements were found.
- No data found on UTF-16 detection and conversion gains, or on separating scripts into streams.

## 7. Bioinformatics text (FASTA / FASTQ)

### Takeaway
The cheap, well-evidenced recipe is NAF's: split headers, sequence, lengths, case mask and qualities into separate streams, pack nucleotides (4 bits per base in NAF, which covers IUPAC codes), and use a general coder; this is about 1.4x more compact than gzip -9 on small genomes and 5.7x on repetitive collections, with decompression 35 to 78 times faster than the context-model tools it nearly matches.

### Cited Findings
- NAF splits input into headers, sequence, mask (for soft-masked lowercase) and qualities, concatenates all sequences and stores their lengths separately, converts sequence to a 4-bit code (two nucleotides per byte, representing IUPAC codes such as N, Y, R natively), and compresses every stream with zstd. — [NAF paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC6761962)
- On the human genome GRCh38 (3.3 GB) at strongest settings, NAF's ratio is "close to DELIMINATE and slightly behind MFCompress", all three well ahead of gzip, bzip2, brotli, zstd and xz, and NAF decompresses 35x and 78x faster than DELIMINATE and MFCompress; NAF's fastest mode still beats gzip -9 on ratio. — [NAF paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC6761962)
- Sequence Compression Benchmark preprint: on small genomes naf-22 is 1.4x more compact than gzip-9, with mfc-3, naf-22 and zpaq-5 the suggested archival choices; on repetitive DNA/RNA sets naf-22 is 5.7x more compact than gzip-9; on protein, naf-22, lzturbo and xz reach 1.8x, 1.8x and 1.7x gzip-9. — [Sequence Compression Benchmark preprint](https://biorxiv.org/content/biorxiv/early/2019/05/20/642553.full-text.pdf)

### Inferences
- Since NAF with plain zstd lands close to order-k context-model tools (MFCompress) on a human genome, most of the available gain over a general coder is in the layout: line-length removal (one stored width plus exceptions), case mask as run lengths, N runs as an exception list, and bit packing. Those are the "basic and cheap" tier.
- 2-bit packing (ACGT only, exceptions listed separately) should be tighter than NAF's 4-bit code before entropy coding, and an order-k model over 2-bit symbols is the next tier (roughly 1.6-1.9 bits/base is the commonly cited range; unverified recall). Reverse-complement matching and mixed order-k models (GeCo, cmix) are the expensive tier.
- For FASTQ the same stream split applies: identifiers tokenised field by field with delta against the previous identifier, bases packed, qualities in their own stream with a context model. This is the standard design but no source for it was read in this session.

### Gaps
- No exact compression ratios for 2bit, MFCompress, GeCo2/3, cmix or XM were obtained; the benchmark's final paper was behind a bot check and its tables were not seen. No bits-per-base figures are sourced here.
- No FASTQ sources (fqzcomp, DSRC, Spring, quality-score modelling) were read; nothing measured on identifier tokenisation.
- No measurement found isolating line-length removal, case mask or N-run lists individually.

## 8. Source code

### Takeaway
Almost no measured evidence was found for code-specific reversible transforms; the single data point is the token-id transform, which gave its largest LZ-class gains on a small Python sample (3.8 pp for zstd-22, 4.7 pp for LZMA).

### Cited Findings
- Python code (98 KB), frequency-ordered BPE token ids as varints: +5.28 pp zlib-9, +3.78 pp zstd-22, +4.71 pp LZMA; gains shrink as files grow (zlib: 10.5 pp at 100 KB of English versus about 7.3 pp at 100 MB). — [Frequency-Ordered Tokenization](https://arxiv.org/html/2602.22958v1)
- The same method lowers PPMd's result on English text by 0.44 pp, with BPE alone costing 1.64 pp. — [same](https://arxiv.org/html/2602.22958v1)
- paq8px added a "linemodel" for structured text (v207) and shifted contexts for text (v208), i.e. it treats column position within a line as model context instead of transforming the text. — [paq8px readme](https://gh.r-mj.com/GotthardtZ/paq8px)

### Inferences
- For LZ-class tiers on small source files, an identifier/keyword dictionary with frequency-ordered codes is plausible at a few percent; for PPM/CM it is likely neutral or negative unless ids are ordered to preserve context, mirroring the text-dictionary results.
- Indentation coding (leading whitespace as a delta of depth against the previous line, with an exception escape for mixed tabs/spaces) is cheap and exactly reversible, but no measurement was found.

### Gaps
- No published numbers found for identifier dictionaries, lexer-based tokenisation or indentation coding in front of LZ77/BWT/PPM/CM. The Python figure comes from one 98 KB file in a preprint and is weak evidence.
