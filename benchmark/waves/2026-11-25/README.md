# Kolmo review and benchmark handoff

25 November 2026. Continue on a fresh topic branch. Review each draft on its own and keep the two measured integration commits available for reproduction.

| Build | Source | Role |
| --- | --- | --- |
| Pinned main | [4db0425b](https://github.com/adnaneben/kolmo/commit/4db0425b361012523d8074ca6f2a760d9466e9f4) | Paired Wave 1 baseline and includes Claude's already merged work through #48 |
| Wave 1 | [f5822f30](https://github.com/adnaneben/kolmo/commit/f5822f303d74d3ed7a8c042c5fb74688d95e27ed), #58 | Integrates #49–#56 with pinned main |
| Wave 2 | [dcbf4831](https://github.com/adnaneben/kolmo/commit/dcbf483184e77e465c02e33bd1aae674ca26b5bc), #73 | Integrates reviewed decoder, archive, release, catalogue and optional performance lanes |
| Stock compatibility | [v0.0.2](https://github.com/adnaneben/kolmo/releases/tag/v0.0.2) | Separate ordinary v12 decoder for scoped frames |

## Implementation review order

1. Review #72 on top of #58. Confirm the declaration-free legacy guard and document caller-owned buffers, FD staging and stream allocations. Establish exact-cap compatibility above and below the metadata thresholds. Fix owned compressed-input budgets, video phase peaks and graph intermediate allocation accounting in separate branches. Add site-specific failure tests before claiming a comprehensive memory cap.
2. Review #59 for per-member atomic extraction, then the separately tightened regular-output publisher. Exercise valid directory metadata, archive hard links, ordinary refusal and I/O failures. Keep whole-archive transactionality as a distinct behavior.
3. Review #60 with GitHub release maintainers. Run draft verification on actual packaged macOS and Linux outputs and clean wheel/sdist installs, using the oldest supported runtime where available. Produce the real provenance and cross-check downloads before any publish step. #47 remains the independent v0.0.3 release preparation.
4. Review #61 as an encoder-only prototype. Freeze source-family manifests independently of algorithm tuning, validate real held-out outputs with stock v12, sign or pin the accepted policy digest, and atomically select a previous accepted ID for rollback. Grow catalogue entries with representative positive and negative controls. No managed policy is promoted by this benchmark.
5. Review #62 and #63 as opt-in candidates. Keep measured wins available for each validated scope. Require independent producer groups and per-scope gates before setting a default. Treat ARM64 sectionless ELF and tiny KLZ follow-ups as separate experiments from the two measured integrations.
6. Rebase the selective idea from #37 against the elastic entry point. The earlier branch reduced aggregate level-7 bytes by 0.183% at roughly 2.1× encode CPU. Keep a fixed-budget runner-up for workload strata that replicate, and reuse staging memory. #47 release prep already matched pinned main frame bytes in the previous audit.
7. Run the specialized rivals under identical timing boundaries. Test training-free OpenZL integer Transformer at 6/7/9 and ZipNN's supported dtype paths. Track code, scientific arrays, short records, large weights, KV-cache and first-recovered-byte latency independently. Same input bytes, v12 verification, seven or more screening pairs, a second sealed corpus and deployment CPU families are promotion prerequisites.

## Evidence

- [Results](results.md) with paired phase timings, sizes, memory and the scope decisions.
- [Acceptance](acceptance.md) with separate workload targets and promotion rules.
- [Review ledger](reviews.md) with the independent assignments and open research lanes.
- Compressed JSONL receipts and matching binary/input manifests beside this file. Receipts contain no source bytes.
- Replay tools `replay.py`, `scoped-replay.py`, `score.py` and native CM4 microbench harness. The portable replay takes a preserved evidence folder with `impact/corpus/holdout`, `corpus-holdout` and `mkt/data/silesia.tar`. Input basenames and digests are in the manifests. Use the exact bytes listed there.
- [GitHub tracker #57](https://github.com/adnaneben/kolmo/issues/57) links all drafts and updates.

Keep the chronological main, Wave 1 and Wave 2 comparisons paired by session. Repeat any borderline differences on an isolated host before changing a preset or publishing an improvement claim. Published competitor sizes and previous branch timings should keep their original compiler, corpus, license and hardware metadata.

## Rebuild the tables

Expand each gzip receipt in a temporary copy of this folder. Run `python3 score.py wave1.jsonl > wave1-score.json` and repeat for Wave 2. Then run `python3 summarize.py`. The Decisions section in the published report records the review disposition.
