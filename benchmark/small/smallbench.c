/* Small-input benchmark: Kolmo levels 1-9 (one and ten threads) against zstd, Brotli and LZ4 on items of 64 B to
 * 64 KiB cut from several data kinds. Per (kind, size, coder, level, threads) it prints one TSV row:
 *   kind size coder level threads items raw_bytes out_bytes ratio c_us d_us c_kib d_kib ok hash frame_bytes
 * c_us / d_us: microseconds per call, the best of several passes over all items after a warm-up pass; rivals reuse
 *   one context per setting (zstd CCtx/DCtx, Brotli and LZ4F instances), their fastest small-item use.
 * c_kib / d_kib: the largest heap working set of one call: the peak of live heap bytes during the call above the bytes
 *   live before it, with fresh rival contexts created and freed inside the call (so their context memory counts, as
 *   Kolmo's per-call allocations do). Kolmo is compiled from src/kolmo.c with its malloc, calloc, realloc and free
 *   calls retargeted to mt_* (run.sh; malloc_size granularity); rivals through their custom-allocator hooks. Thread stacks and
 *   mmap are not counted (no Kolmo route taken by inputs of 64 KiB or less maps memory).
 * ok: 1 when every item decoded back to its bytes (memcmp). hash: FNV-1a 64 of every compressed item in order (two
 *   builds wrote the same bytes when it matches). frame_bytes: Kolmo's container bytes in out_bytes (32 header + 16 index
 *   + 4 plan prefix per block; 0 for rivals), the part a compact frame would replace.
 * Frames: Kolmo's default frame, zstd's default frame (content size, no checksum), the LZ4 frame (no content
 * checksum), a raw Brotli stream (no container).
 *
 * usage: smallbench [-q] [-s SIZES] [-l LEVELS] [-T THREADS] [-c CODERS] KIND=FILE:CUT ...
 *   CUT: line (each item starts at a line start) or raw (at a multiple of min(size, 4096))
 *   -q: sizes and memory only (no timing); -s 64,256: item sizes; -l 1,3: Kolmo levels; -T 1,10: Kolmo threads;
 *   -c kolmo,zstd,brotli,lz4: coders. Items per size: min(256, 4 MiB / size), at least 16, spread over the file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdatomic.h>
#include "kolmo.h"
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <brotli/encode.h>
#include <brotli/decode.h>
#define LZ4F_STATIC_LINKING_ONLY
#include <lz4frame.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#define MT_SIZE(p) malloc_size(p)
#else
#include <malloc.h>
#define MT_SIZE(p) malloc_usable_size(p)
#endif

/* ---------------- heap accounting ---------------- */
static _Atomic long long g_cur, g_peak;
static void mt_add(long long d) {
    long long c = atomic_fetch_add(&g_cur, d) + d, p = atomic_load(&g_peak);
    while (c > p && !atomic_compare_exchange_weak(&g_peak, &p, c)) {}
}
void *mt_malloc(size_t n) { void *p = malloc(n); if (p) mt_add((long long)MT_SIZE(p)); return p; }
void *mt_calloc(size_t a, size_t b) { void *p = calloc(a, b); if (p) mt_add((long long)MT_SIZE(p)); return p; }
void *mt_realloc(void *q, size_t n) {
    long long old = q ? (long long)MT_SIZE(q) : 0; void *p = realloc(q, n);
    if (p) mt_add((long long)MT_SIZE(p) - old); else if (!n) mt_add(-old);
    return p;
}
void mt_free(void *p) { if (p) { mt_add(-(long long)MT_SIZE(p)); free(p); } }
static void *r_alloc(void *o, size_t n) { (void)o; return mt_malloc(n); }
static void r_free(void *o, void *p) { (void)o; mt_free(p); }
static const ZSTD_customMem ZMEM = {r_alloc, r_free, NULL};
static const LZ4F_CustomMem LMEM = {r_alloc, NULL, r_free, NULL};

/* ---------------- items ---------------- */
typedef struct { const uint8_t *p; size_t n; } item;
static uint8_t *slurp(const char *f, size_t *n) {
    FILE *F = fopen(f, "rb"); if (!F) { perror(f); exit(1); }
    fseek(F, 0, SEEK_END); *n = (size_t)ftell(F); fseek(F, 0, SEEK_SET);
    uint8_t *b = malloc(*n + 1); if (fread(b, 1, *n, F) != *n) { perror(f); exit(1); } fclose(F); return b;
}
/* up to cnt items of exactly sz bytes spread evenly over the file, not overlapping */
static size_t cut(const uint8_t *b, size_t n, size_t sz, int line, item *v, size_t cnt) {
    if (n < sz) return 0;
    size_t k = 0, span = n - sz, last_end = 0;
    for (size_t i = 0; i < cnt; i++) {
        size_t s = span * i / (cnt > 1 ? cnt - 1 : 1);
        if (!line) s -= s % (sz < 4096 ? sz : 4096);
        if (s < last_end) s = last_end;
        if (line && s > 0) { while (s < n && b[s - 1] != '\n') s++; }
        if (s + sz > n) break;
        v[k].p = b + s; v[k].n = sz; k++; last_end = s + sz;
    }
    return k;
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + t.tv_nsec * 1e-9; }

/* ---------------- coders ---------------- */
enum { C_KOLMO, C_ZSTD, C_BROTLI, C_LZ4 };
static const char *CN[] = {"kolmo", "zstd", "brotli", "lz4"};
typedef struct { int coder, level, threads; ZSTD_CCtx *zc; ZSTD_DCtx *zd; LZ4F_cctx *lc; LZ4F_dctx *ld; } cfg;
static void ctx_new(cfg *c) {
    if (c->coder == C_ZSTD) { c->zc = ZSTD_createCCtx_advanced(ZMEM); c->zd = ZSTD_createDCtx_advanced(ZMEM); }
    if (c->coder == C_LZ4) { c->lc = LZ4F_createCompressionContext_advanced(LMEM, LZ4F_VERSION); c->ld = LZ4F_createDecompressionContext_advanced(LMEM, LZ4F_VERSION); }
}
static void ctx_free(cfg *c) {
    if (c->zc) ZSTD_freeCCtx(c->zc); if (c->zd) ZSTD_freeDCtx(c->zd);
    if (c->lc) LZ4F_freeCompressionContext(c->lc); if (c->ld) LZ4F_freeDecompressionContext(c->ld);
    c->zc = NULL; c->zd = NULL; c->lc = NULL; c->ld = NULL;
}
static int64_t enc(cfg *c, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    switch (c->coder) {
    case C_KOLMO: {
        kolmo_params p = {0}; if (kolmo_params_for_level(c->level, &p)) return -1; p.threads = c->threads;
        return kolmo_compress(in, n, out, cap, &p);
    }
    case C_ZSTD: { size_t r = ZSTD_compressCCtx(c->zc, out, cap, in, n, c->level); return ZSTD_isError(r) ? -1 : (int64_t)r; }
    case C_BROTLI: {   /* an encoder instance per call: Brotli has no reset, and BrotliEncoderCompress does the same */
        BrotliEncoderState *s = BrotliEncoderCreateInstance(r_alloc, r_free, NULL); if (!s) return -1;
        BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, (uint32_t)c->level);
        BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, BROTLI_DEFAULT_WINDOW);
        BrotliEncoderSetParameter(s, BROTLI_PARAM_SIZE_HINT, (uint32_t)n);
        size_t ai = n, ao = cap; const uint8_t *ip = in; uint8_t *op = out;
        int ok = BrotliEncoderCompressStream(s, BROTLI_OPERATION_FINISH, &ai, &ip, &ao, &op, NULL) && BrotliEncoderIsFinished(s);
        BrotliEncoderDestroyInstance(s); return ok ? (int64_t)(cap - ao) : -1;
    }
    case C_LZ4: {
        LZ4F_preferences_t pr; memset(&pr, 0, sizeof pr); pr.compressionLevel = c->level;
        size_t r = LZ4F_compressFrame_usingCDict(c->lc, out, cap, in, n, NULL, &pr); return LZ4F_isError(r) ? -1 : (int64_t)r;
    }
    }
    return -1;
}
static int64_t dec(cfg *c, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    switch (c->coder) {
    case C_KOLMO: return kolmo_decompress(in, n, out, cap, c->threads);
    case C_ZSTD: { size_t r = ZSTD_decompressDCtx(c->zd, out, cap, in, n); return ZSTD_isError(r) ? -1 : (int64_t)r; }
    case C_BROTLI: {
        BrotliDecoderState *s = BrotliDecoderCreateInstance(r_alloc, r_free, NULL); if (!s) return -1;
        size_t ai = n, ao = cap; const uint8_t *ip = in; uint8_t *op = out;
        BrotliDecoderResult r = BrotliDecoderDecompressStream(s, &ai, &ip, &ao, &op, NULL);
        BrotliDecoderDestroyInstance(s); return r == BROTLI_DECODER_RESULT_SUCCESS ? (int64_t)(cap - ao) : -1;
    }
    case C_LZ4: {
        size_t on = cap, inn = n; LZ4F_resetDecompressionContext(c->ld);
        size_t r = LZ4F_decompress(c->ld, out, &on, in, &inn, NULL);
        return (LZ4F_isError(r) || r != 0 || inn != n) ? -1 : (int64_t)on;
    }
    }
    return -1;
}
/* heap working set of one call (fresh rival contexts inside it) */
static long long one_call_peak(cfg *c, int d, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    cfg f = *c; f.zc = NULL; f.zd = NULL; f.lc = NULL; f.ld = NULL;
    long long base = atomic_load(&g_cur); atomic_store(&g_peak, base);
    ctx_new(&f); if (d) dec(&f, in, n, out, cap); else enc(&f, in, n, out, cap); ctx_free(&f);
    long long g = atomic_load(&g_peak) - base; return g > 0 ? g : 0;
}

static int parse_list(const char *s, int *v, int max) { int k = 0; while (*s && k < max) { v[k++] = atoi(s); while (*s && *s != ',') s++; if (*s) s++; } return k; }

int main(int argc, char **argv) {
    int quick = 0, sizes[16] = {64, 256, 1024, 4096, 16384, 65536}, ns = 6, lv[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9}, nl = 9;
    int thr[4] = {1, 10}, nt = 2, use[4] = {1, 1, 1, 1}, a = 1;
    for (; a < argc && argv[a][0] == '-'; a++) {
        if (!strcmp(argv[a], "-q")) quick = 1;
        else if (!strcmp(argv[a], "-s") && a + 1 < argc) ns = parse_list(argv[++a], sizes, 16);
        else if (!strcmp(argv[a], "-l") && a + 1 < argc) nl = parse_list(argv[++a], lv, 16);
        else if (!strcmp(argv[a], "-T") && a + 1 < argc) nt = parse_list(argv[++a], thr, 4);
        else if (!strcmp(argv[a], "-c") && a + 1 < argc) { const char *s = argv[++a]; for (int i = 0; i < 4; i++) use[i] = strstr(s, CN[i]) != NULL; }
        else { fprintf(stderr, "unknown option %s\n", argv[a]); return 2; }
    }
    if (a >= argc) { fprintf(stderr, "usage: smallbench [-q] [-s SIZES] [-l LEVELS] [-T THREADS] [-c CODERS] KIND=FILE:CUT ...\n"); return 2; }
    /* rivals at comparable levels: zstd 1/3/9/19, Brotli 1/5/9/11, LZ4 fast (1) and HC (9) */
    cfg C[64]; int nc = 0;
    if (use[C_KOLMO]) for (int i = 0; i < nl; i++) for (int t = 0; t < nt; t++) C[nc++] = (cfg){C_KOLMO, lv[i], thr[t], 0, 0, 0, 0};
    static const int ZL[] = {1, 3, 9, 19}, BL[] = {1, 5, 9, 11}, LL[] = {1, 9};
    if (use[C_ZSTD]) for (int i = 0; i < 4; i++) C[nc++] = (cfg){C_ZSTD, ZL[i], 1, 0, 0, 0, 0};
    if (use[C_BROTLI]) for (int i = 0; i < 4; i++) C[nc++] = (cfg){C_BROTLI, BL[i], 1, 0, 0, 0, 0};
    if (use[C_LZ4]) for (int i = 0; i < 2; i++) C[nc++] = (cfg){C_LZ4, LL[i], 1, 0, 0, 0, 0};
    for (int i = 0; i < nc; i++) ctx_new(&C[i]);
    printf("kind\tsize\tcoder\tlevel\tthreads\titems\traw_bytes\tout_bytes\tratio\tc_us\td_us\tc_kib\td_kib\tok\thash\tframe_bytes\n");
    const size_t cap = (1u << 17) + (64u << 10);
    uint8_t *out = malloc(cap), *back = malloc(cap);
    for (; a < argc; a++) {
        char *spec = strdup(argv[a]), *eq = strchr(spec, '='), *colon = strrchr(spec, ':');
        if (!eq || !colon || colon < eq) { fprintf(stderr, "bad set %s\n", argv[a]); return 2; }
        *eq = 0; *colon = 0; const char *kind = spec, *file = eq + 1; int line = !strcmp(colon + 1, "line");
        size_t fn; uint8_t *fb = slurp(file, &fn);
        for (int si = 0; si < ns; si++) {
            size_t sz = (size_t)sizes[si], want = (4u << 20) / sz; if (want > 256) want = 256; if (want < 16) want = 16;
            item *it = malloc(want * sizeof *it); size_t ni = cut(fb, fn, sz, line, it, want);
            if (!ni) { free(it); continue; }
            uint8_t **ct = malloc(ni * sizeof *ct); size_t *cl = malloc(ni * sizeof *cl);
            for (int ci = 0; ci < nc; ci++) {
                cfg *c = &C[ci]; size_t tot = 0; int ok = 1; uint64_t hv = 1469598103934665603ull; size_t frb = 0;
                for (size_t i = 0; i < ni; i++) {   /* warm-up pass: sizes and round trips */
                    int64_t r = enc(c, it[i].p, it[i].n, out, cap);
                    if (r < 0) { fprintf(stderr, "%s %d: compress failed (%lld)\n", CN[c->coder], c->level, (long long)r); return 1; }
                    ct[i] = malloc((size_t)r); memcpy(ct[i], out, (size_t)r); cl[i] = (size_t)r; tot += (size_t)r;
                    for (int64_t k = 0; k < r; k++) hv = (hv ^ out[k]) * 1099511628211ull;
                    if (c->coder == C_KOLMO && r >= 32) { uint32_t nb; memcpy(&nb, out + 20, 4); frb += 32 + 20 * (size_t)nb; }
                    int64_t d = dec(c, ct[i], cl[i], back, cap);
                    if (d != (int64_t)it[i].n || memcmp(back, it[i].p, it[i].n)) ok = 0;
                }
                long long cpk = 0, dpk = 0;
                for (size_t i = 0; i < ni; i += 1 + ni / 32) {   /* heap working set, on up to 32 items */
                    long long g = one_call_peak(c, 0, it[i].p, it[i].n, out, cap); if (g > cpk) cpk = g;
                    g = one_call_peak(c, 1, ct[i], cl[i], back, cap); if (g > dpk) dpk = g;
                }
                double cu = 0, du = 0;
                if (!quick) for (int ph = 0; ph < 2; ph++) {   /* the best pass of at least 3 and 0.15 s */
                    double best = 1e30, spent = 0; int passes = 0;
                    while (passes < 3 || (spent < 0.15 && passes < 200)) {
                        double t0 = now();
                        if (!ph) for (size_t i = 0; i < ni; i++) enc(c, it[i].p, it[i].n, out, cap);
                        else for (size_t i = 0; i < ni; i++) dec(c, ct[i], cl[i], back, cap);
                        double t = now() - t0; spent += t; passes++; if (t < best) best = t;
                    }
                    if (!ph) cu = best * 1e6 / (double)ni; else du = best * 1e6 / (double)ni;
                }
                printf("%s\t%zu\t%s\t%d\t%d\t%zu\t%zu\t%zu\t%.4f\t%.2f\t%.2f\t%.1f\t%.1f\t%d\t%016llx\t%zu\n", kind, sz, CN[c->coder], c->level,
                       c->threads, ni, sz * ni, tot, (double)(sz * ni) / (double)tot, cu, du, cpk / 1024.0, dpk / 1024.0, ok, (unsigned long long)hv, frb);
                fflush(stdout);
                for (size_t i = 0; i < ni; i++) free(ct[i]);
                if (!ok) { fprintf(stderr, "ROUND TRIP FAILED: %s %zu %s %d\n", kind, sz, CN[c->coder], c->level); return 1; }
            }
            free(ct); free(cl); free(it);
        }
        free(fb); free(spec);
    }
    for (int i = 0; i < nc; i++) ctx_free(&C[i]);
    return 0;
}
