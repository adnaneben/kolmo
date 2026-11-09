# Kolmo benchmark reproduction

Run from the Kolmo repository on macOS. The adapter links the original Kolmo shared library separately from lzbench's codec implementations, preserving Kolmo's vendored Brotli 1.1.0 while comparing with lzbench's Brotli 1.2.0. Mach-O two-level namespaces keep these separate. Do not replace this with static archive linking: overlapping codec symbols would substitute backends.

```sh
make -B -j10
make -j10 lib
./build/czntest

git clone https://github.com/inikep/lzbench.git /tmp/kolmo-lzbench-audit
# Reproduce the exact tested revision before applying the adapter:
git -C /tmp/kolmo-lzbench-audit checkout d68cdbc2309408e82da881c61d23cf70cca8db09
python3 benchmark/audit/prepare_lzbench.py /tmp/kolmo-lzbench-audit
make -C /tmp/kolmo-lzbench-audit -j10 BUILD_ONLY='KOLMO LZ4 ZSTD BROTLI LZMA BSC PPMD ZPAQ OPENZL' MOREFLAGS='-mcpu=native'

mkdir -p benchmark/audit/data
curl -L --fail https://sun.aei.polsl.pl/~sdeor/corpus/silesia.zip -o benchmark/audit/data/silesia.zip
unzip -q benchmark/audit/data/silesia.zip -d benchmark/audit/data/silesia
curl -L --fail https://raw.githubusercontent.com/owid/co2-data/master/owid-co2-data.csv -o benchmark/audit/data/owid-co2-data.csv

/tmp/kolmo-lzbench-audit/lzbench -ekolmo,0,1,2,3/lz4/zstd,1,3,9,19/brotli,8 -t0,0 -i3,3 -p3 -o4 -q benchmark/audit/data/silesia/* benchmark/audit/data/owid-co2-data.csv > benchmark/audit/lzbench-single.csv 2> benchmark/audit/lzbench-single.stderr
python3 benchmark/audit/summarize.py benchmark/audit/lzbench-single.csv
```

Modes 0 through 5 are rt, fast, eco, hc, max and ultra. Measurements use the median of at least three compression and decompression iterations, one internal CPU thread, independent files, default Kolmo block sizes, and no GPU. Large slow settings use a separately labelled one-compression-iteration run. All source bytes, headers and checksums remain included in compressed sizes. lzbench verifies decompressed output against the input.

`Ratio` in lzbench CSV means compressed percent, lower is better. The report instead uses original bytes divided by compressed bytes, higher is better. Aggregate throughput is total source bytes divided by the sum of each file's elapsed time inferred from its measured throughput; it is not the arithmetic mean of per-file speeds. The corpus comprises all twelve Silesia files, totaling 211,938,580 bytes. The downloaded OWID snapshot is pinned by its SHA-256 in manifest.json; upstream data may change, so a fresh download must be hash-checked before exact comparisons.

The complete suite must finish or be paused, and compilation must finish before timings. Do not run CPU-heavy work alongside measurements. macOS chooses performance and efficiency cores; these results are local measurements without CPU affinity, fixed clocks or control of other applications. Repeat whole runs in randomized codec order on AC power before publishing a leaderboard claim.

## Archive and scaling runs

```sh
/tmp/kolmo-lzbench-audit/lzbench -ekolmo,4,5/brotli,11/lzma,6/ppmd8,6/bsc1/openzl_generic -t0,0 -i1,1 -p3 -o4 -q benchmark/audit/data/silesia/* benchmark/audit/data/owid-co2-data.csv > benchmark/audit/lzbench-archive.csv 2> benchmark/audit/lzbench-archive.stderr
python3 benchmark/audit/run_scaling.py /tmp/kolmo-lzbench-audit/lzbench
KOLMO_LIB="$PWD/build/libkolmo.dylib" python3 benchmark/audit/check_concurrency.py
```

Archive runs request a minimum of one compression and one decompression iteration, so their speeds are exploratory. Compressed sizes are exact. The OpenZL implementation here is the 0.2.3 version bundled by the pinned lzbench, using its generic preset. It is not a trained per-format comparison or a comparison with current OpenZL 0.3.0, and cannot settle a claim against that release. LZMA is the lzbench 7-Zip LZMA SDK backend, not the xz CLI or a complete 7-Zip archive. No ZPAQ result is implied merely by including it in the build.

Scaling uses Kolmo's `kolmo1m` adapter entry and internal `-I1`, `-I4` or `-I10` workers on whole files; competitors use external `-T1`, `-T4` or `-T10` pools and `-b1024`. Kolmo keeps row-aware boundaries and complete container overhead, so block boundaries and framing are not identical. Each timed setting uses three median iterations. See `run_scaling.py` for the precise input set and calls.

Use `python3 benchmark/audit/check_concurrency.py` only after the timing run. It is a compatibility metadata probe: the current decoder may pass every round trip even when a stream advertises the wrong format version.

## Correctness and memory checks

```sh
./build/czntest > benchmark/audit/native-tests.txt
KOLMO_LIB="$PWD/build/libkolmo.dylib" python3 bindings/python/test_kolmo.py > benchmark/audit/python-tests.txt
./build/kolmo -m fast -T 10 -S 2 benchmark/audit/data/owid-co2-data.csv /tmp/kolmo-audit-stream.czs
./build/kolmo -d -T 10 /tmp/kolmo-audit-stream.czs /tmp/kolmo-audit-stream.out
cmp benchmark/audit/data/owid-co2-data.csv /tmp/kolmo-audit-stream.out
python3 benchmark/audit/measure_memory.py > benchmark/audit/memory.json
python3 benchmark/audit/verify_results.py

clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -DZSTD_DISABLE_ASM -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include -I/opt/homebrew/include fuzz/fuzz_decompress.c fuzz/run_local.c src/czn.c build/libvendor.a -L/opt/homebrew/lib -llzma -lpthread -lm -lc++ -o /tmp/kolmo-fuzz-decompress-audit
./build/kolmo -m fast -T 1 README.md /tmp/kolmo-audit-readme.czn
ASAN_OPTIONS=detect_leaks=0 /tmp/kolmo-fuzz-decompress-audit 20000 /tmp/kolmo-audit-readme.czn
clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -DZSTD_DISABLE_ASM -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include -I/opt/homebrew/include fuzz/fuzz_roundtrip.c fuzz/run_local.c src/czn.c build/libvendor.a -L/opt/homebrew/lib -llzma -lpthread -lm -lc++ -o /tmp/kolmo-fuzz-roundtrip-audit
ASAN_OPTIONS=detect_leaks=0 /tmp/kolmo-fuzz-roundtrip-audit 300
```

On macOS, `ru_maxrss` is reported in bytes. The child maximum is cumulative within the measurement helper, so fast runs first in a fresh helper process; ultra's larger value establishes its own higher maximum. Memory probes overlapped correctness checks during this audit; no throughput claim uses those elapsed times.
