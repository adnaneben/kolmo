# Wave 4, entropy lane: Kolmo's own entropy coders under every tier

Branch `claude/w4-entropy-20261118`, from rc3 `5071f38` with `claude/rc3w3-decmap-20261118` merged (`b61b805`, the
baseline "A" of every A/B below). The owner's goals for this lane: every entropy coder Kolmo uses should beat the
competition in speed and/or ratio without much more memory, and Kolmo should stop depending on libraries it competes
with. Scope: khuf and the Huffman leaves (codecs 13, 21-23, 27), rANS/tANS, QLFC (codec 18, `ratio`), and the binary
arithmetic coders.

**Summary against the targets** (VM, x86-64; Apple M5 numbers come from the integration session):

| item | target | result |
|---|---|---|
| khuf / Huffman leaves | ≥ 1.1× HUF decode at ≤ +0.1% size, so codec 13's HUF can go | khuf v2: smaller than HUF on 12 of 14 inputs (+0.006% worst); decode 1.03-1.76× on Silesia (geometric mean 1.22×), 1.01× and 1.18× on Kolmo's own leaf streams; met on 11 of 14 inputs. **The graph leaves no longer call zstd's HUF** (codecs 27 and 30 are the default; 13 and 22 stay readable) |
| rANS / tANS | own interleaved rANS where smaller | measured, not built: FSE and Kolmo's rANS are 3-14% smaller than any Huffman coder on the leaf streams at a third of the speed: a wide rANS is the leaves' largest open win |
| QLFC (`ratio`) | decode ≥ bsc -m0 -e2 at the same or smaller size | 1 thread: −9.7% instructions per coder block, decode 0.90-1.73× bsc's speed (enwik8 1.10×, webster 1.08×, dickens 1.01×, samba 1.73×; nci 0.94×, xml 0.92×, reymont 0.90×); 4 threads (four coder parts per 4-16 MiB block): 0.51-1.30× (samba 1.30×, nci 1.14×, enwik8 1.05×, reymont 1.04× (was 0.38×), webster 1.02×, dickens 0.95× (was 0.59×), xml 0.51×); every file 0.15-23% smaller than bsc except samba (+1.7%, unchanged). Speed met on 5 of 8 files at 1 thread and 6 of 8 at 4 threads (logs.log 3.3× and 1.9×) |
| binary arithmetic coders (CM, GR6+, KLZMA, audio/image) | shared branch-light coder if measurably faster | not attempted; the QLFC analysis (instruction-bound at IPC ≈ 5, model updates ≫ coder) suggests the model side, not the coder, bounds them too |

Memory: decode peak RSS equal to the baseline everywhere; against bsc it is equal on text up to 10 MB, 1.6x on
webster (210 vs 131-139 MB) and 3.2x at 1 thread on enwik8 (496 vs 155 MB; 1.04x at 4 threads): Kolmo's `ratio`
blocks are larger than bsc's 25 MB (that is where its 7% size lead on enwik8 comes from); not changed here.

## Setup

- Cloud VM, 4 vCPUs (Intel Xeon, AVX-512, 48 KiB L1d, 2 MiB L2 per core), gcc 13, `-O3 -march=native`, the repository's
  flags. Run-to-run noise on this VM is ±5% (more under load); every speed below is the best of several rounds,
  alternating the builds or coders under comparison, one thread count per run.
- Data (`/home/user/data`, not in the repository): Silesia; enwik8; OWID CO2 and energy CSVs, covid
  countries-aggregated; Loghub Apache, HDFS, Linux, OpenStack 2k samples joined four times (`logs.log`, 5 MB); OWID
  population as int64 NPY, 40 OWID energy columns as float64 NPY; and **Kolmo's own Huffman leaf streams**: every leaf
  chunk the baseline decoded with zstd's HUF while decoding those files at `balanced`/`hc` (`leaves.chunks`, 49 chunks,
  425 KB) and at `speed`/`balanced`/`eco`/`hc`/`max` over a wider set (`leaves2.chunks`, 56 chunks, 790 KB), dumped by
  `benchmark/rc3w2/native/libprof.c` (`LIBPROF_DUMP_HUF`).
- Rivals: zstd 1.5.7 (vendored HUF and FSE, called directly), bsc 3.4 (libbsc master built with OpenMP, `-m0 -e2`;
  1 thread: `-t -T`), bzip3 1.4.0 (`-b 16 -j T`).
- Tools (this directory, `build.sh`): `ebench.c` (order-0 coders on the same chunks through Kolmo's own codec
  functions), `hbench.c` (khuf vs HUF, own wrappers: see the caveat below), `qtool.c`/`qdec.c` (QLFC stages), `e2e.py`
  (whole processes, A/B and rivals, wall time and peak RSS), `table.py`, `rt.py`.

## 1. khuf (codec 27): the Huffman coder of the graph leaves

### What changed

The rc3 khuf coded lengths with a greedy 11-bit limit, wrote codes least significant bit first and decoded at most two
symbols per lookup; it was +0.16% larger than HUF and decoded at 0.97-1.05x (benchmark/rc3w2/native). Now:

- **Optimal length-limited codes** (package-merge), 12 bits on blocks of 16 KiB or more: the payload alone is 0.09% to
  0.5% smaller than rc3's lengths and below HUF's on every Silesia file (HUF also limits to 11 bits, heuristically).
  A Moffat-Katajainen Huffman pass comes first and is kept when it already fits; the merge is branch-free.
- **Code-length tables coded** with a small canonical code (Deflate's idea): 129-byte dense tables become ~65 bytes.
  The encoder keeps the smallest of sparse pairs, nibbles and coded.
- **Most significant bit first**, so every code prefix is a contiguous range of the decoder's tables. The
  multi-symbol table (up to six symbols per 11- or 12-bit lookup) is then built level by level, each code's range
  filled with the code followed by the (shorter) level for the remaining bits, branch-free, with symbols of equal
  length sharing one pass: 2.7-5 µs per 128 KiB block (an LSB-first build needed a strided fill or a truncating
  recursion: 10-31 µs).
- **Decoder loops**: four streams, rounds counted ahead (output room and input reach checked once per batch of
  rounds, nothing inside), refill address known a round ahead; a one-symbol 4-stream loop for blocks where few codes
  fit in a lookup (W / mean code length < 2.1, measured crossover) or under 2 KiB (table build cost); **x86-64 BMI2
  assembly** for both loops (`src/arch/x86_64/khuf_x86_64.S`): GCC and clang spill 4 of the 12 live values, the
  kernels keep them in registers; the one-symbol kernel is 5 instructions per symbol (stores from `%ah`). C references
  stay (`kolmo_khuf_dec4_c`, `kolmo_khuf_dec1_c`), `NO_ASM=1` and `KOLMO_CPU=c|sse2|sse41` select them;
  `tests/test_asm.c` compares both kernels with them on random tables and states (8,000 cases).
- **Encoder**: per-stream histograms (eight tables), exact stream sizes known before coding, the four streams coded in
  one loop from their ends (zstd's entry form: code pre-shifted to the top, length in the low bits; 5 instructions per
  symbol), no stream move except closing 8-byte gaps.

Tried and dropped (all decoder-internal): 8 streams (spills), a combined output-pointer/bit-count word and a
bit-position refill (fewer registers, more instructions: slower), zstd's marker-bit refill (one subtraction less per
lookup, but the next load waits for the last lookup: -18%), binary-search placement in the package-merge (slower than
the merge), prefetching. A block-size threshold for the multi-symbol table was measured at 0, 2, 4, 16, 32, 64 KiB:
2 KiB is best on the leaf streams.

### Results: khuf against zstd's HUF, zstd's FSE and Kolmo's rANS (same 128 KiB chunks, 1 thread)

`ebench` calls every coder through Kolmo's own functions (`huf_compress_all`/`huf_decompress_all` of codec 13,
`rans_*` of codec 14), so HUF runs as it does inside Kolmo. Sizes include the chunk headers.

```
file                        bytes | khuf: size, c MB/s, d MB/s     | HUF (codec 13)                 | FSE (zstd tANS)                | rANS (codec 14)               
leaves.chunks              424893 |     157419 c   626.5 d  1446.8 |     157435 c   509.7 d  1426.5 |     145777 c   325.5 d   494.4 |     152370 c   122.7 d   546.7
leaves2.chunks             790240 |     224950 c   714.3 d  1986.2 |     225099 c   660.1 d  1687.1 |     193190 c   326.7 d   473.7 |     199754 c   179.9 d   579.9
dickens                  10192446 |    5816566 c   993.4 d  2457.7 |    5822561 c  1315.9 d  2189.4 |    5768713 c   515.2 d   507.8 |    5768717 c   383.0 d   423.9
mozilla                  51220480 |   36791971 c   909.9 d  1634.1 |   36838438 c   987.9 d  1288.5 |   36666428 c   449.4 d   518.5 |   36669948 c   312.3 d   418.0
mr                        9970564 |    4484167 c   984.6 d  2259.0 |    4493103 c  1071.9 d  1669.9 |    4445657 c   388.3 d   474.5 |    4450482 c   380.3 d   547.3
nci                      33553445 |   10231325 c   992.4 d  3984.1 |   10244387 c  1124.8 d  2268.8 |   10201105 c   406.2 d   468.4 |   10214749 c   470.6 d   529.7
ooffice                   6152192 |    4911410 c  1066.6 d  1668.5 |    4913013 c  1289.5 d  1413.1 |    4901746 c   501.0 d   506.5 |    4901716 c   312.6 d   400.6
osdb                     10085684 |    8348923 c   920.7 d  1480.8 |    8354663 c  1231.3 d  1443.8 |    8336087 c   540.0 d   505.9 |    8338934 c   315.8 d   410.6
reymont                   6627202 |    3992032 c  1052.5 d  2287.8 |    3994824 c  1345.3 d  2067.2 |    3973887 c   574.7 d   508.1 |    3975778 c   379.9 d   411.8
samba                    21606400 |   14488567 c  1016.2 d  1975.3 |   14498113 c  1146.1 d  1722.0 |   14422679 c   496.2 d   512.5 |   14427986 c   367.2 d   418.5
sao                       7251944 |    6766623 c  1095.0 d  1688.9 |    6766384 c  1285.2 d  1198.6 |    6766248 c   542.3 d   509.1 |    6765260 c   453.1 d   499.4
webster                  41458703 |   25854375 c  1011.3 d  2219.5 |   25883607 c  1158.5 d  2093.9 |   25704699 c   541.5 d   508.7 |   25707610 c   358.7 d   407.0
x-ray                     8474240 |    6775065 c  1064.5 d  1686.3 |    6774680 c  1135.8 d  1309.7 |    6761866 c   465.2 d   506.8 |    6764085 c   387.1 d   450.3
xml                       5345280 |    3344778 c  1075.6 d  2230.0 |    3346593 c  1343.2 d  2007.1 |    3324711 c   577.4 d   511.4 |    3326021 c   381.6 d   404.5
```

| input | khuf size vs HUF | decode vs HUF | compress vs HUF |
|---|---:|---:|---:|
| Kolmo's leaf streams (`leaves`) | −0.010% | 1.01× | 1.23× |
| Kolmo's leaf streams (`leaves2`) | −0.066% | 1.18× | 1.08× |
| Silesia, 12 files | −0.073% (mean; +0.006% worst, x-ray) | 1.03×–1.76×, geometric mean 1.22× | 0.75×–0.94× |

Against the target (≥ 1.1× HUF's decode at ≤ +0.1% size): met on 10 of 12 Silesia files and on one of the two leaf
sets; osdb (1.03×), webster (1.06×) and the first leaf set (1.01×) fall short; the size condition holds everywhere.
khuf's encoder is slower than HUF's on large chunks (0.75-0.94×) and faster on the leaf streams.

**Caveat on HUF timings.** The same `HUF_decompress4X_hufOnly_wksp` call decoded the first leaf set at 645 or 1430
MB/s depending only on the wrapper function (its 16 KiB table's stack placement); `hbench`'s own wrapper is the slow
case, so its HUF decode numbers (and rc3w2's natbench) understate HUF. Only `ebench` numbers are used above.

### FSE and rANS (the rANS/tANS item)

On Kolmo's skewed leaf streams an order-0 arithmetic-class coder is much smaller than any Huffman coder: zstd's FSE
−7.4% / −14.1% and Kolmo's rANS leaf (codec 14) −3.2% / −11.2% against khuf; on Silesia −0.1% to −0.9%. Both decode at
0.4-0.6 GB/s, a third of khuf. The leaves already take rANS at `eco` and above (where it is tried); `balanced` skips it
for speed. **A wide interleaved rANS (8-32 lanes, the krans design) decoding at Huffman speed is therefore the
largest open win for the leaves**; it was not built in this lane (time went to khuf and QLFC). FSE itself is not a
candidate: it is no smaller than codec 14 here and slower to decode.

### Default switch

The graph leaves now write khuf (27) where they wrote HUF (13), and khuf then zstd (30, new) where they wrote Huffman
then zstd (22); 13 and 22 stay readable, dictionary and profile frames keep HUF, `KOLMO_NATIVE=libhuf` writes the old
ids (used to verify the determinism change: with it every changed frame hashes as before). Frames with Huffman
leaves change; of the determinism set 9 frames (floats.f64 at six settings, image.u8 at three) changed, all equal or
smaller (57,922 → 57,907; 63,632 → 63,580 bytes). The leaves are a small part of decode time in every mode, so the
end-to-end effect is small (section 3).

## 2. QLFC (codec 18, `ratio`)

Where the time goes (callgrind, whole `kolmo -d -T 1` of a 3 MB `ratio` frame): adaptive QLFC 91% of the
instructions, inverse BWT 7%, LZP 2%. The QLFC decoder runs at an IPC of about 5 on this core, so it is bound by
instructions, not by mispredictions (9.4% of branches, < 10% of cycles) or memory: prefetching the next symbol's
model rows (known one step ahead) changed nothing, and interleaving two coder blocks in one thread cannot help an
instruction-bound loop.

**Same streams, fewer instructions** (decoder; the encoder shares the first):
- the two APM entries of every update are adjacent int16: one SSE2 `pmulhw` moves both. `pmulhw(x, 16 ar)` =
  floor(x ar / 2^12), the scalar code's arithmetic shift, for every |x| < 2^15 and 16 ar < 2^15 (here |x| < 5000,
  ar ≤ 1342); 16-bit adds wrap like the int16 stores: identical values;
- move-to-front steps of rank < 16 and runs of up to 16 bytes are one 16-byte vector move (they were libc
  `memmove`/`memset` calls per symbol, 4% of the instructions).
Instructions per decoded block −9.7% (callgrind, 2 MB of dickens BWT output: 1,325 M → 1,197 M); encoder −2.4%;
decode time per coder block −7% (0.373 → 0.346 s per 10 MB, best of 3, `qdec`); compressed bytes identical.

**Four coder parts for 4-16 MiB blocks.** Adaptive QLFC is serial within a coder block, so a block's QLFC stage
decodes on as many threads as it has parts. rc3 cut parts of at least 4 MiB (a 10 MB block: 2 parts, 2 threads); bsc
cuts a 10 MB block into 12. Now blocks of 4 to 16 MiB are cut into four (each ≥ 1 MiB; larger blocks keep 4 MiB
parts, smaller ones stay whole). Cost measured on the BWT outputs (`qdec`): dickens +0.05% (4 parts), reymont +0.23%
(4), against +0.13% for samba at 8 parts and +0.38% for xml at 3 (not taken). The split depends on the block size
only; the decoder already read any split.

### `ratio` end to end (whole processes, best of 3 alternating decodes; `results/ratio_tables.md`)

| file | T | rc3 → w4 decode s | bsc -m0 -e2 decode s | w4 bytes vs bsc | bzip3 -b 16 decode s / bytes vs w4 |
|---|---:|---:|---:|---:|---:|
| dickens (10 MB) | 1 | 0.526 → 0.501 | 0.505 | −0.15% | 1.121 / −0.96% |
| dickens | 4 | 0.256 → 0.159 | 0.151 | −0.15% | 1.201 |
| reymont (6.6 MB) | 1 | 0.262 → 0.264 | 0.238 | −0.19% | 0.809 / +0.00% |
| reymont | 4 | 0.196 → 0.072 | 0.075 | −0.19% | 0.728 |
| webster (41 MB) | 1 | 1.618 → 1.535 | 1.663 | −3.22% | 4.108 / +5.39% |
| webster | 4 | 0.541 → 0.538 | 0.550 | −3.22% | 1.831 |
| samba (22 MB) | 1 | 0.513 → 0.533 | 0.920 | +1.69% | 1.470 / +0.00% |
| samba | 4 | 0.221 → 0.201 | 0.261 | +1.69% | 1.296 |
| xml (5.3 MB) | 1 | 0.112 → 0.114 | 0.105 | −1.97% | 0.309 / +5.31% |
| xml | 4 | 0.089 → 0.081 | 0.041 | −1.97% | 0.348 |
| nci (33 MB) | 1 | 0.484 → 0.483 | 0.456 | −5.27% | 1.398 / +18.9% |
| nci | 4 | 0.211 → 0.206 | 0.234 | −5.27% | 1.013 |
| enwik8 (100 MB) | 1 | 5.151 → 4.652 | 5.136 | −6.75% | 12.06 / +9.04% |
| enwik8 | 4 | 1.548 → 1.485 | 1.557 | −6.75% | 4.936 |
| logs.log (5 MB) | 1 | 0.014 → 0.016 | 0.053 | −23.2% | 0.105 |
| logs.log | 4 | 0.017 → 0.018 | 0.035 | −23.2% | 0.206 |

Decode peak RSS (MB): dickens 55-56 (bsc 54-65), webster 209-210 (bsc 131-139), enwik8 496-498 (bsc 155 at 1 thread,
478 at 4). Changes of ±5% between rc3 and w4 on files whose QLFC split did not change (samba, xml, logs) are the VM's
noise. The remaining gap to bsc at 4 threads is on blocks under 4 MiB (xml: one QLFC part, bsc decodes 5 MB on 8
threads); splitting those costs +0.4% and was not taken.

## 3. Huffman leaves end to end

Files with Huffman leaves at `balanced`/`hc` (`results/leaf_tables.md`, best of 5): sizes −0.00% to −0.08% (floats
−0.03%, image −0.08%, OWID energy NPY −0.01%, population −0.02%; the other files have no Huffman leaf the coder change
reaches), decode times within ±4% (noise; the leaves are a few percent of these decodes), compress equal within noise
except where `hc` tries more leaf codings (mr `hc` +16%, x-ray `hc` +22% at 1 thread: khuf's encoder is 0.75-0.94× HUF
on large chunks and `hc` codes every leaf several times).

## Correctness

- `tests/test_asm.c`: both khuf kernels against their C references, 4,000 random tables and states each (W 11-12 for
  the multi-symbol kernel, 1-12 for the one-symbol kernel); `tests/test_native.c`: khuf round trips at 28 sizes and 7
  kinds (around the 1, 2 and 16 KiB thresholds), corrupted, truncated and garbage payloads (30,000 random blocks, valid
  blocks with damaged tables and lengths), under ASan/UBSan in `check-native`; `make NO_ASM=1` test-native passes.
- QLFC: the same compressed sizes and bytes before and after (encoder shares the APM change; `qdec` checks every
  decode); determinism hashes of `ratio` frames unchanged.
- `benchmark/determinism/check.py`: 9 of 122 frames changed, as intended (floats.f64 at default, `-m fast -q`,
  `eco`, `hc`, `-x -l 1`, `--native-leaf`; image.u8 at default, `hc`, `--native-leaf`: their Huffman leaves); with
  `KOLMO_NATIVE=libhuf` all nine hash as before, so the Huffman coder is the only change; those 9 lines of
  `expected.sha256` were regenerated. 1- and 4-thread frames are identical.
- `make check`, `make check-rc`, `make check-sanitizers`, `make experimental-check GPU_CHECK=`, `make kfam-check`
  and `benchmark/determinism/check.py` (122 frames OK) pass on the final code; a `make NO_ASM=1` build passes
  test-asm/test-native/test-bwt and writes the same frames as the assembly build (checked at `balanced`, `hc`, `ratio`).

## Not done, next steps

- A wide interleaved rANS leaf (8-32 lanes, AVX2/NEON) to take the 3-14% the leaf streams leave on the table, at
  Huffman's decode speed (krans measured 1.2× HUF on x86 at order 0).
- khuf's encoder on large chunks (0.75-0.94× HUF): histogram and package-merge remain the costs.
- QLFC at 1 thread on small text (reymont, xml: 0.90-0.92× bsc): the decoder keeps ~30 values live and spills; a
  register-friendly restructuring or assembly of the bit loop is the next step. aarch64 kernels were not written (no
  way to time them here; the C loops do not spill with 31 registers).
- Binary arithmetic coders (CM, GR6+, KLZMA, kaud, image): not attempted.

## Files

`ebench.c`, `hbench.c`, `qtool.c`, `qdec.c`, `e2e.py`, `table.py`, `rt.py`, `build.sh`; `results/`: `ebench_T1.txt`,
`hbench_T1.txt` (HUF decode understated, see above), `ratio_T1.jsonl`, `ratio_T4.jsonl`, `leaf_T1.jsonl`,
`leaf_T4.jsonl`, `ratio_tables.md`, `leaf_tables.md`.

