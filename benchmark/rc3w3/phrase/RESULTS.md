# rc3 wave 3, phrase lane: one coded decision for a whole phrase

Branch `claude/rc3w3-phrase-20261118`, from `claude/release-rc3-20261116` (`58f7cee`).

**Decision: kept as a measured study, no format change.** No variant gains 0.5% on any group against `-x` (the
gate). The closest is JSON, at −0.38% (substitution) and −0.37% (adaptive phrase context), and both come almost
entirely from one file each. Container format, codec ids, frames and `benchmark/determinism/expected.sha256` are
unchanged. The library sources (`src/`, `bindings/*/native`) are not touched.

## The question, made precise

The owner asked for "a smart GR6/GR6+ model where a single bit can represent multiple items (letters, sentences,
words)". A bit cannot carry more than one bit of information. What the question can mean is this: one *coded
decision* stands for a whole phrase. A context-mixing coder spends `-log2 p` bits on a decision of probability `p`,
so a predictable phrase costs well under one bit, however many letters it has. Three ways to do that were built and
measured.

1. **Substitution (deterministic layer, then probabilistic layer).** A per-file phrase dictionary is built from the
   block itself. Each phrase is then replaced by a one- or two-byte code, and Kolmo's context-mixing coder (codec 7,
   model 3: the `-x` text model) codes the code bytes and the remaining letters. The dictionary travels in front of
   the stream and is coded with it.
2. **Phrase ids as CM contexts (no substitution).** The same dictionary is sent. CM gets two extra contexts: the
   longest dictionary phrase (or phrase prefix) that ends at the current position, combined with the partial word,
   and with order 2.
3. **Adaptive phrase contexts (no dictionary sent).** As 2, but the "dictionary" is learnt as coding goes: online
   counts of token n-grams, of up to 8 tokens. The context is the longest token suffix already seen at least T times.
   The decoder keeps the same counts, so nothing is transmitted.

Also measured: GR6+ with a **row decision** (one flag per row: "every field takes the op of the field above"). This
is the multi-item decision inside GR6+.

### How the dictionary is built (deterministic)

`phr.c`, `build_candidates` and `phrase_encode`:

- **Tokens.** A run of letters (and bytes ≥ 128) of up to 32 bytes is one token; every other byte is a token of its
  own.
- **Sample.** Up to 8 MiB, taken as 1 MiB chunks spread evenly over the block and cut at token boundaries.
- **Re-Pair-style pair merging on the sample.** Up to 64 rounds. Each round counts every adjacent symbol pair (radix
  sort of 64-bit keys) and merges the most frequent pairs: at most 512, at least a quarter as frequent as the round's
  best, at least 4 occurrences, at most 64 bytes long. Merging runs left to right without overlap. Phrases can span
  words, spaces, punctuation and newlines, so whole repeated lines and line templates qualify.
- **Selection.** The whole block is parsed greedily (longest phrase at each token start, a token trie). Phrases used
  fewer than `minuse` times are dropped and the block is parsed again (3 passes). The kept phrases are those with the
  largest byte saving, up to the code capacity.
- **Codes.** Byte values the block never uses (at most 64; if fewer than 2 are free, the 16 rarest plus an escape
  byte). Half of them are one-byte codes for the most used phrases; the other half prefix a second byte (256
  phrases each). The dictionary is written as length + bytes, in code order.

Every transform was inverted and compared with the input (`phrase_decode`). In the full-size runs, every codec-7
stream (plain, word transform, substitution) was also decoded and compared (`--rt`). The cmx study variants were
only encoded; they share codec 7's coding loop, and their extra contexts read decoded bytes only.

## Machine and method

Intel Xeon @ 2.10 GHz, 4 vCPUs, 15 GiB, Linux 6.18, gcc 13.3 (`make` defaults: `-O3 -march=native`). A shared VM.

- **Kolmo's modes** (`bench.py`): `kolmo -x` and `kolmo -m hc` (the word-transform route), one compress and one
  decompress per file, whole-process wall time, output compared with the input. Each table uses a single thread
  count, `-T 1` or `-T 4`, and each run was alone on the machine.
- **Study variants** (`phr`, built with `Makefile.study` against `src/kolmo.c`): the whole file as one stream into
  codec 7 model 3 (`cm_compress_m`), the coder `-x` uses on text blocks. Plain CM in this tool is `-x` minus 52 bytes
  of container on every prose, log and JSON file, so CM sizes compare directly with `-x`. The study tool is
  single-threaded, and its times were taken with four study processes running at once, so they are indicative only.
- **GR6+ alone:** `benchmark/rc3/gr6/g6tool.c`, the whole file as one block.
- **Corpora:** enwik8 (first 1 MB, first 10 MB, all 100 MB); Silesia dickens, webster, xml, reymont; Loghub (the 16
  `*_2k.log` files concatenated, `loghub_all.log`, 4.46 MB; Loghub `Apache.log`, 5.1 MB); OWID CO2
  (`owid-co2-data.csv`, current release, 15.2 MB); covid (`countries-aggregated.csv`, 5.5 MB); JSON: simdjson's
  `twitter.json` (Twitter API) and `citm_catalog.json`, and 20 MB of GitHub API events (gharchive, 2024-01-01 hour 0,
  cut at a line end). 1 MB slices from the middle of each (`results/s1m`) were used for the parameter search.

Files: `phr.c` (study tool), `mkcmx.py` (generates `cmx.c`, a renamed copy of `src/codecs/cm.c` with the phrase
contexts), `mkg6x.py` + `g6census.c` (GR6+ bit census and row decision), `bench.py`, `mktables.py`, `tab.py`,
`fulltab.py`, raw results in `results/`.

## Results

### 1. Substitution: the parameter search on 1 MB slices

Size against plain CM (codec 7, model 3) on the same slice. The dictionary is included. `minuse` is the number of
uses below which a phrase is dropped. The run with no merging is words only, like the existing word transform.

| file | CM bytes | word transform + CM | phrases, minuse 3 | minuse 50 | minuse 200 | minuse 200, length ≥ 8 | minuse 1000 | words only |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Apache.log | 16,054 | +5.01% | +216.10% | +53.53% | −0.83% | +0.04% | +0.51% | +5.28% |
| citm_catalog.json | 2,629 | +0.65% | +60.75% | −4.41% | −9.85% | −9.62% | −0.95% | +2.09% |
| dickens | 229,266 | +6.45% | +23.79% | +10.92% | +4.53% | +0.47% | +2.59% | +6.35% |
| enwik8_1m | 222,663 | +6.10% | +23.98% | +7.94% | +2.24% | +0.26% | +0.13% | +7.02% |
| gharchive.json | 67,066 | +11.71% | +43.22% | +14.47% | +0.48% | +0.39% | +0.13% | +12.59% |
| loghub_all.log | 44,741 | +15.00% | +111.21% | +31.08% | +0.59% | +0.22% | +0.20% | +16.57% |
| owid-co2-data.csv | 135,690 | +0.91% | +89.11% | +66.20% | +42.29% | +1.62% | +0.12% | +0.52% |
| reymont | 141,619 | +6.73% | +44.01% | +17.98% | +0.32% | +1.14% | +0.05% | +10.61% |
| webster | 171,196 | +12.76% | +34.12% | +11.33% | +7.01% | +2.12% | +1.77% | +12.19% |
| xml | 20,295 | +16.14% | +83.46% | +13.32% | +0.81% | +0.81% | +0.35% | +12.78% |

The more phrases are substituted, the larger the output. The pattern holds on every kind of data.

- A full phrase dictionary (minuse 3) is 24% to 216% larger.
- The existing word transform (codec-agnostic, built for LZ coders) is 1% to 16% larger in front of CM.
- Only very frequent long phrases pay: citm (a JSON catalogue of fixed keys) −10%, Apache −0.8%.

The reason: CM already codes a predictable phrase in a fraction of a bit per letter. A code byte removes letters
that cost almost nothing, and it breaks the byte contexts (orders 1-20, match model) and the word model around it.
Making CM's word model treat a code as one word token (`--subx`) halves the loss on prose (enwik8_1m +2.24% →
+1.04% at minuse 200) but does not turn it into a gain (`results/s1m/x_*.jsonl`).

### 2. All variants at full size, against `-x`

"Best phrase substitution" is the smallest of the dictionary sizes tried on each file (minuse 50, 200 or 1000 per
MB; prose only 200 per MB), with unmodified CM. "Two extra contexts" is the study CM with the two added models fed no
phrases: it isolates the cost or gain of the extra model capacity. The two phrase-context columns are measured
against it.

| group | file | bytes | `-x` | `-m hc` (word transform) | GR6+ alone | plain CM | best phrase substitution vs `-x` | two extra contexts, no phrases, vs CM | phrase contexts, sent dictionary, vs extra contexts | adaptive phrase context vs extra contexts |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prose | enwik8_1m | 1,000,000 | 222,715 | 300,460 |  | 222,663 | +0.02% | +0.02% |  | −0.39% |
| prose | enwik8_10m | 10,000,000 | 2,105,270 | 2,710,705 |  | 2,105,218 | +2.88% | −0.09% | −0.14% | −0.18% |
| prose | enwik8 | 100,000,000 | 19,012,531 | 24,390,220 |  | 19,012,479 | +4.32% |  |  | −0.02% (vs CM) |
| prose | dickens | 10,192,446 | 2,168,559 | 2,728,251 |  | 2,168,507 | +3.74% | −0.02% | −0.12% | −0.14% |
| prose | webster | 41,458,703 | 5,609,529 | 8,858,048 |  | 5,609,477 | +7.36% | +0.05% | −0.40% |  |
| prose | reymont | 6,627,202 | 907,685 | 1,348,510 |  | 907,633 | +0.90% | +0.10% | −0.12% | −0.24% |
| XML | xml | 5,345,280 | 300,720 | 443,624 |  | 300,668 | +0.28% | +0.03% | +0.06% | −0.25% |
| logs | loghub_all.log | 4,458,875 | 186,278 | 266,825 | 272,847 | 186,226 | +0.02% | +0.13% | +0.05% | −0.14% |
| logs | Apache.log | 5,135,876 | 72,771 | 125,598 | 99,500 | 72,719 | −0.32% | +0.21% | +0.21% | +0.06% |
| CSV | owid-co2-data.csv | 15,242,657 | 1,127,837 | 1,291,252 | 1,127,785 | 2,128,172 | +140.47% | +0.04% | −0.02% |  |
| CSV | countries-aggregated.csv | 5,512,931 | 283,038 | 327,637 | 282,986 | 351,922 | +76.04% | +0.10% | +0.02% |  |
| JSON | twitter.json | 631,515 | 25,522 | 37,755 |  | 25,470 | +0.10% | −4.04% | +0.51% | −0.07% |
| JSON | citm_catalog.json | 1,727,204 | 6,578 | 7,734 | 22,020 | 6,182 | −14.12% | +0.47% | +3.83% | −0.03% |
| JSON | gharchive.json | 19,998,312 | 846,939 | 1,271,326 |  | 846,887 | −0.28% | +0.04% | −0.03% | −0.38% |

The CSV files are coded by GR6+ in `-x` (frame version 11, decode 1.5 s and 6.0 s). The substitution column there
is against that. Substitution in front of CM is far behind on tables: it turns numbers into codes that neither CM nor
GR6+ can predict.

Per group, with each variant kept per file only where it is smaller (the blended rule the owner described):

| group | `-x` bytes | best of `-x` / substitution | change | adaptive phrase context (files measured) |
|---|---:|---:|---:|---:|
| prose | 30,026,289 | 30,026,289 | 0.00% | −0.06% |
| XML | 300,720 | 300,720 | 0.00% | −0.25% |
| logs | 259,049 | 258,817 | −0.09% | −0.09% |
| CSV | 1,410,875 | 1,410,875 | 0.00% | — |
| JSON | 879,039 | 875,716 | −0.38% | −0.37% |

- **Substitution never gains on prose, XML or tables**, at any dictionary size. Gains grow with repetition and shrink
  with size: citm −14% of 6.6 KB (929 bytes), gharchive −0.28%, Apache −0.32%. That is −0.38% for JSON and −0.09%
  for logs, below the 0.5% gate.
- **Phrase ids as contexts, dictionary sent:** the coded body shrinks 0.3–0.6% on prose at 1 MB. Sending the
  dictionary costs more than that at 1 MB (+0.2% to +12%). At 10–41 MB, with the dictionary amortised, the net is
  −0.12% to −0.40% on prose. It is never better on logs, tables or JSON.
- **Adaptive phrase contexts:** −0.14% to −0.39% on prose at 1–10 MB, but only −0.02% on the full 100 MB of enwik8.
  The CM tables already hold most n-gram statistics, and CM's word-trigram and order-12/20 contexts already cover
  most of what the token-suffix context adds. JSON −0.37% comes from gharchive alone.
- **Combining both mechanisms on JSON** (substitution on citm, adaptive context on gharchive) gives −0.43%. Still
  below the gate, and it would need two new format pieces for two files.

### 3. GR6+: where the bits go, and one decision per row

Bit census of GR6+'s coder per decision kind (`g6census`, encoder-side `-log2 p`):

| file | GR6+ bytes | numbers (residuals) | ops (one per field) | characters | ranks |
|---|---:|---:|---:|---:|---:|
| countries-aggregated.csv | 282,986 | 91.1% | 7.4% (0.21 bit per op) | 0.4% | 0.0% |
| owid-co2-data.csv (1 MB slice) | 78,588 | 92.0% | 7.2% | 0.2% | 0.1% |
| loghub_all.log (1 MB slice, GR6+ not chosen) | 70,686 | 3.5% | 7.7% | 59.4% | 20.2% |

On the tables where GR6+ is chosen, an op already costs about 0.2 bits. A field that the op predicts entirely (DITTO,
EMPTY) is already "many letters in a fraction of a bit".

The **row decision** (`ROW=1 g6census`) codes one flag per row meaning "every op equals the op above", and then
skips the op decisions. It is exactly decodable: ops depend only on state the decoder has, checked by a dry run, with
one mismatch in 56,269 OWID rows to fix before it could ship.

| file | GR6+ | GR6+ + row decision | ops bytes → | row flag bytes | change |
|---|---:|---:|---|---:|---:|
| countries-aggregated.csv | 282,986 | 283,814 | 20,801 → 10,583 | 11,157 | +0.29% |
| owid-co2-data.csv | 1,127,785 | 1,127,007 | 73,530 → 70,202 | 2,592 | −0.07% |

The flag costs what it saves. The per-field model already conditions each op on the op to its left, the op above,
and the column's op history, so a joint decision has nothing left to capture.

### 4. Speed

Kolmo at one thread (wall seconds, compress / decompress, each run alone on the machine):

| file | `-x` bytes | `-x` s | `-m hc` bytes | `-m hc` s |
|---|---:|---:|---:|---:|
| enwik8_1m | 222,715 | 11.0 / 4.6 | 300,460 | 0.29 / 0.01 |
| enwik8_10m | 2,105,270 | 58.6 / 50.2 | 2,710,705 | 4.56 / 0.07 |
| enwik8 | 19,012,531 | 556.1 / 549.3 | 24,390,220 | 87.07 / 0.52 |
| dickens | 2,168,559 | 61.4 / 55.6 | 2,728,251 | 5.45 / 0.06 |
| webster | 5,609,529 | 228.5 / 229.2 | 8,858,048 | 32.32 / 0.10 |
| reymont | 907,685 | 34.1 / 31.7 | 1,348,510 | 5.20 / 0.02 |
| xml | 300,720 | 29.8 / 22.0 | 443,624 | 2.21 / 0.02 |
| loghub_all.log | 186,278 | 23.9 / 18.0 | 266,825 | 0.37 / 0.01 |
| Apache.log | 72,771 | 23.0 / 18.0 | 125,598 | 0.27 / 0.01 |
| owid-co2-data.csv | 1,127,837 | 49.3 / 6.0 | 1,291,252 | 1.02 / 0.06 |
| countries-aggregated.csv | 283,038 | 23.3 / 1.5 | 327,637 | 0.24 / 0.03 |
| twitter.json | 25,522 | 1.8 / 1.6 | 37,755 | 0.12 / 0.00 |
| citm_catalog.json | 6,578 | 2.7 / 0.3 | 7,734 | 0.04 / 0.00 |
| gharchive.json | 846,939 | 90.5 / 82.4 | 1,271,326 | 6.08 / 0.04 |

T4_TABLE

The study variants are single-threaded at every thread count (CM is sequential within a stream), so their 1-thread
and 4-thread figures are the same. From the `--rt` runs (indicative: four study processes shared the machine):

- **Substitution.** The dictionary takes 0.05–10 s to build (pair rounds on the ≤ 8 MiB sample, then 3 parses).
  Undoing the substitution costs 1–300 ms. CM then codes fewer bytes, so encode and decode are faster where many
  phrases are coded: dickens CM decode 79 s → 27 s, citm 5.0 s → 2.0 s. Where few phrases pass the threshold, CM
  speed is unchanged (Apache 23 → 26 s). Decode was never slower than 1.5×; the variant was rejected on size alone.
- **Phrase contexts (two extra models).** Both directions are 5–21% slower than codec 7 (e.g. enwik8 100 MB: 585 s →
  623 s encode). Decode slows by the same factor, since CM is symmetric.

## Side finding: CM's word model folds UTF-8 bytes

The "two extra contexts" run gained 4.0% on twitter.json without any phrase. The cause is in codec 7's word hash:
`m->word = (m->word + (c | 32) + 1) * ...` applies the ASCII case fold (`| 32`) to bytes ≥ 128 as well. In UTF-8 that
merges distinct lead and continuation bytes (0x80–0x9F with 0xA0–0xBF, 0xC0–0xDF with 0xE0–0xFF), so the word
contexts mix up characters of non-Latin scripts. Folding only ASCII letters (`c < 128 ? c | 32 : c`), with no other
change (`UTF8FIX=1 python3 mkcmx.py`):

| file | codec 7 | ASCII-only fold | change |
|---|---:|---:|---:|
| twitter.json (Japanese tweets) | 25,470 | 24,378 | −4.29% |
| enwik8_1m, reymont (1 MB), gharchive (1 MB) | — | identical | 0.00% |

This is a codec-7 model change: it changes every `-x` frame with such bytes and needs a model version in the CM
header. It is outside this lane's scope and was not made. It is recorded in `research/CODECS.md` as a candidate for
the CM owner, to be measured on a multilingual corpus.

## Why the gate is not met, in one paragraph

A phrase dictionary makes a phrase cheap to *name*. A strong context-mixing model already makes a predictable phrase
cheap to *code*: once "of the" is likely, its letters cost a few hundredths of a bit each. Substitution therefore
saves almost nothing on the phrases it replaces, while it costs the dictionary and damages the byte and word contexts
around every code. As a context instead of a substitution, the phrase id overlaps with what CM's word-trigram,
order-12/20 and match models already see, so it adds 0.1–0.4%, and less as the input grows. Gains appear only where
long phrases repeat with little variation: a catalogue of fixed JSON keys (−14% on 6.6 KB). That is too narrow to
justify a codec id and a format piece. GR6+ already codes whole fields as single decisions, and a whole-row decision
measured −0.07% to +0.29%.

## Reproduce

```
make && make -f benchmark/rc3w3/phrase/Makefile.study build/phr build/g6census
./build/phr --rt --upmb=200 --ctx FILE              # CM, word transform + CM, phrase contexts, substitution
./build/phr --nobase --noword --nosub --adapt --pxvar=2 --pxT=8 FILE   # adaptive phrase context
./build/phr --nobase --noword --subx --upmb=1000 FILE                  # substitution, code-aware CM
ROW=1 ./build/g6census FILE.csv                     # GR6+ bit census with the row decision
python3 -I benchmark/rc3w3/phrase/bench.py out.jsonl 1 "-x,-m hc" FILE...
python3 -I benchmark/rc3w3/phrase/mktables.py
```
