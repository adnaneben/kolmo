# rc3 wave 2, regress lane: the regressions of rc3 against rc2

17 November 2026. Branch `claude/rc3w2-regress-20261117`, from `claude/release-rc3-20261116` (`1536036`). The four
regressions of `benchmark/rc3/mac/RESULTS.md` ("Regressions you should know about"), fixed with the owner-approved
recommendations. The integration lane repeats the key runs on the Apple M5.

## Machine and method

- Cloud VM: 4 vCPU Intel Xeon @ 2.10 GHz (AVX-512), 15 GiB, Linux 6.18, gcc 13.3, `make` defaults
  (`-O3 -march=native -ffp-contract=off`). rc2 = `b2f0c00` built the same way; "base" = rc3 at `1536036`; "new" = this
  branch.
- Wall time: `bench.py` (one process per compression and per decompression, the binaries alternate order every round,
  fastest of 5 rounds for `balanced`, 3 for video `balanced`, 1 for video `eco`/`hc`/`ratio`, 2 for `ratio`; every
  output decoded and compared with the input). `-T 1` and `-T 4` are separate runs, never mixed. Small files take a
  few milliseconds, so their ratios carry the process start-up and are noisy (±1 ms).
- Instruction counts: `ir.sh` runs `callgrind` on a `-march=haswell` build of each variant (valgrind does not decode
  AVX-512), one thread. They are exact and repeatable, so the gate designs were compared on them; the wall times
  follow them (rc3 base on OWID energy: 10.5× rc2's instructions, 9.3–9.7× its time).
- Data (`mkdata.py`, public sources, nothing committed): Silesia; OWID CO2 and energy CSVs (today's release:
  `owid_test.csv` and `energy_test.csv` are the last three quarters of the rows, `wide_test.csv` the energy table
  pivoted to 316 `;`-separated columns, `owid_energy_f64.npy` and `owid_energy.pkl` its 128 numeric columns as float64
  — the same shape and size as the M5's files); covid `countries-aggregated.csv`; `year`, `population` and
  `log_timestamps` arrays as in `benchmark/rc3/format2/make_set.py`; MIT-BIH record 100; the 16 Loghub 2k samples
  concatenated (`loghub_all.log`, the M5's 4,458,875 bytes); xiph.org CIF clips. The scripts hold this VM's paths.

## 1. `balanced`: the format-2 leaf trials (codecs 21-23), gated

**Cause.** A callgrind profile of rc3 `balanced` on `owid_energy_f64.npy`: 5,674 M instructions against rc2's 539 M;
83% of them in `hz_try`, i.e. zstd level 19 over the Huffman stream of every leaf (Huffman streams up to 64 KiB took
level 19 directly, 30–60 ms each). The rest: the zero-run leaf's sub-stream selection (zstd, Huffman and again
Huffman-then-zstd at level 19 on each sub-stream) and a Huffman pass over every leaf. On the small integer files the
per-plane leaf's sub-streams did the same.

**What predicts a win.** Traced per leaf (`owid_energy_f64.npy`, `wide_test.csv`, 75 leaves ≥ 8 KiB):

- Codec 22 wins by 35–60% on leaves where zstd -3 already reaches 18–27% of the order-0 entropy at 2–3% of the raw
  size (DEC64 digit streams with long repeats), and by 28–43% on 116–141 KiB wide-table leaves; it loses on every
  leaf where zstd -3 is above ~60% of the entropy and the leaf is large. The gain comes from the strong zstd level,
  not from Huffman: zstd -1 over the Huffman stream of a 32 KiB sample is within ±10% of zstd -1 on the raw sample
  whether the leaf wins or not, so **sampled predictors do not work** (design B below).
- Codec 23 wins on sparse leaves (1.6–7% non-zero: 3–20% smaller) and gains 1–3% on leaves 17–27% non-zero.
  The exact order-0 estimate of the zero-run split separates winners (estimate/zstd 0.36–0.97) from losers (≥ 1.0),
  but costs a pass of 7 instructions per byte; estimated on 16 slices of 4 KiB it was off by up to 30%, and a
  geometric model of the runs from the histogram alone was off by 1.4–5×.

**Gate designs measured** (instructions ÷ rc2's, and the share of the format-2 gain kept = (no-format-2 size − size)
÷ (no-format-2 size − rc3 base size); `instructions.txt` has the raw counts and sizes; A–F are before the zero-run
split speed-up below, T8 and later after it):

| design | owid_energy_f64.npy | owid_energy.pkl | wide_test.csv | population.i64 | population_i64.npy | year_i32.npy | log_timestamps_ms.i64 | log_timestamps_ms_i64.npy |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| rc3 base: every trial | 10.53× / 100% | 10.40× / 100% | 7.56× / 100% | 4.34× / 100% | 5.85× / 100% | 12.50× / 100% | 2.14× / 100% | 3.10× / 100% |
| A: level-1 probe before the strong level for 22, exact zero-run estimate for 23 | 2.08× / 69% | 2.06× / 69% | 1.39× / 89% | 1.42× / 97% | 1.61× / 97% | 1.34× / 78% | 1.14× / 100% | 1.26× / 100% |
| B: three 32 KiB samples predict 22 and 23 | 1.59× / 43% | 1.61× / 46% | 1.32× / 86% | 1.49× / 97% | 1.72× / 97% | 1.30× / 78% | 1.14× / 100% | 1.25× / 100% |
| D: 23 only (sampled estimate) | 1.25× / 23% | 1.24× / 23% | 1.11× / 40% | 1.28× / 97% | 1.40× / 97% | 1.26× / 78% | 1.12× / 100% | 1.21× / 100% |
| F: 22 only (sampled) | 1.36× / 31% | 1.36× / 31% | 1.23× / 59% | 1.28× / 97% | 1.40× / 97% | 1.30× / 78% | 1.14× / 100% | 1.25× / 100% |
| T8 (time-first): 23 on leaves ≤ 1/8 non-zero, 22 only on leaves ≤ 64 KiB | 1.17× / 31% | 1.16× / 30% | 1.02× / 2% | 1.21× / 100% | 1.30× / 100% | 1.11× / 100% | 1.10× / 100% | 1.18× / 100% |
| S8: T8, plus 22 on leaves where zstd -3 < 30% of the entropy (level 9) | 1.46× / 76% | 1.45× / 76% | 1.17× / 47% | 1.23× / 100% | 1.33× / 100% | 1.11× / 100% | 1.10× / 100% | 1.18× / 100% |
| S8m: S8, 22 on leaves ≤ 192 KiB | 1.46× / 76% | 1.45× / 76% | 1.26× / 89% | 1.23× / 100% | 1.33× / 100% | 1.19× / 100% | 1.10× / 100% | 1.18× / 100% |
| **S8ml (chosen)**: S8m with level 5 | **1.36× / 67%** | **1.35× / 67%** | **1.18× / 78%** | **1.23× / 99%** | **1.33× / 99%** | **1.19× / 100%** | **1.10× / 100%** | **1.18× / 100%** |

All designs share: the level-1 probe of codec 22 before the strong level (taken only when the probe is already no
larger than the leaf's best), Huffman skipped inside 21 and 23 sub-streams when the block codec is already below
Huffman's bound (exact: Huffman cannot win there), and codec 23 at most 1/8 non-zero (A–F: per their estimate).
`eco`, `hc` and above keep every trial: their frames do not change.

**The target cannot be met together with "most of the gain" on the DEC64 files.** A codec-22 win needs a Huffman pass
over the whole leaf (≈30 instructions per byte) and a zstd pass over its Huffman stream: on the energy arrays the eight
winning leaves alone cost 25–35% of rc2's instructions with any gate that keeps them. Within 15% of rc2 the only
design is T8 (energy keeps 31% of the gain, wide 2%). **Choice: S8ml**, which keeps two thirds or more of the gain
everywhere and brings the regression from 9–10× to 1.3–1.5× (1.1–1.3× at 4 threads); T8 is two defines away
(`-DKG_HZ_FAST_RH=0 -DKG_HZ_FAST_MAX=65536`) if the owner prefers rc2's speed to the size there.

**Wall time, `balanced`** (s, fastest of 5; size change vs rc2):

| file | threads | rc2 bytes | base vs rc2 | new vs rc2 | compress s: rc2 / base / new | new ÷ rc2 | decompress s: rc2 / base / new |
|---|---:|---:|---:|---:|---:|---:|---:|
| owid_energy.pkl | 1 | 1,561,534 | -5.40% | -3.63% | 0.098 / 0.915 / 0.126 | 1.28× | 0.035 / 0.044 / 0.044 |
| owid_energy_f64.npy | 1 | 1,576,921 | -5.43% | -3.64% | 0.077 / 0.746 / 0.115 | 1.50× | 0.033 / 0.045 / 0.039 |
| wide_test.csv | 1 | 1,803,332 | -4.54% | -3.53% | 0.097 / 0.678 / 0.112 | 1.16× | 0.061 / 0.066 / 0.057 |
| year.i32 | 1 | 1,349 | +0.00% | +0.00% | 0.004 / 0.004 / 0.005 | 1.09× | 0.002 / 0.002 / 0.002 |
| year_i32.npy | 1 | 1,198 | -33.81% | -33.81% | 0.005 / 0.025 / 0.005 | 1.04× | 0.002 / 0.002 / 0.002 |
| population.i64 | 1 | 72,669 | -2.59% | -2.57% | 0.007 / 0.032 / 0.010 | 1.30× | 0.002 / 0.003 / 0.002 |
| population_i64.npy | 1 | 72,759 | -2.58% | -2.57% | 0.007 / 0.027 / 0.010 | 1.60× | 0.003 / 0.002 / 0.002 |
| log_timestamps_ms.i64 | 1 | 7,545 | -24.49% | -24.49% | 0.004 / 0.008 / 0.005 | 1.16× | 0.002 / 0.002 / 0.002 |
| log_timestamps_ms_i64.npy | 1 | 7,634 | -24.21% | -24.21% | 0.004 / 0.006 / 0.005 | 1.34× | 0.001 / 0.002 / 0.002 |
| mitdb-100.i16 | 1 | 864,758 | +0.00% | +0.00% | 0.047 / 0.038 / 0.035 | 0.75× | 0.012 / 0.012 / 0.013 |
| countries-aggregated.csv | 1 | 1,325,878 | -73.91% | -73.91% | 0.053 / 0.041 / 0.039 | 0.73× | 0.012 / 0.020 / 0.018 |
| energy_test.csv | 1 | 740,789 | +0.00% | +0.00% | 0.130 / 0.129 / 0.124 | 0.96× | 0.024 / 0.023 / 0.025 |
| owid_test.csv | 1 | 1,061,293 | +0.00% | +0.00% | 0.115 / 0.117 / 0.114 | 0.99× | 0.031 / 0.033 / 0.031 |
| owid-co2-data.csv | 1 | 1,392,694 | +0.00% | +0.00% | 0.158 / 0.163 / 0.180 | 1.14× | 0.043 / 0.048 / 0.050 |
| x-ray | 1 | 3,657,342 | +0.00% | +0.00% | 0.039 / 0.029 / 0.038 | 0.95× | 0.019 / 0.018 / 0.022 |
| mr | 1 | 2,316,612 | +0.00% | +0.00% | 0.066 / 0.064 / 0.063 | 0.96× | 0.030 / 0.048 / 0.045 |
| loghub_all.log | 1 | 316,254 | +0.00% | +0.00% | 0.032 / 0.030 / 0.034 | 1.03× | 0.011 / 0.013 / 0.010 |
| dickens | 1 | 3,721,196 | +0.00% | +0.00% | 0.084 / 0.074 / 0.095 | 1.14× | 0.021 / 0.017 / 0.017 |
| owid_energy.pkl | 4 | 1,561,534 | -5.40% | -3.63% | 0.054 / 0.241 / 0.061 | 1.12× | 0.022 / 0.026 / 0.024 |
| owid_energy_f64.npy | 4 | 1,576,921 | -5.43% | -3.64% | 0.045 / 0.270 / 0.057 | 1.26× | 0.022 / 0.024 / 0.026 |
| wide_test.csv | 4 | 1,803,332 | -4.54% | -3.53% | 0.047 / 0.250 / 0.054 | 1.16× | 0.024 / 0.027 / 0.025 |
| year.i32 | 4 | 1,349 | +0.00% | +0.00% | 0.006 / 0.005 / 0.005 | 0.85× | 0.002 / 0.002 / 0.002 |
| year_i32.npy | 4 | 1,198 | -33.81% | -33.81% | 0.005 / 0.029 / 0.006 | 1.24× | 0.002 / 0.003 / 0.002 |
| population.i64 | 4 | 72,669 | -2.59% | -2.57% | 0.008 / 0.033 / 0.010 | 1.27× | 0.003 / 0.003 / 0.003 |
| population_i64.npy | 4 | 72,759 | -2.58% | -2.57% | 0.007 / 0.032 / 0.009 | 1.22× | 0.003 / 0.003 / 0.003 |
| log_timestamps_ms.i64 | 4 | 7,545 | -24.49% | -24.49% | 0.004 / 0.009 / 0.006 | 1.51× | 0.002 / 0.002 / 0.002 |
| log_timestamps_ms_i64.npy | 4 | 7,634 | -24.21% | -24.21% | 0.004 / 0.007 / 0.004 | 1.16× | 0.002 / 0.002 / 0.002 |
| mitdb-100.i16 | 4 | 864,758 | +0.00% | +0.00% | 0.042 / 0.033 / 0.032 | 0.75× | 0.008 / 0.008 / 0.008 |
| countries-aggregated.csv | 4 | 1,325,878 | -73.91% | -73.91% | 0.028 / 0.024 / 0.024 | 0.86× | 0.008 / 0.011 / 0.010 |
| energy_test.csv | 4 | 740,789 | +0.00% | +0.00% | 0.066 / 0.063 / 0.062 | 0.94× | 0.014 / 0.015 / 0.013 |
| owid_test.csv | 4 | 1,061,293 | +0.00% | +0.00% | 0.046 / 0.050 / 0.050 | 1.09× | 0.015 / 0.013 / 0.015 |
| owid-co2-data.csv | 4 | 1,392,694 | +0.00% | +0.00% | 0.067 / 0.069 / 0.070 | 1.05× | 0.020 / 0.020 / 0.020 |
| x-ray | 4 | 3,657,342 | +0.00% | +0.00% | 0.036 / 0.030 / 0.033 | 0.92× | 0.010 / 0.011 / 0.010 |
| mr | 4 | 2,316,612 | +0.00% | +0.00% | 0.053 / 0.046 / 0.045 | 0.86× | 0.014 / 0.020 / 0.020 |
| loghub_all.log | 4 | 316,254 | +0.00% | +0.00% | 0.019 / 0.019 / 0.020 | 1.07× | 0.007 / 0.008 / 0.009 |
| dickens | 4 | 3,721,196 | +0.00% | +0.00% | 0.037 / 0.037 / 0.037 | 1.00× | 0.011 / 0.010 / 0.010 |

`balanced` decode is unchanged (the leaves chosen decode at the same speed). Frames change on the files above where
the size column differs between base and new (energy, pickle, wide, population); `year`, the timestamps and every
other file keep their bytes in `balanced`.

**Same-output speed-up.** `zr_split` (the zero-run split) skips zero words 8 bytes at a time: on the energy array the
split was 68 M of the 120 M instructions the zero-run trials cost. Checked against the old split on 20,000 random
streams of every density (and by the format-2 tests).

## 2. GR6+ (codec 26) in `-x`: gated

GR6+ costs a formula search, O(columns² × 10) candidates per target column, then a context-mixing pass at about
3 MB/s on this VM. It never won on prose, logs or JSON, nor on wider tables.

- **Table gate** (`gr6_applies`): the separator (the same rule as the codec: the most frequent of `, TAB ; |` and
  space in the first MiB), then at least 90% of the first 256 lines with the same field count, 3 to 100. Decisions:
  tried on covid, owid_test, OWID CO2 (the three files where it wins) and Loghub's HealthApp sample (a `|` table);
  skipped on energy_test.csv (130 columns), wide_test.csv (316), dickens, nci, webster, xml, reymont, samba, mozilla,
  the other 15 Loghub samples and their concatenation.
- **Work cap** in the formula search: 2^29 rows scored (screening included), counted, not timed, so the frame is the
  same everywhere. Measured need (rows scored): covid 96 K (5 columns, no formula), owid_test 292 M (31 formulas),
  OWID CO2 339 M (32 formulas), energy_test 3.9 G (77 formulas, never chosen). The cap leaves every winning file's
  formulas as they were.
- GR6+ alone on this VM (`g6tool`): dickens 8.3 s, energy_test 24.8 s of encode for nothing; covid 1.9 s, owid_test
  7.5 s, OWID CO2 8.8 s where it wins.

**`-x -T 1`, rc3 base against new** (`x_t1.jsonl`; one run each, same machine state, SHA-256 of every frame compared):

| file | `-x` bytes | frames identical | compress s base → new | decode s (new) |
|---|---:|---|---:|---:|
| countries-aggregated.csv | 283,038 | yes | 44.5 → 37.3 | 1.9 |
| energy_test.csv | 614,448 | yes | 65.4 → **36.9** | 19.8 |
| owid_test.csv | 856,504 | yes | 59.6 → 59.4 | 5.3 |
| owid-co2-data.csv | 1,127,837 | yes | 69.6 → 72.0 | 7.1 |
| loghub_all.log | 184,718 | yes | 33.6 → 31.9 | 28.1 |
| wide_test.csv | 1,482,094 | yes | 4.6 → 4.2 | 0.1 |
| dickens | 2,062,661 | yes | 91.1 → **81.3** | 75.5 |

Every frame decoded to its input. The first rows ran while variant builds shared the machine: covid's 44.5 s is high
(GR6+ is still tried there; nothing changed for it), the others alternate base and new within a minute of each other.

**`-x` on the text group after the gate.** The M5 run found `-x` about 9% slower than rc2 on dickens, nci and webster,
all of it GR6+ trying and losing. All three now fail the table gate (separator space, field counts that vary from
line to line), so `-x` does exactly what it did before GR6+ was added: same frames, and dickens compresses in 81.3 s
instead of 91.1 s here (−11%; GR6+ alone takes 8.3 s on it). nci and webster were not re-timed with `-x` on this VM
(minutes per run); their frames cannot change (GR6+ was never chosen on them) and the time GR6+ took on them is now
skipped. The integration lane's M5 run will show the time.

## 3. Video (codec 20): frames on all threads

**Cause.** The video route codes the whole Y4M as one block whose payload is a nested Kolmo frame of the kvid layout.
The nested frame is block-parallel already; the layout itself was built (`kv_transform`) and inverted (`kv_inverse`)
on one thread over the whole file, with the portable C prediction (the simd-x86 and simd-arm lanes' strip kernels,
`KOLMO_K->vid_strip_enc/dec`, were only used by the research tool `src/experimental/kvid.c`). At 4 threads the
serial inverse was most of the decode: rc3 decoded 0.40–0.46 s per clip where rc2's plain blocks took 0.10 s.

**Fix (same bytes, no size cost).** The encoder predicts from the source samples only, so `kv_transform` codes the
frames in parallel (`kolmo_par_for`, frames handed out in order). The decoder rebuilds frames as a wavefront: a strip
that uses the previous frame reads only that frame's rows of the same strip (and the row above, already done), so
frame f decodes strip s as soon as frame f − 1 has finished it; every frame is one strip behind the one before. Frames
are handed out in order, so the frame a worker waits for is always being decoded: no deadlock with any thread count,
including one. A bad predictor byte stops the frame and releases its waiters; the inverse then fails. Both paths call
the dispatched SIMD strip kernels. Frames are byte-identical to rc3's at every thread count and at every ISA level
(`KOLMO_CPU=c`, `sse2`, `avx2` checked), so the frame groups the plan proposed (an intra frame per group, a size
cost) were not needed: the wavefront gives the threads without them.

The decoder needs about two strips per thread of lead to keep every thread busy: with 300 frames there is far more
parallelism than threads. Measured (s; sizes identical between base and new):

`-m balanced`:

| clip | threads | rc2 bytes | rc3 vs rc2 | compress s: rc2 / base / new | decompress s: rc2 / base / new | new decode vs rc2 |
|---|---:|---:|---:|---:|---:|---:|
| akiyo_cif.y4m | 1 | 18,118,923 | -53.61% | 0.19 / 0.86 / 0.35 | 0.320 / 0.236 / 0.138 | 2.32× faster |
| foreman_cif.y4m | 1 | 23,156,754 | -3.05% | 0.41 / 1.48 / 0.65 | 0.329 / 0.518 / 0.173 | 1.90× faster |
| mobile_cif.y4m | 1 | 32,418,001 | -7.47% | 0.70 / 1.28 / 0.63 | 0.342 / 0.527 / 0.190 | 1.80× faster |
| akiyo_cif.y4m | 4 | 18,118,923 | -53.61% | 0.09 / 0.74 / 0.14 | 0.099 / 0.168 / 0.066 | 1.50× faster |
| foreman_cif.y4m | 4 | 23,156,754 | -3.05% | 0.16 / 0.97 / 0.22 | 0.100 / 0.402 / 0.094 | 1.06× faster |
| mobile_cif.y4m | 4 | 32,418,001 | -7.47% | 0.19 / 0.98 / 0.23 | 0.097 / 0.456 / 0.080 | 1.20× faster |

`-m eco`:

| clip | threads | rc2 bytes | rc3 vs rc2 | compress s: rc2 / base / new | decompress s: rc2 / base / new | new decode vs rc2 |
|---|---:|---:|---:|---:|---:|---:|
| akiyo_cif.y4m | 1 | 13,250,853 | -41.92% | 13.38 / 6.94 / 6.64 | 0.263 / 0.357 / 0.266 | 1.01× slower |
| foreman_cif.y4m | 1 | 22,690,008 | -3.55% | 22.95 / 21.88 / 20.38 | 0.495 / 0.782 / 0.411 | 1.20× faster |
| mobile_cif.y4m | 1 | 31,998,231 | -7.58% | 24.45 / 24.91 / 23.87 | 0.560 / 0.925 / 0.528 | 1.06× faster |
| akiyo_cif.y4m | 4 | 13,250,853 | -41.92% | 5.01 / 3.30 / 2.64 | 0.138 / 0.236 / 0.139 | 1.01× slower |
| foreman_cif.y4m | 4 | 22,690,008 | -3.55% | 9.24 / 9.18 / 7.96 | 0.198 / 0.579 / 0.199 | 1.01× slower |
| mobile_cif.y4m | 4 | 31,998,231 | -7.58% | 9.70 / 9.75 / 9.75 | 0.264 / 0.625 / 0.196 | 1.35× faster |

`-m hc`:

| clip | threads | rc2 bytes | rc3 vs rc2 | compress s: rc2 / base / new | decompress s: rc2 / base / new | new decode vs rc2 |
|---|---:|---:|---:|---:|---:|---:|
| akiyo_cif.y4m | 1 | 12,953,821 | -46.08% | 32.71 / 31.34 / 30.18 | 0.152 / 0.239 / 0.177 | 1.17× slower |
| foreman_cif.y4m | 1 | 22,973,573 | -7.92% | 44.16 / 44.75 / 45.43 | 0.403 / 0.547 / 0.231 | 1.74× faster |
| mobile_cif.y4m | 1 | 32,352,672 | -8.65% | 31.83 / 37.92 / 36.62 | 0.406 / 0.543 / 0.467 | 1.15× slower |
| akiyo_cif.y4m | 4 | 12,953,821 | -46.08% | 15.01 / 11.43 / 11.20 | 0.401 / 0.192 / 0.100 | 4.03× faster |
| foreman_cif.y4m | 4 | 22,973,573 | -7.92% | 19.37 / 19.26 / 18.76 | 0.203 / 0.473 / 0.148 | 1.37× faster |
| mobile_cif.y4m | 4 | 32,352,672 | -8.65% | 13.37 / 15.06 / 13.86 | 0.195 / 0.539 / 0.133 | 1.47× faster |

`-m ratio`:

| clip | threads | rc2 bytes | rc3 vs rc2 | compress s: rc2 / base / new | decompress s: rc2 / base / new | new decode vs rc2 |
|---|---:|---:|---:|---:|---:|---:|
| akiyo_cif.y4m | 1 | 11,232,641 | -45.19% | 45.99 / 70.49 / 71.08 | 1.141 / 2.190 / 2.625 | 2.30× slower |
| foreman_cif.y4m | 1 | 20,987,517 | -1.54% | 81.18 / 81.13 / 77.94 | 3.537 / 1.659 / 1.265 | 2.80× faster |
| mobile_cif.y4m | 1 | 30,246,791 | -5.33% | 61.04 / 59.99 / 57.94 | 5.699 / 1.991 / 1.676 | 3.40× faster |
| akiyo_cif.y4m | 4 | 11,232,641 | -45.19% | 44.27 / 61.17 / 59.02 | 0.964 / 0.919 / 0.681 | 1.42× faster |
| foreman_cif.y4m | 4 | 20,987,517 | -1.54% | 67.78 / 64.17 / 63.35 | 3.160 / 1.376 / 1.065 | 2.97× faster |
| mobile_cif.y4m | 4 | 30,246,791 | -5.33% | 48.53 / 47.69 / 48.20 | 4.213 / 1.629 / 1.405 | 3.00× faster |

At 4 threads video now decodes as fast as rc2 or faster in every mode (`balanced` 1.06–1.50×, `eco` 0.99–1.35×,
`hc` 1.37–4.0×, `ratio` 1.4–3.0× rc2's speed), against 0.22–0.76× before. At one thread `balanced` decode is
1.8–2.3× faster than rc2 (the SIMD kernels); `hc` at one thread is still 13–16% slower than rc2 on akiyo and mobile
(the nested `hc` frame itself). The `ratio` akiyo row at one thread (2.63 s) is a single run that hit a noisy moment:
run alone, best of 3, base decodes it in 1.84 s and new in 1.71 s. `balanced` compression at 4 threads went from
4–8× rc2's time to 1.2–1.5×.

## 4. `ratio`: static QLFC where adaptive gains under 1%

Static QLFC decodes its stage 2.0–2.6× faster than adaptive QLFC (per coder block, one thread: dickens 0.27 s
against 0.69 s). Codec 18's flags byte already names the coder per block, so this is an encoder choice with no format
change: code the block both ways, keep static when adaptive is less than 1% smaller (`static_pct` in
`kolmo_bwt_params`). Adaptive gained 0.38–2.41% per block on the sets; under 1% on dickens (0.59%), mr's large block
(0.88%) and four blocks of the energy array (0.38–0.87%).

`ratio` (s; "adaptive" = rc3 as shipped, "static < 1%" = the rule on):

| file | threads | rc2 bytes | adaptive vs rc2 | static < 1% vs rc2 | compress s: rc2 / adaptive / static<1% | decompress s: rc2 / adaptive / static<1% |
|---|---:|---:|---:|---:|---:|---:|
| dickens | 1 | 2,274,350 | -0.87% | -0.29% | 0.70 / 0.98 / 1.13 | 0.354 / 0.619 / 0.403 |
| nci | 1 | 1,225,386 | -2.10% | -2.10% | 4.08 / 4.36 / 4.43 | 0.413 / 0.569 / 0.544 |
| osdb | 1 | 2,276,454 | -3.07% | -3.07% | 5.82 / 1.02 / 1.26 | 0.387 / 0.634 / 0.679 |
| owid_energy_f64.npy | 1 | 1,403,314 | -2.66% | -2.20% | 0.28 / 0.77 / 1.02 | 0.260 / 0.377 / 0.279 |
| mr | 1 | 2,014,182 | -1.21% | -0.33% | 4.25 / 4.67 / 4.85 | 0.298 / 0.548 / 0.328 |
| x-ray | 1 | 3,616,667 | +0.00% | +0.00% | 2.33 / 2.66 / 2.69 | 0.274 / 0.189 / 0.177 |
| sao | 1 | 2,184,759 | +0.00% | +0.00% | 1.29 / 1.36 / 1.42 | 0.130 / 0.095 / 0.092 |
| loghub_all.log | 1 | 246,107 | +0.00% | +0.00% | 0.49 / 0.50 / 0.55 | 0.032 / 0.025 / 0.026 |
| dickens | 4 | 2,274,350 | -0.87% | -0.29% | 0.73 / 0.68 / 0.84 | 0.339 / 0.304 / 0.178 |
| nci | 4 | 1,225,386 | -2.10% | -2.10% | 4.07 / 4.06 / 3.99 | 0.415 / 0.222 / 0.208 |
| osdb | 4 | 2,276,454 | -3.07% | -3.07% | 6.02 / 0.74 / 0.97 | 0.367 / 0.340 / 0.350 |
| owid_energy_f64.npy | 4 | 1,403,314 | -2.66% | -2.20% | 0.29 / 0.86 / 1.05 | 0.259 / 0.356 / 0.279 |
| mr | 4 | 2,014,182 | -1.21% | -0.33% | 4.03 / 4.38 / 4.49 | 0.281 / 0.514 / 0.269 |
| x-ray | 4 | 3,616,667 | +0.00% | +0.00% | 2.38 / 2.40 / 2.54 | 0.294 / 0.158 / 0.161 |
| sao | 4 | 2,184,759 | +0.00% | +0.00% | 1.32 / 1.29 / 1.40 | 0.160 / 0.095 / 0.090 |
| loghub_all.log | 4 | 246,107 | +0.00% | +0.00% | 0.52 / 0.59 / 0.50 | 0.026 / 0.026 / 0.024 |

**Not clearly better, so not on by default** (`KOLMO_BWT_STATIC_PCT` 0; the owner's decision to keep adaptive QLFC
stands). Where the rule applies, decode is 1.35–1.9× faster (dickens 0.62 → 0.40 s, mr 0.55 → 0.33 s, energy
0.38 → 0.28 s at one thread), but those files give back two thirds of what adaptive QLFC gained (dickens −0.87% →
−0.29%, mr −1.21% → −0.33%, energy −2.66% → −2.20% against rc2), and every block pays a second QLFC pass: +1% to +33%
compress time, also where nothing changes (osdb +23%). A cheaper predictor of the adaptive gain was not found (it
varies from 0.4% to 2.4% by block, text lowest). The parameter stays, so the integration lane can turn it on with one
define if the owner prefers decode speed there. `ratio` frames are unchanged.

## Tests

All on this VM, on the final tree:

| target | result |
|---|---|
| `make check` | ALL PASSED (with the new GR6+ gate checks in `kolmotest`: the derived-column table is tried, prose and a 120-column table are not) |
| `make check-rc` | passed (with `check-format2`: the video layout gives the same bytes on 1-8 threads, the wavefront inverse decodes on 1-8 threads, and a bad predictor byte in frames 0, 3, 11, 16, 22 and 29 fails the inverse on 1 and 4 threads without hanging; also under ASan/UBSan) |
| `make check-sanitizers` | passed |
| `make experimental-check GPU_CHECK=` | passed |
| `make kfam-check` | passed |
| `python3 benchmark/determinism/check.py build/kolmo build/determinism` | 122 frames checked, OK, unchanged |

The determinism set is unchanged: its seeded inputs do not reach the `balanced` choices that changed (none of their
leaves is a large DEC64 leaf, and its small integer leaves keep their codecs), video and `-x` frames are byte-identical
by construction, and `ratio` is unchanged with static QLFC off. So `expected.sha256` was not regenerated. Frames that
do change (on purpose): `balanced` on `owid_energy.pkl`, `owid_energy_f64.npy`, `wide_test.csv`, `population.i64` and
`population_i64.npy` of this lane's set (sizes in section 1). The zero-run split was also checked against the old split
on 20,000 random streams.

## Commands

```
python3 -I mkdata.py DATA                                  # builds DATA/set from the downloads (see Data)
python3 -I bench.py OUT.jsonl 5 1 "-m balanced" ./kolmo-rc2:rc2 ./kolmo-base:base ./kolmo-new:new -- FILES
MODE="-m balanced" sh ir.sh BIN FILE [VAR=value]           # instructions (callgrind) and size
python3 -I xrun.py OUT.jsonl 1 FILES                       # -x, base against new, frame hashes
```
