# Kolmo 1.2.0-rc2 on Linux x86-64: build, tests, identity, tier A1, tier C

Run on 15 November 2026 by a Claude Code cloud session, following `benchmark/rc2/VM-TASK.md`, `benchmark/rc2/PROTOCOL.md`
and `benchmark/rc1/PROTOCOL.md`. Every timed program runs on **one thread**.

**This is a shared cloud VM, so the timings are noisy.** Steal time was low (about 0.2% of all CPU time, or about 1% of
the one busy core; see "Steal time"). Even so, the drift rows of A1 moved by up to 34% between the start and the end of the tier:
zstd 3 compressed at 163 MB/s at the start and 219 MB/s at the end, and kolmo 16 at 398 and 518 MB/s.
Read speed differences below about 30% as noise on this machine; sizes are exact.

## Summary

- **Build and tests: all passed.** `make ARCH= all lib`, `make ARCH= check`, `make check-sanitizers`, `make check-rc` and
  `make ARCH= experimental-check` all exited 0 (`logs/step2_status.txt`).
- **Identity: passed.** The release CLI (`make ARCH=`) and the control CLI (`CFLAGS="-O3 -DKOLMO_FORCE_GENERIC_INT_FWD"`)
  wrote byte-identical frames for all 12 Silesia files at `-m balanced -T1`, `-m hc -T1` and `-m fast --typed-huf -T1`:
  36 of 36 identical (`logs/identity.tsv`).
- **Tier A1** (lzbench 2.4.1 at `9be0e25`, `silesia.tar`, 211,948,032 bytes): all 45 rows ran, with no errors and no
  `threads` or `busy` marks. The control library (`kolmo_ctl`, generic integer paths) gave the same size as the release
  library at every one of the 14 levels measured. Its speeds were within the noise of the release library's.
- **Tier C**: FASTQ (3 files x 16 settings), video (3 x 19) and research codecs (6 x 14): every row round-tripped (SHA-256
  checked); none failed. Two rows are marked `threads`: zstd `-3` and `-19` on `silesia.tar` (see notes).
- Selected A1 pairs (size; compress / decompress MB/s, start of tier):

  | Kolmo | Size | MB/s | Rival | Size | MB/s |
  |---|---:|---|---|---:|---|
  | `-m speed` (16) | 99,918,118 | 398 / 2123 | lz4 | 100,297,674 | 532 / 2482 |
  | `-m fast -l 1 -Q` (17) | 70,908,594 | 374 / 966 | zstd 1 | 73,191,342 | 388 / 1112 |
  | `-m balanced` (4) | 58,245,973 | 169 / 784 | zstd 3 | 66,132,559 | 163 / 959 |
  | `-m hc` (9) | 47,624,843 | 2.3 / 619 | zstd 19 | 52,843,859 | 1.7 / 735 |
  | `-m ratio` (10) | 40,687,879 | 2.2 / 41 | xz 9 | 48,772,176 | 1.4 / 96 |
  | `-x` (12) | 35,770,715 | 0.2 / 0.2 | zpaq 5 | 40,485,070 | 0.4 / 0.4 |

- Observation, not a failure: `kolmo,13` (`-x -l 2`) gave exactly the same size as `kolmo,12` (`-x`) on `silesia.tar`
  (35,770,715 bytes), with both libraries.

## Machine

- Intel Xeon Processor @ 2.10 GHz (family 6, model 207: Emerald Rapids), KVM guest, 4 vCPUs (1 thread per core),
  L2 8 MiB, L3 260 MiB, AVX-512. 15 GiB memory, no swap.
- Linux `6.18.44-fc-v470` (Firecracker VM), Ubuntu 24.04 userland.
- gcc / g++ 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1), used for everything; clang 18.1.3 is installed but was not used.
  Python 3.13 runs the drivers.
- Full `lscpu`, `nproc`, `free`, `uname -a`, compiler versions: `logs/machine.txt`.
- Shared VM: yes (hypervisor KVM; steal time is non-zero, see below).

## Builds and versions

- Kolmo: commit `94b1132` on `claude/release-rc2-20261115` (VERSION 1.2.0-rc2), the branch head. PROTOCOL.md names
  `48acbf6`; `94b1132` adds only the rc2 benchmark drivers and a kvid fix to its colour-space tag parsing.
  `make ARCH= all lib` gives `-O3 -fno-plt`, with no `-march`.
- Research tools for tier C (`kolmo-fastq`, `kvid`, `krc-cpu`, `krc-auto`, `kbc`, `kbx`): `make ARCH=` of the same tree.
  `krc-auto` is the CPU build here; there is no Metal on Linux.
- lzbench 2.4.1 at `9be0e25`, a fresh clone, built with its own default flags: `-O3 -DNDEBUG -U_FORTIFY_SOURCE`, gcc 13.3,
  **no** `-march=native`. The codec versions inside it are the ones the A1 table prints (zstd 1.5.7, xz 5.8.4,
  brotli 1.2.0, bsc 3.3.12, bzip3 1.5.4, kanzi 2.6.0, zpaq 7.15, lz4 1.11.0, and so on).
- Kolmo in lzbench: `scripts/setup_linux_dl.sh`, a Linux transcription of `benchmark/lzbench/setup_macos.sh`. See
  "Deviations" for why `setup.sh` was not used.
  - Two exports of `94b1132` are built with `make ARCH= lib`: the release build, and the control build with
    `ARCH="-DKOLMO_FORCE_GENERIC_INT_INV -DKOLMO_FORCE_GENERIC_INT_FWD"`.
  - Each export links the system static `liblzma.a` (as the Mac links Homebrew's) and uses a GNU ld version script, so
    the library exports only `kolmo_*` (19 symbols each).
  - Both libraries are loaded at run time by the repository's `kolmo_lzbench_dl.cpp`.
  - Checksums of the two libraries: `logs/lzbench_kolmo_BUILD-INFO.txt`.
- Command-line rivals for tier C (`logs/apt_install.log`, `logs/bsc_build.log`):
  - zstd 1.5.5, xz-utils 5.4.5, brotli 1.1.0 and bzip3 1.4.0, all Ubuntu 24.04 packages.
  - bsc 3.4.0 from source: libbsc commit in `logs/bsc_commit.txt`; CMake Release with `-DBSC_ENABLE_NATIVE_COMPILATION=OFF
    -DBSC_ENABLE_OPENMP=OFF -DBSC_ENABLE_CUDA=OFF`. That is one thread with no tuning for this CPU; the `bsc` command
    line in `make_tools.py` has no thread option.
- Inputs (SHA-256 in `logs/inputs_sha256.txt`):
  - `silesia.tar`: the 12 files of silesia.zip in alphabetical order, `tar -b1 --owner=0 --group=0`, 211,948,032 bytes.
    Plain `tar` pads the archive to a 10 KiB record (211,957,760 bytes); `-b1` gives the protocol's size.
  - FASTQ: `fetch_fastq.py`, first 100,000 reads of each run. SRR1770413 is 63,177,790 bytes, SRR2584863 is 36,637,534
    (the protocol's size) and SRR835775 is 22,877,790.
  - Video: akiyo_cif, foreman_cif and mobile_cif `.y4m` from media.xiph.org.

## Steps

| Step | Result | Log |
|---|---|---|
| 1 machine record, vmstat during timed tiers | done | `logs/machine.txt`, `logs/vmstat_*.log`, `logs/procstat_*` |
| 2 `make ARCH= all lib` | **pass** (3 compiler warnings) | `logs/build_all_lib.log` |
| 2 `make ARCH= check` | **pass** (`ALL PASSED`, 22 min) | `logs/check.log` |
| 2 `make check-sanitizers` | **pass** | `logs/check_sanitizers.log` |
| 2 `make check-rc` | **pass** | `logs/check_rc.log` |
| 2 `make ARCH= experimental-check` | **pass** | `logs/experimental_check.log` |
| 3 identity, release vs `-DKOLMO_FORCE_GENERIC_INT_FWD` | **pass**, 36/36 identical | `logs/identity.tsv`, `scripts/identity.sh` |
| 4 lzbench A1, `kolmo` | **ran**, 45/45 rows, 0 errors, 0 marks | `A1/A1.jsonl`, `logs/A1_run.log` |
| 4 lzbench A1, `kolmo_ctl` | **ran**, 21/21 rows, 0 errors, 0 marks | `A1_ctl/A1.jsonl`, `logs/A1_ctl_run.log` |
| 5 tier C FASTQ | **ran**, 48 rows, 0 errors, 0 marks | `C/fastq.jsonl`, `logs/C_fastq.log` |
| 5 tier C video | **ran**, 57 rows, 0 errors, 0 marks | `C/video.jsonl`, `logs/C_video.log` |
| 5 tier C research codecs | **ran**, 84 rows, 0 errors, 2 marked `threads` | `C/research.jsonl`, `logs/C_research.log` |

## Deviations and notes

1. **`setup.sh` was not used as is.** It registers only levels 0 to 12, under the name "kolmo 1.1.0". It has no
   `kolmo_ctl`, and it builds with `-march=native`, which VM-TASK.md excludes. Tier A1 needs levels 0 to 17 and the
   control library, which is what `setup_macos.sh` sets up, so `scripts/setup_linux_dl.sh` does the same on Linux:
   - a GNU ld version script in place of `-exported_symbols_list`;
   - `.so` files in place of `.dylib`;
   - the codec registration taken verbatim from `setup_macos.sh`.

   Nothing in the repository's source was changed.
2. **lzbench needs `LD_LIBRARY_PATH`.** Its Rust codecs library has a relative rpath, so the lzbench binary needs
   `LD_LIBRARY_PATH=<lzbench>/misc/rust-codecs/target/release`. `scripts/run_a1.sh` sets it.
3. **`_FORTIFY_SOURCE`.** Ubuntu's gcc defines `_FORTIFY_SOURCE` by default. lzbench turns it off for its own codecs
   (`-U_FORTIFY_SOURCE`); Kolmo's library is built with `make ARCH= lib` as told, so it keeps the default. The Mac run
   had no such difference. If anything, it can only cost Kolmo some speed in A1.
4. **The control A1 run** is `benchmark/rc1/run_lzbench.py` with every `kolmo,N` entry renamed `kolmo_ctl,N`
   (`scripts/run_lzbench_ctl.py`, a sed of the original). It is limited to the Kolmo rows and the drift rows lz4 and
   zstd 3: the rivals' rows would have been the same as in the main run.
5. **The `sysctl` error is harmless.** `run_lzbench.py` asks `sysctl` for `machdep.cpu.brand_string`, a macOS-only key.
   On Linux `sysctl` prints an error and the `machine` field of the env line stays empty. The rows are not affected; the
   machine is described above.
6. **zstd marked `threads` in tier C.** On `silesia.tar`, `zstd -3 -T1` and `zstd -19 -T1` used more CPU time than
   elapsed time. The likely cause is the asynchronous file I/O of the zstd 1.5.x command line, which runs beside the
   one compression thread. The rows are reported as marked, not dropped. The same settings in A1 (in memory) are not
   marked.
7. **Tier C was restarted once before any row was kept.** The first start of tier C began twice at the same moment:
   a queued launcher and a manual start, sharing one work directory. Both were killed within about a minute, with
   every process of theirs. Their output files and logs were deleted, and tier C was run again from the beginning,
   alone. Only that clean run is reported. `logs/procstat_C_before.txt` was taken at the first start, so the steal
   figure for C covers about one extra minute.
8. **Tier G is not run.** Tier G (the `scaling` group of `tools.json`: thread counts and Metal) and tier S were not run;
   VM-TASK.md asks for A1 and C only.
9. **Several commits instead of one.** VM-TASK.md asks for one commit. The session's permission check refused the force
   push needed to squash the in-progress commits, so the branch has several commits. Every commit touches only
   `benchmark/rc2/linux/` (results, logs and the scripts that produced them); none changes source.
10. **Nothing else ran during the timed tiers.** The A1 runs and tier C ran one after another, with no other work of
    this session in parallel, apart from short `grep`/`tail` checks of the logs. The `others` field of each A1 row
    records any process above half a core. Only the driver's own `python3` appears there, at the first row.

## Tables

Sizes in bytes; MB/s with 1 MB = 1,000,000 bytes. "Marks" lists `threads` (CPU time above elapsed time by more than 3%
plus 0.05 s), `busy` and errors. A1: lzbench's fastest iteration, `-t1,2`, with the minimum iterations of
`run_lzbench.py`. C: `run_cli2.py`, whole processes, `wait4`.
Regenerate the tables with `python3 scripts/report.py benchmark/rc2/linux`.

### Tier A1: silesia.tar (211,948,032 bytes), lzbench in memory, one thread

| Round | Setting | lzbench name | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | CPU s / elapsed s | Marks |
|---|---|---|---:|---:|---:|---:|---:|---|
| main | `lz4` | lz4 1.11.0 | 100,297,674 | 2.113 | 531.8 | 2481.6 | 3.861 / 3.882 |  |
| main | `kolmo,16` | kolmo 1.2.0-rc2 -16 | 99,918,118 | 2.121 | 398.0 | 2123.2 | 4.31 / 4.326 |  |
| main | `zstd,3` | zstd 1.5.7 -3 | 66,132,559 | 3.205 | 163.0 | 959.4 | 6.859 / 6.905 |  |
| main | `kolmo,4` | kolmo 1.2.0-rc2 -4 | 58,245,973 | 3.639 | 169.2 | 783.6 | 6.977 / 7.026 |  |
| main | `memcpy` | memcpy | 211,948,032 | 1.000 | 9161.1 | 10178.7 | 3.617 / 3.622 |  |
| main | `lzav,1` | lzav 7.1 -1 | 83,153,734 | 2.549 | 399.6 | 1822.5 | 4.475 / 4.48 |  |
| main | `zxc,1` | zxc 1.0.0 -1 | 130,717,706 | 1.621 | 545.7 | 4509.2 | 3.829 / 3.848 |  |
| main | `zxc,3` | zxc 1.0.0 -3 | 99,289,070 | 2.135 | 165.2 | 3699.3 | 6.627 / 6.662 |  |
| main | `snappy` | snappy 1.3.1 | 101,435,293 | 2.089 | 503.1 | 1110.4 | 3.942 / 3.984 |  |
| main | `lzfse` | lzfse 2017-05-22 | 67,628,440 | 3.134 | 77.6 | 693.1 | 11.183 / 11.265 |  |
| main | `kolmo,0` | kolmo 1.2.0-rc2 -0 | 98,721,284 | 2.147 | 484.6 | 1954.9 | 3.901 / 3.911 |  |
| main | `zstd,1` | zstd 1.5.7 -1 | 73,191,342 | 2.896 | 388.3 | 1111.6 | 4.128 / 4.168 |  |
| main | `kolmo,17` | kolmo 1.2.0-rc2 -17 | 70,908,594 | 2.989 | 374.3 | 965.9 | 4.238 / 4.278 |  |
| main | `kolmo,1` | kolmo 1.2.0-rc2 -1 | 64,282,602 | 3.297 | 303.6 | 917.2 | 5.116 / 5.142 |  |
| main | `kolmo,3` | kolmo 1.2.0-rc2 -3 | 59,066,038 | 3.588 | 187.8 | 884.6 | 6.155 / 6.19 |  |
| main | `kolmo_ctl,4` | kolmo_ctl 1.2.0-rc2 -4 | 58,245,973 | 3.639 | 192.4 | 805.7 | 6.175 / 6.257 |  |
| main | `libdeflate,6` | libdeflate 1.26 -6 | 67,510,971 | 3.139 | 83.3 | 798.8 | 10.306 / 10.388 |  |
| main | `zlib-ng,6` | zlib-ng 2.3.3 -6 | 68,915,158 | 3.075 | 53.4 | 359.6 | 14.929 / 15.005 |  |
| main | `brotli,5` | brotli 1.2.0 -5 | 59,565,670 | 3.558 | 42.2 | 367.2 | 18.603 / 18.704 |  |
| main | `kolmo,8` | kolmo 1.2.0-rc2 -8 | 50,606,732 | 4.188 | 8.3 | 339.9 | 80.124 / 80.789 |  |
| main | `zstd,9` | zstd 1.5.7 -9 | 59,074,015 | 3.588 | 50.9 | 889.8 | 15.098 / 15.22 |  |
| main | `kolmo,5` | kolmo 1.2.0-rc2 -5 | 52,354,883 | 4.048 | 45.8 | 666.5 | 16.556 / 16.749 |  |
| main | `bsc0` | bsc 3.3.12 -m0 -e2 | 48,743,564 | 4.348 | 11.4 | 13.1 | 105.135 / 106.151 |  |
| main | `bsc1` | bsc 3.3.12 -m0 -e1 | 49,295,062 | 4.300 | 14.4 | 19.8 | 77.168 / 77.897 |  |
| main | `bzip3,5` | bzip3 1.5.4 -5 | 47,253,463 | 4.485 | 9.3 | 8.7 | 142.125 / 143.303 |  |
| main | `bzip3,10` | bzip3 1.5.4 -10 | 48,753,979 | 4.347 | 8.4 | 7.8 | 156.743 / 158.153 |  |
| main | `kolmo,7` | kolmo 1.2.0-rc2 -7 | 43,966,976 | 4.821 | 17.9 | 31.9 | 32.998 / 33.333 |  |
| main | `zstd,19` | zstd 1.5.7 -19 | 52,843,859 | 4.011 | 1.7 | 734.5 | 128.372 / 129.617 |  |
| main | `zstd,22` | zstd 1.5.7 -22 | 52,283,956 | 4.054 | 1.1 | 764.3 | 186.712 / 188.337 |  |
| main | `kolmo,9` | kolmo 1.2.0-rc2 -9 | 47,624,843 | 4.450 | 2.3 | 619.2 | 92.101 / 92.938 |  |
| main | `xz,9` | xz 5.8.4 -9 | 48,772,176 | 4.346 | 1.4 | 96.4 | 154.872 / 156.47 |  |
| main | `lzma,9` | lzma 26.03 -9 | 48,686,164 | 4.353 | 1.3 | 69.5 | 169.378 / 170.701 |  |
| main | `kolmo,10` | kolmo 1.2.0-rc2 -10 | 40,687,879 | 5.209 | 2.2 | 41.2 | 110.014 / 110.876 |  |
| main | `brotli,11` | brotli 1.2.0 -11 | 50,497,487 | 4.197 | 0.5 | 319.9 | 446.737 / 450.08 |  |
| main | `ppmd8,4` | ppmd8 26.03 -4 | 51,256,310 | 4.135 | 10.3 | 9.3 | 43.321 / 43.792 |  |
| main | `kanzi,7` | kanzi 2.6.0 -7 | 47,308,609 | 4.480 | 10.1 | 14.8 | 35.443 / 35.796 |  |
| main | `kolmo,11` | kolmo 1.2.0-rc2 -11 | 38,132,318 | 5.558 | 0.4 | 0.4 | 1127.7 / 1137.975 |  |
| main | `kolmo,12` | kolmo 1.2.0-rc2 -12 | 35,770,715 | 5.925 | 0.2 | 0.2 | 2298.396 / 2318.23 |  |
| main | `kolmo,13` | kolmo 1.2.0-rc2 -13 | 35,770,715 | 5.925 | 0.2 | 0.2 | 2386.534 / 2409.092 |  |
| main | `kanzi,9` | kanzi 2.6.0 -9 | 41,855,945 | 5.064 | 1.7 | 1.6 | 252.846 / 254.744 |  |
| main | `zpaq,5` | zpaq 7.15 -5 | 40,485,070 | 5.235 | 0.4 | 0.4 | 1149.631 / 1159.482 |  |
| end | `lz4` | lz4 1.11.0 | 100,297,674 | 2.113 | 490.4 | 2750.9 | 4.08 / 4.115 |  |
| end | `kolmo,16` | kolmo 1.2.0-rc2 -16 | 99,918,118 | 2.121 | 517.7 | 2168.6 | 3.889 / 3.921 |  |
| end | `zstd,3` | zstd 1.5.7 -3 | 66,132,559 | 3.205 | 219.2 | 963.3 | 5.512 / 5.512 |  |
| end | `kolmo,4` | kolmo 1.2.0-rc2 -4 | 58,245,973 | 3.639 | 176.7 | 802.2 | 6.273 / 6.307 |  |

### Tier A1 control library (kolmo_ctl): silesia.tar (211,948,032 bytes), lzbench in memory, one thread

| Round | Setting | lzbench name | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | CPU s / elapsed s | Marks |
|---|---|---|---:|---:|---:|---:|---:|---|
| main | `lz4` | lz4 1.11.0 | 100,297,674 | 2.113 | 508.9 | 2759.7 | 3.745 / 3.793 |  |
| main | `kolmo_ctl,16` | kolmo_ctl 1.2.0-rc2 -16 | 99,918,118 | 2.121 | 467.2 | 2155.1 | 3.944 / 3.975 |  |
| main | `zstd,3` | zstd 1.5.7 -3 | 66,132,559 | 3.205 | 167.5 | 974.1 | 6.582 / 6.601 |  |
| main | `kolmo_ctl,4` | kolmo_ctl 1.2.0-rc2 -4 | 58,245,973 | 3.639 | 186.1 | 826.3 | 6.206 / 6.24 |  |
| main | `kolmo_ctl,0` | kolmo_ctl 1.2.0-rc2 -0 | 98,721,284 | 2.147 | 475.7 | 1880.3 | 3.833 / 3.877 |  |
| main | `kolmo_ctl,17` | kolmo_ctl 1.2.0-rc2 -17 | 70,908,594 | 2.989 | 377.6 | 1002.0 | 4.248 / 4.289 |  |
| main | `kolmo_ctl,1` | kolmo_ctl 1.2.0-rc2 -1 | 64,282,602 | 3.297 | 308.8 | 903.6 | 4.668 / 4.721 |  |
| main | `kolmo_ctl,3` | kolmo_ctl 1.2.0-rc2 -3 | 59,066,038 | 3.588 | 197.2 | 878.9 | 5.862 / 5.906 |  |
| main | `kolmo_ctl,8` | kolmo_ctl 1.2.0-rc2 -8 | 50,606,732 | 4.188 | 7.5 | 286.3 | 89.755 / 90.384 |  |
| main | `kolmo_ctl,5` | kolmo_ctl 1.2.0-rc2 -5 | 52,354,883 | 4.048 | 43.5 | 585.1 | 17.612 / 17.722 |  |
| main | `kolmo_ctl,7` | kolmo_ctl 1.2.0-rc2 -7 | 43,966,976 | 4.821 | 15.2 | 29.3 | 36.254 / 36.464 |  |
| main | `kolmo_ctl,9` | kolmo_ctl 1.2.0-rc2 -9 | 47,624,843 | 4.450 | 2.1 | 608.5 | 102.966 / 103.676 |  |
| main | `kolmo_ctl,10` | kolmo_ctl 1.2.0-rc2 -10 | 40,687,879 | 5.209 | 2.3 | 46.5 | 105.436 / 105.951 |  |
| main | `kolmo_ctl,11` | kolmo_ctl 1.2.0-rc2 -11 | 38,132,318 | 5.558 | 0.4 | 0.4 | 1109.187 / 1118.657 |  |
| main | `kolmo_ctl,12` | kolmo_ctl 1.2.0-rc2 -12 | 35,770,715 | 5.925 | 0.2 | 0.2 | 2427.998 / 2447.461 |  |
| main | `kolmo_ctl,13` | kolmo_ctl 1.2.0-rc2 -13 | 35,770,715 | 5.925 | 0.2 | 0.2 | 2385.362 / 2403.815 |  |
| end | `lz4` | lz4 1.11.0 | 100,297,674 | 2.113 | 516.2 | 2716.0 | 3.791 / 3.817 |  |
| end | `kolmo_ctl,16` | kolmo_ctl 1.2.0-rc2 -16 | 99,918,118 | 2.121 | 503.2 | 2124.0 | 3.872 / 3.95 |  |
| end | `zstd,3` | zstd 1.5.7 -3 | 66,132,559 | 3.205 | 200.2 | 1028.4 | 6.363 / 6.384 |  |
| end | `kolmo_ctl,4` | kolmo_ctl 1.2.0-rc2 -4 | 58,245,973 | 3.639 | 200.2 | 815.9 | 5.975 / 6.027 |  |

### Tier C, fastq: whole processes, one thread, fastest of up to 3 rounds within 20 s

| File | Setting | Input (bytes) | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | Rounds | Peak RSS c / d (MiB) | Marks |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| SRR1770413.fastq | kolmo-fastq (prototype) | 63,177,790 | 26,270,480 | 2.405 | 210.37 | 404.58 | 3 | 163 / 96 |  |
| SRR1770413.fastq | kolmo -m balanced | 63,177,790 | 18,115,658 | 3.487 | 187.80 | 633.88 | 3 | 18 / 18 |  |
| SRR1770413.fastq | kolmo -m hc | 63,177,790 | 13,217,550 | 4.780 | 0.97 | 254.39 | 1 | 161 / 23 |  |
| SRR1770413.fastq | kolmo -m ratio | 63,177,790 | 10,327,642 | 6.117 | 18.89 | 31.95 | 3 | 329 / 279 |  |
| SRR1770413.fastq | kolmo -x | 63,177,790 | 10,706,117 | 5.901 | 0.17 | 0.17 | 1 | 5642 / 5581 |  |
| SRR1770413.fastq | zstd -3 | 63,177,790 | 18,088,207 | 3.493 | 169.64 | 693.66 | 3 | 40 / 18 |  |
| SRR1770413.fastq | zstd -19 | 63,177,790 | 12,698,907 | 4.975 | 0.84 | 559.27 | 1 | 151 / 18 |  |
| SRR1770413.fastq | xz -9 | 63,177,790 | 10,253,632 | 6.162 | 0.78 | 78.35 | 1 | 573 / 62 |  |
| SRR1770413.fastq | brotli -11 | 63,177,790 | 11,332,776 | 5.575 | 0.30 | 177.96 | 1 | 227 / 19 |  |
| SRR1770413.fastq | bsc -m0 -e2 | 63,177,790 | 11,507,230 | 5.490 | 15.37 | 21.33 | 3 | 144 / 124 |  |
| SRR1770413.fastq | bzip3 -b 16 | 63,177,790 | 11,834,348 | 5.339 | 8.74 | 7.70 | 2 | 97 / 98 |  |
| SRR1770413.fastq | krc (KRC1, cpu) | 63,177,790 | 58,846,920 | 1.074 | 41.29 | 134.30 | 3 | 178 / 118 |  |
| SRR1770413.fastq | kbc (KBC1) | 63,177,790 | 15,847,649 | 3.987 | 2.01 | 3.52 | 1 | 187 / 81 |  |
| SRR1770413.fastq | kbx -l 0 | 63,177,790 | 15,847,649 | 3.987 | 2.06 | 3.68 | 1 | 185 / 79 |  |
| SRR1770413.fastq | kbx -l 1 | 63,177,790 | 14,259,727 | 4.431 | 1.01 | 1.75 | 1 | 190 / 84 |  |
| SRR1770413.fastq | kbx -l 2 | 63,177,790 | 14,259,727 | 4.431 | 0.83 | 1.74 | 1 | 190 / 83 |  |
| SRR2584863.fastq | kolmo-fastq (prototype) | 36,637,534 | 20,206,308 | 1.813 | 172.09 | 341.13 | 3 | 107 / 66 |  |
| SRR2584863.fastq | kolmo -m balanced | 36,637,534 | 12,636,749 | 2.899 | 148.09 | 649.37 | 3 | 18 / 18 |  |
| SRR2584863.fastq | kolmo -m hc | 36,637,534 | 9,960,582 | 3.678 | 0.89 | 246.71 | 1 | 133 / 24 |  |
| SRR2584863.fastq | kolmo -m ratio | 36,637,534 | 8,031,260 | 4.562 | 16.15 | 26.60 | 3 | 212 / 186 |  |
| SRR2584863.fastq | kolmo -x | 36,637,534 | 7,650,237 | 4.789 | 0.15 | 0.15 | 1 | 5511 / 5476 |  |
| SRR2584863.fastq | zstd -3 | 36,637,534 | 12,648,856 | 2.897 | 124.95 | 547.29 | 3 | 39 / 18 |  |
| SRR2584863.fastq | zstd -19 | 36,637,534 | 9,749,221 | 3.758 | 0.70 | 432.12 | 1 | 128 / 18 |  |
| SRR2584863.fastq | xz -9 | 36,637,534 | 8,700,420 | 4.211 | 0.65 | 57.59 | 1 | 352 / 37 |  |
| SRR2584863.fastq | brotli -11 | 36,637,534 | 8,937,210 | 4.099 | 0.31 | 165.71 | 1 | 242 / 19 |  |
| SRR2584863.fastq | bsc -m0 -e2 | 36,637,534 | 8,266,486 | 4.432 | 14.26 | 17.59 | 3 | 128 / 130 |  |
| SRR2584863.fastq | bzip3 -b 16 | 36,637,534 | 8,363,722 | 4.381 | 7.66 | 6.71 | 2 | 99 / 99 |  |
| SRR2584863.fastq | krc (KRC1, cpu) | 36,637,534 | 35,695,019 | 1.026 | 41.29 | 133.17 | 3 | 105 / 71 |  |
| SRR2584863.fastq | kbc (KBC1) | 36,637,534 | 10,688,192 | 3.428 | 1.67 | 2.70 | 1 | 111 / 51 |  |
| SRR2584863.fastq | kbx -l 0 | 36,637,534 | 10,688,192 | 3.428 | 1.74 | 3.26 | 1 | 108 / 49 |  |
| SRR2584863.fastq | kbx -l 1 | 36,637,534 | 9,653,359 | 3.795 | 0.84 | 1.45 | 1 | 114 / 54 |  |
| SRR2584863.fastq | kbx -l 2 | 36,637,534 | 9,653,359 | 3.795 | 0.73 | 1.49 | 1 | 115 / 54 |  |
| SRR835775.fastq | kolmo-fastq (prototype) | 22,877,790 | 12,642,889 | 1.810 | 181.59 | 366.94 | 3 | 69 / 39 |  |
| SRR835775.fastq | kolmo -m balanced | 22,877,790 | 6,652,692 | 3.439 | 184.35 | 710.94 | 3 | 18 / 18 |  |
| SRR835775.fastq | kolmo -m hc | 22,877,790 | 5,650,910 | 4.049 | 1.04 | 293.19 | 1 | 115 / 23 |  |
| SRR835775.fastq | kolmo -m ratio | 22,877,790 | 4,879,646 | 4.688 | 20.02 | 30.16 | 3 | 112 / 95 |  |
| SRR835775.fastq | kolmo -x | 22,877,790 | 3,890,027 | 5.881 | 0.14 | 0.15 | 1 | 5425 / 5403 |  |
| SRR835775.fastq | zstd -3 | 22,877,790 | 6,667,689 | 3.431 | 149.31 | 686.27 | 3 | 29 / 18 |  |
| SRR835775.fastq | zstd -19 | 22,877,790 | 5,644,174 | 4.053 | 1.00 | 378.89 | 1 | 111 / 18 |  |
| SRR835775.fastq | xz -9 | 22,877,790 | 5,336,036 | 4.287 | 0.90 | 63.69 | 1 | 236 / 24 |  |
| SRR835775.fastq | brotli -11 | 22,877,790 | 5,283,259 | 4.330 | 0.36 | 170.63 | 1 | 208 / 19 |  |
| SRR835775.fastq | bsc -m0 -e2 | 22,877,790 | 4,804,486 | 4.762 | 16.40 | 19.49 | 3 | 94 / 91 |  |
| SRR835775.fastq | bzip3 -b 16 | 22,877,790 | 4,700,964 | 4.867 | 10.57 | 9.75 | 3 | 100 / 100 |  |
| SRR835775.fastq | krc (KRC1, cpu) | 22,877,790 | 22,296,390 | 1.026 | 44.42 | 110.62 | 3 | 66 / 45 |  |
| SRR835775.fastq | kbc (KBC1) | 22,877,790 | 7,263,136 | 3.150 | 1.70 | 2.91 | 1 | 71 / 35 |  |
| SRR835775.fastq | kbx -l 0 | 22,877,790 | 7,263,136 | 3.150 | 1.84 | 3.40 | 2 | 69 / 32 |  |
| SRR835775.fastq | kbx -l 1 | 22,877,790 | 6,369,349 | 3.592 | 0.94 | 1.63 | 1 | 75 / 38 |  |
| SRR835775.fastq | kbx -l 2 | 22,877,790 | 6,369,349 | 3.592 | 0.77 | 1.62 | 1 | 75 / 38 |  |

### Tier C, video: whole processes, one thread, fastest of up to 3 rounds within 20 s

| File | Setting | Input (bytes) | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | Rounds | Peak RSS c / d (MiB) | Marks |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| akiyo_cif.y4m | kvid -m balanced | 45,621,044 | 8,405,112 | 5.428 | 40.32 | 218.17 | 3 | 144 / 98 |  |
| akiyo_cif.y4m | kvid -m hc | 45,621,044 | 6,985,276 | 6.531 | 1.47 | 142.29 | 1 | 224 / 97 |  |
| akiyo_cif.y4m | kvid -m ratio | 45,621,044 | 6,259,122 | 7.289 | 0.65 | 31.89 | 1 | 553 / 130 |  |
| akiyo_cif.y4m | kvid -m x | 45,621,044 | 6,042,133 | 7.550 | 0.17 | 0.33 | 1 | 4661 / 4573 |  |
| akiyo_cif.y4m | kolmo -m balanced | 45,621,044 | 18,118,923 | 2.518 | 207.63 | 153.33 | 3 | 49 / 21 |  |
| akiyo_cif.y4m | kolmo -m hc | 45,621,044 | 12,953,821 | 3.522 | 1.37 | 181.78 | 1 | 161 / 40 |  |
| akiyo_cif.y4m | kolmo -m ratio | 45,621,044 | 11,232,641 | 4.061 | 0.94 | 45.26 | 1 | 557 / 101 |  |
| akiyo_cif.y4m | kolmo -x | 45,621,044 | 8,287,024 | 5.505 | 0.08 | 0.14 | 1 | 5706 / 5581 |  |
| akiyo_cif.y4m | zstd -3 | 45,621,044 | 15,198,562 | 3.002 | 209.45 | 817.53 | 3 | 40 / 18 |  |
| akiyo_cif.y4m | zstd -19 | 45,621,044 | 11,759,434 | 3.880 | 1.31 | 421.91 | 1 | 136 / 18 |  |
| akiyo_cif.y4m | xz -9 | 45,621,044 | 9,505,636 | 4.799 | 1.48 | 55.65 | 1 | 458 / 46 |  |
| akiyo_cif.y4m | brotli -11 | 45,621,044 | 10,561,411 | 4.320 | 0.42 | 198.05 | 1 | 207 / 19 |  |
| akiyo_cif.y4m | bsc -m0 -e2 | 45,621,044 | 9,147,032 | 4.988 | 19.28 | 19.42 | 3 | 100 / 94 |  |
| akiyo_cif.y4m | bzip3 -b 16 | 45,621,044 | 8,947,930 | 5.099 | 12.61 | 10.53 | 3 | 100 / 100 |  |
| akiyo_cif.y4m | krc (KRC1, cpu) | 45,621,044 | 44,557,956 | 1.024 | 46.22 | 128.63 | 3 | 131 / 88 |  |
| akiyo_cif.y4m | kbc (KBC1) | 45,621,044 | 20,692,462 | 2.205 | 1.24 | 2.06 | 1 | 136 / 69 |  |
| akiyo_cif.y4m | kbx -l 0 | 45,621,044 | 20,692,462 | 2.205 | 1.38 | 2.41 | 1 | 134 / 67 |  |
| akiyo_cif.y4m | kbx -l 1 | 45,621,044 | 19,330,097 | 2.360 | 0.71 | 1.27 | 1 | 140 / 72 |  |
| akiyo_cif.y4m | kbx -l 2 | 45,621,044 | 19,330,097 | 2.360 | 0.62 | 1.40 | 1 | 140 / 72 |  |
| foreman_cif.y4m | kvid -m balanced | 45,621,044 | 22,450,190 | 2.032 | 24.02 | 86.03 | 3 | 159 / 112 |  |
| foreman_cif.y4m | kvid -m hc | 45,621,044 | 21,153,482 | 2.157 | 0.77 | 78.24 | 1 | 244 / 110 |  |
| foreman_cif.y4m | kvid -m ratio | 45,621,044 | 20,664,184 | 2.208 | 0.52 | 26.24 | 1 | 566 / 109 |  |
| foreman_cif.y4m | kvid -m x | 45,621,044 | 18,753,337 | 2.433 | 0.12 | 0.25 | 1 | 4673 / 4585 |  |
| foreman_cif.y4m | kolmo -m balanced | 45,621,044 | 23,156,754 | 1.970 | 117.28 | 143.94 | 3 | 50 / 21 |  |
| foreman_cif.y4m | kolmo -m hc | 45,621,044 | 22,973,573 | 1.986 | 0.83 | 100.15 | 1 | 176 / 71 |  |
| foreman_cif.y4m | kolmo -m ratio | 45,621,044 | 20,987,517 | 2.174 | 0.51 | 14.03 | 1 | 568 / 242 |  |
| foreman_cif.y4m | kolmo -x | 45,621,044 | 18,357,467 | 2.485 | 0.14 | 0.20 | 1 | 4658 / 4585 |  |
| foreman_cif.y4m | zstd -3 | 45,621,044 | 33,660,184 | 1.355 | 112.68 | 556.04 | 3 | 49 / 18 |  |
| foreman_cif.y4m | zstd -19 | 45,621,044 | 26,876,415 | 1.697 | 1.03 | 191.36 | 1 | 146 / 18 |  |
| foreman_cif.y4m | xz -9 | 45,621,044 | 24,777,860 | 1.841 | 0.79 | 25.06 | 1 | 458 / 46 |  |
| foreman_cif.y4m | brotli -11 | 45,621,044 | 25,664,666 | 1.778 | 0.32 | 87.70 | 1 | 265 / 19 |  |
| foreman_cif.y4m | bsc -m0 -e2 | 45,621,044 | 21,318,940 | 2.140 | 9.62 | 9.34 | 3 | 129 / 130 |  |
| foreman_cif.y4m | bzip3 -b 16 | 45,621,044 | 20,754,868 | 2.198 | 7.45 | 5.76 | 2 | 100 / 100 |  |
| foreman_cif.y4m | krc (KRC1, cpu) | 45,621,044 | 44,647,763 | 1.022 | 46.35 | 135.05 | 3 | 131 / 88 |  |
| foreman_cif.y4m | kbc (KBC1) | 45,621,044 | 24,365,615 | 1.872 | 1.12 | 1.85 | 1 | 136 / 73 |  |
| foreman_cif.y4m | kbx -l 0 | 45,621,044 | 24,365,615 | 1.872 | 1.36 | 2.44 | 1 | 134 / 70 |  |
| foreman_cif.y4m | kbx -l 1 | 45,621,044 | 23,026,776 | 1.981 | 0.81 | 1.41 | 1 | 140 / 75 |  |
| foreman_cif.y4m | kbx -l 2 | 45,621,044 | 23,026,776 | 1.981 | 0.61 | 1.38 | 1 | 140 / 75 |  |
| mobile_cif.y4m | kvid -m balanced | 45,621,054 | 29,995,313 | 1.521 | 23.78 | 91.53 | 3 | 167 / 119 |  |
| mobile_cif.y4m | kvid -m hc | 45,621,054 | 29,553,642 | 1.544 | 1.10 | 78.97 | 1 | 255 / 118 |  |
| mobile_cif.y4m | kvid -m ratio | 45,621,054 | 28,634,082 | 1.593 | 0.66 | 19.44 | 1 | 574 / 117 |  |
| mobile_cif.y4m | kvid -m x | 45,621,054 | 26,672,675 | 1.710 | 0.10 | 0.19 | 1 | 4681 / 4593 |  |
| mobile_cif.y4m | kolmo -m balanced | 45,621,054 | 32,418,001 | 1.407 | 72.14 | 141.29 | 3 | 50 / 22 |  |
| mobile_cif.y4m | kolmo -m hc | 45,621,054 | 32,352,672 | 1.410 | 1.19 | 86.18 | 1 | 191 / 74 |  |
| mobile_cif.y4m | kolmo -m ratio | 45,621,054 | 30,246,791 | 1.508 | 0.69 | 11.11 | 1 | 581 / 251 |  |
| mobile_cif.y4m | kolmo -x | 45,621,054 | 27,062,151 | 1.686 | 0.13 | 0.15 | 1 | 4672 / 4593 |  |
| mobile_cif.y4m | zstd -3 | 45,621,054 | 40,938,429 | 1.114 | 127.43 | 578.71 | 3 | 51 / 18 |  |
| mobile_cif.y4m | zstd -19 | 45,621,054 | 35,882,711 | 1.271 | 1.44 | 218.34 | 1 | 153 / 18 |  |
| mobile_cif.y4m | xz -9 | 45,621,054 | 31,889,240 | 1.431 | 1.03 | 18.81 | 1 | 458 / 45 |  |
| mobile_cif.y4m | brotli -11 | 45,621,054 | 33,435,161 | 1.364 | 0.29 | 68.00 | 1 | 242 / 19 |  |
| mobile_cif.y4m | bsc -m0 -e2 | 45,621,054 | 30,593,372 | 1.491 | 7.59 | 6.61 | 2 | 129 / 130 |  |
| mobile_cif.y4m | bzip3 -b 16 | 45,621,054 | 29,968,304 | 1.522 | 6.32 | 4.95 | 2 | 100 / 100 |  |
| mobile_cif.y4m | krc (KRC1, cpu) | 45,621,054 | 45,680,952 | 0.999 | 47.99 | 139.70 | 3 | 132 / 89 |  |
| mobile_cif.y4m | kbc (KBC1) | 45,621,054 | 32,951,541 | 1.384 | 0.99 | 1.61 | 1 | 136 / 81 |  |
| mobile_cif.y4m | kbx -l 0 | 45,621,054 | 32,951,541 | 1.384 | 1.15 | 2.08 | 1 | 134 / 79 |  |
| mobile_cif.y4m | kbx -l 1 | 45,621,054 | 31,866,907 | 1.432 | 0.81 | 1.44 | 1 | 140 / 84 |  |
| mobile_cif.y4m | kbx -l 2 | 45,621,054 | 31,866,907 | 1.432 | 0.62 | 1.46 | 1 | 140 / 84 |  |

### Tier C, research: whole processes, one thread, fastest of up to 3 rounds within 20 s

| File | Setting | Input (bytes) | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | Rounds | Peak RSS c / d (MiB) | Marks |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| silesia.tar | krc (KRC1, cpu) | 211,948,032 | 191,504,283 | 1.107 | 43.52 | 127.89 | 3 | 589 / 387 |  |
| silesia.tar | kbc (KBC1) | 211,948,032 | 77,570,988 | 2.732 | 1.46 | 2.45 | 1 | 612 / 282 |  |
| silesia.tar | kbx -l 0 | 211,948,032 | 77,570,988 | 2.732 | 1.62 | 2.88 | 1 | 610 / 280 |  |
| silesia.tar | kbx -l 1 | 211,948,032 | 63,612,060 | 3.332 | 0.91 | 1.61 | 1 | 616 / 272 |  |
| silesia.tar | kbx -l 2 | 211,948,032 | 63,612,060 | 3.332 | 0.76 | 1.67 | 1 | 616 / 272 |  |
| silesia.tar | kolmo -m balanced | 211,948,032 | 58,245,973 | 3.639 | 190.25 | 722.83 | 3 | 56 / 21 |  |
| silesia.tar | kolmo -m hc | 211,948,032 | 47,624,843 | 4.450 | 2.41 | 328.58 | 1 | 516 / 58 |  |
| silesia.tar | kolmo -m ratio | 211,948,032 | 40,687,879 | 5.209 | 2.75 | 42.68 | 1 | 765 / 268 |  |
| silesia.tar | zstd -3 | 211,948,032 | 66,557,030 | 3.184 | 193.17 | 792.32 | 3 | 46 / 18 | threads |
| silesia.tar | zstd -19 | 211,948,032 | 52,999,040 | 3.999 | 1.47 | 434.74 | 1 | 227 / 18 | threads |
| silesia.tar | xz -9 | 211,948,032 | 48,752,732 | 4.347 | 1.42 | 62.72 | 1 | 675 / 66 |  |
| silesia.tar | brotli -11 | 211,948,032 | 49,766,867 | 4.259 | 0.42 | 235.30 | 1 | 287 / 20 |  |
| silesia.tar | bsc -m0 -e2 | 211,948,032 | 47,402,890 | 4.471 | 14.92 | 17.44 | 1 | 158 / 152 |  |
| silesia.tar | bzip3 -b 16 | 211,948,032 | 47,253,491 | 4.485 | 8.64 | 8.37 | 1 | 100 / 100 |  |
| dickens | krc (KRC1, cpu) | 10,192,446 | 9,933,704 | 1.026 | 45.52 | 136.70 | 3 | 31 / 21 |  |
| dickens | kbc (KBC1) | 10,192,446 | 3,767,805 | 2.705 | 1.42 | 2.35 | 2 | 35 / 19 |  |
| dickens | kbx -l 0 | 10,192,446 | 3,767,805 | 2.705 | 1.66 | 2.98 | 3 | 33 / 18 |  |
| dickens | kbx -l 1 | 10,192,446 | 3,220,456 | 3.165 | 0.75 | 1.30 | 1 | 39 / 22 |  |
| dickens | kbx -l 2 | 10,192,446 | 3,220,456 | 3.165 | 0.62 | 1.28 | 1 | 39 / 23 |  |
| dickens | kolmo -m balanced | 10,192,446 | 3,721,196 | 2.739 | 104.73 | 605.69 | 3 | 18 / 18 |  |
| dickens | kolmo -m hc | 10,192,446 | 2,728,251 | 3.736 | 1.89 | 227.12 | 3 | 178 / 23 |  |
| dickens | kolmo -m ratio | 10,192,446 | 2,274,350 | 4.481 | 13.28 | 25.41 | 3 | 93 / 54 |  |
| dickens | zstd -3 | 10,192,446 | 3,669,252 | 2.778 | 105.22 | 554.11 | 3 | 18 / 18 |  |
| dickens | zstd -19 | 10,192,446 | 2,850,281 | 3.576 | 1.06 | 370.93 | 3 | 97 / 18 |  |
| dickens | xz -9 | 10,192,446 | 2,830,604 | 3.601 | 0.94 | 64.15 | 2 | 142 / 18 |  |
| dickens | brotli -11 | 10,192,446 | 2,827,777 | 3.604 | 0.43 | 241.34 | 1 | 126 / 18 |  |
| dickens | bsc -m0 -e2 | 10,192,446 | 2,259,134 | 4.512 | 14.02 | 18.05 | 3 | 52 / 53 |  |
| dickens | bzip3 -b 16 | 10,192,446 | 2,233,954 | 4.563 | 8.02 | 6.62 | 3 | 88 / 88 |  |
| mozilla | krc (KRC1, cpu) | 51,220,480 | 42,483,819 | 1.206 | 47.73 | 156.37 | 3 | 140 / 91 |  |
| mozilla | kbc (KBC1) | 51,220,480 | 22,429,244 | 2.284 | 1.38 | 2.30 | 1 | 152 / 76 |  |
| mozilla | kbx -l 0 | 51,220,480 | 22,429,244 | 2.284 | 1.57 | 2.68 | 1 | 150 / 74 |  |
| mozilla | kbx -l 1 | 51,220,480 | 19,237,000 | 2.663 | 1.00 | 1.76 | 1 | 156 / 77 |  |
| mozilla | kbx -l 2 | 51,220,480 | 19,237,000 | 2.663 | 0.76 | 1.72 | 1 | 156 / 77 |  |
| mozilla | kolmo -m balanced | 51,220,480 | 18,318,543 | 2.796 | 142.08 | 660.38 | 3 | 26 / 18 |  |
| mozilla | kolmo -m hc | 51,220,480 | 15,419,842 | 3.322 | 2.46 | 309.92 | 1 | 153 / 18 |  |
| mozilla | kolmo -m ratio | 51,220,480 | 13,835,767 | 3.702 | 1.96 | 53.20 | 1 | 217 / 33 |  |
| mozilla | zstd -3 | 51,220,480 | 18,504,727 | 2.768 | 188.26 | 723.99 | 3 | 42 / 18 |  |
| mozilla | zstd -19 | 51,220,480 | 15,113,896 | 3.389 | 1.81 | 435.56 | 1 | 144 / 18 |  |
| mozilla | xz -9 | 51,220,480 | 13,374,160 | 3.830 | 1.51 | 46.63 | 1 | 506 / 51 |  |
| mozilla | brotli -11 | 51,220,480 | 13,872,265 | 3.692 | 0.33 | 174.58 | 1 | 238 / 20 |  |
| mozilla | bsc -m0 -e2 | 51,220,480 | 15,977,814 | 3.206 | 12.82 | 13.29 | 3 | 142 / 124 |  |
| mozilla | bzip3 -b 16 | 51,220,480 | 15,881,570 | 3.225 | 7.76 | 7.86 | 2 | 99 / 100 |  |
| osdb | krc (KRC1, cpu) | 10,085,684 | 10,144,807 | 0.994 | 49.46 | 144.29 | 3 | 31 / 21 |  |
| osdb | kbc (KBC1) | 10,085,684 | 5,049,105 | 1.998 | 1.02 | 1.61 | 2 | 35 / 20 |  |
| osdb | kbx -l 0 | 10,085,684 | 5,049,105 | 1.998 | 1.24 | 2.17 | 2 | 33 / 18 |  |
| osdb | kbx -l 1 | 10,085,684 | 4,025,540 | 2.505 | 0.70 | 1.25 | 1 | 39 / 23 |  |
| osdb | kbx -l 2 | 10,085,684 | 4,025,540 | 2.505 | 0.55 | 1.18 | 1 | 39 / 23 |  |
| osdb | kolmo -m balanced | 10,085,684 | 3,533,740 | 2.854 | 147.30 | 714.30 | 3 | 21 / 18 |  |
| osdb | kolmo -m hc | 10,085,684 | 3,100,225 | 3.253 | 1.71 | 276.57 | 3 | 100 / 18 |  |
| osdb | kolmo -m ratio | 10,085,684 | 2,276,454 | 4.430 | 1.28 | 24.26 | 3 | 135 / 54 |  |
| osdb | zstd -3 | 10,085,684 | 3,506,596 | 2.876 | 103.38 | 605.59 | 3 | 18 / 18 |  |
| osdb | zstd -19 | 10,085,684 | 3,098,473 | 3.255 | 1.62 | 382.47 | 3 | 97 / 18 |  |
| osdb | xz -9 | 10,085,684 | 2,849,908 | 3.539 | 1.41 | 51.26 | 3 | 153 / 18 |  |
| osdb | brotli -11 | 10,085,684 | 2,816,278 | 3.581 | 0.43 | 173.38 | 1 | 132 / 18 |  |
| osdb | bsc -m0 -e2 | 10,085,684 | 2,224,780 | 4.533 | 13.01 | 16.87 | 3 | 51 / 53 |  |
| osdb | bzip3 -b 16 | 10,085,684 | 2,261,872 | 4.459 | 6.89 | 5.51 | 3 | 87 / 87 |  |
| nci | krc (KRC1, cpu) | 33,553,445 | 29,422,219 | 1.140 | 43.67 | 133.57 | 3 | 94 / 62 |  |
| nci | kbc (KBC1) | 33,553,445 | 5,871,637 | 5.714 | 2.15 | 3.81 | 1 | 102 / 43 |  |
| nci | kbx -l 0 | 33,553,445 | 5,871,637 | 5.714 | 2.14 | 3.66 | 1 | 100 / 41 |  |
| nci | kbx -l 1 | 33,553,445 | 3,347,803 | 10.023 | 1.24 | 2.37 | 1 | 106 / 45 |  |
| nci | kbx -l 2 | 33,553,445 | 3,347,803 | 10.023 | 1.07 | 2.38 | 1 | 106 / 45 |  |
| nci | kolmo -m balanced | 33,553,445 | 2,027,737 | 16.547 | 314.65 | 681.79 | 3 | 24 / 24 |  |
| nci | kolmo -m hc | 33,553,445 | 1,645,823 | 20.387 | 13.52 | 253.88 | 3 | 213 / 45 |  |
| nci | kolmo -m ratio | 33,553,445 | 1,225,386 | 27.382 | 6.79 | 58.57 | 3 | 322 / 144 |  |
| nci | zstd -3 | 33,553,445 | 2,844,574 | 11.796 | 393.18 | 1032.38 | 3 | 37 / 24 |  |
| nci | zstd -19 | 33,553,445 | 1,688,918 | 19.867 | 1.95 | 938.52 | 2 | 118 / 24 |  |
| nci | xz -9 | 33,553,445 | 1,738,884 | 19.296 | 2.63 | 217.15 | 2 | 308 / 34 |  |
| nci | brotli -11 | 33,553,445 | 1,519,768 | 22.078 | 0.50 | 531.09 | 1 | 198 / 24 |  |
| nci | bsc -m0 -e2 | 33,553,445 | 1,266,320 | 26.497 | 28.04 | 50.78 | 3 | 115 / 113 |  |
| nci | bzip3 -b 16 | 33,553,445 | 1,425,956 | 23.530 | 18.45 | 15.67 | 3 | 99 / 100 |  |
| x-ray | krc (KRC1, cpu) | 8,474,240 | 6,652,988 | 1.274 | 47.52 | 136.25 | 3 | 24 / 24 |  |
| x-ray | kbc (KBC1) | 8,474,240 | 5,005,901 | 1.693 | 1.00 | 1.63 | 2 | 30 / 24 |  |
| x-ray | kbx -l 0 | 8,474,240 | 5,005,901 | 1.693 | 1.27 | 2.05 | 2 | 28 / 24 |  |
| x-ray | kbx -l 1 | 8,474,240 | 4,738,983 | 1.788 | 0.78 | 1.27 | 2 | 34 / 24 |  |
| x-ray | kbx -l 2 | 8,474,240 | 4,738,983 | 1.788 | 0.54 | 1.26 | 1 | 34 / 24 |  |
| x-ray | kolmo -m balanced | 8,474,240 | 3,657,342 | 2.317 | 196.79 | 456.68 | 3 | 24 / 24 |  |
| x-ray | kolmo -m hc | 8,474,240 | 3,649,636 | 2.322 | 8.40 | 248.20 | 3 | 116 / 31 |  |
| x-ray | kolmo -m ratio | 8,474,240 | 3,616,667 | 2.343 | 2.97 | 27.96 | 3 | 137 / 31 |  |
| x-ray | zstd -3 | 8,474,240 | 6,086,279 | 1.392 | 93.67 | 294.82 | 3 | 24 / 24 |  |
| x-ray | zstd -19 | 8,474,240 | 5,155,753 | 1.644 | 1.67 | 192.80 | 3 | 98 / 24 |  |
| x-ray | xz -9 | 8,474,240 | 4,489,868 | 1.887 | 1.41 | 26.00 | 3 | 140 / 24 |  |
| x-ray | brotli -11 | 8,474,240 | 4,682,753 | 1.810 | 0.32 | 82.78 | 1 | 135 / 24 |  |
| x-ray | bsc -m0 -e2 | 8,474,240 | 3,756,584 | 2.256 | 8.79 | 9.52 | 3 | 44 / 45 |  |
| x-ray | bzip3 -b 16 | 8,474,240 | 3,657,090 | 2.317 | 6.42 | 6.48 | 3 | 84 / 84 |  |

### Steal time

- A1: /proc/stat: 0.23% of all CPU time (89 s steal over 9442 s wall x 4 CPUs); vmstat: 9437 samples, st mean 0.14%, max 7%, samples with st>0: 1285
- A1ctl: /proc/stat: 0.21% of all CPU time (55 s steal over 6394 s wall x 4 CPUs); vmstat: 6388 samples, st mean 0.11%, max 2%, samples with st>0: 674
- C: /proc/stat: 0.23% of all CPU time (121 s steal over 13377 s wall x 4 CPUs); vmstat: 13367 samples, st mean 0.12%, max 3%, samples with st>0: 1556


The steal figures from `/proc/stat` are steal jiffies divided by all CPU jiffies of the 4 vCPUs during the tier. With one
busy vCPU, steal on that core is about 4 times the figure shown, so about 1%. The vmstat `st` column is the
machine-wide percentage per second.
