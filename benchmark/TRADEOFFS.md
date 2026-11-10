# Weighing ratio against speed

A compressor change rarely improves everything. It usually makes files smaller and slower, or
faster and bigger. This page records how kolmo decides whether such a trade is worth keeping: the
published research and industry practice we rely on, the models we derive from them, and how they
applied to a real decision (the `claude/graph` branch, measured 2026-11-10).

To rerun the models with your own prices, link speeds or measurements, use
[`tradeoff_model.py`](tradeoff_model.py):

```sh
python3 benchmark/tradeoff_model.py                                 # the 2026-11-10 measurements
python3 benchmark/tradeoff_model.py --storage 0.004 --reads 1       # e.g. cold storage, read once
python3 benchmark/tradeoff_model.py --pair "mine:3.1,300,1100,3.2,280,1000"
```

## What the literature says

There is no single industry threshold. Each source below implies its own exchange rate between
bytes and time, and the right one depends on where the data goes.

| Source | Finding | What it implies for a trade |
|---|---|---|
| Google fleet study (CDPU, ISCA 2023) [1] | Compression takes 2.9% of fleet CPU cycles, 56% of them in decompression. Each compressed byte is decompressed 3.3 times on average. Over 95% of bytes are compressed with Snappy or with zstd at level 3 or lower; fewer than 0.002% of zstd bytes use level 12 or higher. High zstd levels give 1.35× the ratio of low levels for 2.39× the compression cost per byte, which the authors call a non-starter for a service that spends a quarter of its cycles compressing. | On hot paths, operators decline a 26% size cut that costs 58% of compression speed. Decompression speed weighs about three times as much as compression speed. |
| Snappy design goals [2] | About 10× faster than zlib's fastest mode, with output 20–100% bigger. Aims for very high speed and reasonable compression, not maximum ratio. | In the fastest tier, users pay for speed, not ratio. |
| BtrBlocks (SIGMOD 2023) [3] | On 100 Gbit cloud instances, data-lake scans become CPU-bound on decompression. A format that decodes faster made S3 scans 2.2× faster and 1.8× cheaper than Parquet, and loaded full datasets 1.8× cheaper than Parquet with Snappy or zstd (2.6× cheaper than uncompressed Parquet). | On fast links, decompression speed decides cost, ahead of a few percent of ratio. |
| OpenZL at Meta (2025) [4, 5] | Deployed for 10–18% smaller files: a columnar store −10%, logging −15%, a log aggregator −18%, model checkpoints −17%, bf16 embeddings −30%. One deployment also used 5% less CPU than zstd level 6. Results are presented as a Pareto frontier of ratio against speed. | A 10–18% size gain is enough for production adoption on stored data. Settings are compared by Pareto dominance, not a fixed threshold. |
| LZ4 and Squash benchmarks [6, 7] | Rank codecs by total time: compress, transfer at a stated link speed, decompress. | A ratio gain is worth it only below a break-even link speed. |
| Cloudflare [8, 9] | Compresses responses on the fly at Brotli level 4 by default, although the strongest level is about 18% smaller. | On the request path, a low level is kept despite an 18% size gain at the highest level. |
| Dropbox [10] | Its storage pipeline needed compression that runs well above line speed. It notes that a greedy encoder can waste up to about 10% of space but keeps up with almost any line speed. | Throughput limits come first, then ratio. |
| Weissman score [11, 12] | A single figure combining ratio and the log of compression time against a reference compressor. | Not used: the score is sensitive to time units and is criticised for weighting time over ratio. |

## The models

`tradeoff_model.py` implements these four models.

**1. Exchange rate.** The percent of bytes saved per percent of combined (compress + decompress)
speed lost. Google's fleet declines high zstd levels on hot paths at a rate of about **0.45**
(26% fewer bytes for 58% less compression speed) [1]. A speed-tier change below that rate is worse
than a trade production operators already turn down.

**2. Break-even link speed.** With no overlap between compute and transfer, total time is
`compress + size / bandwidth + decompress`. A smaller but slower candidate wins only on links slower
than `bytes saved / extra time` [6, 7]. We report two figures:
- **round trip**: the data is compressed and sent each time
- **read only**: the data was compressed earlier and is fetched and decompressed

**3. Cloud cost.** The extra CPU cost per GB (compression once, decompression `reads` times)
against the storage cost saved per GB-month. The lifetime at which they are equal is the
**break-even lifetime**; data kept longer pays for the slower setting. Defaults, all list prices:
- S3 Standard storage at $0.023/GB-month and internet egress at $0.09/GB [13]
- a vCPU at $0.054/hour, from the $3.89/hour c5n.18xlarge used in [3]
- 3.3 reads per compressed byte, from [1]

**4. Pareto dominance.** A setting loses outright only if another setting is at least as good on
both ratio and speed [4]. Settings that are better on one axis and worse on the other are new
trade-off points, not regressions.

## How kolmo weighs a change

| Tier | Modes | Rule |
|---|---|---|
| Speed | `rt`, `fast` | The default setting must not be dominated by another setting, and any trade must lower total time at a 1 Gbit/s reference link (model 2). Heavier analysis belongs in an explicit level, not the default. |
| Archive | `hc`, `max`, `ultra` | Judged by the cost model (model 3) with data kept 30 days or more. A ratio gain is kept when it pays back within that lifetime. |
| All | — | Files whose output does not change may not get more than 5% slower. Nothing in the literature supports paying time for no gain in bytes. |

Two further bars stay in place:
- **Public leadership claims.** [`benchmark/holdout`](holdout/) requires at least 20% fewer bytes
  with no extra time or memory against both incumbents. That is stricter than what Meta needed to
  adopt OpenZL (10–18%) and is meant for claims against other compressors, not for internal changes.
- **Fresh data.** Measurements on files used for tuning flatter a change, so a final decision uses
  a newly frozen input set.

## Worked example: `claude/graph` against `main` (2026-11-10)

**Setup.**
- **Builds:** `main` at `1a92e2c` against `claude/graph` at `8a6f623`.
- **Machine:** Apple M5, macOS 27.2, one thread.
- **Data:** 35 files, 474 MB in total: tables, logs, prose, images, integer and float arrays, and model weights.
- **Timing:** each pair of runs went in random order, with five pairs per file and setting, and every output was checked byte for byte.
- **Caveats:**
  - Ratios come from the full benchmark run.
  - The `fast -q` and `max` speeds come from single benchmark runs rather than the paired test.
  - The files were used while tuning `claude/graph`, so its gains are likely overstated.

### Measured

| Setting | `main` ratio | `main` MB/s | `claude/graph` ratio | `claude/graph` MB/s |
|---|---:|---:|---:|---:|
| `fast -q` | — | — | 3.236× | 275.5 |
| `fast` | 3.230× | 256.7 | 3.302× | 234.1 |
| `fast -l 9` | 3.410× | 121.3 | 3.475× | 114.1 |
| `hc -l 9` | 3.466× | 93.9 | 3.527× | 92.0 |
| `max -l 1` | 4.038× | 9.0 | 3.908× | 27.4 |
| `max` | 4.038× | 9.0 | 4.119× | 8.1 |

MB/s is combined speed: original bytes divided by compression time plus decompression time.

In `fast`, the slowdown has two parts:
- **12 files (80 MB) got about 11% smaller** and about 37% slower. This is a real trade.
- **23 files (418 MB) came out byte-identical** and were still 8% slower. This is overhead from
  encodings the planner tries and then discards.

### Models applied

| Setting | Bytes | Combined speed | Exchange rate | Break-even link (round trip / read only) | Break-even lifetime |
|---|---:|---:|---:|---|---:|
| `fast` | −2.2% | −8.8% | 0.25 | 144 / 437 Mbit/s | 1.9 days |
| `fast -l 9` | −1.9% | −6.0% | 0.31 | 83 / 303 Mbit/s | 3.1 days |
| `hc -l 9` | −1.7% | −2.1% | 0.84 | 178 / 1,334 Mbit/s | 1.2 days |

- **Exchange rate.** Only `hc -l 9` beats Google's 0.45. In `fast`, even the files that got smaller
  trade at 0.30 (11.2% fewer bytes for 37.5% less speed).
- **Link speed.** On 1–100 Gbit datacenter or cloud links, `claude/graph` loses on total time in
  every setting. It wins on WAN and mobile links below the figures shown.
- **Cost.** The extra CPU costs about $10⁻⁵ per GB, and the storage saved is worth about
  $1.5×10⁻⁴ per GB-month. Any data kept longer than one to three days pays for the slower settings.
- **Pareto.** `fast -q` (3.236×, 275.5 MB/s) dominates `main`'s `fast` (3.230×, 255.5 MB/s in the
  same run). `fast -l 9` (3.475×, 114.1 MB/s) dominates `main`'s `hc -l 9` (3.466×, 93.9 MB/s).
  `max -l 1` is a new point: 3× faster than `max` for about 3% more bytes. `main`'s `fast -l 9` is
  the only `main` setting that no `claude/graph` setting dominates.

### Decision

| Change | Keep? | Reason |
|---|---|---|
| Ratio gains in `hc` and `max` | Yes | Archive tier: pays back within days, and `hc -l 9` trades at 0.84 |
| `max -l 1` as a faster preset | Yes | A new Pareto point between `hc` and `max`, like zstd's intermediate levels |
| Heavier analysis as the `fast` default | No | Below the 0.45 exchange rate and loses on fast links. `fast` should behave like `-q`, which dominates `main`, with the heavier analysis kept at `fast -l 9` and `hc`. |
| Slowdown on byte-identical output | No | Breaks the 5% rule for files whose output does not change. Fix before merging. |

## Follow-up: the decision verified and refined (2026-11-10, `claude/graph` before 1.1.0)

An independent run on the same machine (35 files, 497 MB, median of three) confirmed the measurements above and
corrected two conclusions.

- **Where the slowdown on unchanged output came from.** Compression: planner trials on binary files (x-ray −29%,
  pickle −23%, mr −21%). Decompression: not the planner. The Codex build showed the same −10% against `main` with no
  planner change, because the CLI copied its staged output over an existing output file. Both are fixed in 1.1.0
  (cheaper trials; atomic replace).
- **`fast` = `-q` is not a free speed-up.** Against 1.0.1's `fast`, `-q` makes logs 31% bigger (14.1× → 9.8×) and
  `nci` 28% bigger, because it drops transforms 1.0.1 already applied; the totals only look level because `-q` kept new
  graph gains that fail the same link rule. The 1 Gbit/s rule (model 2) applied consistently also rejects 1.0.1's own
  log transform (42% more total time), so it is too strict to be the only test for the speed tier.
- **Rule adopted for `fast`:** model 1 per transform. A transform stays on by default when it saves at least 0.45% of
  bytes per 1% of combined speed lost, measured against 1.0.1.

| Transform at `fast` | Bytes saved | Combined speed | Rate | Default at `fast` |
|---|---:|---:|---:|---|
| Minified JSON | 51% | −24% | 2.1 | on |
| Wide table (graph) | 43% | −50% | 0.85 | on |
| Logs (already in 1.0.1) | 31% | −52% | 0.60 | on |
| BF16 floats | 15% | +7% | dominates | on |
| float32 weights | 2.7% | +12% | dominates | on |
| 8-bit images | 10–11% | −25% to −27% | 0.38–0.45 | on (files under 1 MB, where fixed trial cost dominates; revisit with larger images) |

Speeds in this table are from a 2-core cloud VM (±10% noise), after the 1.1.0 fixes. Files whose output is unchanged
are within the 5% limit there; the Apple M5 re-run is the reference measurement.

## References

1. S. Karandikar et al., *CDPU: Co-designing Compression and Decompression Processing Units for
   Hyperscale Systems*, ISCA 2023.
   [Paper page](https://research.google/pubs/cdpu-co-designing-compression-and-decompression-processing-units-for-hyperscale-systems/) ·
   [PDF](https://par.nsf.gov/servlets/purl/10432064)
2. Google, Snappy README.
   [Source](https://fuchsia.googlesource.com/third_party/snappy/+/ef5598aa0e867db5243188751054c9b2223b0e5f/README)
3. M. Kuschewski, D. Sauerwein, A. Alhomssi, V. Leis, *BtrBlocks: Efficient Columnar Compression
   for Data Lakes*, SIGMOD 2023.
   [PDF](https://www.cs.cit.tum.de/fileadmin/w00cfj/dis/papers/btrblocks.pdf)
4. Y. Collet et al., *OpenZL: A Graph-Based Model for Compression*, arXiv:2510.03203, 2025.
   [arXiv](https://arxiv.org/abs/2510.03203)
5. Meta Engineering, *OpenZL: An open source format-aware compression framework*, 2025.
   [Blog](https://engineering.fb.com/2025/10/06/developer-tools/openzl-open-source-format-aware-compression-framework)
6. LZ4 benchmark (transfer plus decompression time at 1000 Mbit/s).
   [Page](https://fuchsia.googlesource.com/third_party/lz4/+/refs/heads/upstream/gh-pages/index.html)
7. Squash Compression Benchmark.
   [Site](https://quixdb.github.io/squash-benchmark)
8. Cloudflare, *Results of experimenting with Brotli for dynamic web content*.
   [Blog](https://blog.cloudflare.com/results-experimenting-brotli/)
9. Cloudflare, *This is Brotli from origin*.
   [Blog](https://blog.cloudflare.com/this-is-brotli-from-origin/)
10. Dropbox, *Lossless compression with Brotli in Rust* and *Broccoli: Syncing faster by syncing less*.
    [Brotli in Rust](https://dropbox.tech/infrastructure/lossless-compression-with-brotli) ·
    [Broccoli](https://dropbox.tech/infrastructure/-broccoli--syncing-faster-by-syncing-less)
11. Weissman score.
    [Wikipedia](https://en.wikipedia.org/wiki/Weissman_score)
12. J. Kaufman, *Weissman Scores: Useful?*
    [Blog](https://jefftk.com/p/weissman-scores-useful)
13. Amazon S3 pricing.
    [Page](https://aws.amazon.com/s3/pricing/)

Further context:
- S. Kanev et al., *Profiling a Warehouse-Scale Computer*, ISCA 2015
  ([PDF](https://sites.coecis.cornell.edu/isca50retrospective/files/2023/06/kanev_2015_profiling.pdf)):
  shared low-level routines, compression among them, take close to 30% of fleet cycles.
- Uber, *Reducing logging cost by two orders of magnitude using CLP*
  ([blog](https://www.uber.com/blog/reducing-logging-cost-by-two-orders-of-magnitude-using-clp)):
  a 169× ratio on log data allowed 10× longer retention.
