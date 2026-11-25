# Raw KLZ output capacity

Base a1d39559686a9ec17c8614b9154a64d7b5b98672. Native, Python and Rust bundled source are in sync. The emitter accounts for the complete token when given a tight buffer. Ample destinations use an input-based worst-case bound. When a selected parse cannot fit, an admissible literal-only frame can still satisfy the published capacity contract. Existing direct-API tests now verify that recompression into an exact-size destination preserves valid output. This repair does not change the native decoder or frame syntax.

On Darwin ARM64, Apple clang 21.0.0, the native codec suite, ASan+UBSan codec suite and 357 frozen v12 decoder fixtures passed. The final CLI SHA-256 is ca0a34b5e5fa1b9634d1afd0bc0795be7e80f45bcd655dbaeb04c4553c6b5b0c. Baseline CLI SHA-256 b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f.

Final source df3650d8287a56f34d3485125a113217160d7846. File-to-file -1, same Apple ARM64, two copies of the same baseline binary labeled A/A2 plus candidate B, randomized order with one warmup then 15 scored triplets, at T1/T4. All 384 arms produced identical archive bytes and exact readback by the stock decoder. Full original source-file hashes and individual encode/decode wall, CPU, macOS RSS and byte counts are in cap-block.jsonl. Two earlier versions' 384 arms are retained separately. This is a correctness fix. Timing is shared-host screening, and the other review jobs continued in the background.

| Input | Threads | Main combined MB/s | Fixed combined MB/s | Paired change vs main controls | Paired CPU change |
|---|---:|---:|---:|---:|---:|
| energy | 1 | 213.5 | 215.6 | -2.97% | +0.72% |
| energy | 4 | 318.0 | 309.5 | -1.45% | +1.98% |
| ghcn | 1 | 203.1 | 196.0 | -6.31% | +2.24% |
| ghcn | 4 | 159.0 | 164.4 | -2.90% | +2.22% |
| prose | 1 | 178.6 | 180.0 | +3.99% | -0.19% |
| prose | 4 | 326.6 | 291.6 | -8.08% | +1.12% |
| backblaze | 1 | 678.8 | 687.9 | -1.21% | +7.23% |
| backblaze | 4 | 1859.1 | 1761.7 | -0.75% | +4.24% |

Decision: retain this fix separately for review. Do not claim a throughput improvement. Replay with replay.py CORPUS CONTROL FIX OUTPUT --files energy ghcn prose backblaze --sizes whole --rounds 15 --levels 1 --threads 1 4 and summarize.py SUMMARY OUTPUT. The two early variants used seven scored triplets. No hosted x86 or release checks are claimed by this branch.
