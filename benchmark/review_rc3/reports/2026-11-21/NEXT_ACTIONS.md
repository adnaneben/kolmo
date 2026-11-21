# NEXT_ACTIONS / handoff — RC3 review, 2026-11-20

## Paths and SHAs

| what | where |
|---|---|
| Preservation run (read-only evidence, bundle, manifests, restore drill) | `/Users/adnanbenali/Kolmo-Preserved/review-20261119T083123Z.lKHULl` |
| Review workspace (`$V`) | `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD` |
| Review branch (pushed to github) | `$V/repo`, `review/rc3-audit-review-20261119T083610Z.J62kQD`, head fb58d3d6, plus the local deliverables commit (see CHECKPOINTS) |
| Integration candidate C1 (local only) | `$V/integration-c1`, `review/rc3-integration-J62kQD`, 302e31a6 |
| C1 release binary | `$V/final-builds/c1/build/kolmo` (sha256 in `final-builds/BINARIES.sha256`) |
| Evidence | `$V/evidence/` (this folder); the logs are in `$V/logs/` |
| Holdout corpus (read-only, manifest) | `$V/corpus-holdout/` |

## Completed

- **Preservation:** done, with a restore drill.
- **Inventory:** branch and PR inventory done.
- **Claims:** ledger and errata (C6, E1–E6) done.
- **Known issues C1–C7 audited.** Fixes are on the review branch:
  - CI YAML quoting;
  - the fuzz_codecs compile fix;
  - harness corrections for C3/C4.
- **Harness:** `benchmark/review_rc3/audit.py` with self-test, inventory, preflight, run and report.
- **Validation:** Phase E on C0 and C1, native matrix, compatibility matrices and the id-collision probe.
- **Benchmarks:** branch screening (7 suites plus the decmap-native rerun) and holdout on C1 vs RC3 vs rivals. All are strict T1 and byte-verified.

## Outstanding, in order

1. **Owner approval: push `review/rc3-integration-J62kQD` (C1) so CI runs on the final code.** This also gives x86-64 coverage on ubuntu-latest.
   ```
   cd $V/integration-c1 && git push github review/rc3-integration-J62kQD
   ```
   Do not merge to main; open a PR only if the owner wants one.
2. **Fix KOLMO_NATIVE's needless container v11 bump** (finding 6) and review the `planner.c:158` memmem `-Wnonnull` warning. Rerun the compat matrix after the fix.
3. **Owner decision on W4 cm.** Choose new -x CM ids for W4 cm vs W3 prior, renumber, then rerun `compat_matrix.py` and the ultra suite. Once it is clean, mark it KEEP_OPT_IN.
4. **Domain-specialist rivals.** Install flac, ffmpeg (FFV1), cjxl, Pcodec, OpenZL and samtools/fqzcomp into a run-local prefix (commands in `tool-inventory/TOOL_INVENTORY.md`), then run domain suites with `audit.py run`.
5. **x86 and Linux measurement** of decmap AVX2 and W4 bwt huge pages. They cannot be measured on this Mac.
6. W4 entropy and W4 lz: only after new codec ids, then rescreen.

## Safe resume commands

```
V=/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD
cd $V/repo && python3 -I benchmark/review_rc3/audit.py self-test
python3 -I benchmark/review_rc3/audit.py preflight --config $V/evidence/config/holdout-config.json --suite holdout-main --out <new dir>
python3 -I benchmark/review_rc3/audit.py run --config $V/evidence/config/holdout-config.json --suite holdout-main --new-run <new dir>.noindex/holdout-main
python3 -I benchmark/review_rc3/audit.py report --run $V/evidence/holdout.noindex/holdout-main --out <new dir> --compare a:b ...
```

Always write to a **new** output directory; never reuse an existing run directory.

## Restoring preserved material

See `PRESERVATION_RECEIPT.md`, section "Restore commands". The full history, including archival refs, comes from:

```
git clone --mirror /Users/adnanbenali/Kolmo-Preserved/review-20261119T083123Z.lKHULl/export/history.bundle NEW.git
```

Nothing in the original checkouts, branches, PRs, stashes or reports was deleted or overwritten. Superseded screening runs (screening, -2, -3) are retained.
