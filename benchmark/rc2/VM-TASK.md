Task for a cloud session (Linux x86-64), repository adnaneben/kolmo, branch claude/release-rc2-20261115.
Do not merge, tag, publish, open a PR, or change main or any other branch. Push results only to a new branch
claude/rc2-linux-results-20261115 (one commit, results and logs only, no source change).

1. Record the machine: `lscpu`, `nproc`, `free -g`, `uname -a`, compiler versions. Note whether the VM is shared
   (steal time in /proc/stat) and keep `vmstat 1` running into a log during every timed tier.
2. Build and test: `make ARCH= all lib`, `make ARCH= check`, `make check-sanitizers`, `make check-rc`,
   `make ARCH= experimental-check` (no Metal on Linux; krc-auto is the CPU build). Record pass or fail.
3. Identity: build a control with `CFLAGS="-O3 -DKOLMO_FORCE_GENERIC_INT_FWD"`; compress the Silesia files with
   both at `-m balanced -T1`, `-m hc -T1`, `-m fast --typed-huf -T1`; the frames must be identical.
4. lzbench 2.4.1 at commit 9be0e25: add Kolmo with `benchmark/lzbench/setup.sh` but build both sides without
   -march=native (same flags as the Mac run: -O3, generic). Download Silesia from
   https://sun.aei.polsl.pl/~sdeor/corpus/silesia.zip, build silesia.tar of the 12 files in alphabetical order
   (211,948,032 bytes) and run tier A1 exactly as `benchmark/rc1/run_lzbench.py --tier A1` (one thread, the same
   41 settings, the same minimum times and iterations). Then the same with the control library (`kolmo_ctl`).
5. Tier C with `benchmark/rc2/run_cli2.py` and `make_tools.py`: FASTQ (first 100,000 reads of SRR1770413,
   SRR2584863, SRR835775 from ENA, as `benchmark/rc2/fetch_fastq.py` does), video (akiyo_cif, foreman_cif, mobile_cif
   from https://media.xiph.org/video/derf/y4m/), and the research codecs on silesia.tar, dickens, mozilla, osdb,
   nci, x-ray. Install rivals from the distribution (zstd, xz-utils, brotli, bzip3 if packaged, libbsc from
   source) and record their versions.
6. Report: one markdown table per tier with sizes and MB/s, the rows marked `threads`, and every failure.
