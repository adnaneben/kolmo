# Older branches and PRs against RC3: preservation-first analysis

Date: 2026-11-19. Read-only analysis of the review clone
`/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/repo`.
Baseline RC3 = `5071f388d31e5cae307e28e9ec5a68f3ed8113e5`. `main` (`3c6890a9`) is an ancestor of RC3.
Excluded here (covered by another reviewer): `claude/rc3w3-*`, `claude/w4-*`, `a17d6657` (`rc3w2-integration` local head / `worktree-head/rc3w2`).

Method: `git merge-base`, `git rev-list --left-right --count RC3...X`, `git cherry RC3 X`, `git log --cherry-pick`,
`git patch-id --stable`, per-file blob comparison of every file a branch touches against the same path in RC3, and
`git grep` at RC3. "Ahead/behind" is commits in X not in RC3 / commits in RC3 not in X. Nothing was built or run.

Categories: **KEEP** (preserve the ref; holds unique evidence or code, no action needed for RC3), **TEST** (contains
something not in RC3 that could matter; measure before deciding), **PROPOSE_FOR_INTEGRATION** (ready-looking change
that RC3 lacks), **HISTORICAL** (content is in RC3 already or superseded; keep the ref as a record — never delete).

## 1. Summary

- **RC3 already has `--native-leaf` and `--typed-huf`.** `src/cli/cli.c:154-156` at RC3 parses both;
  `KOLMO_T_NATIVE_LEAF` (16) and `KOLMO_T_TYPED_HUF` (32) are in `src/kolmo.h`; the native leaf is
  `src/codecs/nplane.c` (codec 15, container version 9, same IDs as PR #9/#10); typed-huf is
  `src/transforms/graph_typed.c` (`ktf_*`, ported in RC3 commit `c56961f8` from PR #10 at `b627c11`, frames stated
  byte-identical to RC1's build). `-x -l 2` (PR #8) is in `src/core/compress.c:174,181`.
- **Line classes, fast file planning, FASTA sampling and bounded ultra tables are all in RC3** (via `main`):
  `67a022f4`/`eb7460ed` (line classes), `5426dd90` (fast file-planning pages), `411aeac5` (FASTA sampling),
  `aacf5a8d` (bounded ultra tables). PR #5's copies are patch-id-equal (ultra tables, fast-file pages) or
  differ only in Makefile context lines (FASTA: `2caa0e00` vs `411aeac5`, identical +/- lines apart from the
  `src/lines.inc` dependency list).
- **PR #10 / #7 line (181 / 170 commits not in RC3):** `git cherry` marks all 181 as "+" (no patch-id equivalent),
  because RC3 re-implemented them by hand after the `src/` restructure. By content, every production feature of
  PR #10 is in RC3 **except `--csv-archive`** (`KOLMO_T_CSV_ARCHIVE` 64, `src/csv_archive_encoder.inc`), which RC3
  deliberately did not port (`c56961f8` message: larger than eco on 5 of 7 tables, never smaller). The PR's
  919-file `research/codec/` tree, 35 research workflows and `integration/rc1/` payload are **absent from RC3**
  and exist only on these refs.
- **Genuinely missing and worth a test:** (a) `--csv-archive` CSV regularity picker (conflicting evidence: wins vs
  OpenZL csv-9, but not uniformly better than eco); (b) KRANS 32-way static rANS (`claude/krans-20261116`);
  (c) GR6-L3 three-level 6-bit research and option matrix (`claude/dazzling-maxwell-p8y4u0`, research only);
  (d) macOS wheel runner `macos-15-intel` (reflog `dcd3e14e`); (e) research-level levers not in the format
  (packed residual representations, kfam byte-model leaf, order-1 leaf) documented in `research/codec/`.
- **Hazards:** three branches define container-version / codec-ID meanings that collide with RC3's
  (sections 4.4-4.6). Frames written by those builds must not be assumed readable by RC3, and their code must not
  be merged as is.

## 2. PR #10 / #7 line in detail (patch content, not names)

| | PR #10 `integration/best-features-rc-20261113` | PR #7 `fix/planner-record-ownership-isolated-20261112` |
|---|---|---|
| Head | `d1af945b1aa26bbd6a88be229cc3acf5891475c6` | `df42c639` (ancestor of PR #10) |
| Merge base with RC3 | `a64f57fd` (= PR #6 head, in RC3) | `a64f57fd` |
| Ahead / behind RC3 | 181 / 250 | 170 / 250 |
| `git cherry` | 181 "+", 0 "-" | 170 "+" |
| Files changed vs base | 1021 (919 `research/codec/`, 37 workflows, 28 `integration/rc1/`, 17 bindings, 10 src/tests) | 954 |
| PR state | open, not merged, 0 reviews, 2 comments | open draft, 0 reviews, 26 progress comments |

Production code commits (all others are `research/`, `.github/workflows/`, `integration/` only):
`a552a7c8` (native leaf port), `d03cbd46` (CM leaves, typed routes, fast inverse, CLI cleanup), `ae3957ad`
(binding sync), `b627c117` (CI), `f728d23f` (base64 import payload).

| PR #10 feature | PR #10 location | In RC3? | RC3 evidence |
|---|---|---|---|
| `--native-leaf` / NPLANE leaf (codec 15, v9) | `src/nplane.inc`, `kgraph.inc`, `kolmo.c` | **Yes, equivalent** | `src/codecs/nplane.c`; whitespace-normalised diff = 1 encoder line (RC3 calls `huf_compress_all` directly instead of `codec_compress(C_HUF)`) + codec-table registration. Same codec ID and version, so PR #10 native-leaf frames should decode in RC3 (not run). |
| `--typed-huf` (bit 32, NPY / PCM16 type-first) | `src/typed_int_encoder.inc` | **Yes, verbatim port** | `src/transforms/graph_typed.c`; commit `c56961f8`; `tests/test_rc_options.c` |
| `-x -l 2` CM graph leaves (PR #8) | `kolmo.c` | **Yes** | PR #8 head `39463d63` is an ancestor of RC3; `src/core/compress.c:174,181` |
| Fast KG_INT inverse | `src/kg_int_fast.inc`, `tests/test_int_inverse_fast.c` | **Yes, reimplemented** | `0a5b28e6` "fast integer inverse from the research branch, with a conformance test"; `tests/test_fastint.c`, `make check-fastint`; plus SIMD `src/arch/*/int_inv*` |
| CLI frees buffers on every exit, `spit` returns error | `src/cli.c` | **Yes** | `c56961f8` |
| `fuzz/fuzz_nplane.c` | | **Yes** | RC3 `fuzz/fuzz_nplane.c`, CI loop `.github/workflows/ci.yml:83` |
| `--csv-archive` (bit 64, CSV regularity > 0.9916 -> table + zstd-12, else eco) | `src/csv_archive_encoder.inc`, `cli.c` | **No** (rejected) | `tests/test_rc_options.c:4` says the cases were removed; `c56961f8` gives the measurement |
| Research tree (E02C-G, E03, R2-R4, S1-S2, P2-E1..E31, N1, N2, M1, C1, wire-review, production-proposals) | `research/codec/` (919 files) | **No** | RC3's `research/STACK-AUDIT.md` (from `claude/stack-audit-20261116`, patch-equal) audits it item by item and ports the gains that fit the format; the tree itself is not in RC3 |
| Research workflows (`p2e*.yml`, `r3-compatible-validation.yml`, ...) | 35 new `.github/workflows/*.yml` | **No** | RC3 has only `ci.yml`, `planner-ownership-validation.yml`, `wheels.yml` |
| RC1 import payload and perf pilot | `integration/rc1/import/payload/*.b64`, `apply.py`, `perf/` | **No** (RC3 has 2 files under `integration/rc1/`) | |
| Mac reconciliation task | `integration/mac-reconciliation-20261114/EXECUTE_ON_MAC.md` | No | executed later as RC1/RC2 locally |

Stack-audit outcomes now in RC3 beyond PR #10: aligned Huffman/rANS chunks (`a6c2d5fc`) and singleton rANS fill
(`74d08c2c`) are patch-equal in RC3; per-plane leaves (codec 21), Huffman-then-zstd (22) and zero-run (23) leaves
arrived in RC3's format 2 (container v11, `3225484c`), which covers the main "needs a format decision" items of
`research/STACK-AUDIT.md` section 4. Still absent from RC3: packed residual representations (P-04..P-12),
order-1 entropy leaf (P2-19, not pursued by its own wire review), kfam byte-model leaf, `--csv-archive`.

`--csv-archive` evidence conflict (for the TEST decision):
- For: `research/codec/P2-E26`, `P2-E27` (PR #10) — preregistered fresh tests, 33/35 and 26/28 files smaller than
  OpenZL csv-9 within cost; `C1-csv-claims-20261113/RESULTS.md` reproduces those wins.
- Against: the same C1 audit shows the picker is not uniformly better than eco (E27 aggregate 1.02% larger than eco;
  CNAE9, Wage, diamonds, segmentationData worse on size and cost); RC3 `c56961f8`: larger than eco on all 5 of 7
  tables where it applies (7% to 58%); `research/STACK-AUDIT.md`: 55-58% larger than `-m eco` on 3 of 4 CSVs.
- Format: none (existing v7 frames, `dst[5]=KOLMO_ECO`). Risk is mode semantics only.

## 3. Per-item table (not contained in RC3)

Paths in "claims/evidence" are inside the named commit's tree.

### 3.1 PRs

| Item | SHA | Merge base | Ahead/behind | Files / features | Claims / evidence | Depends on | Format impact | Tests | Category |
|---|---|---|---|---|---|---|---|---|---|
| PR #10 `integration/best-features-rc-20261113` | `d1af945b1aa26bbd6a88be229cc3acf5891475c6` | `a64f57fd` | 181/250 | see section 2 | `integration/rc1/perf/RESULTS.md`, `integration/rc1/FEATURES.json`, `research/codec/*/RESULTS.md`, `research/codec/P2-BENCH` | PR #7 (ancestor), PR #6 base, PR #8/#9 content | native leaf v9/codec 15 (same as RC3); typed-huf and csv-archive v7 | `tests/test_rc_options.c` (with csv cases), `tests/test_int_inverse_fast.c`, `fuzz/fuzz_nplane.c`, `kolmotest.c` additions | **KEEP** — all production features except `--csv-archive` are in RC3 by content; the research tree and RC1 receipts exist only here. Do not merge (pre-restructure `src/`, 35 workflows). |
| PR #10 `--csv-archive` sub-feature | in `d03cbd46` | | | `src/csv_archive_encoder.inc`, CLI flag, bit 64 | see section 2 | RC3 table encoder `kg_encode_table` | none (v7) | RC1 `tests/test_rc_options.c` csv cases | **TEST** — only PR #10 code feature RC3 lacks; evidence conflicts; would need re-port onto `src/` layout and an eco-regression gate. |
| PR #7 `fix/planner-record-ownership-isolated-20261112` | `df42c639` | `a64f57fd` | 170/250 | `research/codec/` (E02C..P2-E31), 35 research workflows | 26 PR comments; `research/codec/STATUS.md`, `PROVISIONAL.md`, `STACKING.md`, `production-proposals/README.md` | PR #6 | none (research containers KST1 etc. are not Kolmo frames) | research harness tests, auditors (e.g. `P2-E17-independent-audit`, 42 tests) | **KEEP** — sole copy of the codec-research evidence; fully contained in PR #10. |
| PR #5 `codex/v110-validation` | `46557bb03b0b1c1b8ffa0bf85ef1c9dc0a7c9bc7` | `87dfc86b` | 4/288 | 3 code commits (FASTA sampling, ultra table compaction, tar file-planning pages) + v1.1.0 measurements | `benchmark/frontier/v110/RESULTS.md`, `REPRODUCE.md`, `measurements.json`, `experiments/*.json`, `benchmark/frontier/v110-lz/` | none | none | code tests already in RC3 (`tests/test_cm_compact.c`, `test_tar_file.c`, `test_frontier_dna.py`) | **KEEP** — code is in RC3 (2 patch-id equal, FASTA equal except Makefile context); 40 v1.1.0 benchmark files exist only here. |
| PR #3 `codex/wide-table-fast` | `8ada3f54` | itself | 0/342 | | | | | | **HISTORICAL** — contained in RC3. |
| PR #4 `codex/lossless-frontier` | `fbed70ec` | itself | 0/339 | | | | | | **HISTORICAL** — contained. |
| PR #6 `fix/planner-record-ownership` | `a64f57fd` | itself | 0/250 | | | | | | **HISTORICAL** — contained. |
| PR #8 `feat/ultra-cm-graph-leaves` | `39463d63` | itself | 0/249 | `-x -l 2` | | | | | **HISTORICAL** — contained. |
| PR #9 `feat/native-residual-leaf` | `e31b3fe3` | itself | 0/247 | `--native-leaf` | | | | | **HISTORICAL** — contained. |

PR metadata: PRs 3-10 are open and unmerged; none has a formal review (all `reviews.json` empty); PR #6 has a
handoff comment, PR #7 26 progress comments, PR #10 two (RC1 pilot at `19665144`, Mac reconciliation at `d1af945b`).
All PR head refs equal the branch heads above.

### 3.2 Remote branches

| Item | SHA | Merge base | Ahead/behind | Files / features | Claims / evidence | Depends on | Format impact | Tests | Category |
|---|---|---|---|---|---|---|---|---|---|
| `research/krc-kbc-metal` | `62c4f07e967090b23f96b33245c244efb66174be` | `5bea3557` | 3/348 | WIP snapshot (`b3eeceb5`): KRC/KBC codecs, Metal experiment (`src/experimental/krc.metal`), `bench/krcbench.c`, cm20 build target; 163 files absent from RC3 (mostly `benchmark/audit`, `benchmark/next`, `benchmark/ratio`) | `benchmark/audit/milestone1/`, `benchmark/ratio/` | merges main | none in Kolmo frames (own KRC1 frames) | `tests/krctest.c` era | **KEEP** — KRC/KBC code is superseded by RC3's newer `src/experimental/krc*` (`a09fdd66`, from `0c6f9ee`); `krc.metal`, `krcbench.c` and benchmark evidence only here. |
| `codex/nov7-research-snapshot` | `0c6f9eed39e70ca9f09204d5be5ddbb98d1b35f5` | `fbed70ec` | 1/339 | KRC (threaded, dispatcher), FASTQ prototype wired into container, CM compact tables (`src/cm.inc`), frontier market measurements (21 files under `benchmark/frontier/` absent from RC3) | `benchmark/frontier/decision.json`, `comparison.html`, `whole-silesia.png` | PR #4 | **hazard**: FASTQ as codec 12 / container v7 under `CZN_FRONTIER_FASTQ` (both now mean other things) | `tests/frontier_reads.c`, `tests/krc_frontier.c` (same in RC3) | **KEEP** — code was brought into RC3 by `a09fdd66` (FASTQ kept outside the container); market measurements only here. |
| `claude/engine-exp` | `b5e80329fe0bbdbe32acb498c12a0e543bfbd8d4` | `3c6890a9` | 3/254 | `research/engine/` (9 files): context coding of zstd literals/sequences, decimal mapping of doubles | `research/engine/README.md`, `exp1_*.csv` (adaptive mix −10.4% vs zstd-3 at ~8x decode; static −2.8..−3.9%) | none | none | none | **KEEP** — research only, not in RC3; README says "this line stops". |
| `claude/rc2-fuzz-20261115` | `bbc65dc1bf1e8cbcaad0838ceb57782f19de2b14` | `fd66a97e` | 4/206 | `fuzz/campaign/` (3849 files: harness scripts, corpora, 6 timeout units); `fuzz_dict.c`, `fuzz_stream.c` differ from RC3 | `fuzz/campaign/RESULTS.md`: 0 crashes/leaks/OOM in ~162 M execs; 6 timeouts = slow valid `-x` frames | rc2 line | none | libFuzzer targets | **KEEP** — campaign 1 corpora/results only here (RC3 has `fuzz/campaign3`). Corpora could seed future runs. |
| `claude/rc2-fuzz2-20261116` | `c39c72acf765bcef0eb4abbcbc3b16233c39ec0a` | `9d4496a6` | 4/202 | `fuzz/campaign2/` (4462 files), `KOLMO_FUZZ_NO_CM`, `craft_tbl3.c`, seed generators, 2 findings files | `fuzz/campaign2/RESULTS.md`: 0 crashes; 1 finding: unbounded allocation of claimed original size in `kolmo_stream_decompress` (`src/stream.inc:108`) | rc2 | none | 14 targets | **KEEP** — finding is fixed in RC3 (CHANGELOG.md:45 "8 GiB stream case ... refused without any limit"); `KOLMO_FUZZ_NO_CM` is in RC3; campaign-2 corpora and repro files (`findings/*.czns`) only here — useful regression fixtures. |
| `claude/rc2-linux-results-20261115` | `d4156cd94035d07e4250ead73823eabc8cd5ee96` | `94b11326` | 5/207 | `benchmark/rc2/linux/` (50 files: A1, A1_ctl, C, logs, scripts) | `benchmark/rc2/linux/*` | rc2 | none | n/a | **KEEP** — rc2 Linux x86-64 results exist only here. |
| `claude/nice-faraday-7c4fjq` | `fbe17aa070b465f21efd659e1430247e0652ddd9` | `3c6890a9` | 1/254 | table transform slack `TBL_SLACK = block/8 + 16 KiB`; group column accepted if it takes >1 value | commit message: countries-aggregated balanced 4.16->15.94; 407 identical/44 smaller/0 larger over 41x11 | main | **hazard**: writes such frames as **container v9** (RC3: v9 = native leaf; table slack is v11) | `kolmotest.c` | **HISTORICAL** — same change is in RC3 as format-2/v11 (`src/core/blocks.c:54,137` `TBL_SLACK`; SPEC.md:81-84; CHANGELOG.md:192 same figures). Keep ref; never ship its build. |
| `claude/stack-audit-20261116` | `54aa4eb8cc1c54fa2927c7362ccdc8c45e916fd6` | `9d4496a6` | 3/202 | aligned Huffman/rANS chunks (opt-in), singleton rANS fill, `research/STACK-AUDIT.md`, `research/stack-audit/` | `research/STACK-AUDIT.md` | rc2 | none (opt-in, same frames for decoder change) | stackbench | **HISTORICAL** — all 3 commits patch-id equal in RC3 (`git cherry` "-"). |
| `claude/krans-20261116` | `e551156b1ac4283dbf2aa726d2d830b29df28e6b` | `9d4496a6` | 4/202 | `src/experimental/krans.{c,h}`, `krans_cli.c`, `krans_rivals.c`, `tests/kranstest.c`, `bench/krans/`; static interleaved rANS order 0/1, 8/16/32 states, AVX2/NEON decoders, own `KRN1` frame | `src/experimental/KRANS.md`, `bench/krans/results/` (1,260 verified runs): order-0 32-way ≈ HUF size, 1.2-1.3x faster decode on residual planes; order-1 20-25% smaller at 0.35-0.47x HUF decode | rc2 | none (standalone); a leaf use would need a new codec ID | `tests/kranstest.c` | **TEST** — not in RC3; candidate leaf coder, but its own doc says it needs a measurement inside Kolmo before any format change; patent note on adaptive variants. |
| `claude/dazzling-maxwell-p8y4u0` | `b23a5dc5d3ceb90b455d591dcb8fb8ffeee915da` | `3c6890a9` | 8/254 | GR6/GR6+ research, `src/gr6.inc`, ultra GR6+ blocks; later GR6-L3 (letters/words/phrases), Huffman final coder, option matrix | `research/FINDINGS.md` (section 9), `research/gr6/*` | main | **hazard**: `12f0c0e3` writes GR6+ as **codec 15, container v9** (RC3: codec 15 = NPLANE; GR6+ = codec 26, v11) | `kolmotest.c` | **KEEP** — GR6+ and the 6-bit study are in RC3 (`src/codecs/gr6.c`, `research/sixbit-study/`, 13 blob-identical files); GR6-L3 (`gr6l3.c`, `huffmt.c`, `l3_research.py`, `matrix_tables.py`, `bench_matrix.py`) and newer `FINDINGS.md` only here. Relation to W3 `rc3w3-phrase` to be checked by that reviewer. |

### 3.3 Local-only refs

| Item | SHA | Merge base | Ahead/behind | Files / features | Claims / evidence | Depends on | Format impact | Tests | Category |
|---|---|---|---|---|---|---|---|---|---|
| `refs/review-local/heads/codex/independent-speed-ultra-memory` | `2c3539177b480c16a3362a14bdb70c7c7e637b3f` | `31ce410e` | 9/351 | pre-rename (`czn.c`) snapshot: decode limits API, KBC/KBX codecs, KRC + Metal, `tests/cznparallel.c`, `cznresource.c`; 1.0 interoperability, holdout, ratio, evolve benchmarks (316 files) | `benchmark/holdout/RESULTS.md` ("failed three-dataset resource gate"), `benchmark/comparison/`, `benchmark/evolve/` | none | none new (legacy CZN names) | `kbctest.c`, `kbxtest.c`, `kbxdivision.c`, `krctest.c` (blob-identical in RC3) | **KEEP** — KBC/KBX files are blob-identical in RC3 (via `a09fdd66`); decode limits exist in RC3 as `kolmo_decompress_limited`; benchmark evidence only here; local-only (not on GitHub). |
| reflog `9ae1e6bf` | `9ae1e6bf947eec43707ab4e6654b931dff1e38a7` | `31ce410e` | 1/351 | same content as `e990f12e` (first commit of the branch above) except `.gitignore` and three `benchmark/*/data` entries | — | — | none | — | **HISTORICAL** — 12 s earlier amend of `e990f12e`; superseded by it. |
| reflog `dcd3e14e` | `dcd3e14ef16d38aa2c639b05aa0828a6ce3e04e7` | `1323e47a` | 1/356 | `wheels.yml`: macOS wheels on `macos-15-intel`; homebrew desc | — | — | none | CI only | **TEST** — homebrew desc already in RC3; RC3 instead comments the macOS wheel job out ("macos-13 sits queued"). Re-enabling on `macos-15-intel` is untested. |
| reflog `312aa919` | `312aa91955dd6472df608265d33bed4b65ad60b8` | `f005eeef` | 1/31 | merge of `rc3w2-regress` into rc3 | — | — | none | — | **HISTORICAL** — tree `08716195` identical to RC3 history merge `910e8877`. |
| reflog `c6de4369` | `c6de436989452962c07303daa875ee55d3c34ded` | `29371ed9` | 2/20 | merge of `rc3w2-verify` on `312aa919` | — | — | none | — | **HISTORICAL** — tree `99376fb3` identical to RC3 history merge `61be4724`. |
| reflog `0b7eb8e7` | `0b7eb8e7566104823e9d4b7ae1ab32bc2745296e` | `5071f388` (RC3) | 2/0 | merge of W3 `claude/rc3w3-decmap-20261118` (`59f117f1`) onto RC3: AVX2 word decoder, khuf table, KLZH loop (34 files) | decmap results on that branch | W3 decmap | decoder-only per its message | per W3 | **KEEP** — equals RC3 + W3 decmap; defer content to the W3 reviewer. |

## 4. Hazards

1. **Container v9 / codec 15 collisions.** RC3 defines v9 = native plane-context leaf, codec 15 = NPLANE
   (`src/core/common.h:14`, `src/codecs/codec.h:6`). Two older branches give the same numbers other meanings:
   `claude/nice-faraday-7c4fjq` (v9 = table slack) and `claude/dazzling-maxwell-p8y4u0` `12f0c0e3` (codec 15 / v9 =
   GR6+). Any file written by builds of those branches is not a valid RC3 file even though the header looks
   readable; RC3 would either reject it or try to decode codec 15 as NPLANE. Do not merge or cherry-pick their
   `src/` commits; RC3 already has both features under v11 / codec 26.
2. **Codec 12 / v7 collision** in `codex/nov7-research-snapshot` (`CZN_FRONTIER_FASTQ` build only). RC3 uses 12 for
   the graph block and v7 for graph frames; already noted and avoided by `a09fdd66`.
3. **PR #10 is not mergeable as is**: pre-restructure `src/*.inc` layout, 35 research workflows that download data on
   networked runners, and `integration/rc1/import/payload/*.b64` — base64 source blobs applied by `apply.py`. Any
   reuse should go through the already-ported RC3 code; if the payload is ever used, verify it against
   `integration/rc1/import/EXPECTED_FILES.json` first.
4. **Preservation**: `research/codec/` (PR #7/#10), fuzz campaigns 1-2 corpora and findings, rc2 Linux results,
   v1.1.0 measurements (PR #5), krc-kbc-metal and independent-speed-ultra-memory benchmarks, engine experiments,
   GR6-L3 and KRANS exist only on these refs (one of them local-only). None of these refs should be deleted.
5. `git cherry` says nothing of PR #10 is in RC3; by content nearly all of it is. Patch-id tools alone would
   overstate what is missing.
6. The review clone has an untracked `benchmark/review_rc3/` directory (not created by this analysis).

## 5. Branches already contained in RC3 (one line each)

All have ahead = 0 against RC3: **HISTORICAL/KEEP** (merged; keep the ref as a record).

- `main` `3c6890a9` — ancestor of RC3 (local `heads/main` `43345ea1` also contained).
- `claude/release-rc3-20261116`, `claude/rc3w2-integration` (remote), `review/rc3-audit-review-*` — `5071f388`, RC3 itself.
- `claude/release-rc2-20261115` `b2f0c003` — rc2 release candidate.
- `claude/simd-dispatch-20261116` `22c99281` — SIMD dispatch (rc2).
- `claude/dec64` `5069da45` — DEC64 sampling.
- `claude/graph`, `claude/rename` `87dfc86b` — 1.0-1.1 M5 benchmark, rename.
- `claude/next` `67a022f4` — line classes.
- `claude/perf` `0ef26e96` — lzbench ladder.
- `claude/kaud-20261116` `3d4b7bac`, `claude/kbin-20261116` `7726df94`, `claude/kfam-fastq-20261116` `d689b103` — experimental codecs and write-ups.
- `claude/rc3-structure-20261116` `b43f7cc6`, `rc3-deps` `5b4717ef`, `rc3-features-a` `c451509b`, `rc3-features-b` `95fcc80d`, `rc3-format2` `e94f4b9e`, `rc3-gpu` `22214f73`, `rc3-bwt` `296357a0`, `rc3-simd-arm` `c12a35af`, `rc3-simd-x86` `1d8c60ce`, `rc3-xmode` `836766ce`, `rc3-gr6` `708dd6ed`, `rc3-jsondigits` `70824752`, `rc3-tablefix` `10af4da0`, `rc3-jsonlog-fix` `d8f4ed88` — rc3 wave-1 lanes.
- `claude/rc3w2-gpu2` `4550ab16`, `-memdyn` `05f7db6d`, `-meta` `8cb3c688`, `-native` `5c82c613`, `-regress` `f005eeef`, `-router` `0d6ae368`, `-twopass` `0620c5e7`, `-verify` `29371ed9` — rc3 wave-2 lanes.
- `codex/fast-file-memory` `27febccf` (PR #2, merged), `codex/release-compatibility-and-holdout` `61dcf9ac` (PR #1, merged), `codex/wide-table-fast` (PR #3), `codex/lossless-frontier` (PR #4), `fix/planner-record-ownership` (PR #6), `feat/ultra-cm-graph-leaves` (PR #8), `feat/native-residual-leaf` (PR #9) — contained.
- Local: `heads/claude/integration-20261113` `a6f4eaea`, `heads/claude/release-rc1-20261114` / `worktree-head/rc` `ea350b02`, `worktree-head/graph` `9209f571`, `rc2m` `7144a052`, `wt-bisect` `4fcabc18`, `wt-memdyn` `44547ae2`, `wt-router` `0d6ae368`, `tfix` `10af4da0`, `agent-*` worktree heads (`d689b103`, `54aa4eb8`*, `22214f73`, `7726df94`, `4550ab16`, `708dd6ed`, `c12a35af`) — contained (*`54aa4eb8` = stack-audit: not an ancestor, but all 3 commits patch-equal, see 3.2).
