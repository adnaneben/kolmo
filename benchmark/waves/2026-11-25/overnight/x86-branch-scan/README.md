# B21 · SQLite and awk exercise the x86 executable transform

Keep the existing scanner in draft #106 at `8d23095b2f09d5c9e3c3425123bea63641ca5d71`. Both ARM NEON experiments produced identical frames. Neither produced a dependable end-to-end gain. The narrower candidate slowed SQLite decoding at every tested level and thread count against both controls.

| Complete input | Input bytes | Transform on the wire | Frame bytes, level 3 / level 7 |
| --- | ---: | --- | ---: |
| Apple SQLite, x86_64 | 1,612,656 | x86 on 1,313,964 bytes, plain on remainder | 871,011 / 772,497 |
| Apple awk, x86_64 | 130,272 | x86 on all bytes | 51,373 / 46,642 |
| Apple diff, x86_64 | 85,744 | Plain | 38,997 / untested |
| Apple SQLite, arm64e | 1,635,552 | A64 on 1,334,948 bytes, plain on remainder | 821,083 / untested |
| Repository x86.c | 3,291 | Plain | 1,480 / untested |

The source tree's `src/plan/planner.c` calls `e8e9_fwd` while evaluating executable routes. `src/core/blocks.c` calls it on a selected `T_X86` block and records flag 8. `src/core/decode.c` dispatches flag 8 to `e8e9_inv`. The two functions in `src/transforms/x86.c` screen 16 positions at once, in reverse order for encoding and forward order for decoding. The host selects SSE2, ARM NEON or scalar code at compile time. Frame flags establish which transform was emitted. An untransformed block can still have cost the encoder a planner probe. [Inputs and executable hashes](manifest.json) · [Per-block flags and frame hashes](wire.json)

## Measured changes

The [first experiment](screening/build.py) used the first or last set bit of four-bit NEON lane groups in both directions. Direct inverse throughput increased by 9.4% on SQLite and 11.1% on awk versus A. Its forward throughput fell 25.3% on awk and 24.7% on diff. The [narrower candidate](candidate.patch) retained the original forward mask packing and used the first set nibble only on inverse. In its direct benchmark, inverse throughput fell by 54.9% on SQLite, 69.7% on awk, and 74.5% on diff versus A. SQLite and awk lost all nine direct inverse pairs against each copy of the original. The two executables and microbenchmark fingerprints are recorded independently. [First direct results](screening/micro-scores.csv) · [First full CLI](screening/cli-scores.csv) · [Narrower direct results](micro-scores.csv)

Complete SQLite CLI decoding, narrower candidate. MB/s uses the original file size and includes CLI startup and file I/O. Paired changes use the median within-round throughput ratio. A and A′ run the same unmodified executable.

| Level / threads | A MB/s | A′ MB/s | Candidate MB/s | Paired vs A | Paired vs A′ |
| --- | ---: | ---: | ---: | ---: | ---: |
| 3/1 | 294.5 | 276.5 | 266.6 | -7.6% | -4.3% |
| 3/4 | 307.5 | 305.3 | 271.0 | -3.2% | -9.5% |
| 7/1 | 258.1 | 278.5 | 258.1 | -2.8% | -8.1% |
| 7/4 | 286.0 | 284.3 | 269.2 | -5.8% | -10.4% |

Each experiment shuffled the three arms within each round and excluded round zero from timing summaries. CLI scoring used nine measured rounds per setting, per experiment. All 540 complete encode/decode pairs restored the source SHA-256, and Homebrew v0.0.2 independently read the 54 warmup frames across the two experiments. All nine frame hashes matched across every arm, round and experiment. The kernel test compared the transformed bytes and cross-decoded 12,336 deterministic length, alignment and pattern combinations before measuring each file. It also checked every timed transformed and restored buffer. Combined throughput is original bytes divided by summed encode and decode time. [CLI observations](cli.csv) · [CLI phase throughput, paired results and A/A drift](cli-scores.csv) · [Kernel observations](micro.csv)

Native timing was on arm64 macOS, Darwin 27.2.0, using Apple clang 21.0.0 with `-O3 -mcpu=native -ffp-contract=off`. The x86_64 Mach-O slices were complete inputs extracted from shipped universal files by lipo. The host lacks an x86_64 execution runtime. Cross-compiling the narrower candidate's forward and inverse wrappers for SSE2 produced byte-identical assembly to the original. Execution speed on physical x86, ELF and PE files, large x86-only corpora, other compilers, and alternate scalar implementations remain unmeasured. The observations are specific to these builds and inputs. [SSE2 code generation](x86-codegen.json)

## Reproduction

Build the unmodified source with `make build/kolmo` and set the environment variables below to its absolute path, an empty work directory, the released CLI, and a fresh output directory. The scripts verify hashes, frames and restores. The extracted inputs vary with macOS versions. Compare their hashes against the manifest. Substitute `screening/build.py` for the first experiment and use separate work and output directories. The SSE2 equality check applies to the narrower candidate.

```sh
python3 build.py "$CHECKOUT" "$WORK"
python3 replay.py --source "$CHECKOUT" --build "$WORK" --reader "$READER" --out "$OUT" --phase micro
python3 replay.py --source "$CHECKOUT" --build "$WORK" --reader "$READER" --out "$OUT" --phase cli
python3 score.py "$OUT"
python3 check-sse2.py --source "$CHECKOUT" --build "$WORK" --out "$OUT"
```
