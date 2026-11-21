
## Where Kolmo is not the best

| type | level | leader | gap | why | plan |
|---|---|---|---:|---|---|
| RGB photographs | `balanced`–`ratio` | JPEG XL e9 (lossless) | +25% (`ratio`) to +63% (`balanced`, RGB only) | no colour decorrelation; order-0 coding of median residuals | items 1, 2 |
| 8-bit photographs | `balanced` | JPEG-LS | +2.6% | same | item 1 |
| 16-bit CT | `balanced` | JPEG-LS (Mac) | +1.8% | no context modelling of residuals below `-x` | item 2 |
| 16-bit MR | `-x` | JPEG XL e9 (Mac) | +5.9% | same | item 2 |
| text, source, JSON lines | `balanced` | zstd -3 (one stream) | +1% to +5% | eight independent blocks for parallel decode | item 5 |
| any text | `eco` | Brotli 11 | +9% to +15% | `eco` is Brotli 8 by design (25–86× faster than Brotli 11) | — |
| decimal float arrays | `ratio` | Kolmo `hc` | +0.4% to +1.3% | block-sorted decimal planes | item 10 |
| WAV inside an archive | every level | FLAC | (the member is coded as bytes) | the audio route needs the whole file to be one WAV | item 12 |
| already-compressed | `ratio`, `-x` | — | slow, not larger | no incompressibility probe before the slow coders | item 6 |
| FASTQ | `ratio` | Kolmo `-x` (codec 16) | +11.6% | codec 16 only at `-x` | item 4 |
| FASTQ (Mac) | decode speed | CRAM 3.1 | 0.25× decode speed | arithmetic-coded family streams | — |
| audio (Mac) | decode speed | FLAC | 0.25× (p0) to 0.08× (p1) | adaptive coders | — |
| lines of text: logs, long tables | `ratio` | GR6+ / GR6-L3 (Kolmo's own) | 7–30% | GR6 only at `-x` | item 3 |

Fixed in this branch: `-x` was larger than `ratio` on osdb (+1.0%) and owid-energy (+1.0%) because its LZMA2 and
block-sorting alternatives were skipped by a pessimistic slice test (R0 in the plan).

Not measured here (no build or no data on this VM): CRAM, ZipNN, OptimFROG/Monkey's Audio, zpaq/paq8/cmix, DNA
specialists, JPEG/deflate recompressors. Where the Mac measured one, the table cites `benchmark/rc2/results/mt/MT.md`.

## GPU: alone or beside the CPU threads

| work | measured | verdict |
|---|---|---|
| suffix sort / BWT (`ratio`, `-x` text) | M5: 0.30–0.84× of one P-core on text, logs, JSON, DNA; 1.05–2.16× on x-ray, sao, bf16 (which `ratio` does not block-sort) | no route uses it |
| GPU worker beside ten CPU threads (block scheduler) | M5: 0.91–0.98× of CPU-only on block-sorted text; no effect on one-block files | dropped from the default build |
| kvid strip prediction | M5: 12.3 ms (6 ms kernel + copies) vs 6.5 ms on ten threads | CPU threads |
| planner histograms | M5: slower below 16 MiB, +65 ms Metal set-up per process | CPU |
| GR6 two-tier design (static transform + interleaved rANS on the GPU; formula search; field hashing; dictionary sort-reduce) | unmeasured (6-bit study §11) | candidate speed tier for very large structured inputs already in GPU memory; formula search is the first part worth a GPU, after a CPU-thread version (plan) |

Sources: `benchmark/rc3/gpu/RESULTS.md`, `research/CODECS.md` (GPU rows), branch `claude/dazzling-maxwell-p8y4u0`
`research/FINDINGS.md` §11.

## Evidence index

| evidence | what |
|---|---|
| `benchmark/rc3/router/RESULTS.md` | this lane: the matrix (25 types × 6 levels × 7 general tools and the specialists), the `-x` fix, GR6-L3 against rc3 `-x`, the experiments behind the plan |
| `benchmark/rc3/router/matrix.jsonl`, `xnew.jsonl` | raw rows (sizes, times, routes) |
| `benchmark/rc3/mac/RESULTS.md` | rc3 against rc2 on the Apple M5, 1 and 10 threads |
| `benchmark/rc2/results/mt/MT.md` | rc2 against the leaders with all cores (CRAM, ZipNN, FFV1, JPEG XL, JPEG-LS, FLAC, zpaq) |
| `benchmark/rc3/gr6/RESULTS.md`, `benchmark/rc3/xmode/RESULTS.md`, `benchmark/rc3/bwt/RESULTS.md`, `benchmark/rc3/format2/RESULTS.md` | the codecs each route uses |
| `research/CODECS.md` | every codec and transform with its measured impact and verdict |
