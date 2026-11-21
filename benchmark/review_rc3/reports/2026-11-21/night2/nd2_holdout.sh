#!/bin/bash
# rc4 holdout: take the timing lock (the KLZH subagent waits for it), preflight, timed run, report. One workload at a time.
V="$1"; CFG="$V/evidence/config/holdout-nd2-config.json"; LK="$V/locks/timing"; cd "$V/repo"
until mkdir "$LK" 2>/dev/null; do sleep 20; done; echo "nd2-holdout $(date -u +%FT%TZ)" > "$V/locks/timing.owner"
echo "$(date -u +%FT%TZ) PREFLIGHT"
python3 -I benchmark/review_rc3/audit.py preflight --config "$CFG" --suite holdout-nd2 --out "$V/evidence/preflight-holdout-nd2" > "$V/logs/preflight-holdout-nd2.out" 2>&1; rc=$?
echo "$(date -u +%FT%TZ) PREFLIGHT rc=$rc"
if [ $rc = 0 ]; then
  echo "$(date -u +%FT%TZ) START holdout-nd2"
  python3 -I benchmark/review_rc3/audit.py run --config "$CFG" --suite holdout-nd2 --new-run "$V/evidence/holdout-nd2.noindex/holdout-nd2" > "$V/logs/holdout-nd2.out" 2>&1
  echo "$(date -u +%FT%TZ) END holdout-nd2 rc=$?"
  python3 -I benchmark/review_rc3/audit.py report --run "$V/evidence/holdout-nd2.noindex/holdout-nd2" --out "$V/evidence/report-holdout-nd2/holdout-nd2" --compare nd2/speed:rc4/speed nd2/speed:lz4-1 nd2/hc:rc4/hc nd2/hc:zstd-19 > "$V/logs/report-holdout-nd2.out" 2>&1; echo "report rc=$?"
fi
rmdir "$LK"; echo HOLDOUT_ND2_DONE
