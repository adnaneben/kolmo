# Worker lifetime and buffer release

On draft #104 (`ac10ce80048ab0664074f7ba031634055f4d2225`), switching from four requested threads back to one left three helpers idle and preserved **9,651,184 bytes** of reusable scratch. Releasing the buffers freed all that capacity. Every subsequent single-thread operation ran without waking a pool helper. The alternating-size measurements support keeping manual release at a known idle or memory-sensitive boundary. A separate contention bug has a focused candidate fix below.

## Two complete files, nine pairs of fresh processes

The public C API encoded and decoded the complete Miami GHCN station CSV (10,134,321 bytes) and #104's complete core/pool.c (6,570 bytes) at preset 3, with an explicit 1 MiB block setting and the normal automatic memory governor. The source file formed one block, the CSV ten. Expected frames first came from a separate, single-thread process. Every benchmark encode matched its reference byte for byte and each decode recovered the entire input. The frames were 577,889 and 2,637 bytes respectively. [Sources, frame SHA-256s and build digests](manifest.json)

Each of nine pairs ran two fresh processes in randomized arm order. Both arms encoded small/T1, large/T4, large/T4 and small/T1. One then called `kolmo_release_buffers()` while the other continued with retained buffers. Both proceeded with small/T1, large/T4 and small/T1. Fresh processes loaded data, expected frames and touched output pages before the first codec call. “Cold” below refers to codec/pool state. The operating system's file cache was warm. Elapsed time covers only the public API call. CPU is process user plus system time across threads and includes the resource-sampling overhead. Throughput and faults apply to this Apple M5 and allocator.

| Round trip or boundary | Retain wall ms | Release-arm wall ms | Retain CPU ms | Release-arm CPU ms | Minor faults, retain / release |
| --- | ---: | ---: | ---: | ---: | ---: |
| Initial small, T1 | 0.184 | 0.162 | 0.172 | 0.165 | 39 / 39 |
| First large, T4 | 26.561 | 25.243 | 78.690 | 82.155 | 3,528 / 3,707 |
| Repeated large, T4 | 27.419 | 24.278 | 77.493 | 76.146 | 653 / 675 |
| First small after T4 | 0.180 | 0.120 | 0.187 | 0.126 | 2 / 2 |
| Retain or release boundary | <0.001 | 0.021 | 0.003 | 0.026 | 0 / 0 |
| Next small, T1 | 0.142 | 0.107 | 0.146 | 0.110 | 1 / 1 |
| Next large, T4 | 26.475 | 25.343 | 75.420 | 76.284 | 449 / 556 |

Each number is a median across nine independent processes per arm. Before the policy started, the two physically identical paths already differed by −16.4% in paired wall time on the repeated large file. After the policy, the release arm's next large call was faster in five of nine pairs, with median −3.0% paired elapsed and +0.864 ms paired CPU. Its paired minor-fault difference was −63 despite its higher marginal median. These samples establish neither a wall-time gain nor a reproducible fault penalty from releasing. Each individual API call's raw time, faults, current RSS, peak RSS and physical footprint are in [public.csv](public.csv). [Summary and paired differences](score.csv)

## What remained resident

A separately compiled inspection run read five actual buffer capacity fields and the transform scratch cache on the caller and every existing helper. Scratch sums exclude thread stacks, allocator overhead, and the small TLS bookkeeping structures. Inspection wakes workers outside the measured operations. Pool-wakeup counts only include successful condition-variable returns during the operation itself. New workers can join the first job immediately.

| End of stage | Helpers | Caller capacity | Helper capacity, total | Helper wakes during measured operation |
| --- | ---: | ---: | ---: | ---: |
| First small T1 encode | 0 | 50,101 B | 0 B | 0 |
| First large T4 encode | 3 | 2,412,796 B | 7,238,388 B | 0, newly created |
| Repeated large T4 encode or decode | 3 | 2,412,796 B | 7,238,388 B | 3 per phase |
| Back to small T1 encode or decode | 3 | 2,412,796 B | 7,238,388 B | 0 |
| Explicit release | 3 | 0 B | 0 B | 3 |
| Next small T1 encode or decode | 3 | 50,101 B | 0 B | 0 |
| Next large T4 encode or decode | 3 | 2,412,796 B | 7,238,388 B | 3 per phase |

The public release call took a median 21 µs (range 9–418 µs). All nine release boundaries recorded the same current RSS immediately before and after the call. Median Mach current RSS was 100.16 MiB, median `getrusage` high-water RSS was 100.16 MiB and median physical footprint was 99.25 MiB. These separately measured process figures include input, reference, output and allocator-owned pages. Here the allocator kept the pages resident immediately after releasing the application-owned scratch. The four small TLS bookkeeping objects and three helpers persisted. Releasing a second time preserved zero scratch. [Worker and fork observations](diagnostic.csv)

The timed public run completed 134 exact round trips. The diagnostic run completed 22 more. In both arms, both parent and child completed the large T4 and small T1 operations after fork with the original reference frames. The diagnostic child initially saw no pool helpers and retained only the caller's observable 2,412,796 bytes of scratch. Its first release cleared that capacity, then its next large call created three new helpers. The parent's existing helpers survived. The shipped concurrent/fork suite separately exercised fork while another parent thread was performing four-thread operations.

## Verified cleanup fix

The original release routine checks whether the pool is idle, drops its ownership lock, and reacquires it through the general scheduler. A competing caller can take the pool in between. The scheduler then creates private helpers to run release callbacks on their own empty scratch, leaving the intended workers' buffers untouched. A deterministic test injected exactly this ownership transfer with three existing helpers. The original implementation created and joined three unnecessary private helpers. The fix holds ownership through dispatch to the known idle pool. With the same ordering, it created zero private helpers and cleared every existing helper's scratch.

The [candidate patch](release-contention.patch) covers the native source, Python and Rust source mirrors, and one regression test in the existing resource-failure suite. It applies cleanly to the measured #104 commit. Verification passed the resource suite with 13 injected allocation failures, the 144-case parallel/fork suite, the separate API hardening suite, and all 357 frozen frames. Twenty-two additional real-file round trips across both policies, including parent and child, reproduced #104's unmodified frames byte for byte. [Validation results](validation.json) · [Patched frame-parity observations](fixed-parity.csv)

Keep the pool alive across T1 calls. This workload shows useful reusable capacity and zero normal T1 wakeups. The patch addresses a specific handoff defect when callers contend. Its incidence and service-latency impact in production are unmeasured. A default trim or worker retirement policy would still need multiple allocators, larger ratio/Ultra blocks and workloads with real idle periods and memory pressure.

To repeat the Darwin measurement, build the default #104 tree, then run `python3 replay.py --source CHECKOUT --large USW00012839.csv --out NEW_DIRECTORY`. Run the included scorer beside the resulting files. Test the patch in a separate checkout with the normal `kolmoresource`, `kolmoparallel`, `test-harden` and `test-v12-corpus` targets. No third-party data is bundled.
