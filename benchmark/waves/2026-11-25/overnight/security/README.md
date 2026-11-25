# Full-archive recipient encryption experiment

25 November 2026. The #91 C/security ledger records an outer authenticated envelope as design only. Its handoff recommends an established recipient tool, private staging, and a separate decision on sender authenticity. The active #57 tracker does not assign an implementation PR. The local Claude fuzz/sign branch covers decoder validation and artifact comparisons. No overlapping encryption implementation was found in the reviewed branches or ledgers. Native Kolmo v12 remains the inner format.

## Offline experiment

Neither age nor rage was available. The host has OpenSSL 4.1.0, GnuPG 2.5.24 and 7-Zip 26.06. The benchmark used OpenSSL CMS AuthEnvelopedData with AES-256-GCM and an ephemeral RSA-3072 recipient, RSA-OAEP with SHA-256 and MGF1-SHA-256. No production credentials or private user files were used. The temporary directory (0700) held scratch keys and private intermediate files (umask 077); only run receipts were retained. Encryption followed normal Kolmo archive creation. After decryption to private staging, the harness checked process success and compared the complete inner archive's SHA-256 to its known local control before invoking the stock T1 Kolmo reader. Restores verified every member's bytes, permissions and modification time.

Seven interleaved plain/enveloped pairs plus one warmup pair per workload, stock level 1 on Apple M5. Input bytes divided by the median behavior of measured total create + wrap + unwrap + restore time; per-phase medians are independently reported. The three SHA/metadata checks and setup work take place outside timed phases. Key creation is excluded. These are shared-host, warm-cache measurements. The file-source data are pinned by SHA in inputs.csv.

| Workload | Plain / enveloped bytes | Plain / enveloped creation ms | Plain / enveloped restore ms | Plain / enveloped combined MB/s |
|---|---:|---:|---:|---:|
| 70,476 B source and prose | 39,762 / 40,372 | 7.11 / 17.01 | 6.48 / 17.31 | 5.26 / 1.99 |
| 4 MiB Silesia prefix | 2,117,191 / 2,117,806 | 25.63 / 41.36 | 9.39 / 26.77 | 114.73 / 57.86 |

The payload AEAD rejected a changed final tag and the wrong recipient. Truncation failed. A separate format check found the general-purpose OpenSSL CLI accepts trailing bytes and also decrypts a CBC-only envelope with the same recipient. Accordingly the prototype is confined to envelopes it created locally. A generic CLI success code is insufficient to enforce an AEAD-only, single-message receiver. No public decryption command or new archive format is enabled. The normal experiment used a known SHA from the producer as a readback oracle, not a sender-signature protocol.

**Decision.** Retain as an external recipient-envelope cost and completeness experiment. For a shipping receiver, qualify age or an explicitly enforced AEAD profile with audited complete-envelope parsing, sender identity policy when needed, authenticated metadata coverage, key lifecycle and interruption handling. The outer structure exposes ciphertext length and recipient identification data. No streaming random access or whole-tree publication semantics were evaluated. [OpenSSL CMS manual](https://docs.openssl.org/master/man1/openssl-cms/) · [Archive design](https://github.com/adnaneben/kolmo/pull/91)

Replay using a disposable output directory:

```sh
python3 envelope-eval.py --openssl /opt/homebrew/bin/openssl --kolmo /path/to/pinned-main/kolmo --repo /path/to/pinned-main --silesia /path/to/silesia-prefix-4 --out measurements
```

## Release verification

[#60](https://github.com/adnaneben/kolmo/pull/60) is a draft at f6e2db9f1623c8b615302555b49a566e6cd17610. Both workflow files and all three release scripts matched that PR's Git blobs. Ten mock checks were rerun offline and passed, including already-present assets, digest/version mismatches, a moved tag, and public/draft handling. The workflow verifies provenance for each file and SHA256SUMS against the expected repository, signer workflow, ref and source digest, then stages missing assets, downloads the final set and checks the published bytes. PyPI has its own digest gates. The mocks replace git/gh interactions and PyPI fixtures; they cannot attest real assets or validate hosted OIDC identity.

[Release PR #47](https://github.com/adnaneben/kolmo/pull/47) is open and records v0.0.3 as unpublished. It records successful local sdist, arm64 wheel, and Rust crate content checks. No real v0.0.3 draft assets or registry uploads were verified in this experiment; owner-controlled release and advisory decisions still apply. #96 hosted PR checks and candidate-local package checks also cannot verify a future tag, its attested binaries, or registry files.
