#!/bin/sh
# Resume of queue.sh after the container restart of 16 November (rows already in the CSVs are not re-run).
set -e
B=$1; D=$2; O=$3; R="python3 -I $(dirname "$0")/run.py"
$R $B/kolmo-base $D $O/base100.csv --sizes 100m --kinds bin,fq --threads 1,4 --label base
$R $B/kolmo-final $D $O/final100.csv --sizes 100m --threads 1,4 --label final
$R $B/kolmo-s4 $D $O/s5.csv --sizes 10m --kinds osdb --threads 1,4 --label s4-columns
$R $B/kolmo-s5 $D $O/s5.csv --sizes 10m --kinds osdb --threads 1,4 --label s5-halves
$R $B/kolmo-base-x86base $D $O/isa.csv --sizes 1m --kinds text,csv,bin --threads 1 --label base-x86-64
$R $B/kolmo-s3-x86base $D $O/isa.csv --sizes 1m --kinds text,csv,bin --threads 1 --label s3-x86-64
$R $B/kolmo-final $D $O/s7.csv --sizes 10m --kinds text --threads 1 --label s7-cap20 --args "-x --x-tables=20"
$R $B/kolmo-final $D $O/s7.csv --sizes 10m --kinds text --threads 1 --label s7-cap21 --args "-x --x-tables=21"
$R $B/kolmo-final $D $O/final100.csv --sizes 100m --kinds text,csv,bin --threads 4 --label final-scaled --args "-x --x-tables=scaled"
