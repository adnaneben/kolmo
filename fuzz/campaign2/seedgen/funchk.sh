#!/bin/bash
# per-seed coverage check: for each file in $2, run cov build $1 once and list which target functions were entered.
# usage: funchk.sh <cov-binary> <seeddir> <outdir>
B=$1; D=$2; O=$3; mkdir -p "$O/p"; rm -f "$O"/p/*
ls "$D" | xargs -P 4 -I{} bash -c 'LLVM_PROFILE_FILE="'"$O"'/p/{}.profraw" timeout 300 "'"$B"'" -runs=1 "'"$D"'/{}" >/dev/null 2>&1; llvm-profdata merge -sparse "'"$O"'/p/{}.profraw" -o "'"$O"'/p/{}.profdata" 2>/dev/null && llvm-cov export -summary-only=false -skip-expansions "'"$B"'" -instr-profile="'"$O"'/p/{}.profdata" 2>/dev/null | python3 -I /tmp/claude-0/gen/funcs.py > "'"$O"'/p/{}.funcs"; rm -f "'"$O"'/p/{}.profraw" "'"$O"'/p/{}.profdata"'
