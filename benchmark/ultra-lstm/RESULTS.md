# Ultra + LSTM mixer input (claim 68): a reproducible negative result

Handoff task 2 (`benchmark/waves/2026-11-25/overnight/CLAUDE-CODE-HANDOFF.md` on `codex/ppp-audit-and-results-20261125`)
asks whether the Claude-owned Ultra model is worth implementing.

**Answer: no, not in this form.** @@VERDICT@@

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

@@TABLES@@
