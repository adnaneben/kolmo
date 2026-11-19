# PRESERVATION_RECEIPT — Kolmo RC3 review, 2026-11-19

Run directory: `/Users/adnanbenali/Kolmo-Preserved/review-20261119T083123Z.lKHULl` (outside every source root; realpath guard passed).
Review workspace: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD` (clone of the bundle; no alternates; origin = the bundle).
Machine: Apple M5 25769803776 10 4 6  | ProductName:		macOS ProductVersion:		27.2 BuildVersion:		26B123  | git version 2.54.0 (Apple Git-157)
Runbook copy: inventory/runbook.txt (sha256 f66ee980…).

## Sources (root-map.tsv)
- worktree-000 ← `/Users/adnanbenali/Projects/kolmo`
- git-common-000 ← `/Users/adnanbenali/Projects/kolmo/.git`
- scratch-001 ← `/private/tmp/claude-501/-Users-adnanbenali-Projects-kolmo/150de77a-2a2f-5cf3-b144-4e505fb7f30d/scratchpad`
- worktree-011 ← `/Users/adnanbenali/kolmo-review/rc3-20261119`
- kolmo-bench-002 ← `/Users/adnanbenali/Downloads/kolmo-bench`
- claude-memory-003 ← `/Users/adnanbenali/.claude/projects/-Users-adnanbenali-Projects-kolmo/memory`

Independent Kolmo clones (inside kolmo-bench-002, each inventoried separately as inventory/clone-*):
src-claude-graph, src-codex-lossless-frontier, src-main, src3-cand, src3-main, src3-v1.0.0, src3-v1.0.1.
Repository facts: not shallow; no alternates; no LFS (git-lfs not installed, no .gitattributes); no submodules; no stashes;
no clean/smudge/diff filters; no active hooks. 103 refs, 580 reflog lines, 20 registered worktrees.

## Verification (manifests/)
Every root: source-before == copy == source-after, full SHA-256 of every regular file, symlink targets, modes, flags, xattrs.
- claude-memory-003.cmp-before-after.txt: compare entries A=9 B=9 diffs=0 protected_xattr_diffs=0
- claude-memory-003.cmp-before-copy.txt: compare entries A=9 B=9 diffs=0 protected_xattr_diffs=9
- git-common-000.cmp-before-after.txt: compare entries A=484 B=484 diffs=0 protected_xattr_diffs=0
- git-common-000.cmp-before-copy.txt: compare entries A=484 B=484 diffs=0 protected_xattr_diffs=450
- kolmo-bench-002.cmp-before-after.txt: compare entries A=42046 B=42046 diffs=0 protected_xattr_diffs=0
- kolmo-bench-002.cmp-before-copy.txt: compare entries A=42046 B=42046 diffs=0 protected_xattr_diffs=42005
- scratch-001.cmp-before-after.txt: compare entries A=80150 B=80150 diffs=0 protected_xattr_diffs=0
- scratch-001.cmp-before-copy.txt: compare entries A=80150 B=80150 diffs=0 protected_xattr_diffs=80148
- worktree-000.cmp-before-after.txt: compare entries A=31353 B=31353 diffs=0 protected_xattr_diffs=0
- worktree-000.cmp-before-copy.txt: compare entries A=31353 B=31353 diffs=0 protected_xattr_diffs=30117
- worktree-011.cmp-before-after.txt: compare entries A=4976 B=4976 diffs=0 protected_xattr_diffs=0
- worktree-011.cmp-before-copy.txt: compare entries A=4976 B=4976 diffs=0 protected_xattr_diffs=0
Only xattr difference: `com.apple.provenance`, added by macOS to files this app writes (not a content change). No ACLs present.
Total: 15.15 GB, 141,309 regular files across 6 roots (git-common-000 is also inside worktree-000), 0 read errors, 0 files changed while hashed.

## History export (export/)
Export copy of git-common; archival refs added only there: refs/archival/worktree-head/<name> for all 19 worktree HEADs
(archival-map.tsv) and refs/archival/reflog/<sha> for 5 reflog-only commits. 127 frozen refs.
history.bundle verifies in an empty repository; mirror clone passes fsck --full; restored refs == frozen refs; no alternates.

## Restore drill (restore-check/)
- worktree-000 restored: git dir resolves inside the copy; status --porcelain=v2 byte-identical to the original's; fsck ok.
- wt-bisect (linked, dirty): .git pointer relocated INSIDE restore-check only; dirty patch byte-identical; HEAD 4fcabc18.
  (The 3 manifest diffs in restore-worktree-000 are exactly that relocation and the index refresh it causes.)
- File classes restored from raw/ to new paths, hash/target-verified: dirty tracked, untracked reports, ignored corpus
  (silesia12), symlinks (venv2/bin), benchmark rows (kolmo-bench nightly, results-mac3.json): all ok, 0 bad.

## Gaps and deviations
- Registered worktree `scratchpad/rc` (claude/release-rc1-20261114): directory present but not a git checkout (its .git link
  is missing); contents copied in scratch-001; HEAD preserved as archival ref. `7eedf813/scratchpad/graph`: directory missing
  before this review; HEAD 9209f571 preserved as archival ref. Neither pruned.
- Before receiving the runbook, this session (a) created ~/kolmo-review-backups/20261119-rc3-review (tar + bundle, kept as an
  extra independent attempt), and (b) ran `git worktree add` in the ORIGINAL repo, creating branch review/rc3-20261119 and
  worktree ~/kolmo-review/rc3-20261119 (5071f388). Additive only; kept; not used for builds. Snapshotted as worktree-011.
- logs/deviations.txt lists two harmless scripting slips (a stray /tmp/x; a symlink class first copied via its target).
- No second copy on independent storage (same APFS volume). Required before any future cleanup proposal; none is proposed.
- Night watcher ~/Downloads/kolmo-bench/night.sh (pid 78773) left running; it can modify kolmo-bench when a watched branch moves.

## Restore commands
  git clone --mirror /Users/adnanbenali/Kolmo-Preserved/review-20261119T083123Z.lKHULl/export/history.bundle NEW.git            # full history incl. archival refs
  /usr/bin/ditto --rsrc --extattr --acl /Users/adnanbenali/Kolmo-Preserved/review-20261119T083123Z.lKHULl/raw/<root> NEW_PATH   # byte-exact tree; verify with tools/manifest.py compare
  Linked worktrees: rewrite NEW_PATH/.git and the matching .git/worktrees/<n>/gitdir in the RESTORED copy only.
