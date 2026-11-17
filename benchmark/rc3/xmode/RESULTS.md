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

Speeds are MB/s of input (higher is better), peak resident memory in MiB. "base" is rc3 base; each later label is
the tree after that step, measured only on the files the step can change (the rows a step cannot change are the
previous step's). Step 5's rows on `text_10m` and `bin_10m` are unchanged on purpose: text is never split and the
binary sample is a tar archive cut into blocks under 8 MiB; Silesia osdb (a single 10 MB non-text stream) is below.
Step 7 (`s7-scaled`) is the opt-in `--x-tables=scaled` on the final build; the default build is unchanged by it.

Headlines (VM, 4 threads unless noted):

| file | before | after all default steps | what did it |
|---|---|---|---|
| FASTQ 10 MB | encode 0.22 MB/s, 5.4 GiB | encode 9.0 MB/s, 294 MiB; decode 6.5 → 8.7 MB/s | 2 (41×), 6 (decode, +0.57%) |
| FASTQ 1 MB, 1 thread | encode 0.21 MB/s, 1.4 GiB | 4.9 MB/s, 32 MiB | 2 (23×) |
| CSV 10 MB | 464,389 B; 0.27 / 0.31 MB/s | 449,066 B (−3.3%); 0.99 / 1.58 MB/s | 4 (3.7× / 5.1×) |
| CSV 1 MB | 51,640 B; 0.15 / 0.32 MB/s | 51,395 B (−0.5%); 0.38 / 1.54 MB/s | 1, 4 |
| text 1 MB | 0.088 / 0.214 MB/s | 0.145 / 0.224 MB/s | 1 (planner samples in parallel), 3 |
| binary 1 MB, 1 thread | 0.19 / 0.26 MB/s | 0.33 / 0.28 MB/s | 3 |
| binary 10 MB | 0.31 / 0.32 MB/s | 0.41 / 0.40 MB/s | 3 |
| text 10 MB | 0.17 / 0.20 MB/s | 0.19 / 0.20 MB/s | 1, 3 (text stays one model) |
| any 100 KB | 0.12–0.30 MB/s | 0.14–0.33 MB/s; FASTQ 2.3 MB/s | 3; 2 |

Costs: step 1 raises peak encode memory at 4 threads (text 1 MB 1.4 → 2.3 GiB, CSV 10 MB with columns 6.8 GiB);
step 4's column trial makes 100 KB CSV encode slower at 1 thread (0.12 → 0.08 MB/s) when it does not win; on the
10 MB sizes step 3 is within noise for text and CSV (the VM's timing noise is about ±8%).

| file | T | step | size | ratio | enc MB/s | enc peak MiB | dec MB/s | dec peak MiB |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| text_100k | 1 | base | 28,061 | 3.564 | 0.164 | 278 | 0.223 | 276 |
| text_100k | 1 | s3-prefetch-avx2 | 28,061 | 3.564 | 0.230 | 278 | 0.221 | 276 |
| text_100k | 1 | s7-scaled | 28,080 | 3.561 | 0.200 | 162 | 0.226 | 160 |
| text_100k | 4 | base | 28,061 | 3.564 | 0.170 | 278 | 0.237 | 276 |
| text_100k | 4 | s1-trials | 28,061 | 3.564 | 0.163 | 281 | 0.188 | 276 |
| text_100k | 4 | s3-prefetch-avx2 | 28,061 | 3.564 | 0.196 | 281 | 0.251 | 277 |
| text_100k | 4 | s7-scaled | 28,080 | 3.561 | 0.225 | 165 | 0.252 | 160 |
| csv_100k | 1 | base | 6,604 | 15.142 | 0.122 | 278 | 0.282 | 276 |
| csv_100k | 1 | s3-prefetch-avx2 | 6,604 | 15.142 | 0.143 | 278 | 0.248 | 276 |
| csv_100k | 1 | s4-columns | 6,604 | 15.142 | 0.082 | 278 | 0.257 | 277 |
| csv_100k | 1 | s7-scaled | 6,604 | 15.142 | 0.091 | 162 | 0.314 | 161 |
| csv_100k | 4 | base | 6,604 | 15.142 | 0.140 | 279 | 0.295 | 276 |
| csv_100k | 4 | s1-trials | 6,604 | 15.142 | 0.157 | 407 | 0.261 | 277 |
| csv_100k | 4 | s3-prefetch-avx2 | 6,604 | 15.142 | 0.161 | 407 | 0.319 | 277 |
| csv_100k | 4 | s4-columns | 6,604 | 15.142 | 0.144 | 650 | 0.276 | 277 |
| csv_100k | 4 | s7-scaled | 6,604 | 15.142 | 0.191 | 490 | 0.274 | 160 |
| bin_100k | 1 | base | 14,276 | 7.005 | 0.283 | 205 | 0.370 | 204 |
| bin_100k | 1 | s3-prefetch-avx2 | 14,276 | 7.005 | 0.308 | 206 | 0.361 | 204 |
| bin_100k | 1 | s7-scaled | 14,291 | 6.997 | 0.394 | 125 | 0.353 | 124 |
| bin_100k | 4 | base | 14,276 | 7.005 | 0.304 | 205 | 0.368 | 204 |
| bin_100k | 4 | s1-trials | 14,276 | 7.005 | 0.320 | 207 | 0.329 | 204 |
| bin_100k | 4 | s3-prefetch-avx2 | 14,276 | 7.005 | 0.329 | 207 | 0.330 | 204 |
| bin_100k | 4 | s7-scaled | 14,291 | 6.997 | 0.407 | 126 | 0.398 | 124 |
| fq_100k | 1 | base | 22,912 | 4.358 | 0.234 | 278 | 3.715 | 15 |
| fq_100k | 1 | s2-fastq-first | 22,912 | 4.358 | 2.149 | 28 | 3.479 | 14 |
| fq_100k | 4 | base | 22,912 | 4.358 | 0.231 | 278 | 6.303 | 15 |
| fq_100k | 4 | s1-trials | 22,912 | 4.358 | 0.204 | 286 | 5.785 | 15 |
| fq_100k | 4 | s2-fastq-first | 22,912 | 4.358 | 2.337 | 28 | 6.053 | 15 |
| text_1m | 1 | base | 216,874 | 4.611 | 0.094 | 1381 | 0.199 | 1377 |
| text_1m | 1 | s3-prefetch-avx2 | 216,874 | 4.611 | 0.102 | 1381 | 0.225 | 1377 |
| text_1m | 1 | s7-scaled | 220,470 | 4.536 | 0.112 | 169 | 0.206 | 165 |
| text_1m | 4 | base | 216,874 | 4.611 | 0.088 | 1381 | 0.214 | 1377 |
| text_1m | 4 | s1-trials | 216,874 | 4.611 | 0.111 | 2357 | 0.187 | 1377 |
| text_1m | 4 | s3-prefetch-avx2 | 216,874 | 4.611 | 0.145 | 2358 | 0.224 | 1377 |
| text_1m | 4 | s7-scaled | 220,470 | 4.536 | 0.130 | 277 | 0.223 | 165 |
| csv_1m | 1 | base | 51,640 | 19.364 | 0.139 | 1378 | 0.286 | 1373 |
| csv_1m | 1 | s3-prefetch-avx2 | 51,640 | 19.364 | 0.131 | 1378 | 0.292 | 1373 |
| csv_1m | 1 | s4-columns | 51,395 | 19.456 | 0.158 | 1171 | 0.463 | 358 |
| csv_1m | 1 | s7-scaled | 51,395 | 19.456 | 0.175 | 133 | 0.524 | 125 |
| csv_1m | 4 | base | 51,640 | 19.364 | 0.147 | 1378 | 0.319 | 1373 |
| csv_1m | 4 | s1-trials | 51,640 | 19.364 | 0.180 | 2329 | 0.326 | 1373 |
| csv_1m | 4 | s3-prefetch-avx2 | 51,640 | 19.364 | 0.172 | 2329 | 0.329 | 1373 |
| csv_1m | 4 | s4-columns | 51,395 | 19.456 | 0.378 | 2824 | 1.535 | 1116 |
| csv_1m | 4 | s7-scaled | 51,395 | 19.456 | 0.360 | 497 | 1.209 | 489 |
| bin_1m | 1 | base | 622,662 | 1.606 | 0.192 | 1167 | 0.260 | 1166 |
| bin_1m | 1 | s3-prefetch-avx2 | 622,662 | 1.606 | 0.327 | 1167 | 0.277 | 1166 |
| bin_1m | 1 | s7-scaled | 624,302 | 1.602 | 0.276 | 128 | 0.283 | 126 |
| bin_1m | 4 | base | 622,662 | 1.606 | 0.250 | 1167 | 0.199 | 1166 |
| bin_1m | 4 | s1-trials | 622,662 | 1.606 | 0.247 | 1178 | 0.240 | 1166 |
| bin_1m | 4 | s3-prefetch-avx2 | 622,662 | 1.606 | 0.292 | 1178 | 0.277 | 1166 |
| bin_1m | 4 | s7-scaled | 624,302 | 1.602 | 0.311 | 138 | 0.323 | 126 |
| fq_1m | 1 | base | 211,473 | 4.727 | 0.209 | 1378 | 4.318 | 26 |
| fq_1m | 1 | s2-fastq-first | 211,473 | 4.727 | 4.864 | 31 | 5.023 | 26 |
| fq_1m | 1 | s6-fq-chunks | 211,473 | 4.727 | 4.868 | 32 | 4.375 | 26 |
| fq_1m | 4 | base | 211,473 | 4.727 | 0.248 | 1378 | 6.620 | 35 |
| fq_1m | 4 | s1-trials | 211,473 | 4.727 | 0.222 | 1416 | 5.369 | 35 |
| fq_1m | 4 | s2-fastq-first | 211,473 | 4.727 | 6.099 | 39 | 6.300 | 35 |
| fq_1m | 4 | s6-fq-chunks | 211,473 | 4.727 | 6.482 | 39 | 7.498 | 35 |
| text_10m | 1 | base | 2,025,970 | 4.936 | 0.179 | 5417 | 0.190 | 5400 |
| text_10m | 1 | s3-prefetch-avx2 | 2,025,970 | 4.936 | 0.185 | 5417 | 0.207 | 5400 |
| text_10m | 1 | s5-halves | 2,025,970 | 4.936 | 0.185 | 5417 | 0.219 | 5400 |
| text_10m | 1 | s7-scaled | 2,129,407 | 4.696 | 0.189 | 227 | 0.225 | 210 |
| text_10m | 4 | base | 2,025,970 | 4.936 | 0.167 | 5417 | 0.195 | 5400 |
| text_10m | 4 | s1-trials | 2,025,970 | 4.936 | 0.191 | 5434 | 0.188 | 5400 |
| text_10m | 4 | s3-prefetch-avx2 | 2,025,970 | 4.936 | 0.191 | 5433 | 0.200 | 5400 |
| text_10m | 4 | s5-halves | 2,025,970 | 4.936 | 0.185 | 5433 | 0.203 | 5400 |
| text_10m | 4 | s7-scaled | 2,129,407 | 4.696 | 0.207 | 286 | 0.223 | 210 |
| csv_10m | 1 | base | 464,389 | 21.534 | 0.277 | 5085 | 0.302 | 5072 |
| csv_10m | 1 | s3-prefetch-avx2 | 464,389 | 21.534 | 0.255 | 5085 | 0.293 | 5072 |
| csv_10m | 1 | s4-columns | 449,066 | 22.268 | 0.392 | 1964 | 0.477 | 1937 |
| csv_10m | 1 | s7-scaled | 449,425 | 22.251 | 0.417 | 161 | 0.539 | 135 |
| csv_10m | 4 | base | 464,389 | 21.534 | 0.270 | 5085 | 0.312 | 5072 |
| csv_10m | 4 | s1-trials | 464,389 | 21.534 | 0.291 | 5102 | 0.295 | 5073 |
| csv_10m | 4 | s3-prefetch-avx2 | 464,389 | 21.534 | 0.274 | 5102 | 0.303 | 5072 |
| csv_10m | 4 | s4-columns | 449,066 | 22.268 | 0.990 | 6797 | 1.582 | 6685 |
| csv_10m | 4 | s7-scaled | 449,425 | 22.251 | 1.212 | 635 | 1.753 | 497 |
| bin_10m | 1 | base | 4,333,900 | 2.307 | 0.254 | 4547 | 0.247 | 4535 |
| bin_10m | 1 | s3-prefetch-avx2 | 4,333,900 | 2.307 | 0.302 | 4547 | 0.315 | 4535 |
| bin_10m | 1 | s5-halves | 4,333,900 | 2.307 | 0.332 | 4547 | 0.298 | 4535 |
| bin_10m | 1 | s7-scaled | 4,379,930 | 2.283 | 0.365 | 147 | 0.329 | 135 |
| bin_10m | 4 | base | 4,333,900 | 2.307 | 0.312 | 7980 | 0.316 | 7973 |
| bin_10m | 4 | s1-trials | 4,333,900 | 2.307 | 0.303 | 7981 | 0.275 | 7973 |
| bin_10m | 4 | s3-prefetch-avx2 | 4,333,900 | 2.307 | 0.388 | 7981 | 0.381 | 7973 |
| bin_10m | 4 | s5-halves | 4,333,900 | 2.307 | 0.413 | 7981 | 0.404 | 7973 |
| bin_10m | 4 | s7-scaled | 4,379,930 | 2.283 | 0.452 | 380 | 0.453 | 374 |
| fq_10m | 1 | base | 1,885,995 | 5.302 | 0.234 | 5354 | 3.226 | 97 |
| fq_10m | 1 | s2-fastq-first | 1,885,995 | 5.302 | 3.979 | 99 | 3.801 | 96 |
| fq_10m | 1 | s6-fq-chunks | 1,896,780 | 5.272 | 4.272 | 100 | 4.139 | 97 |
| fq_10m | 4 | base | 1,885,995 | 5.302 | 0.220 | 5354 | 6.498 | 154 |
| fq_10m | 4 | s1-trials | 1,885,995 | 5.302 | 0.220 | 5485 | 7.575 | 154 |
| fq_10m | 4 | s2-fastq-first | 1,885,995 | 5.302 | 7.778 | 173 | 6.900 | 154 |
| fq_10m | 4 | s6-fq-chunks | 1,896,780 | 5.272 | 8.993 | 294 | 8.737 | 278 |


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
  shared static prior (parts warmed on the same leading bytes), not measured here. A separate study for the
  integration lane (branch `claude/dazzling-maxwell-p8y4u0`, `research/FINDINGS.md` section 7, a CM coder for tables)
  measured exactly that: every block starting from the model state trained on block 0 brought 16 blocks from +7–14%
  to about +2% (still above this lane's 0.5% budget, but the lead for splitting text), with 3.2–3.5× on 4 cores.
- The vectorisable part of the per-bit work is the 30 counter and state-map updates (45% of instructions); they are
  gathers and scatters across 30 cache lines, which AVX2 cannot scatter. Not attempted.
- NEON: the AVX2 loop clone is x86-only; arm64 builds use the generic loops (the simd-arm lane can add the same clone
  with `target("+simd")` if it measures a gain).
- Peak encode memory at 4 threads rises with step 1 (the alternative coder and a second planner sample beside CM).
