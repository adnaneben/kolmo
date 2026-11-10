# Kolmo 1.1.0 on the whole Silesia board tar

The executable and library are built from GitHub commit `87dfc86b2665cac637c98179fb8afa8db3eeb668`, whose VERSION file contains `1.1.0`. GitHub listed public releases and tags through v1.0.1 when this source was verified on November 10, 2026. [Pinned source](https://github.com/adnaneben/kolmo/tree/87dfc86b2665cac637c98179fb8afa8db3eeb668) · [Release history](https://github.com/adnaneben/kolmo/releases)

The archived tar is 211,947,520 bytes with SHA-256 `a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2`. The local Apple M5 runs one lzbench worker and uses one internal worker. The original frozen harness, release CLI, loaded library, bridge and sample hashes are included in the compressed receipt. The harness verifies decompression. The normal rows use the medians of three encoding and five decoding iterations. A row named `ultra-screen` uses one full encoding and one full decoding iteration. The raw `Ratio` field in the harness is percent of original bytes. The normalized `ratio` field in each receipt is original bytes divided by compressed bytes. Speeds are decimal MB/s.

| CLI default mode | Frozen harness setting | Official 1.1.0 ladder |
|---|---:|---:|
| rt | 0 | 0 |
| fast | 1 | 4 |
| eco | 2 | 8 |
| hc | 3 | 9 |
| max | 4 | 10 |
| ultra | 5 | 12 |

The frozen harness dynamically imports the historical `czn_compress` and `czn_decompress` names. A two-function bridge validates default mode parameters and forwards them to the release library's `kolmo_compress` and `kolmo_decompress` entry points. The release library copy keeps its original SHA-256. The six CLI modes and harness settings must produce the same size on the same 64 KiB fixture and restore every input byte. Existing harness labels such as `kolmo audit -1` remain in raw stdout for audit. [Official 1.1.0 ladder](https://github.com/adnaneben/kolmo/blob/87dfc86b2665cac637c98179fb8afa8db3eeb668/benchmark/lzbench/kolmo_lzbench.cpp)

To replay the same Mac harness in a fresh research checkout, supply the unchanged `build/frontier/frozen/lzbench`, `build/frontier/samples/dickens`, and the board tar. Prepare a new source directory from the pinned Git object, then build the release CLI and shared library. The replay uses the same `build/frontier/timing.lock` as the other benchmark jobs.

```sh
mkdir -p build/frontier/github-110
git archive 87dfc86b2665cac637c98179fb8afa8db3eeb668 | tar -xf - -C build/frontier/github-110
make -C build/frontier/github-110 build/kolmo lib
python3 benchmark/frontier/v110-lz/replay.py prepare /path/to/silesia.tar
python3 benchmark/frontier/v110-lz/replay.py short /path/to/silesia.tar
python3 benchmark/frontier/v110-lz/replay.py slow /path/to/silesia.tar
python3 benchmark/frontier/v110-lz/replay.py ultra-screen /path/to/silesia.tar
```

For a clean independent build on Linux, use the release's official `benchmark/lzbench/setup.sh` with a fresh checkout of `inikep/lzbench` and the ladder in the rightmost column. Run `lzbench -ekolmo,0,4,8,9,10/lz4/zstd,3/openzl_generic -I1 -T1 -t0,0 -i3,5 -p3 -o4 -q /path/to/silesia.tar`, then run `-ekolmo,12` separately with `-i1,1` for the ultra screening. A new build or different CPU will give a different binary fingerprint and may give different speeds. [Official build instructions](https://github.com/adnaneben/kolmo/blob/87dfc86b2665cac637c98179fb8afa8db3eeb668/benchmark/lzbench/README.md) · [Upstream revision used for this Mac harness](https://github.com/inikep/lzbench/tree/d68cdbc2309408e82da881c61d23cf70cca8db09)

The optional `ultra` phase collects a separate three/five-iteration row. Each completed row is atomically incorporated into `replay.json.gz`, which includes the original harness output and flags. The upstream November 10 cloud Silesia CSVs use a tar of 211,957,760 bytes. Keep the two tar identities and measurements separate. [Upstream measurement protocol](https://github.com/adnaneben/kolmo/blob/87dfc86b2665cac637c98179fb8afa8db3eeb668/benchmark/lzbench/results/README.md)

The public lzbench 2.4 table uses a 211,947,520-byte Silesia tar. Its smallest entry on November 10 was ZPAQ 7.15 level 5 at 40,470,202 compressed bytes, followed by Kanzi level 9 at 41,809,254. Speeds in that table were measured on an AMD EPYC 9555P under Linux. ZPAQ enables its JIT on x86 and uses its portable interpreter on the local Apple M5. Use the matching Mac measurements from the local panel when comparing timings here. [Official ratio table](https://github.com/inikep/lzbench/blob/master/doc/lzbench24_sorted.md) · [lzbench codec build notes](https://github.com/inikep/lzbench#list-of-compressors)

On that archive, the local 1.1.0 ultra screening produced 36,061,471 bytes (5.8774×), 10.89% smaller than ZPAQ's published output. Its single verified encode/decode pair measured 0.85 and 0.81 MB/s. The local max mode produced 40,996,303 bytes at 7.20 and 115.50 MB/s across the standard three/five iterations. [Raw measurements](replay.json.gz)
