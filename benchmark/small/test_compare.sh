#!/bin/bash
# compare.sh's load gate, with stub benchmark binaries and stub load readings: no row is kept from an attempt that
# starts or ends above MAXLOAD, and exhausted retries leave the step out and exit 3.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; fail=0
for who in base new; do
    printf '#!/bin/sh\nprintf "kind\\tsize\\n"; printf "stub\\t$2\\n"\n' > "$T/smallbench-$who"; chmod +x "$T/smallbench-$who"
done
echo "stub=/dev/null:raw" > "$T/sets.txt"
run() { PREBUILT=1 V=/ SETS_FILE="$T/sets.txt" SIZES="64 256" WAIT=0 POLL=1 MAXLOAD=12 LOAD_CMD="$1" "$HERE/compare.sh" - - "$T" >/dev/null 2>&1; }
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
run "echo 99"; rc=$?
check "always over the limit: exit 3" "[ $rc = 3 ]"
check "always over the limit: no timing row" "[ \$(cat $T/base.tsv $T/new.tsv | grep -c stub) = 0 ]"
check "always over the limit: every step listed" "[ \$(wc -l < $T/incomplete.txt) = 4 ]"
echo 0 > "$T/n"   # start below, end above (every reading alternates 5, 99): the run ends busy, so it is not kept
run "n=\$(cat $T/n); echo \$((n + 1)) > $T/n; [ \$((n % 2)) = 0 ] && echo 5 || echo 99"; rc=$?
check "ends over the limit: exit 3, no row" "[ $rc = 3 ] && [ \$(cat $T/base.tsv $T/new.tsv | grep -c stub) = 0 ]"
run "echo 3"; rc=$?
check "quiet: exit 0, two rows per build" "[ $rc = 0 ] && [ \$(grep -c stub $T/base.tsv) = 2 ] && [ \$(grep -c stub $T/new.tsv) = 2 ] && [ ! -s $T/incomplete.txt ]"
exit $fail
