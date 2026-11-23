# Routing plan for the next wave

> **Since 0.0.1 (container version 12)** Kolmo has no library coders. This document was written before that, and its
> coder names map as follows in 0.0.x (`src/codecs/native.c`): LZ4 → KLZ level 1 and LZ4-HC → KLZ level 3 (28); zstd
> level 1-2 / 3-5 / 6-9 / 10-15 / 16-18 / 19+ → KLZH2 (32) level 1 / 3 / 5 / 6 / 8 / 9; Brotli → KLZH2 level 7; LZMA2 →
> KLZMA (24); zstd's Huffman (HUF) → khuf (27); XXH3/XXH64 → `kolmo_khash64`. Dictionaries
> and trained profiles are removed. Figures below were measured with the library coders.

Status: rc3 wave 2, router lane, 17 November 2026. Companion to `docs/ROUTING-MAP.md` (what each input type gets
today, per level). Every gain below is measured unless it says *estimate*; the evidence is in
`benchmark/rc3/router/RESULTS.md` (cloud VM: x86-64, 4 vCPU, one thread per tool, four tools at once) unless another
file is named. Ordered by expected gain per unit of work; items marked **owner** trade decode speed or `-x` size for
something else and need a decision before they are built.

## Done in this wave (router lane)

| # | change | output | measured |
|---|---|---|---|
| R0 | `-x`: block sorting coded in full on every block (binary ones too, beside CM when a thread is free); LZMA2's slice gate loosened from "3% smaller than CM" to "within 25% of CM" | changes only where an alternative is smaller | `-x` was larger than `ratio` on 2 of 56 files; now on none. osdb −0.97%, owid-energy −0.98%; the rest of the corpus in RESULTS |
| R1 | GR6-L3's dictionary-free `lzp` phrases and level race, considered for codec 26 | none (not ported) | GR6-L3 beats rc3 `-x` only where GR6+ is already chosen (covid −0.06%, OWID CO2 −0.5%, from its word level, not `lzp`); on logs and JSON lines `lzp` is 5–13% smaller than GR6+ but stays 19–100% larger than CM. It belongs to item 3 (GR6 at `ratio`), where it pays |

## Next wave, in order

| # | what to build | type | expected gain | cost | evidence |
|---|---|---|---|---|---|
| 1 | **Colour transform in IMG8** (G, R−G, B−G or YCoCg-R before the median predictor; chosen per file by the planner's image trial) | RGB/RGBA images, Y4M is unaffected (planar) | *estimate* −30% of the residual entropy on 4 Kodak photos (order-0 of median residuals: 3.05 MB → 2.12 MB); `balanced` from +63% to about +15–20% against JPEG XL e9, `ratio` closer to parity (estimate) | new IMG8 variant (graph program flag, container version stays 11 if the flag fits the IMG8 node; else 12), SIMD forward/inverse is two subtractions per pixel | RESULTS §5 |
| 2 | **Context-modelled image residuals at `hc`/`ratio`** (LOCO-I/CALIC-style context per pixel, rANS or the CM image model at reduced order) | 8- and 16-bit images | `ratio` today is +25% vs JPEG XL on RGB and 1.8% behind JPEG-LS on 16-bit CT (rc2 MS); `-x` (CM image model) is already −9.7% vs JPEG XL | medium: a new leaf or a light codec 17 profile | RESULTS §2 (img8, img16); `benchmark/rc2/results/mt/MT.md` |
| 3 | **owner: GR6+ (codec 26) at `ratio` for line-structured text** (logs, long tables), kept when smaller, with GR6-L3's dictionary-free `lzp` phrase level and level race ported for this tier | logs, CSV/TSV long and wide | against `ratio`, GR6+ as built / with `lzp`: Apache −19% / −30%, long CSV −16.5% / −16.5%, covid −8.4% / −8.4%, OWID CO2 −7.8% / −8.3%, HDFS −3.2% / −9.3%, Linux +1.7% / −7.0%; not chosen (larger) on SSH, energy, prose, JSON lines | codec built; `lzp` is ~150 lines of the research coder plus a format flag; decode 2–6 MB/s per core against `ratio`'s 100–230 MB/s on these types; compress 1–10 s per file against `ratio`'s 0.1–5 s | RESULTS §4 |
| 4 | **owner: FASTQ family coder (codec 16) at `ratio`** | FASTQ | −10.4% (5,479,320 → 4,908,324 on 24 MB) | already built; compress 2.3–3.2× and decode 3.6–4.9× slower than `ratio`'s BWT (1–2 threads) | RESULTS §5 |
| 5 | **`balanced`: block 0 as a zstd dictionary for the other blocks** | text, JSON lines, source | bound: one block instead of ~8 is −0.8% (enwik8) to −5.3% (JSON lines) smaller; a shared dictionary keeps parallel decode (one block is 1.6–3.7× slower to decode at 4 threads) | small (zstd prefix dictionary API); decode of blocks 1..n waits for block 0 | RESULTS §5 |
| 6 | **Already-compressed input in the slow modes**: a cheap probe (order-0 entropy ≥ 7.95 bits and a zstd −1 slice ≥ 99%) stores the block | JPEG, PNG, gzip, zip members | `ratio` codes such data at 4 MB/s and `-x` at 0.2 MB/s for 0.5% (ratio) and 1.2% (-x) gain; `eco`/`hc` gain 0–0.4%. Owner choice for `ratio`/`-x` (they promise the smallest output) | small | RESULTS §2 (compressed) |
| 7 | **GR6+ trial cost on wide tables** (Mac regression 5): skip or bound formula discovery above ~100 columns | wide CSV | energy (130 columns): GR6+ alone is 23% larger than `-x` and costs +126% compress time on the M5 for nothing; OWID CO2 (79 columns) must keep it (−0.7% to −3.7%) | small; needs a second wide table to confirm the threshold | `benchmark/rc3/gr6/RESULTS.md`, RESULTS §4 |
| 8 | **`balanced` graph-leaf trials only when an estimate says they can win** (Mac regression 1) | DEC64 floats, wide tables | 9× slower compress on DEC64 floats for −5.5%; target: same bytes at rc2 speed where the estimate rules out codecs 21–23 | medium | `benchmark/rc3/mac/RESULTS.md` |
| 9 | **kvid: several blocks per frame for decode threads** (Mac regression 4) | Y4M | decode at 10 threads back to rc2's speed (1.3–1.9× faster) at a small size cost | medium | `benchmark/rc3/mac/RESULTS.md` |
| 10 | **`ratio` decimal planes: zstd 19 as a leaf alternative** | float64 decimal arrays | `ratio` is 0.4–1.3% larger than `eco`/`hc` on decimal doubles (co2.f64, co2_f64.npy) | small: one more per-leaf candidate | RESULTS §2 (mode inversions) |
| 11 | **owner: `-x` on model weights**: bf16 safetensors run CM for 208 s (one thread, 45 MB) for −0.73% against `ratio` (6.6 s); a weights probe (float planes chosen by the planner, CM slice not within 1%) would skip CM | model weights | ~30× faster `-x` on bf16 weights for +0.7% size | small | RESULTS §2 (weights) |
| 12 | **Format-2 routes per archive member**: a WAV or Y4M member of a tar goes to kaud / kvid like a whole file | archives with audio or video | the member's own gain (WAV files alone: −29% to −32% against the best general tool, −5% to −9% against FLAC -8) | medium: the audio and video routes are file-level today (`core/compress.c`) | RESULTS §2 (archive, audio) |

## GPU: where it can add value

Measured on the Apple M5 (rc3 gpu lane, `benchmark/rc3/gpu/RESULTS.md`): the GPU suffix sort is 0.30–0.84× of one
CPU core on text and 1.05–2.16× only on inputs `ratio` does not block-sort; a GPU worker beside ten CPU threads gives
0.91–0.98× (no gain); kvid prediction on the GPU (12.3 ms with copies) is slower than ten CPU threads (6.5 ms); planner
histograms on the GPU lose below 16 MiB and pay 65 ms of set-up per process. **No routing decision should send work
to the GPU today.**

The 6-bit study's two-tier design (static-table transform + interleaved rANS on the GPU, CM on the CPU) is
unmeasured. The parts that fit a GPU are the ones with thousands of independent items and no carried state: formula
search over column pairs (4.2× on 4 CPU threads in the study; the wide-table cost of item 7), field splitting and
hashing, dictionary building by sort-and-reduce. It would be a new speed tier for large structured inputs already in
GPU memory (analytics, ML pipelines); for files on disk the transfer and set-up cost what the M5 measurements show.
Recommended order: item 7 on CPU threads first (formula search interleaved across threads, as in the study); a GPU
formula search only if wide tables of 100+ MB become a target.

## Not worth building (measured)

- GR6-L3 `lzp` phrases or the level race inside codec 26 (R1 above).
- A phrase dictionary on top of `lzp` (the study: at most +0.25% even with oracle labels).
- x86 call/jump filter on Silesia mozilla (not chosen by the planner): xz with BCJ is 0.6% larger than without on
  mozilla and 2.7% larger on a Linux x86-64 `.so`.
