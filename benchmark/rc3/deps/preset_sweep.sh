#!/bin/sh
# Kolmo -m ratio with LZMA2 match-finder/parser settings of other presets (libprof's LIBPROF_LZMA_PRESET), 1 thread.
# usage: sh benchmark/rc3/deps/preset_sweep.sh DATA_DIR OUT_PREFIX PRESET...
D=$1; P=$2; shift 2
for p in "$@"; do
  LIBPROF_LZMA_PRESET=$p python3 benchmark/rc3/deps/profile_modes.py build/kolmo-libprof "$D" "${P}_$p.tsv" ratio
done
