#!/bin/sh
# Whole-file comparison for the bwt lane. Usage: bench.sh THREADS OLD_KOLMO NEW_KOLMO BSCDRV FILE...
# Prints: file tool bytes compress_s decompress_s ok. Every tool runs with the same thread count (1 or all), as
# benchmark/rc2/PROTOCOL.md requires; one run each (the VM is noisy: see RESULTS.md for repeats).
T=$1 OLD=$2 NEW=$3 BSC=$4; shift 4
W=${BENCH_WORK:-/tmp/bwt-bench}; mkdir -p "$W"
now() { date +%s.%N; }
run() { # name compress_cmd decompress_cmd packed
  t0=$(now); sh -c "$2" >/dev/null 2>&1; t1=$(now); sh -c "$3" >/dev/null 2>&1; t2=$(now)
  ok=$(cmp -s "$W/rt" "$f" && echo ok || echo BAD)
  printf "%s\t%s\t%d\t%.2f\t%.2f\t%s\n" "$(basename "$f")" "$1" "$(stat -c %s "$4")" "$(echo "$t1 - $t0" | bc)" "$(echo "$t2 - $t1" | bc)" "$ok"
}
for f in "$@"; do
  run "kolmo-rc3base-ratio" "$OLD -m ratio -T $T '$f' $W/a.kol" "$OLD -d -T $T $W/a.kol $W/rt" "$W/a.kol"
  run "kolmo-bwt-ratio" "$NEW -m ratio -T $T '$f' $W/b.kol" "$NEW -d -T $T $W/b.kol $W/rt" "$W/b.kol"
  run "xz-9" "xz -9 -k -c -T$T '$f' > $W/c.xz" "xz -d -c -T$T $W/c.xz > $W/rt" "$W/c.xz"
  # bsc (libbsc 3.4.0 with bsc's defaults: 25 MiB blocks, LZP 15/72, static QLFC; OpenMP when T > 1). The driver
  # times itself: its line is printed as is.
  "$BSC" "$T" "$f" | awk -v f="$(basename "$f")" '{print f "\tbsc\t" $5 "\t" $7 "\t" $10 "\t" ($12=="ok=1"?"ok":"BAD")}'
done
