# kfam: the FASTQ family coder

kfam splits FASTQ text into five streams by family and codes each with a small model of its own and a binary
arithmetic coder written for Kolmo:

| stream | content | model |
|----|----|----|
| 0 layout | per record: plus-line form, read length when it changes | byte model |
| 1 names | per name token: same as before / numeric delta / literal | byte model, token index and operation state as context |
| 2 bases | A C G T as 2-bit symbols | orders 6 and 12, three match models |
| 3 exceptions | each non-ACGT base: site delta, byte | byte model |
| 4 qualities | quality - 33 | the quality model below |

The core is `src/kfam.inc`. Kolmo includes it as block codec 16 (`FQFAM`, container version 10, `-x` only);
`src/experimental/kfam.c` is the standalone research tool on the same frame (it adds a CSV family and a one-stream
family). The frame layout is in `SPEC.md`, *FASTQ family blocks*.

```sh
make ARCH= build/kfam kfam-check
./build/kfam -T 10 in.fastq out.kfm        # compress   (-C names/qualities chunk, -B bases chunk; 0 = one job)
./build/kfam -d -T 10 out.kfm back.fastq   # decompress
./build/kfam -T 10 -b in.fastq             # in memory: fastest of 3+ runs each way, JSON line, round trip checked
./build/kfam -v in.fastq                   # coded size of each stream
./build/kfam -S 9 in.fastq                 # CPU time of every job alone, and the thread pool's speed-up bound
./build/kolmo -x in.fastq out.kz           # kolmo: keeps the family coding when it is smaller than -x's own
```

## Models

**Qualities.** Contexts in the spirit of the fqzcomp quality model as published (Bonfield and Mahoney, *Compression
of FASTQ and SAM format sequencing data*, PLoS ONE 2013), re-implemented from that description, combined by context
mixing instead of one direct context:

- the symbols are coded as paths in an order-preserving binary tree, split where the two sides' counts are closest
  (about as many binary decisions per quality as its order-0 entropy in bits; every decision is a threshold, which
  carries over between neighbouring contexts: 0.25% smaller than a Huffman-shaped tree);
- six counter tables: (q1, q2); (q1, max(q2, q3), q2 = q3, position/4); (q1, position, change sum); (q1, q2, q3,
  change sum/2) hashed; (position, mean so far, base is N, q1/2); (q1, change sum, mean so far, base is N). q1..q3
  are the previous qualities of the read, the change sum is the running sum of |q[i] - q[i-1]| bucketed to 16
  levels, the mean is quantised to 16 levels; whether the base is N comes from the exception stream, which is
  decoded first, so the big streams stay independent;
- two mixers (weights selected by tree node with the change sum, and by tree node with the position), a second-layer
  mixer over the two, and two adaptive probability maps (q1 with the node; position with the node) averaged with
  the mixer's output; slow learning rates (the data is stationary: counters adapt as 1/n up to n = 1000);
- read lengths come from the layout stream, so the quality stream has no separators.

Tried and dropped (SRR835775 qualities, 10 M symbols): an order-4 context, the previous read's quality at the same
position, (q1, q2, position), a fast second counter per slot (each 0.0 to +1.2% larger); the previous model's 7-bit fixed
tree with 4 tables: 3,523,361 bytes; this model: 3,232,969 (one job) / 3,246,173 (4 Mi chunks).

**Bases.** Orders 6 and 12 (order 12 hashed into 64-byte buckets by its older 11 bases, so the next base's bucket
is prefetched one base ahead) and three match models: the earlier occurrence of the last 14 bases read forwards,
the earlier occurrence of their reverse complement read backwards (reads come from both strands), and the last
occurrence of the last 8 bases within 1,024 bases (neighbour reads overlap in sorted or amplicon data). A match
survives mismatches (variants, read errors); its reliability is learned per match length and recent misses. The
previous model (orders 2, 4, 8, 12, 16, 22, no match model): 857,582 / 2,905,697 / 4,457,251 bytes on the three runs;
this one: 543,165 / 1,700,305 / 2,035,092. More orders made the output larger and slower.

**Names.** The token coding is unchanged; the byte model's counters now adapt with a rate floor of 1/17 (they
were near-stationary): SRR2584863 names 420,566 -> 320,536 bytes.

## Results

Three Illumina runs, 100,000 reads each (whole files): SRR835775 (human, 100 bp, apparently in genome order:
neighbour reads overlap), SRR2584863 (E. coli, HiSeq 2000, 150 bp), SRR1770413 (301 bp, deep coverage of a small
genome). Sizes in bytes.

| | SRR835775 | SRR2584863 | SRR1770413 | total |
|----|---:|---:|---:|---:|
| input | 22,877,790 | 36,637,534 | 63,177,790 | 122,693,114 |
| CRAM 3.1 (earlier measurement) | | | | 25,289,976 |
| kolmo -m ratio | 4,879,646 | 8,031,260 | 10,327,642 | 23,238,548 |
| kolmo -x, 1.2.0-rc2 | 3,890,027 | | | 22,246,381 |
| kfam before (one job per stream) | 4,383,550 | 7,597,261 | 9,828,876 | 21,809,687 |
| **kfam now (default chunks)** | **3,792,114** | **5,901,977** | **6,926,684** | **16,620,775** |
| kfam now, one job per stream | 3,778,896 | 5,882,778 | 6,888,902 | 16,550,576 |
| **kolmo -x, this branch** | **3,792,166** | **5,902,029** | **6,926,736** | **16,620,931** |

kfam is 23.8% smaller than before, 25.3% smaller than kolmo -x of rc2 and 34.3% smaller than CRAM 3.1 on these
runs; on SRR835775, where kolmo -x was smaller than kfam, kfam is now 2.5% smaller than kolmo -x. kolmo -x on this
branch keeps the family coding of each file (the frame is the kfam frame plus 52 bytes of container).

Streams (bytes, default chunks):

| stream | SRR835775 | SRR2584863 | SRR1770413 |
|----|---:|---:|---:|
| layout | 79 | 80 | 80 |
| names | 811 | 320,536 | 811 |
| bases | 543,165 (0.43 bit/base) | 1,700,305 (0.91) | 2,035,092 (0.54) |
| exceptions | 1,696 | 397 | 8 |
| qualities | 3,246,173 (2.60 bit/quality) | 3,880,458 (2.07) | 4,890,472 (1.30) |

Before: bases 857,582 / 2,905,697 / 4,457,251 and qualities 3,523,361 / 4,270,438 / 5,370,626. The quality stream
is now 86% of the output on SRR835775 (it was 80%).

## Chunking

Names and qualities are cut at record boundaries once a job reaches `-C` symbols, bases once it reaches `-B`; each
job starts with fresh models. The cut points are in the frame, so they depend only on the input, never on the thread
count. Sizes (bytes; bases one job):

| -C (names, qualities) | SRR835775 | SRR2584863 | SRR1770413 | total | vs one job |
|----|---:|---:|---:|---:|---:|
| one job | 3,778,896 | 5,882,778 | 6,888,902 | 16,550,576 | |
| 1 Mi | 3,819,845 | 5,940,903 | 6,990,876 | 16,751,624 | +1.21% |
| 2 Mi | 3,801,529 | 5,918,410 | 6,953,758 | 16,673,697 | +0.74% |
| **4 Mi (default)** | **3,792,114** | **5,901,977** | **6,926,684** | **16,620,775** | **+0.42%** |
| 8 Mi | 3,784,831 | 5,890,519 | 6,907,830 | 16,583,180 | +0.20% |

Bases cost far more to cut, because their models learn the genome (coverage): with `-C 4Mi`,

| -B (bases) | SRR835775 bases | SRR2584863 bases | SRR1770413 bases |
|----|---:|---:|---:|
| one job (default 64 Mi: one job on all three runs) | 543,165 | 1,700,305 | 2,035,092 |
| 16 Mi | 543,165 (one job) | 1,700,305 (one job) | 2,890,500 (+42%) |
| 8 Mi | 541,851 (-0.2%) | 2,252,324 (+32%) | 3,820,441 (+88%) |

Speed-up bound of the thread pool's schedule (largest job first), from the CPU time of every job coded alone
(`kfam -S 9`; CPU time is steadier than elapsed time on the shared machine; the bound is the same at 4 and at 10
threads because the bases job alone sets it):

| setting | SRR835775 | SRR2584863 | SRR1770413 |
|----|---:|---:|---:|
| one job per stream (`-C 0`; the earlier kfam, which scaled 1.8x) | 1.53x | 1.91x | 1.90x |
| -C 1 Mi | 3.11x | 2.71x | 2.08x |
| -C 2 Mi | 3.38x | 2.74x | 2.08x |
| **-C 4 Mi (default)** | **3.39x** | **2.82x** | **2.03x** |
| -C 8 Mi | 1.76x | 2.63x | 2.17x |
| -C 4 Mi -B 16 Mi | 3.33x | 2.68x | 3.47x (bases +42%) |

Default: `-C 4Mi` (0.42% larger than one job per stream, the qualities no longer limit the speed-up) and `-B 64Mi`
(bases are cut only in inputs above about 135 MB of FASTQ, where a 64 Mi-base model has seen much of the genome).
What is left is the bases job: it is 30 to 49% of the CPU time, so more threads cannot help until its model is
faster or shares state between chunks.

## Timing

Provisional: Apple M5 (4 performance and 6 efficiency cores), shared with other work throughout (load average 7 to
28 on 10 cores while these ran), so elapsed times vary by up to 2x between runs; `-T 10` also puts work on the
efficiency cores. MB/s are input megabytes (10^6) per second.

In memory (`kfam -b`: fastest of at least three runs each way, round trip checked; load average 9.5 to 14.5):

| threads | SRR835775 c / d | SRR2584863 c / d | SRR1770413 c / d |
|----|---:|---:|---:|
| 1 | 6.6 / 5.9 | 6.4 / 5.4 | 8.9 / 7.7 |
| 4 | 13.2 / 11.7 | 13.2 / 11.4 | 14.0 / 12.6 |
| 10 | 11.4 / 10.4 | 11.0 / 7.7 | 9.5 / 9.2 |

The earlier kfam and this one back to back (same conditions, load average 7 to 10):

| | threads | SRR835775 c / d | SRR2584863 c / d | SRR1770413 c / d |
|----|----|---:|---:|---:|
| before | 1 | 7.4 / 5.5 | 5.4 / 4.3 | 8.2 / 6.3 |
| now | 1 | 8.2 / 6.3 | 6.0 / 6.4 | 9.7 / 6.6 |
| before | 10 | 15.7 / 10.7 | 11.9 / 9.9 | 13.1 / 10.4 |
| now | 10 | 21.4 / 18.1 | 17.2 / 14.5 | 17.4 / 12.8 |

So the new models cost no speed on one thread (the quality tree takes about 4.2 binary decisions per quality on
SRR835775, whose order-0 entropy is 4.1 bits, where the old fixed tree took 7; the base model has 2 orders instead
of 6), and chunking gives 1.2x to 1.7x more at 10 threads than before.

Whole processes (`benchmark/rc2/run_cli2.py`: fresh processes, fastest of up to three rounds, a setting stops
after a round once 20 s were spent; decoded bytes checked by SHA-256):

| setting | SRR835775 c / d | SRR2584863 c / d | SRR1770413 c / d | peak memory (c) |
|----|---:|---:|---:|---:|
| kfam -T 1 | 1.86 / 1.70 | 2.21 / 3.75 | 6.16 / 11.50 | 264 - 507 MB |
| kfam -T 4 | 2.32 / 4.14 | 4.64 / 8.97 | 13.34 / 24.63 | 418 - 595 MB |
| kfam -T 10 | 2.39 / 4.47 | 4.63 / 16.85 | 12.34 / 22.00 | 457 - 843 MB |
| kolmo -x -T 1 | 0.018 / 7.19 | | | 4.7 GB |
| kolmo -x -T 4 | 0.130 / 22.97 | 0.231 / 20.11 | 0.637 / 22.33 | 4.3 - 5.1 GB |
| kolmo -x -T 10 | 0.037 / 9.90 | 0.392 / 20.39 | 0.639 / 21.80 | 4.6 - 5.3 GB |

The kfam rows for SRR835775 and SRR2584863 ran while the machine was at its busiest (the same runs in memory, a
little later, gave 6 to 13 MB/s); SRR1770413 ran when it was quieter. `kolmo -x` compression is `-x`'s own work
(context mixing of the whole block, then its alternatives) plus the family coding: kfam's part is a few seconds of
the 99 to 1,289 s measured here (1.2.0-rc2's `-x -T 10` took 37 s on SRR835775 on a quieter machine at the start of
the session; its run here was stopped after 19 minutes, and the two larger runs were measured at 4 and 10 threads
only for that reason). Decompression of the family-coded frames takes 1 to 3 s, where 1.2.0-rc2's `-x` frames
decode by context mixing, about as slowly as they encode.

## In Kolmo (block codec 16)

At `-x` (ultra levels 0 and 2; not `-x -l 1`), after a block has been coded the usual way, a block that is complete
FASTQ text is also coded by the family coder, and that coding is kept when it is smaller. The family coder runs
with the threads the block pool leaves over (all of them for a one-block file). Only frames with such a block are
version 10; readers up to 1.2.0-rc2 refuse them at the header ("corrupt or unsupported input"). No other mode and no
other input changes (see *Byte identity*).

Limits: `-x` cuts inputs into 128 MiB blocks at byte boundaries, so in a FASTQ file larger than 128 MiB no block is
complete FASTQ and the family coder is not used; aligning `-x` blocks to records (or letting codec 16 carry a
partial record at each end) is the next step. Memory: about 75 MB per quality job and up to 330 MB for a 64 Mi-base
job, on top of `-x`'s own (4.8 GB on SRR835775).

### Byte identity

The 1.2.0-rc2 build (9d4496a) and this branch's build compressed every file of `kolmo-bench/data` (35 files,
474 MB: text, CSV, JSON, logs, genomes in FASTA, images, model weights, executables) at `-T 4`, and the outputs
were compared byte for byte:

| mode | identical | different |
|----|---:|---:|
| `-m balanced` | 35 | 0 |
| `-m hc` | 35 | 0 |
| `-m ratio` | 35 | 0 |
| `-x -l 1` | 35 | 0 |
| `-x` (the 10 files under 4 MB) | 10 | 0 |

The three FASTQ runs are also identical at `-m balanced`, `-m hc` and `-m ratio`, and SRR835775 at `-x -l 1`.
`tests/test_fqfam.c` (`make check-fqfam`, part of `check-rc`) checks that rt, fast, eco, hc, max and `-x -l 1`
never write codec 16, that `-x` does, and that the output does not depend on the thread count. A version 9 reader
(rc2) given a version 10 frame stops with "kolmo: corrupt or unsupported input" and exit status 1.
