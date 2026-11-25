# Paired performance results

Apple M5, macOS 27.2, Apple Clang 21.0.0, native -O3 and FP contraction disabled. Each run used real CLI file-to-file encoding followed by ordinary v12 decoding and a SHA-256 comparison. The record includes process launch and file I/O. Each preset and file has three randomized pairs. Small deltas are directional on this shared host. There is no p95 claim from this panel.

For each preset, aggregate decimal MB/s uses the total original bytes divided by the sum of per-file median phase times. Combined uses each trial’s paired encode and decode time before computing per-file medians. Ratio is original divided by stored bytes. RSS is the largest observed phase RSS over the eight files and repetitions. Wave 1 appears in both independent sessions so each before/after comparison uses the same session.

Core panel inputs, exactly as preserved in the previous audit. text__reymont, csv__owid-co2, log__bgl, weights__smollm2-bf16, code__jxl-cc, img16__mr, exe__wasmtime-arm64, audio__esc50-3-100018.

## Pinned main to Wave 1

540 verified round trips. 90/90 paired input/level/thread settings produced byte-identical frames.

| Level | Stored bytes | Ratio | Encode MB/s | Decode MB/s | Combined MB/s | Combined ms | Peak RSS MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 14,432,748 | 2.06074× | 459.11 → 474.37 | 879.31 → 927.15 | 295.79 → 308.10 | 100.55 → 96.54 | 12.41 → 12.41 |
| 2 | 10,655,446 | 2.79127× | 325.40 → 324.16 | 618.43 → 662.06 | 208.75 → 216.06 | 142.47 → 137.66 | 12.89 → 12.89 |
| 3 | 8,215,607 | 3.62021× | 175.35 → 174.26 | 616.88 → 625.25 | 136.39 → 136.49 | 218.07 → 217.91 | 25.80 → 25.75 |
| 4 | 7,750,680 | 3.83736× | 49.53 → 52.47 | 591.55 → 639.17 | 45.57 → 48.48 | 652.74 → 613.49 | 72.86 → 72.14 |
| 5 | 7,465,616 | 3.98389× | 14.38 → 14.83 | 549.78 → 563.45 | 14.01 → 14.48 | 2,123.59 → 2,053.70 | 89.61 → 89.61 |
| 6 | 7,264,752 | 4.09404× | 9.92 → 10.03 | 596.78 → 618.89 | 9.76 → 9.87 | 3,046.12 → 3,012.22 | 93.70 → 92.67 |
| 7 | 7,260,731 | 4.09631× | 5.65 → 5.76 | 577.16 → 573.03 | 5.59 → 5.70 | 5,320.57 → 5,221.63 | 93.31 → 91.44 |
| 8 | 6,611,546 | 4.49852× | 15.91 → 15.97 | 43.67 → 45.17 | 11.62 → 11.81 | 2,558.62 → 2,518.36 | 71.64 → 71.72 |
| 9 | 6,565,390 | 4.53015× | 8.19 → 8.38 | 48.00 → 47.94 | 7.00 → 7.11 | 4,250.94 → 4,183.53 | 106.52 → 106.50 |
| ultra | 6,158,611 | 4.82937× | 0.99 → 0.96 | 1.31 → 1.32 | 0.56 → 0.56 | 53,011.33 → 53,136.07 | 324.47 → 320.34 |

### Full-size and specialist inputs

| Input | Level | Threads | Stored bytes | Combined MB/s | Encode CPU s | Decode CPU s | Peak RSS MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SRR2584863.fastq | 3 | 10 | 12,597,365 | 370.66 → 377.76 | 0.43 → 0.43 | 0.05 → 0.05 | 168.55 → 168.59 |
| SRR2584863.fastq | ultra | 10 | 5,916,146 | 11.11 → 11.11 | 5.41 → 5.44 | 6.50 → 6.51 | 813.70 → 814.58 |
| esc50-2-100648-A-43.wav | 5 | 1 | 267,467 | 7.57 → 7.96 | 0.05 → 0.05 | 0.01 → 0.00 | 15.97 → 15.97 |
| esc50-2-100648-A-43.wav | 9 | 1 | 260,497 | 2.31 → 2.33 | 0.16 → 0.16 | 0.03 → 0.03 | 16.77 → 16.73 |
| llm_bf16_47mb.safetensors | 3 | 1 | 30,012,302 | 256.89 → 261.13 | 0.15 → 0.15 | 0.03 → 0.03 | 57.23 → 57.44 |
| llm_bf16_47mb.safetensors | 9 | 1 | 27,186,418 | 2.66 → 2.69 | 16.83 → 16.69 | 0.76 → 0.77 | 565.66 → 565.23 |
| llm_bf16_47mb.safetensors | 9 | 10 | 27,186,418 | 8.98 → 8.95 | 24.60 → 24.63 | 0.78 → 0.77 | 569.52 → 569.59 |
| silesia.tar | 1 | 1 | 87,130,630 | 622.14 → 583.80 | 0.27 → 0.28 | 0.07 → 0.07 | 14.73 → 15.16 |
| silesia.tar | 3 | 1 | 57,551,847 | 228.50 → 226.24 | 0.78 → 0.78 | 0.15 → 0.15 | 59.91 → 59.09 |
| silesia.tar | 9 | 1 | 40,507,982 | 7.99 → 7.80 | 23.79 → 24.22 | 2.57 → 2.63 | 1,279.00 → 1,278.61 |

## Wave 1 to Wave 2

540 verified round trips. 90/90 paired input/level/thread settings produced byte-identical frames.

| Level | Stored bytes | Ratio | Encode MB/s | Decode MB/s | Combined MB/s | Combined ms | Peak RSS MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 14,432,748 | 2.06074× | 527.30 → 523.17 | 1,052.38 → 927.21 | 350.67 → 334.66 | 84.81 → 88.87 | 13.00 → 12.41 |
| 2 | 10,655,446 | 2.79127× | 368.92 → 369.18 | 777.32 → 776.35 | 250.50 → 249.71 | 118.73 → 119.11 | 12.89 → 12.88 |
| 3 | 8,215,607 | 3.62021× | 187.80 → 188.23 | 684.39 → 693.23 | 147.41 → 147.91 | 201.77 → 201.08 | 25.75 → 23.30 |
| 4 | 7,750,680 | 3.83736× | 54.44 → 54.58 | 659.70 → 654.57 | 50.27 → 50.31 | 591.66 → 591.18 | 70.77 → 70.72 |
| 5 | 7,465,616 | 3.98389× | 16.56 → 16.59 | 640.98 → 645.13 | 16.14 → 16.16 | 1,842.43 → 1,839.92 | 88.25 → 88.20 |
| 6 | 7,264,752 | 4.09404× | 12.43 → 12.69 | 652.55 → 657.55 | 12.20 → 12.45 | 2,438.86 → 2,388.99 | 90.30 → 90.25 |
| 7 | 7,260,731 | 4.09631× | 6.84 → 6.84 | 647.76 → 647.19 | 6.77 → 6.77 | 4,396.44 → 4,393.97 | 90.56 → 90.53 |
| 8 | 6,611,546 | 4.49852× | 16.39 → 16.33 | 46.60 → 46.72 | 12.10 → 12.10 | 2,457.26 → 2,458.35 | 72.52 → 71.12 |
| 9 | 6,565,390 | 4.53015× | 8.88 → 8.84 | 50.42 → 51.25 | 7.56 → 7.49 | 3,933.76 → 3,969.49 | 106.50 → 106.67 |
| ultra | 6,158,611 | 4.82937× | 1.12 → 1.12 | 1.44 → 1.44 | 0.63 → 0.63 | 47,076.39 → 47,123.65 | 318.94 → 314.52 |

### Full-size and specialist inputs

| Input | Level | Threads | Stored bytes | Combined MB/s | Encode CPU s | Decode CPU s | Peak RSS MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SRR2584863.fastq | 3 | 10 | 12,597,365 | 332.34 → 294.59 | 0.45 → 0.51 | 0.05 → 0.06 | 168.39 → 168.48 |
| SRR2584863.fastq | ultra | 10 | 5,916,146 | 11.51 → 11.22 | 5.36 → 5.26 | 6.21 → 6.45 | 814.31 → 815.34 |
| esc50-2-100648-A-43.wav | 5 | 1 | 267,467 | 8.31 → 8.28 | 0.05 → 0.05 | 0.00 → 0.00 | 15.95 → 15.70 |
| esc50-2-100648-A-43.wav | 9 | 1 | 260,497 | 2.34 → 2.35 | 0.16 → 0.16 | 0.03 → 0.03 | 19.12 → 16.78 |
| llm_bf16_47mb.safetensors | 3 | 1 | 30,012,302 | 283.85 → 275.27 | 0.13 → 0.13 | 0.03 → 0.03 | 56.78 → 55.33 |
| llm_bf16_47mb.safetensors | 9 | 1 | 27,186,418 | 2.95 → 2.86 | 15.17 → 15.67 | 0.76 → 0.76 | 565.38 → 563.08 |
| llm_bf16_47mb.safetensors | 9 | 10 | 27,186,418 | 9.31 → 7.66 | 23.46 → 24.76 | 0.75 → 0.77 | 572.48 → 568.44 |
| silesia.tar | 1 | 1 | 87,130,630 | 585.62 → 609.85 | 0.27 → 0.27 | 0.07 → 0.07 | 14.73 → 14.72 |
| silesia.tar | 3 | 1 | 57,551,847 | 228.66 → 242.28 | 0.78 → 0.72 | 0.14 → 0.14 | 59.08 → 57.47 |
| silesia.tar | 9 | 1 | 40,507,982 | 8.49 → 8.39 | 22.32 → 22.25 | 2.57 → 2.61 | 1,278.73 → 1,279.78 |

## Level-1 recheck

The broad Wave 2 pass had a visible level-1 slowdown concentrated in BGL. A focused retest ran 15 pairs on BGL and two controls. This remains a shared-host measurement.

| Input | Encode ms | Decode ms | Combined ms | Combined time change |
| --- | --- | --- | --- | --- |
| code__jxl-cc | 8.165 → 8.216 | 3.806 → 3.780 | 12.006 → 11.969 | -0.31% |
| log__bgl | 5.133 → 5.096 | 3.387 → 3.344 | 8.514 → 8.441 | -0.86% |
| text__reymont | 10.766 → 10.675 | 4.307 → 4.268 | 15.065 → 15.023 | -0.28% |

## Opt-in level-9 weight retry

Default Wave 2 and its explicit tensor override used the same native encoder. Every completed frame was decoded with the v0.0.2 release build. The known BF16 results are an engineering confirmation of the original regression. Matching family files are one producer group.

| Input | Threads | Stored bytes | Size change | Encode MB/s | Decode MB/s | Combined MB/s | Encode CPU s | Peak RSS MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| llm_bf16_10mb.safetensors | 1 | 6,965,938 | +0.00% | 3.51 → 3.60 | 509.29 → 501.30 | 3.48 → 3.57 | 2.99 → 2.91 | 230.69 → 230.69 |
| llm_bf16_47mb.safetensors | 1 | 27,186,418 → 26,168,919 | -3.74% | 2.34 → 1.26 | 59.38 → 64.10 | 2.25 → 1.24 | 19.99 → 37.07 | 565.41 → 589.89 |
| magika.safetensors | 1 | 2,608,222 | +0.00% | 4.77 → 4.67 | 551.53 → 530.01 | 4.73 → 4.63 | 0.66 → 0.67 | 84.08 → 86.66 |

## ARM64 mixer intrinsics

Direct CM4 kernel harness. Each case used one 256 KiB prefix, forced scalar or NEON dispatch, and seven paired trials after one warmup. Both paths emitted exactly the same bytes and decoded one shared scalar reference. This measures direct CM4 execution, including the harness process and I/O.

| Input | Stored bytes | Encode ms | Decode ms | Combined MB/s |
| --- | --- | --- | --- | --- |
| csv__owid-co2 | 39662 | 196.10 → 168.29 | 219.68 → 184.45 | 0.63 → 0.74 |
| log__bgl | 4673 | 204.87 → 181.33 | 226.66 → 207.93 | 0.61 → 0.67 |
| text__reymont | 41927 | 209.40 → 180.98 | 224.72 → 204.61 | 0.60 → 0.68 |

## Stream-index reuse follow-up

The archive-tested Wave 2 source and the #79 follow-up decoded the same valid 32 MiB CZNS members. The source was encoded once using level 1, one thread, analysis disabled, a 4 KiB block target and a 64 MiB stream segment. Compare seven randomized pairs after one warmup on each input, cap and arm. Decode wall includes file I/O and process startup. The decoder memory option does not include the compressed member buffer, as reflected in process RSS.

| Input | Memory option | Frame bytes | Decode wall ms | Decode speedup | Decode CPU ms | Peak RSS MiB |
| --- | --- | --- | --- | --- | --- | --- |
| Silesia prefix | 2M | 22,800,394 | 268.207 → 15.813 | 16.96× | 262.647 → 14.955 | 24.28 → 24.83 |
| Silesia prefix | max | 22,800,394 | 18.088 → 17.877 | 1.01× | 17.034 → 16.820 | 56.12 → 55.98 |
| enwik8 prefix | 2M | 24,991,960 | 266.381 → 17.938 | 14.85× | 264.879 → 17.048 | 26.36 → 27.05 |
| enwik8 prefix | max | 24,991,960 | 19.405 → 19.428 | 1.00× | 18.298 → 18.291 | 58.20 → 58.05 |

## ZipNN BF16 size reference

ZipNN 0.5.4 ran in-process with one thread. The runner compressed the BF16 payload with its stock dtype byte grouping and preserved the original SafeTensors header verbatim as counted side data. Each of seven warm trials restored the exact entire file. The timer starts at the input copy. Process launch, input/output file I/O and native CLI framing are different from the Kolmo replay, so compare size directly and keep these timing boundaries distinct.

| Input | ZipNN including header | Kolmo opt-in level-9 bytes | ZipNN warm encode + decode ms |
| --- | --- | --- | --- |
| llm_bf16_10mb.safetensors | 6,990,356 | 6,965,938 | 10.983 |
| llm_bf16_47mb.safetensors | 31,334,467 | 26,168,919 | 58.294 |

## Decisions and verification

- The frozen benchmark integrations have identical output across all 90 paired input/level/thread settings in each wave. Wave 1 passed `make check`. Wave 2 passed `make check check-kernels check-cm4 check-dec64`, including 357 frozen C frames, archive CLI tests, stream/FD parity and existing native sanitizer checks. Race/codec-tail, QLFC, regular publication, and later decoder changes also have scoped validation in their drafts.
- Keep the BF16 retry opt-in. It recovers 1,017,499 bytes on the known 47 MB file, but combined latency exceeds the 1.65× archive screening ceiling. The other two files gained no stored bytes. Profile a cheaper prediction or an alternative initial codec choice before repeating with new model families.
- Keep the NEON option available on ARM64, with identical frames and higher paired combined throughput on all three direct harness inputs. Confirm full CLI codec 7 and end-to-end results on newer and older ARM CPUs before default activation.
- Proceed to review the stream-index follow-up. Two separate densely blocked 32 MiB inputs improved under the same 2 MiB policy. Unlimited-memory control throughput was essentially flat.
- The code-identifier and sectionless ARM64 ELF candidates use ordinary v12 decoders and are opt-in. Synthetic size wins justify sealed real-code follow-up. They have no asserted speed gain.
- Atomic extraction stages and commits individual members. Release attestation logic passed mocked verification, without running a live release. The decoder cap is an admission and scheduling budget. Compressed-member input and all graph temporary allocations still require explicit accounting.
- The previous same-host in-memory Silesia lzbench used another timing boundary. Kolmo level 1 wrote 87,130,630 bytes at 668.31 MB/s encode and 3,652.15 MB/s decode. LZAV 7.1 at level 1 wrote 83,147,688 bytes at 660.55 and 3,597.97 MB/s. Keep these as previous benchmark anchors.
- Previous enwik8 Ultra wrote 18,651,472 B. Published PAQ8PX and cmix references wrote 15,849,084 B and 14,623,723 B respectively, measured in their own environments. Next-round work requires same-host timing, same input and the acceptance matrix at equal size or time. Keep trained OpenZL results separate from training-free Transformer/profile trials.
