# Independent review ledger

Each numbered reviewer owned one implementation or research question. Static findings are engineering hypotheses until the relevant patch or paired test is complete. The acceptance matrix applies to each candidate independently by producer, format, size, level, thread count and architecture.

| Review | Scope and proposed action | Status |
| --- | --- | --- |
| 01 | Admit frame limits before decoder metadata allocations. Charge offsets and sorting. Keep frame checks allocation-free. | #72 |
| 02 | Stage each archive member relative to the held parent FD, verify data and close before commit. | #59 |
| 03 | Bind each release artifact to source and signed provenance. Verify the draft download and uploaded PyPI artifacts. | #60 |
| 04 | Version the format catalogue. Use opt-in encoder learning with a seal, immutable receipts and stock v12 decoding. | #61 prototype |
| 05 | Run a bit-identical opt-in NEON CM4 dot/train kernel. | #63 |
| 06 | Try raw KLZMA at level 9 only after three paired probes inside validated tensor regions. Compare full stored bytes. | #62 opt-in |
| 07 | Record benchmark mode, source, tools and inputs. Refuse stale results and concurrent writes. | #64 |
| 08 | Charge owned stream output windows against codec scratch. Limit tight-cap concurrency and release excess retained capacity. | #72 |
| 09 | Initialize shared Kimg entropy tables before parallel access. | #68 |
| 10 | Bound CSV/TSV output assembly, remove intermediate copy and handle optional allocation failure. | #65 |
| 11 | Make DEC64 conversion deterministic under caller rounding modes. Preserve caller environment. | #66 |
| 12 | Resolve Linux cgroup mount/root and ancestry. Check leaf hard cap and shared parent headroom, preserving v1/v2 distinctions. | Open |
| 13 | Hash and verify trained benchmark artifacts, preserve untrained rows on training failure. | #64 |
| 14 | Honor NO_ASM throughout x86 SIMD/intrinsic dispatch. Verify scalar paths including decimal, graph, word and CRC. | Open |
| 15 | Review Rust wrapper fallible allocation and 32-bit narrowing. | Fixed by R18 |
| 16 | Check QLFC alphabet header writes and reduce serial staging. Verify exact entropy streams. | #81 |
| 17 | Add a versioned strict target-policy ABI and diagnostics. Evaluate arbitrary costs fully unless an audited bound permits pruning. | Open |
| 18 | Make Rust compression reservation fallible, avoid pre-zeroing and validate return lengths. | #67 |
| 19 | Evaluate exact repeated source identifier tokens via existing word dictionary entries. Charge dictionary/header costs. | Research |
| 20 | Reuse the stream parser's CZNS index and block span throughout window decoding. Avoid repeated index traversal. | #79 |
| 21 | Prototype one-time AVX2 dispatch and 32-position scan for x86 branch deltas. Preserve update order and exact frames. | Research |
| 22 | Route validated executable load ranges from sectionless ARM64 ELF to existing BL/ADRP transform. | Research |
| 23 | Separate large Wasm code, data and debug byte ranges under the existing v12 block model. Preserve exact LEB encodings. | Research |
| 24 | Align existing BF16 exponent rANS table resets to selected tensor boundaries. Include ZipNN on the exact same model-family splits. | Research |
| 25 | Add OpenZL 0.3.0 numeric Transformer level 7 and typed producer-disjoint inputs. Keep trained rows distinct. | Benchmark gap |
| 26 | Probe the committed typed layout on each sample against plain/stored, using matched offsets and headers. Measure selection regret. | Research |
| 27 | Trial validated KV-cache page layout, lane-preserving transforms and independent page blocks. Track first-page latency and read amplification. | Research |
| 28 | Skip the hash table for valid tiny KLZ inputs. Evaluate on-demand initialization separately at 14–512 bytes. | #74, ≤13 bytes |
| 29 | Benchmark exact-length fills for long distance-1 matches on run-heavy frames. Keep unrelated controls. | Research |
| 30 | Keep existing FASTQ quality representation and move job cuts to sampled quality-distribution changes at complete read boundaries. | Research |
| 31 | Charge video packed FD input before allocation. Release it before inverse. Account for per-frame inverse arrays by phase. | #83 |
| 32 | Check optional IMG16 detector allocations. Evaluate direct stores and a bounded predictor ring for decode. | Detector fixed in #69, inverse open |
| 33 | Track actual graph live capacity, including retained views and temporary decimal buffers. Enforce intermediate and root bounds. | Open |
| 34 | Bound Huffman short-tail refill to the real quarter stream. Reuse decoded bit-count invariants. | #71 |
| 35 | Switch short graph rANS inputs to a compact bucket map with fallback search. Benchmark before picking crossover. | Research |
| 36 | Reuse classified KLZH2 hashes and replace runtime modulus with exact reciprocal remainder. Check per-thread exactness. | Research |
| 37 | Wake only requested persistent helpers. Test after larger thread counts and concurrent/forked calls. | Research |
| 38 | Count KLZMA synthesized padding separately. Preserve the existing four-byte allowance. | #70 |
| 39 | Use one bounded IMG16 detector scratch buffer and cleanly decline after allocation failure. | #69 |
| 40 | Score bounded CSV grouping keys plus no-group on sampled residuals before formula discovery. Use existing group descriptor. | Research |
| 41 | Try grouping JSONL records by top-level layout, preserving nested values verbatim in existing text columns. | Research, #75 allocation fix |
| 42 | Keep one loaded archive member index and scan selected or listed members in order. Avoid reopen and quadratic traversal. | Open |
| 43 | Close staged regular-path output before publication, require exclusive creation, preserve intended alias/special destination semantics. | #84 |
| 44 | Add independent compressed-member limits and bounded input buffering with spill. Handle CZN1-first pipes in the streaming parser. | Open |
| 45 | Cancel pending parallel compression after output failure, wake waiters and release queued buffers. Preserve first error. | #82 |
| 46 | Make C compression bounds conservative and checked across small blocks, memory-limited and metadata-split routes. Propagate unrepresentable sizes. | Open |
| 47 | Implement and test KLZMA cursor change in native core and bindings. | #70 |
| 48 | Implement and test Huffman quarter-refill change in native core and bindings. | #71 |
| 49 | Rebase #37 candidate selection onto elastic KLZH2. Try one budgeted runner-up on validated strata. | Research |
| 50 | Use existing decimal layouts for header-confirmed rank-1 NumPy arrays, with bounded rows and exact special values. Test Arrow limbs separately. | Research |
| 51 | On archival text, sample alternate existing CM4 context masks for markup and prose. Keep only a fully measured smaller frame. | Research |
| 52 | Shed large repeated-unused TPQ/TLS buffers and measure idle RSS, page faults and tail latency after large-to-small calls. | Research |
| 53 | Limit concurrency under cgroup-local memory pressure, observing hard and soft headroom and preserving one-job progress. | Open |
| 54 | Bind production policy activation to independently approved receipts and active-policy ID. Make previous approved ID an atomic rollback target. | Prototype follow-up |
| 55 | Run installed-binary and installed-wheel v12 comparisons on every supported release architecture and oldest declared runtime. | Release follow-up |
| 56 | Start Windows support with a native UCRT x86-64 memory-only build and fresh installed DLL checks. | Portability research |
| 57 | Start browser support with single-threaded buffer-only wasm32 decode, explicit JS/Wasm memory limits and exact v12 across engines. | Portability research |
| 58 | Keep in-memory rivals and CLI measurements on distinct panels. Record paired CPU/wall and optional power counters. | Benchmark gap |
| 59 | Time first recovered byte and prefix latency with paced producers. Test explicit short first member then ordinary bulk. | Research |
| 60 | Add named allocation fault sites and worker-safe counters. Validate all decoder entry points on valid sealed fixtures at documented caps. | Test coverage gap |
| 61 | Implement regular output atomic publication safeguards after integration build freeze. | #84 |
| 62 | Implement tiny KLZ literal path with output and destination-capacity parity. | #74 |
| 63 | Check optional log/JSON encoder allocations and retain normal-route fallback. | Follow-up |
| 64 | Add opt-in AArch64 branch filtering for validated sectionless ELF and a three-probe screen. | #76 research |
| 65 | Verify integration compatibility and preserve declaration-free codec budget when considering optional sorting. | #79 |
| 66 | Reuse the validated member index across stream windows and charge it once. | #79 |
| 67 | Add archive publication, permission, alias, controlled write failure and cleanup tests. | #77 |
| 68 | Recognize bounded CSV/TSV, JSONL and FASTQ, with binary precedence and catalogue revision 2. | #78 |
| 69 | Encode exact ASCII source identifiers through the existing v12 dictionary with explicit opt-in. | #80 research |
| 70 | Stop new compression work promptly after an output failure and preserve the first error. | #82 |
| 71 | Check QLFC alphabet-header writes in both models. | #81 |
| 72 | Account for C_VID packed input and bounded per-frame inverse metadata by phase. | #83 |

Work gates and the timed scope are in [acceptance.md](acceptance.md). Cursor fixes #70 and #71 were independently verified with 357 frozen frames under ASan/UBSan after the benchmark build froze. Evaluate follow-up code together, on the integrated branch, before merge consideration.