# Mac-local reconciliation, release build and single-thread comparison

User request, 14 November 2026: combine the best features from the ChatGPT RC branch and Claude Code's `kolmo-integration` checkout; build a new release and benchmark on the user's Mac using a single thread.

## Execution status at handoff

This file is an execution task, NOT a completed merge, build, release or benchmark receipt.

- Inspected remote candidate: `adnaneben/kolmo`, `integration/best-features-rc-20261113`, commit `1966514452af4f7f2882921cc252b36967835843`.
- Integrated codec source: `ae3957ad80d134506ce37b59857af25fe8826062`.
- Existing combined CI checkpoint: `b627c1171d821deca1484f023c7eeda6a1e5bde1`; its evidence does not automatically validate subsequent source changes.
- Remote branch inventory did not contain `kolmo-integration`. Its local HEAD, worktree changes and repository identity are UNKNOWN until inspected on the Mac.
- The chat has no connected Mac shell. No execution on the user's Mac has been established.
- The latest instruction authorizes reconciliation and a local release build. Preserve all source branches and main while preparing the combined candidate; do not delete PRs, force-push, overwrite published releases or publish packages as a shortcut.

This task should be executed by Claude Code or Codex **running locally on the user's Mac**, with the `kolmo-integration` checkout available. Do not replace the requested hardware with a cloud VM, GitHub-hosted macOS runner, container, Rosetta process or this chat's runtime. Do not ask the user to repeat a standalone audit when you can inspect that checkout directly.

## 1. Identify both inputs and preserve local work

Before editing, read applicable AGENTS.md/CLAUDE.md, `integration/rc1/FEATURES.json`, `integration/rc1/STATUS.md`, `benchmark/accept/ACTIVE_POLICY.json`, and the competitor policy. Treat these as source context; the current user request is authoritative for this continuation.

1. Require `uname -s` to be Darwin. Record `sw_vers`, `uname -m`, CPU/model identifiers, physical RAM, compiler and linker versions, and whether the process is translated. Do not assume the Mac is an M5 merely because earlier reports used one.
2. From the actual `kolmo-integration` directory record repository root, redacted remote URLs, HEAD SHA, branch, status, submodules and worktrees. A folder name is not a branch identity.
3. Preserve tracked and untracked changes. Never run reset --hard, clean -fd, an automatic stash, a forced checkout, or an indiscriminate git add. Store any necessary local snapshots outside tracked public paths; do not publish private data, credentials or unrelated untracked files.
4. Fetch the official remote candidate normally. If it advanced from the inspected SHA, inspect the new commits and record the actual frozen base. Do not reset it.
5. Use a new isolated worktree or clone. Preserve the original Claude checkout and all original branch pointers. Detached worktrees are acceptable for preparation; the authorized remote integration destination remains `integration/best-features-rc-20261113`.
6. If Claude's source is in another local repository, import its committed objects through a local fetch/bundle into the new clone. Do not assume it has the same history or silently discard uncommitted work. Snapshot explicitly in-scope dirty source separately without changing the original checkout.
7. Produce `INPUTS.json` and a source inventory identifying both exact commits, relevant dirty-source hashes, common ancestor when present, already-equivalent patches, and unique changes. Use tree diffs and patch equivalence, not only ancestry: several RC1 features were ported manually.

## 2. Reconcile features, not whole branch histories blindly

Create `MERGE_DECISIONS.json` with each feature's source, code paths, dependencies, compatibility impact, test evidence and disposition: already-equivalent / include / opt-in / defer / reject. Report unresolved differences explicitly.

Start from RC1's verified feature manifest. It already contains the ownership fix, compatible memory/tar facilities, checked integer inverse, PR #9 native contextual leaf, PR #8 ultra level-2 graph leaves, typed Huffman and CSV archive options, CLI cleanup and synchronized binding sources. Do not apply these twice because their original commit is not an ancestor.

Priorities:
- Preserve correctness, resource handling and byte-identical memory/copy improvements.
- Compare the two checked inverse implementations and type-first routes; choose based on tests and complete-pipeline cost, not author name.
- Keep plain eco available alongside the CSV picker. C1 and RC1 contain concrete routing regressions; do not assume the picker is never worse.
- Keep native contextual leaves and ultra level 2 opt-in unless new mode-specific acceptance is demonstrated.
- Carry useful layered codecs only when framing, reverse operations, dependencies and resource bounds are explicit. Do not relabel S1/S2 KST1 payloads as CZN1 or silently reuse a codec ID.
- Preserve unknown/unsupported-input fallbacks and exact lexical reconstruction for structured formats.
- Preserve all original research reports and negative findings. Do not change thresholds to make a failed combination pass.

Implement accepted changes in separate reviewable commits. Add feature-off or generic-path ablations where necessary to explain a change. Update API/CLI/bindings/specification together when affected. No default promotion is implied by a standalone size gain.

## 3. Build and test the actual combined source

Create separate builds for the released baseline, current main/pre-integration reference, Claude's unchanged input, RC1, and the new combined candidate when they are genuinely distinct. Deduplicate identical trees instead of falsely treating them as independent codecs.

The inspected RC1 Makefile uses Darwin defaults, supports `CC=clang CXX=clang++`, and produces `build/kolmo`. Use explicit release flags consistently: optimization and architecture targeting appropriate for the detected Mac, `-DNDEBUG`, and `-ffp-contract=off` for relevant numerical code. Do not blindly copy Linux-only `-fno-plt` or `-march=native` onto all Mac architectures. Record exact commands. Compilation may be parallel; benchmark execution may not overlap it.

Run the actual available targets, including the combined inverse/route gates, ordinary checks, resource/concurrency tests, corruption/fuzz tests, binding checks and old/new cross-decodes. RC1 has `check-int-inverse-fast`, `check-rc-options`, `check-frontier-dna`, `check-cm-compact`, `check-tar-files`, and sanitizer targets; inspect the final Makefile before invoking them.

Use separate sanitizer and optimized builds. Preserve diagnostics. An OOM, timeout, unsupported sanitizer or skipped test is NOT a pass. Test default-byte preservation where promised, version-9 use and old-reader rejection, native-leaf/typed/CSV flag combinations, unsupported inputs, and allocation failure. Synchronize bundled native source in Python/Rust and test it, rather than only rebuilding the CLI.

Freeze the candidate source commit and executable hashes after correctness. Any source change after this freeze invalidates the corresponding measurement identity and requires rerunning affected work.

## 4. Install or build the matched competitors locally

Inspect existing binaries first. Record canonical executable path, source version/commit, compiler/flags when available, native architecture and hash. Do not substitute a vendored raw-block helper for an official file compressor without labeling that measurement boundary.

Required designated rivals:
- zstd level 3 for speed modes; also fixed level 1/9/19 controls to show trade-offs.
- Pinned OpenZL with applicable binary, CSV and numeric configurations for archive modes. Retain exact-header and whole-numeric controls where relevant. Preserve the historical pin `32246b48faee46807f84183dac4db479089f5445` for reproducibility; record any newer rival separately.
- For source-trained OpenZL, train only on the declared development/train split. Pin graph/model files and charge training time and model bytes at a registered reuse horizon. No free training on test files.

Appropriate additional controls: LZ4, Brotli, XZ, libbsc; Pcodec for numeric values; FLAC for audio; NAF for genomes; ZPAQ/bzip3/Kanzi for cold archive, when their native tools and input semantics can be verified. Missing tools are MISSING_TOOL, not automatic wins. Obtain packages from official sources, pin their resolved versions before measurement, and do not install unrelated software or provision paid resources.

## 5. Enforce one compression thread, not merely one worker argument

- Kolmo: `-T 1` in both directions; verify nested backend concurrency.
- zstd CLI: `--single-thread`, not merely `-T1`; synchronous native API sets `ZSTD_c_nbWorkers=0`.
- XZ: `-T1` in both directions.
- OpenZL: synchronous pinned API/CLI path, with its actual documented controls. Training uses `--threads=1` where supported. Never invent a universal compression -T flag.
- LZ4: use the pinned version's strict single-thread mode or synchronous API, not an assumption that one worker eliminates a separate I/O thread.
- libbsc: verify the pinned CLI's multithreading-disable option; bzip3 uses `-j 1`, Kanzi uses `--jobs=1` when supported by those exact versions.

Set OMP_NUM_THREADS=1, OMP_THREAD_LIMIT=1, OPENBLAS_NUM_THREADS=1, MKL_NUM_THREADS=1, VECLIB_MAXIMUM_THREADS=1, RAYON_NUM_THREADS=1 as safeguards. They are not proof that arbitrary tools obey them. Observe target process/thread CPU use outside the main timing experiment; report helper/I/O threads and any violation separately. Do not count the measurement sampler itself as a codec thread. No parallel file compression, GPU/Metal acceleration or training during timed runs.

Take one shared benchmark lock for the whole session so another Claude/Codex benchmark cannot overlap. Do not terminate another user's process to acquire it. Record AC power, power mode, thermal state and background-load observations. Do not claim hard P-core pinning or controlled thermal conditions unless actually enforced and recorded on macOS.

## 6. Freeze the local protocol and input manifest before measuring

Use whole original files. Include regular/wide/irregular CSV and the known regression cases, exact NPY integer arrays, speech and silent/repetitive/noise audio, logs, text, executables and complete mixed corpora available locally. Include applicable specialist datasets only when available with provenance. Distinguish observed development, stress tests and genuinely source-group-disjoint validation. Do not call historical benchmark corpora untouched holdouts.

Benchmark all six shipping modes, applicable opt-ins and useful feature ablations. Do not run every hypothetical flag combination; enumerate supported distinct configurations with explicit applicability. Keep resource exclusions and rival failures visible. Existing pilot prefixes are diagnostic controls, not full-corpus results.

Native-API and complete CLI/file measurements are separate suites. Never compare native Kolmo timing with a competitor's Python/process startup timing. For the CLI suite include startup and file I/O consistently. For native suites charge planning/typing/inference/rejected trials/checksums/allocation/free consistently. Match integrity settings when possible and report remaining differences.

Warm up each configuration. Run at least seven paired, randomized serial rounds, with encode and decode independently timed and short operations batched where appropriate. Verify every original byte and record compressed hashes. Use medians and dispersion, not cherry-picked best runs. Do not pool timing samples from another machine or old experiment.

Memory is a separate run: time-weighted RSS during active encoding and during active decoding, plus each phase's maximum RSS. Use a macOS-native sampler, record sampling interval/boundary and missed short allocations, and corroborate peaks with process high-water accounting. `/usr/bin/time -l` alone provides no time-weighted average. Do not port Linux `/proc` calls unchanged; do not treat pre-exec launcher memory as codec peak. Include process buffers/libraries and allocator retention, or clearly label any separate scratch-only figure. Report unavailable average memory as NOT_MEASURED, not zero.

Keep memory limits high enough for the actual Mac but predeclare safe bounds. Include full ultra cases only when physical memory permits without swap pollution. Resource failures remain visible and are not excluded from summary coverage.

## 7. Score and report exactly what ran

For each configuration/workload, report:
- ratio = original bytes / complete stored bytes;
- encode and decode MiB/s separately, using original bytes divided by measured duration;
- average and maximum encode RSS and decode RSS separately;
- per-file gains/regressions and workload aggregates on identical supported inputs;
- mode-specific cost using ACTIVE_POLICY and its frozen parameter values;
- byte-exactness, format compatibility, thread verification, coverage, timeouts and limits.

Green = better observed metric than the named comparator; red = worse; amber = missing, incomparable or materially uncertain. Show underlying values and percent differences. Green is not statistical significance. Size wins cannot hide decode or memory losses. An oracle that chooses after all trials is not an online selector; charge all executed search/selection cost.

Keep actual fixed-encoder totals separate from any hindsight fallback calculation. Do not sum standalone savings across layers. Use base/A/B/A+B ablations when attributing combined gains. Source-trained and untrained rival results remain distinct.

Produce `MERGE_DECISIONS.json`, `BUILD_MANIFEST.json`, `TEST_RECEIPT.json`, `PROTOCOL.md`, `INPUTS.json`, raw timing/memory data, a green/red workbook or HTML dashboard, and `RESULTS.md` under a new local evidence directory. Store compact source/receipts on the authorized integration branch; preserve large input/frame archives separately with checksums and a real durable location.

## 8. Build the local release candidate, then report completion

After final source correctness, package a native Mac candidate such as `kolmo-macos-<actual-architecture>-rc2-<actual-sha>.tar.gz` containing the CLI, licenses/NOTICE, release notes, source/binary hashes and reproduction instructions. Include the actual VERSION value; do not label it as a new stable published version without updating all version metadata and completing release checks. Record `otool -L` dependencies and test the extracted binary outside its build directory. Do not silently call a Homebrew-dependent binary portable.

Use this exact packaged binary for at least a smoke replay of the measured configuration. No public tag, package upload, branch deletion or merge into main is necessary to deliver this local build. A public stable release is a later promotion step after the user can see the combined results and unresolved regressions.

Push reviewable source and compact evidence normally to the existing authorized integration branch only after rechecking its current HEAD and reconciling concurrent changes. Never force-push. Update PR #10 rather than opening duplicate implementation PRs. Readable source and receipts must be committed, not only encoded transport blobs.

Completion report must state exact input commits, actual merge decisions, candidate commit, Mac model/OS/compiler, single-thread controls, tests actually run, archive path/hash, each competitor's measured ratio/speeds/memory, and uncompleted areas. A plan, queued job, build log or Linux benchmark is not a completed Mac performance result.
