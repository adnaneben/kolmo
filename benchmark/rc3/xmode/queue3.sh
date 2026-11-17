#!/bin/sh
# Last part of the queue, after the codec 25 fix (937ef77): kolmo-final is the fixed branch head, kolmo-halves8 the
# same code with the halves threshold at 8 MiB (the rejected setting, to measure it on Silesia osdb).
set -e
B=$1; D=$2; O=$3; R="python3 -I $(dirname "$0")/run.py"
$R $B/kolmo-final $D $O/final100.csv --sizes 100m --kinds bin,fq --threads 1,4 --label final
$R $B/kolmo-final $D $O/s5.csv --sizes 10m --kinds osdb --threads 1,4 --label final
$R $B/kolmo-halves8 $D $O/s5.csv --sizes 10m --kinds osdb --threads 1,4 --label halves-from-8MiB
$R $B/kolmo-base-x86base $D $O/isa.csv --sizes 1m --kinds text,csv,bin --threads 1 --label base-x86-64
$R $B/kolmo-s3-x86base $D $O/isa.csv --sizes 1m --kinds text,csv,bin --threads 1 --label s3-x86-64
$R $B/kolmo-final $D $O/s7.csv --sizes 10m --kinds text --threads 1 --label s7-cap20 --args "-x --x-tables=20"
$R $B/kolmo-final $D $O/s7.csv --sizes 10m --kinds text --threads 1 --label s7-cap21 --args "-x --x-tables=21"
$R $B/kolmo-final $D $O/final100.csv --sizes 100m --kinds text,csv,bin --threads 4 --label final-scaled --args "-x --x-tables=scaled"
