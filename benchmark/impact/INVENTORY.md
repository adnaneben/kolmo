# Inventory: every codec, transform and model, and where it runs (origin/main 896dc6d6)

Compiled from FORMAT.md (§6.1, §8.1), `src/api/api.c` (level table), `src/core/blocks.c` (`compress_worker`),
`src/plan/*.c`, the unmerged remote branches and `$V/evidence/FEATURE_DECISIONS.md` / `$V/review40b/SYNTHESIS.md`.
Line numbers are origin/main's. The column "matrix" says which `run.py` feature measures it (RESULTS.md).

Levels: L1 RT/KLZ, minimal analysis (shuffle only); L2 FAST KLZH2 0, minimal; L3 FAST KLZH2 3, full analysis;
L4/6/7 HC (KLZH2 5, 8, 9); L5 ECO (KLZH2 7); L8 MAX level 1 (BWT every block); L9 MAX (BWT text, KLZMA/BWT binary);
U ULTRA (CM model 4 with masked contexts).

| feature | id | status | where it runs today | earlier measurement | earlier verdict | matrix |
|---|---|---|---|---|---|---|
| KLZ | codec 28 | main | L1 | — | — | base |
| KLZH2 | codec 32 | main | L2-7 | — | — | base, klzh2-9 |
| KLZH | codec 29 | read only | — | — | — | — |
| KLZMA | codec 24 | main | L9 binary; ultra alternative trial; graph leaves at max | — | — | klzma, stack-klzma |
| BWT | codec 18 | main | L8 every block; L9 text, JSON tokens, binary when ≥3% smaller than KLZMA (slice gates); ultra alternative; eco text→BWT off (`tunables.h:15`, A6(a)) | rc3/w4 bwt, VM; M5 T1 | W4 bwt adopted (decode RSS −6 to −20%) | bwt, stack-bwt |
| CM model 4 (masked contexts) | codec 7 | main | ultra, `cm4_mask_for` text/bin (`cm.c:1206`) | w4/cm VM T1; M5 T1 | adopted for -x (C2): +0.28% vs model 3, 2× faster | cm, stack-cm |
| CM shared prior + race | codec 7 tune | main | ultra | rc3w2/twopass VM | merged | — |
| lexer token-class contexts + per-kind priors | codec 7 | **unmerged** `claude/rc3w3-prior-20261118` | — | VM `-x -T1` only, 10 KB-10 MB | DEFER (id collision), superseded by W4 cm; apache log 10 KB −14.7% | lex, prior, lex-prior, lex-stream (ported onto model 4) |
| CMI image model | codec 17 | main | ultra, 8-bit image graph plan | not found | "12-20% smaller than graph on photos" (comment) | base (U) |
| CMP (CM over parts) | codec 25 | main | ultra: table columns; non-text ≥32 MiB halves | rc3/xmode VM T1/T4 | halves +0.14% binary | base (U) |
| GR6+ | codec 26 | main | **ultra only**, `gr6_applies` (`blocks.c:524`) | **M5 `-x -T1` only** | −3.1% covid, −3.7% owid, others 0 | gr6, gr6-nogate |
| GR6 lzp phrases / race | — | rejected | — | VM T1 | do not port | — |
| phrase decisions | — | rejected (study) | — | VM | <0.5% on every group | — |
| FASTQ family | codec 16 | main | ultra (not -l1), `kf_fastq_like` | kfam, machine not found | −25% vs rc2 -x | base (U); not forceable below ultra (needs block-level split; follow-up) |
| audio (kaud) | codec 19 | main | L2-U, WAV/AIFF | rc3/format2 all modes | 43/43 WAV smaller | base |
| video (kvid) | codec 20 | main | L2-U, whole Y4M | rc3/format2 | fast −17.4% | base |
| kimg | codec 31 | main | L8, L9, U (image header, ≥128 KiB) | not found | ~14% vs rc3 | base; forcing at L3-7 needs the image geometry (follow-up) |
| graph | codec 12 / plan 9 | main | planner L3-U; L1-2 shuffle only | many | — | notf, tf-full |
| rANS o0 leaf | codec 14 | main | graph leaves L4-U | — | — | — |
| khuf | codec 27 | main | graph leaves, KLZH2 streams | — | khuf v2 DEFER | — |
| NPLANE | codec 15 | opt-in `--native-leaf` | FAST only | level 3 | 5-12% smaller int series, decode 1.5-2.6× slower | — |
| planes / zrun leaves | 21 / 23 | main | graph leaves | format2 | — | — |
| `--typed-huf` | ktf route | opt-in | L3, T1, ≤16 MiB | L3 T1 | faster, sometimes larger | — |
| column-split tables | tflag 13 | opt-in (index) | tflag-4 tables, not RT | rc3/features-b VM | — | — |
| typed transpose | plan 1 | main | L3-U (ultra after the CM sample) | — | — | notf, stack-* |
| shuffle | plan 2 | main | L1 (gate 0.75), L2 (0.93), L3+ trials | — | — | notf, stack-* |
| table v2 | plan 3 tflag 4 | main | L3-U text | — | SYNTHESIS #17: pre-empts log trial, BGL +152% at -3 | notf, stack-*, gr6 |
| log templates / minified JSON | plan 4 tflags 5/10 | main | L3-U | — | JSON tokens → BWT at L9 (+10%) | notf, stack-*, lex-stream |
| img16 | plan 5 | main | L3-U | — | — | notf |
| records | plan 6 | main | L3-U | — | SYNTHESIS #18: Arrow +65% at -9 | notf |
| x86 E8/E9 | plan 7 | main | L3-U | — | SYNTHESIS #27: <1 MiB binaries never get it | notf, stack-* |
| word dictionary | plan 8 | main | L4-U effort modes; dropped for ultra and max text | — | — | notf, stack-bwt |
| line classes | plan 10 | main | **L3 only** (`planner.c:432`) | not found | — | notf |
| ARM64 branch filter | plan 11 | main | L2-U (not RT) | rc3w2/meta VM T4 | BWT on tflag 14 "unmeasured" (`blocks.c` comment) | notf, stack-bwt |
| frontier codecs | ids 8-11 | `-DKOLMO_FRONTIER` only | research builds | codex v110 M5 | outside the format | — |
| KRC/KBC/KBX/kbin/KAUD CLIs | standalone | `src/experimental` | never in the library | M5, lzbench | KRC not promoted; kbin −1.5 to −19% vs pcodec | — (own frames) |
| W4 entropy (khuf v2, QLFC) | 27/29 reuse | unmerged `claude/w4-entropy-20261118` | — | VM; M5 T1 | DEFER +0.02% | — |
| W4 KLZ four-stream | 28 reuse | unmerged `claude/w4-lz-20261118` | — | VM; M5 T1 | DEFER, −1.31% larger | — |
| krans order-1 / auto | — | unmerged `claude/krans-20261116` | — | Silesia, clang/gcc | o1 −24.65% vs huf | — |
| R02 ctx-rANS leaf, img16 block | 33 / 34 | patches on `claude/review40-integration-20261123` | — | VM | 17 PNM: L5 −15.2% | — (patches do not apply to main) |
| R01 tANS codec-32 design | — | same | — | VM T1/T4 | decode 0.86-1.01×, not met | — |
| KLZH2 rep0 gap, bf16 parse | encoder | unmerged `review/klzh2-*` | — | not found | bf16 −2.9% | — |
| engine-exp context-split literals | — | unmerged `claude/engine-exp` | — | VM T1 | −2.8% vs zstd -3, +30% decode | — |

## Never measured outside one level or one thread count (before this matrix)

GR6+ (only `-x -T1`, M5); CMP halves (-x, VM); CMI (ultra only, no results file); line classes (L3 only, no
measurement); `--typed-huf` / `--native-leaf` (L3 T1); the rc3w3 priors and lexer (-x only, VM); FASTQ family (ultra
only; use at -9 proposed by SYNTHESIS #132, unmeasured); kimg (L8-U; the L3-7 colour node is a proposal); BWT on ARM64
blocks (unmeasured); eco text→BWT (one VM T1 run + holdout, now off); W4 entropy / W4 lz / R02 / krans o1 /
engine-exp (VM numbers only).

Not forced by this matrix (they need block geometry the harness does not have, or a split of their own): FASTQ family
below ultra, kimg / CMI below level 8, kaud/kvid (already at every level from 2), NPLANE, the standalone research codecs,
and the unmerged patches that reuse codec ids. Listed as follow-ups in RESULTS.md.
