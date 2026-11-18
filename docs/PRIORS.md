# Per-kind priors and token classes for context mixing

Status: rc3 wave 3, prior lane, 18 November 2026. Measurements and commands: `benchmark/rc3w3/prior/RESULTS.md`.
Builds on the twopass lane (`docs/TWOPASS.md`): one shared prior per model and a race of four learning presets.

The owner's questions: (a) are prebuilt networks for specific kinds of files (XML, JSON, ...) worth shipping, blended
with regular expressions if that adds value; (b) where do regular expressions add value?

## Answer in one table

| technique | what it costs | what it gains (`-x`, against rc3 + twopass) | decode | verdict |
|---|---|---|---|---|
| per-kind prior: the hash-free tables of a model-3 coder trained on public data of one kind (all mixer rows, w4's mean row, state maps, order-1 table, match counters), 4-bit weights, LZMA2 | 38–58 KB of binary per kind (388 KB for 8) | with model 4, against the twopass lane: 10 KB −5.8% to −14.7%, 100 KB −1.4% to −10.5% (largest on logs, HTML, code); at 1 MB the prior adds 0–1.7 points to model 4; not used from 4 MiB | same work per bit; one LZMA2 unpack per process | **in** for 8 kinds; SQL dropped (its dumps take the table route) |
| model 4: model 3 + two token-shape contexts from a hand-written lexer (regex classes as CM context) | 2 more hashed models | 1 MB −1.7% to −4.2% and 10 MB −1.3% to −3.8% on code, JSON, HTML; prose −0.3% to −0.8%; tables 0 | 1.03–1.12× decode time (1 MB, 1 thread), only where it wins the race | **in** (raced against model 3 per stream) |
| numbers routed to a typed stream (per-slot deltas) | a split of the text | **+7% to +154%** (worse) on logs, JSON, CSV | — | not adopted: CM predicts numbers better in context; the log and table transforms keep what structure helps |
| a priming text per kind (the coder learns a sample before the file) | 16–64 KB per kind and its coding time at every decode | similar to the table prior plus 1–2 points at 10 KB | 3.5× slower decode at 10 KB | not adopted (decode rule) |
| full tables of the large mixers and APMs (w4, a2, a3: 4–16 MB each) | MBs per kind | +1–3 points at 10 KB over the hash-free tables | — | not adopted (size) |
| the kind detector (`src/algo/texttype.c`) | a scan of 64 KiB | chooses the prior | — | **in** |
| UTF-8 text that `looks_text` rejects (CLDR XML, tweets) sent to the text models by the detector | — | −0.5% to −10% (CLDR 10 MB −9.8%) | up to 1.56× slower (model 3/4 instead of model 2) | not adopted (decode rule) |

## How it works

1. **Kind.** `text_kind()` reads the first 64 KiB and recognises markup, JSON/NDJSON, delimited tables, logs, C,
   Python, JavaScript, prose or SQL dumps with hand-coded scanners: tags, `"key":` pairs, dates and times (`hh:mm:ss`,
   `yyyy-mm-dd`, `dd/Mon/yyyy`, month names, HDFS `yymmdd hhmmss`, epoch digit runs), IPv4 at the line start, keyword
   and line-shape scores. No regex library; the result depends on the bytes only.
2. **Prior.** For a recognised kind, every contestant of the `-x` race starts from that kind's prior (tune byte 2 =
   2 + kind − 1); otherwise from the shared prior (1). A prior holds the tables of a model-3 coder that do not depend on
   context hashes, after coding 1–4 MB of public data of that kind (`benchmark/rc3w3/prior/RESULTS.md` lists the
   sources; no test file is used). Model 4 uses the same prior with its two extra models at their start values (a
   model-3 prior mapped this way was better than one trained for model 4).
3. **Race.** Stage 1 codes the first 256 KiB with model 3 and with model 4, first preset; stage 2 codes it with the
   three other presets of the winning model. The smallest of the five codes the stream. Threads only run trials side
   by side; the stream stores the model and prior, so the decoder does the same work as for any model-3 stream (plus
   two contexts with model 4).
4. **Format.** Container format 2 (version 11), codec 7. The prior id is the low 7 bits of tune byte 2 (it was 0 or 1),
   bit 7 selects model 4 (`SPEC.md`, CM models). No new codec id: the prior is a property of the CM stream.

## Regular expressions: where they help and where they do not

- **As context (option ii): yes.** Token classes (digit run, letter run, spaces, each punctuation byte) are what a
  regular expression for timestamps, IPs, UUIDs, hex ids and numbers is built from. Given to the mixer as the classes
  of the last eight tokens and as the same token of the previous line, they gain where fields shift position from line
  to line (code, JSON, HTML, access logs), and nothing on prose or tables, where words and columns already are the
  structure. Four variants (the classes of the whole line so far; the hash of the same token of the line before; the
  class with the current token's bytes) were within ±1% of the chosen pair at 100 KB; separate recognisers for each
  class (an IPv4 or UUID state machine as its own context) were not tried, since the class sequence already separates
  them.
- **As routing (option i): no.** Numbers taken out of logs, JSON and CSV into a typed stream (decimal or per-slot
  zig-zag deltas) made `-x` 7–154% larger: the coder loses the context that predicts them (the key, the template, the
  previous field). Where a typed representation helps, Kolmo already has it: the log transform's templates and typed
  fields, the table transform's columns and predictors, chosen by trials.
- **For detection: yes, as a chooser.** The planner keeps its own trials; the kind detector chooses priors. Letting it
  also send UTF-8 text in other scripts to the text models gained 4–10% on CLDR XML but decoded 1.56× slower; not
  adopted.
