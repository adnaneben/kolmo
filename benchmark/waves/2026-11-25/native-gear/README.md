# Gear scanner assembly and archive measurements

An opt-in assembly kernel accelerates the normalized Gear boundary search during DDP1 archive creation. The ARM64 kernel looks up eight Gear entries ahead, computes independent pair increments, and tests for the earliest cut. For consecutive entries A and B and starting hash H, the first hash is 2H + A and the next is 4H + 2A + B modulo 2⁶⁴. The hash passes unchanged across the normal/large mask boundary. The x86-64 SysV candidate uses four-entry lookahead and the same pair algebra. Both use only baseline instructions. The existing C path remains available on all architectures.

## Build and check

Use a fresh build directory via a separate checkout for each setting. The Makefile does not detect changes to compiler flags on existing objects. In that checkout run `make GEAR_ASM=1 build/kolmo check-asm check-features-b`. Stock builds select the original Gear loop. `make NO_ASM=1 GEAR_ASM=1 build/kolmo` selects the portable source and excludes assembly objects. Python and Rust opt in at build time with `KOLMO_GEAR_ASM=1`, and `KOLMO_NO_ASM=1` overrides that selection. The bundled sources match the native tree.

## Results

Apple M5, macOS 27.2, Apple clang 21, Makefile defaults (-O3 -mcpu=native -ffp-contract=off). Compare stock main a1d39559686a9ec17c8614b9154a64d7b5b98672, a second randomized invocation of that same executable (A/A), main with the portable paired-C kernel from #86, and the assembly candidate. Both comparison patches touch only Gear in the compression path. Raw receipts include exact input and archive SHA-256, actual bytes, create/restore wall and CPU time, and macOS peak RSS in bytes. Every run required byte-identical archives across arms within each workload/thread setting and byte-identical restored files. Inputs had mode 0644, timestamp 1600000000, and a single stable member name. Each round shuffled arm order with a fixed seed, after one unscored warmup for each arm. Warm file cache and shared machine.

The table reports throughput in decimal MB/s as original bytes / median paired create-plus-restore time. The gain column instead takes the median of within-round main elapsed time divided by assembly elapsed time. These statistics can differ substantially on a shared host.

| Workload | Threads | Main MB/s | Assembly MB/s | Paired gain over main | Paired gain over #86 |
|---|---:|---:|---:|---:|---:|
| enwik8-16m.gz | 1 | 379.91 | 425.18 | +12.29% | +7.37% |
| enwik8-16m.gz | 4 | 475.34 | 518.76 | +10.59% | +6.11% |
| silesia.tar-16m.gz | 1 | 394.29 | 425.79 | +8.02% | +5.21% |
| silesia.tar-16m.gz | 4 | 469.39 | 518.66 | +13.19% | +6.35% |
| dickens-1m.txt | 1 | 96.05 | 97.18 | -0.01% | +0.01% |
| dickens-1m.txt | 4 | 97.55 | 98.04 | +1.99% | +2.56% |
| openzl-0.3.0.zip | 1 | 529.49 | 577.26 | +9.81% | +5.74% |
| openzl-0.3.0.zip | 4 | 576.79 | 654.68 | +13.35% | +7.00% |
| co2-data-master.zip | 1 | 265.75 | 287.56 | +6.36% | +4.51% |
| co2-data-master.zip | 4 | 279.03 | 302.66 | +8.41% | +5.10% |
| silesia.zip | 1 | 479.05 | 540.07 | +13.18% | +5.60% |
| silesia.zip | 4 | 582.84 | 743.87 | +18.40% | +5.29% |
| dickens | 1 | 202.39 | 219.95 | +6.52% | +3.18% |
| dickens | 4 | 270.61 | 296.35 | +5.89% | +3.53% |

The initial four-byte ARM prototype had worse combined paired medians than the portable paired-C control in every tested cell. Its source and all 192 trials are preserved. The eight-byte implementation produced 384 development and 384 transfer CLI create/restore trials including warmups. It improved every measured large precompressed workload in both thread settings. The 1 MiB Dickens T1 result was effectively flat. On full Silesia ZIP at T4, the paired medians were 18.40% faster than main, 11.99% faster than the second main invocation, and 5.29% faster than paired C; the A/A difference warrants both controls. No decoder change was made. Decode variation in the raw rows captures process, I/O and scheduling effects.

The isolated ARM64 scanner on the two 16 MiB inputs measured 3.66–3.67 GB/s of CPU throughput, versus 2.43–2.46 for paired C and 1.89–1.90 for the single-byte control. See `kernel.c` and `kernel-*.csv` (bytes, round, arm, wall ns, process CPU ns, result checksum). This scanner benchmark uses real bytes in 4 KiB windows and makes no end-to-end claim.

Keep the opt-in for review. Speed evidence covers single-member level-1 archives on one Apple M5. Directory layout, small members, other compilers, higher archive levels, other ARM cores and native x86 timing remain qualification work. No compression-ratio change, generic codec speedup or cross-CPU leadership follows from this experiment.

## Replay

1. Build fresh stock main, that same revision with `pr86-control.patch` applied, and the candidate with `GEAR_ASM=1`. The tested binaries had SHA-256 `b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f` (main), `2b38d7a024ab72cea1847713d95695609e31a0f7b40cb6c747b9b90ddaac6538` (paired C), and `86d7d0ca2c7cb43ff2a62153e858a44ccc7d71e5ae617c493fb9b0813d06eebc` (eight-byte ARM64).
2. Take the first 16 MiB of the Apple gzip 487.40.2 `gzip -n -6 -c` output from enwik8 and silesia.tar, naming them `enwik8-16m.gz` and `silesia.tar-16m.gz`. Use the first 1 MiB of Silesia dickens as `dickens-1m.txt`. The transfer files are complete ZIPs and full Silesia dickens. Check original sizes, hashes and member names in the CSVs.
3. Run `python3 replay.py --main MAIN --paired-c PAIR --asm CANDIDATE --rounds 15 --out fresh.csv --input FILE1 --input FILE2 --input FILE3`. For the four transfer files, use 11 rounds. For the first rejected prototype replace the ARM source with `gear4-rejected.S` in a separate candidate checkout, rebuild, and use seven rounds. Use `python3 score.py fresh.csv` for summaries. All jobs used T1 and T4 at archive level 1.
4. Run `cc -O3 -mcpu=native kernel.c /path/to/src/arch/aarch64/gear_aarch64.S -o kernel` on ARM64, then `./kernel FILE`.

## Platform checks

The optimized and sanitized native assembly differential suites each compared 28,784 cut positions and continuation hashes, across every length 0–256, 16 alignments, seven masks and varying incoming states. Guard-page tail tests explicitly cover reads by uninstrumented assembly. Native feature checks, sanitizer feature checks, the archive CLI suite, frozen v12 readback and a separate NO_ASM fallback build were exercised. Python built the opt-in native binding and completed a round trip; the opt-in Rust binding passed nine unit tests, two integration tests and one doctest.

Both assembly sources assembled as Mach-O and ELF for their respective targets. The x86-64 ELF scanner executed in a QEMU 10.0.12 TCG guest on the M5, covering 28,784 direct comparisons against C with a different Gear table, masks and data. Deliberately changing the paired hash coefficient from four to two produced the expected mismatch marker. `x86-guest-parity.tar.gz` contains the freestanding guest and replay procedure. This verifies x86 instruction semantics on the tested cases. Native x86 timing, native x86 CLI execution and Linux/ARM runtime remain unmeasured.
