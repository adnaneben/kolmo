# WebAssembly compression research

The opt-in encoder check in commit 63789fa8f023cf825c076ac7544ecc8c6bddb96f produces ordinary v12 frames and fixed a measurable error on Oniguruma. Across 71 complete, distinct installed core modules, the check changed frames for one module at levels 1 and 3. Pinned main restored every input directly. Keep this option for review. The separate prototype that groups function headers failed its promotion criteria.

## Results on Oniguruma

The input is 466,610 bytes, SHA-256 fd885c2d12e5951e59d761ebd4a006e06254b1491fd6f530c92b69fb4d8d77d9. Sizes include the full frame. Throughput is decimal MB/s. Rows compare five timed repetitions on the same shared Apple ARM64 host, using arm A as the displayed reference. The paired speed change compares the candidate with the mean time of the two baseline controls in each round.

| Level | PR #104 → check bytes | Encode MB/s | Decode MB/s | Combined MB/s | Paired speed change | A/A2 control |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 245,129 → 205,447 | 78.8 → 84.2 | 115.9 → 118.7 | 47.2 → 48.3 | +4.1% | -2.1% |
| 3 | 210,023 → 138,745 | 47.9 → 41.4 | 112.6 → 117.1 | 31.9 → 29.6 | -7.3% | +4.6% |

The default planner sampled the module as an eight-byte array and chose delta byte shuffling (plan 2,8,1,1 with tflag 1). Both arms used the same coder, KLZ at L1 and KLZH2 at L3. The option checks the selected block against the same coder on the original bytes and retains the smaller encoding. The existing decoder needed no option. The six post-discovery transfer modules covered OCR, font decoding, vector graphics, .NET, the C++ grammar and VS Code authentication. All six kept the original frame at both levels. The subsequent complete inventory found no second naturally occurring activation. The generated CLI fixture independently exercises the changed branch on an ordinary WebAssembly custom section.

The option accepts a recognized core WebAssembly v1 header and a single planner segment, at preset 1 or 3 with the default codec level. Only the eight-byte, one-channel delta shuffle is checked, on blocks at least 64 KiB. All other selected plans run normally. Explicit --analysis and --codec-level settings take precedence. A check can spend more CPU to encode the alternative: on Oniguruma L3 the measured combined throughput fell 7.3% in the paired comparison. The new C API option is KOLMO_T_WASM_SHUFFLE_CHECK. The CLI spelling is --wasm-check-shuffle. On pinned main, the existing broader --analysis none option also produced 205,447/138,745-byte directly readable frames on this module.

## Why the header-grouping wrapper was rejected

The native prototype retained original function body sizes and local declaration vector bytes, gathered them ahead of the instruction spans, preserved other module sections, and added a four-byte experimental marker. The Kolmo decoder recovers the intermediate payload. The prototype then restores the original WASM. All 288 timed arms matched input hashes. On seven of eight modules, frame-size changes stayed within 136 bytes. The executable wrapper was slower in all 16 input/level cells after accounting for file I/O and the two extra processes. Paired combined speed changes ranged from −45.8% to −24.7%. A native in-process version could avoid these two process launches, but the isolated size effect remains small.

| Original file | Bytes | L1 main → grouped | L3 main → grouped |
|---|---:|---:|---:|
| diff | 131,825 | 77,603 → 77,638 | 53,034 → 53,162 |
| ini | 4,716 | 2,847 → 2,851 | 2,015 → 2,022 |
| jbig2 | 104,852 | 60,315 → 60,256 | 39,722 → 39,803 |
| oniguruma | 466,610 | 245,129 → 205,387 | 210,023 → 138,678 |
| openjpeg | 250,009 | 120,690 → 120,625 | 83,251 → 83,208 |
| qcms | 94,519 | 50,262 → 50,287 | 34,938 → 34,961 |
| typescript | 1,413,849 | 207,784 → 207,780 | 130,217 → 130,129 |
| webp | 565,369 | 255,920 → 255,861 | 177,748 → 177,612 |

A four-way control on Oniguruma separated section layout from a change in the stock planner.

| Payload | L1 bytes | L3 bytes |
|---|---:|---:|
| Original module | 245,129 | 210,023 |
| Grouped function headers, no marker (diagnostic) | 244,718 | 209,950 |
| Original bytes preceded by marker (diagnostic) | 205,451 | 138,750 |
| Grouped function headers and marker | 205,387 | 138,678 |

The marker changed which existing transform the planner chose. Grouping alone saved 411 bytes at L1 and 73 bytes at L3. The full marker ablation contains 32 verified control frames. The wrapper has its own inverse and emits no production API. The source option above directly encodes the unmodified WASM file.

## Reproduction and limits

The research wrapper and stock --analysis none matrix used pinned main a1d39559686a9ec17c8614b9154a64d7b5b98672. Its binary SHA-256 was b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f. The candidate option was measured directly against its PR #104 base ac10ce80048ab0664074f7ba031634055f4d2225, CLI SHA-256 552bd577b79b0cb48561747f613dd2ff8ee82d9c1eefba9b2e2a653f277a259f, using the pinned-main decoder for all 504 arms. The guard CLI SHA-256 was c7496491e49419a639c007c9ec0cde34c887d8b6d4f48eaeec1ae5f574f24f51. Both source revisions used make build/kolmo (Apple clang 21.0.0). The new source commit sits directly on ac10ce80. Its portable source bundle SHA-256 is 7d38325dc7599554eca7d7f10bf812c529d6dc889598917cdc8fbe645650823d. The experiment was local ARM64; native x86 performance remains unmeasured.

All file commands were single-threaded on warm files. The timed matrices alternate A/A2/B in seeded shuffled order over one warmup and five scored repetitions. Each side runs full compression and decompression, including subprocess start and output publication. Combined speed is original size divided by the sum of encode and decode wall seconds in each observation. Hash verification is outside the timed commands. The transform and inverse times are charged for wrapper rows, and child wall time, CPU and per-child peak RSS are available in the receipts. These measurements cover CLI latency and scheduling on a shared machine, which matters particularly for 4,716-byte inputs.

observations.jsonl contains the 288 grouping arms, marker-ablation.jsonl the 32 marker controls, analysis-none.jsonl the 288 stock CLI controls, guard.jsonl the 504 candidate arms on 14 exact files and inventory.jsonl the 284 baseline/candidate frames for the full 71-file inventory. The modules were supplied by independently installed packages, including PDF.js, Playwright, Tesseract, tree-sitter and Oniguruma. Some ship inside the same application or share toolchains. They are not 71 independent projects. Source paths, input byte counts, frame hashes and exact restored hashes are recorded. No third-party modules are included. The CLI suite including the new fixture, hardening suite and 357 frozen v12 fixtures all passed.

Build the wrapper with clang++ -std=c++17 -O3 -mcpu=native wasm_headers.cc -o WASMTOOL. benchmark.py takes NODE_MODULES VSCODE_RESOURCES REFERENCE_KOLMO WASMTOOL OUTPUT plus --analysis-none for the stock control or --guard CANDIDATE_KOLMO --stock-decoder MAIN_KOLMO --holdout for the checked encoder. Pass observations.jsonl MAIN_KOLMO WASMTOOL OUTPUT to ablation.py. scan.py takes REFERENCE_KOLMO CANDIDATE_KOLMO MAIN_KOLMO OUTPUT NODE_MODULES VSCODE_RESOURCES. Run summarize.py to verify expected arm counts and regenerate results.csv.

## File descriptor and stream verification

Follow-up source 55e5d53b0590b431e4131f008c1789c6632be91f makes the byte-pointer condition explicit and adds normal file and stream regression coverage. For each nonempty file descriptor input, compress_impl maps the file and assigns the planner's src pointer before planning, returning an I/O error if the mapping fails. The final cumulative source bundle SHA-256 is 1286e3926c8946a1359dc326ca897ea3be023a5b36518b90c75ae100f0a2d6d9. Its rebuilt CLI SHA-256 is 4f795046d70616a78ad2979b20a7ba0ca6c9e7362da81cbd9fcedbc29ab1e244.

The final build passed 56 stock readbacks on the real Oniguruma file and a physically written, valid 9,903,804-byte module containing an extra named custom section. Its SHA-256 is d46ad2aba52aae6d2f371810a9d80971129280395faa80d3bd14ac63bb5aff28. This file crosses the CLI's 8 MiB regular-file threshold. At L1/L3 and T1/T4 the buffer, fd-to-buffer, fd-to-fd, regular-file CLI and pipe CLI paths each produced identical one-shot frames. The file-stream and pipe-stream paths each produced the identical segmented frame at --stream=1. All seven paths returned the exact original bytes using the pinned-main CLI decoder. fd-parity.jsonl records every case. The release candidate's full hosted CI and any x86 timings are separate evidence.

To replay on this Apple platform, link the final branch library with cc -dynamiclib -Wl,-force_load,build/libkolmo.a -lpthread -lm -o GUARD_DYLIB and run fd_parity.py GUARD_DYLIB GUARD_CLI STOCK_CLI ONIG_WASM OUTPUT.
