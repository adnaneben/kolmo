#!/bin/bash
# Moves seeds that take more than LIMIT seconds for one run of the given target (ASan build) into $SLOW.
# Usage: fuzz/campaign/filter_slow.sh target seeddir slowdir [limit=2]
T=$1; D=$2; SLOW=$3; LIMIT=${4:-2}; mkdir -p "$SLOW"
ls "$D" | xargs -P 2 -I{} bash -c 'timeout '"$LIMIT"' '"$T"' -runs=1 "'"$D"'/{}" >/dev/null 2>&1; [ $? -eq 124 ] && mv "'"$D"'/{}" "'"$SLOW"'/" && echo "slow: {}"'
