# Governance

> **Status: draft for the owner to confirm.** Sections that describe how the project works today are marked
> *current*. Sections marked *proposal* are rules the owner has not set yet; they are written so that they can be
> adopted as they are, changed, or deleted before this document is merged.

Kolmo is a small project with one maintainer. This document says who decides what, how a contributor can become a
maintainer, and how releases are made. It is short on purpose and will grow when the project does.

## Maintainers (*current*)

| Name | GitHub | Role |
|---|---|---|
| Adnane Aqartit | [@adnaneben](https://github.com/adnaneben) | Owner and sole maintainer: merges, releases, the package names on PyPI (`kolmoc`) and crates.io (`kolmo`), the Homebrew tap |

The owner holds the repository, the package registries, the Homebrew tap and the release signing and publishing
rights.

## How decisions are made (*current*)

- **Day-to-day changes** are pull requests reviewed and merged by the maintainer. Nothing is merged to `main` without
  the maintainer's approval, whoever wrote it.
- **Decisions that bind users** are made by the owner and recorded in the repository, with their date, where the
  rule lives. Examples so far: the level scale and command line (`docs/LEVELS.md`, owner decisions of 2026-11-22),
  the format promise (`COMPATIBILITY.md`), the release numbering (0.0.x), and the rule that a new version ships only
  when there is something meaningful in it (see *Releases*).
- **Format changes** follow the rules in `COMPATIBILITY.md` and `CONTRIBUTING.md`. No maintainer can approve a change
  that makes an existing version-12 frame decode to different bytes.

### With more than one maintainer (*proposal*)

- Any maintainer may merge a pull request that another maintainer has approved and CI passes.
- A maintainer does not merge their own pull request without a second maintainer's review, except for release
  bookkeeping (version numbers, formula hashes, changelog dates).
- Changes to the format, to `COMPATIBILITY.md`, to this document, or to the level contract (`docs/LEVELS.md`) need
  the owner's explicit approval.
- Disagreements are discussed in the pull request or issue. If maintainers still disagree, the owner decides and
  writes down why.

## Becoming a maintainer (*proposal*)

The project wants a second maintainer. Reducing the bus factor is part of the adoption plan, and users who store data
for years reasonably ask who will read it if the owner cannot.

**Who we are looking for.** Someone who has worked in the code and understands the format promise: what may change
(the encoder, the bytes it writes) and what never may (the bytes a frame decodes to).

**Suggested path:**

1. Contribute: merged pull requests over a few months, ideally including at least one in the core (`src/`) and one
   review of someone else's change.
2. Show the habits the project depends on: changes that keep decoded bytes identical, measurements that follow the
   benchmark rules, and honest reporting of where a change loses.
3. Any maintainer may propose a contributor; the owner decides. New maintainers start with merge rights on the main
   repository. Release and registry rights (PyPI, crates.io, the tap) follow once they have shadowed a release.

The owner will set the concrete thresholds (number of pull requests, time) before this section is adopted.

**Stepping down.** A maintainer may step down at any time by saying so. A maintainer with no activity for a year
(*proposal*) may be moved to an emeritus list after being asked.

## Releases

### Current policy

- **Releases are infrequent and substantial.** A new version ships only when it carries a meaningful update; work
  accumulates under `## Unreleased` in `CHANGELOG.md` on `main` until then. Small fixes do not get a release of their
  own unless they fix wrong output, a decode failure or a security problem.
- **Numbering.** The current line is 0.0.x (0.0.1 and 0.0.2 are out). While the version starts with 0.0, the command
  line and the library API may change between releases (`COMPATIBILITY.md`, *What is not promised*). The compressed
  format does not: every release reads every version-12 frame.
- **Every release adds its frames to the frozen corpus.** The release process (`RELEASING.md`) compresses a fixed set
  of inputs with the release binary and adds them under `tests/v12/r<version>/`. Existing frames are never
  regenerated, so every later release is tested against what every earlier release wrote.
- **Channels.** A release is a git tag `vX.Y.Z`, a GitHub release, the PyPI package `kolmoc`, the crates.io crate
  `kolmo` and the Homebrew formula in `adnaneben/homebrew-kolmo`, all at the same version. `RELEASING.md` has the
  steps. A tag is never moved after a release is out.
- **Fix releases** for wrong output, decode failures or security problems may ship outside the normal rhythm, with
  only the fix.

### Cadence (*proposal*)

> To be decided by the owner. Suggested wording: "Kolmo releases when there is a substantial update, and expects
> roughly two to four releases a year. Each release is announced in `CHANGELOG.md` and on the GitHub releases page.
> Fixes for wrong output, decode failures and security problems are released as soon as they are ready."

## Security

How to report a vulnerability, and how the project handles it, is in `SECURITY.md`.

## Code of conduct

Everyone taking part follows [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). The owner enforces it until there are other
maintainers; then any maintainer may act on a report, and a report about a maintainer goes to the owner.

## Changing this document

Changes to this document are made by pull request and need the owner's approval.
