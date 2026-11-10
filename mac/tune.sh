#!/bin/bash
# Overnight genetic tuning on this Mac. Keeps the Mac awake, searches kolmo's settings on the benchmark corpus and writes
# the best settings to tune/best.json (progress in tune/log.tsv). Default: 8 hours on fast, hc and max.
#   bash mac/tune.sh            # 8 hours
#   bash mac/tune.sh 2 fast,hc  # 2 hours, chosen modes
set -euo pipefail
cd "$(dirname "$0")/.."
hours="${1:-8}"; modes="${2:-fast,hc,max}"
make -j"$(sysctl -n hw.ncpu)" >/dev/null
mkdir -p tune/corpus
for f in owid-co2-data.csv owid-energy-data.csv loghub_all.log sao x-ray; do [ -f "data/$f" ] && ln -sf "../../data/$f" "tune/corpus/$f"; done
[ -f data/enwik8 ] && head -c 20000000 data/enwik8 > tune/corpus/enwik8_20m
caffeinate -dimsu python3 tune/ga.py --corpus tune/corpus --modes "$modes" --hours "$hours" 2>&1 | tee -a tune/run.log
echo; python3 tune/ga.py --show
