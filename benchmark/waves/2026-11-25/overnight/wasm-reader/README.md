# B57 · Pinned-main WebAssembly reader

A memory-only build of the real pinned-main C decoder restored **all 357 frozen one-shot v12 frames in Node/V8 and Apple JavaScriptCore**, 714 byte-exact outputs in total. Each engine produced 47,614,545 original bytes. The module enforced a 768 MiB maximum. Both engines reached the same high-water mark of 709.44 MiB.

| Hard linear-memory limit | Node/V8 | JavaScriptCore | Observed result |
|---|---:|---:|---|
| 256 MiB | 355/357 | 355/357 | Two valid Ultra image frames returned −1. All successful output hashes matched. |
| 512 MiB, two images only | 1/2 | 1/2 | photo.pgm passed at 388.06 MiB. gray-big.pgm still returned −1. |
| 768 MiB | 357/357 | 357/357 | Largest high-water mark was 743,899,136 bytes on gray-big.pgm. |
| 1,024 MiB, two images only | 2/2 | 2/2 | Both original hashes matched at the same observed memory use as with 768 MiB. |

The failing 256 MiB inputs were gray-big.pgm.LU.kolmo (316,998 stored bytes, 786,448 decoded) and photo.pgm.LU.kolmo (90,468 stored bytes, 307,215 decoded). Both use block codec 17, the context-mixing image decoder. Fixed-format valid inputs became decodable with more available linear memory. The −1 error surfaces as KOLMO_ERR_CORRUPT through this public C API for these cases. This error alone cannot diagnose a damaged image. The observed peak is Wasm linear memory, excluding JavaScript runtime, JIT, filesystem, and host memory. None of the tested caps is a general upper bound for valid streams.

## Source, build and runtime

The recovered R28 work directory contains its original SIMD and scalar round-trip WASI executables, its experimental CLI, and a working Zig 0.17.1 compiler with Clang 22.1.8 and bundled WASI libc. Its report refers to historical commit 52df50cf5521003f8bbbf8355abf84398d6540d9. The scalar historical driver exports _start and memory, reads files through WASI, and encodes before its decode check. Its binary SHA-256 is f896349d52435b116132ef7b789d9809617b03b5282c42881ee6424a13db8801. The historical report and additional original hashes are recorded in provenance.json. No external download was needed.

The qualification used a fresh build of commit a1d39559686a9ec17c8614b9154a64d7b5b98672. The codec src tree and frozen fixtures matched that commit. A small reactor entry point calls kolmo_decompress on caller-owned buffers with one thread. The linker exports memory, the reactor initializer, malloc, free and restore_frame. No SIMD, native assembler, CLI, encoders or operating-system threads were exposed. The build uses -O3, -ffp-contract=off and section garbage collection. It reuses R28's no-op pthread_atfork/madvise stubs and prototype setjmp shim. That shim traps on GR6 error unwinding. Only known-good frozen inputs were exercised. Production error recovery, arbitrary inputs, browser deployment, streams and a supported release package remain open.

The host loaded each complete pinned frame into a fresh instance and retrieved its output through linear memory. Its minimal WASI binding refused every file-descriptor operation. The only host import actually invoked was environ_sizes_get, which supplied an empty environment. Both engines recorded 66 such calls per whole-corpus pass. No decoder call used host files, supplied clocks or a native decoder. The JS harness used host file I/O to read fixtures and write outputs; Python then hashed each engine's actual output and compared its exact byte count and SHA-256 with the frozen original.

The tests ran in Node 24.22.3 / V8 13.6.233.17-node.63 and JavaScriptCore 22625.2.8.11.9 on Apple M5/macOS 27.2. Each module is 720,472 bytes and starts with 37 unshared 64 KiB pages (2,424,832 bytes), including a 2 MiB stack. The harness parses the module's memory declaration and checks runtime growth against its cap. The 357 entries are the complete tests/v12/MANIFEST set of one-shot frames, including releases 0.0.1 and 0.0.2. The single separate CZNS stream in r0.0.2/frames.json uses a different API and was excluded. The complete fixture pin file digest and engine/module digests are in verified.json.

## Replay

The 256 MiB module SHA-256 is d701c72bdd3eaa2297e7ceec030d06967114b8ba38f4307a097cac8d75da31ca. The 768 MiB module SHA-256 is 2e7e6d4654ff770dca2551ed34ca92702b4af3fb3c11e876ce1e95c53cf5a076. Rebuilding with the included R28 shims reproduced both binaries byte for byte. These are research artifacts.

With a checkout at the specified commit and the recovered Zig executable, choose fresh output directories. The tested Node and jsc paths are the corresponding versioned local engines. Either bundled module can also be replayed directly.

```sh
sh build.sh "$SOURCE" "$ZIG" "$SCRATCH" "$OUT256" 268435456
sh build.sh "$SOURCE" "$ZIG" "$SCRATCH" "$OUT768" 805306368
python3 verify.py --module "$OUT256/reader.wasm" --node "$NODE" --jsc "$JSC" --corpus "$SOURCE/tests/v12" --out "$RESULTS256" --record-limits
python3 verify.py --module "$OUT768/reader.wasm" --node "$NODE" --jsc "$JSC" --corpus "$SOURCE/tests/v12" --out "$RESULTS768"
```

restores.csv and 768MiB/restores.csv identify every compressed frame and original, their sizes and SHA-256, the actual decoded SHA-256, return value and memory for each engine. The verified.json files record module import/export lists and actual host calls. cap-sweep.csv holds the targeted 512 MiB and 1 GiB runs, with their own binary hashes and recovered-output hashes. The initial 256 MiB reused-instance stop is retained in shared-instance-first-limit.log. The final matrices use a fresh instance per fixture so frames cannot affect each other's allocation results.
