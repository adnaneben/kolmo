# rc3 GR6+ lane: GR6+ blocks in ultra (codec 26, container version 11)

Branch `claude/rc3-gr6-20261117`, from `claude/release-rc3-20261116` (`7082475`). The codec comes from branch
`claude/dazzling-maxwell-p8y4u0` (commit `12f0c0e`, "ultra: GR6+ blocks"), where it was codec 15 in container version 9
(`src/gr6.inc`, written against the single-file `src/kolmo.c` of old `main`). Both numbers are taken in rc3 (15 is the
native plane leaf, 9 the native leaves, 10 FASTQ), so here it is codec 26, written in container version 11.

Machine: Apple M5 (10 cores, 24 GB), macOS 27.2, Apple clang 21.0.0, `make` defaults (`-O3 -mcpu=native
-ffp-contract=off`). The Mac was shared with other work (load average about 1.8 before the runs): every run is one
thread (`-T 1`), base and GR6+ builds alternate per file, the order flips every round, three rounds, minimum time.

## What was ported

| piece | rc3 |
|---|---|
| codec | `src/codecs/gr6.c`, table `codec_gr6` (`C_GR6 = 26`) in the registry (`codecs/codec.h`, `codecs/codec.c`). The table has an encoder and no decoder entry: codec 26 is decoded at block level only, so a graph leaf cannot name it |
| encoder hook | `src/core/blocks.c`, the ultra branch (`codec == C_CM && level != 1`), after CM, LZMA2/BWT and the plain-block retries of transformed blocks: GR6+ is run on the plain block and kept when smaller than `cl + meta`; the block then has plan 0, tflag 0 and sets `newmodes` bit 128, so the frame is version 11 (the same bit as the table-slack feature; `core/compress.c` maps 128 to 11) |
| decoder | `src/core/decode.c`: codec 26 needs version ≥ 11, plan `0,0,0,0` and tflag 0; anything else is `KOLMO_ERR_CORRUPT`. `gr6_decompress` keeps every bounds check of the original (field count, prefix length, characters, dictionary and rank indices, unit ranges, output length) and unwinds through `longjmp` on corrupt input |
| format | `SPEC.md`: header version list, block codec list, *Identifiers and versions* (version 11 row, codec 26 row, 18-25 marked reserved), *GR6+ blocks (codec 26, version 11)* |
| tests | `tests/kolmotest.c`: a derived-column time-series table must be coded by GR6+ in ultra (version 11, codec 26 in the index) and be no larger than GR6+ alone plus the container; `-m rt/fast/eco/hc/max` never write codec 26; the same frame relabelled version 10 (header CRC recomputed) is refused; every 1/64 truncation and 1000 corrupt streams (table and synthetic CSV) decode without crashing. Run also under ASan/UBSan (`kolmotest.c` + `src/kolmo.c` with `-fsanitize=address,undefined`): passed |
| bindings | `bindings/python/native/src` and `bindings/rust/native/src` carry the changed files (`diff -rq` against `src/` shows only the research tools they omit) |

The integer-only arithmetic is unchanged (`__int128` for the formula ratios, integer mixers and APM, the CM
arithmetic coder and CM's logistic tables). The coded bytes are those of `12f0c0e`: `benchmark/rc3/gr6/g6tool.c`
linked against the original commit's library and against this branch gives the same sizes on OWID CO2 (1,111,604)
and covid (282,986), and the same size and FNV-1a hash on energy (755,883). One payload detail differs: the 8-byte line
count is now written and read little-endian explicitly (it was `memcpy` of a host integer; identical on every
little-endian host).

Two encoder-side changes, neither changes a GR6+ stream:

1. **Text gate.** GR6+ is tried only on blocks that `looks_text` accepts (≥ 95% printable in four 16 KiB slices) and
   that have at least 64 lines. The original tried every block with 64 newlines, binary ones included.
2. **Exact pruning in formula discovery.** The scoring pass over up to 8000 rows stops a candidate formula, every 64
   rows, once even all remaining rows hitting could not make it the kept one (best reachable rate
   `(hit + rem) / (seen + rem)` not above the best so far, or not above one half). Same formulas, same bytes. On
   `energy_test.csv` (130 columns) the rows scored fell from 1.28 G to 0.72 G and GR6+ alone from about 11-14 s to
   about 8.7 s.

## Results: `kolmo -x -T 1`, rc3 base against rc3 + GR6+

| file | size | rc3 `-x` bytes | GR6+ `-x` bytes | rc3 ratio | GR6+ ratio | size change | version | compress s (rc3 / GR6+) | decompress s (rc3 / GR6+) |
|---|---:|---:|---:|---:|---:|---:|---|---:|---:|
| owid-co2-data.csv | 14377942 | 1119833 | 1111656 | 12.84 | **12.93** | -0.73% | 5 -> 11 | 14.69 / 19.07 | 10.77 / **2.10** |
| owid_test.csv | 10377936 | 848663 | 817272 | 12.23 | **12.70** | -3.70% | 5 -> 11 | 11.03 / 13.38 | 7.60 / **1.43** |
| energy_test.csv | 6922645 | 635302 | 635302 | 10.90 | 10.90 | 0 | 5 -> 5 | 7.24 / 16.36 | 5.06 / 5.01 |
| wide_test.csv | 8227871 | 925251 | 925251 | 8.89 | 8.89 | 0 | 7 -> 7 | 0.91 / 0.90 | 0.05 / 0.04 |
| loghub_all.log | 4458875 | 184718 | 184718 | 24.14 | 24.14 | 0 | 3 -> 3 | 7.86 / 8.13 | 6.83 / 6.89 |
| dickens | 10192446 | 2062661 | 2062661 | 4.94 | 4.94 | 0 | 3 -> 3 | 18.49 / 20.50 | 17.67 / 17.71 |
| api.json | 3737294 | 132215 | 132215 | 28.27 | 28.27 | 0 | 3 -> 3 | 5.32 / 5.64 | 5.59 / 5.59 |
| countries-aggregated.csv (covid) | 5512931 | 292165 | 283038 | 18.87 | **19.48** | -3.12% | 11 -> 11 | 8.00 / 8.48 | 5.74 / **0.54** |

Every output decoded to the input (checked after every run). No file is larger: GR6+ is kept only when smaller.

- **Gains on top of rc3:** covid 18.87 → 19.48 (−3.1%), owid_test 12.23 → 12.70 (−3.7%), OWID CO2 12.84 → 12.93
  (−0.7%). Where GR6+ is chosen, decompression is 5-11× faster (covid 5.74 s → 0.54 s, owid_test 7.60 → 1.43,
  OWID CO2 10.77 → 2.10), because a GR6+ block decodes at 4-10 MB/s against about 1 MB/s for CM.
- **Smaller than the gains measured on `main`.** `12f0c0e` reported OWID CO2 12.57 → 13.52 and covid 15.41 → 19.48 on
  old `main`. Covid ends at the same 19.48 (GR6+ alone is 282,986 bytes on both), but rc3's table-transform fix had
  already taken `-x` to 18.87, so 3.1% is left. The OWID CO2 file here (14.4 MB) is a different release from the
  study's (15.2 MB): GR6+ alone gives 12.93 on it, and rc3 `-x` is already 12.84.
- **Compression cost.** GR6+ is tried on every text block in ultra. It costs 0.3-2 s on logs, text and JSON (+3% to
  +11%), 0.5 s on covid, 2.4-4.4 s on the OWID files (+21% to +30%), and **9.1 s on `energy_test.csv` (+126%), where it
  is not chosen**: formula discovery over a 130-column table (7.2 M candidate formulas screened, 0.79 M scored over up
  to 8000 rows) takes about 7.5 s of its 8.7 s. See *Open items*.
- `wide_test.csv` is a graph block (version 7) and never reaches the ultra candidates; unchanged.
- The frame version rises from 5 to 11 on the OWID files (they now contain a codec 26 block), as for covid.

GR6+ alone on each file (`g6tool`, whole file as one block; for reference): OWID CO2 1,111,604 (12.93), owid_test
817,220 (12.70), energy 755,883 (9.16, 19% larger than `-x`), wide 1,082,260 (7.60), loghub 272,847 (16.34), dickens
2,958,996 (3.44), api.json 344,192 (10.86), covid 282,986 (19.48).

## Other modes are byte-identical

`frames.py` compressed all eight files with `-m speed`, `-m balanced`, `-m eco`, `-m hc`, `-m ratio` and `-x -l 1`
(one thread) with both builds and compared SHA-256: **48 of 48 frames identical** (`frames.txt`). The hook sits inside
the ultra branch with `level != 1`, so no other path can reach it.

## Tests

| target | result |
|---|---|
| `make check` | passed (includes the GR6+ tests) |
| `make check-rc` | passed |
| `make check-sanitizers` | passed |
| `make experimental-check` | passed |
| `make kfam-check` | passed |
| `kolmotest` under ASan + UBSan | passed |
| `python3 benchmark/determinism/check.py build/kolmo build/determinism` | 110 frames checked, OK, unchanged: GR6+ is not chosen on its inputs (`text.txt`, `table.csv`), so `expected.sha256` was not regenerated |

## Commands

```
make -j4 build/kolmo                                       # base at 7082475, copied to kolmo-base; GR6+ build to kolmo-new
python3 -I benchmark/rc3/gr6/bench.py 3 results-x.json -x -- <8 files>    # interleaved, -T 1, min of 3
python3 -I benchmark/rc3/gr6/frames.py                     # other modes, frame hashes
cc -O2 benchmark/rc3/gr6/g6tool.c build/libkolmo.a build/libvendor.a ... # GR6+ alone (size, hash, times)
```

The scripts hold the Mac's paths (scratch binaries, `~/Downloads/kolmo-bench/data`, the covid download); raw
numbers in `results-x.json`.

## Open items

- **Compression time on wide tables.** Formula discovery is O(columns² × 5 scales × 2 types) per target column. A
  cheaper search (fewer scales for two-column types, a candidate budget, or a pre-filter on value magnitudes) would
  change which formulas are found, so it needs its own size measurement; not done here.
- `balanced`/`max` do not get the new formula types (running totals, copies, products, near-hit scoring); the table
  transform would need them (`research/CODECS.md`, "more derived-column formulas").
- Memory: a GR6+ trial allocates 8 tables of 2^22 4-byte entries (128 MiB) for blocks of 1 MiB or more, on top of CM's
  model, per worker thread.
