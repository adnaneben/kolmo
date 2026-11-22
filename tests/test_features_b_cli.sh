#!/bin/sh
# Command line of queryable and deduplicating archives: kolmo --index, cat, grep, a, x, l.  Usage: sh tests/test_features_b_cli.sh build/kolmo
set -e
K=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; cd "$T"
fail() { echo "FAIL: $*"; exit 1; }
awk 'BEGIN { srand(3); print "ts,station,co2,temp"; for (i = 0; i < 60000; i++) printf "%d,s%d,%.1f,%.2f\n", 1700000000 + i, i % 7, 400 + (i % 97) / 3, (i % 31) - 5 }' > t.csv
"$K" --index -B 0.25 t.csv t.kol
"$K" -d t.kol back.csv && cmp t.csv back.csv || fail "indexed file does not decode with kolmo -d"
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
"$K" a --chunk 4 arc.kol src/a.csv src/sub/b.csv src/c.csv src/empty
"$K" l arc.kol | grep -q 'src/sub/b.csv' || fail "l"
mkdir out; "$K" x -C out arc.kol
for f in a.csv sub/b.csv c.csv empty; do cmp "src/$f" "out/src/$f" || fail "x $f"; done
mkdir one; "$K" x -C one arc.kol src/c.csv; cmp src/c.csv one/src/c.csv || fail "x one file"; [ ! -e one/src/a.csv ] || fail "x extracted an unnamed file"
# a stored name that climbs out of the directory is refused
mkdir -p deep/x; cp t.csv deep/evil.csv; (cd deep/x && "$K" a ../../arc2.kol ../evil.csv)
mkdir safe; if (cd safe && "$K" x ../arc2.kol 2> /dev/null); then fail "unsafe name extracted"; fi
[ ! -e evil.csv ] || fail "unsafe name written outside the directory"
echo "ok: features-b command line"
