#!/bin/sh
# 7-Zip: extract the single entry of archive $2 to file $3 with $1 threads.
set -e; 7zz e -so -mmt="$1" -bso0 -bsp0 "$2" > "$3"
