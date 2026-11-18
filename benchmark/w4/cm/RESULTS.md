# Wave 4, CM lane: `-x` speed, memory and ratio (codec 7 model 4)

18 November 2026, branch `claude/w4-cm-20261118` (from rc3 `5071f38`). Cloud VM: 4 vCPUs of an Intel Xeon at 2.1 GHz
(AVX-512 capable; the build is `-march=native`, so the AVX2 kernels run), 15 GB, Linux 6.18, gcc 13.3. zpaq 7.15
built from source (`g++ -O3 -DNDEBUG`, its x86-64 JIT on), a test tool only. Apple M5 numbers are left to the
integration session.

## Targets and outcome (one thread, full files)

| target (owner, wave 4) | outcome |
|---|---|
| `-x` at least 5% smaller than zpaq -m5 on every group | structured −36.7%, numeric −24.9% (x-ray, sao; mr below), Silesia binaries: nci −15.6%, ooffice −4.8%, samba −3.8%, osdb −1.5%, mozilla +1.2%. **Text −4.6%: missed** (xml −10.5%, reymont −8.5%, enwik8 −5.0%, webster −3.6% to −4.1%, dickens −0.7%). Model 3 had text −4.8% with 5–7 GB. |
| compress and decode at least as fast as zpaq -m5 at 1 thread (was 0.73× / 0.83× on text, M5) | text 1.00× / 1.03× (VM); every other file 1.02–3.3× / 1.1–9.8×. Two directions are 2–3% slower: enwik8 compress (0.98×), dickens decode (0.97×). |
| decode memory ≤ 2.5 GiB per block job | yes: at most about 1.9 GiB (a 128 MiB block); 1.43 GiB for enwik8 |
| peak memory not above about 1.5× zpaq -m5's | 0.48–1.17× on every file up to 41 MB; nci 1.30×; enwik8 1.58× (1.53 GB against 0.97 GB). Model 3: 5–7 GB on every file of 4 MB or more (3–30× zpaq). |

Group totals are sums over the files listed below (the M5 benchmark's groups, `benchmark/rc3/leaders`, as far as the
data could be fetched here: no OWID test tables, `api.json`, pickles or NumPy columns; silesia.tar is measured below if
time allowed).

## What changed

`-x` now writes **model 4** of codec 7 (stream model nibble 9, `SPEC.md`, *Model 4*):

1. **Fewer contexts, chosen by measurement.** A backward elimination on 15 samples of 1 MB (5 text, 3 structured, 5
   Silesia binaries, 2 numeric; `/tmp/c1` slices of the test files, other slices than the prior's training set) over
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

RESULTS_TABLES

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
