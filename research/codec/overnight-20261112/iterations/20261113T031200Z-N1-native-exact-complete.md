# 13 November 2026 — native exact-header OpenZL comparator completed

Status: IMPLEMENTED, BUILT, TESTED, DEVELOPMENT_MEASURED. No P2 acceptance, fresh validation, production integration, merge or release. New Actions workflows: zero; new runner-minutes: zero. Overnight schedule unchanged. All new work is confined to fix/planner-record-ownership-isolated-20261112. PR #7 remains draft/unmerged.

Start 3d847949913a39b831db76570e8a7e47dfe5bc20; claim 4f13bdde066a2a44c1066a3f794359728dba13ca. Refreshed branch to 89cceb240e0a538b7e87cbd20b60d50272965b74, retaining concurrent P2-E21/E22 and independent E17 auditor changes. Add only research/codec/N1-native-exact and this receipt; all production, previous research and other branches remain unchanged.

The new native C++ frontend parses exact integer NPY or PCM16 WAV boundaries and uses standard OpenZL SplitN + endian numeric + ACE default numeric/serial graphs. Original prefixes and tails are encoded, not omitted. Decoding uses separately compiled unmodified ZL_decompress with no application parser. Pinned OpenZL 32246b48faee46807f84183dac4db479089f5445. Current comparator limit 16 MiB; narrow unsupported formats explicitly reject. No custom inverse or new wire ID.

Recovered N1 artifact 13980664053 (run 43299261465); ZIP digest and 1,229 manifest entries verified, 3,674 original OpenZL Git blobs checked. R3 artifact 13970918568 supplied 8 previously observed NPY inputs; all 282 entries verified. Its source and input digests are in EVIDENCE.json. The cloud zli binary requires unavailable GLIBC_2.43: the exact source was rebuilt locally instead. Stock-API decoder passed; that original CLI was not claimed to execute.

Executed: cmake configure/build native_exact, native_exact_san, stock_reader and parse_cli; separate Clang parser sanitizer build; 14 parser test groups and 5 linked roundtrip groups in release and sanitizer variants. Linked suite covers 35 fixtures x 2 levels, exact reconstruction, deterministic bytes, malformed/truncated frames and overwrite rejection. OpenZL/vendor libraries uninstrumented; new wrapper/parser instrumented. All 56 benchmark frames matched a sanitized re-encode.

Measured all six exact/whole/serial x level6/9 controls plus identical exact9 duplicate, on all 8 fixed inputs: 56 configurations, 2 passes x 3 encode/decode rounds, 672 raw native-process samples, 448 recomputed medians. Compact raw data, per-frame hashes and audit script are Git-tracked. Entire local source/result subtree hash: 12773ef622c539a9c2abbc88ef4bd80549202782, identical to uploaded tree.

Result: exact9 776,404 bytes versus whole-numeric9 740,257 (+4.88%) and serial9 1,229,924. Exact typing improves fire_distance by 9.82% but enlarges hillshade by 20.50%; neither profile dominates. All boundaries are byte-lossless. Never choose only the weaker OpenZL rival to manufacture a win. Duplicate timings vary approximately 0.61–1.42, preventing precise latency/acceptance claims. No fresh Kolmo/zstd candidate timing was measured.

Next substantive task: run retained typed Kolmo candidates and both relevant native OpenZL profiles in one matched, batched harness with a frozen rival/profile rule and fully charged actual selection. Keep this a development comparison, then preregister truly new validation. No RL, new parser portfolio or further unrelated audit is required first.
