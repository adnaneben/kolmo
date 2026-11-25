# 0.0.3 release-candidate verification

Release-preparation base `862731aeb88d472fc43a7727dcbf8768e2a83b3f`. Main integration `a1d39559686a9ec17c8614b9154a64d7b5b98672`. Qualified Git tree `5288b50e182d02d48b81e628f31e03056378689a`.

The only native source difference from that main is the CLI version string. The 35 files incorporated from the merged impact research branch use the identical Git blobs on main. Both binding source mirrors match the canonical native tree.

| Verification | Outcome |
|---|---|
| Frozen v12 native decoding | 630 frames restored to their original bytes |
| Frozen fixture inventory | 734 pinned files unchanged |
| Source determinism | 152 frames passed |
| Size ledger | 571 entries, zero size changes and 24 historical level-order inversions |
| Ordinary archival C tests | 3,712 passed |
| ASan/UBSan archive and index tests | 3,744 passed using seed 123 |
| Archive command line | Complete feature suite passed, including selected first, last and empty members |
| Stock 0.0.2 ↔ candidate DDP1 | Both directions passed for a 32 MiB + 97 byte unique member, and a separate two-member archive containing an empty member. The two encoders emitted identical archive bytes in both cases. |
| Rust | Packaged source crate rebuilt successfully and all binding tests passed offline. |
| Python | Built a wheel from the sdist, installed it in a clean environment, and passed the full binding suite including CLI byte parity at every level. |
| Python publishing metadata | The sdist and wheel each passed `twine check --strict`. |
| Distribution contents | All three locally built packages carry exact copies of project and third-party notices. The wheel contains no compiled object intermediates. |
| RC release documentation | Passed for version 0.0.3. The package summaries and checklist items 7 and 8 are updated. |

## Local package receipts

These qualification builds ran on Apple M5 arm64.

| File | Bytes | SHA-256 |
|---|---:|---|
| `kolmo-0.0.3.crate` | 679170 | `0b0a95c95c7ec235903ec789a85eb097aa0544304e40bd6ee0881dcd6c82cd64` |
| `kolmoc-0.0.3-py3-none-macosx_11_0_arm64.whl` | 781680 | `d5f8d90134c213e57c071d0a2d65c02d05298dfa09b6870f6f9f1dc56904284e` |
| `kolmoc-0.0.3.tar.gz` | 686851 | `430c7d282492b11928f57112b290d1f349b29f14fdb4fbc61072bc1f1b8b924f` |


Earlier full Python CM reference coverage of 64 streams is recorded on PR #47. The reference decoder, vectors and corpus bytes are identical to the earlier RC. The quick reference run is also part of the current executable test sequence.

The GitHub-hosted CI, Linux wheels and official tag artifacts require separate receipts when produced.
