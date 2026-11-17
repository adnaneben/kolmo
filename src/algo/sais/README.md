# src/algo/sais: suffix sorting, BWT and inverse BWT (taken over from libsais)

This directory holds the part of [libsais](https://github.com/IlyaGrebnov/libsais) 2.12.1 (Copyright (c) 2021-2026
Ilya Grebnov, Apache License 2.0 with LLVM Exceptions, `LICENSE` here) that Kolmo uses, as vendored by libbsc 3.4.0.
`CHANGES` is libsais's own change log. The file header of `sais.c` keeps the original copyright and licence and lists
Kolmo's changes; `NOTICE` at the repository root records the takeover.

## What Kolmo calls

| Kolmo entry point | libsais function it was | used by |
|---|---|---|
| `kolmo_sais_bwt_aux` | `libsais_bwt_aux` / `libsais_bwt_aux_omp` | the encoder of codecs 5 and 18 |
| `kolmo_sais_unbwt_aux` | `libsais_unbwt_aux` / `libsais_unbwt_aux_omp` | the decoders (blocks of 64 KiB and more carry sampled indexes) |
| `kolmo_sais_unbwt` | `libsais_unbwt` / `libsais_unbwt_omp` | the decoder of small codec 5 blocks |

Below these, Kolmo reaches 386 of libsais's static functions (all that remain in `sais.c`; the compiler reports none
unused). They form these stages:

1. **8-bit level (SA-IS, Nong, Zhang and Chan 2009).** `libsais_count_and_gather_lms_suffixes_8u_omp` classifies
   every suffix as S or L type from right to left and gathers the LMS suffixes; `libsais_radix_sort_lms_suffixes_8u_omp`
   buckets them by first symbol; `libsais_induce_partial_order_8u_omp` (`partial_sorting_scan_left_to_right_8u`,
   `..._right_to_left_8u`, `shift_markers`) induces the order of the LMS substrings; `libsais_renumber_lms_suffixes_8u_omp`
   and `gather_marked_lms_suffixes` name them. If all names are distinct the LMS suffixes are sorted; otherwise the
   reduced string of names is sorted recursively.
2. **Recursion on 32-bit alphabets** (`libsais_main_32s_recursion`, `main_32s_entry`): the same SA-IS steps on the
   reduced string, with four bucket layouts (6k, 4k, 2k, 1k: how many bucket arrays fit in the free space `fs`) and
   the nonunique-suffix sorting of 2.12 (`gather/rank/sort_nonunique_lms_suffixes`), then
   `reconstruct_lms_suffixes` and `place_lms_suffixes_*` put the sorted LMS suffixes back in the 8-bit level.
3. **Final induction with the BWT written directly** (Grebnov's "BWT induced sorting": `final_bwt_aux_scan_left_to_right_8u`
   and `..._right_to_left_8u`). Instead of storing suffix positions during the last two induced scans, each step
   stores the preceding symbol `T[SA[i] - 1]`, so the BWT comes out of the sort with no second pass over a suffix
   array, and every suffix starting at a multiple of `r` records its rank in `I` (the auxiliary indexes).
   `bwt_copy_8u` packs the result.
4. **Inverse BWT** (`libsais_unbwt_main`): blocks up to `UNBWT_PACKED_MAX_SIZE` use a packed LF table; larger ones
   build bigram-based ψ tables (`unbwt_init_single/parallel`, `calculate_biPSI`, `fastbits`) and decode two symbols
   per step, either from each sampled index on its own thread (`unbwt_decode_omp`) or, for large blocks and few
   indexes, by the chain splitting of 2.12 (`unbwt_decode_split`: marks rows every `2^marks_shift` positions, decodes
   chains between marks in parallel into chunks, then links them).

## Review

Checked against the published algorithms: the S/L classification, LMS definition, the two induced scans, naming and
the recursion follow SA-IS; the BWT-producing final scans compute exactly the symbol that a suffix-array pass would
read (each placed suffix `p` contributes `T[p - 1]`, the suffix 0 contributes the primary index), and the sampled
indexes are the ranks of suffixes `i * r`. Every function reached was compared, in Kolmo's build, with the original
libsais 2.12.1 on generated edge cases (one symbol, two symbols, periodic, long runs, random, a half repeated,
reversed ramps) of 2 bytes to 4 MB and sampling intervals 2 to 2^19, at 1 to 6 threads: identical BWT and indexes,
exact inverses (1,728 checks, also under ASan and UBSan). Codec 5 frames, which run through it, are byte-identical to
libbsc on Silesia, enwik8, CSV, JSON and logs.

Issues found:

- **Not a valid BWT.** `unbwt_aux` checks that each index is in range, but an index set that is in range and wrong
  decodes to other bytes without an error; 2.12 fixed the out-of-bounds reads such input caused in 2.10. Kolmo never
  trusts the output: codec 5 checks libbsc's adler32 of the data and every container block has an XXH3 checksum.
- **Data races reported by ThreadSanitizer**, inherited from libsais's design and present under OpenMP too: in
  `gather_lms_suffixes_8u` each thread writes speculative entries one slot past its range (overwritten in order
  afterwards), and `radix_sort_set_markers_32s_*` sets the same sign bit from two threads with a plain `|=`. Both are
  benign on x86-64 and AArch64 (the final values do not depend on the order, as the thread-count tests show); a strict
  C11 reading calls them undefined behaviour.
- `freq`, the GSA and cyclic paths and the `fs` extra space were reachable through flags Kolmo never passes; they are
  removed rather than left as untested code.

Take over or not: libsais is the fastest published suffix sorter and BWT builder on CPUs (SA-IS with cache-friendly
induction, prefetching and per-thread caches), well ahead of divsufsort and of the older SA-IS implementations; a
better algorithm is not available, so Kolmo keeps the code and owns the subset. The places where a better version is
possible are the threading layer (OpenMP is not available in Apple's toolchain and was never enabled in Kolmo's
build), the alphabet (Kolmo only needs 8 bits) and the CPU dispatch (libsais picks AVX2 or SSE2 at compile time).

## What changed

- **Subset.** The suffix array, generalized suffix array, integer alphabet, cyclic BWT, PLCP/LCP and context APIs,
  the GSA and cyclic branches of the remaining code and the functions only they used are removed: 13,861 lines to
  12,077, of which the region structs and wrappers are about 900. Done by pruning entry points and then every static
  function the compiler reported unused, so nothing reached was edited by hand.
- **OpenMP replaced by pthreads.** Each `#pragma omp parallel` region (54 of them, 5 of which were `parallel for` loops) is a
  function taking a struct with the enclosing function's parameters (by value) and the locals it shares (by pointer,
  as OpenMP shares them). `libsais_omp_parallel()` runs it on a crew of threads started once per call
  (`kolmo_crew_*` in `src/algo/par.c`); `omp_get_thread_num()`/`omp_get_num_threads()` read thread-local values the
  crew sets, `#pragma omp barrier` is a spin-then-sleep barrier, `master` is a test of thread 0, the one `critical`
  is an atomic add. The conversion was done by the scripts in `benchmark/rc3/bwt/sais-takeover/` and checked by compiling every
  region with only its captured names in scope.
- **Multithreaded inverse BWT.** libsais's threaded inverse was OpenMP-only, so Kolmo always decoded on one thread;
  it now runs on the crew, from the sampled indexes the format already stores.
- Plain `malloc`/`free`; entry points renamed `kolmo_sais_*` with an explicit thread count (no `omp_get_max_threads`).

## Measurements (x86-64 VM, Intel Xeon 2.1 GHz, 4 vCPUs, gcc 13 -O3 -march=native)

enwik8, first 32 MB, one block:

| threads | BWT (crew) | BWT (libsais + OpenMP, active wait) | inverse (crew) | inverse (OpenMP) |
|---|---|---|---|---|
| 1 | 27 MB/s | 26 MB/s | 86 MB/s | 91 MB/s |
| 4 | 58 MB/s | 59 MB/s | 355 MB/s | 310 MB/s |

OpenMP with `OMP_WAIT_POLICY=passive` reaches only 27 MB/s at 4 threads forward; under load (other processes on the
cores) OpenMP's active waiting collapses to 2 MB/s, the crew's spin-then-sleep to about 10 MB/s.

Tuning tried: barrier spin counts of 4,000, 40,000 and 200,000 pauses (no difference above noise; 4,000 kept). A
baseline x86-64 build (SSE2) against `-march=native` (AVX2): 26.4 against 28.3 MB/s forward (about 7%, at the edge
of this VM's noise), the same inverse; runtime dispatch of an AVX2 copy is left to the SIMD lane with this number.
libsais's prefetch distances were retuned upstream in 2.10.4 and were not changed.
