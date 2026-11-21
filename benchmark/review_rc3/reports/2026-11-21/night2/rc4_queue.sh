#!/bin/bash
# rc4_queue.sh V : compat matrix (rc4 added), kimg compat, native version check, byte identity vs C2/C3r3. One at a time.
V=$1; L=$V/logs/rc4-queue.log; R=$V/final-builds/rc4/build/kolmo
log(){ echo "$(date -u +%FT%TZ) $*" >> $L; }
cd $V/repo
log "compat start"; python3 -I benchmark/review_rc3/compat_matrix.py $V/evidence/config/compat-spec-rc4.json $V/evidence/compat-matrix-rc4 > $V/logs/compat-matrix-rc4.out 2>&1; log "compat rc=$? $(tail -1 $V/logs/compat-matrix-rc4.out)"
cd $V; log "kimg-compat start"; python3 -I tools/kimg_compat.py $V/evidence/kimg-compat-rc4 $R > $V/logs/kimg-compat-rc4.out 2>&1; log "kimg-compat rc=$? $(tail -1 $V/logs/kimg-compat-rc4.out)"
log "native start"; bash tools/run_native_check.sh $V $R native-version-rc4 > $V/logs/native-version-rc4.sh.out 2>&1; log "native $(head -1 $V/logs/native-version-rc4.sh.out)"
log "identity start"; python3 -I $V/tools/rc4_identity.py $V > $V/logs/rc4-identity.out 2>&1; log "identity rc=$? $(tail -1 $V/logs/rc4-identity.out)"
log RC4_QUEUE_DONE
