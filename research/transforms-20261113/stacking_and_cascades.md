# Stacking and cascading of simple lossless transforms: designs, interaction effects, selection methods

Reading notes for the writer:
- "Derived" marks arithmetic I did on cited numbers. Everything else in Cited Findings is as reported by the linked source.
- Several pages were read through an extraction tool that summarises the page; numbers that live only in figures were not recoverable and are flagged.
- Blocked or unreachable pages are listed in the last Gaps section. Items I know only from prior knowledge and did not verify in this session sit in Gaps, labelled as unverified, never in Cited Findings.

## 1. BtrBlocks (SIGMOD 2023): cascade, sampling selection, ablation

### Takeaway
BtrBlocks cascades at most 3 levels of lightweight schemes chosen per 64,000-value block by trial-compressing a 1% sample; the selector is right (optimal or within 2%) 77% of the time, costs 1.2% of compression CPU time, and loses 3.3% on average to the optimum. Its cascade of light schemes still loses to Parquet+zstd on ratio on Public BI (5.28x vs 6.05x) and wins on TPC-H (3.79x vs 3.41x); the paper gives per-scheme ablation only as a figure, with no isolated "cascading on/off" number.

### Cited Findings
- Scheme pool: integers use RLE, One Value, Dictionary, Frequency, SIMD-FastPFOR, SIMD-FastBP128; strings use FSST plus Dictionary/RLE/One Value/Frequency; doubles use Pseudodecimal (new) plus Dictionary/RLE/One Value/Frequency; Roaring bitmaps for NULLs and exceptions — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Cascade shape: RLE splits into a values array and a run-length array, each re-encoded separately; dictionary codes cascade into RLE or FastBP128; strings use Dictionary + FSST or FSST on raw data; Pseudodecimal emits integer digit and exponent columns (each re-encoded) plus a patch column. Maximum depth is 3 by default; beyond that data is left uncompressed — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Selection loop per block (default 64,000 values): collect statistics, filter non-viable schemes, estimate ratio on a sample, pick best, recurse on the output if compressible — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Sample is 10 runs of 64 values from random positions in non-overlapping parts of the block (1% of block). Multiple small chunks spread over the block beat both individual random tuples and one contiguous range; chunks of 16+ tuples performed about the same — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Selector accuracy: 77% of choices optimal or within 2%; average 3.3% worse than optimum; selection costs about 1.2% of compression CPU time — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Ratios (Table 2): Public BI: Parquet 3.37x, +LZ4 4.72x, +Snappy 4.79x, +Zstd 6.05x, BtrBlocks 5.28x. TPC-H: Parquet 1.69x, +LZ4 2.77x, +Snappy 2.85x, +Zstd 3.41x, BtrBlocks 3.79x — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Ablation (Figure 4, approximate): for doubles, adding Dictionary gave about +95% ratio and adding Pseudodecimal about +20%; for strings, Dictionary gave about 7x and FSST on top of dictionary a further +51%; One Value barely changed the average but helped some columns a lot — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Pseudodecimal is column-dependent: CommonGov/26 gets 75.0x (Gorilla 48.0x, FPC 15.1x) but plain RLE gets 187x on the same column; CMSProvider/9: PDE 6.6x vs Dictionary 5.6x; NYC/29 (high-precision coordinates): PDE 1.0x vs FPC 1.5x. PDE is disabled when over 50% of values are exceptions or fewer than 10% are unique — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Speed: compression 75.3 MB/s single thread vs 41.0 MB/s for Parquet+Zstd; in-memory decompression 3.8x (Public BI) and 4.2x (TPC-H) faster than Parquet+Zstd — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Vortex's BtrBlocks-style compressor: MAX_CASCADE = 3, decremented per descent, only terminal schemes (e.g. BitPacking) at zero; exclusion lists block useless chains (dictionary codes skip Dict and Sequence); example chain Dictionary -> REE -> BitPacking takes clustered 32-bit integers to roughly 1 bit per element; ALP cascades into the integer compressor (an `l_discount` column went ALP -> BitPacking at 4 bits) — [Vortex blog](https://vortex.dev/blog/btrblocks-compressor)
- Vortex sampling: stratified, about 1% of data with a 1,024-value floor, fixed RNG seed; some schemes are estimated analytically (FoR, Dictionary, Sparse), others by compressing the sample (REE); trial compression runs through the full cascade so the estimate includes downstream children. Thresholds: Dictionary at <=50% distinct, REE at average run length >=4, Sparse at >90% null or one value — [Vortex blog](https://vortex.dev/blog/btrblocks-compressor)
- Vortex on TPC-H SF10: Parquet+ZSTD 4,465 MB; Vortex default (light encodings only) 2,776 MB; Vortex compact (adds PCodec and ZSTD) 2,016 MB — [Vortex blog](https://vortex.dev/blog/btrblocks-compressor)

### Inferences
- Derived: on Public BI, putting zstd after Parquet's light encodings multiplies ratio by 1.80x (6.05/3.37) and on TPC-H by 2.02x (3.41/1.69). The heavy back end is worth more than the whole light cascade is worth over plain Parquet encodings (BtrBlocks/Parquet = 1.57x on Public BI). For kolmo, which codes every leaf with an entropy/LZ back end, this says the light cascade and the back end remove different redundancy and should be stacked, which is what Vortex "compact" does (derived: 2,776 -> 2,016 MB, a further 27% cut from adding PCodec/zstd on top of the light cascade).
- The strongest ablation numbers are for transforms that change the alphabet (Dictionary: +95% on doubles, 7x on strings) with a second transform compounding on the residue (FSST on the dictionary: +51%). That is the cleanest measured "stacked gain" in this literature: the second transform works on a different redundancy (intra-string substrings) from the first (whole-value repetition).
- Evaluating a candidate by running the full downstream cascade on the sample (Vortex) is the cheap way to capture interaction effects; a per-node estimate that ignores children cannot see them.

### Gaps
- Exact per-step ratios in BtrBlocks Figure 4, the sampling-strategy percentages in Figure 5 and sample-size curve in Figure 6 exist only as plots; I could not extract values.
- Neither BtrBlocks nor Vortex publishes a "depth 1 vs depth 2 vs depth 3" ablation. No isolated number exists for what cascading adds over the best single scheme.

## 2. FastLanes (VLDB 2023 layout paper; 2025 file-format paper): expression encoding and multi-column compression

### Takeaway
FastLanes treats a column's encoding as an expression over about 15 operators, picked from a fixed pool of roughly 31 expressions that was itself found by exhaustive search on Public BI; trying the pool on three 1024-value vectors reaches over 99% of the best ratio. Its leave-one-out ablation is the best published per-operator evidence: Dictionary is worth 42%, every other operator under 6%, and multi-column operators about 8% in total.

### Cited Findings
- Expression encoding chains lightweight operators so one column is encoded by a nested expression; operators are stored as integer IDs in postfix notation in the rowgroup footer. Operators: FFOR, PATCH, DELTA, ALP, ALP_RD, DICT, Transpose, Cast, FRLE, CROSS RLE, FSST, FSST12, CONSTANT, EQUALITY, EXTERNAL DICT — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- Two-phase choice: first a rule-based selector with six ordered rules (Constant, Equality, String-as-Numerical, Double-as-Integer, Narrower Integer Types, One-to-One Map); then remaining columns are encoded with each expression from a fixed pool. The pool was built by exhaustively testing encoding combinations on Public BI and keeping the winners (about 31 entries: 12 string, 9 integer, 9 float, 1 correlated; the count is the extractor's, not the paper's) — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- Sampling: "three-way" sampling of the first, middle and last vector (1024 values each) of a rowgroup reaches over 99% of the best compression ratio — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- Multi-column compression: Equality +4.70%, One-to-One Map +1.17%, Cast +0.78%; together MCC adds roughly 8% ratio and roughly 20% decode speed — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- Leave-one-out ablation (Table 7; ratio gain / decode-speed gain): Dictionary +42.36% / +44.19%; DELTA +5.92% / -1.91%; Equality +4.70% / +2.66%; ALP +4.36% / -7.28%; FSST +3.86% / +3.84%; Patch +2.51% / -7.11%; FFOR +1.30% / +4.48%; One-to-One +1.17% / +9.47%; FSST12 +0.84% / +2.62%; Cast +0.78% / +10.16%; RLE +0.69% / +6.65%; CROSS RLE +0.65% / +16.27%; ALP_RD +0.57% / -2.30%; Frequency +0.09% / +2.06%; Constant +0.00% / +2.92% — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- Ratio (Table 3): Public BI 980.03 MB CSV -> 172.05 MB FastLanes; FastLanes is 41.0% better than Parquet+Snappy, 2.0% better than Parquet+Zstd, 18.0% better than BtrBlocks. On TPC-H Parquet+Zstd is 15.3% smaller than FastLanes — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- Decoding: about 44x faster than Parquet+Zstd and 7x faster than BtrBlocks; its encoder is described as basic and unoptimised — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)

### Inferences
- The ablation shows a heavy-tailed distribution of operator value: one operator carries most of the gain and a long tail gives 0.1 to 6% each. Leave-one-out numbers understate operators that have substitutes (RLE at +0.69% does not mean runs are rare; Dictionary plus bit-packing covers most of the same cases when RLE is removed). This is direct evidence of gains cancelling: two transforms aimed at the same redundancy are substitutes, and the marginal value of the second is small.
- "Exhaustive offline, tiny pool online" is a concrete, cheap design: search all compositions once on a corpus, freeze about 30 winners, then trial only those on 3 x 1024 values. Reported regret under 1%. This is the nearest published analogue to kolmo's trial-by-sample planner and suggests the planner's candidate list can be small if it is mined from an offline exhaustive pass.
- Even with expression encoding plus MCC, a light cascade only matches Parquet+zstd (+2% on Public BI, -15% on TPC-H). Ratio leadership needs the entropy/LZ stage that kolmo already has on every leaf.

### Gaps
- No ablation of cascading depth or of expression choice versus a fixed expression per type.
- I did not verify details of the earlier FastLanes VLDB 2023 layout paper (interleaved bit-packing, Unified Transposed Layout for delta) in this session.
- No Vortex comparison in the FastLanes paper text that was reachable.

## 3. OpenZL (Meta, 2025-2026): graph model, trainer, results

### Takeaway
OpenZL's trainer is greedy cluster merging followed by an NSGA-II genetic search over codec DAGs that returns a Pareto set of speed/ratio points; it uses no reinforcement learning. The two papers publish strong end results but no node-level ablation and do not list the codec chains the trainer found, so OpenZL gives no direct evidence on which transform orderings compound.

### Cited Findings
- Model: a compression graph is a DAG of codecs; function graphs (selectors) map an input to a graph at compression time and are expanded to a resolved graph stored in the frame; the decoder makes no data-dependent decisions. Four stream types: bytes, string, struct(k), numeric(w) — [OpenZL: A Graph-Based Model for Compression](https://arxiv.org/pdf/2510.03203)
- Trainer orchestration: runs the compressor on samples until a component with a registered training function is reached; that function runs local experiments and may return several configurations at different speed/ratio points — [OpenZL: A Graph-Based Model for Compression](https://arxiv.org/pdf/2510.03203)
- Clustering: every stream starts as its own cluster; pairs are greedily merged when the merged compressed size is below the sum of the separate sizes, until a local minimum — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- Graph search (ACE): NSGA-II genetic algorithm with genetic-programming crossover and mutation over DAGs, population seeded with simple commonly effective graphs; per-cluster Pareto sets are merged iteratively and pruned to n entries by crowding distance — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- The second paper states that earlier designs left stage ordering and cross-stage interactions largely unexplored and presents the genetic search as its answer; the typical shape is Parse, Group, Transform, Reduce — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- SAO: zstd -3 5.28 MiB, ratio 1.31, 210/811 MiB/s; xz -9 4.21 MiB, ratio 1.64, 3.34/42.9 MiB/s; OpenZL 3.35 MiB, ratio 2.06, 324/1140 MiB/s. Manual SAO graph: SRA0 gets delta, SDEC0 gets transpose, IS/MAG/XRPM/XDPM get tokenize, with dictionary and index streams from tokenize sent to different graphs — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- Trained ratios (Table 3, OpenZL / xz-9 / zstd-19): ppmf-person 116.70 / 69.10 / 60.15; ppmf-unit 102.69 / 77.05 / 74.99; psam-p 8.59 / 6.75 / 6.47; psam-h 7.67 / 6.27 / 5.98; binance 3.01 / 2.62 / 2.15; tlc 11.42 / 10.52 / 8.95; rea6-precip 24.82 / 21.44 / 18.07; era5-flux 13.62 / 8.73 / 7.45; era5-precip 13.00 / 11.19 / 9.49; era5-pressure 11.20 / 6.58 / 5.60; era5-snow 58.25 / 47.76 / 41.01; era5-wind 6.82 / 4.88 / 4.12 — [OpenZL: A Graph-Based Model for Compression](https://arxiv.org/pdf/2510.03203)
- Speeds (compress/decompress MiB/s): era5-pressure OpenZL 417/873 vs zstd-19 1.79/537; psam-p (CSV) OpenZL 23.8/49.4 vs zstd-19 1.10/969. Average over datasets in the second paper: OpenZL 142/323, zstd -19 6.07/2820, xz -9 6.14/314 — [first paper](https://arxiv.org/pdf/2510.03203); [second paper](https://arxiv.org/pdf/2605.09928)
- Negative case: on enwik7 training did not help and the compressor fell back to zstd-6 (ratio 3.04 vs xwrt 4.86) — [OpenZL: A Graph-Based Model for Compression](https://arxiv.org/pdf/2510.03203)
- Training-set-size ablation on SAO: training on the first 1% of data gives a 29% ratio gain; training on the full dataset adds only about 3% more — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- Untrained OpenZL inside Nimble cut size by 10% versus zstd, mostly from labelling numeric types; without an effective parser and graph OpenZL performs no better than traditional compressors — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- Training throughput is 1.12 to 11.6 MiB per minute depending on dataset — [OpenZL: Using Graphs to Compress Smaller and Faster](https://arxiv.org/pdf/2605.09928)
- The first paper's text says ppmf_person is "55% better than xz-9" while its own table gives 116.70/69.10 (derived: 69% higher); the discrepancy is in the source — [OpenZL: A Graph-Based Model for Compression](https://arxiv.org/pdf/2510.03203)

### Inferences
- OpenZL's own account of where gain comes from is ordered: typing/parsing first (10% from numeric labelling alone), then grouping streams, then transform choice. This matches kolmo's rule that the planner must type columns first.
- Greedy merge clustering is a direct test of a stacking interaction: two streams are concatenated only if coding them together is smaller than apart. It is the cheap, measurable version of "do these two streams share redundancy".
- A 1% training sample captures 29 of about 32 points of gain. Cascade choice saturates quickly with sample size, consistent with BtrBlocks (1%) and FastLanes (3 vectors).
- Because Meta chose an offline evolutionary search with minutes-per-MiB cost, OpenZL's search is not a model for kolmo's in-budget online planner; it is a model for an offline pool-mining step.

### Gaps
- No published list of the node sequences that trained graphs use for any benchmark dataset, and no per-node ablation, in either paper.
- Per-dataset ratios in the second paper are only in a chart (Fig. 6).
- I did not read openzl.org docs or the GitHub repository in this session, so codec-level documentation claims are unverified.
- No explicit cost function is given; the second paper only says graphs are chosen "within a given speed budget".

## 4. Array and scientific pipelines: Blosc/Blosc2, TileDB, HDF5, Zarr, numcodecs

### Takeaway
The Blosc bytedelta study is the clearest measured case of an amplifying stack: shuffle then per-byte-plane delta lifts the median ratio from 5.1x (shuffle) to 5.86x, and the gain grows when the back end also codes the byte planes as separate streams (5.0x to 5.7x for bytedelta, 4.0x to 4.1x for bitshuffle). The same filter worsens ratio on one of five datasets and never wins on the ratio-versus-decode-speed balance.

### Cited Findings
- Bytedelta is applied after shuffle and takes differences between bytes within each byte stream; it is a separate filter chained after shuffle — [Blosc bytedelta post](https://blosc.org/posts/bytedelta-enhance-compression-toolset/)
- Median ratio across ERA5 datasets and codecs: no filter 3.86x, shuffle 5.1x, bitshuffle 5.62x, bytedelta 5.86x — [Blosc bytedelta post](https://blosc.org/posts/bytedelta-enhance-compression-toolset/)
- Pressure dataset, ZSTD clevel 9: bytedelta 5.0x vs bitshuffle 4.0x; with "always split" bytedelta 5.7x vs bitshuffle 4.1x (37% higher, and 14% above bytedelta without splitting) — [Blosc bytedelta post](https://blosc.org/posts/bytedelta-enhance-compression-toolset/)
- Bytedelta worsens ratio on the precip dataset and only slightly improves snow; ZSTD at levels 6 and 9 can need up to 2x compression time with it; decompression is up to about 20% slower; "no single codec and filter combination is best for all datasets" — [Blosc bytedelta post](https://blosc.org/posts/bytedelta-enhance-compression-toolset/)
- Independent test (Aras Pranckevicius): shuffle + bytedelta improves ratio slightly for BloscLZ and LZ4 and substantially for Zstd; Blosc's older "Delta" filter is actually XOR, and shuffle combined with it gave ratio below 2.0 everywhere, worse than Zstd or LZ4 with no filter; a combined single-pass split+delta filter was faster than two chained filters (values in charts only) — [Float Compression 8: Blosc](https://aras-p.info/blog/2023/03/02/Float-Compression-8-Blosc/)
- TileDB: filters run in the order added and are reversed on read; filter set includes BYTESHUFFLE, BITSHUFFLE, POSITIVE_DELTA, BIT_WIDTH_REDUCTION, DOUBLE_DELTA, RLE and general codecs. Documented pipelines: bit-width reduction then Zstd; positive-delta then bit-width reduction then Zstd for offsets. Guidance: put positive-delta before bit-width reduction because it creates more opportunities for a narrower type; shuffle and positive-delta are typically not used alone — [TileDB filters tutorial (v1.6.3)](https://tiledb-inc-tiledb.readthedocs-hosted.com/en/1.6.3/tutorials/filters.html)
- TileDB filters work on chunks (default maximum 64 KB) filtered separately; bit-width reduction uses a 256-byte window and positive-delta a 1024-byte window by default — [TileDB filters tutorial (v1.6.3)](https://tiledb-inc-tiledb.readthedocs-hosted.com/en/1.6.3/tutorials/filters.html)

### Inferences
- The pressure numbers show a three-way interaction that is super-additive: shuffle exposes byte planes, delta within a plane removes smooth variation, and stream splitting stops the entropy coder mixing the statistics of different planes. Splitting adds 14% to bytedelta and (derived) 2.5% to bitshuffle. A gain that looks small in one configuration is larger in another because the later stage can exploit what the earlier stage exposed.
- The effect is back-end-dependent: Zstd gains much more from bytedelta than LZ4 or BloscLZ. Byte-plane delta turns structure into skewed symbol statistics, which only a back end with an entropy stage can harvest; LZ-only back ends see few new matches. For kolmo this argues for evaluating a transform against each leaf back end, not once.
- Blosc's XOR-as-delta result shows the wrong second transform after shuffle is worse than no filter at all: a mis-ordered or mis-chosen stack can cancel the first transform's gain and go below baseline.
- TileDB's "delta before bit-width reduction" is the canonical enabling order: the first transform shrinks the value range and the second is the only stage that can bank that shrinkage.

### Gaps
- Per-dataset, per-codec ratio tables for bytedelta (wind, flux, zlib) are only in charts.
- Not verified in this session (prior knowledge only): Zarr v3's codec pipeline ordering of array-to-array, exactly one array-to-bytes, then bytes-to-bytes codecs; numcodecs Delta, FixedScaleOffset, Quantize, PackBits, Shuffle; HDF5 filter chains (shuffle before deflate, scale-offset, n-bit); Blosc2 trunc_prec; TileDB's later XOR and float-scale filters (the page fetched is the 1.6.3 tutorial, which predates them). No measurements were gathered for these.
- I could not read Aras Pranckevicius's "Float Compression 3: Filters" post (permission prompt timed out); it holds measured ratios for split-floats, split-bytes, XOR and delta in different orders.

## 5. Parquet, ORC, Arrow, Vortex, Lance, Nimble

### Takeaway
Parquet's BYTE_STREAM_SPLIT is a transform whose value depends on the back end behind it: on 2-byte floats it raises zstd ratio by roughly 19 to 56% and can take Snappy from 1.03x to 1.94x, but on one float32 file it lowered the zstd ratio to 0.89x of plain. Enabling Parquet v2 encodings on TPC-H lineitem helps Snappy files (30% smaller) far more than zstd files (11% smaller).

### Cited Findings
- Float16 (2-byte FLBA), ratio PLAIN -> BYTE_STREAM_SPLIT under zstd: msg_sp 2.28 -> 2.71; msg_sppm 45.81 -> 71.49; msg_sweep3d 11.77 -> 17.00; num_plasma 259.58 -> 405.96; obs_spitzer 1.22 -> 1.35; obs_temp 1.08 -> 1.17 — [PARQUET-2414](https://issues.apache.org/jira/browse/PARQUET-2414)
- Same files under Snappy: msg_sweep3d 1.03 -> 1.94; num_plasma 1.00 -> 1.80; obs_temp 1.00 -> 1.00. Under LZ4: msg_sweep3d 2.06 -> 3.20; obs_temp 1.00 -> 1.00 — [PARQUET-2414](https://issues.apache.org/jira/browse/PARQUET-2414)
- Decimal FLBA, PLAIN -> BSS: OSM Belgium lon lz4 1.00 -> 1.93, zstd 1.27 -> 2.06; OSM Belgium lat lz4 1.00 -> 1.63, zstd 1.18 -> 1.73; OSM changesets min_lat lz4 1.00 -> 1.01, zstd 1.05 -> 1.12 — [PARQUET-2414](https://issues.apache.org/jira/browse/PARQUET-2414)
- BSS hurt case: float32 reference file msg_sppm with zstd, ratio fell to 0.89x of plain; neutral with lz4 (1.01x) and snappy (1.05x) — [PARQUET-2414](https://issues.apache.org/jira/browse/PARQUET-2414)
- Integers versus DELTA_BINARY_PACKED (qualitative in the issue): PyPI index data, BSS + compression clearly better with strong gains on timestamps; NYC taxi, only BSS + zstd consistently better, and non-timestamp integer columns were not consistently better than DELTA_BINARY_PACKED; OSM changesets mixed — [PARQUET-2414](https://issues.apache.org/jira/browse/PARQUET-2414)
- TPC-H SF1 lineitem written by DuckDB: Snappy v1 244 MB vs v2 170 MB (about 30% smaller); zstd v1 152 MB vs v2 135 MB (about 11% smaller). v2 turns on DELTA_BINARY_PACKED, DELTA_LENGTH_BYTE_ARRAY and BYTE_STREAM_SPLIT together, so individual effects are not isolated — [DuckDB: Query Engines, Gatekeepers of the Parquet File Format](https://duckdb.org/2025/01/22/parquet-encodings)
- Synthetic extremes: an integer range goes from 3.7 GB (v1) to 1.3 MB (v2); a float sequence from 6.3 GB to 610 MB — [DuckDB blog](https://duckdb.org/2025/01/22/parquet-encodings)
- Vortex cascade and results: see section 1 — [Vortex blog](https://vortex.dev/blog/btrblocks-compressor)

### Inferences
- The Parquet numbers are the best measured case of partial cancellation. Light encodings and zstd overlap: the encodings give 30% with a weak back end and 11% with a strong one, because zstd already captures part of what the encoding removes. A transform's standalone gain overstates its stacked gain whenever the next stage targets the same redundancy.
- The reverse also appears: on OSM Belgium coordinates, lz4 alone achieves 1.00x and BSS + lz4 achieves 1.93x. A back end that found nothing becomes effective once the transform aligns equal bytes. Here the transform is an enabler and the combined gain is far above the sum of parts (lz4 alone gives zero).
- The 0.89x case shows why a guard is needed: byte splitting destroys whole-value repeats that LZ could match, so data with exact repeated floats does better unsplit. The reported practice in this issue is trial comparison per column, not a rule.

### Gaps
- The integer BSS tables in PARQUET-2414 did not come through extraction; only the author's qualitative summary is cited.
- Lance 2.1 ("Lance File 2.1: Smaller and Simpler", lancedb.com blog) redirected and was not read; no Lance findings are verified here.
- Not verified this session: ORC RLEv2's four sub-encodings (short repeat, direct, patched base, delta), Arrow run-end encoding, Parquet's DELTA_BYTE_ARRAY measurements, and Nimble's encoding-selection policy (the only Nimble fact found is the OpenZL-in-Nimble 10% figure in section 3).

## 6. General-purpose compressors that stack stages

### Takeaway
In BWT compressors the evidence is that the transform alone gives nothing to a static order-0 coder and a great deal to a locally adaptive one, and that refinements to the stages after BWT are worth single-digit percentages each (about 2.52 to 2.34 bits/byte in Fenwick's coder variants; about 2.29 to 2.24 bps across IF/WFC variants). Order matters measurably: run-length coding before WFC gives 2.241 bps, after it 2.271 bps.

### Cited Findings
- BWT is a permutation, so static and adaptive order-0 coding is unchanged by it (proved as lemmas): SOURCES stays at 69.48% and XML at 65.54% with or without BWT. A locally weighted adaptive coder goes from 64.20% on the text to 26.44% on BWT(text) for SOURCES, 65.27% to 13.11% for XML, 56.55% to 29.41% for ENGLISH; but only 24.64% to 23.02% for DNA and 56.17% to 53.34% for PITCHES — [Weighted Burrows-Wheeler Compression](https://arxiv.org/pdf/2105.10327)
- Fenwick's block-sorting coder variants on the Calgary corpus (bits/byte): order-0 arithmetic coder after MTF 2.52; three-level cache model 2.42; structured coding model 2.36; structured model plus run-length coding of zeros 2.34. References: original BW94 2.43, gzip -9 2.70, PPMC 2.48, PPMZ 2.21 — [Fenwick, Block sorting text compression](https://www.cs.auckland.ac.nz/~peter-f/FTPfiles/1997%20CompJourn%20BWT.pdf)
- Fenwick: about 63% of MTF output ranks are zero on average; an initial run-length pass on the input changed compression by about 0.1% on most files but improved PIC by about 10% and cut its sort time to about 2% of the original — [Fenwick](https://www.cs.auckland.ac.nz/~peter-f/FTPfiles/1997%20CompJourn%20BWT.pdf)
- BWT second-stage comparison (Calgary average bps, from Abel's tables as reproduced in a survey): IF 2.292 with original alphabet, 2.286 with ascending alphabet sort; WFC with RLE before it 2.241, with RLE after it 2.271; sorted IF 2.268; advanced WFC 2.241; Deorowicz's WFC 2.249; hybrid switching at 256 KB 2.238. WFC is roughly 20 times slower than MTF — [survey reproducing Abel's tables](https://www.cs.brandeis.edu/~dilant/cs175/%5BDan-Preston%5D.pdf)
- Deorowicz's WFC-based BWT compressor is cited at 2.25 bps on the Calgary corpus; WFC replaces MTF and needs no side information unlike IF, at a time cost — [data-compression.info WFC page](https://data-compression.info/Algorithms/WFC/index.html)
- Dictionary text preprocessing before different back ends (xwrt 3.2, calgary.tar bytes): Dict+CM 681,419; Dict+LZMA 820,307; Dict+LZ77 1,021,485; lpaq1 with no dictionary gets 682,512. Reference: bzip2 -9 gets 860,097 — [Mahoney, Data Compression Explained](https://mattmahoney.net/dc/dce.html)
- x86 BCJ filter before xz in squashfs: module went from 52 MB to 51 MB (about 2%); mksquashfs tries each filter per block and keeps the best — [Slax blog](https://www.slax.org/blog/18663-XZ-compression-filters)
- LZ-Sub (Bloom): send each literal as the difference from the byte at the last match offset, so a zero delta is a match; "this is basically LZMA" with sub-literals in place of bitwise literal coding. The post gives no measurements — [cbloom rants, LZ-Sub](https://cbloomrants.blogspot.com/2015/05/05-21-15-lz-sub.html)
- Oodle selects among encodings by cost, treating bytes saved as money that can be spent on decode time (spaceSpeedTradeoffBytes, default 256); the same Lagrangian choice turns entropy coding on or off per stream and decides raw versus entropy-coded literals. Leviathan on e.dds: 2.954:1 at 544 MB/s with setting 256, 2.866:1 at 826 MB/s with 1024, 2.831:1 at 886 MB/s with 2048 — [cbloom rants 2018](https://cbloomrants.blogspot.com/2018); [cbloom rants Feb 2017](https://cbloomrants.blogspot.com/2017/02)

### Inferences
- The weighted-BWT result is the cleanest demonstration of the hypothesis's mechanism. BWT alone is worth exactly zero to a coder with global order-0 statistics and worth a 2.4x to 5x size reduction to a coder that tracks local statistics (derived from 64.20 -> 26.44 and 65.27 -> 13.11). A transform's value is a property of the transform/coder pair. On DNA and PITCHES the same pair gives 3 to 7%: data dependence dominates.
- Stage-order sensitivity inside BWT pipelines is real but small: RLE-before-WFC versus after is 1.3% (derived from 2.241 vs 2.271); alphabet reordering before IF is 0.3%. Gains from polishing post-BWT stages total about 7% across Fenwick's coder variants (derived, 2.52 to 2.34). The first-order decision is which big transform, and ordering of small ones is second-order.
- Dict+CM beating lpaq1 by 0.16% while a dictionary front end is known as a large aid to weak text compressors is consistent with the cancellation pattern in section 5: the stronger the downstream model, the less a redundancy-removing preprocessor adds. (The Mahoney table does not include no-dictionary LZMA/LZ77 on the same file, so this is partial evidence only.)
- Oodle's per-stream on/off entropy decision under a speed price is the production precedent for kolmo's "smallest option whose cost stays within the mode budget" rule.

### Gaps
- The Deorowicz paper itself (Software: Practice and Experience 2002) was blocked; the second-stage numbers above come from a secondary survey reproducing Abel's tables, and I have no bps figures for plain MTF or distance coding on the same basis.
- Not verified this session (prior knowledge, treat as lore until sourced): bzip2's RLE1 exists mainly to protect the sorter from long runs, a point Fenwick's PIC timing above supports; LZMA's literal coder uses the byte at the last offset as context ("matched literal"); 7-Zip's BCJ2 splits call/jump targets into separate streams; delta filters for xz are recommended for uncompressed audio/bitmaps; zstd codes literals with Huffman and sequences with three FSE tables and has a block splitter; brotli context-models literals and distances over the LZ output; LZHAM; zpaq component chains (ICM -> ISSE chain -> mixer) and PAQ preprocessing. I found no per-stage measured gains for these in the pages I could read (the Mahoney extract was truncated before its transform and context-mixing sections).
- Bloom's and Giesen's posts on Kraken/Leviathan literal modes (sub versus raw versus order-1) and offset sub-streams were not reached; the claim that sub-literals help on structured binary and hurt on text is practitioner lore I could not source here.

## 7. Evidence on ordering, and on LZ after a transform hurting

### Takeaway
Measured ordering effects fall into two classes: enabling orders where the second stage is the only one that can bank the first stage's work (delta then bit-width reduction; shuffle then per-plane delta then per-plane entropy coding; dictionary then FSST), which give large gains, and polishing orders inside an already good pipeline (RLE before or after WFC), which give about 1%. Transforms can also destroy what LZ needs; the documented guards are trial compression and analytic exclusion thresholds.

### Cited Findings
- Shuffle then delta within byte planes then zstd, with planes coded separately, beats bitshuffle by 37% on ERA5 pressure; the delta step is harmful on precip — [Blosc bytedelta post](https://blosc.org/posts/bytedelta-enhance-compression-toolset/)
- XOR-style "delta" combined with shuffle gave ratios below 2.0, worse than unfiltered Zstd or LZ4 — [Float Compression 8: Blosc](https://aras-p.info/blog/2023/03/02/Float-Compression-8-Blosc/)
- Positive-delta before bit-width reduction is the recommended order because delta creates narrower values for the reducer — [TileDB filters tutorial](https://tiledb-inc-tiledb.readthedocs-hosted.com/en/1.6.3/tutorials/filters.html)
- RLE before WFC 2.241 bps, RLE after WFC 2.271 bps — [survey reproducing Abel's tables](https://www.cs.brandeis.edu/~dilant/cs175/%5BDan-Preston%5D.pdf)
- Dictionary then FSST: +51% on top of dictionary alone for strings — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Dictionary -> run-end encoding -> bit-packing reduces clustered 32-bit integers to roughly 1 bit per element; a dictionary's codes are never fed to another dictionary (exclusion list) — [Vortex blog](https://vortex.dev/blog/btrblocks-compressor)
- Byte stream split lowered zstd ratio to 0.89x on one float32 file and did nothing for lz4/snappy on obs_temp — [PARQUET-2414](https://issues.apache.org/jira/browse/PARQUET-2414)
- pcodec orders its stages mode -> delta -> binning -> tANS; splitting a number into two latents (e.g. quotient and remainder by a detected multiplier) can raise theoretical entropy while lowering binning cost — [Pcodec paper](https://arxiv.org/html/2502.06112v2)
- OpenZL's manual SAO graph uses a different first transform per field: delta on the sorted right-ascension field, transpose on declination, tokenize on low-cardinality fields — [OpenZL second paper](https://arxiv.org/pdf/2605.09928)
- Guards in use: PDE disabled above 50% exceptions or below 10% unique values (BtrBlocks); REE requires average run length >=4 and Dictionary <=50% distinct (Vortex); per-block try-all-filters-keep-best (mksquashfs BCJ); early stop when delta order o costs more than o-1 (pcodec) — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf); [Vortex blog](https://vortex.dev/blog/btrblocks-compressor); [Slax blog](https://www.slax.org/blog/18663-XZ-compression-filters); [Pcodec paper](https://arxiv.org/html/2502.06112v2)

### Inferences
- A working taxonomy for kolmo's experiment log, each class with a measured exemplar above:
  1. Enabler pairs (super-additive): the first transform's benefit is invisible until a specific successor exists. Examples: BWT + local-adaptive coder (0% alone), BSS + lz4 on clustered decimals (1.00x alone, 1.93x together), delta + bit-width reduction, shuffle + per-plane delta + split streams.
  2. Independent pairs (roughly multiplicative): transforms that remove different redundancy. Example: dictionary (value repetition) then FSST (substring repetition) at 7x then +51%; light cascade then zstd in Vortex compact.
  3. Substitute pairs (sub-additive, partial cancellation): both target the same redundancy. Examples: Parquet v2 encodings give 30% before Snappy but 11% before zstd; RLE's leave-one-out value of 0.69% in FastLanes; dictionary preprocessing before a context-mixing model.
  4. Destructive pairs (negative): the transform removes structure the successor needs. Examples: BSS before zstd on float data with whole-value repeats (0.89x); XOR-delta after shuffle; bytedelta on precip.
- The owner's hypothesis is supported for class 1 and class 2 and contradicted for class 3 and 4. The practical consequence is that a small isolated gain is a poor predictor of stacked gain in either direction: it must be re-measured with its successor stages and its leaf back end attached.
- Small gains are most likely to amplify when the small transform (a) narrows the value range or alphabet ahead of a width- or alphabet-sensitive stage, (b) separates streams with different statistics ahead of an adaptive or per-stream entropy coder, or (c) aligns equal bytes ahead of LZ. They are most likely to vanish when the successor is a strong model that already captures the same dependency.

### Gaps
- I found no published head-to-head of "delta on whole values then shuffle" versus "shuffle then delta per plane" with numbers in a page I could read (the Aras Pranckevicius filters post that tests this was blocked). The claim that whole-value delta then shuffle wins on smooth integers while per-plane delta wins on floats is unverified here.
- No measured data found on zigzag-before-bitpack, sorting dictionaries before coding indices, tokenize-before-delta, or delta-of-delta then shuffle then LZ as isolated ordering experiments.
- No source found that quantifies how often LZ after a numeric transform loses to LZ on the raw bytes across a corpus; only individual cases.

## 8. Cheap methods for choosing a cascade per column/stream, and whether reinforcement learning is used

### Takeaway
Every production system found selects by trial on small samples or by offline search plus a small online pool, with reported regret of 1 to 10%; learned selectors (neural ranking, random forests) are published with accuracy in the 85 to 96% range for single-level choice. Reinforcement learning has been used once that I can document (Alibaba's AMMMO time-series compressor, policy gradient over a parameterised scheme space), where it roughly matched a 5000-sample random search; no general-purpose or columnar cascade system uses it.

### Cited Findings
- BtrBlocks: sample-and-trial per level, greedy top-down, 77% within 2% of optimal, 3.3% average loss, 1.2% of CPU time — [BtrBlocks paper](https://www.cs.cit.tum.de:443/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
- Vortex: trial through the full cascade on about 1% stratified sample; analytic estimates for cheap-to-model schemes — [Vortex blog](https://vortex.dev/blog/btrblocks-compressor)
- FastLanes: rule pass, then all of a roughly 31-expression pool on three 1024-value vectors; over 99% of best ratio; pool mined by exhaustive search offline — [FastLanes File Format paper](https://ir.cwi.nl/pub/35881/35881.pdf)
- CodecDB: two-layer MLP (1,000 hidden units) trained as learning-to-rank on features (value length stats, cardinality ratio, sparsity, entropy, repeated-word count, sortedness with windows 50/100/200). Picks the best Parquet encoding for 96% of string columns (Parquet's rules 80%, Abadi's decision tree 32%) and 87% of integer columns (72%, 40%). On the first 1 MB only: 94% and 85%. Feature extraction 57 ms per column, inference 3 ms. About 30% smaller than Parquet's default choice. Random sampling instead of head sampling dropped accuracy to 65%. Removing any single feature cut accuracy by 18 to 25% — [CodecDB, SIGMOD 2021](https://thedatumorg.github.io/DATUM/papers/JiangSIGMOD21.pdf)
- LEA: per encoding, random-forest regressors predict encoded size and scan speed from a contiguous 1% sample's encoded size plus slice statistics (range, cardinality, gap mean/variance/skewness). Stays within 10% of brute-force optimal for size and latency; versus a commercial system's heuristic advisor, 26% less space and 19% lower cold-cache latency on TPC-H SF10. The full model beat both sample-only and statistics-only variants. Brute force took over 30 minutes per column when scan speed was measured — [LEA: A Learned Encoding Advisor](https://arxiv.org/pdf/2105.08830v1)
- AMMMO (reinforcement learning): scheme space of 6 transforms (delta, reversed delta, xor, delta-of-delta, reversed DoD, delta-xor) and 3 differential codings with 9 control parameters, 331,776 combinations; a network (1024/512/42 units) trained with REINFORCE and entropy regularisation, reward = compression ratio, chosen because no labels exist. Result: average 50% (up to 120%) better ratio than other time-series methods; about 42% over Gorilla. The RL selector beats the rule-based selector "in most cases" and is "very close to or even better" than a 5000-sample random search; a fixed setting beats averaged random settings. Training about 100 seconds per 1024 blocks; mode selection about 900 MB/s on CPU+GPU — [Two-level Data Compression using Machine Learning in Time Series Database](https://users.cs.utah.edu/~lifeifei/papers/MLCompression.pdf)
- OpenZL: greedy cluster merging plus NSGA-II genetic search offline; an ML selector on statistical features can choose a graph at compression time (model type not named); 1% of data gives 29% gain and the remaining 99% adds about 3% — [OpenZL second paper](https://arxiv.org/pdf/2605.09928); [OpenZL first paper](https://arxiv.org/pdf/2510.03203)
- pcodec: mode chosen on a deterministic sample of a few percent by lowest estimated bit cost; delta order chosen by actual compressed size on runs of about 100 consecutive numbers, trying orders 1 to 7 and stopping when order o costs more than o-1; bins via an O(k^2) dynamic program. Against the best alternative given 50% more compression time it is 29 to 94% better (e.g. Air Quality 14.62 vs Blosc shuffle+zstd 7.54; Taxi 6.94 vs Parquet dict+zstd 5.29) — [Pcodec paper](https://arxiv.org/html/2502.06112v2)
- Oodle: Lagrangian size-versus-decode-time cost at both the parse level and the per-stream entropy on/off level; search is heuristic and limited because full enumeration is intractable — [cbloom rants 2018](https://cbloomrants.blogspot.com/2018)

### Inferences
- Reported regret of cheap selection is consistently small: 3.3% (BtrBlocks greedy sample-trial), under 1% (FastLanes pool on 3 vectors), within 10% (LEA). There is little headroom for a learned or RL policy to win on ratio; the possible win is in selection time, by pruning trials. This bears directly on the open kolmo question of whether RL is needed: the published evidence says no for ratio. AMMMO's RL matched random search with 5000 samples, which is a statement about search cost, and its space is a flat parameter vector, not a cascade.
- Sampling shape matters more than sample size: many short runs spread across the block (BtrBlocks), stratified slices (Vortex), first/middle/last vectors (FastLanes). CodecDB's drop to 65% with random (non-contiguous) sampling confirms that samples must preserve local order, since delta, RLE and LZ all depend on adjacency.
- Hybrid estimation is the efficient frontier: closed-form cost for schemes whose size is a function of simple statistics (FoR, dictionary, bit-packing, constant), actual trial for schemes whose size depends on sequence structure (RLE, LZ, FSST), and stats-plus-sample models (LEA) where trial is too slow.
- No source compares greedy top-down cascade choice against beam search or exhaustive cascade search with numbers. BtrBlocks' 3.3% loss conflates sampling error and greedy error. A kolmo experiment measuring greedy versus beam-2 versus exhaustive at depth 3 on its own corpus would be new data.
- The offline-exhaustive, online-pool pattern (FastLanes) and the offline-genetic, online-frozen-graph pattern (OpenZL) both move interaction discovery out of the cost budget. That suits kolmo's zstd -3 budget in fast mode: discover enabling chains offline, ship them as a short typed candidate list, trial only those.

### Gaps
- No verified source on zstd's block splitter as a selection method, on LeCo's learned selection, or on "learning to select compression" works beyond CodecDB, LEA and AMMMO.
- No published regret numbers for greedy versus exhaustive cascade search.
- I found no evidence of RL in any shipped general-purpose compressor or columnar format; absence of evidence from about 25 searches and fetches, not a proof.

## 9. Theory: why and when stacked gains compound

### Takeaway
Lossless transforms are bijections and cannot reduce information; all gain comes from making the remaining redundancy visible to the model that follows. The published proof-level example is BWT: provably neutral for order-0 static and adaptive coders, strongly positive for locally adaptive ones. Gains multiply only when successive stages remove redundancy the others cannot see.

### Cited Findings
- Lemmas: static and adaptive order-0 coding give the same size on a text and on its BWT, since the symbol multiset is unchanged; weighted (recency-biased) coders gain 24.5% on average on BWT output over the traditional variants — [Weighted Burrows-Wheeler Compression](https://arxiv.org/pdf/2105.10327)
- "It is not possible to compress random data or compress recursively"; every compressor is a model plus a coder with optional preprocessing transforms — [Mahoney, Data Compression Explained](https://mattmahoney.net/dc/dce.html)
- pcodec's stated design goal for mode + delta is to make latents "SIID" (smooth, independent, identically distributed) so that binning plus an order-0 tANS coder is near-optimal; Theorem 1 bounds the binning loss at H + 5s*log2(T)/(k - 2s) bits, at most about 1.26 bits per latent for 64-bit data with 256 bins — [Pcodec paper](https://arxiv.org/html/2502.06112v2)
- Splitting one value into two latents can raise theoretical entropy while lowering actual coded size because the coder models each part better — [Pcodec paper](https://arxiv.org/html/2502.06112v2)
- OpenZL frames the same point as semantic specialisation increasing along the graph, with reversible transforms exposing structure and reductive transforms removing data; some transforms need not shrink data — [OpenZL second paper](https://arxiv.org/pdf/2605.09928)
- The parquet evidence that one transform's gain shrinks from 30% to 11% as the back end strengthens — [DuckDB blog](https://duckdb.org/2025/01/22/parquet-encodings)

### Inferences
- A usable accounting identity for kolmo's experiment tracking: coded size = true information + modelling loss of the final coder on the stream it is given. A transform changes only the second term. Two transforms compound when each lowers a different component of that loss (range/alphabet mismatch, cross-symbol dependence, non-stationarity/mixing of sources, long-range repetition) and cancel when both lower the same component.
- This yields a testable prediction for the hypothesis. Measure each transform's gain twice, once before an order-0 coder and once before the strongest back end. If the gain collapses under the strong back end, the transform is a substitute for modelling and will not amplify (class 3). If the gain holds or grows, it removes something the back end cannot model, and stacking another transform aimed at a different loss component should be near-multiplicative (class 2) or better (class 1).
- The pcodec remark (higher entropy, smaller output) is a caution against using empirical entropy of an intermediate stream as the planner's objective: only the size after the actual leaf coder is a valid cost.
- The standard signal-processing statement that transform coding gain comes from decorrelation ahead of a memoryless coder, and the minimum-description-length two-part-code view of choosing a model (model bits + data-given-model bits, which is what a stored transform graph plus payload is), both fit these findings, but I did not source either in this session.

### Gaps
- No source was fetched for classical transform coding gain or for MDL two-part codes; treat those two framings as textbook background that still needs a citation.
- I found no paper that formally characterises conditions for super-additive gains of composed lossless transforms; the four-class taxonomy in section 7 is my synthesis from measured cases, not an established result.
- Blocked or unread pages: Deorowicz "Second step algorithms in the Burrows-Wheeler compression algorithm" (scispace.com PDF, permission prompt timed out); Aras Pranckevicius "Float Compression 3: Filters" (permission prompt timed out); Lance 2.1 blog (redirect not followed); Mahoney's Data Compression Explained beyond the first 100,000 characters (sections on transforms and context mixing); cbloom rants 2018 page beyond 100,000 characters; ALP, Data Blocks, LeCo, Nimble docs and Zarr v3 spec were not attempted.
