#!/bin/sh
# Compile the shipping library with intrinsic headers unavailable. Output parity alone
# cannot detect that both builds called the same hardware kernel.
set -eu
headers=$(mktemp -d)
trap 'rm -rf "$headers"' EXIT
for name in arm_neon.h arm_acle.h immintrin.h emmintrin.h nmmintrin.h; do
    echo '#error "intrinsic header included in the NO_ASM library"' > "$headers/$name"
done
cat > "$headers/require_noasm.h" <<'END'
#ifndef KOLMO_NO_ASM
#error "run make NO_ASM=1 check-noasm"
#endif
END
"$@" -I"$headers" -include "$headers/require_noasm.h" -fsyntax-only src/kolmo.c
