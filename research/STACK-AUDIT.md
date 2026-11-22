# Stacked-codec research: audit against release candidate 2

16 November 2026. Branch `claude/stack-audit-20261116`, from `claude/release-rc2-20261115` (`9d4496a`).

The question: did every measured gain of the stacked-codec research (PR #7, branch
`fix/planner-record-ownership-isolated-20261112` at `df42c63`, directory `research/codec/`) reach this release
candidate, small ones included? Sources read: `STACKING.md`, `PROVISIONAL.md`, `STATUS.md`, `S1-stack-order`,
`S2-conditional-planes`, `R2-stack` (with `COMBINATIONS.md`), `R3_RESULTS.md`, `R4-singleton`, `M1-perf-memory`,
`N1-*`, `N2-*`, `P2-SUMMARY` and the `P2-E*` results, `E02C-harness/X01.md`. Candidate code read:
`src/typed_int_encoder.inc` (`ktf_*`), `src/kg_int_fwd.inc` (`kgf_*`), `src/kgraph.inc` (`kg_serialize`,
`fi_inverse`), `src/kolmo.c` (Huffman and rANS leaf coders).

## Short answer

- Everything the research measured as a gain **against production** and that fits today's format is in the candidate
  except two items: plane-aligned rANS/Huffman chunks with singleton rANS chunks for the planner's integer leaves
  (R2/R3/P2-15), and the singleton rANS decode shortcut (R4). Both are ported here: commit A `a6c2d5f` (encoder,
  opt-in) and commit B `74d08c2` (decoder, same output).
- The plane-aligned leaves change **no file** of the two benchmark sets in any mode (fast, eco, hc, max), default on
  or behind `--native-leaf`. The research gains were measured against weaker baselines (a stored/Huffman/rANS-only
  "native" baseline, zstd-3, or research containers); against the candidate's own leaf rule (zstd/LZMA2/Brotli
  competing with Huffman and rANS per leaf) they do not appear on these files. They are therefore opt-in only.
- The research pipelines that are not in the candidate are, on these sets, **larger than the candidate's own modes**:
  the P2-E25 CSV picker is 55-58% larger than `-m eco` on three of four CSVs; `tp_pent` (per-plane rANS archive frame)
  is larger than `-m max` on all nine typed arrays; S1's per-plane Huffman then zstd is larger than `-m hc` on every
  typed array it accepts.
- Gains that need a format decision (separate leaves per plane, nested Huffman-then-zstd leaves, zero-run leaf,
  packing representations, research containers) are listed in section 4 with their expected gain and the decision.
- Section 6 covers every other codec and transform (kfam, KRC/KBC/KBX, kvid, frontier codecs, engine experiment,
  the in-tree nodes and transforms) and measures five "Kolmo stage -> kfam coder" stacks with a standalone tool: the
  only gain is on integer residual leaves (−4.1% against `max`'s leaves on that class, −0.03% over the whole set),
  and rc2's CM graph leaves (`-x -l 2`) are smaller still.

## 1. Inventory

Needs: **E** = encoder only, the current decoder reads the output; **D** = decoder only, same frames; **L** = new
leaf id or graph node; **F** = new container or research frame format. Gains are as measured by their own experiment,
against that experiment's baseline (named), on observed development data unless marked fresh.

### Stacking map and stack-order experiments

| ID | Idea | Source | Measured gain (baseline) | Needs | In candidate? | If not, why |
|---|---|---|---|---|---|---|
| ST-1 = P-01 + P-03 | Per-plane residual leaves (one leaf per byte plane) plus the sparse-leaf guard | STACKING.md; E02C-harness §9; E02E/E02F/E02G | dev −1.94% fast / −0.64% hc on INT files; E02G fresh −0.67% to −2.00% (production); decode 1.09x fast, 1.20x hc (E02G-verify) | L (`KG_SPLIT` node) | Guard part yes: `kg_serialize` Huffman-only shortcut guard, commit `18d7efb`. Separate leaves: no | Format decision (new node); decode-cost bar missed in E02G |
| ST-2 | Context gate from KG_INT parameters (order 0 or width >= 4) | STACKING.md | Eligibility phenotype, keeps 60-90% of ST-1's gain | with ST-1 | No | Only meaningful with ST-1 |
| ST-3 = P-07 + P-17 + P-18 | Entropy-only per-plane coding as plane-aligned chunks in one leaf, singleton rANS | R2-stack, R3 | R2: H −4.245%, R −5.185%, S 0, H+R+S −5.236% (native baseline, E2C1 frames); R3 fresh −14.70% (native baseline) | E | Partly: aligned Huffman only on the `--typed-huf` route (`ktf_payload_huf`, `c56961f`). Aligned rANS, singleton chunks, aligned chunks for planner-built INT leaves: no | **Ported, commit A**; opt-in (no gain on the sets) |
| ST-4 | Per-plane layout + per-file choice of coder or packing | STACKING.md | Offline ceiling −2.6% to −7.2% fast (ideal choice, selection uncharged) | L/F + selector | No | No realizable selector; P2-E13/E15/E16 found trial selection cost-negative |
| ST-5 | Per-plane leaves at hc | STACKING.md | −0.64% / −0.29% / −0.37% | L | No | As ST-1 |
| ST-6 | Packing + planes on the same residual | STACKING.md | not a stack | — | — | Alternatives, not a sequence |
| ST-7, SQ-6, SQ-7 | Planner feedback from cheaper INT leaves; column split or DECIMAL before INT + planes | STACKING.md | untested | — | No | Never measured |
| ST-8 | Small-message amplification (pageviews 14.7 KB) | STACKING.md | −7% to −12%, 400-800 B | with ST-1 | No | As ST-1 |
| SQ-3 | Packing before zstd | STACKING.md | negative (+2.3% to +25%) | — | — | Rejected by its test |
| SQ-9 | Predictor chosen by coded cost | STACKING.md | blocked | E | No | Not measured (L2 frozen) |
| S1 joined planes | Residual as joined byte planes instead of interleaved values, before zstd-3 | S1 RESULTS | −16.07% (interleaved) | E | Yes: KG_INT writes plane-major residuals (`kg_encode_num`, `kgf_*` in `kg_int_fwd.inc`) | — |
| S1 independent rANS planes | rANS with a model per plane vs one joined stream | S1 RESULTS | −17.31% (joined rANS); Huffman −3.73% | E | Was not; **ported, commit A** (aligned chunks) | — |
| S1 H->Z per plane | Huffman then zstd-3, per plane | S1 RESULTS, S2 | −2.37% vs per-plane zstd-3; 41.68% below whole-file zstd-3 | L (nested leaf) and per-plane leaves | No | Format decision; and on the typed set larger than candidate `hc` everywhere (section 3) |
| S1 H->L per plane | Huffman then LZ4 | S1 | −14.28% vs per-plane Huffman, +3.78% vs H->Z | L | No | As H->Z |
| S1 outer zstd over a KST1 frame | Whole-frame recompression | S1 | −0.273% vs H->Z at 1.2x cost | F | No | Rejected by its own test |
| S2 copy reduction | Output-identical staging-copy removal | S2 §1 | encode −4.1% (research harness) | — | n/a | KST1 harness only: no production code path has these copies |
| S2 upper-plane-only H->Z / H->L | Second stage on upper planes only | S2 §2 | cheaper, +2.74% / +4.55% bytes vs all-plane H->Z | L | No | As H->Z |
| S2 sample gate | Charged sample gate for the second stage | S2 §3 | +0.98% vs all-plane H->Z | — | — | Rejected by its own test |
| R3 gate + shared scratch | Entropy-sample gate for alignment; one rANS buffer sized to the plane | R3 | same frames as fixed HRS; peak heap 6.4 -> 3.4 MB | E | Scratch: **ported, commit A** (rANS work buffer sized to the input). Gate: not needed (the candidate keeps a trial only when smaller) | — |
| R4 | rANS chunk of one symbol decoded as a fill | R4-singleton | kernel 0.78-0.80x, full decode 0.88-0.91x on the one file with such a chunk; no aggregate gain | D | No | **Ported, commit B** |
| M1, N1, N2 | Ratio/speed/memory matrix, OpenZL comparators, matched native benchmark | M1, N1-*, N2-* | measurements of the pipelines listed here (N2: guarded per-plane Huffman 5/8 inside zstd-3 cost; type-first min Huffman/rANS 815,534 B vs 943,819 B guarded Huffman) | — | — | No new lever |

### P-xx proposals (production-connected and forced-typed lines)

| ID | Idea | Measured gain (baseline) | Needs | In candidate? | If not, why |
|---|---|---|---|---|---|
| P-01 | Per-plane residual leaves | see ST-1 | L | No | Format decision |
| P-02 | Sparse-high-plane selection | hypothesis, untested | — | No | Not measured |
| P-03 | Sparse-leaf guard on the fast leaf rule | 164,040 B -> 2,996 B on one dev plane | E | **Yes**, `kg_serialize` (`18d7efb`; tracker X4: co2.npy −18.1% at balanced) | — |
| P-04 | Charged selector over packed representations + zstd-3 | −0.20% at 1.5x encode | F (packing) | No | Packing representations are not in the format; likely superseded |
| P-05, P-06 | Global / local patched packing | fs_inode −4.7%, goog_volume −4.4% (vs production) | F | No | New representation |
| P-07 | Entropy-only coding per plane | X-01: −1.7% to −18.5% vs native baseline, but **+4.2% / +13.8% / −7.0% / −1.0% / 0.0% / +2.4% vs production** | E (aligned) / L (per-plane codec mix) | Aligned form **ported, commit A** | Per-plane codec mixing needs separate leaves |
| P-10, P-11, P-12 | Bounded-width / local / patched packing | E02D: 24.5%, −36.3%, −39.0% vs matched native controls | F | No | New representation; decode screen failed for local packing |
| P-13 | Packed small streams | not supported | — | — | Rejected |
| P-14 | Costed native baseline (best of stored/Huffman/rANS per leaf) | control | E | Yes in substance: `kg_serialize` already competes them per leaf | — |
| P-15 | E03 sampled optional-packing selector | −5.14% beyond P-14 at 1.72x encode | F | No | Rejected (do not promote) |
| P-16 | E03 decode-speed observation | not a finding | — | — | — |
| P-17 | Plane-aligned chunks inside one leaf | see ST-3 | E | **Ported, commit A** | — |
| P-18 | rANS encoder accepting one-symbol chunks | enabler: +45,569 B interaction on synthetic controls, 0 on real | E | **Ported, commit A** (`rans_compress_all_x(.., single)`) | — |
| P-19 | R1 algorithm backlog | untested | — | No | Not measured |
| P-20 | Application profiles | process | — | — | — |

### P2-xx proposals (competitor-bounded overnight work)

| ID | Idea | Measured gain (baseline) | Needs | In candidate? | If not, why |
|---|---|---|---|---|---|
| P2-01 | Type-first planning for declared NPY/WAV | 0/20 -> 7/8 inside zstd-3 cost (CLI) | E | **Yes**: `--typed-huf`, `ktf_type`/`ktf_encode` (`c56961f`) | — |
| P2-02 | Identity-preserving fast KG_INT forward passes | 19/19 identical; 14 -> 8 fits without it | E | **Yes**: `ktf_pass_*` and, for every mode, `kgf_*` in `kg_int_fwd.inc` (`53ba5d4`) | — |
| P2-03, P2-09, P2-13 | Per-plane Huffman as plane-aligned chunks in one `C_HUF` leaf, CZN1 frame | 9/20 feasible-and-smaller (dev); tp_phuf 9/9 of 10 N1 inputs | E | **Yes**: `ktf_payload_huf`; same bytes as the research `tp_phuf_pg16` on the 7 typed-set files it codes typed (checked here) | — |
| P2-04 | Pre-encode guard (bit estimate vs 16 KiB zstd probe) | fs_mtime +340% -> 0.0% | E | **Yes**: `ktf_guard` | — |
| P2-05 | Zero-run residual leaf | mri −28.9% vs −15.9%, fs_inode −75.2% vs −48.3% (vs zstd-3) | L (framing 0xB0) | No | Format decision |
| P2-06 | Type-first CSV columns | −25% to −88% vs zstd-3 | F (research container) | No | P2-D1 closed: research containers not useful |
| P2-07 | Wide-table layout | dem cost +25% -> −19%, mri −25.4% | F | No | Research container |
| P2-08, P2-14 | Probe / trial leaf selection | negative | — | — | Rejected by its own test |
| P2-10 | Fused-order predictor choice | identical decision, fewer sweeps | E | **Yes**: `ktf_c_choose`, `kgf_costs_*` | — |
| P2-11, P2-12, P2-16, P2-18, P2-21 | Archive frontier, cohort breadth, production frames, fresh validation | measurements | — | — | No new lever |
| P2-13 `tp_prod3` | Type-first passes + production zstd-3 leaf rule | same payload as production `kg_encode_num`; a speed lever | E | No | On the typed set it beats `-m fast` on the three 16-bit ECG files (−6.9%, −0.03%, −7.6%), where the planner does not build an integer graph at balanced, but `--typed-huf` already gets those bytes (316,710 vs 316,670); it is larger than `-m fast` on 3 files and equal on 3. Choosing it against `tp_phuf` per file is cost-negative (P2-E13) |
| P2-15 `tp_pent` | Per-plane rANS (singleton) or Huffman archive frame | P2-E14 archive winner on 13-14/61 vs OpenZL | E | Mechanism **ported, commit A** (planner leaves at eco/hc/max under the build switch) | As a standalone route it is larger than `-m max` on all 9 typed files (section 3) |
| P2-17 | Learned / threshold speed selectors | negative | — | — | Rejected |
| P2-19 | Fast order-1 entropy leaf for GHCN | parity with OpenZL at best | L | No | Not pursued (wire review) |
| P2-20 | Singleton `C_RANS` chunks are conformant | 4,500 mutants, 0 findings | E | **Now emitted** under commit A | — |
| P2-22, P2-23 | Table graph + BSC / zstd-12 table frames vs OpenZL csv-9 | fresh FAIL | — | — | Rejected by fresh tests |
| P2-24 | Production `eco` on CSV | validated (27/35) | — | Yes (production eco) | — |
| P2-25 | CSV picker: line regularity -> zstd-12 table frame, else eco | fresh 33/35 vs OpenZL; eco 29-30/35 | E | No | **Superseded**: rc2 `eco` is smaller (section 3); porting would enlarge three of four CSVs by 55-58% |
| P2-26 | Fast KG_INT inverse in the decoder | inverse 3-5x faster; audio fits 24 -> 35/40 | D | **Yes**: `fi_inverse` (`0a5b28e`) | — |
| P2-27 | Audio levers: fast forward pass, fast decoder, repetition-aware leaves | 60-63/80 vs 50-51; ratio 1.535 -> 1.674 | E/D | **Yes**: `kgf_*`/`ktf_pass_*`, `fi_inverse`, `ktf_rep_score` + `ktf_payload_rep` (`53ba5d4`) | The fallback variant `ta_rep_fb` (ratio 1.612) is smaller in gain, not needed |
| P2-D1 | Research containers | closed, not useful | — | — | — |

## 2. Ports on this branch

### Commit A (`a6c2d5f`): plane-aligned integer leaves with one-symbol rANS chunks (encoder only)

What: in `kg_serialize`, for the KG_INT residual stream (`nph_np` byte planes), the Huffman trial and (at
eco/hc/max/ultra) the rANS trial are also run plane by plane, the chunks concatenated in one leaf
(`leaf_compress_planes`). A constant plane becomes a one-symbol rANS chunk (frequency `RANS_M`, 32 bytes of state),
which `rans_decompress_all` has always accepted. The Huffman-skip bound also takes the per-plane entropy. The rANS work
buffer is sized to the input (output identical). Same leaf ids, version-7 frames. A trial is kept only when smaller;
hc keeps its 3% rule for rANS over a non-rANS leaf (R3 found 4x slower kernel decode when rANS replaces Huffman).

Where: opt-in only, because no file of either set got smaller. It runs when `--native-leaf` is given (balanced),
on the `--typed-huf` repetition route, or in every mode when built with `-DKOLMO_ALIGNED_LEAVES=1`.

Evidence (one thread, `-T1`; every new frame decoded by the unmodified rc2 binary and by the new one):

| Build and setting | Files | Result |
|---|---|---|
| Enabled in every mode (first version, without the per-plane Huffman-skip bound), `-m fast/eco/hc/max` | both sets, 44 files x 4 modes = 176 encodes | **176 of 176 byte-identical** to rc2 (563,990,088 bytes both); all decoded by rc2 and by the new build (`results/a1.csv`) |
| Enabled in every mode, final logic (with the bound) | the 15 files where the planner builds integer graphs (typed set, `log_timestamps_ms.i64`, `population.i64`, `year.i32`, `mr`, `x-ray`, `img_camera.u8`) x 4 modes | 60 of 60 byte-identical (`results/a2int.csv`) |
| As committed, `--native-leaf -m fast` | same 15 files | 15 of 15 byte-identical (`results/a3nl.csv`): the native plane-context leaf already wins those leaves |
| As committed, default | — | the new trials run only behind the gate, so default output is unchanged by construction; the build used for the 15-file run above produces the same binary as the committed source |
| `-DKOLMO_ALIGNED_LEAVES=1`, three constructed arrays (mechanism check, not evidence) | `syn_two_planes_u16`, `syn_sparse_spikes_i32`, `syn_const_plane_u32` | fast −7.8% / 0.0% / −4.1%; eco −10.6% / −18.8% / −5.6%; hc −7.1% / −10.5% / −4.1%; max −8.0% / −6.3% / −5.6%; every frame decoded by rc2 (`results/syn_al1.csv`) |

Why no real file moves: X-01 already showed per-plane entropy-only coding is +4.2% / +13.8% / −7.0% / −1.0% / 0.0% /
+2.4% against production's own leaf rule in its six cohorts. On the files here, the leaf winner is zstd, LZMA2 or
the native leaf: on `population_i64.npy` the aligned rANS leaf is 41,072 B against zstd-3's 39,693 (whole-leaf rANS
53,130); on `mr_u16.npy`'s slice 307,521 against 271,641. Alignment does cut the entropy coders' size by 10-25%
there, but not below the LZ coders. On ECG the planner's residuals are one byte wide, so there are no planes, and at
hc the 3% rule keeps whole-leaf rANS (1.3% smaller than Huffman) out on purpose.

Tests at this commit: `make check` passed (20.5 min), `make check-sanitizers` passed, `make check-rc` passed
(`RC_OPTIONS ... aligned_leaves=36 PASSED`, release and ASan builds).

### Commit B (`74d08c2`): one-symbol rANS chunks decoded as a fill (decoder only)

`rans_decompress_all`: a chunk whose table holds one symbol at frequency `RANS_M` must have a 32-byte body of four
states equal to `R64_L`, and decodes to `m` copies of the symbol. This is exactly what the existing loop accepts
and produces (the state update is the identity and reads no word); the decoder now checks it directly and fills.
Same acceptance, same output.

- Byte identity: 36 frames containing such chunks (12 research `tp_prans` frames from P2-E13's encoder, 24 frames of
  the `-DKOLMO_ALIGNED_LEAVES=1` build at max and eco, typed set and constructed inputs) decode to the original with
  rc2 and with this build (`results/dec_b.csv`; whole-CLI timings there are start-up noise on a loaded machine).
- In memory (`rans_micro.c`, median of 31 decodes, three runs): a 4 MiB plane-aligned leaf with three constant 1 MiB
  planes decodes in **1.8-3.1 ms instead of 5.3-7.2 ms**. R4 measured 0.78-0.80x kernel and 0.88-0.91x full decode on
  its one real file with such a chunk; no aggregate gain elsewhere, as here.
- `tests/test_rc_options.c`: a changed state byte in a one-symbol chunk is refused.

Default output is unaffected (rc2's encoder never writes such chunks). Test results for the final tree are in
section 7.

## 3. Research pipelines measured against the candidate on these sets

Built from the PR #7 tree (section 5) and run on the typed set (9 NPY arrays); every research frame decoded by the
rc2 `kolmo -d`. Bytes:

| File | rc2 fast | rc2 eco | rc2 hc | rc2 max | `tp_phuf_pg16` (= `--typed-huf`) | `tp_prod3` | `tp_pent` | `tp_arch19` | S1 `split_HZ` (KST1) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ecg_2ch_i16 | 864,881 | 831,767 | 828,073 | **641,932** | 1,295,136 | 979,010 | 1,145,430 | 768,582 | 1,138,984 |
| ecg_ch0_i16 | 339,974 | 312,368 | 316,668 | **305,172** | 316,710 | 316,670 | 312,414 | 312,414 | 316,743 |
| ecg_ch0_i32 | 316,774 | 312,368 | 316,668 | **305,172** | 316,710 | 316,670 | 312,414 | 312,414 | 316,743 |
| ecg_ch1_i16 | 333,319 | 303,923 | 306,747 | **300,286** | 308,126 | 308,086 | 303,969 | 303,969 | 308,159 |
| log_timestamps_ms_i64 | 2,717 | 2,539 | **2,448** | 2,464 | 5,740 | 2,717 | 3,390 | **2,448** | 2,853 |
| mr_u16 | 2,316,287 | 2,265,447 | 2,258,026 | **2,014,294** | 3,540,672 | 2,611,762 | 2,939,657 | 2,507,892 | header refused |
| population_i64 | 39,873 | 38,872 | 38,506 | **37,458** | 45,295 | 39,873 | 41,293 | 38,506 | 40,445 |
| xray_u16 | 3,657,595 | 3,625,333 | 3,649,874 | **3,616,738** | 4,274,198 | 3,778,281 | 3,735,716 | 3,735,716 | header refused |
| year_i32 | 992 | 756 | 818 | **754** | 1,301 | 992 | 1,254 | 818 | 1,221 |

No research pipeline is smaller than the best rc2 mode on any of the nine files (`tp_arch19` ties hc on
`log_timestamps`). `tp_pent` beats rc2 `hc` on the two single-channel ECG files (−1.3%, −0.9%) because it takes
rANS without hc's 3% decode-speed rule; rc2 `eco` and `max` are smaller there. `--typed-huf` reproduces the research
`tp_phuf_pg16` bytes exactly on the 7 files it codes type-first (mr and x-ray fall back to the planner in rc2, which is
better than the research fallback).

CSV picker (P2-E25, research encoder at `df42c63`) against rc2 `-m eco -T1`: energy_test 1,045,056 vs 670,650
(+55.8%), owid_test 1,420,817 vs 915,227 (+55.2%), owid-co2-data 1,911,453 vs 1,211,445 (+57.8%), wide_test equal
(the picker falls back to eco). rc2 `-m fast` is also smaller than the picker's zstd-12 table frame on all four.

## 4. Needs a format decision (not implemented)

| Item | Expected gain (source, baseline) | Decision needed |
|---|---|---|
| Per-plane residual leaves: one leaf per byte plane, each with its own codec (P-01, ST-1, ST-5, ST-8) | E02G fresh: −0.67% to −2.00% vs production (fast and hc), decode 1.09x fast / 1.20x hc (E02G-verify). The entropy-only part is now available without a format change (commit A) and gained nothing here | A new graph node (`KG_SPLIT`) and a container version; plus a decode-cost gate (ST-2) |
| Nested leaf codecs: Huffman then zstd (or LZ4) per plane (S1/S2 H->Z, H->L) | S1: −2.37% vs per-plane zstd-3, KST1 frames. On the typed set S1's `split_HZ` is larger than rc2 `hc` on every file it accepts (+0.02% to +49%) | A leaf id for a codec chain, on top of per-plane leaves. Not recommended on this evidence |
| Zero-run residual leaf (P2-05) | mri −28.9% vs −15.9%, fs_inode −75.2% vs −48.3% (both vs zstd-3, research container) | A leaf id; first a comparison with rc2 hc/max on sparse residuals |
| Packed residual representations: global/local/patched packing, bounded width (P-04, P-05, P-06, P-10, P-11, P-12) | E02D: −36% / −39% vs matched native controls on one household family; packing before zstd enlarges (SQ-3) | A graph node; only with evidence against rc2's leaf rule |
| Type-first CSV columns and wide-table layout (P2-06, P2-07) | −25% to −88% vs zstd-3 in a research container | P2-D1 closed this (research containers not useful); rc2 eco/fast already code these tables smaller than the production-format alternatives |
| Order-1 entropy leaf (P2-19) | parity with OpenZL-9 at best on GHCN | A leaf id; not pursued by its own review |
| kfam byte model as a graph leaf (section 6) | −4.1% on integer residual leaves vs rc2 max (ECG −4.75%, timestamps −5.7%), 20x faster than ultra -l 2 but 8.8% larger than its CM leaves | A leaf id for an adaptive binary leaf between max and ultra; only if a mode between them is wanted |

## 5. Running the stacked pipelines today

All PR #7 harnesses are single-threaded research encoders: none takes a thread count. Multi-core benchmarking means
running one file per process in parallel (for example `xargs -P 10`); each research frame is one block, so even
`kolmo -d -T 10` decodes it on one thread. Built and checked on this Mac (macOS, clang) in the scratch folder:

```sh
# sources: the PR #7 tree (src/vendor/P2-E9/P2-E11/P2-E13/E02C-harness identical to the S1/S2 pin 4c78ea9)
git archive -o pr7full.tar df42c63 src vendor Makefile research/codec/E02C-harness research/codec/P2-E2 \
  research/codec/P2-E9 research/codec/P2-E11 research/codec/P2-E13 research/codec/P2-E30 research/codec/P2-E28 \
  research/codec/P2-E25 research/codec/S1-stack-order research/codec/S2-conditional-planes
mkdir -p tree shim && tar -xf pr7full.tar -C tree && make -C tree -j2 build/libvendor.a
# e02c.c includes <malloc.h> and calls malloc_trim (Linux): a two-line shim
printf '#include <malloc/malloc.h>\nstatic inline int malloc_trim(unsigned long p) { (void)p; return 0; }\n' > shim/malloc.h
CF="-O3 -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -DZSTD_DISABLE_ASM"
INC="-Ishim -Itree/src -Itree/vendor/zstd -Itree/vendor/lz4 -Itree/vendor/libbsc -Itree/vendor/brotli/include -I/opt/homebrew/include"
LIBS="tree/build/libvendor.a /opt/homebrew/lib/liblzma.a -lpthread -lm -lc++"
for t in P2-E13/p2e13 P2-E30/p2e30 P2-E25/pickcli P2-E11/tpcli; do clang $CF $INC tree/research/codec/$t.c $LIBS -o $(basename $t); done
```

| Binary | Command | Writes | Decoded by |
|---|---|---|---|
| `p2e13` | `p2e13 enc CAND in out`, CAND one of `zstd3 kolmo_fast tp_prod3 tp_phuf tp_phuf_pg16 tb_best tb_best_fb tb_lazy tb_pick tp_prans tp_pent tp_arch19`; `p2e13 run FILE TAG ROUNDS SEED` (in-process timing) | CZN1 frames | rc2 `kolmo -d` (checked: 45/45 typed-set frames) |
| `p2e30` | `p2e30 enc ta_phuf\|ta_rep\|ta_rep_fb in out`; `p2e30 verify FILE...`; `p2e30 score FILE...` | CZN1 frames | rc2 `kolmo -d` |
| `pickcli` | `pickcli pick in.csv out` | CZN1 frames | rc2 `kolmo -d` (checked on 4 CSVs) |
| `tpcli` | `tpcli -p tp_prod3\|tp_phuf\|tp_phuf_pg16\|tp_table3 [-T1] in out` | CZN1 frames | rc2 `kolmo -d` |
| `stack-s1`, `s2` (S1/S2) | `stack-s1 in OUTDIR 0 SEED` writes all 35 arms (`stack-s1 arms` lists them); `stack-s1 decode FRAME LEN out`; `s2 in OUTDIR 0 SEED`; `selftest` | KST1 research frames, inputs up to 16 MiB | only the harness itself (not Kolmo frames) |

S1/S2 build (the pin check of their `build.py` needs a git checkout at `4c78ea9`; the staging it does is reproduced
by `build_stack.py` in the scratch folder): `frozen.inc` is `P2-E11/p2e11.c` before `main` with absolute includes,
`baseline.inc` is S2's `stage_baseline(stack.cpp)`, then
`clang $CF $INC -Itree/research/codec/P2-E13 -c S1-stack-order/bridge.c`,
`clang++ -O3 -std=c++17 S1-stack-order/stack.cpp bridge.o $LIBS` (no `-I` to `vendor/libbsc`: its `version` file
shadows `<version>`), and the same for `S2-conditional-planes/conditional.cpp` with `-I` to the staged `baseline.inc`.
Selftests on this Mac: S1 420 round trips, 2,079 truncations, 2,376 mutations; S2 192 / 882 / 1,008. S1 refuses
`mr_u16.npy` and `xray_u16.npy` ("header bounds").

Parallel use, one file per process: `ls in/*.npy | xargs -P 10 -I{} sh -c 'p2e13 enc tp_pent {} out/$(basename {}).kz'`.

## 6. Every codec and transform, and how they stack

Scope extension from the owner: every codec and transform in the research, in rc2 or not, and their stacking.

### Inventory

| Codec / transform | What it is | Best measured result (data, source) | In rc2? | Stacking measured / worth measuring |
|---|---|---|---|---|
| kfam (`src/experimental/kfam.c`) | Family streams (FASTQ names/bases/qualities/layout; CSV columns; else one stream), small dedicated models, own binary arithmetic coder, streams on threads | FASTQ 2.0% smaller than `kolmo -x`, 13.8% smaller than CRAM 3.1; CSV 14-60% larger than balanced (owner's figures, rc2 benchmark run). Newer version on `claude/kfam-fastq-20261116` (`2098f0b`, not in rc2): 3 FASTQ runs 16,720,709 B (was 21,809,687; rc2 `kolmo -x` 22,246,381) | Yes, standalone tool | Measured here: its byte model as the coder of Kolmo's graph leaves and transformed blocks (section 6, S-A to S-E) |
| KRC1 (`krc*.c`, Metal `krc_simd.metal`, hybrid) | 32-bit word tiles, modular/second-difference/XOR prediction with strides, bit packing, exceptions; GPU planning | Ratio 1.050 FASTQ, 1.015 video, 1.122 research set; encode 157-681 MB/s, decode 1.0-3.0 GB/s, scaling to 10 threads (`benchmark/rc2/results/tables.md`) | Yes, standalone | KRC tile predictor -> kfam residual model: worth measuring (KRC payload is packed bits; needs the residual words exported) |
| KBC1 / KBX3 (`kbc*.c`, `kbx*.c`) | Byte-context arithmetic coder per 256 KiB block, choosing raw / KRC1 / context stream; KBX3 longer tagged contexts | KBX -l 1: ratio 4.052 FASTQ, 1.844 video, 3.315 research set at 3-4 MB/s, 6 MB/s decode; threads to 10 (same table) | Yes, standalone | KBC/KBX context model as a graph leaf: same role as kfam's byte model; kfam measured instead |
| kolmo-fastq / `frontier_fastq` + `frontier_entropy` | Four-line FASTQ split, 12-bit rANS (`frontier_entropy`), runs | ratio 2.075, 550 MB/s encode, 1.1 GB/s decode on 3 FASTQ files (same table) | Yes, standalone; codecs 8-11 only in `-DKOLMO_FRONTIER` builds | Its streams -> kfam models: that is kfam's FASTQ family |
| kvid (`kvid.c`) | y4m front end: per-strip intra MED / temporal / temporal+MED, residuals coded by the Kolmo library | `kvid -m x` 51,468,145 B on akiyo/foreman/mobile (ratio 2.659) vs `kolmo -x` 53,706,642 (same table) | Yes, standalone | Measured here (S-E): kvid residuals -> kfam |
| KOLMO_FRONTIER codecs (`frontier_dna`, `_pcm`, `_table`, `_lag`, `_prefix`, `_runs`, `_shuffle`) | Opt-in build: 2-bit DNA, DNA prefix codes, 16-bit PCM tiles, DNA runs (block codecs 8-11); wide-table and lag transforms (tflag 10/11); shuffle+delta | Codex 7 November "fast" experiments for genomic and wide-table data (`benchmark/frontier`, `2c0aeee`) | Yes, `-DKOLMO_FRONTIER` only | Wide-table / lag outputs -> kfam: same mechanism as S-C, not run (opt-in build) |
| Engine experiment (`origin/claude/engine-exp`, `3a1b82d`, `99dd9ef`) | Context coding of zstd's literals and sequences, keeping zstd's matches | Static: 1-3% smaller than zstd -3; adaptive: 10-15% smaller, ~8x slower decode; own match coder 3.0% smaller than zstd -3 at a level-3 parse | No (line stopped) | It is itself a stack (zstd parse -> context coder). A kfam-style adaptive literal coder after zstd's parse is the same idea |
| Native plane-context leaf (`origin/feat/native-residual-leaf`) | Leaf codec 15: rANS with plane/lane contexts | −9.8% over 317 integer files, whole-file decode 2.3x slower (tracker A10) | Yes, `--native-leaf` | Measured: aligned leaves (commit A) never beat it; kfam plane contexts (S-B) |
| CM graph leaves (`origin/feat/ultra-cm-graph-leaves`) | `-x -l 2`: graph leaves may use context mixing | −11% to −15% on integer series where the graph is chosen, −2% mixed | Yes, `-x -l 2` | Measured: beats the kfam leaf by 8.8% on ECG (S-A) |
| Codex `codex/nov7-research-snapshot` (`0c6f9ee`), `origin/codex/lossless-frontier` | Archive of the 7 November prototypes (KRC, frontier FASTQ/entropy, Metal) and market measurements | as KRC / frontier rows | The codecs are in rc2 as experimental | — |
| `codex/independent-speed-ultra-memory` (local) | KRC/KBC gains record; lower-memory ultra validation | ultra table compaction landed as `aacf5a8` (same archive bytes) | Yes (as merged pieces) | — |
| `research/krc-kbc-metal` | KRC/KBC/Metal WIP and lzbench evidence | as KRC/KBC rows | Yes (as experimental tools) | — |
| DEC64 (graph node 7) | binary64 values that are short decimals -> digits and exponents, byte planes, dictionary columns | massdata −5.7% balanced (dictionary); co2.npy −18.1% with the sparse guard (tracker R1, S1) | Yes | Measured: DEC64 planes -> kfam +15.8% vs max leaves (S-A) |
| INT node (KG_INT), FLOAT node | predictor order 0-2, gcd, bias, plane-major residuals; float byte planes | FLOAT: weights 1.486 at ratio (table above) | Yes | Measured: INT residuals -> kfam −4.1% vs max (S-A); FLOAT planes -> kfam +3.2% |
| Table v1/v2 (`tbl2` formulas), table graph (FIELDS/DECIMAL/CONCAT) | column transforms; v2 with discovered formulas | P2-E24: `eco` beats OpenZL csv-9 on 27/35 fresh CSVs | Yes | Measured: tbl2 output -> kfam +21.7% vs the block codec; table-graph leaves -> kfam +9.8% (S-C, S-A) |
| Log templates, line classes, word transform, JSON split, record model, x86 filter | text/record block transforms | line classes: in rc2 since `0fcca8b`; see TARGETS/BENCHMARKS | Yes | Measured: their outputs -> kfam +2.8% (records) to +39.8% (log templates) vs max (S-C) |
| Image CM model (block codec 17), CM models 1-3 | context mixing in ultra | ultra Silesia 17.01% (model 2); mr/x-ray `-x` 5,501,818 B vs JPEG XL e7 5,485,501 | Yes | Image blocks -> kfam +3.6% vs max (S-C) |
| `--typed-huf` | declared NPY/WAV -> fast INT passes -> per-plane Huffman in one leaf | ESC-50 40 clips: 8,982,932 B at 667 MB/s vs balanced 9,847,874 | Yes, opt-in | Measured (S-D): its residual planes -> kfam −6.6% to −59.5%, at ~1-2 MB/s |
| Plane-aligned leaves (this branch) | see section 2 | none on the sets | opt-in (commit A) | — |

### Stacking combinations measured (no format change: a standalone research tool)

Tool: `research/stack-audit/stackbench.c` (build: `research/stack-audit/build.sh OUT`). It compresses a file with the
Kolmo library as built here, decodes it once through two reporting hooks added to a copy of the source (graph leaves;
non-graph blocks after their transform, before the inverse), codes every captured stream with kfam's byte model
(orders 0-3 plus one side context, kfam's own arithmetic coder, `src/experimental/kfam.c` unchanged) and decodes it
back. "Stacked" = Kolmo frame − replaced payloads + kfam payloads, taking per stream the smaller (what a leaf id
would allow; headers ignored). Every kfam stream was decoded back and compared (72 file runs, all verified). Raw output: `research/stack-audit/results/*.jsonl`.

| Id | Stack | Baseline | Result |
|---|---|---|---|
| S-A | Kolmo graph leaves -> kfam byte model | the same leaves as rc2 `-m max` codes them (LZMA2 / Huffman / rANS), 44 files | Integer residual leaves **−4.07%** (9 leaves; ECG −4.75%, timestamps −5.7% / −4.9%, year −3.5%, population −0.07%); every other class larger: DEC64 planes +15.8%, FLOAT planes +3.2%, table graph +9.8%, image rows +2.8%. Whole set: 126,805,469 -> 126,762,354 B (**−0.034%**) |
| S-A' | the same against `-x -l 2` (CM graph leaves, in rc2) | 5 small typed files | kfam leaf larger: ECG 290,668 vs 267,119 (+8.8%), timestamps 2,170 vs 1,995; gains only on the 128-byte "rest" leaves (−25 B). kfam enc+dec ~0.8 s vs `-x -l 2` 17 s enc / 14.5 s dec on ECG |
| S-B | Integer residual planes -> kfam with plane context (plane index, or the byte at the same index in the previous plane) | kfam without context | timestamps 2,246 -> 2,229 (index) -> 2,170 (cross-plane, −3.4%); population 37,963 -> 37,980 / 37,757; mr, x-ray (typed route) no better |
| S-C | Block-transform outputs (tbl2, log templates, line classes, word, JSON, records, image, x86) -> kfam instead of the block codec | rc2 `-m max` block codecs | never smaller: tbl2 +21.7%, log +39.8%, JSON +10.8%, records +2.8%, image +3.6%, x86 +23.9%, plain blocks +43.7% (vs BWT/LZMA2/CM there) |
| S-D | `--typed-huf` residual planes -> kfam | per-plane Huffman (`-m fast --typed-huf`) | **−23.8%** over the typed set (10,103,888 -> 7,700,191): ECG −8.2%, mr −33.8%, x-ray −10.4%, timestamps −59.5%; but kfam runs at ~1-2 MB/s, typed-huf at hundreds of MB/s, and rc2 `max` is smaller than this stack on mr and x-ray |
| S-E | kvid residual layout -> kfam (whole layout, one stream) | kvid's layout at Kolmo `max` | akiyo +16.3%, foreman −1.98%, mobile −0.37%; per-file choice −0.93% (55,557,352 -> 55,042,306), still 6.9% above `kvid -m x` (51,468,145) |

Threads: kfam's parallelism is per stream. One thread vs ten (on a machine at load 12-15): owid_energy_f64.npy
(18 streams) 19.0 s -> 6.7 s; llm_bf16_10mb.safetensors (5 streams) 24.5 s -> 12.4 s; wide_test.csv (6) 5.8 s ->
4.8 s; single-stream files unchanged. Kolmo itself was run with `-T 1` (blocks of one-thread frames).

Reading: kfam's generic byte model helps only on integer residuals, where max's LZMA2 leaf is weaker than an
adaptive order-0-3 model; there rc2 already has two stronger options (`--native-leaf` at balanced, CM leaves at
`-x -l 2`). On every transformed text/table/record stream Kolmo's block coders win by 3-44%. kfam's own strength is
its FASTQ family models, which are not a generic leaf. Worth measuring next: KRC tile predictor -> kfam residual
model; kfam v2's quality/base models on Kolmo's FASTQ line-class streams; a kfam-style adaptive coder for zstd's
literals (engine experiment) at balanced/hc decode budgets.

## 7. Test results on the final tree

At `74d08c2` (commits A and B), on this Mac, `-j4`, shared machine:

- `make check`: ALL PASSED (24 min).
- `make check-rc`: lines, DEC64, fast integer inverse (three builds), planner ownership, image model, RC options
  (release and ASan, `aligned_leaves=36`) PASSED (14 min).
- `make check-sanitizers`: resource and parallel tests PASSED.
- Mirrors: `src/kolmo.c`, `src/kgraph.inc`, `src/typed_int_encoder.inc` copied to `bindings/python/native/src` and
  `bindings/rust/native/src` (the mirrors were identical to `src/` before).

Files: `research/stack-audit/` holds the stacking tool (`stackbench.c`, `kfam_api.c`, `kvidlayout.c`,
`build.sh`, `summ.py`), the rANS fill micro-benchmark (`rans_micro.c`) and the raw results (`results/`: `a1.csv`,
`a2int.csv`, `a3nl.csv`, `syn_al1.csv`, `th.csv`, `dec_b.csv` from the CLI comparisons, columns
file,mode,old_bytes,new_bytes,old_enc_s,new_enc_s,decoded_ok,identical; `*.jsonl` from stackbench).
