#!/bin/bash
# rc4 holdout: take the timing lock (the KLZH subagent waits for it), preflight, timed run, report. One workload at a time.
V="$1"; CFG="$V/evidence/config/holdout-nhc-config.json"; LK="$V/locks/timing"; cd "$V/repo"
until mkdir "$LK" 2>/dev/null; do sleep 20; done; echo "nhc-holdout $(date -u +%FT%TZ)" > "$V/locks/timing.owner"
echo "$(date -u +%FT%TZ) PREFLIGHT"
python3 -I benchmark/review_rc3/audit.py preflight --config "$CFG" --suite holdout-nhc --out "$V/evidence/preflight-holdout-nhc" > "$V/logs/preflight-holdout-nhc.out" 2>&1; rc=$?
echo "$(date -u +%FT%TZ) PREFLIGHT rc=$rc"
if [ $rc = 0 ]; then
  echo "$(date -u +%FT%TZ) START holdout-nhc"
  python3 -I benchmark/review_rc3/audit.py run --config "$CFG" --suite holdout-nhc --new-run "$V/evidence/holdout-nhc.noindex/holdout-nhc" > "$V/logs/holdout-nhc.out" 2>&1
  echo "$(date -u +%FT%TZ) END holdout-nhc rc=$?"
  python3 -I benchmark/review_rc3/audit.py report --run "$V/evidence/holdout-nhc.noindex/holdout-nhc" --out "$V/evidence/report-holdout-nhc/holdout-nhc" --compare ntune2/hc+native-lz:rc4/hc ntune2/hc+native-lz:zstd-19 > "$V/logs/report-holdout-nhc.out" 2>&1; echo "report rc=$?"
fi
rmdir "$LK"; echo HOLDOUT_NHC_DONE
