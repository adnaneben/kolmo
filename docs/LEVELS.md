# Kolmo levels and command line (0.1.0 specification)

Status: specification for 0.1.0, owner decisions of 2026-11-22. The level numbers, names and letters below are a
long-term contract: they are not renamed or renumbered later. What a level *does* inside (which coder, which setting)
may improve between releases; what it *promises* (its place on the speed/size scale) does not change.

## 1. The scale

One scale, 1 to 9, from fastest to smallest. Each step is slower and smaller than the one before. Every level on the
scale decompresses at a practical speed; the extreme tier (ultra) is outside the scale on purpose.

| level | name     | letter | long option  | promise (contract)                                                          |
|------:|----------|--------|--------------|------------------------------------------------------------------------------|
| 1     | fastest  | `-F`   | `--fastest`  | the fastest Kolmo compresses; output smaller than lz4 -1 at comparable speed |
| 2     |          |        |              | between 1 and 3                                                              |
| 3     | light    | `-L`   | `--light`    | **default**; zstd -3 class speed, smaller output                             |
| 4     |          |        |              | between 3 and 5                                                              |
| 5     | balanced | `-B`   | `--balanced` | the middle; markedly smaller than light at about half its compress speed     |
| 6     |          |        |              | between 5 and 7                                                              |
| 7     | compact  | `-C`   | `--compact`  | small output, still fast to decompress (LZ class decode)                     |
| 8     |          |        |              | between 7 and 9                                                              |
| 9     | max      | `-X`   | `--max`      | the smallest output that still decompresses at practical speed               |
| —     | ultra    | `-U`   | `--ultra`    | extreme: decompresses about as slowly as it compresses, ~1 GB+ of memory     |

Digits `-1` … `-9` are the primary short form. `--level N` (N = 1..9) is the long form; `--level ultra` equals `-U`.
The letters are shortcuts for the five named anchors. Levels 2, 4, 6, 8 have no name and no letter.

When several level options are given, the last one wins (as in gzip, xz, zstd).

Default when no level option is given: **3 (light)**.

### Internal identifiers

C: `kolmo_params_for_level(int level, kolmo_params *p)`, with `KOLMO_LEVEL_MIN 1`, `KOLMO_LEVEL_MAX 9`,
`KOLMO_LEVEL_DEFAULT 3`, `KOLMO_LEVEL_ULTRA 10`, and named constants `KOLMO_LEVEL_FASTEST 1`, `KOLMO_LEVEL_LIGHT 3`,
`KOLMO_LEVEL_BALANCED 5`, `KOLMO_LEVEL_COMPACT 7`, `KOLMO_LEVEL_MAX_RATIO 9` (MAX is the scale bound). The table lives
in one place in the library; the CLI, Python and Rust all use it, so the same level gives the same bytes everywhere.

Python: `kolmo.compress(data, level=3)` with `level` 1..9 or `"ultra"`, or one of the names `"fastest"`, `"light"`,
`"balanced"`, `"compact"`, `"max"`. Rust: `Level::new(1..=9)`, `Level::FASTEST`, `Level::LIGHT`, `Level::BALANCED`,
`Level::COMPACT`, `Level::MAX`, `Level::ULTRA`; `Level::default()` is light.

### What each level runs (0.1.0 table; free to improve later, never to move across the scale)

Initial assignment, to be finalised by measurement (each level must be measurably distinct from its neighbours and
strictly ordered on the screening corpus; final numbers from the timed holdout):

| level | engine (0.1.0)                                                           |
|------:|---------------------------------------------------------------------------|
| 1     | fast LZ (KLZ, or the new fast KLZH2 setting if it measures better), minimal analysis |
| 2     | KLZH2 1, quick analysis                                                    |
| 3     | KLZH2 3 with analysis (the 0.0.x "balanced" default)                       |
| 4     | KLZH2 5                                                                    |
| 5     | KLZH2 7 (0.0.x "eco")                                                      |
| 6     | KLZH2 8                                                                    |
| 7     | KLZH2 9 or a faster ratio-tier setting, whichever is smaller at LZ decode speed |
| 8     | ratio tier, fast variant (BWT-only measured ~3x faster than ratio at the same size) |
| 9     | ratio tier (KLZMA/BWT, 0.0.x "ratio")                                      |
| ultra | context mixing (0.0.x `-x`)                                                |

## 2. Command line

```
kolmo [OPTIONS] [FILE...]
```

Behaviour follows gzip/xz/zstd conventions:

- `kolmo FILE` compresses FILE to `FILE.kolmo`. The input is **kept** (as zstd does); `--rm` removes it after a
  verified success.
- `kolmo -d FILE.kolmo` decompresses to `FILE`. A name without the `.kolmo` suffix needs `-o` or `-c`.
- No FILE, or FILE `-`: read standard input, write standard output. Refuses to write compressed data to a terminal
  unless `-f`.
- Several FILEs: each is processed on its own. `-o` is only allowed with one input.
- An existing output is not overwritten without `-f`; the error names the file.
- The output is staged and published atomically; permissions and mtime are copied from the input.
- Exit status: 0 success, 1 error (I/O, corrupt data, memory), 2 usage error. With several files, processing continues
  after an error and the exit status is 1.

### Options

| short | long                    | meaning                                                                 |
|-------|-------------------------|-------------------------------------------------------------------------|
| `-1`…`-9` | `--level N`         | level on the scale                                                      |
| `-F -L -B -C -X` | `--fastest --light --balanced --compact --max` | named anchors 1, 3, 5, 7, 9           |
| `-U`  | `--ultra`               | extreme tier                                                            |
| `-d`  | `--decompress`          | decompress (`unkolmo` / `kolmocat` aliases are out of scope for 0.1.0)  |
| `-z`  | `--compress`            | compress (the default)                                                  |
| `-c`  | `--stdout`              | write to standard output, keep inputs                                   |
| `-o FILE` | `--output FILE`     | output path (one input only)                                            |
| `-k`  | `--keep`                | keep inputs (the default; accepted for gzip habits)                     |
|       | `--rm`                  | remove the input after success                                          |
| `-f`  | `--force`               | overwrite outputs; allow binary to a terminal                           |
| `-t`  | `--test`                | decode and verify, write nothing                                        |
| `-l`  | `--list`                | show compressed size, original size, ratio, level/engine, checksum      |
| `-T N`| `--threads N`           | worker threads (0 = all cores)                                          |
| `-M SIZE` | `--memlimit SIZE`   | memory limit (512M, 8G, 50%, auto, max); env `KOLMO_MEMLIMIT`            |
| `-q`  | `--quiet`               | no warnings                                                             |
| `-v`  | `--verbose`             | report per file: sizes, ratio, speed                                    |
| `-h`  | `--help`                | help                                                                    |
| `-V`  | `--version`             | version                                                                 |
|       | `--block-size MiB`      | block size (was `-B`)                                                   |
|       | `--analysis none\|minimal\|quick\|full` | content analysis (was `-n`, `-Q`, `-q`, default)        |
|       | `--stream[=SEGMiB]`     | bounded-memory streaming (was `-s`, `-S`)                               |
|       | `--range OFF:LEN`       | random-access read of a range (was `-r`)                                |
|       | `--codec-level N`       | expert: the engine's own level for the chosen tier (was `-l`)           |
|       | `--target SPEC`, `--calib FILE` | per-chunk target (unchanged)                                    |
|       | `--native-leaf`, `--typed-huf`, `--gpu`, `--x-tables=...` | unchanged expert options              |
|       | `--`                    | end of options                                                          |

Combined short options work (`-dc`, `-9T4`, `-kf`). Long options accept `--opt=value` and `--opt value`.

Subcommands keep working: `kolmo calibrate`, and the archive/query commands `kolmo a|x|l|cat|grep` and `--index`.

### Removed in 0.1.0 (owner decision: clean break)

`-m speed|balanced|eco|hc|ratio|rt|fast|max|ultra`, `-x`, the old `-l LEVEL`, `-B MiB`, `-q`/`-Q` (analysis), `-n`,
`-s`, `-S`, `-r`, `-D`, `-t DICT`, `-P`, `kolmo train`, `kolmo profile`, and the two-positional form `kolmo IN OUT`.
Each removed form fails with exit status 2 and a one-line hint naming the replacement, for example:

```
kolmo: -m was removed in 0.1.0; use a level: -1 (fastest) .. -9 (max), -3 light is the default, -U ultra
kolmo: 'kolmo IN OUT' was removed in 0.1.0; use 'kolmo IN -o OUT'
```

### Mapping for people upgrading from 0.0.x

| 0.0.x            | 0.1.0                         |
|------------------|-------------------------------|
| `-m speed`       | `-1` / `-F`                   |
| `-m balanced` (default) | `-3` / `-L` (default)  |
| `-m eco`         | `-5` / `-B`                   |
| `-m hc`          | `-7` / `-C`                   |
| `-m ratio`       | `-9` / `-X`                   |
| `-x`             | `-U`                          |
| `kolmo in out`   | `kolmo in -o out`             |
| `kolmo -d in out`| `kolmo -d in -o out`          |

Bindings: Python `mode="speed"|"balanced"|"ratio"` and Rust `Mode::*` are replaced by `level`; the old names are
removed in 0.1.0 with the same mapping.

### Frame format

Unchanged (container version 12). Every 0.0.x decoder reads 0.1.0 output, and 0.1.0 reads 0.0.x output.
