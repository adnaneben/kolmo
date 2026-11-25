#!/bin/sh
# Run from an Apple command-line-tools checkout against the absolute source tree being assessed.
set -eu
repo=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
uname -sm
clang --version | sed -n '1,2p'
printf 'source='; git -C "$repo" rev-parse HEAD
for tool in x86_64-w64-mingw32-gcc x86_64-w64-mingw32-clang clang-cl wine wine64; do
  command -v "$tool" || printf '%s unavailable\n' "$tool"
done
cat > "$tmp/abi.c" <<'C'
#include "kolmo.h"
int abi_probe(const kolmo_params *params) { return params->threads; }
C
clang --target=x86_64-w64-windows-gnu -ffreestanding -I"$repo/src" -c "$tmp/abi.c" -o "$tmp/public-header.obj"
file "$tmp/public-header.obj" | sed "s|$tmp/||"
printf '\nNormal hosted compilation of the product translation unit\n'
if clang --target=x86_64-w64-windows-gnu -I"$repo/src" -c "$repo/src/kolmo.c" -o "$tmp/kolmo.obj" > "$tmp/product.log" 2>&1; then
  echo 'Translation unit compiled'
else
  sed "s|$repo/||g" "$tmp/product.log" | head -12
fi
printf '\nMinimal UCRT hosted header probe\n'
printf '#include <stdio.h>\nint main(void) { return puts("ucrt") < 0; }\n' > "$tmp/ucrt.c"
if clang --target=x86_64-w64-windows-gnu "$tmp/ucrt.c" -o "$tmp/ucrt.exe" > "$tmp/ucrt.log" 2>&1; then
  file "$tmp/ucrt.exe"
else
  sed "s|$tmp/||g" "$tmp/ucrt.log" | head -8
fi

printf '\nExplicit UCRT import-library link\n'
lld=$(rustc --print sysroot)/lib/rustlib/aarch64-apple-darwin/bin/rust-lld
if test -x "$lld"; then
  if "$lld" -flavor link /dll /noentry /nodefaultlib /export:abi_probe /out:"$tmp/abi.dll" "$tmp/public-header.obj" ucrt.lib > "$tmp/link.log" 2>&1; then
    file "$tmp/abi.dll"
  else
    cat "$tmp/link.log"
  fi
fi
