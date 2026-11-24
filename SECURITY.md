# Security policy

Kolmo decodes compressed data that may come from anyone. A crash, hang, out-of-bounds read or write, unbounded
allocation, or a frame that decodes to the wrong bytes without an error is a security bug.

## Reporting a vulnerability

Report it privately through GitHub: **[Security → Report a vulnerability](https://github.com/adnaneben/kolmo/security/advisories/new)**
on this repository. Please do not open a public issue, pull request or discussion for it.

Include what you can of:
- the Kolmo version (`kolmo -V`) or commit, the platform, and whether you used the CLI, the C library, the Python
  package `kolmoc` or the Rust crate `kolmo`;
- the input that triggers it (attach the file to the advisory), and the command or call;
- what happens (sanitizer report, stack trace, wrong output) and what you expected.

What to expect:
- an acknowledgement within 7 days;
- an assessment, and a fix or a plan with a date, within 30 days;
- a fixed release, then a published GitHub security advisory (with a CVE where it applies) crediting you unless you
  ask not to be named. We aim to publish within 90 days of the report, sooner once a fix is out.

## Supported versions

Kolmo is pre-1.0 and fixes go into the latest release only.

| Version | Supported |
|---|---|
| 0.0.2 (latest) | yes |
| 0.0.1 and earlier, 1.0.0 and 1.0.1 (yanked) | no: upgrade to the latest |

The frame format (container version 12) is stable (COMPATIBILITY.md), so upgrading never loses the ability to read
existing frames.

## Fuzzing

The decoders are fuzzed continuously, with the targets in `fuzz/` built by `fuzz/build.sh`:

- **Every pull request and push to `main`**: libFuzzer under AddressSanitizer and UndefinedBehaviorSanitizer, 2
  minutes per target (`.github/workflows/fuzz.yml`), and a bounded ASan/UBSan smoke run of every harness in
  `ci.yml`.
- **Nightly**: 30 minutes per target, continuing from the corpus the earlier runs grew.
- **Targets**: the frame decoder, the limited and range decoders, the streaming decoder (with and without limits),
  round trips at every level, Kolmo's own coders on raw streams, the block and leaf decoders called directly, the
  container extensions (KEXT, IDX1, DDP1) and the `--target` parser.
- **Seeds**: frames of every mode (`fuzz/corpus`) and the frozen frames of every release (`tests/v12`).
- **OSS-Fuzz**: an application is prepared in `packaging/oss-fuzz/` but not yet submitted.

Findings from fuzzing are fixed before a release, and each fix adds the input to the regression tests.

## Verifying release files

Release files are signed with GitHub build provenance. See [RELEASING.md](RELEASING.md#verifying-release-files):

```sh
gh attestation verify kolmo-X.Y.Z-linux-x86_64.tar.gz --repo adnaneben/kolmo
```
