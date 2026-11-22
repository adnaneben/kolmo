#!/bin/sh
# Command line of queryable and deduplicating archives: kolmo --index, cat, grep, archive, extract, contents.  Usage: sh tests/test_features_b_cli.sh build/kolmo
set -e
K=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; cd "$T"
fail() { echo "FAIL: $*"; exit 1; }
awk 'BEGIN { srand(3); print "ts,station,co2,temp"; for (i = 0; i < 60000; i++) printf "%d,s%d,%.1f,%.2f\n", 1700000000 + i, i % 7, 400 + (i % 97) / 3, (i % 31) - 5 }' > t.csv
"$K" --index --block-size 0.25 t.csv t.kol
"$K" -d t.kol -o back.csv && cmp t.csv back.csv || fail "indexed file does not decode with kolmo -d"
"$K" cat t.kol | cmp - t.csv || fail "cat"
"$K" cat --col co2 --rows 1000-2000 t.kol > a.txt
awk -F, 'NR >= 1000 && NR <= 2000 { print $3 }' t.csv | cmp - a.txt || fail "cat --col --rows"
"$K" cat --col '#4' --rows 59990- -n t.kol > b.txt
awk -F, 'NR >= 59990 { print NR ":" $4 }' t.csv | cmp - b.txt || fail "cat --col #N -n"
[ "$("$K" grep -c -F s3 t.kol)" = "$(grep -c -F s3 t.csv)" ] || fail "grep -c"
"$K" grep -n -w '1700012345' t.kol | cmp - "$(printf '' > c.txt; grep -n -w 1700012345 t.csv > c.txt; echo c.txt)" || fail "grep -n -w"
"$K" grep -i --col co2 'S5' t.kol | cmp - "$(awk -F, 'tolower($0) ~ /s5/ { print $3 }' t.csv > d.txt; echo d.txt)" || fail "grep --col"
if "$K" grep zzzzabsent t.kol > /dev/null; then fail "grep with no match must exit 1"; fi
if "$K" cat --col nosuch t.kol 2> /dev/null; then fail "unknown column must fail"; fi
# damaged index (the last bytes): an error that says so, never output
cp t.kol bad.kol; printf 'X' | dd of=bad.kol bs=1 seek=$(( $(wc -c < t.kol) - 30 )) conv=notrunc 2> /dev/null
if "$K" cat --col co2 bad.kol > /dev/null 2>&1; then fail "damaged index accepted"; fi
"$K" cat --col co2 --no-index bad.kol | cmp - "$(awk -F, '{ print $3 }' t.csv > e.txt; echo e.txt)" || fail "--no-index on a damaged index"
# archives
mkdir -p src/sub; cp t.csv src/a.csv; head -c 300000 t.csv > src/sub/b.csv; cat t.csv t.csv > src/c.csv; : > src/empty
"$K" archive --chunk 4 arc.kol src/a.csv src/sub/b.csv src/c.csv src/empty
"$K" contents arc.kol | grep -q 'src/sub/b.csv' || fail "l"
mkdir out; "$K" extract -C out arc.kol
for f in a.csv sub/b.csv c.csv empty; do cmp "src/$f" "out/src/$f" || fail "x $f"; done
mkdir one; "$K" extract -C one arc.kol src/c.csv; cmp src/c.csv one/src/c.csv || fail "x one file"; [ ! -e one/src/a.csv ] || fail "x extracted an unnamed file"
# a stored name that climbs out of the directory is refused
mkdir -p deep/x; cp t.csv deep/evil.csv; (cd deep/x && "$K" archive ../../arc2.kol ../evil.csv)
mkdir safe; if (cd safe && "$K" extract ../arc2.kol 2> /dev/null); then fail "unsafe name extracted"; fi
[ ! -e evil.csv ] || fail "unsafe name written outside the directory"
# levels as in the main command line; the 0.0.x options are refused with exit status 2
"$K" --index -9 --block-size 0.5 t.csv t9.kol && "$K" -d t9.kol -o back9.csv && cmp t.csv back9.csv || fail "--index -9"
"$K" archive --level 1 arc1.kol src/a.csv && mkdir o1 && "$K" extract -C o1 arc1.kol && cmp src/a.csv o1/src/a.csv || fail "archive --level 1"
for old in "-m hc" "-x" "-l 9" "-B 0.25" "-mhc" "-B0.5"; do
    set +e; "$K" --index $old t.csv told.kol 2> err.txt; rc=$?; set -e
    [ $rc = 2 ] && grep -q "removed in 0.1.0" err.txt || fail "--index $old: exit $rc"
    set +e; "$K" archive $old arcold.kol src/a.csv 2> err.txt; rc=$?; set -e
    [ $rc = 2 ] && grep -q "removed in 0.1.0" err.txt || fail "archive $old: exit $rc"
done
# the 0.0.x single-letter archive commands are refused; a file of that name is compressed with ./NAME or -- NAME
for old in a x l; do
    set +e; "$K" $old arc.kol 2> err.txt; rc=$?; set -e
    [ $rc = 2 ] && grep -q "removed in 0.1.0" err.txt || fail "kolmo $old: exit $rc"
done
cp t.csv a; "$K" ./a && [ -f a.kolmo ] || fail "kolmo ./a"
cp t.csv x; "$K" -- x && [ -f x.kolmo ] || fail "kolmo -- x"
echo "ok: features-b command line"
