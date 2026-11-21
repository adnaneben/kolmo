# RIVALS_BUILT — domain-specialist rivals built for the overnight run (2026-11-20T20:32:00Z)

The owner approved this in chat on 2026-11-20: official sources only, built into a run-local prefix. Global Homebrew and /opt/homebrew were not touched.

**Paths**
- Prefix: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/rivals/prefix`. ffmpeg 6.1.6 has its own prefix, `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/rivals/prefix-ffmpeg6`.
- Sources: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/rivals/src/`. Downloads: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/rivals/dl/<file>.d/`, each in its own directory.
- Build logs: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/logs/rivals-build/`.
- Binary hashes: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/rivals/BINARIES.sha256`.
- Smoke files: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/rivals/smoke/`.

**Compiler:** Apple clang 21.0.0 (clang-2100.4.1.1), macOS SDK 27.2, arm64.

## Downloads (sha256 of the fetched file)

- https://github.com/xiph/flac/releases/download/1.5.0/flac-1.5.0.tar.xz f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920
- https://ffmpeg.org/releases/ffmpeg-7.1.5.tar.xz de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f
- https://github.com/samtools/samtools/releases/download/1.25/samtools-1.25.tar.bz2 8bbd2b3cb447953149a74ab0b00e3ec4e88a789a27b8373a31fa6b5b2dee02b1
- https://github.com/samtools/htslib/releases/download/1.25/htslib-1.25.tar.bz2 158c271e4e2986bb2dd11283b640edc10c56695b293df77a109ce99480303730
- https://ffmpeg.org/releases/ffmpeg-6.1.6.tar.xz d4fcb164028dd3beee5d92c0ac72e46aac6973c75ea12dc14de07bf8f407370a
- libjxl: `git clone --depth 1 --branch v0.13.0 https://github.com/libjxl/libjxl.git` at commit 5a7da83447212fe4900fc29ce97430b881caa67e. Submodules: brotli db24a98, highway 457c891, skcms 96d9171. The other submodules were not fetched.
- pcodec: `cargo install pco_cli --locked --root $V/rivals/prefix`, which installed pco_cli 1.0.1 from crates.io.

The tarballs were not checked against their GPG signatures; only these sha256 values are recorded.

## Binaries

| tool | version | binary | sha256 |
|---|---|---|---|
| flac | 1.5.0 | prefix/bin/flac | 4747b784… |
| ffmpeg (FFV1) | 7.1.5 | prefix/bin/ffmpeg | 59881de0… |
| ffmpeg (FFV1) | 6.1.6 | prefix-ffmpeg6/bin/ffmpeg | 39f02d55… |
| cjxl / djxl | v0.13.0 5a7da83 [_NEON_] | prefix/bin/cjxl, djxl | a109233d… / 4219278a… |
| samtools / htslib | 1.25 / 1.25 | prefix/bin/samtools | bb37cae7… |
| pcodec (pco_cli) | 1.0.1 | prefix/bin/pcodec | 46073891… |

Full hashes are in `rivals/BINARIES.sha256`.

## Configure lines

**flac:**
```
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DWITH_OGG=OFF -DBUILD_DOCS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DINSTALL_MANPAGES=OFF -DBUILD_CXXLIBS=OFF
```

**ffmpeg 7.1.5 and 6.1.6:**
```
--cc=clang --disable-everything --disable-autodetect --disable-network --disable-doc --disable-ffplay --disable-ffprobe --enable-ffmpeg --disable-shared --enable-static --enable-encoder=ffv1,rawvideo,wrapped_avframe --enable-decoder=ffv1,rawvideo,wrapped_avframe --enable-demuxer=yuv4mpegpipe,matroska,nut,rawvideo --enable-muxer=yuv4mpegpipe,matroska,nut,rawvideo,framemd5,md5,null --enable-protocol=file,pipe --enable-filter=null,format,scale,copy --enable-swscale
```
The 7.1.5 build also enables the aformat and anull filters.

**libjxl:**
```
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_TOOLS=ON -DJPEGXL_ENABLE_{DOXYGEN,MANPAGES,BENCHMARK,EXAMPLES,JNI,SJPEG,OPENEXR,JPEGLI,PLUGINS}=OFF -DJPEGXL_ENABLE_SKCMS=ON -DJPEGXL_BUNDLE_LIBPNG=OFF -DCMAKE_DISABLE_FIND_PACKAGE_{PNG,GIF,JPEG,ZLIB,OpenEXR,PkgConfig}=ON
```
Targets built: cjxl and djxl. They read and write PNM/PPM only; PNG and JPEG input are not built.

**htslib:**
```
./configure CC=clang --disable-libcurl --disable-gcs --disable-s3 --disable-plugins
```
bz2 was found. liblzma was found as a library, but lzma.h was missing. libdeflate was not found.

**samtools:**
```
./configure CC=clang --with-htslib=<src>/htslib-1.25 --without-curses
```

## Smoke round trips (all exact)

| tool | input | result | how to rebuild the exact input |
|---|---|---|---|
| flac -8 | holdout esc50-2-100648-A-43.wav, 441,044 B | 297,065 B; decode `cmp`-identical | the WAV header is restored by flac. For WAVs with extra chunks, add `--keep-foreign-metadata` and verify. |
| ffmpeg FFV1 level 3, `-g 1 -slices 4 -slicecrc 1`, in mkv | holdout foreman_cif.y4m, 45,621,044 B | about 20.15 MB mkv. framemd5 of the frames is identical. | the y4m from ffmpeg differs only in its first line: it adds `C420jpeg XYSCSS=420JPEG XCOLORRANGE=LIMITED`. Rebuild = the original header line (a 44 B sidecar) + `tail -n +2` of the ffmpeg y4m. This was verified `cmp`-identical with both 7.1.5 and 6.1.6. **The full-file size must include the header sidecar and the mkv container.** |
| cjxl -d 0 -e 7 / djxl | holdout chelsea.ppm, 405,915 B | 142,016 B; the djxl PPM is `cmp`-identical | none needed for P6 maxval 255 |
| samtools import (CRAM 3.1 archive), then samtools fastq | first 10,000 reads of holdout SRR2584863.fastq, 3,653,611 B | CRAM 783,853 B | **samtools drops the read-name comment** (`HWI-ST957:…/1`). Rebuild = CRAM + the header lines (`awk 'NR%4==1'`, xz -9e: 51,184 B), with the header lines put back. Verified `cmp`-identical. Sequences and qualities round-trip exactly. Use `--no-PG` for deterministic output. |
| pcodec compress / decompress -o binary | population.i64 (screening data), 151,152 B | 37,438 B, exact | the input filename must look like `<dtype>_<name>`, for example `i64_population` (use a symlink). This is raw binary only; .npy needs its header stored separately. |

## Single-thread (T1) settings and observed threads

The observation was a crude `ps -M` sample every 20 ms. Before any timed run, `audit.py preflight` must confirm the thread count.

| tool | settings | observed |
|---|---|---|
| flac | default (single-threaded; `-j` adds threads only when used) | 1 thread |
| cjxl / djxl | `--num_threads=0` (0 = no multithreading; the default -1 uses all cores) | 1 thread |
| samtools | `-@0` | 1 thread |
| ffmpeg 7.1.5 | `-threads 1` for input and output, `-filter_threads 1` | **6 threads**: ffmpeg 7's CLI runs demux, decode, filter and mux in separate threads by design. **Not strict T1**; label it "worker + async I/O". |
| ffmpeg 6.1.6 | `-nostdin -threads 1` for input and output | 4 threads exist, but at most 1 was seen running |

**Recommendation:** use ffmpeg 6.1.6 for the strict-T1 FFV1 lane, after preflight confirms it. Keep 7.1.5 for reference only.

**pcodec:** pco_cli depends on rayon and on tokio rt-multi-thread. In the sources, rayon is used under `bench/`; compress and decompress were not checked for thread use. Run preflight before using it on the T1 board.

## Unavailable or not done

- OpenZL, Kanzi, fqzcomp5 and cmix were not attempted in this step; they are outside the directive. They stay UNAVAILABLE in TOOL_INVENTORY.md.
- htslib was built without libdeflate and without lzma headers. CRAM 3.1 archive uses rANS-Nx16, arithmetic coding, fqzcomp_qual and name tokenisation, and needs neither, but CRAM's lzma codec is not available.
