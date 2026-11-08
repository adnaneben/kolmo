# kolmo in lzbench

[lzbench](https://github.com/inikep/lzbench) is the standard in-memory benchmark for lossless
compressors. These files add kolmo to a stock lzbench checkout as an ordinary codec, so kolmo is
measured by the same harness, on the same input, with the same flags as every other entry.

```sh
git clone https://github.com/inikep/lzbench
sh benchmark/lzbench/setup.sh ./lzbench "KOLMO ZSTD BROTLI XZ"   # or omit the list to build every codec
./lzbench/lzbench -ekolmo,1,2,3,4/zstd,1,2,3,5 silesia.tar   # the fast tier; -ekolmo alone runs all 13 levels
```

| File | Role |
|---|---|
| `kolmo_lzbench.cpp` | The codec wrapper: lzbench levels 0–12 are a ladder of ordinary kolmo settings, from `rt` to `ultra` (see the file header) |
| `kolmo.mk` | The lzbench build fragment (`mk/kolmo.mk`) |
| `setup.sh` | Builds kolmo, isolates it, drops it into lzbench, registers it and builds lzbench |

## The ladder

| lzbench level | kolmo setting | lzbench level | kolmo setting |
|---|---|---|---|
| 0 | `-m rt` | 7 | `-m max -l 1` |
| 1 | `-m fast -l 1 -q` | 8 | `-m eco` |
| 2 | `-m fast -l 2 -q` | 9 | `-m hc` |
| 3 | `-m fast -q` | 10 | `-m max` |
| 4 | `-m fast` | 11 | `-m ultra -l 1` |
| 5 | `-m hc -l 9` | 12 | `-m ultra` |
| 6 | `-m hc -l 12` | | |

Each is reproducible with `kolmo -T1 <setting> in out`. `-q` is quick analysis (only the cheap structure
transforms). Levels 11 and 12 need about 4 GB of memory. Measured results are in [`results/`](results/).

## Fairness

- **One thread.** kolmo runs with `threads = 1`, like every lzbench entry. (kolmo's own multi-core
  scaling is real but is not what lzbench measures.)
- **Defaults.** Each level uses kolmo's default block size and transforms, exactly what a user gets
  from `kolmo -T1 -m <mode>`. Nothing is tuned to the benchmark files.
- **Same compiler options.** kolmo is built at `-O3 -march=native`; lzbench builds every other codec at
  `-O3` plus the same `MOREFLAGS="-march=native"`.
- **No shared code.** kolmo vendors its own zstd, lz4, brotli and libbsc, and `setup.sh` adds a static
  liblzma. All of it is partially linked into one object whose only global symbols are `czn_*`, and its
  C++ symbols are renamed so their COMDAT groups cannot merge with lzbench's own libbsc. No other entry
  ever runs kolmo's copies, and kolmo never runs theirs.
- **Verified.** lzbench checks every decompressed buffer against the input.

Linux only (GNU `ld -r` and `objcopy`); needs a static liblzma (`apt install liblzma-dev`).
