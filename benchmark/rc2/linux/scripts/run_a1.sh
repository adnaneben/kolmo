#!/bin/sh
# Tier A1 (kolmo), then A1 with the control library (kolmo_ctl rows only), vmstat 1 logged throughout.
export LD_LIBRARY_PATH=/home/user/tools/lzbench-kolmo-rc2/misc/rust-codecs/target/release
R=/home/user/kolmo/benchmark/rc2/linux; LZ=/home/user/tools/lzbench-kolmo-rc2/lzbench; T=/home/user/data/silesia/silesia.tar
head -1 /proc/stat > $R/logs/procstat_A1_before.txt
vmstat -t 1 > $R/logs/vmstat_A1.log 2>&1 & V=$!
python3 -I /home/user/kolmo/benchmark/rc1/run_lzbench.py --lzbench $LZ --out $R/A1 --tier A1 --silesia-tar $T > $R/logs/A1_run.log 2>&1
echo "A1 rc=$?" >> $R/logs/A1_run.log
head -1 /proc/stat > $R/logs/procstat_A1_after.txt
kill $V
head -1 /proc/stat > $R/logs/procstat_A1ctl_before.txt
vmstat -t 1 > $R/logs/vmstat_A1_ctl.log 2>&1 & V=$!
python3 -I /home/user/tools/scripts/run_lzbench_ctl.py --lzbench $LZ --out $R/A1_ctl --tier A1 --silesia-tar $T --only " (kolmo_ctl,[0-9]+|lz4|zstd,3)$" > $R/logs/A1_ctl_run.log 2>&1
echo "A1_ctl rc=$?" >> $R/logs/A1_ctl_run.log
head -1 /proc/stat > $R/logs/procstat_A1ctl_after.txt
kill $V
