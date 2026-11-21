/* System and public headers every part of the library uses, the container version, and the
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
#endif
/* The container version this build writes and the only one it reads. 12: Kolmo's own coders only (no zstd, Brotli, LZ4,
 * liblzma), kolmo_khash64 checksums, no dictionaries or trained profiles (SPEC.md). Versions 1-11 were written by
 * earlier Kolmo builds with library codecs: the decoder refuses them with KOLMO_ERR_LEGACY. */
#define KOLMO_MAX_VERSION 12
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
#include "algo/khash.h"   /* kolmo_khash64: block checksums and every other hash of the format (src/algo/khash.c) */
#include "algo/klz.h"     /* KLZ, KLZH, KLZH2 (src/algo/klz.c): also the size probe below */
/* The size probe of the planner and the transforms (plan/planner.c, plan/frontier.c, plan/meta.c, transforms/sci.c,
 * transforms/graph_typed.c): the bytes KLZH2 codes p[0..n) into at `level` on zstd's scale (1 -> KLZH2 1, 3 -> 3;
 * the mapping of codecs/native.c), n when it does not get smaller. Decision rules compare such sizes with each other;
 * only the measuring coder changed when the library coders were removed (it was zstd at levels 1 and 3, and LZ4). */
static size_t kprobe_size_l(const uint8_t *p, size_t n, int level) {
    if (!n) return 0;
    size_t cap = kolmo_klzh_bound(n); uint8_t *o = malloc(cap); if (!o) return n;
    size_t r = kolmo_klzh2_compress(p, n, o, cap, level <= 2 ? 1 : level <= 5 ? 3 : level <= 9 ? 5 : 6);
    free(o); return r && r < n ? r : n;
}
static inline size_t kprobe_size(const uint8_t *p, size_t n) { return kprobe_size_l(p, n, 1); }
#include "codecs/bwt_stream.h"   /* LZP + BWT + QLFC in C (was libbsc): compiled on its own, see the Makefile */
#include "algo/qlfc.h"
#include "algo/par.h"              /* kolmo_par_for: the video layout codes frames in parallel */
#include <sched.h>
