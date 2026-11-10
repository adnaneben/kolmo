#!/bin/bash
# Full benchmark on this Mac: kolmo against LZ4, zstd, brotli and xz, single core and all cores.
# Keeps the Mac awake while it runs; results go to results/ (one TSV per run plus a log).
#   bash mac/bench.sh            # everything (about 1-2 hours, slow reference codecs included)
#   bash mac/bench.sh quick      # small datasets, no slow reference codecs (about 10 minutes)
set -euo pipefail
cd "$(dirname "$0")/.."
make -j"$(sysctl -n hw.ncpu)" >/dev/null
mkdir -p results
stamp=$(date +%Y%m%d-%H%M)
chip=$(sysctl -n machdep.cpu.brand_string 2>/dev/null | tr ' ' '_')
out="results/mac_${chip}_${stamp}.tsv"; log="results/mac_${chip}_${stamp}.log"
small="data/owid-co2-data.csv data/owid-energy-data.csv data/loghub_all.log data/sao data/x-ray"
big="data/enwik8 data/silesia.tar"
{
  echo "machine: $(sysctl -n machdep.cpu.brand_string) | cores $(sysctl -n hw.ncpu) | memory $(( $(sysctl -n hw.memsize) / 1073741824 )) GB | macOS $(sw_vers -productVersion)"
  ./build/kolmotest | tail -1
  if [ "${1:-}" = "quick" ]; then
      caffeinate -dimsu ./build/kolmobench -T "$(sysctl -n hw.ncpu)" -x -o "$out" $small
  else
      caffeinate -dimsu ./build/kolmobench -T "$(sysctl -n hw.ncpu)" -X -o "$out" $small
      for f in $big; do [ -f "$f" ] && caffeinate -dimsu ./build/kolmobench -T "$(sysctl -n hw.ncpu)" -X -o "$out" "$f"; done
  fi
} 2>&1 | tee "$log"
echo
echo "Results: $out"
