#!/usr/bin/env python3
"""Writes docs/ROUTING-MAP.md: the hand-written routing notes below, with the measured tables of maptables.py.
usage: python3 mkmap.py MATRIX.jsonl XNEW.jsonl > docs/ROUTING-MAP.md"""
import io, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
tables = subprocess.run([sys.executable, os.path.join(HERE, 'maptables.py')] + sys.argv[1:3], capture_output=True,
                        text=True, check=True).stdout
sect = {}
for part in tables.split('### ')[1:]:
    name, body = part.split('\n', 1); sect[name.strip()] = body.strip()

HEAD = """# Routing map: input type × level

Status: rc3 wave 2, router lane, 17 November 2026 (branch `claude/rc3w2-router-20261117`). What Kolmo detects, which
transform and codec each level then uses, how that compares with the best tool for the type, and where Kolmo is not
the best. The ordered list of what to build next is `docs/ROUTING-PLAN.md`.

**Levels.** `speed` (internal `rt`): LZ4. `balanced` (`fast`, the default): zstd 3. `eco`: Brotli 8, window 2^24.
`hc`: zstd 19 (22 for word-transformed text). `ratio` (`max`): LZMA2 9e, block sorting (codec 18) for text and where it
wins on binary data. `-x` (`ultra`): context mixing (codec 7) with LZMA2 and block-sorting trials, plus codec 25
(columns / halves), GR6+ (codec 26) on line-structured text, the FASTQ family coder (16) and the CM image model (17).
Transforms (`-n` turns them off) are chosen by the planner per file or archive member (`src/plan/planner.c`), and
format-2 codecs (audio 19, video 20) per file.

**Measurements.** Unless a row names another source, every number comes from `benchmark/rc3/router/RESULTS.md`:
cloud VM (x86-64, 4 vCPU, 15.7 GiB), every tool on one thread (`kolmo -T 1`, `zstd -T1`, `xz -T1`), four tools at a
time (two for `-x`), so MB/s are per core on a loaded machine; every output decoded and compared. The corpus is
public data (Silesia, enwik8, OWID, Loghub, GH Archive, Kodak, ESC-50, xiph CIF, ENA SRR062634, UCSC chr22,
MiniLM-L6) cut to at most 16–25 MB per file; `-x` rows are this branch (router fix R0; times from the rc3 run, whose frames are identical except on two files), the rest are rc3 at `1536036`.
"Best general" is the smallest of zstd -3/-19/-22, xz -9e, Brotli 11, bzip2 -9 and LZ4 on the same files;
"specialist" is FLAC -8 (audio), FFV1 (video, one thread, 4 slices), JPEG XL e9 / JPEG-LS (8-bit images). Ratio =
input ÷ output. "Route" is the transform+codec covering most input bytes (`frameinfo.py`).

## How the router decides (detection order)

| step | signal | where | route |
|---|---|---|---|
| 1 | `YUV4MPEG2 ` header, 8-bit, ≥ 4 KiB | `core/compress.c` | the whole file to kvid (codec 20), every level but `speed` |
| 2 | 16-bit PCM WAV (`ka_wav_parse`) | `core/compress.c` | every block holding samples is also coded by kaud (codec 19, profile 0 up to `hc`, profile 1 at `ratio`/`-x`), the smaller kept; not at `speed` |
| 3 | tar archive | `transforms/tar.c` `segments()` | each member planned on its own (step 5 onwards) |
| 4 | trained profile (`-P`) | `plan/profile.c` | pinned plan for the member's shape and size band |
| 5 | text: ≥ 95% printable bytes in the first 64 KiB | `plan/planner.c` `make_plan` | 5a–5g; `speed` and `-Q` code text as it is |
| 5a | delimited, > 256 columns with a constant count, ≥ 64 KiB | same | wide-table graph (codec 12) when 8% below zstd -1 (and 3% below the level's coder from `eco` up) |
| 5b | minified JSON (no newline, ≥ 16 `},{`), few key layouts | same | split into records, log tokens (tflag 10) |
| 5c | JSON lines / pretty JSON (lines start with `{ [ " } ]`) | same | JSON or log tokens when 8% smaller on a zstd -1 trial; pretty JSON not at `ratio`/`-x` |
| 5d | ≥ 50 lines, 2–256 fields per line, ≥ 95% of lines alike (`, \\t ; \\| space`) | same | typed table (`tbl2`, formulas) when 3% smaller on zstd -1; at `balanced`, a first line of more than 256 fields repeated over 9 lines: transpose |
| 5e | log lines (template trial 8% smaller) | same | log templates (tflag 5); `balanced`: fixed-length line classes when smaller |
| 5f | other text at `eco` and up | same | word dictionary when 1% smaller on zstd -3 |
| 5g | `ratio`: block sorting for text blocks; `-x`: CM (transform kept only if CM on a 1 MiB transformed sample wins; tables also tried as codec 25 columns) | `core/blocks.c`, `plan/parallel.c` | |
| 6 | binary: a byte-plane width whose planes are more predictable than the bytes | `make_plan` | shuffle when 5% below plain on a zstd -1 trial (32 KiB) |
| 7 | binary ≥ 256 KiB: candidates on a 1 MiB middle slice, scored by zstd: x86 E8/E9 density ≥ 1/1000, 16-bit image, fixed records, float32/16 (exponent predictable, mantissa random), integer arrays, DEC64 (decimal doubles), 8-bit image (row lag) | same | the cheapest wins if 3% below plain (DEC64: 7% below the others); four 32 KiB probes elsewhere mark mixed files (`verify`) |
| 8 | `.npy` header | same | dtype and shape fix width, header and row length |
| 9 | FASTQ records (`@`, `+`, equal lengths), block ≥ 64 KiB | `core/blocks.c` | `-x` only: family coder (codec 16) first, CM skipped |
| 10 | incompressible | `core/blocks.c` | stored when the coder does not shrink the block; `speed`/`balanced` find it fast, slower levels spend their full coder time first (plan item 6) |
"""

NOTES = {
 'archive': ('tar of 7 mixed members: prose, CSV, PPM, WAV, x86 DLL, log, 16-bit image', 'tar members planned one by one (step 3); each member then routes as its type',
             'Ahead of the level rival at every level (`speed` −7% vs LZ4 through the shuffle on the x-ray member; `ratio` −20% vs xz), because members get their own transforms. The WAV member is coded as plain bytes: the audio route (step 2) needs the whole file to be a WAV'),
 'audio': ('16-bit PCM WAV (ESC-50 clips, 44.1 kHz mono, and the clips joined)', 'kaud (codec 19): profile 0 (LPC + Rice) from `balanced` to `hc`, profile 1 (LMS cascade + binary coder) at `ratio`/`-x`; `speed` is LZ4 (by design)',
           'Beats FLAC -8 from `balanced` up (−5.5%, −9.2% at `ratio`); decodes slower than FLAC (77 against 126 MB/s at `balanced`, 10 MB/s at `ratio`). Mac, 40 clips 10 at a time: p0 −4.0%, p1 −8.2% vs FLAC -8 (`benchmark/rc2/results/mt/MT.md`). Leaders not measured here: OptimFROG, Monkey\'s Audio. A WAV inside a tar is not routed to kaud (gap)'),
 'compressed': ('JPEG, PNG, .tar.gz', 'no detection: every level runs its coder and stores the block when it does not shrink',
                'Nothing to gain (≤ 1.2% at `-x`), but `ratio` spends 4 MB/s and `-x` 0.2 MB/s on it (plan item 6). Recompression of JPEG (lepton/brunsli class; published gains about 20%) and of deflate streams (precomp class) is not in Kolmo and not measured here'),
 'csv_long': ('long tables: covid daily counts per country (5 columns), OWID CO2 melted to 4 columns', 'typed table `tbl2` with formulas at every level but `speed`; `-x`: GR6+ (covid) or codec 25 columns',
              'Far ahead of every general tool (xz −44% at `ratio`). The 6-bit study\'s GR6+ is 8–17% below `ratio` on these (plan item 3)'),
 'csv_num': ('sparse integer matrix CSV (HDFS event counts, 32 columns, mostly 0/1)', '`balanced` to `hc`: typed transpose; `ratio`: plain block sorting (the transpose lost); `-x`: CM',
             'Ahead of the level rival at `balanced` (−14%), `hc` (−4%), `ratio` (−7% vs xz) and `-x` (−16%); `eco` is +10% vs Brotli 11; `ratio` is only 2.6% below bzip2 -9 (the transpose loses to plain block sorting there). A sparse-count model (zero runs per column) is not in the table transform'),
 'csv_wide': ('wide tables: OWID CO2 (79 columns), OWID energy (130 columns)', '`tbl2` with derived-column formulas; `-x`: GR6+ on CO2, table + LZMA2 on energy (R0: was table + CM, 1% larger than `ratio`)',
              '−55% to −63% against the best general tool, −58% to −68% against each level\'s rival. The GR6+ trial costs +126% compress time on energy for nothing (plan item 7)'),
 'exe': ('x86 code: Silesia mozilla (tar of 2002 Linux x86 binaries), ooffice (Windows DLL), first 16 MB of libLLVM.so (x86-64)', 'x86 call/jump filter when E8/E9 density ≥ 1/1000 (ooffice); DEC64 graph on the ELF\'s relocation and symbol tables (`balanced`–`hc`); else plain',
         '`ratio` −2.7% vs xz (mozilla +0.8%, ooffice −14% through the x86 filter, libLLVM equal); `-x` −12.8%. The x86 filter is not chosen on mozilla, and xz\'s BCJ is larger there too (0.6%), so not a miss. Leaders not measured: zpaq/paq8 with exe models (`-x` vs zpaq -m5 general −7.4%, rc2)'),
 'fasta': ('chr22 of hg38, 16 MB from 20 MB on (upper/lower case runs, N runs)', '`ratio`: block sorting; `-x`: CM; no DNA-specific route in release builds (the frontier DNA codecs are experimental)',
           '`ratio` −6.4% vs xz; `-x` −15.9%. kfam one-stream family is +9.9% vs `-x` on chr22 (rc2). `-x` is 1.73 bits per byte of file here; DNA specialists (2-bit packing with order-k models) were not measured'),
 'fastq': ('Illumina FASTQ, SRR062634 (ENA), first 24 MB', '`-x`: family coder (codec 16); every other level: plain coder (`ratio`: block sorting)',
           '`-x` (codec 16) is −10.4% vs `ratio` at 2.3–3.2× its compress and 3.6–4.9× its decode time (plan item 4). Mac: codec 16 −34.3% vs CRAM 3.1 at 3.4× its compress speed, 0.25× its decode (`MT.md`)'),
 'float': ('Silesia sao: star catalog, 28-byte records of mixed float/int fields', 'fixed-record transform (record period found by lag search) at every level but `speed`',
           '−50% vs xz at `ratio`; `-x` −52%'),
 'img16': ('Silesia x-ray (12-bit in 16), mr (16-bit DICOM)', '16-bit image transform (row stride search, median prediction) everywhere but `speed`; `-x`: + CM',
           '−8% to −15% against the best general tool (bzip2), −21% to −38% against each level\'s rival. Mac: JPEG-LS beats `balanced` by 1.8% on x-ray, `-x` beats JPEG-LS by 0.9%; JPEG XL e9 is 5.9% below `-x` on MR (`MT.md`)'),
 'img8': ('Kodak photographs as PPM (RGB) and PGM (grey)', 'IMG8 graph (median predictor per channel, row length from the header or a lag search); `ratio`: + block-sorted leaves; `-x`: CM image model (codec 17)',
          '**Largest gap.** `balanced` +60%, `ratio` +25% against JPEG XL e9 (RGB alone: +63% at `balanced`), and bzip2 -9 is smaller than `balanced`, `eco` and `hc`; only `-x` wins (−9.7%), at 0.2 MB/s. No colour decorrelation: a reversible colour transform alone cuts the residual entropy by 30% (plan item 1)'),
 'json': ('GH Archive events: pretty-printed (indent 2) and minified array', 'pretty: JSON/log tokens (`balanced`–`hc`), log tokens + LZMA2 (`ratio`), CM (`-x`); minified: plain (`balanced`), word dictionary (`eco`–`ratio`), CM',
          'Close to Brotli 11 at `ratio` (−1.4%); `-x` −33%. The minified-JSON split (step 5b) was not chosen (route: plain zstd at `balanced`)'),
 'jsonl': ('GH Archive JSON lines (one event per line, nested, heterogeneous)', '`balanced`: plain zstd (the token trial does not gain 8%); `eco`–`ratio`: word dictionary; `-x`: CM',
           '`balanced` is +4.9% against single-stream zstd -3: eight independent 2 MiB blocks (plan item 5). `ratio` ≈ Brotli 11 (−0.5%); `-x` −32%'),
 'logs': ('Loghub Apache, Linux, SSH (16 MB), HDFS (16 MB)', 'log templates with typed fields at every level but `speed` (`ratio`: + LZMA2 or block sorting); `-x`: CM over the plain lines',
          '−36% to −57% against each level\'s rival (−18% to −57% against the best general tool). GR6+ with `lzp` phrases would be 7–30% below `ratio` on Apache, Linux, HDFS (plan item 3); `-x` stays 19–31% below that'),
 'npy': ('.npy arrays derived from OWID CO2: float64 decimal, float32 (computed, not decimal), int64, int32', 'graph per dtype: DEC64, integer (float32 coded as int4 here), with per-plane leaves (codecs 21–23)',
         '−30% to −43% against the general tools. Mac: kbin (research) equals `balanced` over 11 NPY files'),
 'other': ('Silesia nci (chemical structures, text), osdb (database pages)', 'nci: line classes (`balanced`), log templates (`eco`/`hc`), block sorting (`ratio`); osdb: plain, block sorting at `ratio` (slice test), `-x`: block sorting (R0: was CM, +1.0% over `ratio`)',
           '`ratio` −21% vs Brotli 11'),
 'prose': ('Silesia dickens (English), reymont (Polish), Webster dictionary (16 MB)', 'word dictionary at `eco`/`hc` where it gains (dickens); `ratio`: block sorting; `-x`: CM',
           '`ratio` −21% vs bzip2 at 14 MB/s; `-x` −31%. `balanced` +1.0% vs zstd -3 and `eco` +9.9% vs Brotli 11 (Brotli 8 by design)'),
 'raw_float': ('raw float64 (decimal values, no header)', 'DEC64 graph found by the decimal probe', '−34% vs bzip2; `ratio` is 1.3% larger than `hc` here (BWT on decimal planes; plan item 10)'),
 'raw_int': ('raw int64 (population counts, no header)', 'integer graph, width found by trial', '−34% to −46% against the general tools'),
 'source': ('Silesia samba (tar of C sources), Kolmo `src/` as a tar', 'plain coders (tar members are text; the word transform does not gain on code); `ratio`: LZMA2 or block sorting',
            '`ratio` = xz (−0.4%); `-x` −21%. No code-specific model (identifier dictionaries, indentation); `balanced` +2.8% vs zstd -3 (block split)'),
 'tsv': ('OWID CO2 as TSV', 'same route as CSV (tab is a delimiter candidate)', 'Same as csv_wide'),
 'video': ('xiph CIF 4:2:0 Y4M: akiyo (static), foreman (motion), 300 frames each', 'kvid (codec 20) everywhere but `speed`: per-strip MED / temporal / temporal + MED prediction, layout coded by a nested Kolmo frame',
           '−15% (`balanced`) to −32% (`-x`) against FFV1 (one thread, 4 slices); `balanced` decodes at 139 MB/s per core. Mac, 3 clips, all threads: `balanced` −14% vs FFV1 at 0.94× its compress and 2.1× its decode speed (`MT.md`)'),
 'weights': ('MiniLM-L6 safetensors: float32 (first 24 MB) and the same tensors as bf16', 'float graph (exponent and mantissa planes) from `balanced` up; `speed` stores (LZ4 cannot shrink it); `-x` on bf16: CM over the plain bytes',
             '−7% to −13% against the general tools. Mac: `balanced` −0.8% vs ZipNN (10 threads) at 2.1× its compress and 4.8× its decode speed. `-x` on bf16 runs CM for 208 s for −0.7% (plan item 11)'),
 'xml': ('enwik8 (16 MB), Silesia xml, three Wikipedia HTML pages', 'word dictionary at `eco`–`ratio` (enwik8, xml); `ratio`: block sorting; `-x`: CM',
         '`ratio` −14% vs Brotli 11; `-x` −26%. No markup-aware transform (tag/attribute dictionaries); `balanced` +1.0% vs zstd -3'),
}
ORDER = ['csv_long', 'csv_wide', 'tsv', 'csv_num', 'json', 'jsonl', 'logs', 'prose', 'source', 'xml', 'npy', 'raw_int', 'raw_float',
         'float', 'weights', 'img8', 'img16', 'audio', 'video', 'fastq', 'fasta', 'exe', 'other', 'archive', 'compressed']
TITLE = {'csv_long': 'CSV, long (few columns, many rows)', 'csv_wide': 'CSV, wide', 'tsv': 'TSV', 'csv_num': 'CSV, sparse integer matrix',
         'json': 'JSON documents (pretty and minified)', 'jsonl': 'JSON lines', 'logs': 'Logs', 'prose': 'Prose', 'source': 'Source code',
         'xml': 'XML / HTML', 'npy': 'NPY arrays', 'raw_int': 'Raw integer array', 'raw_float': 'Raw float array (decimal values)',
         'float': 'Binary records with floats', 'weights': 'Model weights', 'img8': 'Images, 8-bit', 'img16': 'Images, 16-bit',
         'audio': 'Audio (WAV)', 'video': 'Video (Y4M)', 'fastq': 'FASTQ', 'fasta': 'FASTA', 'exe': 'Executables',
         'other': 'Other binary and scientific text', 'archive': 'Archives of mixed files', 'compressed': 'Already-compressed data'}

out = io.StringIO()
out.write(HEAD)
out.write('\n## The map\n\nEach type: what it is in the corpus, the detection signal and route, the measured table, and the '
          'comparison with the leader. Negative = Kolmo smaller.\n')
for t in ORDER:
    what, route, verdict = NOTES[t]
    out.write('\n### %s\n\n**Corpus:** %s. **Route:** %s.\n\n%s\n\n**Against the leaders:** %s.\n' % (TITLE[t], what, route, sect[t], verdict))
out.write(open(os.path.join(HERE, 'map_tail.md')).read())
sys.stdout.write(out.getvalue())
