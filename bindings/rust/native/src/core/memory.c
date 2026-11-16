/* Memory: the per-thread scratch cache of the transforms, the output buffer type, and checked array allocation.
 * Budgets and limits: docs/MEMORY-MODEL.md. */
/* Per-thread reuse of large scratch blocks in the transforms. They allocate and free blocks of similar sizes block
 * after block; handing the same pages back avoids the page faults of fresh memory, which on the fast paths cost as
 * much as the work itself. Workers release the cache when they finish a call. */
typedef struct { size_t n, cap; } zs_hdr;                 /* 16 bytes: keeps malloc alignment */
typedef struct { zs_hdr *h; } zs_ent;
static _Thread_local zs_ent zs_cache[24]; static _Thread_local int zs_n;
#define ZS_MIN ((size_t)64 << 10)
static void *zs_malloc(size_t n) {
    if (n >= ZS_MIN) {
        int bi = -1;
        for (int i = 0; i < zs_n; i++) { size_t c = zs_cache[i].h->cap; if (c >= n && c <= 4 * n && (bi < 0 || c < zs_cache[bi].h->cap)) bi = i; }
        if (bi >= 0) { zs_hdr *h = zs_cache[bi].h; zs_cache[bi] = zs_cache[--zs_n]; h->n = n; return h + 1; }
    }
    if (n > SIZE_MAX - sizeof(zs_hdr)) return NULL;
    zs_hdr *h = malloc(sizeof(zs_hdr) + n); if (!h) return NULL;
    h->n = n; h->cap = n; return h + 1;
}
static void zs_free(void *p) {
    if (!p) return;
    zs_hdr *h = (zs_hdr *)p - 1;
    if (h->cap >= ZS_MIN && zs_n < 24) { zs_cache[zs_n++].h = h; return; }
    free(h);
}
static void *zs_calloc(size_t a, size_t b) {
    if (b && a > SIZE_MAX / b) return NULL;
    void *p = zs_malloc(a * b); if (p) memset(p, 0, a * b); return p;
}
static void *zs_realloc(void *p, size_t n) {
    if (!p) return zs_malloc(n);
    zs_hdr *h = (zs_hdr *)p - 1;
    if (n <= h->cap) { h->n = n; return p; }
    void *q = zs_malloc(n); if (!q) return NULL;
    memcpy(q, p, h->n); zs_free(p); return q;
}
static void zs_trim(void) { for (int i = 0; i < zs_n; i++) free(zs_cache[i].h); zs_n = 0; }
typedef struct { uint8_t *p; size_t n, cap; } Buf;
static void bput(Buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = zs_realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n); b->n += n;
}

/* Container allocations use checked element counts and retain the old pointer
 * on growth failure. The test-only counter exercises each cleanup path. */
#ifdef KOLMO_TESTING
static _Thread_local long kolmo_alloc_fail_after = -1;
static int kolmo_alloc_fail(void) {
    if (kolmo_alloc_fail_after < 0) return 0;
    if (!kolmo_alloc_fail_after) return 1;
    kolmo_alloc_fail_after--; return 0;
}
#else
#define kolmo_alloc_fail() 0
#endif
static void *array_alloc(size_t count, size_t width, int zero) {
    if (!width || count > SIZE_MAX / width || kolmo_alloc_fail()) return NULL;
    return zero ? calloc(count ? count : 1, width) : malloc((count ? count : 1) * width);
}
static void *array_resize(void *old, size_t count, size_t width) {
    if (!width || count > SIZE_MAX / width || kolmo_alloc_fail()) return NULL;
    return realloc(old, (count ? count : 1) * width);
}
