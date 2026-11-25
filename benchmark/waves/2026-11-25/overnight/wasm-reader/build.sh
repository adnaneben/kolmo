#!/bin/sh
# Build in a disposable working directory; leave the original R28 toolchain and Kolmo checkout intact.
set -eu
src=$1
zig=$2
work=$3
out=$4
max_bytes=${5:-268435456}
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$work" "$out"
cd "$src"
export ZIG_GLOBAL_CACHE_DIR="$work/zig-cache"
"$zig" cc -target wasm32-wasi -mexec-model=reactor -O3 -ffp-contract=off \
    -DKOLMO_NO_ASM -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS \
    -I"$here/r28-shims" -Isrc -ffunction-sections -fdata-sections \
    "$here/reader.c" "$here/r28-shims/wasi_stubs.c" src/kolmo.c src/arch/cpu.c \
    src/algo/predict.c src/algo/planes.c src/algo/mixer.c src/algo/video.c \
    src/algo/lzp.c src/algo/qlfc.c src/algo/par.c src/algo/sais/*.c src/codecs/bwt_stream.c \
    src/algo/khuf.c src/algo/krans.c src/algo/klz.c src/algo/klzma.c src/algo/khash.c \
    -lwasi-emulated-mman -lwasi-emulated-signal -lwasi-emulated-process-clocks \
    -Wl,--gc-sections -Wl,--export=malloc -Wl,--export=free -Wl,--export=restore_frame \
    -Wl,-z,stack-size=2097152 -Wl,--max-memory="$max_bytes" -Wl,--strip-all -o "$out/reader.wasm"
