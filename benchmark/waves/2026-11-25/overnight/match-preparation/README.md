# B36 match preparation

**Keep the existing threaded KLZH2 classifier.** An exact reciprocal reduced the cost of isolated bucket classification on all four real files. It did not deliver a repeatable end-to-end encode CPU gain. In a second, longer run, CPU time increased by 0.13% for the TypeScript grammar, 1.88% for JSON, and 1.79% for the Tesseract module. End-to-end combined speed changed by −1.15%, +0.44% and −1.67%, respectively. Keep this as a research result. No default or production source change is proposed.

## Controlled results

Five scored A/A2/B rounds per table row, one prior warmup, on complete unmodified files. The encode CPU column is the trial’s percentage time difference against the two reference arms, paired by round. Positive means more CPU time. The last column is an isolated 18-bit KLZH2-style scan of the original file, with an actual class sum checked across paired runs. The isolated scan omits tree maintenance, scheduling, parsing, framing and I/O.

| Input | CLI setting | Worker class operations | Parse class operations | Encode CPU time change | Isolated billions of classes/s, original → reciprocal |
|---|---|---:|---:|---:|---:|
| TypeScript grammar | L5 T4 | 4,110,454 | 220,024 | +0.76% | 0.88 → 3.06 |
| JSON | L5 T4 | 7,299,742 | 403,240 | −2.83% | 0.68 → 1.41 |
| Tesseract OCR | L5 T4 | 10,216,411 | 2,056,000 | −0.98% | 1.51 → 2.50 |
| Retina image | L5 T3 | 21,992,312 | 7,272,479 | +0.74% | 0.70 → 1.74 |
| OpenJPEG module, below 1 MiB | L5 T4 | 0 | 0 | +3.81% | — |
| TypeScript grammar, T1 | L5 T1 | 0 | 0 | −1.98% | — |
| TypeScript grammar, L3 | L3 T4 | 0 | 0 | +4.56% | — |

The 12-scored-round recheck covered the three T4 inputs that exercised the path. Every frame was byte-identical across both binaries in **243 timed arms** including warmups, and all restored hashes matched using the pinned-main CLI. The ten separate instrumented passes (seven original, three rechecks) produced matching frames as well. Helper scheduling can change the number of worker scans. For example the TypeScript recheck counted 4,175,990 worker class operations, with exactly 220,024 parser operations in both runs. The counter sums account for each helper chunk traversal, rather than assuming the input length is the number of work items.

## What was tested

The audit's original pinned-main klz.c SHA-256 was f23d9f1a1c4888bf2c81ac236578ab34b77d0450e5637d34850a4c5f2d814d4d. Its worker and parser compute a four-byte hash and take the bucket class modulo the runtime worker count. The November PR #37 source commit f16c0ad1 belongs to a different comparison. It runs lower codec levels to bound the size of higher levels; its documented approximately 1.5× and 2.1× encode costs cannot establish a match-classification speedup. The assignment ledger supplies no previous implementation for B36. This new qualification targeted the two ordinary class calculations, without changing the finder, model or parser.

The trial computes ceil(2^32/K) once per threaded finder and uses multiply/shift/subtract at the two classification sites. The live finder uses 18–20 hash bits and at most 16 classes. Multiplying by the rounded reciprocal adds less than 2^-12 to h/K throughout that domain, smaller than the 1/K spacing of possible fractional parts. The quotient floor therefore agrees exactly. An independent compiled check enumerated all 16,777,216 combinations of 20-bit hashes and K=1…16. No extra tables, hashing passes or production allocations were introduced. This experiment says nothing about an independently designed shared hash cache or a different classifier.

## Reproduction and scope

Base ac10ce80048ab0664074f7ba031634055f4d2225 (PR #104), source file SHA-256 4b415990f906256a7cb0dfdf83a61cd543d19413d8fbc81c3ab1b51ae0249e37. The untouched comparator rebuilt byte-identical to the existing base executable, SHA-256 552bd577b79b0cb48561747f613dd2ff8ee82d9c1eefba9b2e2a653f277a259f. Trial binary b2b9ae33cdc347d303bee5c56505fdca3ab4a5909e05f3b34c4e07f99e457519. Counting binary d02abc4d46d3992edcee2adc5b495f763c1882d220ad54f8541bb330d2536132. Stock decoder a1d39559686a9ec17c8614b9154a64d7b5b98672, binary SHA-256 b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f.

Build the base's CLI with make build/kolmo. Run build.sh BASE WORK to reproduce the three executables from the same objects. cases.py lists the corpus paths and thread/level settings. Run replay.py OUTPUT WORK STOCK_READER; for the extended recheck add 12 typescript json ocr. summarize.py optionally takes OUTPUT and the scored round count. Build classify.c and check_remainder.c with Apple clang -O3 -mcpu=native. Classify accepts a file and a worker-class count. A compiler memory barrier prevents reusing a previous timing arm's checksum. classifier.csv has one warmup and ten scored rounds in balanced rotating order. results.csv and recheck/results.csv include individual encode/decode/combined throughput, A/A2 differences and paired CPU observations. The JSONL files preserve source and frame hashes, CPU seconds, elapsed times and peak RSS. Throughput is decimal MB/s on a shared Apple ARM64 host; the recorded A/A controls show substantial timing variation. x86 throughput is unmeasured.
