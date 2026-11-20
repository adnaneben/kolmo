# Wave 4, CM lane: `-x` speed, memory and ratio (codec 7 model 4)

18 November 2026, branch `claude/w4-cm-20261118` (from rc3 `5071f38`). Cloud VM: 4 vCPUs of an Intel Xeon at 2.1 GHz
(AVX-512 capable; the build is `-march=native`, so the AVX2 kernels run), 15 GB, Linux 6.18, gcc 13.3. zpaq 7.15
built from source (`g++ -O3 -DNDEBUG`, its x86-64 JIT on), a test tool only. Apple M5 numbers are left to the
integration session.

## Targets and outcome (one thread, full files)

| target (owner, wave 4) | outcome |
|---|---|
| `-x` at least 5% smaller than zpaq -m5 on every group | Silesia (its 12 files, each coded alone) −7.1%, structured −55.5%, numeric −21.8%. **Text −4.7%: missed by 0.3 points** (xml −10.6%, reymont −8.6%, enwik8 −5.0%, webster −4.1%, dickens −0.7%; model 3 had −4.8% with 5–7 GB). Within Silesia the binaries alone are −1.4% (nci −15.6%, ooffice −5.0%, samba −3.8%, osdb −1.6%, mozilla +1.2%). |
| compress and decode at least as fast as zpaq -m5 at 1 thread (was 0.73× / 0.83× on text, M5) | text 1.00× / 1.02× (VM), Silesia 1.15× / 1.34×, structured 1.16× / 3.9×, numeric 1.6× / 2.3×. Two directions are 2–3% slower, within the VM's noise: enwik8 compress (0.98×), dickens decode (0.97×). |
| decode memory ≤ 2.5 GiB per block job | yes: at most about 1.9 GiB (a 128 MiB block); 1.43 GiB for enwik8 |
| peak memory not above about 1.5× zpaq -m5's | one thread: 0.48–1.46× on every file but enwik8 (nci and owid-co2 1.46×, webster 1.41×, mr 1.34×); enwik8 1.58× (1.53 GB against 0.97 GB). Four threads: the trials beside CM add 0.1–0.15 GB to compression (osdb 1.51×, x-ray 1.56×). Model 3: 5–7 GB on every file of 4 MB or more (3–30× zpaq). |

Group totals are sums over the files listed below (the M5 benchmark's groups, `benchmark/rc3/leaders`, as far as the
data could be fetched here: no OWID test tables, `api.json`, pickles or NumPy columns; silesia.tar was not measured, its
12 files were, one by one).

Reference points for enwik8 from the literature (Mahoney's Large Text Compression Benchmark, quoted from memory, not
measured here and approximate): cmix about 14.6–15 MB and paq8px at its strongest settings about 15.5–16.5 MB, both at a
few KB/s and several GB of memory; Kolmo `-x` 18.65 MB at 0.3 MB/s and 1.5 GB, zpaq -m5 19.63 MB at the same speed and
1.0 GB. Model 4 trades the last 15–20% of ratio of the strongest coders for 100× their speed.

## What changed

`-x` now writes **model 4** of codec 7 (stream model nibble 9, `SPEC.md`, *Model 4*):

1. **Fewer contexts, chosen by measurement.** A backward elimination on 15 samples of 1 MB (5 text, 3 structured, 5
   Silesia binaries, 2 numeric: 1 MiB at enwik8 +20 MiB, dickens +2, xml +1, webster +10, reymont +2, Loghub +2, the
   OWID CO2 CSV +5, OWID JSON lines +10, samba +5, mozilla +10, osdb +3, ooffice +2, nci +10, x-ray +3, sao +3; other
   slices than the priors' training set) over
   model 3's 29 hashed contexts: one pass of single drops, then a second pass on the reduced set per class. Text, logs,
   CSV and JSON keep 17 (orders 3, 4, 5, 7, 12, 20; word pair, current word, word trigram; column; two indirect; the
   expected byte with order 2; five structure contexts); the rest keeps 15 (orders 3, 4, 7, word pair, column, current
   word, word trigram, indirect, the two sparse, the three record contexts, the expected byte with orders 1 and 2). The
   mask travels in the stream. Dropping the other 12 cost 0.34% (text) and 0.26% (binary) on the samples; each context
   costs about 3.5% of the time. Single drops (cost of removing one context from all 29, %):

   | context | 3 | 12 | 10 | 4 | 6 | 25 | 14 | 15 | 8 | 13 | 9 | 18 | 16 | 20 | 17 | 29 |
   |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
   | text + structured | 0.36 | 0.69 | 0.36 | 0.24 | 0.24 | 0.20 | 0.20 | 0.14 | 0.11 | 0.11 | 0.08 | 0.03 | 0.03 | −0.05 | 0.02 | 1.63 |
   | binary + numeric | 0.32 | 0.05 | 0.04 | 0.04 | 0.05 | 0.00 | 0.09 | 0.03 | −0.01 | 0.04 | −0.03 | 1.26 | 0.49 | 0.50 | 0.21 | −0.01 |

2. **Buckets of bit-history states, four per cache line.** Model 3 kept per nibble a 64-byte bucket: a 16-bit check and
   15 slots of a 16-bit adaptive probability, a state and a count. Model 4 keeps 16 bytes (a check byte, 15 one-byte
   bit-history states), four buckets per 64-byte line with the four check bytes first (one word compare finds the
   bucket; replacement: the bucket whose first state has seen the fewest bits). On 1 MB of text, model 5 (the research
   name) with a quarter of model 3's table memory coded within 0.02% of model 3; at an eighth, 0.2% larger. One table
   shared by all contexts was worse at equal memory (+0.1% at 1.5× the memory).
3. **Tables sized by the stream, at any size**: `n / 32` lines per context up to 48 MiB, `n / 64` to 128 MiB,
   `n / 128` beyond (half for contexts 16–24, a quarter for 25–29), the line chosen by `h * lines >> 32`; the match
   table follows `n` (2^16–2^22 slots) and the match chain keeps the last 2^24 positions (64 MiB) instead of 4 bytes per
   input byte (400 MB for enwik8).
4. **A 16-bit mixer**: `pmaddwd` dot products of pairs shifted by 8, updates by `pmulhrsw` and saturating adds; C
   reference `kolmo_cm_dot16_c` / `kolmo_cm_train16_c` (the specification), SSE4.1 and AVX2 kernels through `KOLMO_K`,
   compared at every level in `tests/test_kernels.c` and end to end in `tests/test_cm4.c`. 8–12% faster than the
   32-bit exact mixer and 0.05% smaller (1 MB samples); writing the inputs as int16 directly gave another 12% (the
   int32 → int16 copy was a store-forwarding stall).
5. **Rows set on first use**: the four mixers' rows and the two 64K-row APMs are copied from a template when first
   touched (same values as setting all of them at the start). A codec-25 table part used to spend most of its time
   writing 13 MB of initial weights.
6. **Own priors and settings, no race.** The race of four presets gained model 4 0.15% (text) and 0.05% (binary) on
   the samples, for twice the encode time of a 1 MB stream at one thread; the fourth preset alone came within 0.02%.
   Model 4's priors (`cm_prior4`, text and binary, trained by `cmprior4.c` on other slices: enwik8, dickens, webster,
   xml, reymont, an OWID energy CSV, OWID JSON lines, Loghub logs; samba, mozilla, ooffice, osdb, nci, x-ray, sao, mr)
   gained 0.11% and 0.15% over model 3's prior mapped onto model 4's inputs. A coordinate search of the 8 learning
   settings on the training slices found 0.04% more; not kept.
7. **Encoder**: the planner's transform check (`ultra_keep_transform`) codes 256 KiB samples with model 4 instead of
   1 MiB with model 2 (same decisions on every file tried: 3 CSVs, 2 logs, JSON lines, xml); a transform that does not
   apply to the sample leaves the block plain (it used to be kept untested: +9% on a 1 MB log slice). 1 MB CSV at one
   thread: 23.6 → 4.7 s to compress.
8. **Every model** (no change of bytes): the match model's backward comparison 8 bytes at a time, its hash slot
   prefetched for both values of the byte's last bit, and model 4 hashes only its own contexts.

## Speed profile (perf, cpu-clock sampling; the VM has no hardware counters)

Model 3 on 1 MB of text spent its time over 30 contexts evenly: state-map reads and updates ~25%, bucket lookups and
prefetch stalls ~22%, slot updates ~16%; tables 16 times smaller made it barely faster (instruction-bound, not
memory-bound). Model 4 with 17 contexts: 1 context costs 1.18 s/MB (match model, mixers, APMs, coder), each context
~0.085 s/MB. On large inputs memory latency returns: 2.3 s/MB on 1 MB, 3.0 s/MB on dickens (10 MB). Tried and dropped:
prefetching the second nibble one bit earlier (4 candidates: slower), 4 instead of 16 match candidates (no faster,
+0.6% on xml), state maps by state only (+10% speed, +0.06% size, +1% on JSON), a match-state APM (+0.0 to +0.2%), a
mixer selected by the number of contexts seen (+0.1%), a learned final mix of the mixer and APMs (+0.1 to +1.1%), three
buckets per line with 16-bit checks (+0.14% on 10 MB of enwik8, +0.44% on 1 MB at enwik8's load).

## Results, full files

Each tool in its own process, one direction at a time, wall time and peak RSS (`wait4`), decoded output compared by
SHA-256. Two measurement processes ran side by side on the 4 vCPUs (each single-threaded), and the VM's timing noise is
about ±10%: ratios within 0.95–1.05 are a tie.

The rows are in `rows/t1.jsonl` and `rows/t4.jsonl` (`python3 summarize.py rows/t1.jsonl rows/t4.jsonl` prints these
tables). Nine files (xml, reymont, dickens, loghub, countries, ooffice, osdb, x-ray, sao) were timed with a build that
differed from the final one only in the priors' initial weights (model 3's prior mapped onto model 4, before
`cm_prior4`): their sizes here are the final build's (0.0–0.14% smaller; the frames do not depend on the thread count,
`benchmark/determinism`), their times and memory those runs' (the priors change neither). The four-thread rows are a
subset, measured while the checks ran on the other cores: their times are noisier still. zpaq -t4 splits only files of
more than one 64 MB block, so on these files both tools work on one thread but for Kolmo's trials beside CM.

### 1 thread

| group | file | MB | kolmo -x | zpaq -m5 | size | compress s (x) | decompress s (x) | peak RSS MiB, c / d | RSS x |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| text | enwik8 | 100.0 | 18,651,472 | 19,625,010 | -5.0% | 325.7 / 319.6 (0.98x) | 327.6 / 334.9 (1.02x) | 1526 / 1429 vs 967 / 923 | 1.58 |
| text | dickens | 10.2 | 2,079,263 | 2,094,742 | -0.7% | 30.7 / 30.9 (1.01x) | 30.3 / 29.2 (0.97x) | 344 / 330 vs 304 / 302 | 1.13 |
| text | webster | 41.5 | 5,433,072 | 5,666,830 | -4.1% | 119.6 / 124.2 (1.04x) | 121.0 / 124.6 (1.03x) | 1188 / 1146 vs 840 / 835 | 1.41 |
| text | reymont | 6.6 | 874,420 | 956,498 | -8.6% | 16.5 / 17.8 (1.08x) | 15.9 / 17.8 (1.12x) | 238 / 230 vs 212 / 211 | 1.12 |
| text | xml | 5.3 | 292,425 | 326,945 | -10.6% | 11.9 / 12.7 (1.07x) | 11.1 / 12.1 (1.09x) | 198 / 190 vs 198 / 199 | 1.00 |
| **text** | **5 files** | | **27,330,652** | **28,670,025** | **-4.7%** | **1.00x** | **1.02x** | | |
| Silesia (12 files, each coded alone) | dickens | 10.2 | 2,079,263 | 2,094,742 | -0.7% | 30.7 / 30.9 (1.01x) | 30.3 / 29.2 (0.97x) | 344 / 330 vs 304 / 302 | 1.13 |
| Silesia (12 files, each coded alone) | mozilla | 51.2 | 12,182,983 | 12,041,054 | +1.2% | 136.3 / 168.5 (1.24x) | 124.2 / 179.9 (1.45x) | 403 / 314 vs 833 / 822 | 0.48 |
| Silesia (12 files, each coded alone) | mr | 10.0 | 1,941,087 | 2,181,309 | -11.0% | 21.2 / 22.4 (1.06x) | 15.6 / 24.9 (1.60x) | 335 / 304 vs 250 / 248 | 1.34 |
| Silesia (12 files, each coded alone) | nci | 33.6 | 1,056,026 | 1,251,107 | -15.6% | 77.3 / 77.8 (1.01x) | 62.2 / 85.4 (1.37x) | 979 / 942 vs 670 / 666 | 1.46 |
| Silesia (12 files, each coded alone) | ooffice | 6.2 | 1,678,511 | 1,766,549 | -5.0% | 18.7 / 19.1 (1.02x) | 16.5 / 20.2 (1.23x) | 192 / 179 vs 185 / 184 | 1.04 |
| Silesia (12 files, each coded alone) | osdb | 10.1 | 2,169,690 | 2,204,740 | -1.6% | 30.4 / 35.8 (1.18x) | 29.9 / 34.2 (1.14x) | 318 / 308 vs 281 / 279 | 1.13 |
| Silesia (12 files, each coded alone) | reymont | 6.6 | 874,420 | 956,498 | -8.6% | 16.5 / 17.8 (1.08x) | 15.9 / 17.8 (1.12x) | 238 / 230 vs 212 / 211 | 1.12 |
| Silesia (12 files, each coded alone) | samba | 21.6 | 2,939,009 | 3,053,819 | -3.8% | 73.3 / 79.3 (1.08x) | 49.3 / 77.1 (1.56x) | 619 / 598 vs 537 / 535 | 1.15 |
| Silesia (12 files, each coded alone) | sao | 7.3 | 2,121,585 | 3,899,257 | -45.6% | 10.4 / 34.3 (3.30x) | 7.3 / 34.6 (4.73x) | 148 / 130 vs 201 / 198 | 0.74 |
| Silesia (12 files, each coded alone) | webster | 41.5 | 5,433,072 | 5,666,830 | -4.1% | 119.6 / 124.2 (1.04x) | 121.0 / 124.6 (1.03x) | 1188 / 1146 vs 840 / 835 | 1.41 |
| Silesia (12 files, each coded alone) | x-ray | 8.5 | 3,562,410 | 3,669,700 | -2.9% | 19.3 / 26.2 (1.36x) | 14.5 / 28.0 (1.93x) | 292 / 266 vs 249 / 246 | 1.17 |
| Silesia (12 files, each coded alone) | xml | 5.3 | 292,425 | 326,945 | -10.6% | 11.9 / 12.7 (1.07x) | 11.1 / 12.1 (1.09x) | 198 / 190 vs 198 / 199 | 1.00 |
| **Silesia (12 files, each coded alone)** | **12 files** | | **36,330,481** | **39,112,550** | **-7.1%** | **1.15x** | **1.34x** | | |
| Silesia binaries | mozilla | 51.2 | 12,182,983 | 12,041,054 | +1.2% | 136.3 / 168.5 (1.24x) | 124.2 / 179.9 (1.45x) | 403 / 314 vs 833 / 822 | 0.48 |
| Silesia binaries | samba | 21.6 | 2,939,009 | 3,053,819 | -3.8% | 73.3 / 79.3 (1.08x) | 49.3 / 77.1 (1.56x) | 619 / 598 vs 537 / 535 | 1.15 |
| Silesia binaries | nci | 33.6 | 1,056,026 | 1,251,107 | -15.6% | 77.3 / 77.8 (1.01x) | 62.2 / 85.4 (1.37x) | 979 / 942 vs 670 / 666 | 1.46 |
| Silesia binaries | ooffice | 6.2 | 1,678,511 | 1,766,549 | -5.0% | 18.7 / 19.1 (1.02x) | 16.5 / 20.2 (1.23x) | 192 / 179 vs 185 / 184 | 1.04 |
| Silesia binaries | osdb | 10.1 | 2,169,690 | 2,204,740 | -1.6% | 30.4 / 35.8 (1.18x) | 29.9 / 34.2 (1.14x) | 318 / 308 vs 281 / 279 | 1.13 |
| **Silesia binaries** | **5 files** | | **20,026,219** | **20,317,269** | **-1.4%** | **1.13x** | **1.41x** | | |
| structured | loghub_all.log | 4.5 | 184,200 | 214,064 | -14.0% | 9.7 / 11.0 (1.13x) | 9.5 / 10.6 (1.11x) | 168 / 159 vs 197 / 197 | 0.85 |
| structured | countries-aggregated.csv | 5.5 | 283,038 | 524,015 | -46.0% | 13.1 / 13.6 (1.04x) | 1.7 / 16.9 (9.79x) | 229 / 171 vs 204 / 204 | 1.12 |
| structured | owid-co2-data.csv | 15.2 | 1,127,837 | 2,844,316 | -60.3% | 34.7 / 42.1 (1.21x) | 7.0 / 43.2 (6.19x) | 419 / 247 vs 287 / 285 | 1.46 |
| **structured** | **3 files** | | **1,595,075** | **3,582,395** | **-55.5%** | **1.16x** | **3.88x** | | |
| numeric | x-ray | 8.5 | 3,562,410 | 3,669,700 | -2.9% | 19.3 / 26.2 (1.36x) | 14.5 / 28.0 (1.93x) | 292 / 266 vs 249 / 246 | 1.17 |
| numeric | mr | 10.0 | 1,941,087 | 2,181,309 | -11.0% | 21.2 / 22.4 (1.06x) | 15.6 / 24.9 (1.60x) | 335 / 304 vs 250 / 248 | 1.34 |
| numeric | sao | 7.3 | 2,121,585 | 3,899,257 | -45.6% | 10.4 / 34.3 (3.30x) | 7.3 / 34.6 (4.73x) | 148 / 130 vs 201 / 198 | 0.74 |
| **numeric** | **3 files** | | **7,625,082** | **9,750,266** | **-21.8%** | **1.63x** | **2.34x** | | |

### 4 threads

| group | file | MB | kolmo -x | zpaq -m5 | size | compress s (x) | decompress s (x) | peak RSS MiB, c / d | RSS x |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| text | dickens | 10.2 | 2,079,263 | 2,094,742 | -0.7% | 30.8 / 32.3 (1.05x) | 31.9 / 32.1 (1.01x) | 370 / 330 vs 304 / 302 | 1.22 |
| text | xml | 5.3 | 292,425 | 326,945 | -10.6% | 12.2 / 13.3 (1.09x) | 12.6 / 13.0 (1.03x) | 216 / 189 vs 199 / 199 | 1.09 |
| **text** | **2 files** | | **2,371,688** | **2,421,687** | **-2.1%** | **1.06x** | **1.01x** | | |
| Silesia (12 files, each coded alone) | dickens | 10.2 | 2,079,263 | 2,094,742 | -0.7% | 30.8 / 32.3 (1.05x) | 31.9 / 32.1 (1.01x) | 370 / 330 vs 304 / 302 | 1.22 |
| Silesia (12 files, each coded alone) | osdb | 10.1 | 2,169,690 | 2,204,740 | -1.6% | 27.4 / 34.5 (1.26x) | 27.9 / 35.2 (1.26x) | 425 / 308 vs 281 / 280 | 1.51 |
| Silesia (12 files, each coded alone) | x-ray | 8.5 | 3,562,410 | 3,669,700 | -2.9% | 16.7 / 26.5 (1.59x) | 15.6 / 28.6 (1.83x) | 390 / 266 vs 249 / 246 | 1.56 |
| Silesia (12 files, each coded alone) | xml | 5.3 | 292,425 | 326,945 | -10.6% | 12.2 / 13.3 (1.09x) | 12.6 / 13.0 (1.03x) | 216 / 189 vs 199 / 199 | 1.09 |
| **Silesia (12 files, each coded alone)** | **4 files** | | **8,103,788** | **8,296,127** | **-2.3%** | **1.22x** | **1.24x** | | |
| Silesia binaries | osdb | 10.1 | 2,169,690 | 2,204,740 | -1.6% | 27.4 / 34.5 (1.26x) | 27.9 / 35.2 (1.26x) | 425 / 308 vs 281 / 280 | 1.51 |
| **Silesia binaries** | **1 files** | | **2,169,690** | **2,204,740** | **-1.6%** | **1.26x** | **1.26x** | | |
| structured | loghub_all.log | 4.5 | 184,200 | 214,064 | -14.0% | 10.3 / 11.3 (1.10x) | 9.4 / 10.8 (1.15x) | 173 / 159 vs 197 / 197 | 0.88 |
| **structured** | **1 files** | | **184,200** | **214,064** | **-14.0%** | **1.10x** | **1.15x** | | |
| numeric | x-ray | 8.5 | 3,562,410 | 3,669,700 | -2.9% | 16.7 / 26.5 (1.59x) | 15.6 / 28.6 (1.83x) | 390 / 266 vs 249 / 246 | 1.56 |
| **numeric** | **1 files** | | **3,562,410** | **3,669,700** | **-2.9%** | **1.59x** | **1.83x** | | |

speed ratios: zpaq time / Kolmo time (above 1: Kolmo faster); RSS x: Kolmo peak / zpaq peak (largest of the two directions)

## Bytes, determinism, checks

- Models 1–3 decode and encode the same bytes as before (determinism frames of every mode but `-x` unchanged; the
  compact-table build `make check-cm-compact` agrees with the reference).
- `-x` frames change: listed in the commit that regenerates `benchmark/determinism/expected.sha256`.
- Model-4 streams: the same bytes with the C, SSE4.1 and AVX2 kernels (`tests/test_cm4.c`), at 1 and 4 threads and
  under every memory budget (`benchmark/determinism/check.py`).
- aarch64 was not checked on this VM: model 4 uses the portable C mixer there (no NEON kernel was written without a way
  to test it); the M5 measurement at merge will tell whether a NEON `cm_dot16` pays.

## Open

- Text −4.6% against −5%: the gap is dickens (−0.7%) and webster. More table memory closes it on enwik8 (−5.5% with
  twice the tables, 2.9 GB) but breaks the memory target; a better text model (paq-style word and sparse contexts, an SSE
  stage that wins) is the next step.
- mozilla +1.2%: the binary context set is weaker than model 2's on executables.
- At all threads a single text stream still runs on one thread (zpaq -t4 splits files above 64 MB into blocks).
