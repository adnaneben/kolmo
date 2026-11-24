# Kolmo in lzbench

[lzbench](https://github.com/inikep/lzbench) is the standard in-memory benchmark for lossless compressors. The
files here add Kolmo to lzbench as an ordinary codec, in the layout lzbench's `CONTRIBUTING.md` asks for: Kolmo's
sources unchanged from a release tag, compiled by lzbench's own Makefile and called through the public C API
(`kolmo_compress`, `kolmo_decompress`, `kolmo_params_for_level`). The upstream pull request (see
[`UPSTREAM-PR.md`](UPSTREAM-PR.md)) is what `install.py` does to a checkout, copied.

```sh
git clone https://github.com/inikep/lzbench
python3 -I bench/lzbench/install.py lzbench            # Kolmo at the tag of VERSION; --ref <tag> for another
make -C lzbench -j BUILD_ONLY="KOLMO ZSTD XZ ZPAQ"     # or without BUILD_ONLY for every codec
./lzbench/lzbench -ekolmo,1,3,6,9,10/zstd,3,12,19/xz,9 silesia.tar
```

`install.py` needs a git checkout of Kolmo (it exports `src/` at the tag with `git archive`) and lzbench 2.4.1 or
later (the `mk/<codec>.mk` layout). Kolmo is tested here against lzbench commit `9be0e25` (2.4.1, 2026-11-11).

## Files

| File | Becomes, in lzbench | Role |
|---|---|---|
| `kolmo.mk` | `mk/kolmo.mk` | Objects and flags. Disables Kolmo on 32-bit, big-endian and Windows targets |
| `kolmo_codec.cpp` | a block in `bench/misc_codecs.cpp` | The wrapper: two functions, about 20 lines |
| `install.py` | — | Copies `src/` to `misc/kolmo/src`, adds the above, registers the codec in `bench/codecs.h`, `bench/lzbench.h`, `README.md` and `CHANGELOG` |
| `corpora.py` | — | Downloads Silesia, enwik8 and Canterbury from their canonical sites and checks sha256; rebuilds the exact `silesia.tar` of the Mac run |
| `summarize.py` | — | Median and spread over repeated runs, sizes checked across runs (and against another machine with `--mac`), the market report's claims per corpus |
| `cli_bench.py` | — | The command-line comparison (kolmo, zstd, xz, bzip3; one thread and all cores) |
| `UPSTREAM-PR.md` | — | Draft text of the lzbench pull request. **Not sent**: it waits for the owner's go |

`.github/workflows/bench-x86.yml` runs all of it on a GitHub x86-64 runner (manual dispatch).

## Levels

lzbench level *n* is `kolmo -n` for 1 to 9, and level 10 is `kolmo -U` (ultra). The wrapper calls
`kolmo_params_for_level`, the table the command line and the bindings use, so lzbench's compressed sizes equal what
`kolmo -T 1 -<n>` writes, byte for byte.

| lzbench | kolmo | engine (0.0.2) |
|---|---|---|
| 1 | `-1` fastest | KLZ (LZ77) |
| 2–7 | `-2` … `-7` (3 = default) | KLZH2 (LZ77 + Huffman/ANS), stronger parses at higher levels |
| 8–9 | `-8`, `-9` max | block sorting (BWT) or KLZMA per block |
| 10 | `-U` ultra | context mixing; about 1 GB of memory or more |

## Fairness

- **One thread.** Kolmo runs with `threads = 1` (`NO_THREADING` in lzbench's table), like the other entries.
- **Defaults.** Default block size and transforms, exactly what `kolmo -T 1 -<n>` does. Nothing is tuned to the
  benchmark files.
- **Same compiler options** as every other codec in lzbench (`-O3`, `MOREFLAGS`), except two that Kolmo turns off for
  its own objects: `-ffast-math` (in lzbench's `OPT_FLAGS`) and floating-point contraction. Kolmo's encoders choose
  some routes by floating-point cost, and its frames are specified to be identical on every machine; with those
  options the choices, and so the output, could differ from the command line's.
- **No shared code.** Kolmo has no vendored codec library. Its libsais subset (`src/algo/sais`) exports only
  `kolmo_sais_*`, so it does not collide with the libsais in lzbench's bsc. A few internal helpers are still global
  without the `kolmo_` prefix (`kx_*`, `tbl_*`, `*_pub`); none collides with lzbench 2.4.1.
- **Verified.** lzbench checks every decompressed buffer against the input.

## Measured

- Apple M5 (arm64): `MARKET_POSITION_0.0.2.md` in the review workspace (not in this repository).
- x86-64 (GitHub runner): the `bench-x86` workflow's summary and artifacts.

The older `benchmark/lzbench/` directory is the 1.1.0-era integration (levels 0–12 of the old modes, Linux link
isolation for the then-vendored codecs). It does not match 0.0.2's level scale.
