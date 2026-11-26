#!/bin/zsh
# GCC 16.2 lifetime-warning gate: rc a2e430fe vs #112 4557e49f, canonical + Python + Rust sources.
# Usage: gcc-gate.sh S [GCC]. One compile at a time. Prints the compiler version, then one line per tree:
# the exact command, its exit code and the -Wuse-after-free warning count. Full compiler output: gcc-gate-<tree>.log.
S=$1; GCC=${2:-gcc-16}; R=$S/rc-receipts
echo "compiler: $($GCC --version | head -1)"
for t in rc rcfix; do for d in src bindings/python/native/src bindings/rust/native/src; do
  name=$t-${${d//\//-}%-native-src}; log=$R/gcc-gate-$name.log
  cmd="$GCC -O1 -Wuse-after-free=3 -I. -c kolmo.c -o /dev/null"
  (cd $S/$t/$d && eval $cmd) > $log 2>&1; ec=$?
  echo "tree=$t dir=$d cmd='cd \$S/$t/$d && $cmd' exit=$ec warnings=$(grep -c 'Wuse-after-free' $log)"
done; done
