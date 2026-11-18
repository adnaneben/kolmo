#!/bin/sh
# End-to-end runs of the lane (RESULTS.md), strictly one process at a time. base = rc3 base 1536036, new = branch head.
B=${B:-/tmp/claude-0/-home-user-kolmo/62c8be69-86e4-5155-a7a9-ec7153d8f311/scratchpad}; D=/home/user/data/c
OUT=$(dirname "$0")/e2e.csv; P="python3 -I $(dirname "$0")/bench.py $OUT base=$B/kolmo-base new=$B/kolmo-new"
$P -- x:1 x:4 -- $D/text_100k $D/log_100k $D/table_100k $D/bin_100k $D/num_100k $D/image_100k \
                 $D/text_1m $D/log_1m $D/table_1m $D/bin_1m $D/num_1m $D/image_1m $D/mixed_1m
$P -- x:4 -- $D/text_10m $D/log_10m $D/table_10m $D/bin_10m $D/num_10m $D/image_8m $D/mixed_10m
$P -- x:1 -- $D/text_10m $D/num_10m
$P -- balanced:1 balanced:4 hc:1 hc:4 ratio:1 ratio:4 -- $D/mixed_1m $D/mixed_10m $D/mixed_100m
$P -- balanced:4 hc:4 ratio:4 -- $D/text_100m $D/log_100m $D/table_100m $D/bin_100m $D/num_100m
$P -- x:4 -- $D/text_100m $D/num_100m
