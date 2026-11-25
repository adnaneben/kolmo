# Kolmo levels and command line (0.0.2 specification)

Status: specification for 0.0.2, owner decisions of 2026-11-22. The level numbers, names and letters below are a
long-term contract: they are not renamed or renumbered later. What a level *does* inside (which coder, which setting)
may improve between releases; what it *promises* (its place on the speed/size scale) does not change.

## 1. The scale

One scale, 1 to 9, from fastest to smallest. Each step gives smaller output than the one before, and costs more time.
The scale has two decode tiers. Levels 1-7 decompress at LZ speed (hundreds of MiB/s; about 700-1500 MiB/s on one
Apple M5 core) and spend more *compress* time at each step. Levels 8 and 9 use block sorting: smaller again, but they
decompress about ten times more slowly (about 50-60 MiB/s per core), even though they compress faster than levels 6-7.
No engine exists that is both smaller than level 7 and slower to compress but faster to decode than level 8, so the
step from 7 to 8 trades decode speed, not compress speed (decision of 2026-11-23, measured on the screening corpus).
Every level on the scale decompresses at a practical speed; the extreme tier (ultra) is outside the scale on purpose.

| level | name     | letter | long option  | promise (contract)                                                          |
|------:|----------|--------|--------------|------------------------------------------------------------------------------|
| 1     | fastest  | `-F`   | `--fastest`  | the fastest Kolmo compresses; output smaller than lz4 -1 at comparable speed |
| 2     |          |        |              | between 1 and 3                                                              |
| 3     | light    | `-L`   | `--light`    | **default**; zstd -3 class speed, smaller output                             |
| 4     |          |        |              | between 3 and 5                                                              |
| 5     | balanced | `-B`   | `--balanced` | the middle; markedly smaller than light, much slower to compress, same fast decode |
| 6     |          |        |              | between 5 and 7                                                              |
| 7     | compact  | `-C`   | `--compact`  | the smallest output that still decompresses at LZ speed                      |
| 8     |          |        |              | block-sorting tier: smaller than 7, decompresses ~10x slower                 |
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

### What each level runs (0.0.2 table; free to improve later, never to move across the scale)

Initial assignment, to be finalised by measurement (each level must be measurably distinct from its neighbours and
strictly ordered on the screening corpus; final numbers from the timed holdout):

| level | engine (0.0.2)                                                           |
|------:|---------------------------------------------------------------------------|
| 1     | fast LZ (KLZ, or the new fast KLZH2 setting if it measures better), minimal analysis |
| 2     | fast KLZH2 parse (KLZH2 level 0), minimal analysis                         |
| 3     | KLZH2 3 with analysis (the 0.0.1 "balanced" default)                       |
| 4     | KLZH2 5                                                                    |
| 5     | KLZH2 7 (0.0.1 "eco")                                                      |
| 6     | KLZH2 8                                                                    |
| 7     | KLZH2 9 or a faster ratio-tier setting, whichever is smaller at LZ decode speed |
| 8     | ratio tier, fast variant: BWT for text, JSON and log streams; other blocks BWT only where a 512 KiB slice finds it 2% smaller than KLZMA 1, else KLZMA 1 (from 0.0.3) |
| 9     | ratio tier (KLZMA/BWT, 0.0.1 "ratio")                                      |
| ultra | context mixing (0.0.1 `-x`)                                                |

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
| `-d`  | `--decompress`          | decompress (`unkolmo` / `kolmocat` aliases are out of scope for 0.0.2)  |
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

### Removed in 0.0.2 (owner decision: clean break)

`-m speed|balanced|eco|hc|ratio|rt|fast|max|ultra`, `-x`, the old `-l LEVEL`, `-B MiB`, `-q`/`-Q` (analysis), `-n`,
`-s`, `-S`, `-r`, `-D`, `-t DICT`, `-P`, `kolmo train`, `kolmo profile`, and the two-positional form `kolmo IN OUT`.
Each removed form fails with exit status 2 and a one-line hint naming the replacement, for example:

```
kolmo: -m was removed in 0.0.2; use a level: -1 (fastest) .. -9 (max), -3 light is the default, -U ultra
kolmo: 'kolmo IN OUT' was removed in 0.0.2; use 'kolmo IN -o OUT'
```

### Mapping for people upgrading from 0.0.1

| 0.0.1            | 0.0.2                         |
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
removed in 0.0.2 with the same mapping.

### Frame format

Unchanged (container version 12). Every 0.0.1 decoder reads 0.0.2 output, and 0.0.2 reads 0.0.1 output.

## 3. Implementation notes (decisions where this specification was silent)

Recorded by the 0.0.2 implementation (lane 1). zstd's behaviour was followed unless noted.

Levels and library
- Internal table (`src/api/api.c`, `kolmo_level_table`), in 0.0.1 knobs: 1 = rt with minimal analysis (exactly 0.0.1
  `-m speed`), 2 = fast at codec level -1 (the fast KLZH2 parse, KLZH2 level 0) with minimal analysis, 3 = fast (0.0.1 balanced), 4 = hc's planning
  at codec level 6 (KLZH2 5) (lane 3's measured rows for 2 and 4), 5 = eco, 6 = hc at codec level 16 (KLZH2 8), 7 = hc, 8 = max at codec level 1 (BWT for every
  block), 9 = max (0.0.1 ratio), ultra = ultra. Levels 1, 3, 5, 7 and ultra therefore write the same bytes as 0.0.1's
  `speed`, `balanced`, `eco`, `hc` and `-x`. Level 9 writes the same bytes as 0.0.1's `ratio` except where block sorting now
  wins on a binary block (the gate was widened in 0.0.2: two raw videos of the 21 held-out files, 2.2% smaller in all). On small inputs neighbouring levels can coincide (6 and 7 did on a
  200 KB test file); making every level measurably distinct is the measuring lane's job.
  `benchmark/determinism/expected.sha256` pins every level, 2, 4, 6 and 8 included.
- Since 0.0.2 (unreleased, encoder only; every frame is still container version 12): level 1 has a new KLZ parse and no
  longer writes 0.0.1's `speed` bytes; levels 2 and 3 have KLZH2 parse and entropy-chunking changes and no longer write
  0.0.1's `balanced` bytes; level 8 codes 32 MiB blocks instead of 128 MiB (less memory; -9 keeps the large window and
  is now smaller than -8 on large text). Levels 4-7, 9 and ultra choose as before.
- An out-of-range level returns a new error code, `KOLMO_ERR_PARAM` (-15).
- `-10`, `-19`: a run of digits is read as one number (as zstd), then refused (levels are 1..9; ultra is `-U`).

Letters whose meaning changed
- `-B`, `-l`, `-t` and `-q` mean `--balanced`, `--list`, `--test` and `--quiet` now. The 0.0.1 forms are recognised
  where they are unambiguous and get the removed-form hint: `-B` or `-l` followed (in the same argument or as the next
  argument) by a number that is not an existing file (`-B 0.5`, `-B4`, `-l 9`, `-l9`), and `-t` followed by a name
  ending in `.dict`. The old analysis option `-q` cannot be told from `--quiet`.
- The two-positional form is recognised when exactly two names are given without `-o`/`-c`, and the second does not
  exist yet, or (compressing) ends in `.kolmo`, or (decompressing) does not. Otherwise two names are two inputs.

Files and output
- No prompting, ever: an existing output is refused without `-f` (exit 1, the message names the file), also on a
  terminal (zstd asks there).
- The output is never the input file (same device and inode), in every path, even with `-f`: exit 1, input untouched.
  `kolmo -c FILE >> FILE` is refused the same way.
- Compressing a name that already ends in `.kolmo` is refused unless `-f` (gzip). Decompressing a name without the
  suffix, without `-o` or `-c`, is an error for that file (exit 1).
- Directories are refused ("is a directory"; there is no recursive mode). Other non-regular inputs (FIFOs, devices)
  are refused unless `-c` or `-f` (gzip).
- Staging: every output, streamed and from standard input included, is written to a private file in the output's
  directory and published only on success, by `link` (never replacing a file that appeared meanwhile) or, with `-f`
  over a plain file, by `rename`. A failed run never touches an existing file and leaves no staging file. Over a
  symlink or hard link with `-f`, the content is written through the existing path, so the link and the target's mode
  are kept (0.0.1 behaviour).
- Metadata: permission bits `mode & 0777` (setuid, setgid and sticky are dropped), access and modification times;
  ownership is not changed. Outputs from standard input get `0666 & ~umask` and the current time.
- `--rm` removes the input after its output is published (decoded outputs are verified before publishing; compressed
  frames carry block checksums; there is no extra verification pass). It is ignored, with a warning, with standard
  output, `-t` and `-l`.
- `-c` with several inputs writes their frames one after another; `-d` decodes concatenated frames in order (as zstd)
  and fails (exit 1) on trailing bytes that are not a frame. `-l` reports such a file as one entry with `N frames`.
  `--range` needs a file of exactly one frame.
- Terminals: compressed data is not written to a terminal without `-f` (exit 1, as zstd); `-d` and `-t` do not read
  compressed data from a terminal without `-f`; compressing from a terminal prints a notice and reads until end of file.
- A streamed (CZNS) file read from a pipe is buffered (in memory, then a temporary file) before decoding, so nothing
  read by the format check is lost.

Options
- Long options must be spelled in full (no abbreviations, unlike getopt_long). `--stream` takes its segment size only
  as `--stream=SEGMiB`. `-o -` is standard output. `-c` with `-o`, and `-o` with `-t` or `-l`, are usage errors.
- Ranges checked (exit 2 otherwise): `-T` 0..4096, `--level` 1..9 or a name or `ultra`, `--codec-level` 1..1000,
  `--block-size` above 0 and at most 4095 MiB, `--range OFF:LEN` digits only. `--range` implies decompression, writes
  standard output unless `-o`, and clamps LEN to the end of the data; an offset past the end is an error (exit 1).
- Messages: silent by default; `-v` prints sizes, ratio and speed per file on standard error; `-q` silences warnings
  (not errors).
- `-l`: compressed size, original size, ratio, the header CRC-32C of the (first) frame, and the engine: the tier
  recorded in header byte 5 (`rt`, `fast`, `eco`, `hc`, `max`, `ultra`; the frame does not record the level) and the
  block codecs used. It does not read standard input (`-l` without FILE is a usage error).

Subcommands
- `calibrate`, `cat`, `grep`, `archive`, `extract`, `contents` are recognised only as the first argument; a file of
  that name is `kolmo -- NAME` or `kolmo ./NAME`. `kolmo a`, `x` and `l` (0.0.1 names, owner decision during review)
  print the removed-form hint naming `archive`, `extract` and `contents`. `--index` is recognised anywhere before `--`
  and keeps its `in out.kol` positional form. `--index` and `archive` take the level options of the main command line
  (`-1`..`-9`, letters, `--level`, `--codec-level`, `--block-size`); their 0.0.1 `-m`, `-x`, `-l N` and `-B MiB` get
  the removed-form hint.
