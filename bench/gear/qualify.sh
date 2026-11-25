#!/usr/bin/env bash
# Two exact revisions, three fresh builds, one host. An optional fifth argument supplies a verified cached enwik8.zip.
set -euo pipefail
candidate=$1
baseline=$2
work=$3
out=$4
cached=${5:-}
pin=a1d39559686a9ec17c8614b9154a64d7b5b98672
revision=$(git -C "$candidate" rev-parse HEAD)
test "$revision" = "${GITHUB_SHA:-$revision}"
test "$(git -C "$baseline" rev-parse HEAD)" = "$pin"
mkdir "$work" "$out"
mkdir "$work"/{asm,c,main,input}
for variant in asm c main; do
  if [[ $variant == main ]]; then repo=$baseline; sha=$pin; else repo=$candidate; sha=$revision; fi
  git -C "$repo" archive "$sha" | tar -xf - -C "$work/$variant"
done
cc=${CC:-cc}
build() {
  local variant=$1 log=$2
  shift 2
  make -C "$work/$variant" -j2 CC="$cc" "$@" > "$out/$log" 2>&1 || { tail -100 "$out/$log"; return 1; }
}
build main build-main.log build/kolmo
build c build-c.log GEAR_ASM=0 build/kolmo
build asm build-asm-parity.log GEAR_ASM=1 build/kolmo check-asm
case $(uname -m) in
  x86_64) kernel=kolmo_gear_x86_64 ;;
  arm64|aarch64) kernel=kolmo_gear_aarch64 ;;
  *) echo "Unsupported native comparison architecture" >&2; exit 2 ;;
esac
for variant in c asm; do nm -u "$work/$variant/build/kolmo.o" > "$out/undefined-$variant.txt"; done
grep -Eq "(^|[[:space:]])_?${kernel}$" "$out/undefined-asm.txt"
if grep -Eq "(^|[[:space:]])_?${kernel}$" "$out/undefined-c.txt"; then
  echo "C control unexpectedly references the assembly scanner" >&2
  exit 1
fi
test "$(grep -c 'Gear scanner: 28,784 matching positions and hash states' "$out/build-asm-parity.log")" -eq 2
test "$(grep -c 'Gear scanner: 257 guard-page tails passed' "$out/build-asm-parity.log")" -eq 2
{
  uname -a
  "$cc" --version
  if command -v lscpu >/dev/null; then lscpu; fi
  if [[ -r /proc/cpuinfo ]]; then head -30 /proc/cpuinfo; fi
} > "$out/environment.txt"

source_url=https://mattmahoney.net/dc/enwik8.zip
input="$work/input/enwik8.zip"
if [[ -n $cached ]]; then
  cp "$cached" "$input"
  origin=verified-local-cache
elif curl --fail --silent --show-error --location --retry 2 --connect-timeout 8 --max-time 90 --max-filesize 40000000 "$source_url" --output "$input"; then
  origin=$source_url
else
  input="$work/c/tests/v12/r0.0.2/mixed.bin.L3.kolmo"
  origin=tracked-fallback
fi
python3 "$work/c/bench/gear/receipt.py" prepare "$work" "$out" "$revision" "$pin" "$input" "$origin"
python3 "$work/c/benchmark/waves/2026-11-25/native-gear/replay.py" \
  --main "$work/c/build/kolmo" --asm "$work/asm/build/kolmo" --paired-c "$work/main/build/kolmo" \
  --input "$input" --threads 4 --rounds 11 --out "$work/legacy-rows.csv"
python3 "$work/c/bench/gear/receipt.py" score "$work" "$out"
