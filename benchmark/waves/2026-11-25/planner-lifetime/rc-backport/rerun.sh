#!/bin/zsh
# Re-run after Codex's harness review: v12 readback with exit codes, GCC gate, ledger. One process at a time.
S=$1; R=$S/rc-receipts
for t in rc rcfix; do for b in test-v12-corpus test-v12-corpus-asan; do
  (cd $S/$t && ./build/$b) > $R/v12-$t-$b.log 2>&1; ec=$?
  echo "v12 tree=$t bin=build/$b exit=$ec: $(grep -E 'PASSED|FAILED' $R/v12-$t-$b.log | tail -1)"
done; done
$S/gcc-gate.sh $S
$S/ledger.sh $S --self-test > $R/ledger-self-test.log 2>&1; echo "ledger self-test exit=$? ($(tail -1 $R/ledger-self-test.log))"
$S/ledger.sh $S; echo "ledger exit=$?"
