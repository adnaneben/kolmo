# C2-cm validation (2026-11-20/21) — branch review/c2-cm-J62kQD

Commits on top of C1 302e31a6:
- 0e7321f3: merge of W4 cm ca81739e; CHANGELOG kept both sides.
- 4e5601b1: SPEC.md model-nibble registry; test_cm4 reserved-nibble tests.
- a555c05d: test_cmpar table expectation.

Binaries tested: validate-portable-c2cm (portable, ARCH=) and validate-c-reference-c2cm (NO_ASM=1), both built at 4e5601b1.
a555c05d changes only a test, and check-rc was rerun at a555c05d.

- **Phase E** (logs/phase-e-c2cm/, summary logs/phase-e-c2cm.summary): every step exits 0 except check-rc at 4e5601b1.
  That failure is test_cmpar "table: codec 25 not chosen", which also fails on unmodified ca81739e
  (logs/c2-cm/w4cm-baseline-cmpar-build.log). check-rc was rerun at a555c05d and passes
  (logs/phase-e-c2cm/*check-rc-rerun*).
  - Also exit 0: check-sanitizers, experimental, bindings (Python, Rust, source diffs), C-reference dispatch and asm,
    and determinism in both builds.
- **test_cm4** (ASan): all checks pass, including the new reserved-nibble checks.
- **Compatibility matrix** (evidence/compat-matrix-c2cm, spec evidence/config/compat-spec-c2cm.json):
  - 11,928 decodes: 9,950 exact, 1,978 rejected, 0 silent misdecodes, 0 crashes.
  - Versus C1: C2-cm's decode outcome matches C1 on all 852 shared frames except 23. All 23 are model-4 frames
    (from W4 cm or C2-cm -x) that C1 refuses and C2-cm decodes exactly.
  - Frames: C2-cm's non-x frames are identical to C1 (104/120). Its -x frames are byte-identical to W4 cm's (24/24).
- **Model-nibble probes** (tools/cm_xprobe.py):
  - C2-cm -x, 120 inputs: 99 frames with nibble 9, 1 basic CM, 20 with no CM block. Every older decoder refuses every
    nibble-9 frame: rc3, C1, v1.0.1, 1.1.0, rc2 and W3 prior. Only W4 cm and C2-cm decode them, exactly.
    Results in evidence/cm-xprobe-c2cm-x.
  - C2-cm -x -l 2, 40 inputs: same pattern (evidence/cm-xprobe-c2cm-xl2).
  - W3 prior -x, 120 inputs, decoded by C2-cm and rc3: nibble-14 frames decode exactly. Every nibble-15 frame (W3
    prior's bit-7 model) is refused by both, as are 4 frames classified as basic CM.
    Results in evidence/cm-xprobe-w3prior-x-to-c2cm.
  - Totals across all probes: 0 silent misdecodes, 0 crashes, 0 timeouts.
- **Not done:** timed benchmarks (left to the main session); CI (runs on push).
