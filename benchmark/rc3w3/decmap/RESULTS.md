# Decode maps: prebuilt decoder tables and faster decode paths (rc3 wave 3, decmap lane)

Branch `claude/rc3w3-decmap-20261118`, from rc3 `58f7cee`. The owner's ask: *test a prebuilt decompression map that
expedites decompression further and avoids search and lookups as much as possible while optimizing the storage; test
it in parallel to see if it adds value.*

**Decision.** No "decode map" stored in the frame pays for itself: every stored candidate measured either stays within
the noise (inverse-BWT sample points) or costs far more bytes than the decode time it could save (Huffman decode
tables). Where the decoder spends its time searching or rebuilding tables, it is cheaper to rebuild faster than to
store. Three **decoder-only** changes are kept. They change no frame (the same bytes are written and read, and the
determinism hashes are unchanged):

| change | where it runs | decode gain (same frames) |
|---|---|---|
| AVX2 word-transform decoder (`src/arch/x86_64/word_avx2.c`, the x86 counterpart of the NEON kernel) | text and JSON blocks with the word transform (`eco`, `hc`; JSON also `balanced`) | text `eco`/`hc` +14.8% (1 thread) / +12.1% (4 threads); JSON +13.7% / +12.1%; best file +23% |
| khuf (codec 27) table build: one 11-bit table instead of two, bit reversal by a constant byte table | codec 27 leaves, KLZH streams (`KOLMO_NATIVE`) | khuf blocks +7.6% to +8.9% on Silesia; 4 KiB blocks −20% decode time |
| KLZH (codec 29) sequence loop: one validation pass (maxima), refills only when the extra bits need them | codec 29 (`KOLMO_NATIVE=lz`) | KLZH1 +10%, KLZH3 +10%, KLZH5 +17% (Silesia, 1 thread) |

With `KOLMO_NATIVE=all`, frames of `balanced`, `eco` and `hc` decode +16.3% (1 thread) / +14.2% (4 threads) faster
(18 file/mode pairs). Every other path is untouched; across the 60 CSV, log, numeric, image, audio and video frames of
all modes the geometric mean is −0.0% (1 thread) / +1.0% (4 threads), i.e. noise.

## Setup

- Cloud VM, 4 vCPUs (x86-64 with AVX2 and AVX-512), gcc 13, `-O3 -march=native`, the repository's flags.
- Files (`/home/user/data/set`, 17): Silesia dickens, webster, xml, nci, mr, x-ray, sao, osdb, samba; OWID CO2 CSV
  (15 MB), covid countries-aggregated CSV (5.5 MB), OWID as JSON records (31 MB, `owid.json`), Loghub Apache, HDFS,
  Linux and OpenStack 2k samples joined four times (5 MB, `logs.log`), OWID population as an int64 NPY, OWID energy as a
  float64 NPY (24 MB), five ESC-50 clips joined (WAV, 2.2 MB), the first 60 frames of akiyo CIF (Y4M, 9 MB).
- Frames: each file compressed once by rc3 `58f7cee` with `-T 4` in `speed`, `balanced`, `eco`, `hc`, `ratio`; six
  files also with `KOLMO_NATIVE=all` (codecs 24, 27, 28, 29; the `n-` rows); `-x` on eight files.
- `dbench.c` (this directory) decodes a frame in memory, best of ≥ 3 runs and ≥ 1 s, the output checked against the
  input's hash. `ab.py` decodes each frame with build A (rc3 `58f7cee`) and build B (this branch), alternating the
  order every round, and keeps the best of 2 rounds (4–6 rounds for the outliers re-measured). One thread count per
  run: `-T 1` and `-T 4` are separate passes, never mixed. Since the frames are the same for both builds, sizes are
  equal by construction. The VM's run-to-run noise is about ±5% per frame (up to ±15% for 10 GB/s `speed` frames at 4
  threads); every outlier beyond ±5% on a path this lane did not touch was re-measured and fell inside it.
- Per-file results: `results/table.md` (from `table.py results`), raw rows in `results/ab_*.jsonl`.

## Where decode time goes (step 1)

Callgrind, one thread, 2 MB samples of each class (`results/callgrind_top.txt`, instructions; cache misses are not
counted, so stages bound by memory latency, such as the inverse BWT and CM table probes, take more of the wall time
than shown):

| mode | class | top of the decoder |
|---|---|---|
| speed | text, JSON, logs | LZ4 decode only (Kolmo's own code < 2%) |
| speed | CSV | table transform: `tbl2_decode` + `tbl2_stream` + decimal formatting 60% |
| speed | images, audio, NPY | LZ4, byte-plane merge (SSE2), shuffle inverse |
| balanced | text | zstd sequences 65% |
| balanced, eco, hc, ratio | CSV | table transform and decimal formatting 50–67% (already SIMD: simd lanes) |
| balanced, eco, ratio | JSON | `transpose.c:decode_block` 44–55% |
| balanced, eco, ratio | logs | `log_decode` 15–27% |
| balanced, hc | images | `img16_inv` 41% |
| balanced, eco, hc | audio | kaud profile 0: Rice decode 46%, LPC restore 30% |
| eco, hc | text | Brotli / zstd 49–62%, **word transform 26–31%** (a byte-table scan per input byte on x86) |
| ratio | text, samba, video | **adaptive QLFC 85%**, inverse BWT (biPSI) 3.4–3.9%, LZP 2.3–3.4% |
| ratio | CSV, logs, NPY, images | LZMA2 44–84% |
| ratio | audio | kaud profile 1 (NLMS cascade) 90% |
| -x | text | CM bit coding 87%, mixer dot/train 10% |
| -x | CSV (GR6+) | GR6+ CM bit coding 72%, init 4.7%, field decisions (`g6_set`, `g6_val`, `g6_unit`) 8.5% |

Searching proper (move-to-front, rank and dictionary lookups, canonical-code search) is small everywhere: the decoders
already index (word codes into 64-byte slots, table-driven Huffman, libsais' 16-way interleaved inverse). The time is
in model arithmetic (QLFC, CM, GR6+, kaud), in vendored coders (zstd, Brotli, LZ4, LZMA2) and in Kolmo's structure
inverses.

## Candidates (step 2)

| candidate | what was measured | result | verdict |
|---|---|---|---|
| **Encoder-built Huffman decode tables in the frame** (khuf, codec 27) | `khufbench.c`: fixed cost per block vs per byte | rc3: 5.6–10 µs per block for the tables (8% of a 128 KiB block, most of a 4 KiB one), 0.48 ns/byte. Shipping the 11-bit tables costs 4 KiB (one symbol per entry) to 8 KiB (two-symbol table) per ≤ 128 KiB block, +3% to +6% of a 1.4× Huffman leaf, against ≤ 8% decode | rejected (bytes ≫ time); the build made faster instead (below) |
| khuf table build, decoder only | same | one table at 11 bits for the two-symbol loop and the tails (was a T-bit and an 11-bit table), bit reversal from a constant 256-byte table (was a bit loop per symbol): 4 KiB blocks 7.8 → 6.2 µs, 16 KiB 13.2 → 11.5 µs, 128 KiB 66.8 → 65.5 µs; natbench Silesia `khuf` 1133 → 1234 MB/s and 1119 → 1203 MB/s (two runs) | **kept** |
| A decode table stored once per file and reused by every block (khuf repeat mode) | upper bound from the numbers above | saves the table header (~130 B per 128 KiB chunk, 0.1%) and the table build, now ~4 µs per chunk (≤ 3% of a 128 KiB chunk); needs a format change and a decoder that keeps state across chunks | not built (< 5%) |
| zstd / Brotli / LZ4 / LZMA2 tables | — | vendored and untouched (`docs/LAYOUT.md` rule 6); zstd already reuses its Huffman and FSE tables across blocks (repeat modes) | — |
| **Inverse BWT: more sampled indexes** (codec 18) | `bwtbench.c`: whole codec-18 decode with 8 → 255 sampled indexes, 1 and 4 threads; `unbwtbench.c`: the inverse alone | dickens, 1 thread: 19.4 MB/s with rc3's 9 indexes, 19.8 / 19.9 / 22.0 / 20.6 with 19 / 38 / 77 / 154 (not monotonic); 4 threads 40.9–43.1 → 43.0–44.3; samba +0–3% (1 and 4 threads); webster −5% to +4%; nci −11% to +1%; cost +32 to +900 bytes per block. The inverse is 3.4–3.9% of `ratio` text decode (instructions) and libsais already interleaves up to 16 chains; QLFC is 85% | rejected (inside the noise, costs bytes) |
| **GR6+: precomputed move-to-front / rank tables, dictionary order** (codec 26) | callgrind of a GR6+ CSV frame | all field decisions together (`g6_set`, `g6_val`, `g6_unit`) are 8.5% of the instructions; the CM bit coder is 72%. Removing every search would give < 5% | not built |
| **CM: squash/stretch/state tables as constant data** (codec 7, GR6+) | timed `cm_init_tables` | already built once per process (`pthread_once`): 0.30 ms, 0.01% of a 5 MB `-x` decode (3.2 s). The bit-history state table is built at the same time. Hash-slot hints: the xmode lane measured prefetch of the hashed buckets as no gain | no change |
| GR6+ table initialisation | callgrind | 4.7% of a 2 MB GR6+ decode (8 tables of 4 Mi entries set to 1 << 31); could start from zero pages with a re-based counter | not built (< 5%, `-x` tables only) |
| **LZ: literal/length tables, branchless copy** — KLZH (codec 29) | natbench, 1 thread, 6 Silesia files | the length codes already go through a base/extra-bits table (`kz_ltab`). rc3 checked the codes in two passes and refilled the bit buffer three times per sequence. One pass over the code maxima and refills only when the buffered bits run short: KLZH1 1250 → 1376 MB/s, KLZH3 1138 → 1256, KLZH5 1181 → 1385 (best of 3 runs each); `results/natbench_klzh.txt` | **kept** |
| KLZ (codec 28) | natbench | already byte-aligned with a branch-light fast path; no change made (2295–2431 MB/s both builds) | no change |
| **Word transform: direct-index decode** (block flag 9) | dbench A/B | the decoder already indexes words (64-byte slots); on x86 it found the next code by a table lookup per input byte. The AVX2 kernel classifies 64 bytes at once (special-byte bitmap through two in-lane shuffles) and walks the tokens with count-trailing-zeros, as the NEON kernel does on arm64 | **kept** |
| kaud profile 0: whole-word bit-buffer refill | dbench A/B, 4 WAV frames | 147 → 147, 148 → 150, 159 → 155, 15.8 → 15.8 MB/s (±2%): the Rice loop and LPC restore are compute-bound, not refill-bound | reverted |

## Results (step 3): decode MB/s, rc3 `58f7cee` against this branch, same frames

Per-mode geometric means of the decode speed ratio (17 files per default mode, 6 per `KOLMO_NATIVE` mode, 8 at `-x`):

| mode | files | 1 thread | 4 threads |
|---|---:|---:|---:|
| speed | 17 | −1.1% | +1.5% |
| balanced | 17 | +0.8% | +0.8% |
| eco | 17 | +3.6% | +3.5% |
| hc | 17 | +3.5% | +3.9% |
| ratio | 17 | +0.9% | −0.0% |
| -x | 8 | +0.6% | +1.3% |
| `KOLMO_NATIVE=all` speed | 6 | +1.9% | −0.5% |
| `KOLMO_NATIVE=all` balanced | 6 | +12.0% | +10.7% |
| `KOLMO_NATIVE=all` eco | 6 | +16.3% | +17.2% |
| `KOLMO_NATIVE=all` hc | 6 | +20.7% | +14.8% |
| `KOLMO_NATIVE=all` ratio | 6 | +0.2% | +1.0% |

By group (geometric means):

| group | pairs | 1 thread | 4 threads |
|---|---:|---:|---:|
| text, `eco`/`hc` (dickens, webster, xml) | 6 | +14.8% | +12.1% |
| JSON, `balanced`/`eco`/`hc` (owid.json) | 3 | +13.7% | +12.1% |
| native codecs, `balanced`/`eco`/`hc` | 18 | +16.3% | +14.2% |
| CSV, logs, numeric, images, audio, video (all default modes; untouched paths) | 60 | −0.0% | +1.0% |

Selected rows (all rows: `results/table.md`):

| frame | raw bytes | frame bytes | 1 thread rc3 → branch MB/s | 4 threads rc3 → branch MB/s |
|---|---:|---:|---:|---:|
| eco/dickens | 10192446 | 2881188 | 276.6 → 330.4 (+19.4%) | 295.2 → 308.5 (+4.5%) |
| hc/dickens | 10192446 | 2728251 | 538.7 → 664.3 (+23.3%) | 493.7 → 604.5 (+22.4%) |
| eco/webster | 41458703 | 9834135 | 410.8 → 475.4 (+15.7%) | 1029.1 → 1254.4 (+21.9%) |
| hc/webster | 41458703 | 8858048 | 861.1 → 852.8 (−1.0%, 4 rounds) | 2365.4 → 2334.6 (−1.3%) |
| eco/xml | 5345280 | 468409 | 556.4 → 621.3 (+11.7%) | 587.5 → 634.7 (+8.0%) |
| hc/xml | 5345280 | 443624 | 851.3 → 1033.7 (+21.4%) | 859.4 → 1026.6 (+19.5%) |
| balanced/owid.json | 30926608 | 1411138 | 603.2 → 666.5 (+10.5%) | 2355.4 → 2630.9 (+11.7%) |
| eco/owid.json | 30926608 | 1217857 | 424.2 → 507.9 (+19.7%) | 821.7 → 912.4 (+11.0%) |
| hc/owid.json | 30926608 | 847144 | 611.4 → 679.9 (+11.2%) | 1133.6 → 1288.5 (+13.7%) |
| n-hc/dickens | 10192446 | 2968964 | 428.6 → 644.8 (+50.5%) | 507.4 → 673.5 (+32.7%) |
| n-eco/xml | 5345280 | 547457 | 828.7 → 1054.1 (+27.2%) | 803.1 → 1002.6 (+24.8%) |
| n-balanced/samba | 21606400 | 5278933 | 1482.7 → 1744.7 (+17.7%) | 5861.2 → 6720.4 (+14.7%) |
| ratio/dickens | 10192446 | 2254558 | 20.8 → 21.5 (+3.6%) | 42.8 → 44.7 (+4.6%) |
| speed/dickens | 10192446 | 6437034 | 3374.0 → 3387.1 (+0.4%, 4 rounds) | 11608.5 → 12405.1 (+6.9%) |

hc/webster gains nothing: its blocks do not take the word transform at `hc`. One `-x` frame stayed slightly negative
after re-measuring (countries-aggregated, GR6+: −3.9% over 4 rounds at 1 thread, −1.3% at 4 threads); no code on that
path changed.

Not addressed here and left open (largest shares of Kolmo's own decode time): JSON `transpose.c:decode_block`
(44–55%), `log_decode` (15–27%), `img16_inv` (41% of image `balanced`/`hc`), and kaud's LPC restore. Separately,
`-x` decodes dickens at 0.2 MB/s and x-ray at 0.4 MB/s on this VM (one CM pass per bit over scaled tables), worth a
look by the xmode owners.

## Correctness

- Same frames: the encoder is unchanged except `kh_codes`' bit reversal, which is checked against the bit loop for
  every value and length 1–16 (exhaustively, while developing); `benchmark/determinism/check.py` hashes are
  unchanged, so no `expected.sha256` entry was regenerated.
- The AVX2 word decoder is exercised by `make check-arch` (`tests/test_arch.c`: `word_decode` with `kolmo_force_c` 0
  and 1 on random vocabularies and on damaged streams, 43,645 checks); it stops in front of any token the C loop
  rejects, so errors are reported by the reference.
- `make check`, `make check-rc`, `make check-sanitizers`, `make experimental-check GPU_CHECK=`, `make kfam-check` pass.

## Files

`dbench.c` (decode benchmark), `ab.py` (A/B driver), `table.py` (tables), `bwtbench.c`, `unbwtbench.c` (inverse-BWT
sampling), `khufbench.c` (khuf fixed cost); `results/`: A/B rows (`ab_T1.jsonl`, `ab_T4.jsonl`, `ab_x.jsonl`,
re-measured outliers in `ab_re*.jsonl`), `table.md`, `callgrind_top.txt`, `bwt_index_sampling.txt`,
`natbench_klzh.txt`.
