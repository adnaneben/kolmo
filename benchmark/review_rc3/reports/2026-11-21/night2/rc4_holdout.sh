#!/bin/bash
# rc4 holdout: take the timing lock (the KLZH subagent waits for it), preflight, timed run, report. One workload at a time.
V="$1"; CFG="$V/evidence/config/holdout-rc4-config.json"; LK="$V/locks/timing"; cd "$V/repo"
until mkdir "$LK" 2>/dev/null; do sleep 20; done; echo "rc4-holdout $(date -u +%FT%TZ)" > "$V/locks/timing.owner"
echo "$(date -u +%FT%TZ) PREFLIGHT"
python3 -I benchmark/review_rc3/audit.py preflight --config "$CFG" --suite holdout-rc4 --out "$V/evidence/preflight-holdout-rc4" > "$V/logs/preflight-holdout-rc4.out" 2>&1; rc=$?
echo "$(date -u +%FT%TZ) PREFLIGHT rc=$rc"
if [ $rc = 0 ]; then
  echo "$(date -u +%FT%TZ) START holdout-rc4"
  python3 -I benchmark/review_rc3/audit.py run --config "$CFG" --suite holdout-rc4 --new-run "$V/evidence/holdout-rc4.noindex/holdout-rc4" > "$V/logs/holdout-rc4.out" 2>&1
  echo "$(date -u +%FT%TZ) END holdout-rc4 rc=$?"
  python3 -I benchmark/review_rc3/audit.py report --run "$V/evidence/holdout-rc4.noindex/holdout-rc4" --out "$V/evidence/report-holdout-rc4/holdout-rc4" --compare rc4/speed:c2/speed rc4/balanced:c2/balanced rc4/eco:c2/eco rc4/hc:c2/hc rc4/ratio:c2/ratio rc4/speed:lz4-1 rc4/balanced:zstd-3 rc4/eco:zstd-9 rc4/hc:zstd-19 rc4/ratio:xz-9 > "$V/logs/report-holdout-rc4.out" 2>&1; echo "report rc=$?"
fi
rmdir "$LK"; echo HOLDOUT_RC4_DONE
