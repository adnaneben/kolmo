# rc3 plan: C-native, owned algorithms, SIMD/assembly, faster `-x`, differentiating features

Owner's request of 16 November 2026. Base: `claude/release-rc3-20261116` (= rc2 head `b2f0c00` plus these documents).
Integration branch: `claude/release-rc3-20261116` (integration lane only). Nothing is merged to `main`, tagged,
published or opened as a pull request.

**Format:** backward compatibility is not a constraint. rc3 may write a new container format (format 2) that rc2 cannot
read and that cannot read rc2 files, if that makes Kolmo faster or smaller. Every format change must be measured and
justified in the lane's results.

## Rules for every lane

1. Read `docs/LAYOUT.md` first. New code goes where the layout says, even before the structure lane has moved the old
   code. Keep edits to existing files to the hook the lane needs (a call site, a Makefile line), so merges stay small.
2. Branch: `claude/rc3-<lane>-20261116`, from `claude/release-rc3-20261116`. Push only that branch
   (`git push -u origin <branch>`, retry on network errors). No merge to `main`, no tag, no PR, no force-push of
   anyone else's branch.
3. When the structure lane (`claude/rc3-structure-20261116`) has pushed its first complete commit, merge it into your
   branch (`git fetch origin claude/rc3-structure-20261116 && git merge FETCH_HEAD`) and move your hook to the new
   files. Check for it every hour or so.
4. Tests before every push: `make check`, `make check-rc`, `make check-sanitizers`, `make experimental-check`
   (`GPU_CHECK=` on Linux), `make kfam-check`, and
   `python3 benchmark/determinism/check.py build/kolmo build/determinism`. If the lane changes output on purpose,
   regenerate `benchmark/determinism/expected.sha256` with `--write` in a commit of its own that says why.
5. Benchmarks follow `benchmark/rc2/PROTOCOL.md`: every tool on 1 thread, and every tool on all threads, never mixed.
   Report the machine. Cloud VMs are x86-64 with 4 vCPUs; the integration lane repeats the key runs on the owner's
   Apple M5.
6. Ledger: add or update your rows in `research/CODECS.md` (technique, measured impact, verdict). Write the lane
   report as `benchmark/rc3/<lane>/RESULTS.md`, with numbers, commands and what failed.
7. Licences: code may be ported or adapted from Apache-2.0, BSD, MIT, zlib or 0BSD sources, keeping the copyright
   and licence in the file header and adding a line to `NOTICE`. Never GPL or LGPL code. For liblzma, use only an
   xz-utils release that is not 5.6.0 or 5.6.1 (the 2024 backdoor), and vendor its C sources only: no build scripts,
   no test files.
8. Commits end with the attribution trailer your session's instructions give. Never put a model name in a commit,
   code or docs.
9. Treat downloaded data as untrusted (own directory, `python3 -I`).

## Reserved identifiers (final numbering is decided at integration)

| id / tag | owner lane | use |
|---|---|---|
| codec 18 | bwt | BWT pipeline in C (LZP + BWT + QLFC, with the lane's improvements) |
| codec 19 | format2 | audio (kaud) |
| codec 20 | format2 | video (kvid) |
| codec 21-23 | format2 | stacking leaves: per-plane leaves, Huffman-then-zstd leaf, zero-run leaf |
| codec 24 | deps | an LZMA-class coder of our own, if the lane writes one |
| codec 25 | xmode | context mixing with block-parallel or stream-parallel models |
| container extension `IDX1` | features-b | column and row index (queryable archives) |
| container extension `DDP1` | features-b | chunk references to a shared chunk store (deduplication) |
| header field "profile id" | features-a | trained profile (`kolmo train`) |

## Lanes

### structure (cloud, starts first)
Restructure `src/` into the layout of `docs/LAYOUT.md`: split `kolmo.c` and the `.inc` files into `api/`, `core/`,
`plan/`, `transforms/`, `codecs/` (one codec per file behind a `kolmo_codec` table), `arch/` (move the existing
dispatch code: CRC32C, XXH3, decimal formatter, integer inverse), `cli/`. Update the Makefile (directory wildcards,
`NO_ASM`, `NO_GPU`), the bindings' `native/` copies and their build scripts, CI. Output must stay byte-identical (the
determinism check and the frame comparisons prove it). Then add `arch/asm.h`, the assembly conventions and one
example kernel with its C reference, test and dispatch entry, so the other lanes copy a working pattern. Push the
first complete restructure as early as possible: every lane waits to merge it.

### bwt (cloud): libbsc in C, libsais owned and reviewed, BWT speed
1. Port what Kolmo uses of libbsc from C++ to C: LZP, the BWT glue, QLFC (static and adaptive models), the range
   coder and predictors, the block header, into `src/algo/lzp.c`, `src/algo/qlfc*.c` and `src/codecs/bwt.c`. First prove
   byte-identical output against libbsc (`bsc_compress` with Kolmo's parameters) on the benchmark sets, then remove
   libbsc and the C++ runtime from the build.
2. Review libsais: list every function Kolmo reaches (`libsais_bwt`, `libsais_unbwt` and the OpenMP variants), check
   them against the published algorithms (SA-IS; Grebnov's BWT-only induced sorting), and decide whether to take
   them over. Take over the subset Kolmo uses into `src/algo/sais/` (Apache-2.0 header and `NOTICE` kept). Replace
   OpenMP with Kolmo's pthread pool, and add a multithreaded inverse BWT. Then tune: prefetch distances, 8-bit alphabet
   fast paths, and SIMD or assembly in `arch/` where measured.
3. Format changes the measurements justify (the owner's earlier "decision 5"): adaptive QLFC (about 1.1% smaller),
   threaded unBWT, LZP parameters, dropping the LZMA2 trial when BWT clearly wins. Codec 18.
4. Write `src/algo/sais/README.md`: what was kept, changed and why, with its reference to libsais.

### deps (cloud): dependency review and the recommendations, in order
Assess every dependency (zstd, LZ4, Brotli, liblzma from the system or Homebrew, xxHash, libbsc, libsais) on four
questions: 1. can the dependency be removed or reduced, 2. should it be rewritten in C, and where in assembly,
3. how much speed can owning it gain, 4. which parts should be integrated into Kolmo as specialised code (for example a
zstd decoder path specialised for Kolmo's leaves). Write `docs/DEPENDENCIES.md` with the measured answers. Then
implement them in that order:
- remove the system liblzma dependency by vendoring a pinned, verified liblzma subset (LZMA2 encoder and decoder only);
- trim the vendored zstd, Brotli and LZ4 to what Kolmo calls;
- integrate specialised paths where measured (e.g. Huffman-only and raw-literal leaves without the full zstd frame).
Rewriting zstd or LZMA is in scope only if the measurements show a gain. libbsc and libsais belong to the bwt lane.

### simd-x86 (cloud) and simd-arm (local Mac)
Profile text, image, audio and video paths (`-m balanced`, `hc`, `ratio`, `-x` and the new nodes) and write SIMD
intrinsics or assembly for the hot loops in `src/arch/x86_64` (AVX2, SSE4.2, BMI2) or `src/arch/aarch64` (NEON).
Candidates: the line/field scanners of the table and log transforms, the word transform, histograms and entropy
estimates in the planner, MED/gradient image prediction (image model and NPLANE), the PCM/LPC filters of the audio
node, kvid's temporal prediction and SAD, the CM mixer's dot product and weight update (`-x`), Huffman and rANS
decode loops, and the inverse BWT. A kernel lands with any gain above noise, a C reference and a test. Output must not
change.

### xmode (cloud): `-x` on files under 128 MiB and under 1 MiB
`-x` codes each 128 MiB block on one thread, so files under 128 MiB use one core. Implement and measure, in this order:
1. run the per-block trials (CM, LZMA2, BWT, family coder) concurrently when threads are free (same output);
2. skip the CM trial when the family coder applies to the block, as on FASTQ (same output, about 20x faster there);
3. SIMD mixer and counter updates, software prefetch of the next byte's context slots (same output; helps every size,
   including under 1 MiB);
4. stream-parallel CM: the structure transforms already split blocks into streams (columns, fields, planes); code the
   streams with their own models on separate threads (codec 25; helps structured files of any size);
5. block-parallel CM for large single streams: split at n/T only when n ≥ T × 8 MiB and the measured size cost stays
   under 0.5%, optionally with a shared static prior so each part starts warm.
6. kfam decode parallelism: more, smaller chunks per stream for decode speed, size cost measured.
7. Model tables sized to the block (`docs/MEMORY-MODEL.md`): `-x` holds 4.7 GiB to decode a 10 MB file today. Measure
   size and speed with tables scaled to the block; implement behind an option until the owner approves the memory model.
Report the size and speed for files of 100 KB, 1 MB, 10 MB and 100 MB at 1, 4 and all threads.

### format2 (cloud): every stacking gain, audio and video nodes
1. Every gain in `research/STACK-AUDIT.md` that needed a format decision, now that format 2 allows it: separate
   leaves per byte plane with the sparse guard (ST-1, ST-5, ST-8), Huffman-then-zstd leaves (S1 H->Z), the zero-run
   leaf, packing representations. Each is measured against rc2's own modes, not against the research baselines, and
   kept only if it gains. Make the plane-aligned leaves the default where they gain.
2. kaud as Kolmo's WAV node (codec 19), chosen by the planner where it beats the current WAV path.
3. kvid as the Y4M video node (codec 20).
Update `research/CODECS.md`: every stacking gain must be either in rc3 or listed with its measured reason.

### features-a (cloud): `kolmo train` and `--target`
- `kolmo train DIR -o profile.kpf`: a reusable profile from a sample. It holds the plan (transforms, column types,
  codec per leaf), zstd and Brotli dictionaries, and the CM priors if xmode provides them. `kolmo -P profile.kpf`
  compresses with it at full speed. Measure against OpenZL's trained compressors and zstd `--train` on the existing
  benchmark sets.
- `--target 'decode>=1GB/s'`, `--target 'size'`, `--target 'cost:<formula>'`: the planner picks per block the
  cheapest plan under the TRADEOFFS.md cost model.

### features-b (cloud): queryable archives and deduplication
- Column and row index (`IDX1`): `kolmo cat file --col co2 --rows 1000-2000` and `kolmo grep` that decode only the
  blocks and streams they touch. Measure bytes decoded and time against a full decode.
- Content-defined chunking (Gear/FastCDC) with a chunk store, for archives of many similar files
  (`kolmo a archive.kol files...`). Measure on checkpoints, log rotations and CSV snapshots against zstd `--long`,
  borg and restic where available.

### gpu (local Mac): CPU and GPU at the same time
A scheduler that keeps the 10 CPU cores and the GPU busy together. Data-parallel work (suffix sorting for BWT, image
and video prediction, histograms, wide interleaved rANS) goes to the GPU, and bit-serial work (CM, arithmetic coding)
to the CPU, in a pipeline over blocks and streams. Measure on the Mac against CPU-only `-T 10`. Port the GPU suffix
sort idea (libcubwt, CUDA, Apache-2.0) to Metal if the profile says BWT time is dominated by the sort.

### integration (local, this session)
Merges the lanes into `claude/release-rc3-20261116` in dependency order (structure first), runs every test on macOS
and Linux, the full benchmark on the Mac at 1 thread and all threads, keeps `research/CODECS.md` complete, and writes
the rc3 report.
