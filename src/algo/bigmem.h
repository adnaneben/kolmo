/* Large working arrays of the block-sorting pipeline (the suffix array of the forward BWT, the PSI array of the
   inverse). Copyright 2026 Adnane Aqartit and contributors. Apache-2.0.
   On Linux they are mapped anonymously and marked for transparent huge pages (MADV_HUGEPAGE: Ubuntu's and most
   distributions' default THP policy is "madvise", so without the hint they get 4 KiB pages). Both arrays are walked
   at random (induced sorting, the inverse's chains), and with 4 KiB pages most of those accesses also miss the TLB:
   2 MiB pages made the inverse BWT 1.3-1.4x and the suffix sort up to 1.15x faster on 100 MB blocks
   (benchmark/w4/bwt/RESULTS.md). Small arrays and other systems use malloc. Contents never depend on the path. */
#ifndef KOLMO_ALGO_BIGMEM_H
#define KOLMO_ALGO_BIGMEM_H
#include <stddef.h>
#include <stdlib.h>
#if defined(__linux__)
#include <sys/mman.h>
#endif
#define KOLMO_BIG_MIN ((size_t)4 << 20)     /* below this, malloc: a huge page would not pay for the system calls */
/* KOLMO_NO_HUGEPAGE=1 (read once per process) keeps malloc for every size: the A/B switch of the benchmarks. */
static inline int kolmo_big_mapped(size_t n) {
#if defined(__linux__) && defined(MAP_ANONYMOUS)
    static int off = -1;
    if (off < 0) { const char *e = getenv("KOLMO_NO_HUGEPAGE"); off = e && *e && *e != '0'; }
    return n >= KOLMO_BIG_MIN && !off;
#else
    (void)n; return 0;
#endif
}
static inline void *kolmo_big_alloc(size_t n) {
#if defined(__linux__) && defined(MAP_ANONYMOUS)
    if (kolmo_big_mapped(n)) {
        void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) return NULL;
#ifdef MADV_HUGEPAGE
        madvise(p, n, MADV_HUGEPAGE);
#endif
        return p;
    }
#endif
    return malloc(n ? n : 1);
}
/* n must be the size given to kolmo_big_alloc. */
static inline void kolmo_big_free(void *p, size_t n) {
    if (!p) return;
#if defined(__linux__) && defined(MAP_ANONYMOUS)
    if (kolmo_big_mapped(n)) { munmap(p, n); return; }
#endif
    (void)n; free(p);
}
#endif
