#!/bin/sh
# Lists the external symbols of the vendored liblzma subset (built without the namespace header) for
# vendor/xz/kolmo_lzma_namespace.h. usage: sh benchmark/rc3/deps/gen_lzma_namespace.sh  (from the repository root)
set -e
D=$(mktemp -d)
XI="-DHAVE_CONFIG_H -Ivendor/xz -Ivendor/xz/liblzma/api -Ivendor/xz/liblzma/common -Ivendor/xz/liblzma/check -Ivendor/xz/liblzma/lz -Ivendor/xz/liblzma/lzma -Ivendor/xz/liblzma/rangecoder -Ivendor/xz/liblzma/simple -Ivendor/xz/liblzma/delta -Ivendor/xz/common"
for f in $(find vendor/xz -name '*.c'); do ${CC:-cc} -O2 $XI -c "$f" -o "$D/$(basename "$f" .c).o"; done
nm "$D"/*.o | awk '$2 ~ /^[TDRBC]$/ {print $3}' | sed 's/^_//' | sort -u | awk '{printf "#define %s kolmo_%s\n", $1, $1}'
rm -rf "$D"
