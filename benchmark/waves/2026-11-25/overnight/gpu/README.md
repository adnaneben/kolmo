# Native Metal availability and prior histogram evidence

The system inventory identified an Apple M5 with a 10-core GPU and Metal support. Two independent invocations of a freshly built Objective-C probe returned zero devices from `MTLCopyAllDevices()` and nil from `MTLCreateSystemDefaultDevice()`, then exited with status 2. The active macOS 27.2 command-line toolchain has the Metal framework and compiled and linked the probe successfully. Its offline shader compiler lookup, `xcrun --find metal`, exited 72. Runtime shader compilation needs an MTLDevice and could not start. The receipts establish the process-visible device state. They do not identify why the runtime exposed zero devices.

A fresh GPU/CPU parity result, cold setup, per-call transfer, dispatch and synchronization time, persistent GPU throughput, encode/decode/combined throughput, and impact on file size are all unavailable from this session. No production or encoder changes were made for this probe.

## Earlier measurements on an accessible M5

The repository already includes an encoder-relevant Metal histogram and a historical M5 experiment. The canonical report and raw rows were introduced in commit `4550ab163373c9ac66db2a8d12cb617e6eb6ed6a` at `benchmark/rc3w2/gpu2/{RESULTS.md,results.jsonl}`. The original JSONL SHA-256 is `352849f6232d505363a5994040b6f36b05240fe9778ceb1252425edd534c338a`. The adjacent CSV preserves all 34 original histogram and batch rows and their result-match flags. The historical kernel calculates per-64-KiB-block histograms and order-0 Q16 entropy on generated mixtures. Each reported median uses five measured rounds after warmup with resident shared memory.

| Historical input | CPU 10 threads, ms | GPU wall, ms |
|---|---:|---:|
| 1 MiB mixture | 0.080 | 0.242 |
| 16 MiB mixture | 0.909 | 3.110 |
| 64 MiB mixture | 3.288 | 5.474 |
| 256 MiB mixture | 12.215 | 13.741 |
| 512 MiB mixture | 23.794 | 18.871 |

The GPU overtook the 10-thread reference at 512 MiB by 1.26×. The CSV also includes the separate order-1 and batched operations. These measurements compare against the historical benchmark reference on generated inputs. They cannot establish a current optimized CPU-versus-GPU result on real files or a compression-level improvement. The current planner uses samples and small segments. This evidence supports keeping GPU routing experimental.

## Reproduce the device check

Build `xcrun clang -O2 -fobjc-arc -framework Foundation -framework Metal probe.m -o /tmp/kolmo-metal-probe` and run `/tmp/kolmo-metal-probe`. With an accessible device the same program compiles a runtime MSL kernel, creates a pipeline and shared buffer, dispatches it, waits for completion and checks the four exact integers 73 through 76. The observed outputs are in `probe.log`, `environment.log` and `displays.json`. On a GPU-visible process the existing `src/arch/metal/gpu2.metal` and `benchmark/rc3w2/gpu2` provide runnable reference code for the next measurement.
