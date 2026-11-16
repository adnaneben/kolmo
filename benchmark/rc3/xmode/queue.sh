#!/bin/sh
# The xmode lane's measurement queue, one run at a time (the VM has 4 vCPUs and one 16 GB memory limit).
# usage: queue.sh BINDIR DATA OUTDIR. BINDIR holds kolmo-base, kolmo-s1 ... kolmo-s6 (each step's build) and kolmo-final (the merged branch head).
# A step is measured on the files it can change; the others are identical to the previous step by construction.
set -e
B=$1; D=$2; O=$3; R="python3 -I $(dirname "$0")/run.py"
$R $B/kolmo-base $D $O/base.csv --sizes 100k,1m,10m --threads 1,4 --label base
$R $B/kolmo-s1 $D $O/s1.csv --sizes 100k,1m,10m --threads 4 --label s1-trials
$R $B/kolmo-s2 $D $O/s2.csv --sizes 100k,1m,10m --kinds fq --threads 1,4 --label s2-fastq-first
$R $B/kolmo-s3 $D $O/s3.csv --sizes 100k,1m,10m --kinds text,csv,bin --threads 1,4 --label s3-prefetch-avx2
$R $B/kolmo-s4 $D $O/s4.csv --sizes 100k,1m,10m --kinds csv --threads 1,4 --label s4-columns
$R $B/kolmo-s5 $D $O/s5.csv --sizes 10m --kinds text,bin --threads 1,4 --label s5-halves
$R $B/kolmo-s6 $D $O/s6.csv --sizes 1m,10m --kinds fq --threads 1,4 --label s6-fq-chunks
$R $B/kolmo-final $D $O/s7.csv --sizes 100k,1m,10m --kinds text,csv,bin --threads 1,4 --label s7-scaled --args "-x --x-tables=scaled"
# 100 MB: before (rc3 base) and after (all steps), 1 and 4 threads
$R $B/kolmo-base $D $O/base100.csv --sizes 100m --threads 1,4 --label base
$R $B/kolmo-final $D $O/final100.csv --sizes 100m --threads 1,4 --label final
$R $B/kolmo-final $D $O/final100.csv --sizes 100m --kinds text,csv,bin --threads 4 --label final-scaled --args "-x --x-tables=scaled"
# step 3 on a baseline-ISA build (-march=x86-64, what releases ship): base against s3, 1 MB, 1 thread
$R $B/kolmo-base-x86base $D $O/isa.csv --sizes 1m --kinds text,csv,bin --threads 1 --label base-x86-64
$R $B/kolmo-s3-x86base $D $O/isa.csv --sizes 1m --kinds text,csv,bin --threads 1 --label s3-x86-64
# step 5 on a single non-text stream of 10 MB (Silesia osdb; the bin_* inputs are tar archives cut into blocks < 8 MiB)
$R $B/kolmo-s4 $D $O/s5.csv --sizes 10m --kinds osdb --threads 1,4 --label s4-columns
$R $B/kolmo-s5 $D $O/s5.csv --sizes 10m --kinds osdb --threads 1,4 --label s5-halves
# step 7: intermediate table caps on text (the kind that pays most for small tables)
$R $B/kolmo-final $D $O/s7.csv --sizes 10m --kinds text --threads 1 --label s7-cap20 --args "-x --x-tables=20"
$R $B/kolmo-final $D $O/s7.csv --sizes 10m --kinds text --threads 1 --label s7-cap21 --args "-x --x-tables=21"
