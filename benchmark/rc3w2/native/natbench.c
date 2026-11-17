/* natbench: Kolmo-native coders against the libraries they would replace (rc3 wave 2, native lane).
 *
 * usage: natbench CODEC[,CODEC...] THREADS BLOCK_KIB REPS FILE...
 * Each file is cut into blocks of BLOCK_KIB (the way Kolmo cuts a file), the blocks are coded by THREADS workers (one
 * block per task, as Kolmo's block pool), every block is decoded and compared. Prints per codec: input bytes, output
 * bytes, ratio, compress and decompress MB/s (wall time, best of REPS for decode, one pass for compress when REPS
 * is 1). Codecs:
 *   lz4            LZ4_compress_fast (acceleration 1)              klz1   Kolmo fast LZ, byte-aligned (codec 28)
 *   zstd1 zstd3 zstd9 zstd19                                       klzh1..klzh9   Kolmo LZ + khuf (codec 29)
 *   huf            zstd HUF_compress4X_repeat in 128 KiB chunks     khuf   Kolmo Huffman chunks (codec 27)
 *   brotli8        Brotli quality 8, window 24
 *   lzma9e         raw LZMA2 preset 9e (liblzma 5.8.1)             klzma1..klzma9  Kolmo LZMA-class (codec 24)
 *   xxh3           XXH3-64 over the block (speed only)             khash  Kolmo 64-bit hash (speed only)
 */
#define _GNU_SOURCE
#define HUF_STATIC_LINKING_ONLY
#define ZDICT_STATIC_LINKING_ONLY
#include "common/huf.h"
#include "common/cpu.h"
#include "zstd.h"
#include "lz4.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#include "kolmo_lzma.h"
#include "algo/khuf.h"
#include "algo/klz.h"
#include "algo/klzma.h"
#include "algo/khash.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

unsigned long long kolmo_xxh3(const void *p, size_t n, unsigned long long seed);
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static size_t put_uv(uint8_t *p, uint64_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
static int get_uv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int sh = 0;
    while (*i < n && sh < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << sh; if (!(c & 128)) { *v = r; return 0; } sh += 7; }
    return -1;
}
static int hufl(void) { static int f = -1; if (f < 0) f = ZSTD_cpuid_bmi2(ZSTD_cpuid()) ? HUF_flags_bmi2 : 0; return f; }
/* zstd HUF in the framing of Kolmo's codec 13 (src/codecs/huf.c) */
static size_t huf_c(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    U64 wk[HUF_WORKSPACE_SIZE_U64]; HUF_CREATE_STATIC_CTABLE(ct, HUF_SYMBOLVALUE_MAX); size_t o = 0;
    for (size_t i = 0; i < n;) {
        size_t m = n - i < HUF_BLOCKSIZE_MAX ? n - i : HUF_BLOCKSIZE_MAX;
        if (cap - o < m + 16) return 0;
        uint8_t *h = out + o; size_t hp = 1 + put_uv(h + 1, m);
        HUF_repeat rp = HUF_repeat_none; uint8_t tmp[10];
        size_t z = HUF_compress4X_repeat(h + hp + 4, cap - o - hp - 4, in + i, m, HUF_SYMBOLVALUE_MAX, HUF_TABLELOG_DEFAULT, wk, sizeof wk, ct, &rp, hufl());
        if (HUF_isError(z)) return 0;
        if (z == 1) { h[0] = 1; h[hp] = in[i]; o += hp + 1; }
        else if (z == 0 || z + 4 >= m) { h[0] = 0; memcpy(h + hp, in + i, m); o += hp + m; }
        else { size_t tn = put_uv(tmp, z); memmove(h + hp + tn, h + hp + 4, z); memcpy(h + hp, tmp, tn); h[0] = 2; o += hp + tn + z; }
        i += m;
    }
    return o;
}
static int huf_d(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX); U32 wk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32]; size_t i = 0, o = 0;
    while (o < n) {
        uint64_t m, z; if (i >= cn) return -1;
        uint8_t mode = in[i++]; if (get_uv(in, cn, &i, &m) || m == 0 || m > n - o) return -1;
        if (mode == 0) { memcpy(out + o, in + i, m); i += m; }
        else if (mode == 1) { memset(out + o, in[i++], m); }
        else { if (get_uv(in, cn, &i, &z)) return -1; size_t r = HUF_decompress4X_hufOnly_wksp(dt, out + o, m, in + i, z, wk, sizeof wk, hufl()); if (HUF_isError(r) || r != m) return -1; i += z; }
        o += m;
    }
    return 0;
}

typedef struct { const char *name; int kind, level; } codec;
enum { K_LZ4, K_ZSTD, K_HUF, K_BRO, K_LZMA, K_XXH, K_KLZ, K_KLZH, K_KHUF, K_KLZMA, K_KHASH };
typedef struct { void *zc, *zd; void *ks; } wctx;
static size_t do_c(const codec *c, wctx *w, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    switch (c->kind) {
    case K_LZ4: { int r = LZ4_compress_fast((const char *)in, (char *)out, (int)n, (int)cap, 1); return r > 0 ? (size_t)r : 0; }
    case K_ZSTD: { if (!w->zc) w->zc = ZSTD_createCCtx(); size_t r = ZSTD_compressCCtx(w->zc, out, cap, in, n, c->level); return ZSTD_isError(r) ? 0 : r; }
    case K_HUF: return huf_c(in, n, out, cap);
    case K_BRO: { size_t o = cap; return BrotliEncoderCompress(c->level, 24, BROTLI_MODE_GENERIC, n, in, &o, out) ? o : 0; }
    case K_LZMA: {
        lzma_options_lzma opt; lzma_lzma_preset(&opt, 9 | LZMA_PRESET_EXTREME);
        uint32_t d = 4096; while (d < n && d < (64u << 20)) d <<= 1; opt.dict_size = d;
        lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}}; size_t pos = 0;
        return lzma_raw_buffer_encode(f, NULL, in, n, out, &pos, cap) == LZMA_OK ? pos : 0; }
    case K_KLZ: return kolmo_klz_compress(in, n, out, cap, c->level);
    case K_KLZH: return kolmo_klzh_compress(in, n, out, cap, c->level);
    case K_KHUF: return kolmo_khuf_compress_chunks(in, n, out, cap);
    case K_KLZMA: return kolmo_klzma_compress(in, n, out, cap, c->level);
    case K_XXH: { unsigned long long h = kolmo_xxh3(in, n, 0); memcpy(out, &h, 8); return 8; }
    case K_KHASH: { uint64_t h = kolmo_khash64(in, n, 0); memcpy(out, &h, 8); return 8; }
    }
    return 0;
}
static int do_d(const codec *c, wctx *w, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    switch (c->kind) {
    case K_LZ4: return LZ4_decompress_safe((const char *)in, (char *)out, (int)cn, (int)n) == (int)n ? 0 : -1;
    case K_ZSTD: { if (!w->zd) w->zd = ZSTD_createDCtx(); size_t r = ZSTD_decompressDCtx(w->zd, out, n, in, cn); return !ZSTD_isError(r) && r == n ? 0 : -1; }
    case K_HUF: return huf_d(in, cn, out, n);
    case K_BRO: { size_t o = n; return BrotliDecoderDecompress(cn, in, &o, out) == BROTLI_DECODER_RESULT_SUCCESS && o == n ? 0 : -1; }
    case K_LZMA: {
        lzma_options_lzma opt; lzma_lzma_preset(&opt, 6); uint32_t d = 4096; while (d < n && d < (64u << 20)) d <<= 1; opt.dict_size = d;
        lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}}; size_t ip = 0, op = 0;
        return lzma_raw_buffer_decode(f, NULL, in, &ip, cn, out, &op, n) == LZMA_OK && op == n ? 0 : -1; }
    case K_KLZ: return kolmo_klz_decompress(in, cn, out, n);
    case K_KLZH: return kolmo_klzh_decompress(in, cn, out, n);
    case K_KHUF: return kolmo_khuf_decompress_chunks(in, cn, out, n);
    case K_KLZMA: return kolmo_klzma_decompress(in, cn, out, n);
    case K_XXH: case K_KHASH: return 0;
    }
    return -1;
}

typedef struct { const uint8_t *in; size_t n; uint8_t *z; size_t zn, cap; uint8_t *r; } blk;
static blk *B; static size_t NB; static const codec *CUR; static int PH; static int next_blk; static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static void *worker(void *a) {
    (void)a; wctx w = {0};
    for (;;) {
        pthread_mutex_lock(&mu); int k = next_blk++; pthread_mutex_unlock(&mu);
        if ((size_t)k >= NB) break;
        blk *b = &B[k];
        if (PH == 0) { b->zn = do_c(CUR, &w, b->in, b->n, b->z, b->cap); if (!b->zn) { fprintf(stderr, "%s: compress failed on block %d\n", CUR->name, k); exit(1); } }
        else if (do_d(CUR, &w, b->z, b->zn, b->r, b->n)) { fprintf(stderr, "%s: decode failed on block %d\n", CUR->name, k); exit(1); }
    }
    if (w.zc) ZSTD_freeCCtx(w.zc);
    if (w.zd) ZSTD_freeDCtx(w.zd);
    return NULL;
}
static double run(int threads) {
    pthread_t t[64]; next_blk = 0; double t0 = now();
    for (int i = 0; i < threads; i++) pthread_create(&t[i], NULL, worker, NULL);
    for (int i = 0; i < threads; i++) pthread_join(t[i], NULL);
    return now() - t0;
}
static const codec CODECS[] = {
    {"lz4", K_LZ4, 1}, {"zstd1", K_ZSTD, 1}, {"zstd3", K_ZSTD, 3}, {"zstd9", K_ZSTD, 9}, {"zstd19", K_ZSTD, 19}, {"zstd22", K_ZSTD, 22},
    {"huf", K_HUF, 0}, {"brotli8", K_BRO, 8}, {"lzma9e", K_LZMA, 9}, {"xxh3", K_XXH, 0},
    {"klz1", K_KLZ, 1}, {"klz2", K_KLZ, 2}, {"klz3", K_KLZ, 3},
    {"klzh1", K_KLZH, 1}, {"klzh2", K_KLZH, 2}, {"klzh3", K_KLZH, 3}, {"klzh4", K_KLZH, 4}, {"klzh5", K_KLZH, 5}, {"klzh6", K_KLZH, 6}, {"klzh7", K_KLZH, 7}, {"klzh8", K_KLZH, 8}, {"klzh9", K_KLZH, 9},
    {"khuf", K_KHUF, 0},
    {"klzma1", K_KLZMA, 1}, {"klzma2", K_KLZMA, 2}, {"klzma3", K_KLZMA, 3}, {"klzma4", K_KLZMA, 4}, {"klzma5", K_KLZMA, 5}, {"klzma6", K_KLZMA, 6}, {"klzma7", K_KLZMA, 7}, {"klzma8", K_KLZMA, 8}, {"klzma9", K_KLZMA, 9},
    {"khash", K_KHASH, 0},
};
int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "usage: natbench CODEC[,CODEC...] THREADS BLOCK_KIB REPS FILE...\n"); return 2; }
    int threads = atoi(argv[2]), reps = atoi(argv[4]); size_t bs = (size_t)atoi(argv[3]) << 10;
    size_t total = 0;
    for (int a = 5; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); return 1; }
        fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); rewind(f);
        uint8_t *buf = malloc(n ? n : 1); if (fread(buf, 1, n, f) != n) return 1; fclose(f);
        for (size_t i = 0; i < n; i += bs) {
            B = realloc(B, (NB + 1) * sizeof *B); blk *b = &B[NB++];
            b->in = buf + i; b->n = n - i < bs ? n - i : bs; b->cap = b->n + b->n / 4 + 65536;
            b->z = malloc(b->cap); b->r = malloc(b->n);
        }
        total += n;
    }
    char *list = strdup(argv[1]);
    for (char *s = strtok(list, ","); s; s = strtok(NULL, ",")) {
        const codec *c = NULL; for (size_t k = 0; k < sizeof CODECS / sizeof *CODECS; k++) if (!strcmp(CODECS[k].name, s)) c = &CODECS[k];
        if (!c) { fprintf(stderr, "unknown codec %s\n", s); return 2; }
        CUR = c; double tc = 1e9, td = 1e9;
        for (int r = 0; r < (reps > 1 && c->kind != K_LZMA && c->kind != K_KLZMA && c->kind != K_BRO ? reps : 1); r++) { PH = 0; double t = run(threads); if (t < tc) tc = t; }
        size_t zt = 0; for (size_t k = 0; k < NB; k++) zt += B[k].zn;
        if (c->kind != K_XXH && c->kind != K_KHASH) {
            for (int r = 0; r < reps; r++) { PH = 1; double t = run(threads); if (t < td) td = t; }
            for (size_t k = 0; k < NB; k++) if (memcmp(B[k].r, B[k].in, B[k].n)) { fprintf(stderr, "%s: MISMATCH block %zu\n", c->name, k); return 1; }
        } else td = 0;
        printf("%-8s T%d  in %11zu  out %11zu  ratio %6.3f  c %8.1f MB/s  d %8.1f MB/s\n", c->name, threads, total, zt, (double)total / zt, total / tc / 1e6, td > 0 ? total / td / 1e6 : 0.0);
        fflush(stdout);
    }
    return 0;
}
