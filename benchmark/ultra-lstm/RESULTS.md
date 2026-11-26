# Ultra + LSTM mixer input (claim 68): a reproducible negative result

Handoff task 2 (`benchmark/waves/2026-11-25/overnight/CLAUDE-CODE-HANDOFF.md` on `codex/ppp-audit-and-results-20261125`)
asks whether the Claude-owned Ultra model is worth implementing.

**Answer: no, not in this form.** The archival gate rejects the candidate. On the sealed holdout the LSTM saves **4.8%** of bytes, which
passes the byte gate (≤0.98×). On dev it saves 0.6%. The real problem is speed. In the timed quiet-window run (§5a:
3 rounds, shuffled, one thread, load 2.3–8.5), the candidate runs at **0.030 MB/s** in each direction against
0.93–1.06 MB/s for stock Ultra: **0.029–0.032× encode and 0.030× decode throughput**, and **32–34× the combined
latency**. The gate asks for ≥0.67×, ≥0.95× and ≤1.50×. Peak RSS passes (0.85–1.01×). No size of this LSTM can close
a gap that large. The byte counts are deterministic and exact, and all 36 timed round trips are SHA-256 verified.

The holdout byte gain is the most useful finding. It suggests that an LSTM-class predictor pays off on text unlike
model 4's tuning data (here Polish). A cost-reduced version (a quantised or fixed-point LSTM, NEON kernels, or an LSTM
used only on blocks that a cheap probe admits) is a separate experiment. It would have to recover a factor of about 30 in speed.

Nothing here changes the release candidate, the stable writer, any reader, or any default. The code is a research harness
outside every build target. The candidate stream is a research container that no Kolmo reader accepts.

## 1. Recovery: what R16 actually left behind

| item | what exists | where |
|---|---|---|
| Claim 68 `cm-lstm-input` ("execute") | **No LSTM code was ever written.** R16 measured paq8px v217 `-8L` against `-8` as a proxy: −0.71% on the first 1,000,000 B of dickens and −0.41% on xml. It quoted LTCB at −1.2% on enwik8. It set its own go/no-go gate: at least −1.5% on texts of 4 MB or more, at no more than 3× the decode time. | `review40b/reviews/R16.md` (ranked plan, item 1; *Claims registered*) |
| Claim 66 `cm-neural-text-codec` ("planned") | **No code, no model, no measurement.** It is a literature row (ts_zip: RWKV 169M, GPU, enwik8 13.8 MB). Its effort is estimated in months, with open licence, training-data and GPU-determinism risks. | `R16.md` ranked plan, item 2 |
| R16 prototype code | A copy of `cm.c` with `XP` flags for the F3 stage bundle (APMs, mixer sets, word contexts, 16-bit checks), the harnesses `h.c`, `h2.c`, `h3.c`, and `cap.py` (case flags). None of it is an LSTM or a neural model. | `review40b/work/R16/proto/`, `cap.py` |
| R16 measured results | F2: model 4 is +0.96% against model 3. F3: the best bundle is −0.2% to −0.4% at default tables for 1.8× CPU, and −1.2% with 4× tables. F4: case flags +0.3%. F1: on 1 MB slices paq8px -8 is 9.8–31% smaller. | `R16.md` F1–F4 |
| Format dependencies | Claim 68 proposed CM **model nibble 10** (still the next free nibble in the SPEC.md registry). Claim 66 proposed **block codec 40**. Neither was assigned or written down. R16 F8 recommends that model 4 itself be specified bit by bit (FORMAT §24, a Python reference) before any model 5. | SPEC.md *Model nibble registry*; `R16.md` F8 |
| Ledger | The audit marks both claims 66 and 68 as "historical claim only … current implementation and gain unqualified". | `audit/legacy-claim-audit.csv` rows 66, 68 |

## 2. Prior results read before choosing (reused, not re-measured)

| prior result | outcome | bearing on the choice |
|---|---|---|
| CM model 4 explicit-table repair (Codex, `ultra/README.md`) | The numeric17 cap moves bytes by ±0.3–0.6% and memory by about 25%. It is not a default change. | Shows the size of the levers left inside model 4 |
| Alternate v12 CM4 mask (#88) | 0 bytes saved in 12 settings, with a CPU/wall cost. Rejected. | Rules out more context reshuffling |
| Lexer token-class contexts and per-kind priors (`benchmark/impact/RESULTS.md`, JXL 128 KiB replay) | 20,404 / 20,463 / 20,053 B against 20,830 (−2.0% / −1.8% / −3.7%) on one small code stream, research decoder only. Needs a model-format decision. | Small-stream wins. The lexer still needs the format decision this file also faces. |
| Prior packs (R11 #56) | Shipping readers rejected Lexi/prior extensions | A trained-weight side file is a known no-go for shipping readers |
| GR6+ | Kept gated. Below Ultra: 3–11 MB/s decode and 6–25× decode memory on admitted blocks. | The cost profile a heavier model must beat |
| Reset / confidence expert / Lexi LUT counterexamples (#48, #91) | Negative | Ruled out cheap mixer additions. R16 F3 also measured a learned final mixer at +0.36%. |

**Choice.** Of the two Claude-owned claims, only claim 68 can be built and measured on a CPU in one sitting. It also
has the smallest format footprint: one new model nibble and no shipped weights. Claim 66 needs a trained 100+ MB
model, a GPU path and a training-data licence decision. It cannot be a "production candidate" without those. The
experiment is therefore **one candidate: model 4 unchanged, plus a byte-level LSTM (cmix/lstm-compress style, one
layer, coupled input/forget gate, softmax over 256 bytes, online Adam/BPTT), whose bit probability enters a small
learned 3-input final mixer with model 4's output.** It learns online from zero, with deterministic initialisation,
and has no model side data.

## 3. Inputs (one dev producer, one sealed independent holdout)

| role | input | bytes | SHA-256 | producer |
|---|---|---:|---|---|
| dev | `dickens4m`: first 4,194,304 B of Silesia `dickens` | 4,194,304 | `6c7a2dec…6215` | Charles Dickens, English, Project Gutenberg |
| holdout | `reymont`: Silesia `reymont`, whole file, from `corpus-holdout/` | 6,627,202 | `0eac0114…7578` | W. Reymont, *Chłopi*, Polish, a different author, language and typesetting source |
| startup | `tiny1`: the single byte `K` | 1 | `86be9a55…b177` | — |

Both text inputs are at least 4 MB, which is R16's own gate size. The holdout was hashed and set aside before the
first harness run. Every setting was chosen on dev prefixes only (`dev-sweep.md`). The candidate was frozen
(`frozen-SHA256SUMS`) before any process read the holdout. Full hashes are in `inputs-SHA256SUMS`.

## 4. Arms and method

- **stock**: `kolmo -U -T 1 -c IN > frame`, then `kolmo -d -T 1 -c frame`. Pinned main `a1d39559`, built with the repo
  Makefile (`-O3 -mcpu=native -ffp-contract=off`).
- **control**: `lstm_h enc0/dec0`, the production model-4 encoder and decoder called from the harness process. It is
  a duplicate control that isolates harness I/O from the LSTM.
- **candidate**: `lstm_h enc/dec` with the frozen settings, in the `KLS1` container (a 24-byte header).
- **zpaq -m5 -t1** (7.15): the strongest *practical* competitor on text in the 24 Nov one-thread archiver run
  (`evidence/ARCHIVERS_2026-11-24.md`), where `-m58` = `-m5` for inputs under 64 MB. cmix v21 is not practical
  here: the recorded probe ran at 0.44 kB/s with 8.5 GB on 200 kB of dickens, so a round trip of the holdout would
  take about 8 h. paq8px is Codex's lane. Its recorded numbers are reused below, and no new paq8px runs were made.
- **Runner** (`run.py`): every cell, (round, input, arm), holds `$V/tools/timed.sh` for its encode and decode.
  Each process is measured with `os.wait4`: wall time, user and system CPU, and `ru_maxrss`. File I/O and process
  startup are included, and hashing happens after timing. Every restored file is checked against the input
  SHA-256. There are 3 rounds. The cell order within each round is shuffled with seed 20261125 and written to
  `order.json` before the first cell runs. Each cell writes its own JSON, so a stopped run resumes at the first
  missing cell.
- **Gate**: the archived ratio-priority promotion gate for L8/L9/Ultra in `prior-routing/README.md`, applied to the
  candidate against stock on the same input:
  - ≤0.98× stored bytes;
  - ≥0.67× encode throughput;
  - ≥0.95× decode throughput;
  - ≤1.10× peak RSS in each phase;
  - ≤1.50× median combined latency.

## 5a. Timed run (quiet window, 26 November 2026 04:47–05:29Z)

The coordinator ran `run.py` with `config-quiet.json` inside one `$V/tools/timed.sh` hold. That is 36 cells:
3 rounds × {dickens4m, reymont, tiny1} × {stock, control, candidate, zpaq}, shuffled with seed 20261125. The order is in
`timed-run1-order.json`, the medians in `timed-run1-summary.json`, and the raw per-cell JSON in `$V/ultra-lstm/run1/cells/`. The 1-minute load was 2.3–8.5
across all cells. The full `report.py` output, with every hash, is in `timed-run1.md`. Medians over 3 rounds; MB/s is
10^6 input bytes per wall second, including process start and file I/O.

| input | arm | stored B | enc MB/s | dec MB/s | enc / dec RSS MiB | enc / dec CPU s |
|---|---|---:|---:|---:|---:|---:|
| dev dickens4m | stock `-U -T 1` | 886,241 | 0.931 | 0.983 | 186.0 / 151.3 | 4.50 / 4.26 |
| dev dickens4m | control | 886,193 | 1.065 | 0.982 | 155.1 / 151.1 | 3.93 / 4.27 |
| dev dickens4m | **candidate** | **880,692** | **0.0301** | **0.0300** | 157.3 / 153.3 | 139.2 / 139.9 |
| dev dickens4m | zpaq -m5 -t1 | 892,624 | 0.654 | 0.625 | 195.8 / 197.4 | 6.39 / 6.71 |
| holdout reymont | stock `-U -T 1` | 874,420 | 1.062 | 1.012 | 262.4 / 229.8 | 6.24 / 6.54 |
| holdout reymont | control | 874,372 | 1.101 | 1.013 | 235.9 / 229.5 | 6.01 / 6.54 |
| holdout reymont | **candidate** | **832,051** | **0.0304** | **0.0304** | 238.5 / 232.1 | 218.1 / 217.7 |
| holdout reymont | zpaq -m5 -t1 | 956,498 | 0.625 | 0.603 | 211.3 / 211.5 | 10.60 / 10.98 |

Every cell's restored SHA-256 equals its input SHA-256. The frame hashes match the byte runs below (for example,
candidate reymont `540493c3…529d` and candidate dickens4m `3df42aad…cea2`), so the encoder is deterministic across runs.

**Startup (1-byte input, medians).** stock 3.5 / 2.4 ms encode / decode; control 3.1 / 2.4 ms; candidate 3.7 / 2.9 ms
(of which 0.54 ms is in-process model and LSTM initialisation); zpaq 13.4 / 12.3 ms. Startup is not where the cost is.
The candidate stores 0 bytes of model side data. Its container is 24 B, against 52 B of frame overhead for stock.

**Gates, candidate against stock Ultra (timed).**

| input | bytes (≤ 0.98) | enc thr (≥ 0.67) | dec thr (≥ 0.95) | enc RSS (≤ 1.10) | dec RSS (≤ 1.10) | combined latency (≤ 1.50) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| dev dickens4m | 0.994 **fail** | 0.032 **fail** | 0.030 **fail** | 0.85 pass | 1.01 pass | 31.8 **fail** | **reject** |
| holdout reymont | 0.952 pass | 0.029 **fail** | 0.030 **fail** | 0.91 pass | 1.01 pass | 34.1 **fail** | **reject** |

**Isolating the LSTM (candidate against control, the same harness with the LSTM off):** 35–36× the encode time and
33× the decode time, for −0.62% (dickens4m) and −4.84% (reymont) of bytes. The quiet-host ratios are worse than the
indicative CPU ratios below (about 20× on dev1m). Even in CPU seconds the quiet ratio is about 31× (139.2 s against
4.50 s on dickens4m). The likely cause is contention: on the loaded host, stock's CPU seconds were inflated more than
the candidate's. That was not isolated, so treat the §5 ratios as optimistic for the candidate.

## 5. Results (indicative: one untimed `nice -n 15` pass on a loaded host, 25 November 2026 ~19:40–22:30Z)

These numbers came first, and §5a supersedes their speeds. The byte counts are the same in both.

Binaries (`frozen-SHA256SUMS`): `kolmo` built from main a1d39559 (`b67bbddb…`) and `lstm_h` (`b6b77104…`), compiled from
`lstm_h.c` (`5009b468…`). zpaq 7.15 (`9a5c86a4…`). Speeds are CPU seconds (user + sys) because the 1-minute load was
44–235 during these runs. Wall times are listed only for completeness. RSS is `/usr/bin/time -l` maximum resident set.

### Dev: `dev1m` (first 1,048,576 B of dickens, SHA-256 `afd479dd…a9ab`)

| arm | stored B | vs stock | enc CPU s | dec CPU s | enc / dec RSS MiB | enc / dec wall s (loaded) | restored SHA-256 |
|---|---:|---:|---:|---:|---:|---:|---|
| stock `kolmo -U -T 1` | 229,724 | 1 | 4.04 | 2.72 | 61.0 / 44.8 | 38.7 / 19.4 | `afd479dd…` OK |
| control (`enc0`, model 4 in the harness) | 229,676 | 0.9998 | 2.46 | 2.00 | 45.4 / 46.3 | 14.7 / 5.9 | `afd479dd…` OK |
| **candidate** (model 4 + LSTM H128 T30) | **228,445** | **0.9944** | **72.41** | **64.15** | 46.8 / 46.4 | 533.7 / 240.3 | `afd479dd…` OK |
| zpaq -m5 -t1 | 232,202 | 1.0108 | 3.81 | 4.18 | 103.9 / 102.6 | 18.8 / 29.6 | `afd479dd…` OK |

Frame SHA-256: stock `8a352cdb…f300`, control `252f0b32…a3a1`, candidate `8bf34b7c…e617`, zpaq `6036438d…de4b`. The
candidate's in-process initialisation (model 4 tables plus LSTM weights) took 1.3 ms to encode and 1.1 ms to decode.
The candidate transmits **0 bytes of model side data**: the LSTM starts from a fixed seed and learns online. Its
container header is 24 B, against 16 B for the control and 52 B of frame overhead for stock.

### Holdout: `reymont` (whole file, SHA-256 `0eac0114…7578`, sealed until the candidate was frozen)

| arm | stored B | vs control (model 4) | notes |
|---|---:|---:|---|
| stock `kolmo -U -T 1` | 874,420 | 1.0001 | frame `0137e1b0…af16`; the same size as the 24 Nov archiver row |
| control (`enc0`) | 874,372 | 1 | frame `1e21d55b…2032` |
| **candidate** | **832,051** | **0.9516 (−4.84%); 0.9515 vs stock** | frame `540493c3…529d`; restored SHA-256 `0eac0114…7578` OK (decode wall 1,536 s at load ~100+); ideal code length: model 4 alone 874,353 B, LSTM alone 970,199 B, mixed 832,023 B; encode wall 2,269 s at load ~90–235 |
| zpaq -m5 (recorded 24 Nov, same SHA-256) | 956,498 | 1.0939 | `evidence/ARCHIVERS_2026-11-24.md` rows |
| paq8px v217 -8 (recorded 24 Nov, Codex lane, not re-run) | 743,538 | 0.8504 | 282 s / 284 s, 2.6 GB RSS; the candidate is still 11.9% larger |

**Holdout decode check: done.** The candidate frame decoded to the exact input (SHA-256 `0eac0114…7578`).

### Dev at full size: `dickens4m` (4,194,304 B, SHA-256 `6c7a2dec…6215`; R16's ≥4 MB gate size)

| arm | stored B | vs stock | frame SHA-256 | notes |
|---|---:|---:|---|---|
| stock `kolmo -U -T 1` | 886,241 | 1 | `8f8e4775…9584` | |
| control (`enc0`) | 886,193 | 0.9999 | `b8799e84…1d54` | |
| **candidate** | **880,692** | **0.9937 (−0.63%)** | `3df42aad…cea2` | restored SHA-256 `6c7a2dec…6215` OK; ideal: model 4 886,174, LSTM 1,099,706, mixed 880,665 B |

On dev, the gain grows only slowly with length (−0.54% at 1 MiB, −0.63% at 4 MiB), and it stays far from the byte gate.
On the holdout the gain is 8× larger. Model 4's tuning (contexts, word hash, priors) was done on English and
structured data, and the LSTM fills that gap on Polish. Expect the gain to depend on the language and producer, not
to hold uniformly.

### Gate evaluation (candidate against stock Ultra)

| gate | dev1m | holdout | threshold |
|---|---:|---:|---|
| stored bytes | 0.994 **fail** (dickens4m: 0.9937 **fail**) | 0.9515 pass | ≤ 0.98 |
| encode throughput (CPU) | 0.056 **fail** | 0.029 **fail** (timed, §5a) | ≥ 0.67 |
| decode throughput (CPU) | 0.042 **fail** | 0.030 **fail** (timed, §5a) | ≥ 0.95 |
| peak RSS, encode | 0.77 pass | 0.91 pass (timed) | ≤ 1.10 |
| peak RSS, decode | 1.04 pass | 1.01 pass (timed) | ≤ 1.10 |
| combined latency (CPU) | 20.2 **fail** | 34.1 **fail** (timed) | ≤ 1.50 |

The decode-throughput gate is missed by a factor of about 23. The holdout encode took the same per-byte time as dev
(the LSTM's cost does not depend on content), so the holdout cannot change the speed verdict.

### Earlier, smaller arm (dev256k, H = 32, one timed `run.py` cell each, load ~43–46)

stock 65,152 B; control 65,104 B; candidate (H32 T10) 65,058 B; zpaq 66,095 B. Candidate encode/decode CPU was 2.43 s / 2.28 s,
against 0.48 s / 0.47 s for stock, so even a 4× smaller LSTM costs about 5× the CPU of stock.

## 6. Format implications

The candidate would need a new model identifier. `WIRE-CONTRACT.md` writes down the opt-in contract: model nibble 10,
frame version 13, opt-in only, and the deterministic binary32 reference behaviour with its own `kexp`. It also lists
what the contract leaves open (cross-ISA bit-exactness was not tested). Because the gate rejects the candidate, the
contract is a record and not a proposal for adoption.

## 7. Deviations and open items

- **Timed run: done (§5a).** The coordinator ran the 3-round timed run in the quiet window
  (`config-quiet.json`, 04:47–05:29Z on 26 Nov, rc 0). It used `notimed.sh` inside one `timed.sh` hold for the whole
  run, rather than one hold per cell. Cells and frames are in `$V/ultra-lstm/run1/`.
- The tiny1 cells (a 1-byte input) measure startup only; their MB/s figures carry no information.
- The candidate was tuned on dev only, in about 15 configurations (`dev-sweep.md`). Larger LSTMs gained more on dev, at
  2.5× the time per doubling of H.
- A leaderboard claim (LTCB) would need enwik8/enwik9 and a decoder-inclusive size. Neither was attempted.
- A comment on issue #57 at 19:15Z says task 2 is "on hold pending the owner's decision". This work started on the
  owner's instruction, relayed by the coordinating session. Nothing was posted on #57.

## Files

- `lstm_h.c`: the harness (it includes the production `cm.c`; build:
  `clang -O3 -mcpu=native -ffp-contract=off -Isrc benchmark/ultra-lstm/lstm_h.c build/libkolmoarch.a -o lstm_h -lpthread -lm`
  after `make build/libkolmoarch.a`).
- `run.py`, `config.json`, `config-quiet.json`, `notimed.sh`, `report.py`: the checkpointed, load-gated timed benchmark and its report.
- `timed-run1.md`, `timed-run1-summary.json`, `timed-run1-order.json`: the quiet-window run's report, medians and order.
- `dev-sweep.md`, `raw-sweep1m.txt`: development tuning. `raw-indicative.txt`: the raw `time -l` output and hashes behind §5.
- `WIRE-CONTRACT.md`: the opt-in model nibble 10 contract.
- `inputs-SHA256SUMS`, `frozen-SHA256SUMS`: input and binary hashes.

