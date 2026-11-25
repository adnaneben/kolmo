# KLZ level 1 — exact wire-cost repair of literal runs

Research base: source tree of PR #90, commit f1ea02dff9616e91ca8d0b9ed7fc50333bc925d1. The primary candidate is encoder-only. Every new token is part of the existing codec 28 wire grammar. This is a raw-codec diagnostic. The archive planner can choose transforms, stored blocks or other codecs, so archive-level improvements depend on routing.

## Losing opportunities exposed by the existing parser

The fast search indexes six-byte keys, accepts one candidate per bucket, skips positions on misses and preserves its existing repeat-offset state. Its final literal runs therefore sometimes contain short copies the decoder could represent more cheaply. The experiment inserts up to one exact-cost match per existing literal run without changing the primary parser's decisions. It also tests replacing three-byte offsets with two-byte offsets where exactly the same bytes exist within 65,535 positions. Source equality is checked across the entire chosen match. Overlapping backward matches are legal under the KLZ grammar.

Let E(x,t)=0 for x<t and 1+floor((x−t)/255) otherwise. A literal run L is split around a match of m >= 4 bytes into a prefix a and suffix b with a+m+b=L. A new match saves exactly m+E(L,7)−[1+w+E(m−4,15)+E(a,7)+E(b,7)] bytes, where w=2 or 3 is the distance width. This includes the extra token. With a fixed (start, distance) and arbitrary feasible lengths, extending a match cannot reduce the saving; the extension charge can increase by at most one byte per extra covered byte. The candidate comprises at most one improvement per frozen run. This does not minimize arbitrary parses. Minimizing each run independently is valid within this restricted class because the original follow-on matches and existing match boundaries stay fixed.

The primary reference for pricing the actual encoded phrases of an LZ parse is Ferragina, Nitto and Venturini, *On the Bit-Complexity of Lempel-Ziv Compression* (2013), https://pages.di.unipi.it/rossano/assets/pdf/papers/SICOMP13.pdf. Farruggia, Ferragina, Frangioni and Venturini's *Bicriteria Data Compression* identifies distance and phrase count as machine-dependent decoder time costs and shows how to trade them against bytes, https://pages.di.unipi.it/frangio/papers/BiCriteriaCompression.pdf. Neither paper gives the wall time of these implementations.

## Offline experiment

Sixteen real development and transfer files use up to four slices each (64 KiB and 256 KiB prefixes, 256 KiB at offset 1 MiB, 1 MiB at offset 2 MiB, subject to file length). Prefix slices overlap; rows are not statistically independent. The eight project holdout files have been used in earlier work. The cases are source-disjoint across the development and transfer groups, but this is not a new sealed acceptance corpus. The 62 baseline inputs and 310 independently decoded arms are recorded in raw.csv with source SHA-256 and exact sizes.

| Group | Slices | Baseline bytes | Full repair bytes | Size saving | Extra / original phrases | Narrow-only saving | Sparse split saving |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Development | 30 | 6,540,460 | 6,371,444 | 2.584% | 91,353 / 478,701 | 0.077% | 0.032% |
| Transfer | 32 | 7,886,602 | 7,714,650 | 2.180% | 96,467 / 768,408 | 0.083% | 0.028% |

All 32 transfer cases shrank 0.120% to 7.475%. One development case tied. Unchanged parses were first rewritten to an identical baseline byte sequence as a format check, and the stock C decoder restored every variant exactly. The full variant used a 65,536-entry four-byte index, one candidate per position and at most 64 bytes per comparison. Its cost is O(n) in time for fixed match cap and O(2^16) 32-bit words of scratch. The offline diagnostic allocates output buffers and scans the stock result. It gives a directly achievable size baseline in the existing format and provides no throughput guarantee. The cheap sparse split (one indexed position in four, require at least 32 literals and three bytes savings) gave little benefit.

Let R=N/compressed_bytes and V=N/(encode_wall+decode_wall). For fixed input, R×V is proportional to 1/[compressed_bytes × total_wall]. On the 32 combined transfer slices a hypothetical equal relative slowdown may not exceed 2.229% to improve this product. For Pareto dominance, total time must also be at or below baseline. Per-file bounds differ. For operations with a different restore frequency, use E+qD and declare q explicitly.

## Native encoder experiment

The supplied single-pass patch leaves the baseline hash parser and its repeat distance history intact, maintaining a separate 64 KiB four-byte table. It inserts positions inside existing literal runs and one position per original match. It scores a single split per run with the exact formula and emits only positive savings. A failed secondary allocation falls back to original encoding. The decoder and wire format are unchanged. The primary parse's work plus at most 64 comparisons per index hit gives O(n) for fixed cap, with O(64 KiB) additional memory per encoder worker. This is intentionally a research patch. The full offline near-offset replacement and all-input position index are omitted in the prototype.

Run from this directory with an ordinary stock build:
```
python3 run.py /path/to/project-review-data /path/to/kolmo-source
python3 timing.py /path/to/project-review-data /path/to/kolmo-source
```

The C benchmarks time raw KLZ-1 encoding and ordinary stock decoding in the same optimized executable. T1 runs in the caller and T4 uses four independent input blocks; n is each block's length. Each timed phase processes at least 1 MiB per worker. Hash verification and file reads are outside timings. All outputs are compared to source after each timed pair arm. The first pair warms both arms, then nine randomized-order pairs are recorded. Baseline and candidate are built with the same Apple M5 native compiler flags. T4 measurements include worker creation/join separately in each phase. These are warm memory, standalone codec calls, not whole-Kolmo archive wall timings, and end-to-end checks on sealed files and other CPUs are still necessary for any deployment.

### Decision after actual paired timings

The single-pass prototype shrank every one of 24 contexts but lost the ratio × combined throughput metric in **every** context. This result persisted across development English and transfer Polish prose, another text source, OWID CSV, Mozilla binary and BF16 weights, independently on 64 KiB and 256 KiB blocks and at one and four concurrent workers. There were 216 randomized measured pairs (432 measured arms); 214/216 individual product comparisons lost. Median product changes were −5.53% to −78.37%, while size improved 0.06–6.55%. The transfer Polish prose 256 KiB/T4 case came closest at −1.43% bytes, +7.40% combined time, −5.53% product. The BF16 64 KiB/T4 case reduced size 0.06% but increased total time 362.60%. The prototype passed 3,850 additional valid tiny/boundary input tests using the existing bound and the independent stock decoder. The exhaustive split math is valuable for identifying missed phrases, but this implementation is **rejected for every tested lane**. Raw cost evidence does not justify an extra match index in the fast preset. If a future SIMD or hardware-prefetch implementation can reclaim these matches while reusing *existing* lookups, use the per-cell bound and rerun independent source-family, archive, memory, and x86 tests. The current evidence affords no measured leadership claim over a rival.
