/* System, vendored-codec and public headers every part of the library uses, the container version, and the
 * timing helper of the profiling builds. Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md). */
#include "../kolmo.h"
#ifdef KOLMO_FRONTIER
#include "../experimental/frontier_dna.h"
#include "../experimental/frontier_table.h"
#include "../experimental/frontier_prefix.h"
#include "../experimental/frontier_runs.h"
#include "../experimental/frontier_pcm.h"
#include "../experimental/frontier_lag.h"
#include "../experimental/frontier_shuffle.h"
#define KOLMO_MAX_VERSION 10
#else
#define KOLMO_MAX_VERSION 10  /* 6: minified JSON split (log plan flag 10); 7: kolmo graph blocks; 8: line classes (flag 12); 9: native plane-context graph leaves (codec 15); 10: FASTQ family blocks (codec 16) */
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#if defined(KOLMO_TRACE) || defined(KOLMO_PROF) || defined(KOLMO_TIMING)
#include <stdio.h>
#endif
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
#include <time.h>
static double prof_now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
#endif
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
#include <x86intrin.h>
#endif
#include <stdint.h>
unsigned long long kolmo_xxh3(const void *p, size_t n, unsigned long long seed);   /* src/arch/xxh3.c */
#include "zstd.h"
#include "zdict.h"
#include "lz4.h"
#include "lz4hc.h"
#include "common/xxhash.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#ifndef KOLMO_NO_LZMA
#include "kolmo_lzma.h"   /* vendored liblzma subset (vendor/xz) */
#endif
#include "libbsc/libbsc.h"
