# rc3 xmode lane: `-x` on files under 128 MiB and under 1 MiB

Branch `claude/rc3-xmode-20261116`. Plan: `research/RC3-PLAN.md`, lane "xmode", items 1–7. Summary of which technique
serves which file size: `docs/XMODE-PLAN.md`.

## Machine and method

- Cloud VM: Intel Xeon (Sapphire Rapids class) @ 2.3 GHz, 4 vCPUs, 15.7 GiB, Linux 6.18, gcc 13, `make` defaults
  (`-O3 -march=native`). "All threads" is 4 on this machine, so the 1 / 4 / all columns of the plan are 1 and 4 here.
  The integration lane repeats the key runs on the Apple M5 (10 cores).
- Every number is one whole process (`benchmark/rc3/xmode/run.py`): wall time and peak resident memory from `wait4`,
  encode and decode separately, decoded output compared byte for byte. Runs are strictly one at a time
  (`queue.sh`): an early attempt that overlapped a 5.5 GiB experiment with a 4-thread `-x` run was killed by the
  container's memory limit and is not used.
- Inputs (`*_100k`, `*_1m`, `*_10m`, `*_100m`, the first N bytes, cut at line or record boundaries for CSV and FASTQ):
  - text: Silesia dickens, webster, nci, reymont, xml, dickens, concatenated;
  - csv: UCI "individual household electric power consumption" (semicolon-separated, 9 columns);
  - bin: Silesia mozilla, samba, ooffice, sao, x-ray, mr, concatenated;
  - fq: the first reads of SRR2584863_1 (E. coli, from ENA), whole records.
- Builds: `kolmo-base` = rc3 base `2207da1`; `kolmo-s1` … `kolmo-s6` = the tree after each step (commits `feda9f3`,
  `4749789`, `5bd1fbd`, the steps of `5bd1fbd` built with `-DCMP_SPLIT_MIN=SIZE_MAX -DKF_CHUNK_MIN=KF_CHUNK` for s4 and
  `-DKF_CHUNK_MIN=KF_CHUNK` for s5); `kolmo-final` = the branch head after the merge of the structure lane (frames
  identical to s6 on every file checked).
- A step is measured on the files it can change. Rows a step cannot change are equal to the previous step by
  construction and are not re-run: step 1 changes nothing at 1 thread (its trials only start when the block has a
  thread to spare), step 2 only FASTQ, step 4 only tables, step 5 only non-text streams of 8 MiB and more, step 6
  only FASTQ blocks between about 2 and 64 MiB (fq_10m here; 1 MB has a single quality chunk either way and 100 MB
  keeps 4 MiB chunks).

## Micro-measurements behind the choices

### Where CM time goes (callgrind, 50 KB of text, model 3, `-march=haswell`)

37,000 instructions per byte (4,600 per bit). Counter updates of the 30 models (`cm_upd_ctr`) 25%, state-map updates
(`cm_upd_sm`) 15%, bucket priority increments 6%, stretch lookups 5%, hashing 5%, the four mixers' dot products 7% and
weight updates 4%. Tables do not dominate at 1 MB: capping them at 2^16 buckets per model (116 MiB instead of 1.4 GiB)
left the speed unchanged and cost 1.7%.

### Step 3 kernels (`cmbench`, codec 7 alone, same stream hash before and after)

| change | input | encode | decode | output |
|---|---|---:|---:|---|
| none (base) | text 1 MB, model 3 | 5.28 s | 5.14 s | — |
| prefetch of the next bit's w4 / a2 / a3 rows | text 1 MB, model 3 | 4.03 s | 4.00 s | identical |
| … | binary 1 MB, model 2 | 3.96 → 3.14 s | 5.03 → 3.01 s | identical |
| byte-ahead prefetch of 10 hashed models (2 candidates) | text 1 MB | 3.96 s | 4.14 s | identical (no gain, dropped) |
| baseline-ISA build (`-march=x86-64`) without dispatch | text 300 KB | 1.51 s | 1.55 s | identical |
| baseline-ISA build with the AVX2 loop clone | text 300 KB | 1.19 s | 1.21 s | identical (−21%) |
| `-march=native` build | text 300 KB | 1.25–1.36 s | 1.25–1.48 s | identical |
| hand-written AVX2 dot4 / train4 (`-march=haswell`) | text 300 KB | 1.12–1.15 s vs 1.14–1.18 s | 1.13 vs 1.13–1.21 s | identical (2%, noise: dropped) |

### Step 4: one model per column (1 MB of CSV, codec 7 streams)

| coding | bytes |
|---|---:|
| raw, model 3 (what `-x` wrote) | 51,588 |
| raw, model 2 | 52,253 |
| 9 columns, best model per column | 51,012 |
| 9 columns, model 2 each (codec 25 as written) | 51,036 |

### Step 5: equal parts (`cmpbench`, codec 25, 4 threads; encode time)

| input | parts | table cap | bytes | vs 1 part | encode |
|---|---:|---|---:|---:|---:|
| text 10 MB | 1 | — | 2,025,927 | — | 46.2 s |
| text 10 MB | 2 | — | 2,082,069 | +2.77% | 46.5 s (two 5.4 GiB models do not fit the memory budget together) |
| text 10 MB | 4 | — | 2,155,098 | +6.38% | 47.7 s |
| text 10 MB | 8 | — | 2,245,955 | +10.86% | 17.5 s |
| text 100 MB | 1 | — | 9,749,207 | — | 373.7 s |
| text 100 MB | 4 | 2^20 | 10,592,747 | +8.65% | 113.5 s |
| bin 10 MB | 1 | — | 4,287,250 | — | 31.4 s |
| bin 10 MB | 2 | — | 4,302,227 | +0.35% | 18.2 s |
| bin 10 MB | 2 | 2^21 | 4,304,475 | +0.40% | 16.4 s |
| bin 10 MB | 4 | — | 4,386,183 | +2.31% | 16.9 s |
| bin 10 MB | 8 | — | 4,430,890 | +3.35% | 8.5 s |
| bin 100 MB | 1 | — | 25,192,824 | — | 276.2 s |
| bin 100 MB | 2 | 2^21 | 25,228,916 | +0.14% | 136.8 s |
| bin 100 MB | 4 | 2^20 | 25,420,441 | +0.90% | 74.2 s |
| bin 100 MB | 8 | 2^20 | 25,519,168 | +1.30% | 83.5 s |
| csv 10 MB | 1 | — | 464,346 | — | 32.4 s |
| csv 10 MB | 2 | — | 465,007 | +0.14% | 32.8 s |
| csv 10 MB | 2 | 2^21 | 464,993 | +0.14% | 15.6 s |
| csv 10 MB | 4 | — | 467,762 | +0.74% | 33.7 s |
| csv 10 MB | 8 | — | 473,208 | +1.91% | 12.2 s |
| csv 100 MB | 1 | — | 4,470,882 | — | 312.1 s |
| csv 100 MB | 4 | 2^20 | 4,380,465 | −2.02% | 77.7 s |

Adopted: two halves with half-size tables for non-text streams from 8 MiB (≤ 0.40% measured, 2× faster both ways,
the same memory as one model). Rejected: any split of text; 4 or 8 parts of binary data. A split by `-T` (the plan's
"n/T") was not adopted: frames must not depend on the thread count. The shared static prior was not attempted.

### Step 6: FASTQ chunks (`fqbench`, family coder alone; decode = best of 3)

| input | name/quality chunk | base chunk | bytes | vs default | encode T4 | decode T4 | decode T1 |
|---|---|---|---:|---:|---:|---:|---:|
| fq 10 MB | 4 MiB (rc2) | 64 MiB | 1,885,943 | — | 1.12 s | 1.26 s | 2.43 s |
| fq 10 MB | 1 MiB | 64 MiB | 1,896,728 | +0.57% | 0.83 s | 0.89 s | 2.18 s |
| fq 10 MB | 512 KiB | 64 MiB | 1,907,055 | +1.12% | 0.92 s | 0.94 s | |
| fq 10 MB | 256 KiB | 64 MiB | 1,920,926 | +1.86% | 1.08 s | 0.73 s | |
| fq 10 MB | 1 MiB | 16 MiB | 1,896,728 | +0.57% | 0.82 s | 0.79 s | |
| fq 100 MB | 4 MiB (rc2) | 64 MiB | 13,978,267 | — | 9.03 s | 9.13 s | |
| fq 100 MB | 1 MiB | 64 MiB | 14,079,731 | +0.73% | 11.07 s | 10.57 s | |
| fq 100 MB | 512 KiB | 64 MiB | 14,172,601 | +1.39% | 11.15 s | 9.53 s | |
| fq 100 MB | 256 KiB | 64 MiB | 14,305,699 | +2.34% | 9.41 s | 10.85 s | |
| fq 100 MB | 1 MiB | 16 MiB | 15,811,415 | +13.1% | 6.80 s | 6.84 s | |

At 100 MB the single bases job bounds decode time, so smaller quality chunks do not help; cutting the bases costs
13%. Adopted: name and quality chunks of a sixteenth of the block, between 1 and 4 MiB (blocks under 64 MiB get more
chunks; 100 MB keeps 4 MiB).

## End to end, step by step

STEP_TABLES

## 100 MB, before and after

TABLES_100M

## Tests

Before the push of `53895f8`, on the merged tree: `make check`, `make check-rc`, `make check-sanitizers`,
`make experimental-check GPU_CHECK=`, `make kfam-check` all pass. `benchmark/determinism/check.py`: 108 of 110 frames
unchanged; `table.csv` at `-x` and `-x -l 2` now use codec 25 (78,869 → 77,764 bytes) and the expected digests were
regenerated in a commit of their own; 1 and 4 threads give the same frames.

## What failed or is left

- Hand-written SIMD mixer kernels and a byte-ahead bucket prefetch: no measurable gain (above).
- Block-parallel text: every split costs 3–11%; plain text over 10 MB remains `-x`'s slowest case. Next candidate: a
  shared static prior (parts warmed on the same leading bytes), not measured here.
- The vectorisable part of the per-bit work is the 30 counter and state-map updates (45% of instructions); they are
  gathers and scatters across 30 cache lines, which AVX2 cannot scatter. Not attempted.
- NEON: the AVX2 loop clone is x86-only; arm64 builds use the generic loops (the simd-arm lane can add the same clone
  with `target("+simd")` if it measures a gain).
- Peak encode memory at 4 threads rises with step 1 (the alternative coder and a second planner sample beside CM).
