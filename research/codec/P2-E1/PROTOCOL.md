# P2-E1 — first complete-pipeline measurements under the competitor-bounded policy (development)

Status: **development plumbing** (NEXT_TASK.md steps 3 and 5, first part). All inputs here were observed before;
nothing in this directory can establish P2 acceptance, and `policy.py` is required to refuse it.

## Question

Under P2 (`benchmark/accept/ACTIVE_POLICY.json`, `competitor-policy-v2/DECISION.md`), what do complete Kolmo
pipelines look like against the designated rivals on the same in-memory boundary? In particular, does the E02G
stacked pipeline (per-plane residual leaves + sparse-leaf guard) stay inside the zstd-3 speed budget once its
confirmed decode cost (E02G-verify) is charged, and is it smaller?

## Comparison contract (this file's SHA-256 is `comparison_contract_sha256`)

- Boundary: original bytes in memory -> stored bytes in memory -> original bytes in memory. File I/O and process
  start-up are excluded for every tool alike. Output buffers are allocated inside the timed encode.
- One thread for every tool: `kolmo_params.threads = 1`, `kolmo_decompress(..., 1)`, zstd `nbWorkers = 0`.
- Candidates and rivals are listed in `p2e1.c`. The designated rival is zstd 1.5.7 level 3 for `fast` (vendored
  copy, the one Kolmo links). For `hc` the designated rival is OpenZL; **it is not measured in this run**, so every
  hc comparison is `BLOCKED_MISSING_COMPETITOR`. zstd -19 appears only as an informative control.
- Stack candidates use an **emulated container** (`measurement_scope = complete_pipeline_emulated_container`):
  stored bytes are exact for replacing each KG_INT residual leaf with its per-plane blob, encode is charged as
  production compress + frame walk + residual capture + per-plane re-encode (an upper bound for a realizable encoder),
  decode is index/program walk + sequential decode with the replacement leaves. `policy.py` rejects this scope.
- Cost model: `policy.CostModel()` defaults, unchanged (1 Gbit/s link; archive 3.3 reads, 30 days, $0.023/GB-month,
  $(3.89/72)/vCPU-hour, no egress). CPU time is charged where it exceeds wall time by more than 3%.
- Repetition: 5 paired rounds (`pair_id` r0..r4) per input, rotated then shuffled candidate order (seed 20261119);
  each timed sample is a mean over B repetitions calibrated so a sample takes at least about 20 ms.
- Exactness: every candidate's decode is compared byte for byte with the input before timing and after every timed
  sample; stored bytes are re-derived inside every timed encode and must not drift.
- Peak memory: VmHWM of a separate process doing one encode + verified decode (input buffer included);
  registered limit 1 GiB.
- Typing: production Kolmo performs its own internal analysis and consumes no external typed descriptor, so
  `typing_precedes_planning` is recorded as **false** for `kolmo_*`/`stack_*`. The `typed_*` candidates parse the
  input's own NumPy/WAV declaration inside the timed encode before any graph choice (true); undeclared inputs are
  opaque (zstd at the mode level). Their container (flag, length, checksum, payload) is a research container
  (`complete_pipeline_research_container`), which `policy.py` also rejects.
- Candidate set (extended during this development run, before any fresh data): every speed setting
  (`speed`=rt+minimal analysis, `rt`, `fast` with full/quick/minimal/no analysis), the per-plane stack on each fast
  analysis level, the archive settings (`eco`, `hc`, `max`), and the type-first pipelines with and without the
  per-plane stack. All share the zstd-3 rival in the speed modes.

## Data

The E02C development corpus (`E02C-harness/corpus/manifest.json`, 20 inputs, hashes checked). Previously observed
validation downloads (E02E/E02F/E02G) are development for any new rule and may be added in a later run.

## Outputs

`results/*.jsonl` raw records, `results/p2e1.json` (policy documents' verdicts + development feasibility),
`RESULTS.md`. No threshold is tuned on these outputs; they decide what a later preregistered, source-disjoint
validation with a pinned OpenZL must measure.
