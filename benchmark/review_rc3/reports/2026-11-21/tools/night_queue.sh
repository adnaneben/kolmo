#!/bin/bash
# night_queue.sh V : after Phase E C2 finishes and passes, run the timed C2 campaign one suite at a time.
V=$1; cd $V/repo; L=$V/logs/night-queue.log; log(){ echo "$(date -u +%FT%TZ) $*" | tee -a $L; }
while ! grep -q "PHASE_E_DONE c2" $V/logs/phase-e-c2.out 2>/dev/null; do sleep 30; done
fails=$(awk '$1!=0' $V/logs/phase-e-c2.summary | wc -l | tr -d ' ')
log "Phase E C2 done; nonzero steps: $fails"
[ "$fails" = 0 ] || { log "ABORT: Phase E C2 has failures"; exit 1; }
log "compat matrix C2 start"; python3 -I benchmark/review_rc3/compat_matrix.py $V/evidence/config/compat-spec-c2.json $V/evidence/compat-matrix-c2 > $V/logs/compat-matrix-c2.out 2>&1; log "compat C2 rc=$? $(tail -1 $V/logs/compat-matrix-c2.out)"
bad=$(python3 -c "import json,collections;c=collections.Counter(json.loads(l)['status'] for l in open('$V/evidence/compat-matrix-c2/rows.jsonl'));print(sum(v for k,v in c.items() if k not in ('exact','rejected')))"); log "compat C2 non-exact-non-rejected rows: $bad"
[ "$bad" = 0 ] || { log "ABORT: compat C2 has misdecodes/crashes/encode failures"; exit 1; }
python3 -I $V/tools/make_c2_configs.py >> $L 2>&1 || { log "ABORT: config"; exit 1; }
python3 -I $V/tools/make_domain_config.py $V/evidence/config/holdout-config.json $V/evidence/config/domain-config-c2.json c2=$V/final-builds/c2/build/kolmo >> $L 2>&1 || { log "ABORT: domain config"; exit 1; }
run(){ cfg=$1; s=$2; tag=$3
  python3 -I benchmark/review_rc3/audit.py preflight --config $cfg --suite $s --out $V/evidence/preflight-$tag/$s > $V/logs/preflight-$tag-$s.out 2>&1 || { log "PREFLIGHT FAIL $s"; return 1; }
  log "START $s"; python3 -I benchmark/review_rc3/audit.py run --config $cfg --suite $s --new-run $V/evidence/$tag.noindex/$s > $V/logs/$tag-$s.out 2>&1; log "END $s rc=$?"; }
for s in dom-audio dom-image dom-numeric dom-genomics dom-video; do run $V/evidence/config/domain-config-c2.json $s domain-c2; done
run $V/evidence/config/holdout-c2-config.json holdout-c2 holdout-c2
run $V/evidence/config/holdout-c2-config.json holdout-native holdout-c2
log QUEUE_DONE
