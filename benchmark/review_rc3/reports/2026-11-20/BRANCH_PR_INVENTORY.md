# BRANCH_PR_INVENTORY — RC3 review, 2026-11-19

Baseline RC3 = `5071f388`. Raw ref table for every branch/PR/local ref: `branch-pr-raw.tsv`, `local-only-raw.tsv`.
Detailed per-item analyses (files, claims, evidence paths, format impact, tests): `W3_W4_BRANCH_ANALYSIS.md`,
`OLDER_BRANCH_PR_ANALYSIS.md`. Preservation: every ref is in the preserved bundle (`Kolmo-Preserved/.../export/history.bundle`)
and the GitHub mirror (`.../github/remote.git`), PR metadata in `.../github/prs/<n>/`. **HISTORICAL never means delete;
no branch or PR was closed, deleted or rewritten.**

Totals: 63 GitHub branches (41 fully contained in RC3, 22 not), 10 PRs (7 heads contained in RC3; #5, #7, #10 not),
2 local-only branches (`claude/rc3w2-integration` a17d6657, `codex/independent-speed-ultra-memory`), 5 reflog-only commits,
19 worktree heads (archival refs).

## Not in RC3 — decisions pending measurement (wave 3 / wave 4)

| ref | SHA | base | change | format | Mac-verified so far | category |
|---|---|---|---|---|---|---|
| a17d6657 (local) = rc3 + W3 decmap + W3 phrase | a17d6657 | 5071f388 | decoder-only speedups (khuf table, KLZH loop, x86 AVX2 word decoder); phrase study files | none (compat matrix: identical acceptance to rc3) | determinism 122/122; Mac timing: screening | TEST → see FEATURE_DECISIONS |
| W3 decmap | 59f117f1 | 58f7cee8 | as above (same commit inside W4 entropy/lz) | none | as above | via a17d6657 |
| W3 phrase | cee3b121 | 58f7cee8 | study, not adopted by its author | none | — | HISTORICAL (negative result kept) |
| W3 prior | ae00b26f | 58f7cee8 | CM priors (388 KB tables), "model 4" nibble 15+bit 7; re-implements rc3 6f8761b7 | NEW CM generation; rc3 rejects 1/9 -x frames; collides internally with W4 cm model 4 | compat only | HISTORICAL + TEST (rebase onto rc3, renumber) |
| W4 entropy | 5f24d6ae | 5071f388 | khuf v2 (package-merge, 12-bit, 4 streams), codec 30, QLFC SSE2 | **reuses codec 27 and changes 29**; default balanced output changes | determinism 122/122; compat breaks confirmed | TEST, blocked on new codec id |
| W4 lz | 3a027805 | 5071f388 | KLZ four-stream layout | **reuses codec 28** (opt-in) | compat break confirmed | TEST, blocked on new codec id |
| W4 cm | ca81739e | 5071f388 | CM model 4 for -x (nibble 9), 16-bit mixer, priors | new -x generation, old readers reject safely | determinism 122/122 | PROPOSE after Mac screening |
| W4 bwt | 5ce318ad | 5071f388 | in-place BWT/QLFC, LZP unroll, Linux huge pages | byte-identical frames claimed | determinism 122/122 | PROPOSE after Mac screening |
| W4 docs | fba28558 | 5071f388 | async decode writer even at -T1; docs | none | preflight: 2 running threads at -T1 decode | SEPARATE scoreboard; not strict T1 |
| W4 multi | fdb99760 | 5071f388 | machine-wide job pool on by default; `kolmo_params` gains a field (ABI) | none | — | SEPARATE (governor research); not on T1 board |

## Not in RC3 — older lines (from OLDER_BRANCH_PR_ANALYSIS.md)

| ref | SHA | category | reason |
|---|---|---|---|
| PR #10 integration/best-features-rc-20261113 | d1af945b | KEEP | every production feature is in RC3 by content except `--csv-archive`; pre-restructure tree + 35 workflows: do not merge |
| PR #10 `--csv-archive` | d03cbd46 | TEST | the only code feature RC3 lacks; conflicting evidence (larger than eco on 5/7 tables) |
| PR #7 planner-record-ownership-isolated | df42c639 | KEEP | sole copy of `research/codec/` evidence |
| PR #5 codex/v110-validation | 46557bb0 | KEEP | code in RC3; v1.1.0 measurements only here |
| claude/krans-20261116 | e551156b | TEST | static rANS 1.2–1.3x decode vs HUF on planes; never measured inside Kolmo; needs a new codec id |
| claude/dazzling-maxwell-p8y4u0 | b23a5dc5 | KEEP | GR6-L3 research only here; **its build writes GR6+ as codec 15/v9 — never ship** |
| claude/nice-faraday-7c4fjq | fbe17aa0 | HISTORICAL | in RC3 as v11; **its build writes v9 with another meaning — never ship** |
| codex/nov7-research-snapshot | 0c6f9eed | KEEP | market measurements only here; FASTQ codec-12/v7 build variant is a hazard |
| research/krc-kbc-metal | 62c4f07e | KEEP | Metal/krcbench evidence only here |
| claude/engine-exp | b5e80329 | KEEP | research (stopped) |
| claude/rc2-fuzz-20261115, rc2-fuzz2-20261116 | bbc65dc1, c39c72ac | KEEP | fuzz corpora/repro files only here (finding fixed in RC3) |
| claude/rc2-linux-results-20261115 | d4156cd9 | KEEP | rc2 Linux results only here |
| claude/stack-audit-20261116 | 54aa4eb8 | HISTORICAL | patch-equal in RC3 |
| codex/independent-speed-ultra-memory (local only) | 2c353917 | KEEP | benchmark evidence only here; **not on GitHub** — exists only in the user's local repo and in this review's preserved bundle |
| reflog dcd3e14e | dcd3e14e | TEST | macOS-15-intel wheel job (RC3 disables the macOS wheel job) |
| reflog 9ae1e6bf, 312aa919, c6de4369 | — | HISTORICAL | superseded / identical trees in RC3 history |
| reflog 0b7eb8e7 | 0b7eb8e7 | KEEP | = RC3 + decmap (same content as part of a17d6657) |
| PRs #3, #4, #6, #8, #9 and 41 branches | — | HISTORICAL | fully contained in RC3 |
