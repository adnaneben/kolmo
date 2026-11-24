> **DRAFT. Not posted, and not to be posted by anyone but the owner.** Launch texts for Hacker News and encode.su
> (adoption plan, phase 3). They use only the claims the market report can prove (Silesia, Apple M5, one thread).
> Before posting:
> - the x86-64 run must confirm each claim, and the numbers here are replaced by the x86 ones where they differ;
> - the benchmark page must be live, with its link filled in below;
> - the release that the launch goes with must be out on Homebrew, PyPI and crates.io.
>
> The plan's wording "2-4x smaller than zstd on CSV, logs, pickles and integer columns" does not hold file by file
> in the report's section 4. At level 3 against zstd -3 the range is 1.38x (loghub) to 3.39x (OWID CO2 CSV). At each
> file's best level it is 1.73x (population.i64) to 4.0x. The texts below use the measured range.
>
> Claims deliberately left out: "faster than zstd", anything about LLM or model weights, "beats OpenZL" (trained
> OpenZL has not been tested), and anything about small files.

# Show HN

## Title (pick one; HN allows 80 characters)

1. `Show HN: Kolmo – smaller than xz -9, decodes 8x faster, detects data structure`
2. `Show HN: Kolmo – a compressor that detects tables, arrays and images on its own`
3. `Show HN: Kolmo – xz-class ratios with zstd-class decoding`

Recommended: 1. It is the claim the plan leads with, and every part of it is measured (level 6 against xz -9 on
Silesia, decode 8.6x). It is 78 characters.

**URL:** the GitHub repository, https://github.com/adnaneben/kolmo (or the benchmark page, once it is live).

## First comment (posted by the author right after submitting)

Hi HN, I'm the author of Kolmo, a lossless compressor written in C, with a CLI, a Python package (`kolmoc`) and a
Rust crate.

The idea: a lot of data isn't really "a stream of bytes". CSV files are tables, logs are templates with variables,
scientific files are arrays of records, and images are grids of pixels. Kolmo looks at each block of the input,
trial-codes a few candidate transforms on a slice (typed table columns, log templates, byte planes, image prediction,
fixed records, decimal doubles), and keeps one only if it is clearly smaller. Then it codes the result with its own LZ,
block-sorting or context-mixing coders, depending on the level. It never looks at file names, and there is no schema,
profile or training step.

What it does, measured on Silesia with every codec in lzbench on one thread (Apple M5; the method and the raw results
are on the benchmark page):

- `kolmo -6` is smaller than `xz -9` (ratio 4.367 vs 4.346) and decodes 8.6x faster (1463 vs 169 MiB/s).
- `kolmo -4` is within 1.6% of `zstd -19`'s ratio and compresses 13x faster (79 vs 6 MiB/s).
- `kolmo -9` matches `zpaq -5`'s ratio (5.232 vs 5.237) and decodes 120x faster.
- On structured files the gap gets large. At the default level, Silesia's star catalog is 2.2x smaller than with
  zstd -3, and the x-ray image 1.65x. On CSV, logs, pandas pickles and integer columns from public datasets, the
  default level is 1.4-3.4x smaller than zstd -3, and the higher levels reach up to 4x.

Where it loses, from the same runs:

- On a single core, zstd decodes 30-40% faster than Kolmo's levels 2-7. Kolmo's blocks decode in parallel, so on many
  cores it pulls ahead, but if your hot path is one-core decode, use zstd.
- Kolmo's fastest level is beaten by lzav -1 on both ratio and speed.
- On plain text at the low levels it is within a few percent of zstd. There is no structure to find.
- Small files: on the Canterbury corpus, the default level is 4% larger than zstd -3.
- Memory: level 9 uses about 1.4 GB to compress.

The format (container version 12) is specified in FORMAT.md. Every release reads every frame written since 0.0.1 to
the same bytes, which CI checks against a frozen corpus of frames from each release (COMPATIBILITY.md). The decoders
are fuzzed under ASan/UBSan in CI. It is 0.0.x because the command line and API may still change; the format will not.

Try it: `brew install adnaneben/kolmo/kolmo`, `pip install kolmoc` or `cargo add kolmo`. Then run `kolmo -v FILE`, and
`kolmo -l FILE.kolmo` to see what it detected.

I'd especially like data where it does badly. A public sample in an issue is the most useful thing you can send.
There's a write-up of how the detection works here: [link to structure-detection post].

---

# encode.su

**Forum:** Data Compression, new thread.

**Title:** `Kolmo 0.0.x: structure-detecting block compressor (typed tables, graph blocks, KLZH2/BWT/CM), Silesia results`

Hi all,

I'd like to share Kolmo, a block compressor that I've been working on, and get your criticism. The repository is
https://github.com/adnaneben/kolmo (Apache-2.0, C, no third-party codec code in 0.0.x).

**Design in brief**

- Independent blocks, each with a stored plan and a 64-bit checksum. CRC-32C covers the header and the block index,
  so any range decodes alone and blocks decode in parallel.
- A planner chooses each block's transform by trial on a slice from the middle of the input. A candidate has to beat
  plain coding by a margin (5% for byte-plane shuffles; the per-type thresholds are in `docs/ROUTING-MAP.md`).
  Candidates:
  - **text:** typed CSV/TSV columns (exact decimals with per-column predictors: delta, delta within an entity, delta
    of delta, third order, and searched formulas of other columns); log templates with variable columns; JSON record
    splitting; line classes for fixed-width text; a word dictionary at higher levels.
  - **binary:** byte-plane widths; x86 E8/E9 and ARM64 BL/ADRP filters; 8- and 16-bit images (row stride by lag
    search, LOCO-I median prediction); fixed records (period by self-similarity, per-field coding); float plane
    splits; integer residuals of order 0-2 with gcd and bias; decimal doubles as integers over 10^k.
  - Container headers (tar, zip/npz, ELF, Mach-O, PE, PNM/BMP/TIFF, WAV, AIFF, Y4M) are parsed as untrusted hints that
    move block boundaries or fix geometry. File names are never read.
- **Graph blocks:** a block can hold a small program (FIELDS, DECIMAL, INT, FLOAT, IMG8, DEC64, CONCAT, ...). Each
  leaf stream is coded by its own coder. New transforms are new node ids, which older decoders refuse.
- **Coders**, all our own since 0.0.1:
  - KLZ, a byte-aligned LZ77 (level 1);
  - KLZH2, LZ77 with Huffman or static-rANS streams (levels 2-7);
  - block sorting: LZP, then a BWT with libsais-derived suffix sorting, then QLFC ported from libbsc (levels 8-9);
  - KLZMA, an LZMA-class coder (level 9);
  - an integer CM coder (ultra: orders 1-7, word, column and indirect contexts, NN mixing, APMs).
- Encoder and decoder share no floating-point decision that depends on FMA (`-ffp-contract=off`), so x86-64 and ARM64
  write identical frames at any thread count. CI checks this against pinned hashes.

**Silesia (211,947,520 B), lzbench 2.4.1, one thread, Apple M5, Kolmo 0.0.2 built with generic flags inside lzbench**

| | ratio | comp MiB/s | dec MiB/s |
|---|---:|---:|---:|
| kolmo -3 | 3.659 | 332 | 1506 |
| zstd -3 | 3.205 | 533 | 2147 |
| zstd -12 | 3.660 | 65 | 2501 |
| kolmo -4 | 3.949 | 79 | 1487 |
| zstd -19 | 4.012 | 6.0 | 2190 |
| kolmo -6 | 4.367 | 8.8 | 1463 |
| xz -9 | 4.346 | 5.1 | 169 |
| kolmo -8 | 4.897 | 40 | 46 |
| bzip3 -5 | 4.487 | 15 | 27 |
| bsc -e2 | 4.348 | 37 | 34 |
| kolmo -9 | 5.232 | 10 | 85 |
| zpaq -5 | 5.237 | 0.7 | 0.7 |
| kolmo -U | 5.912 | 1.2 | 1.2 |

Per file at level 3 against zstd -3, the gain is in sao (+124%), x-ray (+65%), mr (+52%) and nci (+34%). It is 0-3%
on the text and record files, and −1% on xml.

**Known weaknesses** (I'd welcome ideas on any of them):

- The fast tier. Level 1 (2.336 at 922/4164) loses to lzav -1 (2.549 at 918/4968).
- Single-core decode at levels 2-7 is 30-40% below zstd.
- Small inputs. On Canterbury as one tar, -3 gives 4.277 against zstd -3's 4.475, and -7 (5.137) is below -6 (5.221).
- On enwik8, -8 and -9 give identical output (both choose the BWT route). Ultra reaches 18,651,472 B, far from the
  CM leaders on LTCB.
- Memory: -9 uses about 1.4 GB to compress Silesia.

**Caveats on the numbers.** They come from one machine (an x86-64 run is [pending / linked here]). The structured-data
comparisons against OpenZL used untrained OpenZL profiles, so I'm not claiming anything against trained OpenZL
yet. Silesia was among the files used in earlier tuning.

Format spec: FORMAT.md. Compatibility promise: COMPATIBILITY.md. How the planner works: [link to the
structure-detection post].
