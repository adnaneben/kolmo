# OSS-Fuzz application (draft, not submitted)

`projects/kolmo/` holds the three files a new project adds to [google/oss-fuzz](https://github.com/google/oss-fuzz):

| File | What it does |
|---|---|
| `project.yaml` | homepage, language, contacts, sanitizers (ASan, UBSan, MSan), engines (libFuzzer, AFL++, honggfuzz) |
| `Dockerfile` | `base-builder` image, `make` and `zip`, a clone of this repository |
| `build.sh` | calls `fuzz/build.sh` from this repository, so the target list stays with the code |

`fuzz/build.sh` builds ten targets (the frame, range and streaming decoders, the round trips, Kolmo's own coders, the
leaf and block decoders, the container extensions and the `--target` parser) and their seed corpora from
`fuzz/corpus` and the frozen frames in `tests/v12`. MSan builds use the C reference kernels (`NO_ASM=1`).

The same script runs in this repository's CI:
- `.github/workflows/fuzz.yml`: libFuzzer under ASan + UBSan, 2 minutes per target on each pull request, 30 minutes
  nightly, with the grown corpus kept in the Actions cache.
- `.github/workflows/oss-fuzz-check.yml`: builds this draft with OSS-Fuzz's own helper and images for each
  sanitizer, then runs OSS-Fuzz's `check_build`. A green run here is the best local evidence that the submission
  builds.

## Before submitting (owner)

1. **Contacts.** Replace `OWNER-EMAIL@REPLACE-ME` in `project.yaml` (`primary_contact` and `auto_ccs`). Each address
   must be linked to a Google account: OSS-Fuzz uses it to sign in to the ClusterFuzz dashboard and the issue tracker,
   where the reports are restricted to those addresses until disclosure. Add a second maintainer to `auto_ccs` if
   there is one.
2. **Is Kolmo eligible?** OSS-Fuzz accepts projects that are widely used or critical to infrastructure. A new
   compressor may be declined until it has users; the application asks for evidence (downloads, dependents). If it is
   declined, the in-repo workflow keeps running, and [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/) can
   reuse these same three files with no approval needed.
3. **Read the obligations** (below) and decide whether to take them on.

## How to submit

```sh
gh repo fork google/oss-fuzz --clone && cd oss-fuzz
mkdir -p projects/kolmo && cp /path/to/kolmo/packaging/oss-fuzz/projects/kolmo/* projects/kolmo/
python3 infra/helper.py build_image kolmo
python3 infra/helper.py build_fuzzers --sanitizer address kolmo
python3 infra/helper.py check_build kolmo
git checkout -b kolmo && git add projects/kolmo && git commit -m "Add kolmo"
gh pr create --repo google/oss-fuzz --title "Add kolmo" --body "..."
```

The PR body should say what Kolmo is, who uses it, and why its decoders are worth fuzzing (it parses untrusted
compressed input). An OSS-Fuzz maintainer reviews it; once merged, fuzzing starts within a day.

## What OSS-Fuzz asks of maintainers

- **Fix reported bugs.** Crashes are filed in the OSS-Fuzz issue tracker, visible only to the listed contacts.
- **Disclosure deadline.** A report becomes public 90 days after it is filed, or 30 days after OSS-Fuzz verifies
  the fix, whichever comes first.
- **Keep the build working.** A broken build is reported to the contacts; projects that stay broken are disabled.
- **Respond.** Contacts are expected to read and triage reports; unresponsive projects can be removed.

Kolmo's own reporting policy is in `SECURITY.md`; OSS-Fuzz reports follow OSS-Fuzz's deadlines above.
