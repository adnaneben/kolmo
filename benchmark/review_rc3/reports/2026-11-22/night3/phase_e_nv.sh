#!/bin/bash
# Phase E sequence. Each step via run_logged (own log dir, real exit code). Continues after failures; summary at end.
V="$1"; TAG="$2"; P="$V/validate-portable-$TAG"; CR="$V/validate-c-reference-$TAG"; E="$V/evidence"; L="$V/logs/phase-e-$TAG"
RL(){ python3 -I "$V/tools/run_logged.py" "$L" "$@"; echo "$? $*" >> "$L.summary"; }
mkdir -p "$P/build" "$CR/build"
RL build-all --cwd "$P" --timeout 3600 -- make -j4 ARCH= all
RL check --cwd "$P" --timeout 7200 -- make -j1 check
RL check-rc --cwd "$P" --timeout 14400 -- make -j1 ARCH= check-rc
RL check-sanitizers --cwd "$P" --timeout 14400 -- make -j1 ARCH= check-sanitizers
RL check-dna-cm-tar --cwd "$P" --timeout 7200 -- make -j1 ARCH= check-frontier-dna check-cm-compact check-tar-files
RL experimental --cwd "$P" --timeout 14400 -- make -j1 ARCH= experimental-check kfam-check kaud-check kbin-check GPU_CHECK=
RL lib --cwd "$P" --timeout 3600 -- make -j4 ARCH= lib
shasum -a 256 "$P/build/libkolmo.dylib" > "$L.libhash"
RL py-binding --cwd "$P" --env KOLMO_LIB="$P/build/libkolmo.dylib" --timeout 3600 -- python3 bindings/python/test_kolmo.py
RL py-concurrent-dict --cwd "$P" --env KOLMO_LIB="$P/build/libkolmo.dylib" --timeout 3600 -- python3 tests/test_concurrent_dict.py
RL cargo-test --cwd "$P" --env CARGO_TARGET_DIR="$P/build/cargo-target" --timeout 3600 -- cargo test --manifest-path bindings/rust/Cargo.toml
RL determinism-portable --cwd "$P" --timeout 7200 -- python3 benchmark/determinism/check.py build/kolmo "$E/determinism-portable-$TAG"
for b in python rust; do RL "bindings-diff-$b" --cwd "$P" -- sh -c "diff -r -x experimental src bindings/$b/native/src && for f in src/experimental/frontier_*; do cmp \"\$f\" \"bindings/$b/native/\$f\" || exit 1; done"; done
RL cref-build --cwd "$CR" --timeout 3600 -- make -j4 ARCH= NO_ASM=1 build/kolmo build/test-dispatch build/test-asm
RL cref-test-dispatch --cwd "$CR" --timeout 3600 -- ./build/test-dispatch
RL cref-test-asm --cwd "$CR" --timeout 3600 -- ./build/test-asm
RL determinism-c-reference --cwd "$CR" --timeout 7200 -- python3 benchmark/determinism/check.py build/kolmo "$E/determinism-c-reference-$TAG"
echo "PHASE_E_DONE $TAG"; cat "$L.summary"
