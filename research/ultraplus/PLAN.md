# Ultra+ plan: what is missing against paq8px and cmix, and what each ingredient is worth

Goal (owner, 26 Nov 2026): an opt-in cold-storage mode that is smaller than cmix v21 and paq8px -12L on enwik8 and
Silesia, with the decoder's own bytes counted, and clearly ahead of Kolmo ultra on every data type.
Ultra+ is a new mode. Under the acceptance standard (the owner's per-condition effect map, item 5) it is judged on ratio against the
leaders, and its speed and memory are stated but not gated.

## Where Kolmo ultra stands (measured 24 Nov, `$V/evidence/ARCHIVERS_2026-11-24.md`)

| target | leader (published, decoder included) | Kolmo -U | gap to close |
|---|---:|---:|---:|
| enwik8 | cmix v21: 14,623,723 + 281,387 decoder = **14,905,110** | 18,651,472 | −20.1% |
| enwik8 | nncp v3.2: 14,915,298 + 628,955 = 15,544,253 | | −16.7% |
| enwik8 | paq8px_v206fix1 -12L: 15,849,084 + 402,949 = 16,252,033 | | −12.9% |
| Silesia (12 files) | paq8px_v217 -12L: 27,682,006 (+ decoder) | 36.33 MB | −23.8% |
| Silesia | precomp + cmix v21: 28,261,094 (+ decoder) | | −22.2% |

LTCB's decoder column is the zipped size of the decompressor plus any dictionary. Kolmo's decoder would be counted the
same way: the compressed `kolmo` binary: 803,458 B as zip -9 and 592,116 B as xz -9e at this commit (the full CLI with every coder). A decode-only build would be smaller.

Gaps against paq8px -8 per type (measured, same files): text 9-19%, JSON 25%, logs 16%, x86 code 9-34%, 16-bit image
1%. Kolmo is ahead on typed CSV (41% smaller than paq8px) and on audio. A 256 KiB ESC-50 cut gives Kolmo -U 13,940 B
and cmix 14,478 B.

## Ingredients, by measured or evidenced gain

"Measured" figures come from this lane's runs (RESULTS.md). The 200 kB dickens slice is the 24-Nov probe's slice, where
cmix gives 47,064 B, paq8px -8 46,745 B and Kolmo -U 51,151 B.

| # | ingredient | status | gain against stock ultra | evidence |
|---|---|---|---|---|
| 1 | Hashed context bank (38 contexts, counters, history and count state maps, run models), 2 match models, 9-set mixer plus final mixer, 5 APMs, 16-bit coder, model 4 as an input source | **step 1, built** | d200k −3.3%. Dev cuts: see RESULTS.md (text, code, JSON, logs −3 to −6%; containers up to −28%) | this lane |
| 2 | Byte LSTM (#116), as one more input and a mixer selector | in the engine (flags H) and off by default | d200k −0.5% on top of step 1. #116: −4.85% on reymont 4 MB, −0.6% on dickens 4 MB. It grows with file size, and cmix and nncp get most of their enwik8 lead from the neural part | #116, this lane |
| 3 | Lexer token-class contexts and per-type priors | next | impact matrix (model 4): −1 to −3.7% for the lexer and −0.5 to −2% for priors on files under 1 MB. They partly overlap the bank's word and shape contexts | `benchmark/impact/RESULTS.md` |
| 4 | SSE/APM chain like paq8px's (order-2/3 hashed, match, word; chained and mixed) | next | the 5 APMs give 0.35% now. Estimate −0.5 to −1% | this lane |
| 5 | Encoder-side parameter search (the stream already carries 13 parameters) | next | single-parameter sweeps on d200k moved −0.1 to −0.4%. Estimate −0.5 to −1.5% per file | this lane |
| 6 | Two-pass text dictionary (words to codes, the dictionary built from the file and stored in the stream) | planned | literature: −3 to −5% for paq-class coders on enwik8 | cmix and paq8 dictionary preprocessing |
| 7 | x86/ARM model (opcode, ModRM and target contexts) on the BCJ-filtered stream | planned | the ooffice gap to paq8px -9 is 34% | G23 |
| 8 | UPX inside codec 25 parts (table columns) and the image path (CMI/kimg) | planned | where ultra already beats paq8px (CSV, audio, images) | blocks.c routing |
| 9 | Larger LSTM / LSTM ensemble, mixer of several LSTM sizes | planned, slow | needed to approach cmix on enwik8; cost ~0.005 MB/s | cmix design |

## Target arithmetic for enwik8

Step 1 gives about −3%. With the LSTM grown over 100 MB (−4 to −6%), a dictionary (−3 to −5%), richer text contexts
and priors (−1 to −2%), the APM chain (−0.5 to −1%) and parameter search (−1%), the total is about −13 to −18%
against ultra. That passes paq8px_v206 -12L (−12.9% needed). It is not yet enough for nncp (−16.7%) or cmix (−20.1%).
Passing cmix needs a cmix-scale model: hundreds of models and an LSTM mixture. That is weeks of work, and the
estimates here will be replaced by measurements as each step lands.

## Method (per step)

- **Dev split:** the 22 impact-corpus dev files cut to 256 KiB (`$V/ultra-plus/cuts/dev`), plus the d200k/d1m dickens
  slices. **Sealed holdout:** the 31 holdout cuts, run once per step after the dev decision.
- **Arms:** stock `-U`, `-U --codec-level 2`, `--archive-max`. Also cmix v21 on the cuts (local), and paq8px -8 (Codex's
  runs, requested on #57).
- **Recorded per cell:** bytes, encode/decode wall time and peak RSS, and a SHA round trip. Timed runs go through
  `timed.sh` at load < 12. Background runs are niced, at most 2 processes.

## Folded in from the Codex re-assessment (fleet4/CODEX_REASSESS.md §4, 26 Nov)

| item | status in #122 |
|---|---|
| 1. #88's sampled pre-trial gate | done (`upx_gate`: two 16 KiB windows). Its miss rate is being measured on the dev cuts against `KOLMO_UPX_NOGATE=1` |
| 2. memory: admission before allocation, governor | done: decode refuses a stream over the declared working set; block jobs reserve `upx_need`. Compact tables (#5) to cut RSS: open |
| 3. checked allocations, declining to plain CM | done |
| 4. per-block stored-raw fallback | already in place: a block whose coded size reaches its raw size is stored (`blocks.c`), so v13 frames keep `kolmo_bound` |
| 5. #63 NEON | open: measure once #63 is on main |
| 6. explicit `--x-tables` / `-M` cap frames | open: rerun on #93 + #100/#104 |
| 7. block layout (16 MiB target, no small tails) | open: a stacked ingredient |
| 8. leader points | in RESULTS.md (paq8px -8L enwik8[0:256K] 54,648 B; full enwik8 −13.8% vs ultra) |
| 9. stacking: #110 FASTQ cuts keep codec 16; #102 never under archive-max | #102 is not on main; to be guarded when it lands |
