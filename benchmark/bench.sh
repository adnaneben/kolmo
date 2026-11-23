#!/usr/bin/env bash
# Reproducible benchmark for kolmo.
#
# Runs kolmo's modes in memory (build/kolmobench). Every output is decompressed and compared
# byte-for-byte; speeds are best-of-repeated runs. The vendored rivals (LZ4, zstd, brotli, xz)
# were removed in 0.0.1: measure them with their own command-line tools.
#
# Usage:
#   make -j                      # build build/kolmobench (and the rest)
#   benchmark/bench.sh FILE...   # benchmark each FILE; -x / -X are accepted for compatibility
#
# Suggested public corpora (download yourself; not bundled):
#   - Silesia corpus            http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia
#   - enwik8 (text)             http://mattmahoney.net/dc/enwik8.zip
#   - Our World in Data CSVs    https://github.com/owid/co2-data , owid/energy-data
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
flags=""
case "${1:-}" in -x|-X) flags="$1"; shift;; esac
[ "$#" -ge 1 ] || { echo "usage: benchmark/bench.sh [-x|-X] FILE..."; exit 2; }
[ -x "$root/build/kolmobench" ] || make -C "$root" -j"$(nproc 2>/dev/null || echo 4)"
for f in "$@"; do
  echo "== $f ($(wc -c <"$f") bytes) =="
  "$root/build/kolmobench" $flags "$f"
  echo
done
