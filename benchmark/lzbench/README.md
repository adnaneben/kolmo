# kolmo in lzbench

[lzbench](https://github.com/inikep/lzbench) is the standard in-memory benchmark for lossless
compressors. These files add kolmo to a stock lzbench checkout as an ordinary codec, so kolmo is
measured by the same harness, on the same input, with the same flags as every other entry.

```sh
git clone https://github.com/inikep/lzbench
sh benchmark/lzbench/setup.sh ./lzbench "KOLMO ZSTD BROTLI XZ"   # or omit the list to build every codec
./lzbench/lzbench -ekolmo/zstd,3,19/xz,6,9 silesia.tar
```

| File | Role |
|---|---|
| `kolmo_lzbench.cpp` | The codec wrapper: lzbench levels 0–5 are kolmo's modes `rt`, `fast`, `eco`, `hc`, `max`, `ultra` |
| `kolmo.mk` | The lzbench build fragment (`mk/kolmo.mk`) |
| `setup.sh` | Builds kolmo, isolates it, drops it into lzbench, registers it and builds lzbench |

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
