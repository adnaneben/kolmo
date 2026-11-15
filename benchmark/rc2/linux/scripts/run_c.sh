#!/bin/sh
# Tier C: FASTQ, video, research codecs; every tool on one thread; vmstat 1 logged.
R=/home/user/kolmo/benchmark/rc2/linux; D=/home/user/data; S=$D/silesia/files
C="python3 -I /home/user/kolmo/benchmark/rc2/run_cli2.py --tools /home/user/tools/tools.json --work /home/user/tools/cwork"
head -1 /proc/stat > $R/logs/procstat_C_before.txt
vmstat -t 1 > $R/logs/vmstat_C.log 2>&1 & V=$!
mkdir -p $R/C
$C --group fastq --files $D/fastq/SRR1770413.fastq $D/fastq/SRR2584863.fastq $D/fastq/SRR835775.fastq --out $R/C/fastq.jsonl > $R/logs/C_fastq.log 2>&1; echo "fastq rc=$?" >> $R/logs/C_fastq.log
$C --group video --files $D/video/akiyo_cif.y4m $D/video/foreman_cif.y4m $D/video/mobile_cif.y4m --out $R/C/video.jsonl > $R/logs/C_video.log 2>&1; echo "video rc=$?" >> $R/logs/C_video.log
$C --group research --files $D/silesia/silesia.tar $S/dickens $S/mozilla $S/osdb $S/nci $S/x-ray --out $R/C/research.jsonl > $R/logs/C_research.log 2>&1; echo "research rc=$?" >> $R/logs/C_research.log
head -1 /proc/stat > $R/logs/procstat_C_after.txt
kill $V
echo C done > $R/logs/C_done.txt
