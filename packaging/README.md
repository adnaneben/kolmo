# Packaging

Drafts for each distribution channel, so that each submission is a copy-paste at release time. Each draft targets the
**0.0.2** tarball so it can be built and tested now; they move to a new release once its tag and tarball exist
(RELEASING.md). Nothing here has been submitted anywhere.

| Folder | Channel | State |
|---|---|---|
| `homebrew/` | Homebrew tap `adnaneben/kolmo` | **live** (0.0.2 published; the file here names 0.0.3, its sha256 is set after the tag) |
| `conda-forge/kolmo/` | conda-forge: `libkolmo` + `kolmo` (one multi-output recipe) | draft; built and tested locally (osx-arm64) and in CI |
| `conda-forge/kolmoc/` | conda-forge: `kolmoc` (Python binding) | draft; built and tested locally (osx-arm64) and in CI |
| `debian/` | Debian (then Ubuntu by sync): `kolmo`, `libkolmo0`, `libkolmo-dev` | draft; built and tested in CI; ITP text in `debian/ITP.txt` |
| `aur/` | Arch User Repository: `kolmo` | draft; built and tested in CI |
| `nix/` | nixpkgs: `kolmo` | draft; built in CI |
| `homebrew-core/` | homebrew-core: `kolmo` | draft; built, tested and audited in CI; **not eligible yet** (see below) |

CI: `.github/workflows/packaging.yml` builds every draft in a clean environment on each PR that touches `packaging/`,
`src/` or the `Makefile`, from the release tarball. Debian is built a second time from the checkout, so a change on
main that breaks the packaging shows before a release.

## At each release: where the version and checksum live

The tarball is always `https://github.com/adnaneben/kolmo/archive/refs/tags/vX.Y.Z.tar.gz`. Its sha256 is
`curl -sL <url> | shasum -a 256`.

| File | Change |
|---|---|
| `conda-forge/kolmo/recipe.yaml` | `context.version`, `context.sha256` |
| `conda-forge/kolmoc/recipe.yaml` | `context.version`, `context.sha256` (the same tarball) |
| `debian/debian/changelog` | a new entry `kolmo (X.Y.Z-1)` (`dch -v X.Y.Z-1`); the orig tarball is the GitHub tarball renamed `kolmo_X.Y.Z.orig.tar.gz` |
| `aur/PKGBUILD` | `pkgver`, `sha256sums`, `pkgrel=1`; then `makepkg --printsrcinfo > .SRCINFO` |
| `nix/package.nix` | `version`, `hash` (SRI form: `nix hash to-sri sha256:<hex>`, or `nix-prefetch-url <url>`) |
| `homebrew-core/kolmo.rb` | `url`, `sha256` (after acceptance homebrew-core's autobump does this) |

The feedstock (conda-forge), `debian/watch`, Repology and homebrew-core's autobump find new tags by themselves once
the packages exist.

## Before the first submission: upstream gaps

These are outside `packaging/`, so the drafts work around them. Fixing them upstream makes every recipe shorter and
is expected by Debian reviewers.

1. **No `make install`, no soname, no pkg-config file.** Each recipe copies files by hand and sets the soname itself
   (`libkolmo.so.0` on Linux, `libkolmo.0.dylib` on macOS) by passing linker flags through `LIBS`. An upstream
   `install` target with `PREFIX`/`DESTDIR`/`LIBDIR`, a versioned soname and a `kolmo.pc` would replace all of it.
   Debian strongly prefers the soname to come from upstream.
2. **The Makefile ignores `LDFLAGS` and `CPPFLAGS`.** The recipes pass the distribution's linker flags (hardening,
   relro) through `LIBS` and `CPPFLAGS` through `CFLAGS`.
3. **The 0.0.2 PyPI sdist has no `LICENSE` or `NOTICE`.** conda-forge requires the licence in the package, so the
   `kolmoc` recipe builds from the GitHub tarball. `bindings/python/MANIFEST.in` should `include LICENSE NOTICE`
   (copied next to `setup.py` by `sync-native.sh`).
4. **The 0.0.2 wheels ship the build's object files.** `setup.py` compiles into `build_lib/_kolmo_build/`, which then
   lands in the wheel and in `site-packages/_kolmo_build/` (about 1.4 MB of `.o` files in the macOS arm64 wheel on
   PyPI). The `kolmoc` recipe deletes it. `setup.py` should build in a temporary directory.
5. **Test data provenance (Debian).** `tests/v12/text.txt` is a Project Gutenberg etext (Dickens, public domain) with
   the Gutenberg header. `debian/copyright` lists it. Before the ITP, check where every original in `tests/v12/` and
   the `benchmark/` data came from: ftpmasters review the whole source tarball, not only what is installed. Exclude
   anything unclear with `Files-Excluded:` in `debian/copyright` and a `+ds` version.
6. **ABI stability.** At 0.0.x the C ABI may change in any release. conda-forge pins `libkolmo` to `x.x.x`, and Debian
   names the package `libkolmo0`. Bump the soname (and rename the Debian package `libkolmoN`) on any ABI break.

## Channel checklists

### conda-forge (`conda-forge/`)

Requirements: an OSI licence with the licence files in the package (done: `LICENSE`, `NOTICE`, libsais's `LICENSE`).
Tests that run without network access (done). At least one maintainer on GitHub (placeholder `MAINTAINER_PLACEHOLDER`
to replace or remove). No vendored libraries that conda-forge already ships (none: every coder is Kolmo's own). No
Windows build (`skip: win`).

1. Fork `conda-forge/staged-recipes` and branch from `main`.
2. Copy `conda-forge/kolmo/` to `recipes/kolmo/` and `conda-forge/kolmoc/` to `recipes/kolmoc/`. Do not copy
   `local-variants.yaml`: on conda-forge, the global pinning provides it.
3. Set `extra.recipe-maintainers` (GitHub handles). Each listed maintainer must confirm in the PR.
4. Test locally:
   `rattler-build build --recipe recipes/kolmo/recipe.yaml -m <path>/local-variants.yaml -c conda-forge`.
5. Open one PR ("Add kolmo and kolmoc"). The linter bot comments; reply `@conda-forge-admin, please ping team` when it
   is green.
6. After the merge, a `kolmo-feedstock` and a `kolmoc-feedstock` are created; you get commit rights. The autotick bot
   opens version-bump PRs on new tags.

Notes: `kolmo` links the library statically, as the upstream Makefile builds it; `libkolmo` is the separate shared
library for C users. `kolmoc` builds per Python version although it does not use the Python C API (ctypes), because it
carries a compiled library. If reviewers ask, it can become `noarch: python` depending on `libkolmo`, but `kolmo.py`
would first need to find the library in `$CONDA_PREFIX/lib` on macOS.

### Debian (`debian/`)

Requirements: DFSG-free source (yes, Apache-2.0 plus libsais's LLVM exception), a complete DEP-5 `debian/copyright`
(drafted; see gap 5), a sponsor (the owner is not a Debian Developer), a maintainer email (`MAINTAINER_EMAIL`
placeholder in `control`, `changelog`, `copyright`, `ITP.txt`).

1. Fill in `MAINTAINER_EMAIL` and send `debian/ITP.txt` to `submit@bugs.debian.org` (from the same address). Put the
   returned bug number into `debian/changelog` (`Closes: #NNNNNN`).
2. Create `https://salsa.debian.org/debian/kolmo` (or under your Salsa user), import the orig tarball with
   `gbp import-orig`, and add `debian/`.
3. Build in a clean chroot: `sbuild -d unstable` (or `pbuilder`), then `lintian -EvIL +pedantic` and
   `autopkgtest . -- null` (or a schroot).
4. Upload to mentors.debian.net (`dput mentors ../kolmo_X.Y.Z-1_source.changes`) and file an RFS bug against
   `sponsorship-requests`, or ask the Debian Science or Debian Python teams for a sponsor.
5. After the sponsor's upload the package waits in the NEW queue (ftpmaster review), then reaches unstable. Ubuntu
   syncs it automatically for its next release.

What `debian/` builds: `kolmo` (CLI with a help2man man page), `libkolmo0` (`libkolmo.so.0`), `libkolmo-dev`
(`kolmo.h`, `libkolmo.so`, `kolmo.pc`). The build runs the fast part of `make check`; `kolmotest` and the decoder
fuzz regressions are left out because they take tens of minutes on amd64 and far longer on slow buildds. The
autopkgtests round-trip through the installed CLI and build a program against `libkolmo-dev` with pkg-config.

### AUR (`aur/`)

No notability requirement: this channel can open as soon as a release ships.

1. Create an AUR account with an SSH key (aur.archlinux.org).
2. `git clone ssh://aur@aur.archlinux.org/kolmo.git` (an empty repository creates the package).
3. Copy `aur/PKGBUILD` and `aur/.SRCINFO`, fill in the `# Maintainer:` email, and check with
   `makepkg -sci && namcap PKGBUILD kolmo-*.pkg.tar.zst`.
4. `git add PKGBUILD .SRCINFO && git commit -m "kolmo X.Y.Z" && git push`.

`options=('!lto')`: the build is not link-time optimised, so that the floating-point choices of the encoder stay those
of the per-file `-ffp-contract=off` build. Frames must not depend on the compiler's optimisation of the encoder.

### nixpkgs (`nix/`)

1. Fork NixOS/nixpkgs. In a first commit, add yourself to `maintainers/maintainer-list.nix`
   (`maintainers: add <handle>`).
2. Copy `nix/package.nix` to `pkgs/by-name/ko/kolmo/package.nix`, set `maintainers = with lib.maintainers; [ <handle> ];`
   and commit as `kolmo: init at X.Y.Z`.
3. Test: `nix-build -A kolmo`, `nixpkgs-review wip`, and on macOS too if possible. Format with `nixfmt`.
4. Open the PR against `master` with the template filled in.

Reviewers may ask for `fetchFromGitHub` instead of `fetchurl`: its hash is that of the unpacked tree. The CI job
"nix" prints it.

### homebrew-core (`homebrew-core/`)

**Not eligible yet.** homebrew-core's acceptance policy (docs.brew.sh, "Package Acceptance Policy", notability) asks
for at least 30 forks, 30 watchers or 75 stars. A **self-submission** (by the repository owner, or by a commit author
whose email is at the package's domain) needs **90 forks, 90 watchers or 225 stars**. A repository less than 30 days
old is normally not eligible: kolmo's was created on 2026-10-31, so not before 2026-11-30. On 2026-11-23 the
repository had 0 stars, 0 forks and 0 watchers. Until then, the tap (`brew install adnaneben/kolmo/kolmo`) is the
Homebrew channel. A submission by someone else needs only 75 stars.

Other requirements, met by the draft: a stable tagged release, an open-source licence (`license` stanza), a `test do`
block that exercises the tool (CLI round trip and a program linked against the library), building from source with
no vendored dependencies, and portable binaries (`ARCH=`). The bottle block is not written by hand: BrewTestBot adds it.

1. When the notability rule is met: `brew tap --force homebrew/core`, then copy `homebrew-core/kolmo.rb` to
   `$(brew --repository homebrew/core)/Formula/k/kolmo.rb` (drop the header comment).
2. `HOMEBREW_NO_INSTALL_FROM_API=1 brew install --build-from-source kolmo`, `brew test kolmo`,
   `brew audit --new --formula kolmo` and `brew style kolmo` must all pass.
3. Commit as `kolmo X.Y.Z (new formula)` and open the PR to Homebrew/homebrew-core.
4. After the merge, retire the tap formula: add `tap_migrations.json` (`{"kolmo": "homebrew/core"}`) to the tap and
   remove `Formula/kolmo.rb` from it, so tap users move to homebrew-core.

Differences from the tap formula: no `version_scheme` (core starts fresh), `ARCH=` (no `-mcpu=native`), and it installs
`libkolmo` and `kolmo.h` too.
