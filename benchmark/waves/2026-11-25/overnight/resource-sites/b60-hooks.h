/* Included only in the copied test build, after the existing KOLMO_TESTING hook. */
static _Thread_local struct {
    int active;
    long target, ordinal, hits;
    size_t live;
    void *owned[2048];
    const char *trace[2048];
} b60;

static void b60_enter(const char *label) {
    if (!b60.active) return;
    assert(b60.ordinal < 2048);
    b60.trace[b60.ordinal] = label;
    if (b60.ordinal++ == b60.target) {
        kolmo_alloc_fail_after = 0;
        b60.hits++;
    }
}
static void b60_forget(void *p) {
    if (!p || !b60.active) return;
    for (size_t i = 0; i < b60.live; i++) {
        if (b60.owned[i] == p) {
            b60.owned[i] = b60.owned[--b60.live];
            return;
        }
    }
}
static void *b60_result(void *p) {
    if (b60.active) {
        kolmo_alloc_fail_after = -1;
        if (p) {
            assert(b60.live < 2048);
            b60.owned[b60.live++] = p;
        }
    }
    return p;
}
static void *b60_array_alloc(const char *site, size_t n, size_t w, int zero) {
    b60_enter(site);
    return b60_result(array_alloc(n, w, zero));
}
static void *b60_array_resize(const char *site, void *p, size_t n, size_t w) {
    b60_enter(site);
    void *result = array_resize(p, n, w);
    if (result) b60_forget(p);
    return b60_result(result);
}
static void *b60_malloc(const char *site, size_t n) {
    b60_enter(site);
    return b60_result(kolmo_alloc_fail() ? NULL : malloc(n));
}
static void *b60_calloc(const char *site, size_t n, size_t w) {
    b60_enter(site);
    return b60_result(kolmo_alloc_fail() ? NULL : calloc(n, w));
}
static void *b60_realloc(const char *site, void *p, size_t n) {
    b60_enter(site);
    void *result = kolmo_alloc_fail() ? NULL : realloc(p, n);
    if (result) b60_forget(p);
    return b60_result(result);
}
static void b60_free(void *p) {
    b60_forget(p);
    free(p);
}
#define free(p) b60_free(p)
