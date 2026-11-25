# Startup-aware speed floors for small files — preregistered protocol

Written and committed with `dev.json` and `holdout.json` before any measurement on either split.

## Question

`--target 'decode>=R'` / `'encode>=R'` predicts each setting's time as `setup + bytes / rate` from a built-in
in-process table (x86 VM, 16 Nov 2026). End-to-end floors on small files are dominated by process launch and file
I/O, which the table does not contain. B26 (`benchmark/waves/2026-11-25/overnight/target-floors`) recorded three
GMP-manual floors (decode ≥ 30, decode ≥ 80, encode ≥ 60 MB/s) that the selector missed while another preset met
them. Does a startup-aware estimate raise end-to-end floor attainment on new producers, at no loss of the
size objective and with selection time and memory charged?

## Policies (no C change, no default change)

Every policy is a `--calib FILE` table read by the unchanged pinned encoder (`a1d39559`, SHA-256 `b67bbddb…`,
identical to B26's). The stock path is the same binary with no `--calib`, no `KOLMO_*`/`CZN_*` environment and
no `~/.config/kolmo/calibration`.

- **stock** — built-in table.
- **stock-dup** — a second, independent stock arm: duplicate timing control.
- **B (launch)** — built-in rates; every setup term gets `+Pe` (encode) and `+Pd` (decode) ms, where `Pe`/`Pd`
  are the median end-to-end wall times of `-1 -T1` encode and stock decode of a 1-byte file, measured in the
  development phase (21 rounds).
- **C (host + launch)** — `kolmo calibrate -s 0.1` run on this host with the development files only (in-process
  host rates and setups; classes the development files do not cover are measured on calibrate's built-in synthetic
  data, as the tool does), then `+Pe`/`+Pd` on every entry as in B. (Amended before any measurement: the first
  text said such classes keep built-in values; `kolmo calibrate` always emits all 54 entries.)

The development phase picks one of B and C (more development floor cases met, ties → smaller total bytes on met
cases). Only that one goes to the holdout. If neither beats stock on development, the holdout still runs stock vs
B and the result is a rejection.

## Workload

- Splits: 7 development and 11 holdout files, one per upstream producer, 4.6 KB–400 KB, text, JSON, XML, C
  header, HTML and Mach-O. No producer overlaps B26 (GMP, GnuPG, GnuTLS), #99's pilots (zpaq, HDF5, libtiff,
  PCRE2, GNU tar, libjpeg-turbo, lz4, xz, libwebp, Poppler), the small-dictionary set or the impact corpus; the
  GnuPG/g10code and GMP families (gpgme, libgcrypt, libassuan, libksba, libgpg-error, pinentry, MPFR, MPC) are
  excluded too. The three recorded B26 inputs are never used for selection or the decision.
- Specs per file: the six ordinary B26 floors `encode>=1MB/s`, `encode>=20MB/s`, `encode>=60MB/s`,
  `decode>=1MB/s`, `decode>=30MB/s`, `decode>=80MB/s`, plus `size` (objective check).
- Whole-file target oracle: the nine selector settings forced directly (`-1`; `-3 --analysis none`; `-3`; `-5`;
  `-7`; `-8`; `-9`; `--ultra --codec-level 1`; `--ultra`), as frozen in B26.
- Each cell is a fresh file-to-file process with `-T 1`; target arms run with `-v` (route capture), direct arms
  with `-q`. Every frame is decoded by the stock Homebrew 0.0.2 reader (SHA-256 `5c116ba0…`) with `-T 1`.
  Wall time from spawn to `wait4`; peak RSS from `wait4`'s rusage. Hashing outside timing.
- Rounds: development 1 warmup + 5 scored; holdout 1 warmup + 11 scored. All cells of a file are shuffled within a
  round (seed 20261125); file order shuffled per round. The whole run holds the machine-wide timing lock.

## Scoring

- A floor case is **met** when the arm's median end-to-end rate meets it: decode floors use the stock reader's
  decode of the arm's own frame; encode floors use the `--target` process itself, so trial compressions, table
  parsing and classification are charged. The B26-style "direct rate of the chosen setting" is also reported.
- Oracle: smallest direct frame among settings whose own median end-to-end rate meets the floor.

## Holdout gates (all must hold to qualify)

1. **Attainment**: candidate meets strictly more of the 66 floor cases than stock, and the gain exceeds the number
   of cases where stock and stock-dup disagree (minimum 1).
2. **No new misses**: no case that stock and stock-dup both meet is missed by the candidate.
3. **Objective**: on cases both stock and candidate meet, candidate total bytes ≤ 1.01 × stock; the `size` spec
   produces the stock frame on every file.
4. **Selection cost**: geometric mean over the 66 cases of the median paired encode-time ratio candidate/stock ≤
   1.05; every case's median paired peak encode RSS ratio ≤ 1.10.
5. **Integrity**: every round trip restores the input SHA-256; each (file, spec, arm) emits one frame across rounds.

A qualifying policy ships only as an opt-in calibration file in a draft PR; the owner decides any adoption.

## Amendment 1: quiet-host replication (2026-11-26, before any measurement of `holdout2`)

The first run (results/) ran under a 1-minute load of 80–310 on 10 cores because other jobs ran outside the timing lock.
Under the coordinator's quiet-window rule, its timings are invalid. The protocol, policies, specs, rounds, seed, gates
and development rule above are unchanged. Two things are added:

- **Load gate**: before each round (and before the launch measurement and `kolmo calibrate`) the harness waits for
  the 1-minute load average to be ≤ 12. It records the load before and after on every row. A round that ends above
  12 is discarded and redone, and discarded rows are kept in raw.jsonl with `invalid_load: true`.
- **New sealed holdout `holdout2.json`**: 11 producers not used anywhere before (cairo, GIFLIB, Fontconfig, libusb,
  libaec, C-Blosc2, LZO, Tectonic, ICU, libX11, Graphite2). The first holdout has been seen, so **`holdout2` alone decides**.
  The first holdout is re-measured as a replication of the loaded run and does not count toward the decision. The
  reference replay on the B26 inputs follows, explanatory only.

The development phase is re-measured too, so B or C is re-chosen on quiet development data by the same rule.
