#!/bin/bash
# waits for Phase E of rn, writes the config from the release binary, then runs the timed holdout under the lock
V="$1"; until grep -q "PHASE_E_DONE rn" $V/logs/phase-e-rn.out 2>/dev/null; do sleep 30; done
python3 -I $V/tools/make_rn_config.py || exit 1
CFG="$V/evidence/config/holdout-rn-config.json"; LK="$V/locks/timing"; cd "$V/repo"
until mkdir "$LK" 2>/dev/null; do sleep 20; done; echo "rn-holdout $(date -u +%FT%TZ)" > "$V/locks/timing.owner"
python3 -I benchmark/review_rc3/audit.py preflight --config "$CFG" --suite holdout-rn --out "$V/evidence/preflight-holdout-rn" > "$V/logs/preflight-holdout-rn.out" 2>&1; rc=$?; echo "preflight rc=$rc"
if [ $rc = 0 ]; then
  python3 -I benchmark/review_rc3/audit.py run --config "$CFG" --suite holdout-rn --new-run "$V/evidence/holdout-rn.noindex/holdout-rn" > "$V/logs/holdout-rn.out" 2>&1; echo "run rc=$?"
  mkdir -p "$V/evidence/report-holdout-rn"
  python3 -I benchmark/review_rc3/audit.py report --run "$V/evidence/holdout-rn.noindex/holdout-rn" --out "$V/evidence/report-holdout-rn/holdout-rn" --compare rn/speed:rc4/speed rn/balanced:rc4/balanced rn/eco:rc4/eco rn/hc:rc4/hc rn/ratio:rc4/ratio rn/speed:lz4-1 rn/balanced:zstd-3 rn/eco:zstd-9 rn/hc:zstd-19 rn/ratio:xz-9 > "$V/logs/report-holdout-rn.out" 2>&1; echo "report rc=$?"
  python3 -I $V/tools/summarize_holdout.py $V/evidence/report-holdout-rn holdout-rn > /dev/null
fi
rmdir "$LK"; echo HOLDOUT_RN_DONE
