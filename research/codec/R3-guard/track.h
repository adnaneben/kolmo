/* Requested-heap accounting for this translation unit only. Not RSS, vendor
 * allocation internals, allocator overhead or stack. Includes temporary overlap. */
#ifndef R3_TRACK_H
#define R3_TRACK_H
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef union { max_align_t align; struct { size_t n; } v; } allocation_header;
static size_t heap_live, heap_peak, heap_calls, heap_origin;
static long heap_fail_after = -1;
static int heap_fail(void) {
    if (heap_fail_after < 0) return 0;
    if (heap_fail_after == 0) return 1;
    --heap_fail_after; return 0;
}
static void *r3_malloc(size_t n) {
    if (n > SIZE_MAX - sizeof(allocation_header) || heap_fail()) return NULL;
    allocation_header *h = malloc(sizeof(*h) + (n ? n : 1));
    if (!h) return NULL;
    h->v.n = n; heap_live += n; ++heap_calls;
    if (heap_live > heap_peak) heap_peak = heap_live;
    return h + 1;
}
static void r3_free(void *p) {
    if (!p) return;
    allocation_header *h = (allocation_header *)p - 1;
    if (h->v.n > heap_live) abort();
    heap_live -= h->v.n; free(h);
}
static void *r3_calloc(size_t n, size_t w) {
    if (w && n > SIZE_MAX/w) return NULL;
    void *p = r3_malloc(n*w); if (p) memset(p,0,n*w); return p;
}
static void *r3_realloc(void *p, size_t n) {
    if (!p) return r3_malloc(n);
    if (!n) { r3_free(p); return NULL; }
    allocation_header *old=(allocation_header *)p-1;
    void *q=r3_malloc(n); if (!q) return NULL;
    memcpy(q,p,old->v.n<n?old->v.n:n);r3_free(p);return q;
}
static void heap_reset(void) { heap_origin=heap_live;heap_peak=heap_live;heap_calls=0; }
#define malloc r3_malloc
#define calloc r3_calloc
#define realloc r3_realloc
#define free r3_free
#endif
