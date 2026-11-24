#!/bin/sh
# kolmo archive / extract / --index safety (review 40, SYNTHESIS bugs 2-4: R20 #88 B1, R25 #107).
# Usage: sh tests/test_archive_safety.sh build/kolmo build/mkarchive
set -e
K=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
MK=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; cd "$T"
fail() { echo "FAIL: $*"; exit 1; }
# expect STATUS CMD...: the exit status of CMD (stderr in err.txt)
expect() { want=$1; shift; set +e; "$@" > out.txt 2> err.txt; rc=$?; set -e; [ "$rc" = "$want" ] || { cat err.txt; fail "$* : exit $rc, expected $want"; }; }
mkdir d; awk 'BEGIN { for (i = 0; i < 20000; i++) printf "line %d of the archive test %d\n", i, i * 7 }' > d/a; cp d/a d/b; printf zz >> d/b
"$K" archive a.kol d

# 1. -d, -t, -l, --range and a pipe refuse an archive (its frame is the chunk store), naming kolmo extract
expect 1 "$K" -d -c a.kol; [ ! -s out.txt ] || fail "-dc wrote the chunk store"; grep -q "kolmo extract a.kol" err.txt || fail "-dc message"
expect 1 "$K" -d a.kol -o plain; [ ! -e plain ] || fail "-d -o created an output"
expect 1 "$K" -t a.kol; grep -q "kolmo extract" err.txt || fail "-t message"
expect 1 "$K" -l a.kol; grep -q "kolmo contents a.kol" err.txt || fail "-l message"
expect 1 "$K" --range 0:10 a.kol; [ ! -s out.txt ] || fail "--range wrote store bytes"
set +e; "$K" -dc < a.kol > out.txt 2> err.txt; rc=$?; set -e
[ $rc = 1 ] && [ ! -s out.txt ] && grep -q "kolmo extract" err.txt || fail "-dc from a pipe: exit $rc"
cp a.kol two.kolmo; "$K" -c d/a >> two.kolmo                  # an archive followed by a plain frame: still refused
expect 1 "$K" -d -c two.kolmo
"$K" -c d/a > one.kolmo; "$K" -t one.kolmo && "$K" -l one.kolmo > /dev/null || fail "a plain frame is refused"
"$K" --index d/a idx.kol; "$K" -dc idx.kol | cmp - d/a || fail "an indexed file (IDX1) no longer decodes"

# 2. extract never writes outside its directory
echo safe > victim.txt
mkdir -p x1/d; ln -s ../../victim.txt x1/d/a                     # the last component is a symbolic link: replaced
(cd x1 && "$K" extract ../a.kol) || fail "extract over a symbolic link"
[ "$(cat victim.txt)" = safe ] || fail "extract wrote through a symbolic link"
[ ! -L x1/d/a ] && cmp x1/d/a d/a || fail "the symbolic link was not replaced by the file"
mkdir -p outside x2; ln -s ../outside x2/d                       # a directory on the way is a symbolic link: skipped
(cd x2 && expect 1 "$K" extract ../a.kol); grep -q "symbolic link" x2/err.txt || fail "symlinked directory message"
[ -z "$(ls outside)" ] || fail "extract wrote through a symbolic link to a directory"
mkdir x3; ln -s ../outside x3/d
(cd x3 && expect 1 "$K" extract ../a.kol d/a) ; [ -z "$(ls outside)" ] || fail "named extract"
"$MK" evil.kol ../evil1 /tmp/kolmo-evil2-$$ 'ok/../../evil3' good
mkdir x4; (cd x4 && expect 1 "$K" extract ../evil.kol)
[ ! -e evil1 ] && [ ! -e /tmp/kolmo-evil2-$$ ] && [ ! -e evil3 ] && [ ! -e x4/tmp ] || fail "an unsafe name was extracted"
[ "$(cat x4/good)" = evil ] || fail "the safe name next to them was not extracted"
expect 1 "$K" extract -C x4 evil.kol ../evil1; grep -q "unsafe" err.txt || fail "named unsafe extract"

# 3. archive and --index refuse an existing output without -f, and replace it with -f (staged, nothing left over)
echo precious > o.kol
expect 1 "$K" archive o.kol d; [ "$(cat o.kol)" = precious ] || fail "archive overwrote without -f"; grep -q "use -f" err.txt || fail "archive message"
"$K" archive -f o.kol d && "$K" contents o.kol | grep -q d/b || fail "archive -f"
echo precious > i.kol
expect 1 "$K" --index d/a i.kol; [ "$(cat i.kol)" = precious ] || fail "--index overwrote without -f"
"$K" --index -f d/a i.kol && "$K" -dc i.kol | cmp - d/a || fail "--index -f"
ln -s precious.txt l.kol; expect 1 "$K" archive l.kol d; [ ! -e precious.txt ] || fail "archive wrote through a symbolic link"
[ -z "$(ls -a | grep 'kolmo-')" ] || fail "a staged file was left behind"

# 4. what DDP1 cannot store is reported (exit 1, archive written); mtime restored; leading ../ and / removed; numbers checked
mkdir -p t/sub t/empty; echo hi > t/sub/f; touch -t 202001021200 t/sub/f; ln -s sub/f t/link; mkfifo t/fifo
expect 1 "$K" archive t.kol t
for w in "t/link: symbolic link" "t/fifo: FIFO" "t/empty: empty directory"; do grep -q "$w" err.txt || fail "not reported: $w"; done
"$K" contents t.kol | grep -q 't/sub/f' || fail "the regular file was not archived"
mkdir x5; "$K" extract -C x5 t.kol; [ "$(cat x5/t/sub/f)" = hi ] || fail "x5"
[ "$(date -r x5/t/sub/f +%Y%m%d)" = 20200102 ] || fail "mtime not restored"
mkdir -p up/in; echo top > up/top.txt; (cd up/in && "$K" archive ../../up.kol ../top.txt 2> ../../err.txt)
grep -q "removing leading '../'" err.txt || fail "no warning for ../"
"$K" contents up.kol | grep -q '  top.txt$' || fail "../ not removed from the stored name"
mkdir x6; "$K" extract -C x6 up.kol; [ "$(cat x6/top.txt)" = top ] || fail "extract of a stripped name"
for c in "extract -T abc a.kol" "archive -T abc z.kol d" "archive --chunk abc z.kol d" "--index -T abc d/a z.kol" "cat -T abc idx.kol"; do
    expect 2 "$K" $c; grep -q "got 'abc'" err.txt || fail "$c: message"
done
echo "ok: archive safety"
