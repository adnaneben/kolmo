# Night 3 summary — 2026-11-22 (+03), Apple M5

Nothing was merged, tagged, published or yanked, and nothing outside the working branches was deleted. The log is `NIGHT3_LOG.md`; the plan is `NIGHT3_PLAN.md`.

## 1. The library-free build: 0.0.1-rc1 is ready for review

**Draft PR [adnaneben/kolmo#12](https://github.com/adnaneben/kolmo/pull/12)**, branch `review/rc4-native-J62kQD`. It is built on rc4 (#11), so review #11 first.

What it is:
- **No third-party codec library.** zstd, Brotli, LZ4, liblzma and xxHash are gone from the code, the build, both bindings and the tree. `nm` shows only Kolmo's own symbols, and the binary links only libSystem.
- **Every preset is native:**
  - speed → KLZ;
  - balanced → KLZH2 level 3;
  - eco → KLZH2 level 7;
  - hc → KLZH2 level 9;
  - ratio → KLZMA;
  - Huffman leaves → khuf.
- **New format: container version 12.**
  - Older files fail with a clear error: "written by an earlier Kolmo (1.x or a release candidate)…".
  - rc4 refuses version-12 files.
- **Removed:** `-D` dictionaries, `kolmo train` and trained profiles. They now return "not available in this build".
- **Labelled 0.0.1-rc1** (wheel `0.0.1rc1`, crate `0.0.1-rc.1`). The Homebrew formula no longer needs xz.

| gate | result |
|---|---|
| Phase E (17 steps) | 17/17 |
| GitHub CI | 5/5 on 9a830edc; the later commits are re-checked by the PR's CI |
| Round trips | all presets, screening corpus, `-T 1` and `-T 4` |
| Determinism | 122 frames OK; reference regenerated for the intended encoder changes |

**Timed holdout** (21 unseen files, strict T1, 1,575 accepted trials, binary 647ff915, before tonight's tuning):

| preset | vs rc4 | vs nearest rival |
|---|---|---|
| speed | **−9.6%**, encode 0.90×, decode 0.92× | lz4 -1: −9.8% at the same decode |
| balanced | +1.10% (since improved, see §2) | zstd -3: −20.8% |
| eco | −0.33%, decode **1.22×**, encode 0.70× | zstd -9: −20.8% |
| hc | +1.24% (since improved) | zstd -19: −15.9% |
| ratio | +2.92% → fixed (see §2) | xz -9: −8.9% |

## 2. Tuning after the first holdout (Track B)

- **ratio fix (9a830edc).**
  - A 47 MB bf16 weights file was 12.7% larger because its float graph was never checked against plain KLZMA.
  - Now it is **3.3% smaller than rc4**.
  - It was diagnosed on a holdout file, so that file no longer counts as unseen for ratio.
- **KLZH2 encoder tuning** (subagent, `review/klzh2-gap-J62kQD`; cherry-picked as 1f446a4b, 9a6d1db7, 565b3f95):
  - Level 3: three repeat offsets, larger tables, 3-byte repeats.
  - Huffman spans chosen per block.
  - KLZH (29) output is byte-identical.
  - Level 3: −3.6% on the tuning set (now 3.0% smaller than zstd -3), encode −12%.
  - Level 9: +0.62% vs zstd -19 (was +0.77%). The rest of the gap is Huffman vs FSE in the format.
- **Literal price floor of 1 bit** (f3144860). The 2-bit floor cost raw video 2.8% under hc.
- **End to end, screening corpus vs rc4:** balanced **−0.27%** (was +0.62%), hc **−0.60%** (was −0.32%).
- **Timed holdout re-run** (balanced, hc, ratio vs rc4): see §5.

## 3. Decisions I made (alternatives for you)

| # | decision | alternative |
|---|---|---|
| D12 | Container version 12, plus a dedicated legacy error for older files | Keep version 11 and fail on checksums (confusing "corrupt" errors) |
| D13 | `khash64` replaces xxHash for every checksum and hash | Write our own XXH3 implementation (same values, more code) |
| D14 | Dictionaries, `kolmo train` and profiles removed (return "unsupported") | Build a native dictionary coder first (days of work) |
| D15 | Planner size probes use KLZH2 (level 1, or 3 where zstd 3 was asked) | Keep a library just for probes (not library-free) |
| D16 | Track B priorities: eco was no longer behind, so balanced and hc first | — |
| D17 | Under ratio, float graph blocks are also checked against plain KLZMA (gated by a KLZMA slice) | Accept the +12.7% on bf16 weights |
| D18 | Took the KLZH2 encoder tuning: −3.6% at level 3 for −12% encode speed | Keep the faster level 3 |
| D19 | Literal price floor of 1 bit, not 2 | 2 bits (raw video +2.8%) |
| — | The holdout/CI fix: the concurrency test now checks the refusal instead of using dictionaries | — |

## 4. For you to decide
1. **PR #11 (rc4), then PR #12 (0.0.1-rc1):** review and merge. #12 is stacked on #11.
2. **The 0.0.1 release steps** (`PLAN_FORWARD.md` §2):
   - Homebrew `version_scheme 1`;
   - yank 1.0.0 and 1.0.1 only after 0.0.1 is up;
   - next versions 0.1.0, 0.2.0.
   - The README should say plainly that 0.0.1 does not read 1.x files.
3. **Whether a small separate tool should convert 1.x files.** It would be built from rc4, with the libraries, and would decompress old files so users can recompress them with 0.0.1.

## 5. Final holdout re-run (f3144860; balanced, hc, ratio vs rc4; 630 accepted, byte-verified)
| preset | vs rc4 now | before tonight's tuning | encode | decode |
|---|---|---|---|---|
| balanced | **+0.56%** | +1.10% | 0.79× | 0.91× |
| hc | **+1.02%** | +1.24% | 0.86× | 0.93× |
| ratio | **−0.79%** | +2.92% | 0.97× | 0.99× |

Remaining losses, by file:
- balanced: sao +7.4%, nci +4.4%, foreman +1.7%;
- hc: nci +3.5%, the bf16 weights +3.4%, mozilla +2.6%.

These are large binary and long-range inputs, where the gap that remains is in the format (Huffman vs FSE for the code streams) and in the parse on bf16. Both are listed as next work in the subagent's report.

**0.0.1-rc1 today against rc4:**

| preset | size |
|---|---|
| speed | −9.6% |
| balanced | +0.6% |
| eco | −0.3% |
| hc | +1.0% |
| ratio | −0.8% |

Every preset is still 9–21% smaller than its nearest rival (lz4, zstd, xz).
