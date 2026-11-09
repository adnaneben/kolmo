# Kolmo assessment and improvement plan

Kolmo has a credible engineering direction: discover structure automatically, encode exact residuals, and decode independent blocks in parallel. It is currently a hybrid compressor whose principal backends are existing codecs. The measurements and source review do not establish a world leading general purpose compressor. A defensible goal is to advance the frontier of compression ratio, compression speed, decompression speed and memory on a declared, diverse workload.

Assessment date: 7 November 2026. Source revision and dataset hashes are recorded in [manifest.json](manifest.json). Reproduction commands and measurement limits are in [COMMANDS.md](COMMANDS.md). Raw measurements accompany this report; existing README numbers are historical claims, rather than measurements from this audit.

Implementation update: [concurrency fixes and parallel archive planning](milestone1/IMPLEMENTATION.md) now have separate validation and before/after results. The measurements and findings below remain the original baseline.

## Measured results

The primary single-thread experiment uses the original shared Kolmo library and lzbench 2.4.1 at the pinned revision. Each setting has at least three compression and decompression iterations, summarized with the median. All 130 measured codec/file combinations completed without a reported mismatch. These are local measurements, with the test suite paused and compilation finished during timing; other desktop activity and CPU clocks were not controlled.

### Complete Silesia corpus

| Codec | Files | Ratio | Compress MB/s | Decompress MB/s |
|---|---:|---:|---:|---:|
| kolmo audit -0 | 12 | 2.146 | 885.7 | 4886.8 |
| kolmo audit -1 | 12 | 3.623 | 257.4 | 1768.2 |
| kolmo audit -2 | 12 | 4.174 | 34.9 | 738.3 |
| kolmo audit -3 | 12 | 4.427 | 8.4 | 1601.6 |
| lz4 1.10.0 | 12 | 2.101 | 1024.5 | 6026.1 |
| zstd 1.5.7 -1 | 12 | 2.894 | 840.8 | 2295.8 |
| zstd 1.5.7 -3 | 12 | 3.205 | 534.2 | 2162.3 |
| zstd 1.5.7 -9 | 12 | 3.587 | 127.8 | 2425.8 |
| zstd 1.5.7 -19 | 12 | 4.007 | 6.0 | 2205.4 |
| brotli 1.2.0 -8 | 12 | 3.711 | 40.1 | 788.2 |

Kolmo fast stores 11.5 percent fewer bytes than Zstandard level 3 on this corpus, but takes approximately 2.1 times as long to compress and 1.2 times as long to decompress. Kolmo hc stores 9.5 percent fewer bytes than Zstandard level 19 and compresses faster, but decodes more slowly. The rt mode is slightly smaller than LZ4 in aggregate and slower in both directions. These results support useful tradeoffs, not dominance across every metric.

The aggregate hides large differences between files. On XML, fast produces 652,714 bytes versus Zstandard level 3's 635,299; on Mozilla it produces 18,318,543 versus 18,227,296. Report each file alongside the aggregate and do not infer general superiority from the strongest table result.

### OWID CO2 table

| Codec | Ratio | Compress MB/s | Decompress MB/s |
|---|---:|---:|---:|
| Kolmo rt | 8.132 | 364.41 | 1054.60 |
| Kolmo fast | 10.693 | 340.85 | 914.43 |
| Kolmo eco | 11.868 | 99.28 | 646.67 |
| Kolmo hc | 12.013 | 35.18 | 980.68 |
| lz4 1.10.0 | 1.947 | 750.51 | 4406.81 |
| zstd 1.5.7 -1 | 3.496 | 759.47 | 2057.38 |
| zstd 1.5.7 -3 | 3.169 | 381.32 | 1731.86 |
| zstd 1.5.7 -9 | 3.565 | 87.11 | 1920.21 |
| zstd 1.5.7 -19 | 4.176 | 3.97 | 1762.91 |
| brotli 1.2.0 -8 | 3.553 | 32.53 | 694.17 |

This 14,377,942-byte table is a strong fit for Kolmo: fast produces approximately 70 percent fewer bytes than Zstandard level 3 while retaining about 89 percent of its compression throughput. It decodes at approximately 53 percent of that baseline's one-thread speed. This is the most convincing measured fast-mode advantage, but OWID already appears in the project's tuning and published results, so it is not an unseen generalization test.

### Archive settings on complete Silesia

The slower settings use a minimum of one compression and decompression iteration per file. These speeds are exploratory; their compressed sizes and successful verification are the stronger evidence. All 91 codec/file combinations, including OWID, completed without a reported mismatch.

| Codec | Files | Ratio | Compress MB/s | Decompress MB/s |
|---|---:|---:|---:|---:|
| kolmo audit -4 | 12 | 5.025 | 10.0 | 130.5 |
| kolmo audit -5 | 12 | 5.535 | 1.2 | 1.6 |
| brotli 1.2.0 -11 | 12 | 4.211 | 1.1 | 670.5 |
| lzma 26.03 -6 | 12 | 4.273 | 6.2 | 168.8 |
| ppmd8 26.03 -6 | 12 | 4.296 | 23.1 | 21.2 |
| bsc 3.3.12 -m0 -e1 | 12 | 4.501 | 52.9 | 52.9 |
| openzl 0.2.3 'generic' | 12 | 3.461 | 187.5 | 2031.9 |

Ultra has the highest ratio among these tested settings, with 18.7 percent fewer bytes than the selected libbsc configuration, but approximately 44 times slower compression and 33 times slower aggregate decoding. Max provides a useful middle ground at 5.03 times compression and approximately 130 MB/s decoding. Those decoder costs matter for retrieval and distribution workloads even when cold archival storage prioritizes size.

On OWID, max and ultra both produce exactly 1,131,224 bytes. Ultra compresses at 1.46 MB/s versus max's 7.17 MB/s: approximately five times more work without improving size. A bounded codec-selection experiment should target this kind of wasted effort.

The tested OpenZL is lzbench's bundled 0.2.3 generic preset, not current 0.3.0 or a trained table profile. The LZMA result is level 6 from the 7-Zip SDK, not a sweep of every 7-Zip setting. No measurements of ZPAQ, current OpenZL structured profiles, GPU kernels, or an independent Kolmo entropy backend are included. Consequently these results do not establish a win over every leading compressor or a public leaderboard record.

### Equal block parallel comparison

The scaling experiment uses approximately 1 MiB Kolmo blocks with internal workers and exact 1 MiB competitor blocks with lzbench's external pool. Kolmo may shift boundaries to preserve rows, and its output includes a container; the baseline reports raw block payloads. This experiment measures a comparable parallel strategy rather than byte-identical block boundaries. Medians use three iterations per setting. Baseline compressed sizes remain identical across thread counts.

| Input and codec | Threads | Compress MB/s | Decompress MB/s |
|---|---:|---:|---:|
| Mozilla Kolmo fast | 1 | 200.77 | 1863.11 |
| Mozilla Kolmo fast | 4 | 269.66 | 7001.22 |
| Mozilla Kolmo fast | 10 | 276.09 | 13073.68 |
| Mozilla Zstandard level 3 | 1 | 601.13 | 1911.92 |
| Mozilla Zstandard level 3 | 10 | 3510.24 | 13109.51 |
| OWID Kolmo fast | 1 | 356.93 | 910.32 |
| OWID Kolmo fast | 4 | 971.41 | 3145.76 |
| OWID Kolmo fast | 10 | 1473.62 | 5817.01 |
| OWID Zstandard level 3 | 1 | 400.81 | 1727.73 |
| OWID Zstandard level 3 | 10 | 2389.92 | 11158.67 |

Kolmo's Mozilla compression improves only 1.38 times from one to ten threads, while its decode improves 7.02 times. This points to serial planning or other overhead outside the parallel compression workers as a profiling priority; the benchmark alone does not identify the exact function. Giving competitors parallel blocks removes the premise that only Kolmo can benefit from more decoding cores. OWID still has the much smaller Kolmo representation at each thread count.

## Hardware and execution

This machine has an Apple M5, ten physical CPU cores consisting of four performance and six efficiency cores, ten logical CPU threads, ten GPU cores, and 24 GiB of unified memory. CPU and GPU cores are different execution resources. Kolmo currently contains CPU implementations, including ARM NEON paths and hardware CRC instructions; there is no Metal or CUDA compression backend.

The default worker count comes from `sysconf(_SC_NPROCESSORS_ONLN)`. The persistent pool uses the caller plus helpers, caps workers at 256, and limits active workers to the number of blocks. Formula search has a separate sixteen-worker cap. Speed modes target approximately eight blocks on intermediate inputs, with a 1 MiB floor and 4 MiB ceiling. This can leave CPU cores idle. Archive modes use blocks up to 128 MiB, and consequently can have only one block on common benchmark files. These are ratio versus throughput choices, not evidence that changing the worker count alone will make every mode scale.

Context mixing can allocate about 3.8 GiB of hash tables per large block, before input, output and other scratch space. Its admission control budgets 60 percent of physical memory; it does not account for every allocation or current memory pressure. With 24 GiB, three such workers fit that nominal budget, whereas four do not. A request for ten workers therefore cannot imply ten active context mixers.

## Build tests and memory

A forced native rebuild with `make -B -j10` succeeded using Apple clang 21 and native CPU optimization; the original shared library also built successfully. The complete native `czntest` suite printed `ALL PASSED`. Python binding tests passed every mode on empty, tiny, random and synthetic table inputs. A seven-segment OWID streaming round trip using 2 MiB segments and ten workers matched the source byte for byte.

AddressSanitizer and UBSan checks completed on 20,000 random decoder inputs, one valid README frame and 500 mutated versions of that frame, plus 300 small randomized round trips. No sanitizer diagnostics appeared. These bounded checks instrumented the Kolmo core; vendored libraries were linked from the ordinary build, and leak detection was disabled. They do not replace coverage-guided fuzzing or a full sanitizer build of every dependency.

A separate one-thread CLI memory probe on the 10,192,446-byte Dickens file reported a peak resident process size of 21,626,880 bytes for fast, approximately 20.6 MiB, and 4,602,789,888 bytes for ultra, approximately 4.29 GiB. These are whole-process values including input, output, transforms and backends, measured using macOS child-process resource usage. The probes overlapped correctness work, so their elapsed times are not used as throughput results. Full commands and raw test output are included in this directory.

The concurrency reproducer exposes a compatibility defect despite all these successful round trips. Passing the existing suite therefore establishes a useful baseline without resolving the source-review findings below.

## What the implementation contributes

| Component | Source | Assessment |
|---|---|---|
| Typed tables and relationship discovery | `src/tbl2.inc` | Automatically discovers grouped predictors and selected formulas; stores corrections so values remain exact. This is a plausible source of substantial advantage on suitable data. |
| Logs and scientific data | `src/log.inc`, `src/sci.inc` | Template extraction, inferred record layouts and image predictors expose structure that byte compressors cannot directly interpret. Effectiveness depends on detection and real input families. |
| Text transform | `src/word.inc` | Builds an in-band word dictionary with reversible markers. Metadata and scanning cost must pay for themselves. |
| Seekable container and execution | `src/czn.c`, `src/stream.inc` | Independent blocks, checksums, range reads, persistent scratch storage, streaming frames and parallel decoding are useful product capabilities. |
| Compression backends | `vendor/`, system liblzma | rt uses LZ4, fast and hc use Zstandard, eco uses Brotli, max uses BWT via libbsc or LZMA2, and ultra compares its context mixer with alternatives. Gains cannot be attributed to a wholly new standalone codec. |
| Context mixer | `src/cm.inc` | The source explicitly describes lpaq-style modelling, arithmetic coding and adaptive probability maps. Its implementation may have project-specific work, but these algorithm families are established prior art. |

Existing codec reuse is explicit in source and notices. That observation does not establish plagiarism, nor does an audit of these files establish independent invention or complete licensing provenance. The user's desired independent algorithm requires a separate experimental core and clear records of its design and sources.

## Source review findings

1. **Per-call format state is global.** `cz_newmodes` in `src/czn.c:18` is reset at the start of each compression, ORed by table workers at `src/tbl2.inc:561`, then read to choose the version byte at `src/czn.c:1611`. Concurrent calls can reset another call's accumulated flags. Atomic operations avoid an elementary data race but do not make this per-job state logically independent. A stream can advertise a version below the predictors it contains, violating the stated compatibility contract. Move flags into the compression job and combine worker results there. The reproducer in `check_concurrency.py` confirmed this: a grouped cubic table advertised version 5 alone, but version 3 in all twelve concurrent trials, with every current-decoder round trip still passing. See `concurrency.txt`. This demonstrates incorrect compatibility metadata rather than current-decoder data loss.
2. **Planning concurrency has an unsynchronized global.** `cz_plan_threads` at `src/czn.c:921` is assigned by each compression at line 1567 and read by formula discovery. Concurrent callers with different thread settings race. Pass the setting explicitly through planning context. The adapter deliberately disallows lzbench external concurrent calls; internal block threads remain enabled.
3. **The concurrent-caller fallback ignores thread creation failure.** `run_pool` at `src/czn.c:1487` creates private helper threads and joins every slot without checking whether creation succeeded. Resource exhaustion can lead to joins on uninitialized thread identifiers. Track only successfully started helpers and let the caller drain remaining work.
4. **Allocation failures are not consistently handled.** Several planning paths dereference allocations without checking them; for example `compress_impl` allocates plans and block arrays at lines 1563–1566, and `parse` allocates offset arrays at line 1635 before populating them. Some `realloc` calls overwrite the sole pointer. Add checked allocation and overflow helpers plus targeted failure injection. A memory budget around the context mixer alone cannot prevent these failures.
5. **The existing performance harness is unsuitable as a release gate.** `bench/czbench.c` reports best times, samples slow settings only once, and returns success even after a printed verification failure. It omits failed file opens from its exit status. Keep it as a development tool, but use an independent harness with nonzero failure exits, explicit iteration counts and raw data for claims.
6. **Compatibility and adversarial parsing need broader evidence.** Header and index checksums catch accidental corruption but are not a defense against intentionally constructed frames. The decoder trusts fields enough to size allocations. Add configurable limits for output bytes, block count, transformed bytes and decoder memory, then test valid checksums around malicious lengths. This is a review recommendation, not a claim that every such input causes a memory error.

7. **Portability has a defined scope.** `put32`, `put64`, `get32` and `get64` use native-endian `memcpy`, which agrees on the tested little-endian Intel and Apple silicon targets. General support for arbitrary machines requires explicit little-endian serialization and inspection of transforms for the same assumption. The build and execution layer also assumes POSIX facilities and pthreads. A Windows or big-endian port needs separate validation rather than only CPU-count detection.

The existing suite covers round trips, ranges, transform codecs and corruptions; CI also defines sanitizer and fuzz jobs. Those definitions are useful engineering groundwork. A job's presence is not evidence that the current revision passed it on every platform.

## Fair comparison rules

Use all twelve Silesia files individually, without concatenating or using a tar wrapper for the primary aggregate. Tar segmentation is one of Kolmo's features and can change its planning; archive packaging therefore belongs in a separate experiment. Keep OWID tables separate from Silesia. In later evaluation add enwik8 and enwik9, held-out logs, JSON, source code, executables, modern document formats, floating point arrays, compressed media, random bytes and many small files.

Report original bytes divided by total compressed bytes, including headers, transforms, model parameters, exception streams and dictionaries. For a trained model, declare whether weights are shared preinstalled assets or transmitted bytes, and publish both amortized and standalone costs. Reserve whole input families for held-out evaluation; random rows from the same table do not establish generalization.

Report compression and decompression separately, with peak process memory, decoder working memory, and one-thread versus whole-machine results. Match block sizes in a separate experiment, because whole-stream history can materially improve a competitor's ratio. Give competitors parallel independent blocks when evaluating that execution strategy. Record actual versions, compiler flags, input hashes and CPU thread counts. GPU results must include synchronization, transfers, model setup and CPU work, alongside kernel-only figures if useful.

[lzbench](https://github.com/inikep/lzbench) is an appropriate independent in-memory harness, but has many workloads and settings rather than one universal score. Its external pool supports parallel comparisons; the public [Silesia results](https://inikep.github.io/lzbench/) use a different machine and cannot be compared directly to this Mac's MB/s. Our adapter preserves Kolmo's production backends through a shared-library boundary; baseline codecs use lzbench's own sources.

## Improvement milestones

Use two explicitly different product targets while experimenting. A proposed fast profile minimizes total bytes subject to compression and decoding throughput at least matching a chosen Zstandard configuration within a fixed memory budget. A proposed archive profile minimizes bytes subject to an agreed minimum decode speed and maximum decoder memory. Set the actual thresholds from deployment needs before tuning. An improvement that wins only by multiplying decoder memory or allowing minutes of extra compute must be reported with those conditions.


### Establish a trustworthy baseline

Make the existing library safe for independent concurrent calls, handle thread creation and allocation failures, and enforce decoder resource limits. Preserve test vectors for every readable format version. Add regression tests specifically for overlapping formula and non-formula compression, injected allocation failures, tiny destination buffers, truncated indexes and memory-limited decoding. Keep performance changes separate from correctness fixes so each result has an attributable cause.

Freeze an evaluation manifest before tuning. Run at least five whole benchmark repetitions in randomized order and report medians and variation. Split one-thread, equal-block parallel, default whole-file and streaming experiments. Add competitor level sweeps and plot the non-dominated configurations: a point is dominated when another is at least as good on all declared axes and strictly better on one. This replaces the misleading goal of one codec winning every setting.

**Gate:** zero observed round-trip mismatches, sanitizer failures or silent corruption; reproducible baseline results and explicit coverage limits. A passing finite test set never proves absence of all defects.

### Measure where time and bytes go

Instrument planning, parsing, transform selection, residual creation, backend coding, checksumming, output assembly and decoding. Record block sizes, transform IDs, codec IDs, model bytes and exception rates. Run ablations with transforms disabled, formulas disabled, each predictor removed, and unchanged backends. This separates semantic gains from codec and parallelism gains.

The highest-value immediate targets are avoiding wasted analysis on incompressible data, reducing repeated parsing and temporary arrays, and improving decode locality. Implement a bounded early-rejection probe and retain a raw path whenever estimated benefit does not cover metadata and compute. Validate probes on held-out inputs so they do not become corpus-specific heuristics.

**Gate:** a proposed speed improvement changes the measured bottleneck and preserves correctness. Proposed acceptance targets are at least 10 percent lower elapsed time or 5 percent fewer bytes on the intended held-out workload, with no unexplained large regression elsewhere; these are decisions for experiments, not promised gains.

### Develop an independent residual codec

Create an isolated experimental codec with its own format, encoder and decoder, without importing competitor implementation code. Keep current backends as named baselines so the experiment can be evaluated honestly. Start with exact integer residual streams and a transparent entropy coding primitive derived from published mathematics. Reimplementing a standard entropy primitive does not make that primitive a new invention.

The central hypothesis is that many datasets admit short reversible programs: a model generates most values and a sparse correction stream reconstructs every original byte. Search a deliberately small language of bounded integer affine relations, piecewise trends and exact rational relations. Preserve lexical details such as original decimal scale, signs, missing values and quoting in explicit residual channels. Build an acyclic dependency graph so decoding follows a fixed order.

Choose a model using total description length:

`total bits = program bits + restart state bits + residual bits + exception bits + container bits`

Use multiple separated sample windows and validate candidates on additional windows before processing the full block. Estimate the actual residual coder cost rather than choosing solely by residual magnitude or correlation. Cap search time and compare with the raw residual model. Extend the existing narrow formula work only when this objective finds a measurable advantage.

**Gate:** the new codec must earn its own result with all side information counted. Label any win that still uses a third-party backend accordingly. Establish novelty through a broader literature and provenance review before describing the design as new.

### Preserve useful relationships across parallel blocks

Investigate storing a compact relationship program once per independent group, with explicit restart state for each block. The hypothesis is that this retains stable table or record relationships without repeating full metadata and without a serial decode chain. Each random-access block must reference only bounded, independently available state. Account for shared metadata in full-file and range-read costs.

Measure restart spacing against ratio, decoder working set, parallelism and time to first requested byte. Select a few stable, documented profiles instead of silently changing compressed output with the local core count. Hardware scheduling can adapt while file content stays deterministic.

**Gate:** a metadata reduction or ratio gain survives accounting for restart state and preserves bounded range reads. Cross-block model sharing is established territory; this specific implementation needs evaluation and prior-art checking.

### Use CPU and GPU resources adaptively

Replace static thread counts with a resource-aware scheduler that respects affinity and container CPU quotas, decoder memory, current memory pressure and input size. Test 1, 4 and 10 threads on this Mac: four fast cores may be preferable for some phases, while ten cores can improve throughput for others. Size the work queue for load balancing, but treat changes in compression block size as a separate ratio decision. Pipeline bounded read, plan, encode and write stages where dependencies permit.

Prototype one Metal stage with enough regular work to justify GPU launch cost: batched histograms, record-stride scoring, predictor scoring, byte-plane transforms or independent residual streams. GPU-accelerate the useful measured stage first. The current adaptive bitwise context mixer updates state after every bit, so porting its loop unchanged is an unsuitable first GPU project. Begin with independent lanes and deterministic integer kernels.

Apple supports parallel work through [Metal compute](https://developer.apple.com/documentation/metal/performing-calculations-on-a-gpu). An optional NVIDIA backend can use CUDA; [nvCOMP](https://developer.nvidia.com/nvcomp) is a useful comparison baseline but would remain third-party compression if incorporated. Other machines retain a CPU fallback. Backend availability does not guarantee that using it is faster.

**Gate:** keep a GPU stage only if end-to-end throughput improves with all overheads counted at a useful file size, without loss of exactness or unexpected peak-memory growth. The scheduler should minimize completion time rather than force every execution unit busy.

### Evaluate learned models only where their cost pays

A small model predicting which reversible representation to try is a lower-risk experiment than a large generative decoder. Compare it with a simple measured decision rule, and count feature extraction and inference costs. Predictor probabilities must be quantized and deterministic if they affect decoding.

A research branch can test batch-parallel learned residual probabilities. Define training data, freeze weights, and evaluate on unseen source families. Compare full model distribution costs and cold startup with shared-model service use. A stronger language model may improve text compression while losing badly on latency, energy and memory; it is not automatically a superior file compressor.

**Gate:** held-out compression savings exceed model and inference costs for the intended deployment. Do not train on the evaluation corpus or claim standalone ratios that exclude required weights.

### Validate a competitive claim

Expand the independent comparison to current OpenZL with suitable structured profiles, 7-Zip LZMA2 and PPMd, libbsc, ZPAQ and other relevant lzbench contenders. Run on x86-64 Linux, ARM64 Linux and Apple silicon, plus a supported discrete GPU if the GPU branch survives its gate. Publish raw data and an adapter that an independent reviewer can build; submit upstream only after review and explicit authorization to publish.

A meaningful claim might be “smaller tables at at least Zstandard level 3 throughput under this memory budget,” or “better Silesia ratio at a fixed minimum decode speed.” A finite leaderboard victory remains a result for a declared corpus, version and machine. No plan can guarantee world leadership over all data and resource budgets.

## Proposed experiment order

| Priority | Deliverable | Evidence needed to continue |
|---|---|---|
| 1 | Isolate per-call state and make resource failures safe | Concurrent reproducer no longer changes version metadata; targeted failure tests pass |
| 2 | Freeze unseen input families and profile the current implementation | Repeated measurements identify where bytes and CPU time are spent |
| 3 | Reduce planning and parsing cost, preserving current format | Meaningful throughput gain without losing the useful ratio advantage |
| 4 | Build the independent residual codec and explicit model-cost objective | Held-out gains with every model and exception byte counted |
| 5 | Evaluate bounded shared relationship programs and restart state | Better ratio or lower metadata cost while preserving range reads |
| 6 | Prototype a single Metal kernel and resource-aware CPU scheduling | End-to-end improvement after synchronization and memory costs |
| 7 | Test small learned selectors or residual probabilities | Gains over simple rules on input families excluded from training |
| 8 | Publish reproducible cross-platform frontier comparisons | Independent reviewer reproduces declared wins and limitations |

These are staged experiments, not a forecast that all ideas will succeed. Stop a branch when its measured costs fail its gate, retain the successful parts, and keep an attributable baseline for each decision.

## Research that informs the experiments

[The Description Length of Deep Learning Models](https://arxiv.org/abs/1802.07044) motivates accounting for a model's description as well as data fit. The proposed residual objective applies that principle; it is not a claim of inventing minimum description length.

[Language Modeling Is Compression](https://arxiv.org/abs/2309.10668) demonstrates the relationship between predictive models and lossless compression. It motivates evaluating probability models while keeping model size, inference work and generalization explicit.

[Practical Lossless Compression with Latent Variables using Bits Back Coding](https://arxiv.org/abs/1901.04866) presents an existing learned-compression construction. It is a research comparator, not an original Kolmo proposal or a component to copy into the independent core.

[ZipNN](https://arxiv.org/abs/2411.05239) reports benefits from structure in neural-network weights. It supports testing domain-specific data representations; it does not establish gains for Kolmo on arbitrary files.

[OpenZL's framework description](https://engineering.fb.com/2025/10/06/developer-tools/openzl-open-source-format-aware-compression-framework/) already uses reversible transforms and optimized plans. Its [Compression Transformer](https://openzl.org/blog/2026-09-24-compression-transformer/) selects numeric compression graphs automatically. Consequently, “automatically choose transforms using ML” is insufficient as a novelty claim. Kolmo needs independently implemented, quantitatively demonstrated advances in the actual models, cost estimation, metadata or execution.
