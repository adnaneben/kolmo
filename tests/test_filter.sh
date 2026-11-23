#!/bin/sh
# Kolmo as a filter: tar -I kolmo, tar --use-compress-program, pipes, and the xz/gzip flags that scripts pass
# (docs/XZ-COMPAT.md).  Usage: sh tests/test_filter.sh build/kolmo
# GNU tar (tar on Linux, gtar on macOS when installed) and bsdtar (tar on macOS, bsdtar on Linux when installed) are
# each tested when present. KOLMO_FILTER_BIG=0 skips the 300 MB pipe (bounded memory above 256 MiB).
set -e
K=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; cd "$T"
mkdir bin; ln -s "$K" bin/kolmo; PATH=$T/bin:$PATH; export PATH
fail() { echo "FAIL: $*"; exit 1; }
ok() { echo "ok   $*"; }

# a small tree: text, numbers, an empty file, a subdirectory
mkdir -p src/sub
awk 'BEGIN { for (i = 0; i < 20000; i++) printf "%d,line %d of the filter test,%.3f\n", i, i % 97, i / 7 }' > src/a.csv
awk 'BEGIN { srand(5); for (i = 0; i < 50000; i++) printf "%c", 32 + int(rand() * 90) }' > src/sub/b.txt
: > src/empty
cat src/a.csv > in.bin; cat src/sub/b.txt >> in.bin

# ---- tar ---------------------------------------------------------------------------------------------------------
gnutar=; bsdtar=
for t in tar gtar; do
    if command -v $t > /dev/null 2>&1 && $t --version 2> /dev/null | grep -q 'GNU tar'; then gnutar=$t; break; fi
done
for t in bsdtar tar; do
    if command -v $t > /dev/null 2>&1 && $t --version 2> /dev/null | grep -q 'bsdtar'; then bsdtar=$t; break; fi
done
[ -n "$gnutar$bsdtar" ] || fail "neither GNU tar nor bsdtar found"
if [ -n "$gnutar" ]; then
    $gnutar -I kolmo -cf g.tar.kolmo src || fail "GNU tar -I kolmo -c"
    $gnutar -I kolmo -tf g.tar.kolmo | grep -q 'src/sub/b.txt' || fail "GNU tar -I kolmo -t"
    $gnutar -I 'kolmo -6' -cf g6.tar.kolmo src || fail "GNU tar -I 'kolmo -6' -c"
    mkdir gx; $gnutar --use-compress-program=kolmo -xf g6.tar.kolmo -C gx || fail "GNU tar --use-compress-program=kolmo -x"
    diff -r src gx/src > /dev/null || fail "GNU tar round trip differs"
    $gnutar -I 'kolmo -T1 -9' -cf - src | kolmo -dc | $gnutar -tf - | grep -q 'src/a.csv' || fail "GNU tar to a pipe"
    ok "GNU tar ($gnutar): -I kolmo, -I 'kolmo -6', --use-compress-program=kolmo"
fi
if [ -n "$bsdtar" ]; then
    # bsdtar runs the program as given in both directions (it adds no -d): reading needs 'kolmo -d'
    $bsdtar --use-compress-program=kolmo -cf b.tar.kolmo src || fail "bsdtar --use-compress-program=kolmo -c"
    $bsdtar --use-compress-program='kolmo -6' -cf b6.tar.kolmo src || fail "bsdtar 'kolmo -6' -c"
    $bsdtar --use-compress-program='kolmo -d' -tf b.tar.kolmo | grep -q 'src/sub/b.txt' || fail "bsdtar 'kolmo -d' -t"
    mkdir bx; $bsdtar --use-compress-program='kolmo -d' -xf b6.tar.kolmo -C bx || fail "bsdtar 'kolmo -d' -x"
    diff -r src bx/src > /dev/null || fail "bsdtar round trip differs"
    kolmo -dc b.tar.kolmo | $bsdtar -tf - | grep -q 'src/a.csv' || fail "kolmo -dc | bsdtar -t"
    ok "bsdtar ($bsdtar): --use-compress-program=kolmo / 'kolmo -d'"
fi

# ---- pipes -------------------------------------------------------------------------------------------------------
cat in.bin | kolmo | kolmo -d | cmp - in.bin || fail "cat | kolmo | kolmo -d"
kolmo < in.bin > p.kolmo; kolmo -c in.bin | cmp - p.kolmo || fail "a pipe and a file give different frames"
kolmo -dc - < p.kolmo | cmp - in.bin || fail "-dc -"
kolmo -cd < p.kolmo | cmp - in.bin || fail "-cd"
kolmo -d < p.kolmo | cmp - in.bin || fail "-d from standard input"
cat p.kolmo | kolmo --decompress --stdout | cmp - in.bin || fail "--decompress --stdout"
printf '' | kolmo | kolmo -d | cmp - /dev/null || fail "empty input"
kolmo -c in.bin in.bin | kolmo -d | cmp - "$(cat in.bin in.bin > in2.bin; echo in2.bin)" || fail "-c of two files: concatenated frames"
kolmo --stream=1 -c in.bin | kolmo -d | cmp - in.bin || fail "--stream through a pipe"
kolmo --stream=1 -c in.bin | kolmo -t || fail "-t of a stream from a pipe"
kolmo -t < p.kolmo || fail "-t from standard input"
ok "pipes: cat | kolmo | kolmo -d, -dc -, -cd, --decompress --stdout, empty input, streams"

# ---- the xz/gzip flags scripts pass (docs/XZ-COMPAT.md) -------------------------------------------------------------
for f in -0 -1 -6 -9 -9e -6e -e --fast --best --extreme -T0 "-T 1" -T1 --threads=2 "--memory=2G" "--memlimit=2G" -M2G \
         -q -qq -v --no-name -k --keep -z --compress; do
    kolmo $f -c < in.bin > f.kolmo 2> /dev/null || fail "kolmo $f -c"
    kolmo -dc f.kolmo | cmp - in.bin || fail "kolmo $f round trip"
done
kolmo -0 -c in.bin | cmp - "$(kolmo -1 -c in.bin > l1.kolmo; echo l1.kolmo)" || fail "-0 is level 1"
kolmo -9e -c in.bin | cmp - "$(kolmo -9 -c in.bin > l9.kolmo; echo l9.kolmo)" || fail "-9e is level 9"
kolmo --best -c in.bin | cmp - l9.kolmo || fail "--best is level 9"
kolmo --fast -c in.bin | cmp - l1.kolmo || fail "--fast is level 1"
kolmo --uncompress --to-stdout l9.kolmo | cmp - in.bin || fail "--uncompress --to-stdout"
kolmo -dcf in.bin | cmp - in.bin || fail "-dcf passes data that is not Kolmo's through (file)"
cat in.bin | kolmo -dcf | cmp - in.bin || fail "-dcf passes data that is not Kolmo's through (pipe)"
cat l9.kolmo | kolmo -dcf | cmp - in.bin || fail "-dcf still decodes Kolmo frames"
cp in.bin k.bin; kolmo k.bin; [ -f k.bin ] && [ -f k.bin.kolmo ] || fail "kolmo FILE keeps FILE (the default)"
rm k.bin; kolmo -d k.bin.kolmo; cmp k.bin in.bin || fail "kolmo -d FILE.kolmo"
kolmo -f k.bin; kolmo -t k.bin.kolmo || fail "-t FILE"
kolmo -l k.bin.kolmo | grep -q 'k.bin.kolmo' || fail "-l FILE"
kolmo --rm -f k.bin; [ ! -e k.bin ] && [ -f k.bin.kolmo ] || fail "--rm removes the input, as xz and gzip do by default"
ok "flags: -0 -9e --fast --best --extreme -T -M --memory --no-name -k --rm -z -t -l -dcf --uncompress --to-stdout"

# ---- exit status --------------------------------------------------------------------------------------------------
rc() { set +e; "$@" > /dev/null 2>&1; r=$?; set -e; echo $r; }
[ "$(rc sh -c 'printf junk | kolmo -d')" = 1 ] || fail "corrupt input from a pipe: exit 1"
[ "$(rc sh -c 'head -c 40 l9.kolmo | kolmo -d')" = 1 ] || fail "truncated input from a pipe: exit 1"
[ "$(rc kolmo -dc missing.kolmo)" = 1 ] || fail "missing file: exit 1"
[ "$(rc kolmo --no-such-option)" = 2 ] || fail "unknown option: exit 2"
[ "$(rc kolmo -10 -c in.bin)" = 2 ] || fail "-10: exit 2"
# a reader that stops early ends the writer with SIGPIPE, as for xz and gzip (no error message, no hang)
cat in.bin in.bin in.bin in.bin in.bin in.bin in.bin in.bin > big.bin
kolmo -1 -c big.bin | kolmo -d 2> sp.err | head -c 10 > /dev/null
[ ! -s sp.err ] || fail "early close: kolmo -d printed $(cat sp.err)"
# a stream decoder that stops before its pipe ends (data after the end, or a damaged segment) exits 0 or 1, never by
# the SIGPIPE of the thread that feeds it
kolmo --stream=1 -1 -c big.bin > s.kolmo
[ "$(rc sh -c 'cat s.kolmo big.bin big.bin | kolmo -d > s.out')" = 0 ] && cmp s.out big.bin || fail "stream + trailing data from a pipe"
python3 -c "import sys; d = bytearray(open('s.kolmo','rb').read()); d[len(d)//2] ^= 0xff; open('s.bad','wb').write(d)"
[ "$(rc sh -c 'cat s.bad big.bin big.bin | kolmo -d')" = 1 ] || fail "damaged stream from a pipe: exit 1"
ok "exit status: 1 corrupt/truncated/missing, 2 usage; SIGPIPE when the reader stops"

# ---- signals: SIGTERM while the output is staged removes the private file -------------------------------------------
mkfifo fifo
kolmo -o sig.kolmo < fifo & pid=$!
exec 3> fifo                        # the writer end: kolmo has its output staged and waits for input
i=0; while [ -z "$(ls -A . | grep '^\.kolmo-')" ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i + 1)); done
[ -n "$(ls -A . | grep '^\.kolmo-')" ] || fail "no staged output appeared"
kill -TERM $pid; set +e; wait $pid; r=$?; set -e; exec 3>&-
[ $r = 143 ] || fail "SIGTERM: exit $r, expected 143"
[ -z "$(ls -A . | grep '^\.kolmo-')" ] || fail "SIGTERM left the staged file behind"
[ ! -e sig.kolmo ] || fail "SIGTERM published a partial output"
ok "SIGTERM removes the staged output"

# ---- a pipe larger than 256 MiB: 256 MiB stream segments (bounded memory), still one command each way -------------
if [ "${KOLMO_FILTER_BIG:-1}" != 0 ]; then
    awk 'BEGIN { for (i = 0; i < 2000; i++) printf "%06d some repeated text for a big pipe\n", i }' > blk
    i=0; while [ $i -lt 12 ]; do cat blk blk > blk2; mv blk2 blk; i=$((i + 1)); done     # 78 KB * 2^12: 320 MB
    cat blk | kolmo -1 > big.kolmo
    [ "$(head -c 4 big.kolmo)" = CZNS ] || fail "a pipe over 256 MiB is not streamed"
    cat big.kolmo | kolmo -d | cmp - blk || fail "a big pipe round trip"
    head -c 1000000 blk | kolmo -1 | head -c 4 | grep -q CZN1 || fail "a small pipe is not one frame"
    ok "a 320 MB pipe: stream of 256 MiB segments, round trip through pipes"
fi
echo "filter tests passed"
