#!/bin/bash
# GR6-L3 (research, branch claude/dazzling-maxwell-p8y4u0, research/gr6/gr6l3.c) against GR6+ (codec 26 alone, g6tool)
# on logs, prose, tables and JSON lines; one thread each. rc3 -x sizes come from the routing matrix.
# usage: l3eval.sh GR6L3_BIN G6TOOL_BIN OUT FILES...
L3=$1; G6=$2; OUT=$3; shift 3
for f in "$@"; do
  for pol in auto lzp none; do
    echo "L3 $pol $($L3 b $f 1 $pol | grep -E 'race on| ratio ' | tr '\n' ' ')" >> $OUT
  done
  echo "G6 $($G6 $f)" >> $OUT
done
