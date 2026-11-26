#!/bin/zsh
# Per-pair ledger: rc a2e430fe vs #112 4557e49f, every input x levels 1-9 x T1/T4.
# Usage: ledger.sh S [RC_BIN PR_BIN]. One process at a time (no pipelines), so every exit status is checked:
# both encoders must exit 0, both cross-decoders must exit 0, and the restored bytes must equal the input.
# Exits nonzero if any row fails. `ledger.sh S --self-test` checks that a failing decoder yields a failed row.
S=$1; R=$S/rc-receipts; W=$S/ledger-work; mkdir -p $W
A=${2:-$S/rc/build/kolmo}; B=${3:-$S/rcfix/build/kolmo}; OUT=${LEDGER_OUT:-$R/frame-ledger.csv}
if [ "$2" = "--self-test" ]; then
  # a stand-in decoder that exits 1 with no output, on empty input: the row must fail and the harness must exit nonzero
  printf '#!/bin/sh\ncase "$1" in -d) exit 1;; esac\nexec %s "$@"\n' "$S/rcfix/build/kolmo" > $W/bad-decoder; chmod +x $W/bad-decoder
  mkdir -p $W/st; : > $W/st/empty.bin
  LEDGER_OUT=$W/st/ledger.csv LEDGER_INPUTS=$W/st/empty.bin LEDGER_LEVELS=1 LEDGER_THREADS=1 $0 $S $S/rcfix/build/kolmo $W/bad-decoder
  rc=$?; echo "self-test: harness exit=$rc (expected nonzero)"; tail -1 $W/st/ledger.csv
  [ $rc -ne 0 ] && grep -q ',NO' $W/st/ledger.csv && { echo "SELF-TEST PASSED"; exit 0; } || { echo "SELF-TEST FAILED"; exit 1; }
fi
inputs=(${=LEDGER_INPUTS:-$(print -l $S/rcout/det-rc/*(.) $S/idcorp/*.bin $S/idcorp/*.npy $S/idcorp/*.log | grep -vE '\.(k|back|json)$')})
levels=(${=LEDGER_LEVELS:-1 2 3 4 5 6 7 8 9}); threads=(${=LEDGER_THREADS:-1 4})
echo "input,input_sha256,level,threads,rc_enc_exit,pr112_enc_exit,rc_frame_sha256,pr112_frame_sha256,bytes,identical,rc_dec_pr112_exit,rc_dec_pr112_bytes_ok,pr112_dec_rc_exit,pr112_dec_rc_bytes_ok,row_ok" > $OUT
bad=0
for f in $inputs; do
 ih=$(shasum -a256 < $f | cut -c1-64)
 for L in $levels; do for T in $threads; do
  rm -f $W/a $W/b $W/da $W/db
  $A -$L -T $T -c $f > $W/a; ea=$?
  $B -$L -T $T -c $f > $W/b; eb=$?
  ha=$(shasum -a256 < $W/a | cut -c1-64); hb=$(shasum -a256 < $W/b | cut -c1-64)
  $A -d -c $W/b > $W/db; d1=$?; cmp -s $W/db $f && c1=yes || c1=NO
  $B -d -c $W/a > $W/da; d2=$?; cmp -s $W/da $f && c2=yes || c2=NO
  [ $ha = $hb ] && same=yes || same=NO
  ok=yes; [ $ea -eq 0 ] && [ $eb -eq 0 ] && [ $d1 -eq 0 ] && [ $d2 -eq 0 ] && [ $same = yes ] && [ $c1 = yes ] && [ $c2 = yes ] || { ok=NO; bad=$((bad+1)); }
  echo "$(basename $f),$ih,$L,$T,$ea,$eb,$ha,$hb,$(wc -c < $W/b | tr -d ' '),$same,$d1,$c1,$d2,$c2,$ok" >> $OUT
 done; done; done
echo "rows=$(($(wc -l < $OUT)-1)) failed_rows=$bad"
[ $bad -eq 0 ]
