# Workload targets and promotion gates

All limits below are relative to the pinned Wave 1 configuration on the same hardware, input, build flags, level and thread count. A ratio of 0.98 for bytes means a 2% saving. Throughput entries are minimums. Latency and RSS entries are maximums. Both phase RSS figures must pass.

| Workload | Level and scope | Stored bytes | Encode throughput | Decode throughput | Peak RSS | Encode + decode latency |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Fast and small | 1–3, ≤64 KiB, by format and CPU | ≤1.00× | ≥0.97× | ≥0.97× | ≤1.05× | p95 ≤1.03× |
| General streaming | 1–4, MiB blocks, 1 and 10 threads | ≤1.00× | ≥0.97× | ≥0.97× | ≤1.05× | p95 ≤1.03× |
| Structured, general | 3–7, separate numeric, table, log and source code families | ≤0.98× | ≥0.90× | ≥0.97× | ≤1.05× | median ≤1.08× |
| Mixers and native CPU kernels | Exact v12 frames | 1.00× | ≥0.97× | ≥0.97× | ≤1.05× | median ≤0.95× for the stated workload |
| Weights, quick reload | 1–5, dtype and container exact | ≤0.98× | ≥0.90× | ≥0.97× | ≤1.08× | median ≤1.08× |
| Weights, ratio priority | 8–9, ≥1 MiB, bfloat16/float families separately | ≤0.98× | ≥0.55× | ≥0.97× | ≤1.10× | median ≤1.65× |
| Archive, ratio priority | 8–9 and Ultra, by workload | ≤0.98× | ≥0.67× | ≥0.95× | ≤1.10× | median ≤1.50× |
| Archive repair and decoder hardening | Existing valid frames and failure behavior | 1.00× or format-compatible | Report | Report | Bound as documented | Report |
| Directory extraction | Small-file and large-member cohorts | Same archive | Same encode scope | Exact output | Record | Small-file metadata cost and large-file I/O separately |

Named reference competitors are LZAV, LZ4 and zstd fast for low latency; zstd, Brotli and xz for the middle; OpenZL's training-free numeric Transformer and ZipNN for specialties; PAQ8PX and cmix for the high-ratio envelope. Run matched hardware, input bytes, starting state and process scope. Match rivals both at equal byte size and at equal total latency where curves overlap. Preserve negative cases and every lossless integrity check.

A promotion scope specifies format, size interval, level, thread count, architecture and policy version. Seal independent source groups before testing. Use matched options and seven paired trials for a tuned candidate, then repeat on a separately sealed corpus. Use enough samples for tail metrics. Keep a gated candidate available when its measured win is confined to a narrow scope. Keep weak or non-transfer results in the evidence and retain the stock preset as fallback. A changed codec budget is a product decision even when a safety correction requires extra work.

Report decimal MB/s as raw bytes divided by time / 1,000,000. For the chart, the x coordinate uses raw bytes divided by the sum of paired encode and decode wall times. The y coordinate uses raw bytes divided by compressed bytes. Do not average rates across unmatched files or use compression ratio to stand in for size.

## Competitive targets

Measure targets on the exact input and timing boundary used by the strongest eligible rival point for each metric. First match size or latency where the curves overlap, then record the speed and size margin. Lower is better for stored bytes, peak RSS and p95 latency. Keep the closest rival, exact options, absolute values and any unsupported boundary with each row. These are target margins for the next validation, before results.

| Workload | Required rivals | Stored bytes | Encode | Decode | Peak RSS | p95 latency |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Fast API, ≤64 KiB | LZAV, LZ4, zstd-fast | ≤1.00× at matched latency | ≥1.05× at matched size | ≥1.05× at matched size | ≤1.00× | ≤0.95× warm block latency |
| General filesystem and streaming | zstd, zlib-ng, Brotli and LZ4-HC | ≤0.99× at matched combined speed | ≥1.03× at matched size | ≥1.03× at matched size | ≤1.00× | ≤0.95× first recovered prefix |
| Structured datasets and source code | OpenZL Transformer/profile, zstd, Brotli, xz | ≤0.98× at matched combined speed | ≥1.03× at matched size | ≥1.03× at matched size | ≤1.00× | ≤1.00× whole-record batch |
| Weights and cache quick reload | ZipNN, OpenZL safetensors, LZ4/zstd for cache | ≤0.99× at matched reload latency | ≥1.03× at matched size | ≥1.05× at matched size | ≤1.00× | ≤0.95× first page |
| Weights ratio priority | ZipNN, OpenZL safetensors, xz | ≤0.98× at a fixed encode budget | ≥1.00× at matched size | ≥1.00× at matched size | ≤1.00× | ≤1.00× full reload |
| High-ratio archives | cmix, PAQ8PX, zpaq and 7-Zip | ≤0.99× within the measured CPU budget | ≥1.00× at matched size | ≥1.00× at matched size | ≤1.00× | ≤1.00× first requested file |

Report each observed metric and each miss. Use deployment limits where they are tighter than the corresponding rival target. Report a frontier win on a named workload and runtime only once the exact benchmark is reproduced on independent sealed data. Keep file-to-file, in-memory and first-byte comparisons independent and disclose platform and external model or prior costs.
