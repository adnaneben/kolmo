#!/bin/zsh
# Per-pair ledger: rc a2e430fe vs #112 4557e49f, every input x levels 1-9 x T1/T4.
S=$1; R=$S/rc-receipts; A=$S/rc/build/kolmo; B=$S/rcfix/build/kolmo; W=$S/ledger-work; mkdir -p $W
echo "input,input_sha256,level,threads,rc_frame_sha256,pr112_frame_sha256,bytes,identical,rc_decodes_pr112,pr112_decodes_rc" > $R/frame-ledger.csv
for f in $S/rcout/det-rc/*(.) $S/idcorp/*.bin $S/idcorp/*.npy $S/idcorp/*.log; do case $f in *.k|*.back|*.json) continue;; esac
 ih=$(shasum -a256 < $f | cut -c1-64)
 for L in 1 2 3 4 5 6 7 8 9; do for T in 1 4; do
  $A -$L -T $T -c $f > $W/a; $B -$L -T $T -c $f > $W/b
  ha=$(shasum -a256 < $W/a | cut -c1-64); hb=$(shasum -a256 < $W/b | cut -c1-64)
  $A -d -c $W/b | cmp -s - $f && x1=yes || x1=NO; $B -d -c $W/a | cmp -s - $f && x2=yes || x2=NO
  echo "$(basename $f),$ih,$L,$T,$ha,$hb,$(wc -c < $W/b | tr -d ' '),$([ $ha = $hb ] && echo yes || echo NO),$x1,$x2" >> $R/frame-ledger.csv
 done; done; done
echo "rows=$(($(wc -l < $R/frame-ledger.csv)-1)) not_identical=$(grep -c ',NO' $R/frame-ledger.csv)"
