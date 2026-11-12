#!/bin/sh
# E02G registered run (PREREGISTRATION.md). usage, from the repository root: research/codec/E02G/run.sh DOWNLOAD_DIR OUT_DIR
set -eu
dl=$1; out=$2; e=research/codec/E02G; h=research/codec/E02C-harness
[ ! -e "$out" ] || { echo "OUT_DIR must be new" >&2; exit 2; }
(cd $h/harness && sha256sum -c ../../E02G/FROZEN.sha256) || { echo "harness differs from the frozen registration" >&2; exit 2; }
git diff --quiet 2f3308a3380a140ad0f6873ae228de7715e5eef5 -- src vendor Makefile tests benchmark/accept/TARGETS.md || { echo "production files changed" >&2; exit 2; }
mkdir -p "$out"
python3 -I $e/test_prepare.py
make ARCH= build/libvendor.a
make -f $h/Makefile all check
python3 -I $e/prepare.py "$dl" "$out/inputs" "$out/manifest.json"      # hashes recorded before any codec call
python3 -I $h/run_matrix.py "$out/inputs" "$out/run" --reps 7 --seed 20261117 --manifest "$out/manifest.json"
python3 -I $h/analyze.py "$out/run/raw.jsonl" "$out/run" "$out/manifest.json"
python3 -I $e/decide.py "$out/run/raw.jsonl" "$out/manifest.json" "$out/decision.json"
