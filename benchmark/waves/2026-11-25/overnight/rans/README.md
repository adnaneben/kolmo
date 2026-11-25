# B35 — two-symbol graph rANS on short valid streams

**Decision: preserve both prototypes for research; no default or release inclusion.** The direct fast path removes repeated 64 KiB slot-table construction for short chunks that contain exactly two symbols. The natural workload scan found **zero eligible chunks**. The first integrated implementation also slowed noneligible direct controls. The second implementation isolated the short test and substantially reduced that penalty, but the repeats do not establish a stable advantage at the 4 KiB boundary.

## Scope and implementation

Source is `a1d39559686a9ec17c8614b9154a64d7b5b98672`, native Apple ARM64 at `-O3 -mcpu=native -ffp-contract=off`. B35 concerns the **16-bit graph-leaf rANS** decoder in `src/codecs/rans.c`, codec 14 or a plane-aligned graph substream. KLZH2 calls the separate 12-bit 4K-slot `src/algo/krans.c`, which these patches do not touch. The graph decoder already handles a one-symbol chunk as a fill. No new one-symbol format, encoder, external dictionary or assembly is required.

Both research builds decode an ordinary published-format two-symbol graph chunk by comparing each state’s slot with the lower symbol’s cumulative boundary; the frequency, offset, end-of-stream consumption and terminal states remain checked. The threshold tested was at most 4,096 decoded bytes. The first patch shares frequency parsing and changes the table allocation order. The second patch adds a separate short-chunk routine with a boundary check and falls back to the original general decoder. An unlimited microbenchmark-only variant removes the cutoff to reveal behavior beyond the proposed gate. The prototype patches include native and vendored sources on isolated main checkouts. They were not landed. Only valid inputs were used.

## Direct measurements and confidence

The kernel runner compiled the repository’s exact graph rANS, varint reader, and histogram code as separate native libraries. Generated workloads contained one symbol, two balanced symbols, two skewed nonadjacent symbols, three symbols, and random bytes. All 90 valid streams from 1 to 262,144 bytes produced identical serialized bytes and exact decode in stock, bounded and unbounded variants. Nine additional valid multichunk combinations straddling the 4 KiB gate, the 1 MiB graph chunk boundary, and four-state tails also decoded exactly. Encode and decode are timed separately in-process; the full input and compressed output hashes are checked outside timers. Seven seeded, shuffled paired batches per cell include two separately timed stock-control arms plus bounded/unbounded candidate arms. A second whole run of the isolated experiment is retained, including a reversal near the upper boundary.

The table shows the **range across two independent measured sessions, two symbol distributions** for each input size. Ratios use the paired mean of the two stock controls divided by the bounded candidate time. Actual per-phase nanoseconds, stored bytes and same-binary variability are available for every cell in the two summary CSVs. Combined values sum native encode and decode costs per pair.

| Raw bytes | Direct decode speedup across sessions/distributions | Direct combined speedup |
|---:|---:|---:|
| 32 | 20.75–29.05× | 3.11–3.53× |
| 256 | 3.30–7.17× | 2.23–2.55× |
| 1,024 | 1.28–2.94× | 1.19–1.67× |
| 4,096 | 0.95–1.70× | 0.99–1.29× |

For three-symbol and random controls above the 4 KiB gate, the first prototype returned **0.71–0.91×** stock direct decode speed, despite its ineligible branch. The isolated version reached **0.96–1.02×** across the two sessions. These are scheduling- and compiler-sensitive native measurements. The repeated 4 KiB borderline result precludes admitting a 4 KiB guarantee. A future targeted implementation should start at 1 KiB or less and require an actual consumer with frequent eligible calls before spending effort on a larger table scheme.

## Normal CLI coverage

Normal `kolmo -d` decoded **357 frozen manifest entries** (309 distinct frame hashes, 56 source hashes) using an observation-only build, and matched every original. Only 12 frames used graph rANS. They had 6,212–49,152 decoded bytes and 21–58 distinct symbols per chunk. A further **15 complete newly encoded T1 frames** crossed levels 3/6/9 and five inputs: 256 KiB SmolLM2 BF16, MR image, and Reymont slices, plus complete `f64.npy` and `bf16.st` fixtures. One frame chose graph rANS, with a 12,288-byte, 40-symbol chunk. **Zero eligible short two-symbol chunks were seen in either group.** The 15 fresh inputs/settings were re-timed at the normal command boundary using the uninstrumented candidate and two baseline-control arms, five repeats per arm (225 matched trials per prototype). Archive bytes matched stock and restored bytes matched the input every time. The per-phase and paired combined values in `cli-summary.csv` and `isolated/cli-summary.csv` are controls only; variation cannot be attributed to an eligible two-symbol call. These files cannot establish how frequently a separate production deployment sees this distribution.

## Reproduction

1. Checkout the pinned main in independent directories. Apply `first-prototype.patch` to one and `isolated-prototype.patch` to another. Build all regular CLI binaries with `make build/kolmo`.
2. Run `python3 micro.py MAIN_SOURCE PATCHED_SOURCE OUT_DIR`. `./` contains the first variant, `isolated/` the second variant, and `verify-compound/` the repeated isolated screen and multi-chunk checks. The published runner includes additional compound parity checks added after the first direct screen; all timed workload generators and seeds remain the same.
3. For the trace-only binary, apply the first patch and insert `fprintf(stderr, "RANS-OBS %zu %d\\n", (size_t)m, used);` immediately before the single-symbol case. Use an actual newline escape in the C string, as in `trace-observation.diff`. This observer is used only for counting alphabet size on valid frames. Time the ordinary, uninstrumented candidate and stock binaries.
4. Run `python3 inspect_cli.py BASE_CLI PATCHED_CLI TRACE_CLI V12_FIXTURES IMPACT_HOLDOUT OUT_DIR`. Then run `python3 summarize.py OUT_DIR`. The CLI receipts contain all input/frame hashes and phases, and the binary manifest identifies the exact executables.
