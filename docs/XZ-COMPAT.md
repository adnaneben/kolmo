# Kolmo as a drop-in for xz and gzip

Kolmo's command line follows the gzip/xz/zstd conventions (`kolmo -h`, `docs/LEVELS.md`). This page lists the flags
that scripts and tar commonly pass to `xz` and `gzip`, what Kolmo does with each, and why. Everything marked
*same* or *added* is tested by `tests/test_filter.sh` (`make check`, CI on Linux with GNU tar and on macOS with
bsdtar).

## Use with tar

| tar | Compress | List / extract |
|---|---|---|
| GNU tar (Linux; `gtar` on macOS) | `tar -I 'kolmo -6' -cf a.tar.kolmo DIR` | `tar -I kolmo -xf a.tar.kolmo` |
| GNU tar, long form | `tar --use-compress-program='kolmo -6' -cf a.tar.kolmo DIR` | `tar --use-compress-program=kolmo -xf a.tar.kolmo` |
| bsdtar (macOS `tar`, FreeBSD) | `tar --use-compress-program='kolmo -6' -cf a.tar.kolmo DIR` | `tar --use-compress-program='kolmo -d' -xf a.tar.kolmo` |
| any tar, through a pipe | `tar -cf - DIR \| kolmo -6 > a.tar.kolmo` | `kolmo -dc a.tar.kolmo \| tar -xf -` |

- **GNU tar** appends `-d` itself when it reads, so the same program string works in both directions.
- **bsdtar** runs the program exactly as given in both directions: reading needs `kolmo -d`. On bsdtar, `-I` means
  something else (a file of names), so use `--use-compress-program`.
- Neither tar recognises `.kolmo` by suffix (`tar -a`, automatic detection on read): name the program.
- `.tar.kolmo` is the suggested name. Kolmo's own `-d` strips the `.kolmo` suffix, giving `a.tar`.
- **Memory.** A pipe up to 256 MiB is compressed as one frame: the same bytes as `kolmo -c` of the same data in a
  file. A longer pipe (a tar of a large tree) is compressed as a stream of 256 MiB segments, so memory stays bounded
  whatever the archive's size. Decompression from a pipe reads a stream segment by segment. On two copies of Silesia
  (424 MB) at level 6, 256 MiB segments gave 102,265,009 bytes against 102,867,009 for one frame, with 1.70 GB peak
  RSS against 1.84 GB (Apple M5, 2026-11-23). `--stream=N` chooses another segment size.
- The level is fixed by the program string (`'kolmo -6'`); `-T` and `-M` can go there too (`'kolmo -6 -T4'`).
  Frames are the same at every thread count.

## Decision table

*Same* means the flag has the meaning it has in xz/gzip. *Added* means this page's change added it. *Conflict* means
the letter already means something else in Kolmo; the table says what happens.

### Operation and output

| Flag | xz | gzip | Kolmo | Decision |
|---|---|---|---|---|
| `-z`, `--compress` | compress | (default) | compress (the default) | same |
| `-d`, `--decompress` | decompress | decompress | decompress | same |
| `--uncompress` | — | = `-d` | = `-d` | added |
| `-c`, `--stdout` | write standard output | write standard output | write standard output | same |
| `--to-stdout` | = `-c` | = `-c` | = `-c` | added |
| `-dc`, `-cd`, `-dcf`, `-9T4`, … | combined short flags | combined short flags | combined short flags | same |
| `-` | standard input | standard input | standard input | same |
| no FILE | stdin to stdout | stdin to stdout | stdin to stdout | same |
| `--` | end of options | end of options | end of options | same |
| `-k`, `--keep` | keep the input | keep the input | keep the input | same, but see the next row |
| default for FILE | input **removed** | input **removed** | input **kept** (as zstd) | differs: `--rm` gives the xz/gzip behaviour |
| `-f`, `--force` | overwrite; compressed data to a terminal | same | same | same |
| `-dcf` on data that is not compressed | copied through unchanged | copied through (`zcat -f`) | copied through unchanged | added |
| `-t`, `--test` | test | test | test | same |
| `-l`, `--list` | list | list | list (compressed and original size, ratio, engine, CRC) | same; the columns differ |
| `-q`, `--quiet` / `-v`, `--verbose` | | | | same |
| `-h`, `--help` / `-V`, `--version` | | | | same |
| `-H`, `--long-help` | long help | — | — | not added (exit 2): `-h` is the full help |

### Levels and resources

| Flag | xz | gzip | Kolmo | Decision |
|---|---|---|---|---|
| `-1` … `-9` | presets | levels | levels 1 (fastest) … 9 (max) | same scale; Kolmo's own levels (`docs/LEVELS.md`) |
| default level | `-6` | `-6` | `-3` | differs: for xz's ratio use `-6` (smaller than `xz -9` on Silesia) |
| `-0` | fastest preset | — | = `-1` | added: the fastest Kolmo level. Kolmo has no level 0, and `--level 0` stays an error |
| `-e`, `--extreme` | slower variant of the preset | — | accepted, no effect (`-9e` = `-9`) | added. Kolmo has no "same level, more effort" variant; `-U` (ultra) is not one, it decodes about as slowly as it compresses, so `-e` is not mapped to it |
| `--fast`, `--best` | = `-0`, `-9` | = `-1`, `-9` | = `-1`, `-9` | added |
| `-T N`, `--threads=N` | threads, 0 = all | — (pigz `-p`) | threads, 0 = all (the default) | same; output identical at every N |
| `-M SIZE`, `--memlimit=SIZE` | memory limit | — | memory limit (`512M`, `8G`, `50%`, `max`) | same |
| `--memory=SIZE` | = `--memlimit` | — | = `--memlimit` | added |
| `--block-size=SIZE` | bytes (`16MiB` accepted) | — | MiB, a number (`--block-size 16`) | conflict in units: `--block-size 16MiB` is refused (exit 2), a bare number means MiB |

### Format, names and checks

| Flag | xz | gzip | Kolmo | Decision |
|---|---|---|---|---|
| `-n`, `--no-name` | — | store no name or time | `-n`: removed 0.0.1 option, exit 2 with a hint; `--no-name`: accepted | `--no-name` added (no effect: Kolmo frames store no name or time, so output is already reproducible); `-n` kept as the 0.0.1 hint |
| `-N`, `--name` | — | restore the stored name | — | not added (exit 2): nothing is stored |
| `-S SUF`, `--suffix=SUF` | other suffix | other suffix | `-S` is a removed 0.0.1 option (exit 2) | not added: the suffix is always `.kolmo`; use `-o` or `-c` for other names |
| `-r`, `--recursive` | — | recurse into directories | `-r` is a removed 0.0.1 option (exit 2) | not added: use tar, or `find … -exec kolmo {} +` |
| `-F FMT`, `--format=FMT` | container format | — | `-F` = `--fastest` (level 1) | conflict: one format only; `xz -F xz` must not be carried over |
| `-C CHECK`, `--check=CHECK` | integrity check | — | `-C` = `--compact` (level 7) | conflict: every frame checks itself (CRC-32C of header and index, and a 32-bit hash per block, FORMAT.md 5.1) |
| `-Q`, `--no-warn` | warnings don't set exit 2 | — | `-Q` is a removed 0.0.1 option (exit 2) | not added: Kolmo's exit 2 means a usage error, never a warning |
| `-L`, `--license` | — | licence | `-L` = `--light` (level 3) | conflict |
| `--rsyncable`, `--single-stream`, `--no-sparse`, `--ignore-check`, `--robot`, `--files` | | | — | not added (exit 2) |

### Behaviour

| | xz / gzip | Kolmo |
|---|---|---|
| Exit status | 0 success, 1 error, 2 warning | 0 success, 1 error, 2 usage error. With several files, processing continues and the status is 1 |
| Compressed data to a terminal | refused without `-f` | same; reading compressed data from a terminal is refused too |
| Concatenated files (`cat a b \| … -d`) | decoded one after the other | same |
| A reader that stops early (`… -d \| head`) | ended by SIGPIPE (status 141 in the shell) | same; where SIGPIPE is ignored (some CI runners), exit 1 with no message |
| SIGINT, SIGTERM, SIGHUP | the partial output file is removed | same: the private staging file is removed and no output appears (added) |
| Output files | | written to a staging file and published atomically with the input's mode and mtime; never over an existing file without `-f` |
