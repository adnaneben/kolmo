/* Elastic thread budget and longest-first dispatch (review40 #7): what runs where changes, the bytes never do.
 *  1. Frames are identical at -T 1..8 for levels 1, 3-9 on a tar of members of different sizes (blocks of different
 *     lengths, so longest-first reorders them), with the default window and with a 2 MiB window, and every frame
 *     decodes back at -T 2..8 (the decoder also starts the longest blocks first).
 *  2. The reorder buffer stays within the window: with a forced window of W bytes, the finished blocks held for an
 *     earlier one never exceed W plus one block (the last block may start just inside the window) and the per-block
 *     overhead.
 *  3. The class pipeline (algo/par.c) runs each class's chunks in order, completes every chunk, gives back every
 *     token, and advances with no helper at all (no passes, an empty budget), with helpers from the start, and with
 *     tokens given in mid-run, also when the consumer skips chunks (a parse jumping a long match).
 *  4. KLZMA and KLZH2 give the one-thread output through the elastic entry points in all those cases. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/kolmo.c"
#include "../src/algo/par.h"
#include "../src/algo/klzma.h"
#include "../src/algo/klz.h"

static uint64_t rng = 0x243F6A8885A308D3ull;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }

/* text-like (0), numeric (1) or random (2) bytes */
static void fill(uint8_t *p, size_t n, int kind) {
    static const char *w[] = {"the ", "block ", "thread ", "window ", "order ", "budget ", "kolmo ", "data ", "of ", "and "};
    size_t i = 0;
    while (i < n) {
        if (kind == 0) { const char *s = w[rnd() % 10]; size_t l = strlen(s); for (size_t k = 0; k < l && i < n; k++) p[i++] = (uint8_t)s[k]; }
        else if (kind == 1) { char b[32]; int l = snprintf(b, sizeof b, "%u,%u.%02u\n", rnd() % 100000, rnd() % 999, rnd() % 100); for (int k = 0; k < l && i < n; k++) p[i++] = (uint8_t)b[k]; }
        else p[i++] = (uint8_t)rnd();
    }
}
/* a ustar archive of members of the given sizes and kinds (member k: kind k % 3) */
static uint8_t *make_tar(const size_t *sz, int m, size_t *outn) {
    size_t n = 1024; for (int k = 0; k < m; k++) n += 512 + (sz[k] + 511) / 512 * 512;
    uint8_t *t = calloc(1, n); size_t o = 0;
    assert(t);
    for (int k = 0; k < m; k++) {
        uint8_t *h = t + o;
        snprintf((char *)h, 100, "member%02d.%s", k, k % 3 == 2 ? "bin" : "txt");
        memcpy(h + 100, "0000644", 7); memcpy(h + 108, "0000000", 7); memcpy(h + 116, "0000000", 7);
        snprintf((char *)h + 124, 12, "%011llo", (unsigned long long)sz[k]);
        memcpy(h + 136, "00000000000", 11); h[156] = '0'; memcpy(h + 257, "ustar", 6); memcpy(h + 263, "00", 2);
        memset(h + 148, ' ', 8); unsigned sum = 0; for (int q = 0; q < 512; q++) sum += h[q];
        snprintf((char *)h + 148, 8, "%06o", sum); h[155] = ' ';
        o += 512; fill(t + o, sz[k], k % 3); o += (sz[k] + 511) / 512 * 512;
    }
    *outn = n; return t;
}

static int64_t comp(const uint8_t *s, size_t n, uint8_t **out, int level, int threads, uint32_t block) {
    kolmo_params p; memset(&p, 0, sizeof p); assert(!kolmo_params_for_level(level, &p)); p.threads = threads; if (block) p.block = block;
    size_t cap = kolmo_bound(n, &p); *out = malloc(cap); assert(*out);
    int64_t r = kolmo_compress(s, n, *out, cap, &p);
    if (r <= 0) { fprintf(stderr, "level %d -T %d: compress failed %lld\n", level, threads, (long long)r); exit(1); }
    return r;
}

/* ---- 3: the class pipeline ---- */
typedef struct { int K; unsigned nch; unsigned *last; pthread_mutex_t m; } cpt;
static int cpt_fn(void *ctx, unsigned j, unsigned mask) {
    cpt *c = ctx;
    pthread_mutex_lock(&c->m);
    for (int k = 0; k < c->K; k++) if (mask >> k & 1) {
        if (c->last[k] != j) { fprintf(stderr, "class %d ran chunk %u after %u\n", k, j, c->last[k]); exit(1); }
        c->last[k] = j + 1;
    }
    int slow = rnd() % 4 == 0;
    pthread_mutex_unlock(&c->m);
    if (slow) usleep(100);
    return 0;
}
typedef struct { kolmo_budget *b; int n; } giver;
static void *give_later(void *a) { giver *g = a; usleep(2000); kolmo_budget_give(g->b, g->n); return NULL; }
static void test_pipeline(int K, int passes, int tokens, int later, unsigned step) {
    cpt c = {K, 200, calloc((size_t)K, sizeof(unsigned)), PTHREAD_MUTEX_INITIALIZER};
    kolmo_budget *b = kolmo_budget_create(tokens); assert(b && c.last);
    kolmo_cp *q = kolmo_cp_start(K, c.nch, 4, passes, b, cpt_fn, &c); assert(q);
    pthread_t th; giver g = {b, later}; if (later) pthread_create(&th, NULL, give_later, &g);
    for (unsigned j = 0; j < c.nch; j += step) {   /* step > 1: the consumer skips chunks, as a parse on a long match */
        assert(!kolmo_cp_need(q, j));
        pthread_mutex_lock(&c.m);
        for (int k = 0; k < K; k++) if (c.last[k] < j + 1) { fprintf(stderr, "chunk %u incomplete for class %d\n", j, k); exit(1); }
        pthread_mutex_unlock(&c.m);
    }
    kolmo_cp_end(q);
    if (later) pthread_join(th, NULL);
    const unsigned need = (c.nch - 1) / step * step + 1;   /* chunks up to the last one requested are complete */
    for (int k = 0; k < K; k++) if (c.last[k] < need) { fprintf(stderr, "class %d stopped at chunk %u\n", k, c.last[k]); exit(1); }
    if (kolmo_budget_free(b) != tokens + later) { fprintf(stderr, "tokens not returned: %d of %d\n", kolmo_budget_free(b), tokens + later); exit(1); }
    kolmo_budget_destroy(b); free(c.last);
}

/* ---- 4: the elastic coders against one thread ---- */
typedef size_t (*elfn)(const uint8_t *, size_t, uint8_t *, size_t, int, int, int, struct kolmo_budget *);
/* KLZH2's elastic entry with the literal-floor warm-ups on (nowarm 0), as kolmo_klzh2_compress runs them */
static size_t klzh2_el(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level, int threads, int passes, struct kolmo_budget *b) {
    return kolmo_klzh2_compress_el(in, n, out, cap, level, threads, passes, b, 0);
}
static void test_coder(const char *name, elfn el, size_t (*one)(const uint8_t *, size_t, uint8_t *, size_t, int), int level,
                       const uint8_t *in, size_t n) {
    size_t cap = n + n / 16 + 4096; uint8_t *a = malloc(cap), *b = malloc(cap);
    size_t za = one(in, n, a, cap, level); assert(za);
    /* {threads, passes, tokens at start, tokens given after 2 ms} */
    static const int cfg[][4] = {{8, 0, 0, 0}, {8, 7, 0, 0}, {8, 0, 3, 0}, {8, 2, 0, 5}, {4, 0, 0, 3}, {16, 1, 2, 9}, {3, 0, 0, 0}};
    for (unsigned k = 0; k < sizeof cfg / sizeof cfg[0]; k++) {
        kolmo_budget *bud = kolmo_budget_create(cfg[k][2]);
        pthread_t th; giver g = {bud, cfg[k][3]}; if (g.n) pthread_create(&th, NULL, give_later, &g);
        size_t zb = el(in, n, b, cap, level, cfg[k][0], cfg[k][1], bud);
        if (g.n) pthread_join(th, NULL);
        if (zb != za || memcmp(a, b, za)) { fprintf(stderr, "%s level %d: elastic configuration %u differs\n", name, level, k); exit(1); }
        if (kolmo_budget_free(bud) != cfg[k][2] + cfg[k][3]) { fprintf(stderr, "%s: tokens not returned\n", name); exit(1); }
        kolmo_budget_destroy(bud);
    }
    free(a); free(b);
}

int main(void) {
    /* 1 and 2 */
    static const size_t sz[] = {3u << 20, 200u << 10, 5u << 20, 700u << 10, 1u << 20, 2500u << 10, 90u << 10, 4u << 20, 1500u << 10, 300u << 10};
    size_t n; uint8_t *tar = make_tar(sz, (int)(sizeof sz / sizeof sz[0]), &n);
    static const int levels[] = {1, 3, 4, 5, 6, 7, 8, 9};
    for (unsigned li = 0; li < sizeof levels / sizeof levels[0]; li++) {
        const int L = levels[li]; uint8_t *ref; int64_t zr = comp(tar, n, &ref, L, 1, 0);
        uint8_t *back = malloc(n); assert(back);
        for (int T = 2; T <= 8; T++) {
            for (int small = 0; small < 2; small++) {
                kolmo_reorder_window = small ? (2u << 20) : 0;
                uint8_t *f; int64_t z = comp(tar, n, &f, L, T, 0);
                if (z != zr || memcmp(f, ref, (size_t)z)) { fprintf(stderr, "level %d -T %d window %s: frame differs\n", L, T, small ? "2 MiB" : "default"); return 1; }
                if (small) {
                    size_t bound = (2u << 20) + (5u << 20) + 64 * 16;
                    if (kolmo_reorder_held_max > bound) { fprintf(stderr, "level %d -T %d: reorder buffer %zu > %zu\n", L, T, kolmo_reorder_held_max, bound); return 1; }
                }
                free(f);
            }
            kolmo_reorder_window = 0;
            if (kolmo_decompress(ref, (size_t)zr, back, n, T) != (int64_t)n || memcmp(back, tar, n)) { fprintf(stderr, "level %d: decode -T %d\n", L, T); return 1; }
        }
        printf("level %d: identical frames at -T 1..8 (default and 2 MiB windows), %lld bytes\n", L, (long long)zr);
        free(back); free(ref);
    }
    /* the window bound with many equal blocks finishing out of order (slow text blocks first, fast random ones after) */
    {
        const size_t bn = 24u << 20; uint8_t *big = malloc(bn); assert(big);
        fill(big, bn, 2); fill(big, 6u << 20, 0);
        uint8_t *ref; int64_t zr = comp(big, bn, &ref, 4, 1, 1u << 20);
        kolmo_reorder_window = 4u << 20;
        uint8_t *f; int64_t z = comp(big, bn, &f, 4, 8, 1u << 20);
        printf("window 4 MiB, 1 MiB blocks, -T 8: reorder buffer high-water %zu bytes\n", kolmo_reorder_held_max);
        if (z != zr || memcmp(f, ref, (size_t)z)) { fprintf(stderr, "equal blocks: frame differs\n"); return 1; }
        if (kolmo_reorder_held_max > (4u << 20) + (1u << 20) + 64 * 16) { fprintf(stderr, "equal blocks: reorder buffer over the window\n"); return 1; }
        kolmo_reorder_window = 0; free(f); free(ref); free(big);
    }
    /* 3 */
    for (unsigned step = 1; step <= 3; step += 2) {
        test_pipeline(1, 0, 0, 0, step); test_pipeline(7, 0, 0, 0, step); test_pipeline(7, 7, 0, 0, step); test_pipeline(7, 0, 3, 0, step);
        test_pipeline(7, 2, 0, 5, step); test_pipeline(16, 0, 0, 16, step);
    }
    printf("class pipeline: chunks in order and complete, tokens returned (no helper, helpers, tokens in mid-run)\n");
    /* 4: 3 MiB, above the finders' 1 MiB threshold */
    uint8_t *in = malloc(3u << 20); assert(in); fill(in, 2u << 20, 0); fill(in + (2u << 20), 1u << 20, 1);
    test_coder("klzma", kolmo_klzma_compress_el, kolmo_klzma_compress, 9, in, 3u << 20);
    test_coder("klzma", kolmo_klzma_compress_el, kolmo_klzma_compress, 6, in, 3u << 20);
    test_coder("klzh2", klzh2_el, kolmo_klzh2_compress, 9, in, 3u << 20);
    test_coder("klzh2", klzh2_el, kolmo_klzh2_compress, 7, in, 3u << 20);
    printf("klzma 6 and 9, klzh2 7 and 9: elastic output identical to one thread\n");
    free(in); free(tar);
    printf("ELASTIC TESTS PASSED\n");
    return 0;
}
