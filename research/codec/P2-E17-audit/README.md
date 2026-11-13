# P2-E17 evidence regression audit — local, unpublished continuation

This is a standard-library, read-only auditor of the frozen P2-E17 artifact, plus
42 deterministic regression tests. It does not encode/decode data, launch jobs,
train a selector, change production code, or issue P2 acceptance.

## Publication state and concurrency

**NOT COMMITTED OR PUSHED.** The current session exposes read-only GitHub tools;
there is no authenticated publishing/dispatch tool or usable Git CLI connection.
This directory is a partial staging tree, not a complete Git checkout.
Starting remote HEAD was `da639cb5b5ca1bec33b33aff3c191d4c15657175`.
During execution, collaborators advanced it to `b01b15d4e2ccf6178fa5cac7b63e99386bf6b399`
and then the observed `0fb224702feacca1f2ea868e4848d258f2963682`.
The new `P2-E17-independent-audit/`, R4 claim, fresh P2-E20 validation and wire
review are separate collaborator work. This patch does not replace any of them.
The substantive 47/61 finding overlaps that concurrent audit; this continuation
adds broader evidence/negative-case regression coverage and joint six-pair checks,
not a second compression experiment or an additional compression gain.

Only apply reviewed additions in a clean, refreshed checkout on
`fix/planner-record-ownership-isolated-20261112`, after checking current HEAD and
working-tree changes. Never reset a newer branch to a pin in this packet. Keep
PR #7 draft/unmerged; leave main, PR #6 and every other branch untouched. No
workflow, production code, format, default, policy or old result is modified.
Use `git apply --check` before applying the patch; preserve any newer additions
at these paths rather than overwriting them. Publication still requires a
write-capable session and normal, non-force fast-forward updates.

## Reproduction

From the repository root (or this partial staging tree):

```sh
python -m unittest discover -s research/codec/P2-E17-audit -p 'test_*.py' -v
python research/codec/P2-E17-audit/audit_e17.py /path/to/p2e17-cli-43341163635-1.zip \
  --output /tmp/p2e17-audit.json
```

Input: GitHub run `43341163635`, artifact `14001980117`,
`p2e17-cli-43341163635-1`, expires 2026-12-13 (not permanent storage).
ZIP SHA-256:
`f2e4c589b9e77c3c1cce609f3df120d6a6fc0be6033cc18f2a3cf8ec00669e1a`.
Executed experiment source: `f4fcadcd762d7d7bc9e484a4730900eb3388d5e9`.
Pinned OpenZL source recorded by its workflow:
`32246b48faee46807f84183dac4db479089f5445`.
The large artifact and source checkout are not included in this patch/bundle.
No network or codec executable is needed for the synthetic tests; the real audit
needs the exact artifact ZIP. The auditor reads ZIP members without extracting.

## What is checked

Artifact digest; complete manifest/input coverage; bounded safe ZIP paths and
member sizes; duplicate JSON and record keys; frozen configuration/command/seed
and source/binary receipts; complete input paths; wall/CPU accounting; success
and timeout statuses; raw-to-summary timing consistency; all three paired rounds
per pass; byte/frame identity across passes; medians; recorded sanitizer cases.
The joint diagnostic requires the same fixed configuration to fit all six paired
archive costs, with no zip truncation, median substitution or cherry-picked repeat.
An exact byte tie is parity, never a compression win.

Tests use clearly synthetic infrastructure fixtures. They do not stand in for
real OpenZL, Kolmo, codec malformed-input tests, sanitizer runs or fresh validation.
Full measured-pipeline acceptance is deliberately always `NOT_EVALUATED`.
See RESULTS.md, TEST_RECEIPT.json and the dated iteration record for actual
outcomes and limitations. Test/source hashes identify the exact implementation
executed locally, not an equivalent prototype or a later untested variant.
