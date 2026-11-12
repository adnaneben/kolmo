# E02C baseline and scope

## Frozen reference

Repository: adnaneben/kolmo. Only `fix/planner-record-ownership-isolated-20261112` is writable for this continuation. PR #7 remains draft/unmerged. Do not move or modify any other branch or PR #6.

- Validated repaired reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`.
- Original leak-reproduction reference: `3c6890a91314b4d71db1911043ec19b87c16a86d`.
- Repaired core blob: `e06467c8872d494b97270811bedf6fd0bac2aa31`.
- Original graph blob: `91ad067dc583b9d9e6f060f214a0f7c402bb4a31`.

These are different baselines for different questions. The repaired reference is the comparator for compression research. Do not apply the ownership patch again.

## Existing evidence observed on 12 November 2026

GitHub reports completed/success for the following runs on the validated reference:

| Run | Meaning |
|---|---|
| 43187874283 | Dedicated focused ownership/identity, original full-main expected-leak reproduction, and repaired full-main sanitizer jobs passed |
| 43187874421 | Ordinary CI passed |
| 43187874238 | Wheels workflow passed |

The original full-main job succeeds by reproducing its expected leak; it does not establish that original code is leak-free. The focused validator requires 36 matching plan cases, 45 identical-frame pairs and 90 cross-decodes. These checks establish repair correctness, NOT improved compression or M5 release acceptance.

Artifact identifiers from the dedicated run: focused 13929272548; main-baseline 13933582138; main-candidate 13932569145. The research workflow will retrieve these existing artifacts and verify their status identities, recorded command-log hashes and identity-frame hashes. This record does not claim that artifact download/hash verification has already been executed in the current research environment.

## E02C research question

Hold the numeric predictor/GCD and input boundaries fixed. Compare original residual layout, global/local bitpacking, patched packing and a matched local byte-plane control under stored, fixed existing Huffman, fixed existing rANS and fixed zstd-1 coding.

The target is complete stored size AND useful encode/decode/memory trade-offs. A smaller intermediate buffer is not proof of a smaller compressed file. Earlier E02B development fixtures showed global packing 35.71% smaller than stored residuals but 2.92% larger with a fixed zstd backend; patched packing with zstd was 7.28% larger. Preserve that negative result. It was not a complete Kolmo benchmark.

## Guardrails

No production codec, default, API, operation ID, CZN1/CZNS format or release threshold changes. Research adapters and generated build files are isolated from src/. Use complete hash-verified production source, not manually copied predictor excerpts. Explicit numeric interpretation and research framing must not be represented as automatic production-planner integration. Measure ordinary Kolmo separately.

All benchmark inputs are DEVELOPMENT data. Independent validation, final holdout, M5 acceptance and production integration remain separate gates. Keep the existing `benchmark/accept/TARGETS.md` authoritative for releases.

## Current status

Baseline correctness gate: passed on the frozen reference above.
E02C production-connected experiment: implementation in progress; no compression result or experiment pass is claimed by this baseline record.
