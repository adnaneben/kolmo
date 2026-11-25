# Kolmo 0.0.3 release checklist

Prepared on 2026-11-25. **Nothing is released.** No tag, no GitHub release, no upload to PyPI, crates.io or Homebrew,
and the security advisory is still a private draft. This file lists what is done, what the owner must decide, and the
exact commands to release. Delete it after the release (or keep it under `benchmark/`): it is not a user document.

0.0.3 is a patch release for the long-field data-loss bug of 0.0.1 and 0.0.2, with the fixes of the 40-reviewer
review. The frame format is unchanged (container version 12).

## 1. Done in this pull request

| Item | State |
|---|---|
| Version 0.0.3 in `VERSION`, `pyproject.toml`, `Cargo.toml`, `KOLMO_VERSION_STR`, the tap formula's `url`, README Status, SECURITY.md, GOVERNANCE.md, `docs/LEVELS.md` | done |
| `bindings/*/native/` synced with `src/` | done |
| CHANGELOG: `## Unreleased` became `## 0.0.3`, with a top note (data-loss bug, security fixes, sizes by file type); an empty `## Unreleased` is above it | done; **the date is not set** (section 4, step 1) |
| Release binary built from the branch (`make ARCH= build/kolmo`, rebased on main 4b977c20) | done |
| `tests/v12/r0.0.3/`: 274 frames, 42 originals, `frames.json`; 273 lines appended to `tests/v12/MANIFEST` | done; no existing frame or line changed |
| Frames written again from the rebased build (after PR #44) | 274 of 274 byte-identical |
| `tests/v12.sha256`: 318 lines added, none changed | done |
| `tests/vectors/cm/prefixes.json`: digests for the new frames, none changed | done |
| COMPATIBILITY.md, FORMAT.md, SPEC.md say 0.0.3 where they said "Unreleased" | done |
| Package pages (`bindings/python/README.md`, `bindings/rust/README.md`) name the data-loss fix | done |

### Tooling changed on the way (please review)

- **`tests/check_v12_pinned.py`: `MANIFEST` is now pinned as append-only.** The pin file held one hash for the whole
  `MANIFEST`, but every release must append its lines to that file, so `--write` refused and the release procedure
  could not be followed. The old `MANIFEST` line is kept unchanged. A new line `MANIFEST#630` pins the file after
  0.0.3. Every such pin must be the hash of the first lines of today's file, so lines can only be added at the end.
  Changing or removing an earlier line still fails (tested by hand both ways).
- **`tests/check_release_docs.py`: "Since X.Y.Z" is accepted like "since X.Y.Z"** on the package pages. Both pages say
  "**Since 0.0.2:** ..." and the check only passed while the version was 0.0.2.
- **RELEASING.md** now lists the pin step, the context-mixing digest step and the rule for the packaging drafts.

### Checks run locally (Apple M5, macOS, 2026-11-25)

| Check | Result |
|---|---|
| `make ARCH= check` on the rebased branch | passed (exit 0), with the 632-frame corpus |
| `make check-cm-reference` (every context-mixing stream decoded whole) | still running when this file was committed (about 40 minutes); its result is in a comment on the pull request. The quick form in `make check` (first 1024 bytes of every stream) passed |
| `python3 benchmark/determinism/check.py build/kolmo build/determinism` | 152 frames, OK |
| `python3 benchmark/ledger/ledger.py build/kolmo` | passed, 571 entries, no size change against `sizes.tsv` |
| `python3 tests/check_release_docs.py build/kolmo` | OK (version 0.0.3) |
| `python3 tests/check_v12_pinned.py --base <main's pin file>` | passed, 734 pins |
| 0.0.2 tag binary decodes the 274 frames of `r0.0.3/` | 274 of 274 to the original |
| 0.0.1 tag binary decodes them | 274 of 274 to the original |
| 300 MiB pipe through 0.0.3 (a `CZNS` stream of 2 segments), read by 0.0.2 and 0.0.1 | same SHA-256 as the input |
| 0.0.3 reads `longfield.csv` written by 0.0.1 (8 modes) and 0.0.2 (10 levels) | all to the original |
| Python sdist and wheel built (`python -m build`), `twine check` | passed; not uploaded |
| `bindings/python/test_kolmo.py` against the built wheel's library | ALL PASSED |
| `cargo package` (builds the packaged crate), `cargo test` | passed (12 tests); not uploaded |

### Package pages read from the built files

The description inside the built sdist, the built wheel and the packaged crate was read, not only the source files.

- PyPI: the sdist's and the wheel's descriptions are identical to `bindings/python/README.md`. Version 0.0.3.
- crates.io: the crate's `README.md` is identical to `bindings/rust/README.md`. Version 0.0.3, `links = "kolmo"`.
- Both pages now carry a "New in 0.0.3: a data-loss fix" paragraph. Nothing on them describes 1.x or a removed
  option as current.
- Found and corrected: the Rust paragraph first gave error code -2; "corrupt" is -1 (`KOLMO_ERR_CORRUPT`).
- Left as it is, for the owner (section 3): the one-line summary, the missing licence files and the object files in
  the wheel.

## 2. What 0.0.3 writes that 0.0.2 refuses

The full list is in COMPATIBILITY.md, section *0.0.3 and earlier releases*. In short:

- **Frames: nothing.** No new codec id, plan type, tflag, node id, extension tag or container version.
- **One case in the bindings.** The 0.0.3 command line writes a pipe over 256 MiB as a `CZNS` stream (0.0.2 wrote one
  frame). The 0.0.1 and 0.0.2 command lines read it. The 0.0.1 and 0.0.2 **Python and Rust bindings refuse every
  `CZNS` stream** with an error (checked with the 0.0.2 Python binding; the Rust one by the changelog entry of PR #39).
- **Related, not written by 0.0.3 itself:** a file made by appending outputs after a stream. 0.0.2 and 0.0.1 stop
  after the first stream and exit 0 without the rest. Only 0.0.3 reads such files in full.
- **Refused by 0.0.3, accepted by 0.0.2:** only frames that no encoder ever wrote (seven cases, listed in
  COMPATIBILITY.md), and `kolmo -d` on a `kolmo archive` file.

## 3. Open owner decisions

Decide these before tagging. The first three can change what ships.

1. **Level 9 is 3.9% larger than 0.0.2 on bf16 model weights. Ship, or fix first?**
   `llm_bf16_47mb.safetensors -9`: 26,168,919 bytes with 0.0.2, 27,186,418 with 0.0.3. The change came with PR #32
   (main before it writes 26,168,919; the merge 47198180 writes 27,186,418). Its changelog entry says only levels 1,
   2, 3 and 8 changed. On the 42 corpus files this makes level 9 0.7% larger in total; every other level 9 frame is
   byte-identical to 0.0.2's. The README says level 9 is 14.1% smaller than xz -9 on the held-out files, measured
   on 0.0.2. The CHANGELOG top note states the regression. Not investigated further and not fixed here.
2. **Level 8 is larger on large text** (webster +2.4%, FASTQ +1.0% to +1.5%, bf16 weights +2.7%), from 32 MiB blocks
   instead of 128 MiB. This was a documented choice of PR #32 (a third less memory). Confirm it ships.
3. **PR #37 (levels 6 and 7 never larger than the level below): hold, or include?** It guarantees the order by
   encoding twice. Cost on one thread: `-6` +45% to +53% compress time, `-7` +114%, for 0.02% to 0.18% smaller
   output on large inputs (Canterbury tar `-7` -1.6%). It now conflicts with main and needs a rebase. Recommended: keep
   it out of 0.0.3, which is a bug-fix release.
4. **Release date.** The CHANGELOG heading says "date to be set on release day".
5. **Security advisory GHSA-w749-2668-7wp2: the draft on GitHub has two errors.** Section 5 has the corrected text.
   - Its workaround says "or `fast` (0.0.1)". `-m fast` of 0.0.1 **is affected** (checked with the 0.0.1 binary). The
     safe mode is `-m speed`.
   - Its table lists `balanced`, `eco`, `hc`, `ratio` for 0.0.1. `rt`, `fast` and `max` are affected too: every mode
     except `speed` and `-x`.
   - Also decide: severity (the draft says medium), whether to request a CVE, and the credit line.
6. **README results table.** It was measured on 0.0.2. A sentence now says so. Measure it again on 0.0.3 before or
   after the release? Level 1 is 6.5% smaller on the corpora, so the `-1` row undersells; the `-9` row oversells by
   about 0.7%.
7. **Package summaries updated.** Both distribution manifests describe supported structured input and parallel
   decoding. Per-workload performance claims belong in the measured results.
8. **Distribution contents verified.** The Python sdist, wheel and Rust crate include the project licence and notice
   and the libsais licence. Wheel builds keep compiler objects outside the installed module tree.
   Run `tests/check_package_contents.py` on built files to verify the notices against the source tree.
9. **Packaging drafts stay at 0.0.2 in this pull request** (conda-forge, Debian, AUR, nixpkgs, homebrew-core). The
   task asked for the formula drafts to be bumped. Only the tap formula was: the others pin the sha256 of a release
   tarball and CI (`packaging.yml`) downloads it, so naming v0.0.3 before the tag exists turns that workflow red.
   Section 4, step 8 bumps them after the tag. Say so if you want them bumped now and the workflow red until the tag.
10. **Yank 0.0.1 and 0.0.2 on PyPI and crates.io after 0.0.3 is out?** They write files they cannot read. Yanking
    keeps pinned installs working and stops new ones. Not done.
11. **PR #25 (outreach kit, drafts only)** is open and mergeable. It does not touch the release. Merge before or
    after, or leave.
12. **Launch drafts** (`docs/launch/`) wait on the benchmark page (PR #29, GitHub Pages is off) and name the release
    as `[RELEASE]`. Fill in 0.0.3 when they are used. Not part of this release.
13. **Still open from earlier work, not release blockers:** release cadence wording (GOVERNANCE.md, *Cadence*), the
    HDF5 filter ID and Blosc2 codec ID requests (drafts, not sent), the OSS-Fuzz application (not submitted), the
    lzbench upstream pull request (not sent), homebrew-core (not eligible before 2026-11-30 and the star count).
14. **This file.** Delete it after the release, or keep it as a record.

## 4. Commands to release

Run from a clean checkout of `main` after this pull request is merged. Replace `2026-MM-DD` with the release date.

### Step 1: set the date, merge, check main

```sh
# on the pull request branch, before merging:
sed -i '' 's/^## 0.0.3 (date to be set on release day)$/## 0.0.3 (2026-MM-DD)/' CHANGELOG.md
git commit -am "changelog: 0.0.3 release date" && git push
# merge the pull request on GitHub, then:
git checkout main && git pull
gh run list --repo adnaneben/kolmo --branch main --limit 6      # ci, fuzz, packaging, integrations: all green
make -j4 ARCH= build/kolmo && ./build/kolmo -V                   # kolmo 0.0.3
python3 tests/check_release_docs.py build/kolmo                  # release docs: OK (version 0.0.3)
```

### Step 2: tag (this also publishes to PyPI)

**Pushing the tag starts `wheels.yml`, which uploads the sdist and the Linux wheels to PyPI through Trusted
Publishing, and `release.yml`, which builds and signs the release files.** A version can be uploaded only once, and a
tag must never be moved after the release is out. Read the two package pages one last time first.

```sh
git tag -a v0.0.3 -m "kolmo 0.0.3" && git push origin v0.0.3
gh run list --repo adnaneben/kolmo --workflow wheels.yml --limit 3
gh run list --repo adnaneben/kolmo --workflow release.yml --limit 3
```

### Step 3: GitHub release

```sh
# notes: the 0.0.3 section of the changelog, without its heading
awk '/^## 0\.0\.3/{f=1;next} /^## /{f=0} f' CHANGELOG.md > /tmp/notes-0.0.3.md
gh release create v0.0.3 --repo adnaneben/kolmo --title "kolmo 0.0.3" --notes-file /tmp/notes-0.0.3.md
```

If `release.yml` finished first, the release exists as a draft with the signed files: then run
`gh release edit v0.0.3 --repo adnaneben/kolmo --title "kolmo 0.0.3" --notes-file /tmp/notes-0.0.3.md --draft=false`
instead. Check a file:

```sh
gh release download v0.0.3 --repo adnaneben/kolmo --pattern 'kolmo-0.0.3-linux-x86_64.tar.gz' --dir /tmp/rel
gh attestation verify /tmp/rel/kolmo-0.0.3-linux-x86_64.tar.gz --repo adnaneben/kolmo
```

### Step 4: PyPI

Automatic from the tag (step 2). Check it, then build and upload the Apple silicon wheel by hand:

```sh
pip download kolmoc==0.0.3 --no-deps --no-binary :all: -d /tmp/pypi && ls /tmp/pypi   # the sdist is up
bash bindings/python/build-macos-wheel.sh          # builds from the published sdist; prints the upload command
twine upload <the wheel it names>                  # your own terminal; a PyPI token limited to kolmoc
```

Then open https://pypi.org/project/kolmoc/0.0.3/ and read the page.

### Step 5: crates.io

```sh
cd bindings/rust
cargo publish --dry-run
cargo publish                                      # needs `cargo login`, your own terminal
```

Then open https://crates.io/crates/kolmo/0.0.3 and read the page.

### Step 6: Homebrew tap

```sh
curl -sL https://github.com/adnaneben/kolmo/archive/refs/tags/v0.0.3.tar.gz | shasum -a 256
# put the hash in packaging/homebrew/kolmo.rb (sha256) and delete the comment line above it, then:
git checkout -b release/0.0.3-formula && git commit -am "homebrew: sha256 of the v0.0.3 tarball" && git push -u origin HEAD
# merge that pull request, then copy the file to the tap:
cp packaging/homebrew/kolmo.rb "$(brew --repository adnaneben/kolmo)/Formula/kolmo.rb"
cd "$(brew --repository adnaneben/kolmo)" && git commit -am "kolmo 0.0.3" && git push
brew update && brew upgrade adnaneben/kolmo/kolmo && brew test adnaneben/kolmo/kolmo
brew audit --strict adnaneben/kolmo/kolmo
kolmo -V                                           # kolmo 0.0.3
```

### Step 7: publish the security advisory

Only after PyPI, crates.io and Homebrew all serve 0.0.3. Open
https://github.com/adnaneben/kolmo/security/advisories/GHSA-w749-2668-7wp2 , replace the description with the text of
section 5, check the affected ranges (`kolmoc` and `kolmo` crate: `>= 0.0.1, < 0.0.3`, patched `0.0.3`; `>= 1.0.0,
<= 1.0.1`, no patch), and press **Publish advisory**.

### Step 8: packaging drafts and the benchmark default (one follow-up pull request)

```sh
H=$(curl -sL https://github.com/adnaneben/kolmo/archive/refs/tags/v0.0.3.tar.gz | shasum -a 256 | cut -d' ' -f1)
# packaging/README.md, "At each release", names each field:
#   conda-forge/kolmo/recipe.yaml, conda-forge/kolmoc/recipe.yaml   context.version, context.sha256
#   debian/debian/changelog        dch -v 0.0.3-1        debian/ITP.txt   Version
#   aur/PKGBUILD                   pkgver, sha256sums, pkgrel=1; makepkg --printsrcinfo > .SRCINFO
#   nix/package.nix                version, hash (nix hash to-sri sha256:$H)
#   homebrew-core/kolmo.rb         url, sha256
#   .github/workflows/packaging.yml (comment), bench-x86.yml (default ref v0.0.3), ISSUE_TEMPLATE/3-performance.yml
```

### Step 9: after the release

- Decision 10: yank 0.0.1 and 0.0.2, or not.
- Update the master plan document and the memory notes: 0.0.3 is out; next numbers 0.0.4 to 0.0.9.

## 5. Advisory text to publish (corrected)

Title: **Files written by Kolmo 0.0.1, 0.0.2 and 1.0.x can fail to decompress: delimited text with a field over
64 KiB**

Changes from the draft on GitHub: the 0.0.1 row of the table, the "cannot upgrade" line (`speed`, not `fast`), the
`pip` command, and the last sentence of *Details*.

```markdown
### Summary

Kolmo 0.0.1 and 0.0.2 (and the yanked 1.0.0 and 1.0.1) can write a compressed file that they then refuse to
decompress. Compression reports success; decompression fails with `corrupt or unsupported input`. **The data inside
these files is intact**: Kolmo 0.0.3 reads every affected file correctly, and nothing needs re-compressing.

Do not delete originals you compressed with an affected version until you have checked the compressed files as
described below.

### Who is affected

Only input that Kolmo treats as a table, where a block has the same number of fields on every row **and** one field
is longer than 65,535 bytes. In practice:

- CSV, TSV and other delimited text (`;`, `|`) with one very long cell (embedded JSON, base64, long free text).
- **Space-separated logs with one line over 64 KiB.** In a log, the last field runs to the end of the line, so one
  long line is enough.

| Version | Affected settings |
|---|---|
| 0.0.2 | levels 3–9, including the default (3) |
| 0.0.1 | every mode except `speed` and `-x`: `balanced` (the default), `ratio`, and `rt`, `fast`, `eco`, `hc`, `max` |
| 1.0.0, 1.0.1 (yanked) | `rt`, `fast` (the default), `eco`, `hc`, `max`. `ultra` is not affected. |

Kolmo 0.0.2 at levels 1–2 and ultra, and binary or non-tabular input, are not affected. The CLI (Homebrew
`adnaneben/kolmo/kolmo`), the Python package `kolmoc` and the Rust crate `kolmo` share the same code and are all
affected.

### How to check your files

Test every file you compressed with an affected version; this writes nothing. A file that fails here may be affected;
one that passes is fine.

    kolmo -t file.kolmo                       # 0.0.2 or 0.0.3
    kolmo -d file.kolmo /dev/null             # 0.0.1

From Python: `kolmo.decompress(open("file.kolmo", "rb").read())` raises an error for an affected file.

### How to fix

- **Upgrade to 0.0.3** (`brew upgrade kolmo`, `pip install -U "kolmoc<1"`, `cargo update -p kolmo`). It reads every
  file 0.0.1 and 0.0.2 wrote, including affected ones, and no longer writes files that older versions cannot read.
- **Files written by 1.0.0 or 1.0.1:** 0.0.x does not read 1.x files at all (see the README section "Files written by
  Kolmo 1.x"). Decompress them with kolmoc 1.0.1 with `CZN_OLDTBL=1` set, which sends the decoder down the path that
  handles long fields:

      python3 -m venv kolmo1 && kolmo1/bin/pip install kolmoc==1.0.1
      CZN_OLDTBL=1 kolmo1/bin/python -c 'import kolmo,sys; open(sys.argv[2],"wb").write(kolmo.decompress(open(sys.argv[1],"rb").read()))' old.kolmo plain

- If you cannot upgrade yet: compress affected data at level 1 or 2 (0.0.2), or with `-m speed` (0.0.1).

### Details

Aligned table blocks are decoded by a fast path that stores field lengths in 16 bits and rejects longer fields, while
the encoder allowed them. 0.0.3 lets the decoder fall back to the general table path
([#22](https://github.com/adnaneben/kolmo/pull/22)) and stops the encoder from writing such blocks
([#30](https://github.com/adnaneben/kolmo/pull/30)). A block with a field over 65,535 bytes is now coded without the
table transform, so those inputs compress less well than before; other inputs are not affected by this fix.

### Credit

Found during internal testing.
```
