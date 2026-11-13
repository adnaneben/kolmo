# Byte-stream preprocessing filters and reversible transforms for binary and mixed data

> Reader note on evidence quality. Web access in this session was partly blocked: a large share of fetches
> (Matt Mahoney's DCE, fgiesen's kkrunchy article, 7-Zip method docs, precomp/brunsli/libbsc/zstd GitHub pages,
> xz NEWS, Wikipedia Rzip) were refused by the permission layer and could not be retried. Everything under
> "Cited Findings" was actually read this session. Everything I know only from prior knowledge is placed under
> "Gaps" and explicitly tagged **[UNVERIFIED RECALL]** with the URL where it should be checked. Do not present
> the recall items as sourced facts, and do not attach numbers to them.

## 1. Executable filters: BCJ variants, BCJ2, ARM64/RISC-V, disassembly-based stream splitting

### Takeaway
The only hard, primary-sourced numbers obtained are from the xz maintainer: a per-architecture BCJ filter in front of LZMA2 is worth about 5% on ARM64 and ARM-Thumb2 and 7-8% on RISC-V kernel images, plus 0-2% from matching LZMA2 lc/lp to instruction alignment. No sourced number was obtained this session for the extra gain of stream splitting (BCJ2 / disassembly) over plain E8E9; that remains the main open item for kolmo's executable gap.

### Cited Findings
- BCJ replaces relative branch/call/jump addresses with absolute ones so that an LZ compressor sees repeated targets as repeated byte strings; each ISA needs its own filter; RISC ISAs with fixed-length instructions are simpler to filter than x86. Implementations: Microsoft CAB/LZX (1996, x86 CALL), 7z and xz (multiple architectures), ZPAQ ("E8E9"). — [Wikipedia: BCJ (algorithm)](https://en.wikipedia.org/wiki/BCJ_(algorithm))
- Fedora 31 live image (squashfs+xz): x86 BCJ saves about 30 MB out of roughly 1.7 GB compressed (about 1.8% on a mixed image, not pure code) but roughly doubles installation (decompression) time. — [Wikipedia: BCJ (algorithm)](https://en.wikipedia.org/wiki/BCJ_(algorithm))
- Linux kernel, xz with BCJ, by the xz maintainer (Lasse Collin), 2024:
  - ARM64 BCJ filter: "On ARM64 the compressed kernel size is reduced about 5 %" versus unfiltered XZ or plain LZMA.
  - RISC-V BCJ filter: reduction "by 7-8 %" versus unfiltered XZ or plain LZMA.
  - ARM-Thumb2 kernels: switching from the ARM filter to the ARM-Thumb filter "reduces compressed kernel size about 5 %"; the ARM filter "didn't do anything useful with ARM-Thumb2 code" (wrong-filter choice gives zero gain, so ISA detection matters).
  - LZMA2 literal/position alignment tuning by instruction alignment: alignment 1 (x86) no change; alignment 2: `lp=1`; alignment 4: `lp=2,lc=2`. "This change reduces compressed kernel size 0-2 % depending on the arch"; no difference on 1-byte-aligned x86. For unknown arch: "Guessing too low should hurt less than guessing too high."
  - Tool versions: xz 5.4.0 is required for the ARM64 filter and 5.6.0 for the RISC-V filter.
  — [LKML: "xz: Adjust arch-specific options for better kernel compression"](https://lore.kernel.org/lkml/20240320183846.19475-12-lasse.collin@tukaani.org/)
- 7z container filter inventory (from a third-party 7z format specification, not Igor Pavlov's own docs): x86 BCJ converts E8 (CALL) and E9 (JMP) operands, with an optional 4-byte start offset; PowerPC (4-byte alignment), IA-64 (16-byte), ARM (4), ARM Thumb (2), SPARC (4), ARM64 (method ID 0x0A, handles BL and B... 4-byte alignment), RISC-V (method ID 0x0B, 2-byte alignment for compressed code or 4 for standard). — [zesven 7z spec, ch. 11 Filters](https://zesven.pages.dev/7z/11-filters)
- BCJ2 (x86) structure: four streams into one decoder — (0) main data with address placeholders, (1) CALL (E8) displacements as little-endian UINT32, (2) JMP (E9) displacements as little-endian UINT32, (3) a range-coded selector stream that says for each candidate opcode whether it was converted as CALL, JMP, or neither. Each of the first three streams is fed by its own codec; the selector stream is stored directly. Support is optional in readers and some implementations are decode-only. — [zesven 7z spec, ch. 11 Filters](https://zesven.pages.dev/7z/11-filters)
- Typical 7z chains: BCJ x86 -> LZMA2 for executables. In solid archives, BCJ state either carries across file boundaries or resets per file (implementation-defined). — [zesven 7z spec, ch. 11 Filters](https://zesven.pages.dev/7z/11-filters)
- Courgette (Chromium) goes further than BCJ: it disassembles, finds absolute addresses via the relocation table and relative branch targets, replaces the relocation table with an instruction that regenerates it, numbers each referenced address and replaces it by its index ("labels"), then serialises instructions and the index-to-address map into separate byte vectors. It is designed for diffing (bsdiff on the transformed streams), not standalone compression. — [Courgette Internals](https://chromium.googlesource.com/chromium/src/+/lkgr/courgette/description.html)
- For executable diffs, bsdiff yields about 6% of original size for Chrome, and Courgette is described as producing "9x smaller diffs" than bsdiff — indication of how much redundancy lives in address fields once code is disassembled, though this is delta coding, not single-file compression. — [Wikipedia: BCJ (algorithm)](https://en.wikipedia.org/wiki/BCJ_(algorithm))
- FreeArc ships "BCJ: preprocesses executables" among its filters. — [Wikipedia: FreeArc](https://en.wikipedia.org/wiki/FreeArc)

### Inferences
- The xz figures (5% ARM64, 7-8% RISC-V, 5% Thumb) are for LZMA2 on kernel images, which are nearly pure code. For an LZ back end these are the realistic ceiling for a simple branch-conversion filter on those ISAs; mixed files will see proportionally less (cf. Fedora's ~1.8% on a whole OS image).
- RISC-V's larger gain is consistent with the filter having more to do (multi-instruction address materialisation, JAL), so a RISC-V filter is the higher-value of the two missing kolmo filters per unit of code, with ARM64 the higher-value by installed base.
- The lc/lp alignment finding generalises: any back end with position-aware literal contexts (LZMA `lp`, CM position contexts) should be told the instruction alignment (4 for ARM64/PPC/SPARC, 2 for Thumb/RISC-V-C). It is a zero-cost 0-2% stackable gain independent of the filter.
- Composition (reasoned, not sourced this session): absolute-address conversion helps LZ and BWT because it creates repeated 4-byte strings; for context mixing the gain from plain E8E9 is smaller because a CM model can partly learn the pattern, which is why PAQ-class compressors add an instruction-aware model on top rather than rely on the filter. kolmo's 33% gap to PAQ8PX on an x86 binary is therefore unlikely to be closed by BCJ2-style splitting alone; splitting plus per-stream modelling (opcode/modrm contexts) is the relevant direction.
- BCJ2's design is the cheapest form of stream splitting: only 3 extra streams and a 1-bit-per-candidate selector, no disassembler. It is the natural next step from kolmo's existing E8/E9 filter before a full disassembly split.

### Gaps
- **No sourced number for BCJ2 vs BCJ gain.** [UNVERIFIED RECALL] 7-Zip's own documentation and practitioner lore put BCJ2 a low-single-digit percentage ahead of BCJ on x86 code with LZMA; I could not open the 7-Zip method docs ([7-zip.opensource.jp method.htm](https://7-zip.opensource.jp/chm/cmdline/switches/method.htm), blocked) to confirm. The 7-Zip docs example chain is, from memory, `-m0=BCJ2 -m1=LZMA:d25 -m2=LZMA:d19 -m3=LZMA:d19 -mb0:1 -mb0s1:2 -mb0s2:3` (main stream with a big dictionary, the two address streams with small dictionaries) — verify before quoting.
- **No sourced number for disassembly-based splitting (opcode / modrm / imm / disp streams) vs E8E9.** The key practitioner source is Fabian Giesen's "x86 code compression in kkrunchy" ([fgiesen.wordpress.com](https://fgiesen.wordpress.com/2011/01/24/x86-code-compression-in-kkrunchy/), blocked). [UNVERIFIED RECALL] It describes a table-driven x86 instruction-length disassembler that splits code into separate streams (opcodes+modrm, 8-bit immediates, 32-bit displacements, jump/call targets, etc.), converts call targets to absolute with a move-to-front list of recent targets, and reports a clear gain over E8-only filtering for a context-mixing back end; I cannot quote his numbers from memory. This article should be read by the report writer or a follow-up researcher: it is the single best source for the question asked.
- [UNVERIFIED RECALL] FreeArc "dispack" (originally by Shelwien / used in Bulat's FreeArc as `dispack070`) is the same idea packaged as a standalone filter; licence and numbers not verified. Wikipedia's FreeArc page does not mention dispack, srep or mm.
- [UNVERIFIED RECALL] PAQ8PX: applies an E8/E9 transform at block-detection time and additionally runs a dedicated x86/x64 exe model (instruction-decoding contexts: opcode, modrm, prefix state). Source: paq8px source tree (github.com/hxim/paq8px), not opened this session. No isolated gain figure found.
- Not researched for lack of budget/access: UPX filters (families of call/jump filters with selectable opcode sets and "swap" variants, GPL with exception), Crinkler (context-mixing linker, per-section models), LZHAM/Oodle exe handling, zstd/Meta handling of executables (I found no evidence that zstd has an exe filter; the "superblock" term in zstd refers to block splitting for the entropy stage, not an exe transform — unverified), LZX's E8 translation with file-size bound.
- 7-Zip version history for the ARM64 and RISC-V filters (I recall ARM64 in the 22.xx/23.xx line and RISCV in 24.xx) was not confirmed; the current GitHub release page (7-Zip 26.05, 21 Oct) has no filter notes — [ip7z/7zip releases](https://github.com/ip7z/7zip/releases/latest).
- Licences of reference code not verified this session: [UNVERIFIED RECALL] LZMA SDK (Bra.c, Bcj2.c, Delta.c incl. ARM64/RISCV) is public domain; xz/liblzma filters are public domain / 0BSD in 5.6+; Courgette is BSD-3 (Chromium); UPX GPL-2+ with exception. All would be compatible with Apache-2.0 except UPX/FreeArc code.

## 2. Delta and multimedia filters (stride detection, table detection, PCM/RGB predictors)

### Takeaway
The one sourced figure: FreeArc's automatic `delta` filter (table detection in arbitrary binaries) improved compression of binary files by about 2% when introduced. Fixed-distance byte delta (7-Zip Delta, distance 1-256) is trivial; the value is in automatic detection, which kolmo partly has via fixed-record detection.

### Cited Findings
- 7z Delta filter: one property byte = distance minus 1 (range 1-256, default 1); output byte = input byte minus the byte `distance` positions earlier; no alignment requirement. Common chain: Delta (distance 2 or 4 depending on audio layout) -> LZMA2 for audio. — [zesven 7z spec, ch. 11 Filters](https://zesven.pages.dev/7z/11-filters)
- FreeArc 0.40 release notes: adding the delta filter "improved compression of binary files by about 2%". — [Presenting FreeArc 0.40](https://freearc.sourceforge.net/Presenting%20FreeArc%200.40.htm)
- FreeArc DELTA is described as a filter that "improves compression of tables in binary data"; multimedia mode (`-mm`) uses TrueAudio (TTA), GRZip and an MM filter, with automatic switching by file type; file groups in `arc.groups` include `$wav`, `$bmp`, `$precomp`, `$jpg`, `$iso`. — [Wikipedia: FreeArc](https://en.wikipedia.org/wiki/FreeArc); [Presenting FreeArc 0.40](https://freearc.sourceforge.net/Presenting%20FreeArc%200.40.htm)
- FreeArc is GPL-2.0-only (so its delta/mm code cannot be copied into an Apache-2.0 project; ideas only). — [Wikipedia: FreeArc](https://en.wikipedia.org/wiki/FreeArc)

### Inferences
- A ~2% average gain on "binary files" from automatic table delta is exactly the "small but stacks" class the brief asks for; kolmo's fixed-record detection plus delta/shuffle likely already captures most of it, so the incremental item is detection of tables embedded at arbitrary offsets inside larger files (resource tables, relocation tables, index arrays in executables and game data).
- Composition (reasoned): delta on tables turns arithmetic progressions into constant runs, which helps LZ and BWT strongly; for CM back ends with record/stride models (paq8-style record model) the filter gain largely disappears and can be negative, because the model can use both the raw value and the stride context. Delta should therefore be planner-selected per back end, consistent with kolmo's per-transform rule.

### Gaps
- **How FreeArc `delta` detects tables** — not sourced. [UNVERIFIED RECALL] Bulat Ziganshin's Delta scans for runs where bytes at a candidate record width N (small N, roughly 2-32) repeat/are monotone over a minimum number of rows, then subtracts successive rows only for the columns that look like counters and optionally reorders columns; it emits (offset, N, rows) descriptors and leaves the rest untouched. Verify against FreeArc source (`Compression/Delta/Delta.cpp`) or encode.su FreeArc thread ([encode.su/threads/43-FreeArc](https://encode.su/threads/43-FreeArc/page57), found by search, not opened).
- RAR filters: [UNVERIFIED RECALL] RAR3 used a VM with standard filters (E8, E8E9, Itanium, delta, RGB, audio); RAR5 dropped the VM and keeps hard-coded delta (channel count), E8/E8E9 and ARM filters. No ratio numbers found. Source to check: rarlab RAR5 technote / unrar source (unrar licence forbids reuse for compression).
- WAV/PCM predictors (TTA, OptimFROG-class, paq8px audio model), BMP/TIFF/PNM "subtract green" decorrelation, bit-depth packing: no sources fetched. kolmo already has 8/16-bit image predictors; the uncovered items are (i) inter-channel decorrelation for PCM stereo (mid/side) and RGB (G, R-G, B-G), (ii) linear-prediction (order 1-2 fixed predictors as in FLAC "fixed" subframes) for PCM. No numbers obtained.

## 3. Long-range deduplication / match pre-pass

### Takeaway
I obtained no sourced head-to-head numbers for rep/srep, lrzip or zpaq dedup; the only measured data point is zstd `--long=31` on CSV archives, which does not isolate the long-range gain. This question is essentially unanswered with citations and needs a follow-up pass.

### Cited Findings
- FreeArc REP: "finds repetitions at separations up to 1gb" as a pre-pass before the main coder. — [Wikipedia: FreeArc](https://en.wikipedia.org/wiki/FreeArc)
- zstd with `--long=31` on a 16 GiB set of 318 CSV files: `-19 --long=31 -T8` gives 2.70 GiB (6.45x) in 41m40s; `--ultra -22 --long=31 -T8` gives 2.67 GiB (6.54x) in 2h48m — about 1.3% smaller for more than 4x the time. On a 1 GiB CSV with `-T0 --long=31`: level 3 = 177 MiB in 1.5 s; level 8 = 135 MiB in 5.2 s; level 19 = 107 MiB in 122.3 s. The post has no run without `--long`, so the long-range contribution is not isolated. — [perihelion: Archival Compression Comparisons (Dec 2025)](https://perihelion.codeberg.page/blog/2025-12-22-archival-comparisons.html)
- lrzip: gains grow with file size and available RAM; windows above 2 GB made on 64-bit may not decompress on 32-bit; described as "not really usable" with under 256 MB RAM for compression; benchmark numbers live in a separate README.benchmarks not included on the page read. — [lrzip README (SlackBuilds mirror)](https://slackware.uk/sbo/13.1/system/lrzip/README)
- FreeArc increased usable dictionary by using temporary files, which "sometimes improves compression by up to 10%" — an indication of the size of long-range effects on archive-scale data. — [Presenting FreeArc 0.40](https://freearc.sourceforge.net/Presenting%20FreeArc%200.40.htm)

### Inferences
- A dedup/long-match pre-pass is back-end-agnostic by construction: it removes repeats that lie outside the back end's window (zstd default windows of a few MB, BWT block size, CM hash-table reach), so its gain survives LZ, BWT and CM alike, provided the minimum match length is long enough (hundreds of bytes) not to steal short matches the back end would code better.
- For kolmo, which already has a tar-aware member planner, the cheapest form is whole-member and chunk-level (content-defined) dedup at the planner level, followed by an srep-style long-match pass with a large minimum length.

### Gaps
- [UNVERIFIED RECALL] srep (Bulat Ziganshin): long-range matcher with minimum match length typically 32-512 bytes, memory roughly a few percent of input size, hundreds of MB/s, optional "future-LZ" mode that lowers decompression memory; widely used ahead of LZMA in game repacks. Licence unclear (FreeArc ecosystem). No numbers verified. Leads found but not opened: [encode.su HFCB: Huge Files Compression Benchmark](https://encode.su/threads/507-HFCB-Huge-Files-Compression-Benchmark/page3).
- [UNVERIFIED RECALL] rzip (Tridgell): rolling-hash long-distance matching over a ~900 MB history then bzip2; lrzip (Kolivas) extends to unlimited window with LZMA/ZPAQ back ends; GPL-2. [Wikipedia: Rzip](https://en.wikipedia.org/wiki/Rzip) was blocked.
- [UNVERIFIED RECALL] zstd long-distance matching (LDM) was introduced in v1.3.2 with a release-note table showing ratio gains on multi-version tarballs at modest speed cost; default `--long` window is 2^27 (128 MB), max 2^31. [zstd v1.3.2 release notes](https://github.com/facebook/zstd/releases/tag/v1.3.2) was blocked — the table there is the best primary source for LDM gain and cost.
- [UNVERIFIED RECALL] zpaq: content-defined fragments averaging 64 KiB, SHA-1 identified, dedup before compression (libzpaq is public domain). No numbers verified.

## 4. Recompression of already-compressed streams (deflate, JPEG, PNG/ZIP/PDF, MP3)

### Takeaway
JPEG recompression is the best-quantified item: Lepton saves ~22-23% losslessly at hundreds of Mbps multi-threaded within 24-39 MiB, matching packJPG's ratio at roughly 10x the speed, while generic codecs get 1% or less on JPEG. For deflate, preflate (Apache-2.0) reconstructs any zlib-level stream bit-exactly with a few bytes to a few KiB of side data but is slow; preflate-rs (Microsoft) is the maintained embeddable successor.

### Cited Findings
JPEG
- Lepton compresses JPEGs losslessly to 77% of original size on average (about 23% saving; 22.7% in the body), decodes "more than nine times faster" than the best prior work, with streaming decode; as of Feb 2017 Dropbox had compressed over 203 PiB of JPEGs, saving over 46 PiB. — [Horn et al., Lepton, NSDI 2017 (arXiv 1704.06192)](https://arxiv.org/abs/1704.06192)
- Lepton details: 16 cores decode at about 300 Mbps; decode p50 under 60 ms, p99 under 250 ms; encode p50 170 ms, p99 1 s; decode memory 39 MiB at p99 with a hard 24 MiB maximum for single-threaded decode; encode limit 178 MiB. packJPG achieves about the same ~23% but Lepton is about an order of magnitude faster. PAQ8PX is about 0.8 percentage points better than single-threaded Lepton but encodes 35x and decodes 50x slower. Lepton is 1.5-4x faster than JPEGrescan/MozJPEG. Deflate, LZMA, Zstandard and Brotli achieve 1% or less on JPEGs. — [Lepton paper, ar5iv full text](https://ar5iv.labs.arxiv.org/html/1704.06192)
- Lepton production coverage: 94.069% success; rejected: progressive 3.043%, unsupported JPEG 1.535%, not an image 0.801%, 4-colour CMYK 0.478%, memory limits ~0.04%. Progressive and CMYK were intentionally disabled. It works on independent 4 MiB chunks via "Huffman handover words" so any chunk can be decoded alone and in parallel. — [Lepton paper, ar5iv full text](https://ar5iv.labs.arxiv.org/html/1704.06192)
- A learned DCT-domain recompressor (CVPR 2022) reports about 30% savings at quality 75, beating Lepton, JPEG XL and CMIX on Kodak/DIV2K/CLIC, but slower than Lepton and JPEG XL (GPU-based) — sets the practical ceiling above the ~22% class. — [Guo et al., arXiv 2203.16357](https://ar5iv.labs.arxiv.org/html/2203.16357)

Deflate
- preflate v0.3.5 (Dirk Steinke, Apache-2.0): splits a raw deflate stream into uncompressed data plus reconstruction info and rebuilds it bit-for-bit. It estimates the zlib parameters that could have produced the stream and stores only the deviations from that prediction, arithmetic-coded (packARI-derived coder, static models). Large streams are cut into meta-blocks of a few MB for bounded memory and parallelism. Tested on ~10,000 valid deflate streams, all reconstructed; a precomp fork was tested on 100+ GB. — [preflate README](https://github.com/deus-libri/preflate)
- preflate side-data size: goal is "a few bytes" for zlib streams at any level; example: 350 KiB file at zlib level 1 needs 1.5 KiB (3 bytes without meta-blocks). Older precomp (brute-force zlib parameter search) is smaller on roughly 20-30% of zlib streams because it needs 3 bytes where preflate needs tens of bytes; but precomp only reconstructs zlib-made streams efficiently, whereas preflate/reflate/grittibanzli handle any deflate stream. reflate is near-perfect only for zlib level 9 and is closed source; grittibanzli's side data is usually 20-30% smaller than preflate's but with weaker zlib-level detection, more memory and reported stability problems. — [preflate README](https://github.com/deus-libri/preflate)
- preflate speed: match finder dominates; single-thread performance "still poor"; for small streams or one core about 50-500% slower than older precomp and reflate; multi-threaded over meta-blocks since 0.3.3. Works only on raw deflate (a library, not a tool; container parsing is the caller's job). — [preflate README](https://github.com/deus-libri/preflate)
- preflate-rs 0.7.6 (Microsoft, Rust, `forbid(unsafe_code)`): same split into plaintext + CABAC-coded corrections; handles DEFLATE and PNG streams; fingerprints zlib, zlib-ng, libdeflate, miniz/miniz_oxide and Windows zlib; unknown compressors still round-trip with "higher corrections overhead"; streaming API; defaults `max_chain_length` 4096, `plain_text_limit` 128 MB, `verify_compression` true. No throughput or overhead figures are published on the docs page. — [preflate-rs on docs.rs](https://docs.rs/crate/preflate-rs/latest)
- FreeArc has `$precomp` and `$jpg` file groups, i.e. recompression is routed by file type in the planner. — [Presenting FreeArc 0.40](https://freearc.sourceforge.net/Presenting%20FreeArc%200.40.htm)

### Inferences
- JPEG: a Lepton-class coder is the single largest "free" gain available on mixed user data (20%+ where every general codec gets about 1%), and it is back-end independent because it replaces the back end for those bytes. The gain does not stack with LZ/BWT/CM; it is a route, not a filter.
- Deflate: the gain from preflate-style expansion is whatever the stronger back end achieves on the inflated plaintext minus the side data; it survives all back ends and grows with back-end strength (CM > BWT/LZMA > zstd). The cost is asymmetric in the wrong direction for kolmo's speed goals: decompression must re-run a zlib-equivalent match finder (re-deflate), so decode speed on those members falls to zlib-compress speed. This should be an archive/ratio-mode option, not the fast tier.
- Embeddability ranking by code size and licence (licences partly unverified, see Gaps): preflate / preflate-rs (Apache-2.0, verified for preflate) are directly compatible with kolmo's Apache-2.0; Lepton is compatible if Apache-2.0 as recalled; packJPG/packMP3 (LGPL as recalled) are not embeddable in a permissive static build without care.
- Lepton's rejection statistics give a design rule: a JPEG route needs a guaranteed bit-exact fallback (about 6% of real-world "JPEG" inputs fail), and baseline-only support already covers about 94%.

### Gaps
- brunsli / JPEG XL lossless JPEG transcoding: [UNVERIFIED RECALL] brunsli claims about 22% reduction, supports progressive JPEGs, is MIT-licensed and became the basis of JPEG XL's JPEG recompression mode (libjxl, BSD-3; commonly quoted 16-22%, typically ~20%); throughput on the order of 100+ MB/s. [google/brunsli](https://github.com/google/brunsli) was blocked; a search hit ("JPEG XL offers up to 22% storage savings through lossless JPEG transcoding") was not opened.
- precomp: [UNVERIFIED RECALL] handles zlib/gzip/zip/PNG/PDF/GIF/SWF/base64/bzip2 plus JPEG via packJPG and MP3 via packMP3; classic method brute-forces the 81 zlib level x memLevel combinations (plus window bits) to find parameters that reproduce the stream, with "intense"/"brute" modes for headerless streams; newer versions fall back to preflate; Apache-2.0. [schnaader/precomp-cpp](https://github.com/schnaader/precomp-cpp) was blocked. Lead not opened: [encode.su precomp thread](https://encode.su/threads/3223-precomp-further-compress-already-compressed-files).
- No sourced end-to-end numbers for PNG/ZIP/docx/PDF recompression gains (e.g. docx via inflate + LZMA/CM) or for packMP3 (recall: ~14-16% on MP3; unverified). xtool (Razor12911) not researched.
- No published MB/s for preflate or preflate-rs found; must be measured.
- Lepton licence and the status of Microsoft's `lepton_jpeg_rust` port (recall: Apache-2.0, actively maintained, deterministic across platforms) not verified this session.

## 5. Other basic reversible transforms (RLE, LZP, sparse, endian swap, base64/hex decode, UTF-16, segmentation, tar ordering)

### Takeaway
Almost nothing in this bucket could be sourced with numbers this session; what was confirmed is that mature archivers (FreeArc, 7-Zip, bsc) all implement these as planner-selected, type-routed stages (LZP before BWT for text, DICT for text, file grouping by type). Treat this section mainly as a checklist with verification pointers.

### Cited Findings
- FreeArc's filter set includes "LZP: removes repetitions in text" and "DICT: dictionary replacements for text", used ahead of PPM/GRZip (BWT) back ends, with automatic algorithm switching by file type. — [Wikipedia: FreeArc](https://en.wikipedia.org/wiki/FreeArc)
- FreeArc routes by file group (`$wav`, `$bmp`, `$precomp`, `$jpg`, `$iso`) defined in `arc.groups`, i.e. sorting/grouping members by type before solid compression. — [Presenting FreeArc 0.40](https://freearc.sourceforge.net/Presenting%20FreeArc%200.40.htm)
- The Large Text Compression Benchmark taxonomy treats dictionary coding as "usually a preprocessing step" and notes compressors "combine multiple steps such as Dict+PPM or LZP+DMC"; for paqar's WRT word-replacement transform it says this was "the biggest improvement" for text (no size given). No LTCB entry in the portion read isolates a preprocessing gain in bytes. — [Mahoney, Large Text Compression Benchmark](https://mattmahoney.net/dc/text.html)

### Inferences
- Candidate cheap transforms not in kolmo's stated inventory, ordered by expected value on mixed/binary corpora (my judgement, unsourced): (1) base64 / hex / uuencode decode-to-binary with exact re-encode metadata (line length, padding, case) — unlocks nested content such as embedded images in email/XML/JSON/PDF, and stacks with recompression routes; (2) zero-run / sparse-region elision before any back end (disk images, tar padding, .bss-like regions); (3) LZP pre-pass before BWT for highly redundant text/logs (libbsc already includes one, so kolmo inherits it when using bsc — check that it is enabled and tuned); (4) UTF-16/UTF-32 -> UTF-8 (or simple byte-plane split) for Windows resources and string tables; (5) endianness normalisation for big-endian numeric arrays so delta/shuffle predictors see LSB-first data (7-Zip has SWAP2/SWAP4 filters — recall); (6) segmentation of heterogeneous files into typed blocks before filter choice (paq8px block detection, Shkarin's seg_file) — this is the enabling stage for all embedded-table/embedded-code filters.
- Bit reversal and nibble swap have no evidence of practical gain in any source seen; low priority.

### Gaps
- [UNVERIFIED RECALL] libbsc (Apache-2.0) includes LZP preprocessing (hash-table size and minimum match length parameters, defaults around 16 bits / 128 bytes), plus data-type detection, record-size detection and "segmentation" / "reordering" preprocessing; [IlyaGrebnov/libbsc](https://github.com/IlyaGrebnov/libbsc) was blocked. No isolated gain numbers.
- Matt Mahoney's "Data Compression Explained" ([mattmahoney.net/dc/dce.html](https://mattmahoney.net/dc/dce.html)) was blocked. [UNVERIFIED RECALL] Its Transforms chapter covers RLE, LZ77/LZP preprocessing, dictionary encoding, BWT, predictive filtering (delta, colour transform, linear filtering), specialised transforms (E8E9, precomp) and deduplication, with small before/after tables; it is the most likely single source for sourced "gain survives which back end" statements and should be read in a follow-up.
- RLE before BWT (bzip2's initial RLE1 stage exists to protect the sorter from long runs rather than to gain ratio; modern suffix sorters do not need it) — recall, unverified.
- 7-Zip solid ordering (`-mqs`, sort by type/extension), string-table/resource reordering, durilca/seg_file: not sourced.
- No sourced data on base64/hex decode gains or UTF-16 conversion gains.

## 6. Ordering and combination rules from practitioner sources

### Takeaway
Confirmed rules are few: filters run before the main coder in fixed order with type-specific chains (BCJ -> LZMA2 for code, Delta -> LZMA2 for PCM), the wrong ISA filter yields nothing, and back-end literal/position parameters should match the filtered data's alignment. Broader ordering lore (dedup first, recompression before everything, exe filter before LZ but after dedup) is standard practice but was not sourced this session.

### Cited Findings
- 7z chains are strictly ordered filter -> main coder; folder coder lists are in decompression order (reverse of compression order). Standard chains: BCJ x86 -> LZMA2 for executables; Delta(2 or 4) -> LZMA2 for audio. — [zesven 7z spec, ch. 11 Filters](https://zesven.pages.dev/7z/11-filters)
- Applying the wrong architecture filter is useless rather than slightly helpful: the ARM filter "didn't do anything useful with ARM-Thumb2 code"; switching to the right one gave about 5%. — [LKML xz kernel patch](https://lore.kernel.org/lkml/20240320183846.19475-12-lasse.collin@tukaani.org/)
- Filter and back-end parameters interact: after choosing the BCJ filter, set LZMA2 `lp`/`lc` from instruction alignment (alignment 2: `lp=1`; alignment 4: `lp=2,lc=2`) for an additional 0-2%; when alignment is unknown, guess low. — [LKML xz kernel patch](https://lore.kernel.org/lkml/20240320183846.19475-12-lasse.collin@tukaani.org/)
- BCJ has a real decode-time cost at system scale: about 2x installation time for about 30 MB saved on a 1.7 GB Fedora image. — [Wikipedia: BCJ (algorithm)](https://en.wikipedia.org/wiki/BCJ_(algorithm))
- Solid-archive caveat: whether BCJ state (position counter) carries across member boundaries or resets is implementation-defined in 7z; address conversion depends on the stream position. — [zesven 7z spec, ch. 11 Filters](https://zesven.pages.dev/7z/11-filters)
- preflate cuts streams into meta-blocks for memory/parallelism at a measurable side-data cost (3 bytes -> 1.5 KiB on one 350 KiB level-1 stream): chunking a transform for speed costs ratio. — [preflate README](https://github.com/deus-libri/preflate)
- Lepton deliberately gives up cross-chunk context (independent 4 MiB chunks, multi-threaded segments) for speed and still lands within about 0.8 points of PAQ8PX. — [Lepton paper, ar5iv](https://ar5iv.labs.arxiv.org/html/1704.06192)

### Inferences
- Proposed pipeline order for kolmo, consistent with the above and with common practice (unsourced as a whole): (1) container parse / member planning (tar, zip) and type grouping; (2) recompression routes that expand or replace data (inflate via preflate-style, JPEG route, base64 decode) — these must come first because they change what later stages see; (3) long-range dedup / match pre-pass on the expanded data (so identical payloads inside differently-compressed containers dedupe); (4) segmentation into typed blocks; (5) per-block reversible filters (exe BCJ/BCJ2/disasm split, delta/table, shuffle, image/audio predictors, endian swap); (6) back end with parameters tied to the filter (alignment, stream-specific models).
- Known bad combinations (reasoned): delta or shuffle applied to non-tabular data destroys LZ matches; E8E9 on non-code produces false positives (mitigated by BCJ2's selector stream or by x86 detection — kolmo should verify its detector's false-positive handling); a dedup pre-pass with a short minimum match in front of BWT/CM hurts because it fragments contexts; an exe filter applied before dedup breaks identity of duplicate code at different offsets (absolute-address conversion is position-dependent), so dedup should precede position-dependent filters or the filter should be reset per deduped unit.
- The position-dependence point also applies inside a solid stream: identical DLLs at different offsets become non-identical after BCJ unless the filter's start offset is reset per member (the 7z x86 BCJ "start offset" property and the per-file reset option exist for this).

### Gaps
- encode.su practitioner threads (FreeArc, 7-Zip, precomp, HFCB) were located by search but not opened; no direct quotes on ordering rules obtained.
- Mahoney's DCE and the FreeArc documentation (arc.ini default chains such as `rep+exe+delta+lzma`, per-type method tables) were not accessible; [UNVERIFIED RECALL] FreeArc's default binary chain order is rep (long-range) -> exe filter -> delta -> main coder, and for text dict -> lzp -> ppmd, which matches the proposed order above.
- No sourced evidence on which filters lose their gain under context mixing versus LZ/BWT; all back-end composition statements in this file are reasoned inferences.

## Blocked URLs (permission layer refused; not retried)
- https://fgiesen.wordpress.com/2011/01/24/x86-code-compression-in-kkrunchy/
- https://mattmahoney.net/dc/dce.html
- https://7-zip.opensource.jp/chm/cmdline/switches/method.htm
- https://github.com/schnaader/precomp-cpp
- https://github.com/facebook/zstd/releases/tag/v1.3.2
- https://github.com/google/brunsli
- https://github.com/IlyaGrebnov/libbsc
- https://en.wikipedia.org/wiki/Rzip
- https://github.com/tukaani-project/xz/blob/master/NEWS
