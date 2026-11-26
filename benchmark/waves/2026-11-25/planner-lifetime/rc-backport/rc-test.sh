#!/bin/zsh
S=$1; cd $S; O=$S/rcout; mkdir -p $O
for t in rc rcfix; do make -C $S/$t -j2 build/kolmo build/test-v12-corpus build/test-v12-corpus-asan > $O/build-$t.log 2>&1 || echo BUILD_FAIL $t; done
for t in rc rcfix; do make -C $S/$t check-planner-ownership > $O/own-$t.log 2>&1; echo "ownership $t rc=$? $(grep PLANNER_OWNERSHIP $O/own-$t.log)"; done
for t in rc rcfix; do (cd $S/$t && ./build/test-v12-corpus | tail -1; ./build/test-v12-corpus-asan | tail -1) > $O/v12-$t.log 2>&1; echo "v12 $t: $(tr '\n' ' ' < $O/v12-$t.log)"; done
for t in rc rcfix; do python3 $S/$t/benchmark/determinism/check.py $S/$t/build/kolmo $O/det-$t > $O/det-$t.log 2>&1; echo "determinism $t rc=$? $(tail -1 $O/det-$t.log)"; done
A=$S/rc/build/kolmo; B=$S/rcfix/build/kolmo; n=0; bad=0; W=$O/pairs; mkdir -p $W
for f in $O/det-rc/*(.) $S/idcorp/*.bin $S/idcorp/*.npy $S/idcorp/*.log; do case $f in *.k|*.back|*.json) continue;; esac
 for L in 1 2 3 4 5 6 7 8 9; do for T in 1 4; do $A -$L -T $T -c $f > $W/a; $B -$L -T $T -c $f > $W/b; cmp -s $W/a $W/b || { bad=$((bad+1)); echo "DIFF $f -$L -T$T"; }; $A -d -c $W/b | cmp -s - $f || { bad=$((bad+1)); echo "XDEC $f $L"; }; n=$((n+1)); done; done; done
echo "frame pairs=$n failures=$bad"
load1(){ sysctl -n vm.loadavg | awk '{print int($2)}'; }
F=$O/det-rc/floats.f64; if [ $(load1) -ge 12 ]; then echo "timing skipped: 1-min load $(load1) >= 12 (lane rule)"; : > $O/timing.txt; else for L in 4 9; do for r in 1 2 3 4 5 6 7; do for t in rc rcfix; do s=$(perl -MTime::HiRes=time -e '$t=time; system("$ARGV[0] $ARGV[1] -T 1 -c $ARGV[2] >/dev/null 2>&1"); printf "%.4f", time-$t' $S/$t/build/kolmo -$L $F); echo "time -$L $t $s"; done; done; done > $O/timing.txt; fi
[ -s $O/timing.txt ] && python3 - $O/timing.txt <<'PY'
import sys,statistics as st,collections as c
d=c.defaultdict(list)
for l in open(sys.argv[1]):
  _,L,t,s=l.split(); d[(L,t)].append(float(s))
for L in ('-4','-9'):
  a=st.median(d[(L,'rc')]); b=st.median(d[(L,'rcfix')]); print(f"floats.f64 {L} T1 median s: rc={a:.4f} rcfix={b:.4f} ratio={b/a:.3f}")
PY
for t in rc rcfix; do make -C $S/$t -j2 check-features-b > $O/featb-$t.log 2>&1; echo "check-features-b $t rc=$?"; grep -iE "assertion|passed|failed" $O/featb-$t.log | tail -3; done
make -C $S/rcfix -j2 check > $O/check-rcfix.log 2>&1; echo "make check rcfix rc=$?"; tail -3 $O/check-rcfix.log
echo ALL_DONE
