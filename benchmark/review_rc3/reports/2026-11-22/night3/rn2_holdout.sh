#!/bin/bash
# rc4 holdout: take the timing lock (the KLZH subagent waits for it), preflight, timed run, report. One workload at a time.
V="$1"; CFG="$V/evidence/config/holdout-rn2-config.json"; LK="$V/locks/timing"; cd "$V/repo"
until mkdir "$LK" 2>/dev/null; do sleep 20; done; echo "rn2-holdout $(date -u +%FT%TZ)" > "$V/locks/timing.owner"
echo "$(date -u +%FT%TZ) PREFLIGHT"
python3 -I benchmark/review_rc3/audit.py preflight --config "$CFG" --suite holdout-rn2 --out "$V/evidence/preflight-holdout-rn2" > "$V/logs/preflight-holdout-rn2.out" 2>&1; rc=$?
echo "$(date -u +%FT%TZ) PREFLIGHT rc=$rc"
if [ $rc = 0 ]; then
  echo "$(date -u +%FT%TZ) START holdout-rn2"
  python3 -I benchmark/review_rc3/audit.py run --config "$CFG" --suite holdout-rn2 --new-run "$V/evidence/holdout-rn2.noindex/holdout-rn2" > "$V/logs/holdout-rn2.out" 2>&1
  echo "$(date -u +%FT%TZ) END holdout-rn2 rc=$?"
  python3 -I benchmark/review_rc3/audit.py report --run "$V/evidence/holdout-rn2.noindex/holdout-rn2" --out "$V/evidence/report-holdout-rn2/holdout-rn2" --compare rn2/balanced:rc4/balanced rn2/hc:rc4/hc rn2/ratio:rc4/ratio > "$V/logs/report-holdout-rn2.out" 2>&1; echo "report rc=$?"
fi
rmdir "$LK"; echo HOLDOUT_RN2_DONE
python3 -I /Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/tools/summarize_holdout.py /Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/evidence/report-holdout-rn2 holdout-rn2 > /dev/null
