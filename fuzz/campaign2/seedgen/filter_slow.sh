#!/bin/bash
# move seeds of dir $2 that take more than $4 s on binary $1 into $3 (4 in parallel)
mkdir -p "$3"; ls "$2" | xargs -P 4 -I{} bash -c 'timeout '"$4"' '"$1"' -runs=1 "'"$2"'/{}" >/dev/null 2>&1; [ $? -eq 124 ] && mv "'"$2"'/{}" "'"$3"'/" && echo "slow: {}"'
