# 20261113T021934Z — bounded P2-E17 evidence-regression continuation (local unpublished)

Status: **IMPLEMENTED_LOCAL / EXACT_SOURCE_TEST_PASS / REAL_ARTIFACT_AUDIT_PASS /
REMOTE_COMMIT_BLOCKED_READ_ONLY_TOOLING**. This receipt is supplied as an additive
patch, not claimed to exist in the remote repository.

## Source, instructions and collaboration

Initial actual HEAD: `da639cb5b5ca1bec33b33aff3c191d4c15657175`.
Authorized branch only: `fix/planner-record-ownership-isolated-20261112`.
PR #7 was verified draft and unmerged. Read AGENTS.md, CLAUDE.md,
benchmark/accept/ACTIVE_POLICY.json, overnight PLAN.md, P2 DECISION/NEXT_TASK,
relevant N1/E17/E18/close iteration records, P2 summary, executed E17 protocol,
runner, N1 harness, workflow and latest PR discussion. Not a full repository clone.

Remote HEAD advanced during local work: `b01b15d4e2ccf6178fa5cac7b63e99386bf6b399`
adds Claude's P2-E20 preregistration after an independent E17 audit and R4 claim;
`0fb224702feacca1f2ea868e4848d258f2963682` adds a wire review. Those paths were read
as needed and left untouched. The separate audit is already committed under
P2-E17-independent-audit; this patch's new P2-E17-audit path is regression/evidence
hardening, not a claim of first discovery, a replacement, or a new compression win.
The E20 claim records newer user approval for fresh sources, wire review and
P2-D1 testing. No fresh validation data or results were inspected/tuned here.

## Actions inspection and budget

Initial inspection found ordinary CI/validation active, no queued overnight
experiment and completed E17/E18. Later the independent E20 run `43350744793`
(job `131640934955`) was in progress, with rival build active and fresh-data,
sanitizer and measurement stages pending. It was not duplicated or cancelled.
No new workflow was launched or configured. New experiment runner consumption: 0.
Last earlier estimate: approximately 176/480 minutes consumed; Claude reserves
up to 110 for E20, leaving approximately 194 unreserved. This is unreconciled
planning arithmetic, not verified aggregate runner billing; other CI consumption
is not included in that historical estimate. No further reservation made.
No new night, automation, resource provisioning, release or secrets operation.

## Executed bounded step

Local source: `research/codec/P2-E17-audit/audit_e17.py` and
`test_audit_e17.py`; exact hashes in TEST_RECEIPT.json.
Downloaded actual completed E17 artifact `14001980117`, run `43341163635`,
experiment source `f4fcadcd762d7d7bc9e484a4730900eb3388d5e9`.
ZIP SHA256: `f2e4c589b9e77c3c1cce609f3df120d6a6fc0be6033cc18f2a3cf8ec00669e1a`.

Commands from the partial staging-tree root:

```sh
python -m unittest discover -s research/codec/P2-E17-audit -p 'test_*.py' -v \
  > research/codec/P2-E17-audit/TEST_LOG.txt 2>&1
/usr/bin/time -f 'elapsed_s=%e user_s=%U system_s=%S max_rss_kib=%M' \
  -o research/codec/P2-E17-audit/AUDIT_RESOURCES.txt \
  python research/codec/P2-E17-audit/audit_e17.py \
  /mnt/data/p2e17-cli-43341163635-1.zip \
  --output research/codec/P2-E17-audit/AUDIT_RESULT.json \
  > research/codec/P2-E17-audit/AUDIT_LOG.txt
```

PASS: 42 exact-source unit tests, 7,842 manifest files plus 61 separate input
hashes; 488 configuration identities; 5,856 events; 2,928 paired cost samples;
183 recorded sanitizer-gate cases. Joint-six-pair diagnostic retains 47/61
all-candidate strict smaller-byte opportunities, 37/61 production-only and
43/61 research-only. These are offline diagnostic alternatives, not additive
improvements or an implemented/timed planner. Historical verdicts are intact.

FAILED then resolved locally: initial audit's 64 MiB member cap rejected the
82,741,337-byte CSV; raised only the read-only auditor cap to 128 MiB and reran.
Earlier 41-test iterations passed; added the incomplete-pair rejection test
and reran the final 42-test source. No old test count is borrowed as a new pass.
Read-only access attempts: guessed workflow path p2-e17.yml returned 404; fetched
actual p2e17-cli.yml from run metadata. Workflow-collection URL was rejected by
the connector; permitted run/job endpoints supplied active status instead.
Container GitHub network lookup failed DNS; connected tools provide reads only.
No Git checkout, remote commit, push or dispatch success is claimed.

NOT RUN: native header-separated OpenZL measurements, independent re-decode
(frames/binaries absent), new sanitizer execution, holdout validation, ML/RL,
production integration, or CI on this unpublished source. P2 acceptance remains
NOT_EVALUATED: current comparator reads header bytes as values, outer selection
is uncharged, production controls do not establish mandatory typing first,
and inputs are observed development.

## Next prerequisite

Publication needs a write-capable session: refresh actual HEAD, inspect concurrent
paths and apply only reviewed new files on the authorized branch with a normal
fast-forward. Do not overwrite the other E17 audit, R4 work, wire review or E20.
While E20 is active, no additional experiment should launch. Next substantive
research remains native header-exact matched OpenZL accounting and a realizable
fully costed selector; fresh E20 validation belongs to its preregistered owner and
must not become tuning data. Keep PR #7 draft/unmerged and leave all other refs alone.
