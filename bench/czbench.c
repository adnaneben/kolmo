/* czbench: in-memory benchmark of cz (czn) against zstd, LZ4 and brotli built from the same sources and flags.
 *   czbench [-T threads] [-x] [-o results.tsv] file...
 *   -x adds the slow settings (zstd -19 whole-file, brotli -11).
 * Timing: best of repeated runs (>= 2 runs or >= 0.5 s), every output decoded and compared byte for byte. */
#define _GNU_SOURCE
#include "czn.h"
#include "zstd.h"
#include "lz4.h"
#include "lz4hc.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#ifndef CZN_NO_LZMA
#include <lzma.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
typedef struct { const char *name; int kind, level, threads; } cfg_t;   /* kind: 0 lz4 1 lz4hc 2 zstd 3 brotli 10+ cz mode */
static ZSTD_CCtx *zc; static ZSTD_DCtx *zd;

static size_t comp(const cfg_t *c, const uint8_t *s, size_t n, uint8_t *d, size_t cap) {
    switch (c->kind) {
        case 0: return LZ4_compress_default((const char *)s, (char *)d, (int)n, (int)cap);
        case 1: return LZ4_compress_HC((const char *)s, (char *)d, (int)n, (int)cap, c->level);
        case 2: ZSTD_CCtx_reset(zc, ZSTD_reset_session_and_parameters);
                ZSTD_CCtx_setParameter(zc, ZSTD_c_compressionLevel, c->level);
                ZSTD_CCtx_setParameter(zc, ZSTD_c_nbWorkers, c->threads > 1 ? c->threads : 0);
                { size_t r = ZSTD_compress2(zc, d, cap, s, n); return ZSTD_isError(r) ? 0 : r; }
        case 3: { size_t o = cap; return BrotliEncoderCompress(c->level, 24, BROTLI_MODE_GENERIC, n, s, &o, d) ? o : 0; }
#ifndef CZN_NO_LZMA
        case 4: { size_t o = 0; uint32_t pr = (uint32_t)c->level | (c->threads < 0 ? LZMA_PRESET_EXTREME : 0);
                  return lzma_easy_buffer_encode(pr, LZMA_CHECK_CRC64, NULL, s, n, d, &o, cap) == LZMA_OK ? o : 0; }
#endif
        default: { czn_params p = {(czn_mode)(c->kind - 10), c->level, c->threads, 0, 1}; int64_t r = czn_compress(s, n, d, cap, &p); return r > 0 ? (size_t)r : 0; }
    }
}
static int decomp(const cfg_t *c, const uint8_t *s, size_t cn, uint8_t *d, size_t n) {
    switch (c->kind) {
        case 0: case 1: return LZ4_decompress_safe((const char *)s, (char *)d, (int)cn, (int)n) == (int)n;
        case 2: { size_t r = ZSTD_decompressDCtx(zd, d, n, s, cn); return !ZSTD_isError(r) && r == n; }
        case 3: { size_t o = n; return BrotliDecoderDecompress(cn, s, &o, d) == BROTLI_DECODER_RESULT_SUCCESS && o == n; }
#ifndef CZN_NO_LZMA
        case 4: { uint64_t lim = UINT64_MAX; size_t ip = 0, op = 0;
                  return lzma_stream_buffer_decode(&lim, 0, NULL, s, &ip, cn, d, &op, n) == LZMA_OK && op == n; }
#endif
        default: return czn_decompress(s, cn, d, n, c->threads) == (int64_t)n;
    }
}
int main(int argc, char **argv) {
    int T = (int)sysconf(_SC_NPROCESSORS_ONLN), slow = 0, vslow = 0, opt, status = 0, measured = 0; const char *outp = NULL, *only = NULL;
    while ((opt = getopt(argc, argv, "T:xXo:c:")) != -1) { if (opt == 'c') only = optarg; if (opt == 'T') T = atoi(optarg); else if (opt == 'x') slow = 1; else if (opt == 'X') slow = vslow = 1; else if (opt == 'o') outp = optarg; }
    if (optind == argc) { fprintf(stderr, "czbench: supply at least one input file\n"); return 1; }
    zc = ZSTD_createCCtx(); zd = ZSTD_createDCtx();
    if (!zc || !zd) { ZSTD_freeCCtx(zc); ZSTD_freeDCtx(zd); return 1; }
    cfg_t C[] = {
        {"lz4", 0, 0, 1}, {"lz4hc -9", 1, 9, 1},
        {"zstd -1", 2, 1, 1}, {"zstd -3", 2, 3, 1}, {"zstd -3 -T", 2, 3, T}, {"zstd -9", 2, 9, 1}, {"zstd -19", 2, 19, 1}, {"zstd -19 -T", 2, 19, T},
        {"brotli -8", 3, 8, 1}, {"brotli -11", 3, 11, 1}, {"xz -6", 4, 6, 1}, {"xz -9e", 4, 9, -1},
        {"cz rt", 10, 0, 1}, {"cz rt -T", 10, 0, T}, {"cz fast", 11, 0, 1}, {"cz fast -T", 11, 0, T},
        {"cz eco", 12, 0, 1}, {"cz eco -T", 12, 0, T}, {"cz hc", 13, 0, 1}, {"cz hc -T", 13, 0, T},
        {"cz max", 14, 0, 1}, {"cz max -T", 14, 0, T}, {"cz ultra", 15, 0, 1}, {"cz ultra -T", 15, 0, T},
    };
    FILE *o = outp ? fopen(outp, "a") : NULL;
    if (outp && !o) { perror(outp); ZSTD_freeCCtx(zc); ZSTD_freeDCtx(zd); return 1; }
    for (int a = optind; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); status = 1; continue; }
        long length;
        if (fseek(f, 0, SEEK_END) || (length = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
            perror(argv[a]); fclose(f); status = 1; continue;
        }
        size_t n = (size_t)length;
        uint8_t *s = malloc(n ? n : 1), *back = malloc(n + 1);
        if (!s || !back || fread(s, 1, n, f) != n) {
            fprintf(stderr, "czbench: cannot allocate/read %s\n", argv[a]);
            free(s); free(back); fclose(f); status = 1; continue;
        }
        fclose(f);
        czn_params bp = {CZN_RT, 0, 0, 0, 1}; size_t cap = czn_bound(n, &bp);
        if (cap < ZSTD_compressBound(n) + 4096) cap = ZSTD_compressBound(n) + 4096;
        if (cap < BrotliEncoderMaxCompressedSize(n)) cap = BrotliEncoderMaxCompressedSize(n) + 4096;
        uint8_t *d = malloc(cap);
        if (!d) { free(s); free(back); status = 1; continue; }
        const char *base = strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a];
        for (size_t k = 0; k < sizeof C / sizeof *C; k++) {
            const cfg_t *c = &C[k];
            if (!vslow && !strcmp(c->name, "brotli -11")) continue;
            if (!slow && !strcmp(c->name, "zstd -19")) continue;
            if (c->threads > 1 && T <= 1) continue;
            if (!vslow && !strcmp(c->name, "xz -9e")) continue;
#ifdef CZN_NO_LZMA
            if (c->kind == 4) continue;                       /* built without liblzma: no xz baseline */
#endif
            if (only) {                                       /* -c "name1;name2": run only these settings */
                char buf[512]; snprintf(buf, sizeof buf, ";%s;", only); char key[64]; snprintf(key, sizeof key, ";%s;", c->name);
                if (!strstr(buf, key)) continue;
            }
            double bc = 1e9, bd = 1e9, tot = 0; size_t cl = 0; int ok = 1, runs = 0;
            while (runs < 10 && (runs < 3 || tot < 1.0)) {      /* compression: best of up to 10 runs or about 1 s */
                double t0 = now(); cl = comp(c, s, n, d, cap); double t1 = now();
                if (!cl) { ok = 0; break; }
                if (t1 - t0 < bc) bc = t1 - t0; tot += t1 - t0; runs++;
                if (t1 - t0 > 3.0) break;                      /* slow settings: one run */
            }
            for (int r = 0; ok && r < 30; r++) {               /* decompression: best of up to 30 runs or about 1 s */
                memset(back, 0, 64);
                double t1 = now(); int good = decomp(c, d, cl, back, n); double t2 = now();
                ok &= good && !memcmp(back, s, n);
                if (t2 - t1 < bd) bd = t2 - t1;
                tot += t2 - t1; if (r >= 4 && tot > 2.0) break;
            }
            double cm = n / bc / 1e6, dm = n / bd / 1e6;
            measured++; if (!ok) status = 1;
            printf("%-22s %-14s T=%-2d ratio %7.3f  c %8.1f MB/s  d %8.1f MB/s  %s\n", base, c->name, c->threads, cl ? (double)n / cl : 0, cm, dm, ok ? "ok" : "FAIL");
            if (fflush(stdout)) status = 1;
            if (o) {
                if (fprintf(o, "%s\t%s\t%d\t%zu\t%zu\t%.2f\t%.2f\t%d\n", base, c->name, c->threads, n, cl, cm, dm, ok) < 0 || fflush(o)) status = 1;
            }
        }
        free(s); free(back); free(d);
    }
    if (o && fclose(o)) status = 1;
    if (!measured) { fprintf(stderr, "czbench: no settings measured\n"); status = 1; }
    ZSTD_freeCCtx(zc); ZSTD_freeDCtx(zd);
    return status;
}
