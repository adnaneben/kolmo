# w4 documents lane: text, CSV, JSON, XML, logs and source at speed, balanced, eco and hc

18 November 2026, branch `claude/w4-docs-20261118` (from rc3 `5071f38`). Cloud VM: Intel Xeon @ 2.10 GHz, 4 vCPU,
15 GB, Linux; build `-O3 -march=native` (the Makefile default). Apple M5 speeds are measured by the integration session.

## Summary against the targets

One thread unless stated. "Base" is rc3 at `5071f38` measured in the same session on the same files.

| target | result | met? |
|---|---|---|
| balanced smaller than zstd -3 on every document group, text included | text −0.3% (was +0.0%), xml −0.4% (was +2.1%), source −3.7% (was +1.3%), csv −55.8%, json −2.7% (was −0.0%), logs −30.2%; every text file is smaller (was: dickens +1.2%, reymont +1.0%, xml +2.1%, source.tar +10.6%). At 4 threads text ties zstd -3 -T4 (+0.0%): zstd's multithreaded mode is 0.3% smaller than its single-thread mode | **yes at 1 thread**; text is a tie at 4 threads |
| balanced ≥ 0.8× zstd -3's compress speed on every group | text 1.17×, xml 0.95×, source 0.84×, json 0.87×; **csv 0.71×, logs 0.56×** (unchanged from base: table formula search and log templates) | no (csv, logs) |
| balanced ≥ 1.0× zstd -3's decode on every group | text 0.95× (base 0.85×), xml 0.83×, source 0.85×, csv 0.62×, json 0.71×, logs 0.67× | no |
| eco smaller than brotli -11 on text and Silesia text files, ≥ 10× its compress speed | text −14.6% (dickens −20.3%, webster −19.7%, reymont −26.6%, enwik8 −11.8%), xml −10.5%, json −1.1%, at 24–32× brotli -11's compress speed; all document files together −15.8% (base +5.6%) | **yes**, but eco now decodes text at 15–20 MB/s (0.1× brotli) |
| speed ≥ 10% smaller than lz4 -1 on documents at ≥ 1.0× its speed | ±0.5% on every group (unchanged); the measured options (below) do not reach 10% at lz4's speed | no |
| hc ≥ 5% smaller than zstd -19 --long=27 on documents | all document files together −10.0% (csv −52.6%, logs −19.9%); per group text −3.7%, xml −2.2%, json −3.5%, source +1.8% (was +6.5%) | over all documents yes; text, xml, json, source no |
| peak memory ≤ 1.5× the rival's | eco compress ≤ 1.27× (decode up to 6.3×: block sorting); hc text 2.05× (was 5.02×); balanced 1.8–4.7× (zstd -3 streams in 6 MB; base was already 1.7–4.7×) | no for balanced and hc |

## What changed (no new container fields; every frame is rc3, container version 11)

1. **balanced, plain blocks** (`core/compress.c`, `codecs/zstd.c`): a segment without a transform is cut into blocks of up to
   6 MiB, split evenly (was n/8 blocks of 1–4 MiB), and a block over 4 MiB is coded by zstd 3 with an 8 MiB window
   (level 3's is 2 MiB). Gains are small but on every text file, at the same compress speed.
2. **Segments with the same plain or word plan are merged** (`core/compress.c`), so tar members and content runs share
   one LZ window: source.tar balanced −12.5% (2.27 → 1.99 MB) and hc −10.5%; samba balanced −1.4%, hc −2.0%.
3. **eco: block sorting for text** (`core/blocks.c`, `plan/detect.c`): text blocks ≥ 64 KiB (at most 5% control bytes
   besides NUL/tab/newline/CR and at most 25% NUL, so UTF-8 and tar members qualify) and JSON token streams (as
   `ratio` already did) go to codec 18 instead of brotli 8; text files up to 32 MiB are one block (gharchive NDJSON
   with 16 MiB blocks was +4.4% against brotli -11, as one block −0.9%). Larger files keep 16 MiB blocks: 32 MiB blocks
   on enwik8 were 1.8× brotli -11's memory.
4. **hc text: zstd level 19 with a 128 MiB window** (as `zstd -19 --long=27`), was level 22; plain text blocks also get
   the 128 MiB block. Level 22 used 942 MB on enwik8 (5× zstd -19 --long); now 384 MB, at +3.3% size (24.39 → 25.18 MB,
   −4.9% against zstd -19 --long). Level 20 with chain log 25 keeps 24.75 MB at about 80 MB more.
5. **Asynchronous file writes at one thread** (`core/decode.c`): a single decode worker hands each decoded block to a
   writer thread and decodes the next block into a second buffer (zstd's command line also writes on a second
   thread). enwik8 balanced decode 0.40 → 0.22 s; +1 block of decode memory. With several workers nothing changes.
6. **Table encoder prefetch** (`transforms/table.c`): a column's fields are a row apart; prefetching 16 fields ahead
   cut the column parse's cache misses (CSV compress −8% to −12% in `perf stat`, same bytes).

## Files

Downloaded (not committed): enwik8; Silesia dickens, webster, reymont, xml, samba; `source.tar` (lua 5.4.7,
react-dom 18.3.1 from npm, and Kolmo's vendored zstd and brotli sources, 10.3 MB); OWID `owid-co2-data.csv` (15.2 MB)
and `owid-energy-data.csv` (9.2 MB, 130 columns: the wide CSV); the first 20 MB of the OWID covid compact table;
simdjson's `citm_catalog.json` and `twitter.json`; GH Archive 2015-01-01-15 (`gharchive.ndjson`, 26.1 MB) and its first
3000 events as a pretty-printed JSON array (`github_events.json`, 8.6 MB; api.github.com refused unauthenticated
requests); the 16 Loghub 2k samples concatenated (`loghub_all.log`, 4.5 MB).

Groups: text = enwik8, dickens, webster, reymont; xml; source = samba, source.tar; csv; json = citm, twitter,
github_events, gharchive; logs.

## Method

`run.py` runs each tool as a whole process: wall time, fastest of up to 3 rounds (11 for the balanced table below)
within 20 s per file and tool; every decoded file is compared with the input by SHA-256. Peak RSS from GNU time (a
child forked from Python starts from Python's ~10 MB, which Linux keeps in `ru_maxrss` across exec; the first baseline
run had that floor and was discarded). Rivals: lz4 1.9.4 -1, zstd 1.5.5 -3 and -19 --long=27 (`--single-thread` at one
thread, `-T4` at four), brotli 1.1.0 -q 11. **The VM is noisy:** repeated runs of the same binary differ by up to ±10%
(decode of the same balanced JSON frames read 0.67× and 0.82× zstd in two runs). CPU-time minima over 15 runs
(`perf`/`wait4`) were used to check decode findings: e.g. dickens balanced decode 22.7 ms with 6 MiB blocks against
zstd -3's 22.5 ms and base's 19.3 ms.

## One thread: speed, eco and hc (3 rounds); "vs base" is Kolmo's size change against base

### 1 thread

| group | Kolmo | rival | size | compress | decompress | peak RSS c / d (worst file) | vs base |
|---|---|---|---:|---:|---:|---:|---:|
| text | speed | lz4 -1 | +0.0% | 1.14x | 1.43x | 1.76x / 1.75x | +0.0% |
| text | eco | brotli -11 | -14.6% | 32.35x | 0.10x | 1.27x / 6.25x | -23.9% |
| text | hc | zstd -19 --long=27 | -3.7% | 1.41x | 0.77x | 2.05x / 1.92x | +1.5% |
| xml | speed | lz4 -1 | +0.4% | 1.16x | 1.21x | 1.56x / 1.45x | +0.0% |
| xml | eco | brotli -11 | -10.5% | 23.85x | 0.11x | 0.83x / 2.85x | -17.7% |
| xml | hc | zstd -19 --long=27 | -2.2% | 1.52x | 0.59x | 0.96x / 1.53x | +0.3% |
| source | speed | lz4 -1 | +0.1% | 1.31x | 1.21x | 2.33x / 1.25x | +0.0% |
| source | eco | brotli -11 | +3.9% | 27.36x | 0.08x | 0.60x / 3.66x | -12.0% |
| source | hc | zstd -19 --long=27 | +1.8% | 1.40x | 1.10x | 1.39x / 1.05x | -4.4% |
| csv | speed | lz4 -1 | +0.0% | 1.27x | 1.73x | 0.98x / 1.34x | +0.0% |
| csv | eco | brotli -11 | -42.6% | 59.84x | 0.69x | 0.67x / 2.99x | +0.0% |
| csv | hc | zstd -19 --long=27 | -52.6% | 5.91x | 0.41x | 1.51x / 2.34x | +0.0% |
| json | speed | lz4 -1 | +0.1% | 1.23x | 1.25x | 1.87x / 1.60x | +0.0% |
| json | eco | brotli -11 | -1.1% | 31.87x | 0.13x | 0.94x / 5.02x | -14.5% |
| json | hc | zstd -19 --long=27 | -3.5% | 1.52x | 0.51x | 1.40x / 1.68x | +0.2% |
| logs | speed | lz4 -1 | +0.5% | 1.09x | 1.26x | 1.35x / 1.29x | +0.0% |
| logs | eco | brotli -11 | -10.4% | 86.17x | 0.84x | 0.63x / 1.66x | +0.0% |
| logs | hc | zstd -19 --long=27 | -19.9% | 6.29x | 0.81x | 0.57x / 1.62x | +0.0% |

## One thread: balanced (11 rounds)

### 1 thread

| group | Kolmo | rival | size | compress | decompress | peak RSS c / d (worst file) | vs base |
|---|---|---|---:|---:|---:|---:|---:|
| text | balanced | zstd -3 | -0.3% | 1.17x | 0.95x | 2.28x / 2.65x | -0.3% |
| xml | balanced | zstd -3 | -0.4% | 0.95x | 0.83x | 1.76x / 1.56x | -2.5% |
| source | balanced | zstd -3 | -3.7% | 0.84x | 0.85x | 4.65x / 2.75x | -4.9% |
| csv | balanced | zstd -3 | -55.8% | 0.71x | 0.62x | 4.12x / 2.15x | +0.0% |
| json | balanced | zstd -3 | -2.7% | 0.87x | 0.71x | 2.28x / 2.39x | -2.6% |
| logs | balanced | zstd -3 | -30.2% | 0.56x | 0.67x | 2.32x / 1.78x | +0.0% |

## Four threads (balanced and hc; lz4 and brotli have no threads)

### 4 threads

| group | Kolmo | rival | size | compress | decompress | peak RSS c / d (worst file) | vs base |
|---|---|---|---:|---:|---:|---:|---:|
| text | balanced | zstd -3 | +0.0% | 1.50x | 1.71x | 1.37x / 5.70x | -0.3% |
| text | hc | zstd -19 --long=27 | -3.7% | 1.04x | 0.59x | 1.34x / 1.93x | +1.5% |
| xml | balanced | zstd -3 | -0.5% | 1.15x | 0.90x | 1.10x / 1.58x | -2.5% |
| xml | hc | zstd -19 --long=27 | -2.2% | 1.56x | 0.66x | 0.96x / 1.54x | +0.3% |
| source | balanced | zstd -3 | -3.7% | 1.18x | 1.46x | 1.31x / 4.20x | -4.9% |
| source | hc | zstd -19 --long=27 | +1.8% | 1.93x | 0.87x | 2.65x / 1.30x | -4.4% |
| csv | balanced | zstd -3 | -55.7% | 1.33x | 1.26x | 3.38x / 4.92x | +0.0% |
| csv | hc | zstd -19 --long=27 | -52.6% | 6.72x | 0.43x | 1.77x / 2.62x | +0.0% |
| json | balanced | zstd -3 | -3.4% | 0.93x | 1.11x | 2.43x / 3.91x | -2.6% |
| json | hc | zstd -19 --long=27 | -3.5% | 1.64x | 0.55x | 1.37x / 1.68x | +0.2% |
| logs | balanced | zstd -3 | -30.1% | 0.95x | 0.94x | 2.40x / 2.09x | +0.0% |
| logs | hc | zstd -19 --long=27 | -19.9% | 5.40x | 0.75x | 0.57x / 1.65x | +0.0% |

balanced decode memory at 4 threads: text 5.7× zstd -3's (base 4.2×): four 6 MiB blocks in flight instead of four 1–4 MiB ones.

## Per file, one thread

| enwik8 | balanced | 35,518,150 | 149.9 / 615 | 13 | 35,584,688 | 122.8 / 640 | 6 | -0.2% | 1.22x | 0.96x |
| dickens | balanced | 3,668,484 | 117.4 / 397 | 12 | 3,677,178 | 116.2 / 446 | 6 | -0.2% | 1.01x | 0.89x |
| webster | balanced | 12,115,296 | 154.5 / 659 | 12 | 12,173,030 | 137.3 / 648 | 6 | -0.5% | 1.13x | 1.02x |
| reymont | balanced | 1,943,476 | 127.0 / 331 | 14 | 1,948,616 | 134.0 / 422 | 6 | -0.3% | 0.95x | 0.78x |
| xml | balanced | 636,202 | 233.4 / 420 | 11 | 638,978 | 245.8 / 503 | 6 | -0.4% | 0.95x | 0.83x |
| samba | balanced | 4,800,466 | 158.7 / 671 | 31 | 4,997,085 | 197.0 / 735 | 6 | -3.9% | 0.81x | 0.91x |
| source.tar | balanced | 1,987,667 | 180.5 / 434 | 11 | 2,052,465 | 193.7 / 574 | 6 | -3.2% | 0.93x | 0.76x |
| owid-co2-data.csv | balanced | 1,392,694 | 109.2 / 364 | 26 | 4,644,521 | 135.9 / 515 | 6 | -70.0% | 0.80x | 0.71x |
| owid-energy-data.csv | balanced | 946,078 | 67.5 / 340 | 22 | 2,746,499 | 131.6 / 452 | 6 | -65.6% | 0.51x | 0.75x |
| covid20.csv | balanced | 2,001,119 | 263.4 / 416 | 12 | 2,436,081 | 293.7 / 898 | 6 | -17.9% | 0.90x | 0.46x |
| citm_catalog.json | balanced | 11,544 | 114.2 / 263 | 11 | 15,645 | 274.4 / 302 | 5 | -26.2% | 0.42x | 0.87x |
| twitter.json | balanced | 48,394 | 69.9 / 122 | 5 | 48,458 | 110.2 / 129 | 4 | -0.1% | 0.63x | 0.95x |
| github_events.json | balanced | 787,704 | 169.8 / 481 | 13 | 886,119 | 288.7 / 648 | 6 | -11.1% | 0.59x | 0.74x |
| gharchive.ndjson | balanced | 2,982,019 | 393.9 / 629 | 10 | 2,984,597 | 323.8 / 981 | 6 | -0.1% | 1.22x | 0.64x |
| loghub_all.log | balanced | 316,254 | 138.5 / 331 | 14 | 453,010 | 247.4 / 491 | 6 | -30.2% | 0.56x | 0.67x |
| file | Kolmo | bytes | MB/s c / d | RSS c MB | rival bytes | MB/s c / d | RSS c MB | size | c | d |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| enwik8 | speed | 57,286,385 | 319.4 / 1137 | 9 | 57,285,984 | 292.7 / 767 | 7 | +0.0% | 1.09x | 1.48x |
| enwik8 | eco | 22,713,842 | 11.1 / 17 | 257 | 25,742,001 | 0.3 / 177 | 203 | -11.8% | 32.00x | 0.10x |
| enwik8 | hc | 25,184,188 | 1.5 / 241 | 384 | 26,483,993 | 0.9 / 337 | 187 | -4.9% | 1.60x | 0.72x |
| dickens | speed | 6,437,034 | 215.3 / 785 | 5 | 6,431,133 | 187.9 / 436 | 8 | +0.1% | 1.15x | 1.80x |
| dickens | eco | 2,254,558 | 9.9 / 16 | 103 | 2,827,777 | 0.4 / 207 | 126 | -20.3% | 25.52x | 0.08x |
| dickens | hc | 2,728,336 | 1.9 / 209 | 130 | 2,849,702 | 1.0 / 316 | 95 | -4.3% | 1.89x | 0.66x |
| webster | speed | 20,149,426 | 335.0 / 852 | 9 | 20,149,249 | 259.4 / 661 | 7 | +0.0% | 1.29x | 1.29x |
| webster | eco | 6,770,236 | 15.5 / 22 | 152 | 8,428,575 | 0.4 / 217 | 198 | -19.7% | 37.09x | 0.10x |
| webster | hc | 8,606,033 | 1.1 / 368 | 141 | 8,624,384 | 1.1 / 370 | 127 | -0.2% | 1.05x | 1.00x |
| reymont | speed | 3,189,095 | 206.0 / 495 | 13 | 3,182,851 | 187.9 / 401 | 7 | +0.2% | 1.10x | 1.23x |
| reymont | eco | 978,208 | 13.4 / 21 | 49 | 1,332,158 | 0.5 / 273 | 94 | -26.6% | 27.38x | 0.08x |
| reymont | hc | 1,348,510 | 1.2 / 354 | 94 | 1,347,634 | 1.2 / 355 | 90 | +0.1% | 1.02x | 1.00x |
| xml | speed | 1,232,806 | 310.8 / 521 | 9 | 1,228,106 | 267.0 / 431 | 6 | +0.4% | 1.16x | 1.21x |
| xml | eco | 385,428 | 16.9 / 38 | 56 | 430,566 | 0.7 / 340 | 68 | -10.5% | 23.85x | 0.11x |
| xml | hc | 444,775 | 3.9 / 263 | 86 | 454,693 | 2.6 / 447 | 89 | -2.2% | 1.52x | 0.59x |
| samba | speed | 7,728,497 | 403.4 / 628 | 18 | 7,721,752 | 307.1 / 666 | 7 | +0.1% | 1.31x | 0.94x |
| samba | eco | 3,931,774 | 14.7 / 20 | 96 | 3,766,340 | 0.5 / 239 | 194 | +4.4% | 27.87x | 0.08x |
| samba | hc | 3,990,707 | 3.1 / 534 | 148 | 3,888,797 | 2.0 / 360 | 106 | +2.6% | 1.55x | 1.48x |
| source.tar | speed | 4,005,215 | 326.6 / 881 | 7 | 3,999,278 | 253.0 / 443 | 7 | +0.1% | 1.29x | 1.99x |
| source.tar | eco | 1,365,334 | 15.1 / 24 | 69 | 1,330,448 | 0.6 / 326 | 115 | +2.6% | 26.24x | 0.07x |
| source.tar | hc | 1,420,418 | 2.5 / 305 | 100 | 1,425,264 | 2.2 / 476 | 95 | -0.3% | 1.14x | 0.64x |
| owid-co2-data.csv | speed | 7,554,712 | 321.7 / 947 | 6 | 7,552,450 | 245.1 / 597 | 7 | +0.0% | 1.31x | 1.59x |
| owid-co2-data.csv | eco | 1,309,130 | 29.8 / 197 | 102 | 3,154,593 | 0.4 / 186 | 188 | -58.5% | 80.84x | 1.06x |
| owid-co2-data.csv | hc | 1,291,252 | 11.4 / 239 | 150 | 3,602,386 | 1.0 / 424 | 99 | -64.2% | 11.06x | 0.56x |
| owid-energy-data.csv | speed | 4,585,681 | 312.5 / 812 | 5 | 4,582,446 | 259.6 / 430 | 7 | +0.1% | 1.20x | 1.89x |
| owid-energy-data.csv | eco | 875,539 | 19.8 / 187 | 76 | 2,070,389 | 0.4 / 136 | 113 | -57.7% | 49.52x | 1.38x |
| owid-energy-data.csv | hc | 864,398 | 8.4 / 231 | 135 | 2,191,010 | 1.1 / 318 | 93 | -60.5% | 7.46x | 0.73x |
| covid20.csv | speed | 4,006,283 | 660.1 / 1479 | 6 | 4,004,574 | 524.8 / 834 | 6 | +0.0% | 1.26x | 1.77x |
| covid20.csv | eco | 1,842,656 | 18.9 / 115 | 98 | 1,785,621 | 0.3 / 353 | 188 | +3.2% | 54.22x | 0.32x |
| covid20.csv | hc | 1,503,353 | 5.4 / 151 | 136 | 1,924,452 | 1.5 / 649 | 105 | -21.9% | 3.58x | 0.23x |
| citm_catalog.json | speed | 44,127 | 292.3 / 304 | 5 | 43,189 | 267.4 / 278 | 3 | +2.2% | 1.09x | 1.09x |
| citm_catalog.json | eco | 7,998 | 68.6 / 279 | 26 | 9,090 | 3.3 / 289 | 28 | -12.0% | 20.80x | 0.96x |
| citm_catalog.json | hc | 7,734 | 30.6 / 259 | 15 | 9,784 | 11.0 / 269 | 37 | -21.0% | 2.78x | 0.96x |
| twitter.json | speed | 80,095 | 112.2 / 128 | 4 | 80,062 | 126.3 / 122 | 2 | +0.0% | 0.89x | 1.05x |
| twitter.json | eco | 31,698 | 32.7 / 46 | 8 | 33,339 | 1.4 / 102 | 19 | -4.9% | 24.09x | 0.45x |
| twitter.json | hc | 37,755 | 4.8 / 126 | 21 | 37,685 | 4.2 / 118 | 20 | +0.2% | 1.14x | 1.06x |
| github_events.json | speed | 1,800,517 | 474.5 / 877 | 5 | 1,794,799 | 369.1 / 557 | 6 | +0.3% | 1.29x | 1.57x |
| github_events.json | eco | 558,000 | 32.1 / 45 | 56 | 564,027 | 0.7 / 370 | 99 | -1.1% | 47.54x | 0.12x |
| github_events.json | hc | 566,134 | 4.6 / 229 | 116 | 633,024 | 2.5 / 547 | 93 | -10.6% | 1.88x | 0.42x |
| gharchive.ndjson | speed | 5,781,072 | 564.8 / 963 | 7 | 5,780,457 | 445.0 / 804 | 6 | +0.0% | 1.27x | 1.20x |
| gharchive.ndjson | eco | 1,905,860 | 19.1 / 39 | 161 | 1,923,854 | 0.7 / 339 | 179 | -0.9% | 29.13x | 0.11x |
| gharchive.ndjson | hc | 2,072,206 | 3.5 / 286 | 154 | 2,101,022 | 2.5 / 596 | 110 | -1.4% | 1.43x | 0.48x |
| loghub_all.log | speed | 768,456 | 344.2 / 495 | 8 | 764,678 | 316.4 / 393 | 6 | +0.5% | 1.09x | 1.26x |
| loghub_all.log | eco | 275,583 | 45.1 / 241 | 38 | 307,613 | 0.5 / 286 | 60 | -10.4% | 86.17x | 0.84x |
| loghub_all.log | hc | 266,825 | 11.9 / 307 | 51 | 333,164 | 1.9 / 378 | 88 | -19.9% | 6.29x | 0.81x |

## Measured and not adopted

- **32 MiB balanced text blocks coded by zstd's own jobs** (`nbWorkers` ≥ 1 gives the same bytes for any worker
  count, checked at 1, 2 and 4): −0.2% to −4.1% against zstd -3, but 65 MB peak memory on enwik8 and decode 0.89×
  (a 16 MiB window misses the cache). 6 MiB blocks keep most of the size gain.
- **Minimum match 6 at level 3:** prose −0.6%, but enwik8 +1.0% and source +0.5%. **Level 4–5 or larger tables at level 3:**
  −2% to −4% at 0.6–0.7× zstd -3's speed.
- **Word transform at balanced:** dickens −7.5%, webster −2.6%, xml −5.9% against zstd -3 (reymont +1.5%), but the
  transform encodes at 50–100 MB/s and decodes at 500–1400 MB/s: about 0.3× zstd's compress speed and below its decode.
- **eco with brotli at higher quality or LZMA on word-coded text:** dickens q9 2.87 MB, q10 2.74 MB (8.7 s), xz -6
  2.68 MB (5.8 s) against brotli -11's 2.83 MB: none both smaller and ≥ 10× faster. Block sorting: 2.25 MB in 1.1 s.
- **speed: a larger LZ4 hash table** (`LZ4_MEMORY_USAGE` 16 or 18, same format and decoder): dickens −12% at 0.7× lz4's
  speed, OWID CSV −7% at 0.73×, xml −5%, JSON −4%, logs −2% at 1.0×. KLZ (`KOLMO_NATIVE=lz`, rc3 wave 2) is −6.9% on
  Silesia at 0.79× compress and 0.77× decode. Neither reaches −10% at lz4's speed.
- **JSON key-path columns** (a Python prototype: each key path a value stream, key structure a stream, each coded by
  zstd -3): gharchive −4.1%, pretty GitHub events −12.5% (Kolmo's JSON tokens already −11%), twitter −11.6% against
  whole-file zstd -3. Not built: the gain over the present transforms is small.

## Open

- balanced CSV and logs compress at 0.56–0.71× zstd -3: on the 130-column OWID energy CSV the formula search is 36% of
  the time (cubic in columns), and the column parse and the table encoder most of the rest.
- balanced decode of tables and logs (0.6–0.7× zstd -3): the table decoder (`tbl2_stream`) is 56% of CSV decode time;
  its time is spread out, no single hot spot.
- balanced peak memory: independent blocks need the block and its output in memory; zstd -3 streams in 6 MB. Meeting
  1.5× needs a streaming block path (or chained blocks, which serialise decoding).
- hc text: −3.7% against zstd -19 --long; level 20 (−6.5% on enwik8) needs more memory than the 1.5× target allows.
- eco source code: block sorting is 2.6% (source.tar) to 4.4% (samba) larger than brotli -11's long-window LZ.
- eco decode of text is block-sorting speed (15–20 MB/s on this VM), 10× slower than brotli -11. If eco's decode speed
  matters more than its size, the block-sorting route should be limited (owner's call).

Raw rows: `results/*.jsonl` (rivals-T1, base2-T1, w4c-T1, bal2-*, T4-rivals, T4-base, T4-new2; T4-new is the same build before the writer thread was limited to one worker). Commands:
`python3 benchmark/w4/docs/run.py OUT.jsonl build/kolmo FILES... [--threads 4] [--tools ...] [--rounds N]` and
`python3 benchmark/w4/docs/summarize.py OUT.jsonl [--base OLD.jsonl] [--files]`.
