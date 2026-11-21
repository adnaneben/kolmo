/* Link-time profiler for Kolmo's calls into its dependencies (deps lane, rc3).
 * Built into build/kolmo-libprof with -Wl,--wrap=<symbol> for every library entry point Kolmo calls; each wrapper
 * accumulates calls, bytes in and wall time (CLOCK_MONOTONIC; run with -T 1 so the sum is not inflated by threads)
 * and prints a table to stderr at exit. Calls inside a library are not wrapped, so nothing is counted twice. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include "kolmo_lzma.h"
#include "zstd.h"
#include "lz4.h"
#include "lz4hc.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#define HUF_STATIC_LINKING_ONLY
#include "common/huf.h"

enum { P_ZC, P_ZC1, P_ZC2, P_ZDICT, P_ZD, P_ZDD, P_HUFC, P_HUFD, P_L4C, P_L4D, P_L4HC, P_L4DEF, P_BRC, P_BRD, P_LZC, P_LZD, P_X3, P_BSC, P_BSD, P_N };
static const char *pn[P_N] = {"ZSTD_compressCCtx", "ZSTD_compress", "ZSTD_compress2", "ZSTD_compress_usingDict",
    "ZSTD_decompressDCtx", "ZSTD_decompress_usingDict", "HUF_compress4X_repeat", "HUF_decompress4X_hufOnly_wksp",
    "LZ4_compress_fast", "LZ4_decompress_safe", "LZ4_compress_HC_extStateHC", "LZ4_compress_default",
    "BrotliEncoderCompress", "BrotliDecoderDecompress", "kolmo_lzma_raw_buffer_encode", "kolmo_lzma_raw_buffer_decode", "kolmo_xxh3", "bsc_compress", "bsc_decompress"};
static _Atomic unsigned long long ncall[P_N], nbytes[P_N], nns[P_N];
static _Atomic unsigned long long zlev[23];
static unsigned long long now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000000000ull + t.tv_nsec; }
#define ACC(k, b, t0) do { ncall[k]++; nbytes[k] += (b); nns[k] += now() - (t0); } while (0)
static void report(void) {
    const char *f = getenv("LIBPROF_OUT"); FILE *o = f ? fopen(f, "a") : stderr; if (!o) o = stderr;
    for (int k = 0; k < P_N; k++) if (ncall[k])
        fprintf(o, "%s\t%llu\t%llu\t%.3f\n", pn[k], (unsigned long long)ncall[k], (unsigned long long)nbytes[k], nns[k] / 1e6);
    for (int l = 0; l < 23; l++) if (zlev[l]) fprintf(o, "zstd_level_%d\t%llu\t0\t0\n", l, (unsigned long long)zlev[l]);
    if (o != stderr) fclose(o);
}
__attribute__((constructor)) static void init(void) { atexit(report); }

size_t __real_ZSTD_compressCCtx(ZSTD_CCtx *, void *, size_t, const void *, size_t, int);
size_t __wrap_ZSTD_compressCCtx(ZSTD_CCtx *c, void *d, size_t dc, const void *s, size_t n, int l) {
    unsigned long long t = now(); size_t r = __real_ZSTD_compressCCtx(c, d, dc, s, n, l); ACC(P_ZC, n, t); if (l >= 0 && l < 23) zlev[l]++; return r; }
size_t __real_ZSTD_compress(void *, size_t, const void *, size_t, int);
size_t __wrap_ZSTD_compress(void *d, size_t dc, const void *s, size_t n, int l) {
    unsigned long long t = now(); size_t r = __real_ZSTD_compress(d, dc, s, n, l); ACC(P_ZC1, n, t); return r; }
size_t __real_ZSTD_compress2(ZSTD_CCtx *, void *, size_t, const void *, size_t);
size_t __wrap_ZSTD_compress2(ZSTD_CCtx *c, void *d, size_t dc, const void *s, size_t n) {
    unsigned long long t = now(); size_t r = __real_ZSTD_compress2(c, d, dc, s, n); ACC(P_ZC2, n, t); return r; }
size_t __real_ZSTD_compress_usingDict(ZSTD_CCtx *, void *, size_t, const void *, size_t, const void *, size_t, int);
size_t __wrap_ZSTD_compress_usingDict(ZSTD_CCtx *c, void *d, size_t dc, const void *s, size_t n, const void *di, size_t dl, int l) {
    unsigned long long t = now(); size_t r = __real_ZSTD_compress_usingDict(c, d, dc, s, n, di, dl, l); ACC(P_ZDICT, n, t); return r; }
size_t __real_ZSTD_decompressDCtx(ZSTD_DCtx *, void *, size_t, const void *, size_t);
size_t __wrap_ZSTD_decompressDCtx(ZSTD_DCtx *c, void *d, size_t dc, const void *s, size_t n) {
    unsigned long long t = now(); size_t r = __real_ZSTD_decompressDCtx(c, d, dc, s, n); ACC(P_ZD, dc, t); return r; }
size_t __real_ZSTD_decompress_usingDict(ZSTD_DCtx *, void *, size_t, const void *, size_t, const void *, size_t);
size_t __wrap_ZSTD_decompress_usingDict(ZSTD_DCtx *c, void *d, size_t dc, const void *s, size_t n, const void *di, size_t dl) {
    unsigned long long t = now(); size_t r = __real_ZSTD_decompress_usingDict(c, d, dc, s, n, di, dl); ACC(P_ZDD, dc, t); return r; }
size_t __real_HUF_compress4X_repeat(void *, size_t, const void *, size_t, unsigned, unsigned, void *, size_t, HUF_CElt *, HUF_repeat *, int);
size_t __wrap_HUF_compress4X_repeat(void *d, size_t dc, const void *s, size_t n, unsigned m, unsigned tl, void *w, size_t ws, HUF_CElt *ct, HUF_repeat *rp, int fl) {
    unsigned long long t = now(); size_t r = __real_HUF_compress4X_repeat(d, dc, s, n, m, tl, w, ws, ct, rp, fl); ACC(P_HUFC, n, t); return r; }
size_t __real_HUF_decompress4X_hufOnly_wksp(HUF_DTable *, void *, size_t, const void *, size_t, void *, size_t, int);
size_t __wrap_HUF_decompress4X_hufOnly_wksp(HUF_DTable *dt, void *d, size_t dc, const void *s, size_t n, void *w, size_t ws, int fl) {
    unsigned long long t = now(); size_t r = __real_HUF_decompress4X_hufOnly_wksp(dt, d, dc, s, n, w, ws, fl); ACC(P_HUFD, dc, t);
    const char *dump = getenv("LIBPROF_DUMP_HUF");   /* experiment: append each decoded Huffman leaf chunk (u32 length, bytes) */
    if (dump && r == dc) { FILE *o = fopen(dump, "ab"); if (o) { unsigned len = (unsigned)dc; fwrite(&len, 4, 1, o); fwrite(d, 1, dc, o); fclose(o); } }
    return r; }
int __real_LZ4_compress_fast(const char *, char *, int, int, int);
int __wrap_LZ4_compress_fast(const char *s, char *d, int n, int c, int a) {
    unsigned long long t = now(); int r = __real_LZ4_compress_fast(s, d, n, c, a); ACC(P_L4C, n, t); return r; }
int __real_LZ4_compress_default(const char *, char *, int, int);
int __wrap_LZ4_compress_default(const char *s, char *d, int n, int c) {
    unsigned long long t = now(); int r = __real_LZ4_compress_default(s, d, n, c); ACC(P_L4DEF, n, t); return r; }
int __real_LZ4_decompress_safe(const char *, char *, int, int);
int __wrap_LZ4_decompress_safe(const char *s, char *d, int n, int c) {
    unsigned long long t = now(); int r = __real_LZ4_decompress_safe(s, d, n, c); ACC(P_L4D, c, t); return r; }
int __real_LZ4_compress_HC_extStateHC(void *, const char *, char *, int, int, int);
int __wrap_LZ4_compress_HC_extStateHC(void *st, const char *s, char *d, int n, int c, int l) {
    unsigned long long t = now(); int r = __real_LZ4_compress_HC_extStateHC(st, s, d, n, c, l); ACC(P_L4HC, n, t); return r; }
BROTLI_BOOL __real_BrotliEncoderCompress(int, int, BrotliEncoderMode, size_t, const uint8_t *, size_t *, uint8_t *);
BROTLI_BOOL __wrap_BrotliEncoderCompress(int q, int w, BrotliEncoderMode m, size_t n, const uint8_t *s, size_t *on, uint8_t *o) {
    unsigned long long t = now(); BROTLI_BOOL r = __real_BrotliEncoderCompress(q, w, m, n, s, on, o); ACC(P_BRC, n, t); return r; }
BrotliDecoderResult __real_BrotliDecoderDecompress(size_t, const uint8_t *, size_t *, uint8_t *);
BrotliDecoderResult __wrap_BrotliDecoderDecompress(size_t n, const uint8_t *s, size_t *on, uint8_t *o) {
    unsigned long long t = now(); BrotliDecoderResult r = __real_BrotliDecoderDecompress(n, s, on, o); ACC(P_BRD, *on, t); return r; }
lzma_ret __real_kolmo_lzma_raw_buffer_encode(const lzma_filter *, const lzma_allocator *, const uint8_t *, size_t, uint8_t *, size_t *, size_t);
lzma_ret __wrap_kolmo_lzma_raw_buffer_encode(const lzma_filter *f, const lzma_allocator *a, const uint8_t *s, size_t n, uint8_t *o, size_t *op, size_t oc) {
    /* LIBPROF_LZMA_PRESET=N[e]: experiment, code with the match finder and parser settings of preset N (e: extreme)
     * instead of 9e, keeping Kolmo's dictionary size and lc/lp/pb. LIBPROF_LZMA_NICE / _DEPTH / _MF override one field. */
    const char *e = getenv("LIBPROF_LZMA_PRESET"), *ni = getenv("LIBPROF_LZMA_NICE"), *de = getenv("LIBPROF_LZMA_DEPTH"), *mf = getenv("LIBPROF_LZMA_MF");
    lzma_options_lzma x; lzma_filter g[2];
    if ((e || ni || de || mf) && f[0].id == LZMA_FILTER_LZMA2) {
        const lzma_options_lzma *k = f[0].options; x = *k;
        if (e) { lzma_options_lzma p; lzma_lzma_preset(&p, (uint32_t)atoi(e) | (strchr(e, 'e') ? LZMA_PRESET_EXTREME : 0));
                 x.mode = p.mode; x.nice_len = p.nice_len; x.mf = p.mf; x.depth = p.depth; }
        if (ni) x.nice_len = (uint32_t)atoi(ni);
        if (de) x.depth = (uint32_t)atoi(de);
        if (mf) x.mf = (lzma_match_finder)strtol(mf, NULL, 0);
        g[0].id = LZMA_FILTER_LZMA2; g[0].options = &x; g[1] = f[1]; f = g;
    }
    unsigned long long t = now(); lzma_ret r = __real_kolmo_lzma_raw_buffer_encode(f, a, s, n, o, op, oc); ACC(P_LZC, n, t); return r; }
lzma_ret __real_kolmo_lzma_raw_buffer_decode(const lzma_filter *, const lzma_allocator *, const uint8_t *, size_t *, size_t, uint8_t *, size_t *, size_t);
lzma_ret __wrap_kolmo_lzma_raw_buffer_decode(const lzma_filter *f, const lzma_allocator *a, const uint8_t *s, size_t *ip, size_t n, uint8_t *o, size_t *op, size_t oc) {
    unsigned long long t = now(); lzma_ret r = __real_kolmo_lzma_raw_buffer_decode(f, a, s, ip, n, o, op, oc); ACC(P_LZD, oc, t); return r; }
unsigned long long __real_kolmo_xxh3(const void *, size_t, unsigned long long);
unsigned long long __wrap_kolmo_xxh3(const void *p, size_t n, unsigned long long s) {
    unsigned long long t = now(); unsigned long long r = __real_kolmo_xxh3(p, n, s); ACC(P_X3, n, t); return r; }
int __real_bsc_compress(const unsigned char *, unsigned char *, int, int, int, int, int, int);
int __wrap_bsc_compress(const unsigned char *s, unsigned char *d, int n, int a, int b, int c, int e, int f) {
    unsigned long long t = now(); int r = __real_bsc_compress(s, d, n, a, b, c, e, f); ACC(P_BSC, n, t); return r; }
int __real_bsc_decompress(const unsigned char *, int, unsigned char *, int, int);
int __wrap_bsc_decompress(const unsigned char *s, int n, unsigned char *d, int dn, int f) {
    unsigned long long t = now(); int r = __real_bsc_decompress(s, n, d, dn, f); ACC(P_BSD, dn, t); return r; }
