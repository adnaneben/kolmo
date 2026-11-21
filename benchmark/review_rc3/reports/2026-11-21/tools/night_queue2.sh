#!/bin/bash
V=$1; cd $V/repo; L=$V/logs/night-queue2.log; log(){ echo "$(date -u +%FT%TZ) $*" | tee -a $L; }
run(){ cfg=$1; s=$2; tag=$3
  python3 -I benchmark/review_rc3/audit.py preflight --config $cfg --suite $s --out $V/evidence/preflight-$tag/$s > $V/logs/preflight-$tag-$s.out 2>&1 || { log "PREFLIGHT FAIL $s"; return 1; }
  log "START $s"; python3 -I benchmark/review_rc3/audit.py run --config $cfg --suite $s --new-run $V/evidence/$tag.noindex/$s > $V/logs/$tag-$s.out 2>&1; log "END $s rc=$?"; }
run $V/evidence/config/domain-config-c3.json dom-image domain-c3
run $V/evidence/config/ultra-c2-config.json ultra-c2 ultra-c2
log QUEUE2_DONE
