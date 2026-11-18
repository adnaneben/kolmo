/* hbench: Kolmo's khuf (codec 27 chunks) against zstd's HUF in codec 13's framing (src/codecs/huf.c), same input,
 * decoders alternating in one process (best of REPS rounds each), every output checked.
 * usage: hbench REPS FILE...   (a FILE ending in .chunks holds records u32 length + bytes, each one leaf)
 * Prints per file: input bytes, HUF and khuf sizes, size difference, compress and decompress MB/s, speed ratios. */
#define HUF_STATIC_LINKING_ONLY
#include "common/huf.h"
#include "common/cpu.h"
#include "algo/khuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static size_t put_uv(uint8_t *p, uint64_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
static int get_uv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int sh = 0;
    while (*i < n && sh < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << sh; if (!(c & 128)) { *v = r; return 0; } sh += 7; }
    return -1;
}
static int hufl(void) { static int f = -1; if (f < 0) f = ZSTD_cpuid_bmi2(ZSTD_cpuid()) ? HUF_flags_bmi2 : 0; return f; }
static size_t huf_c(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {   /* src/codecs/huf.c */
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
typedef struct { uint8_t *p; size_t n; } rec;
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: hbench REPS FILE...\n"); return 2; }
    int reps = atoi(argv[1]);
    printf("%-28s %11s %11s %11s %8s | %8s %8s %6s | %8s %8s %6s\n", "file", "bytes", "HUF", "khuf", "size", "HUF c", "khuf c", "c x", "HUF d", "khuf d", "d x");
    double gl[3] = {0}; int nf = 0;
    for (int a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); return 1; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *buf = malloc(n + 8); if (fread(buf, 1, n, f) != (size_t)n) return 1; fclose(f);
        rec *rs = malloc(sizeof(rec) * (n / 4 + 2)); int nr = 0; size_t tot = 0;
        size_t L = strlen(argv[a]);
        if (L > 7 && !strcmp(argv[a] + L - 7, ".chunks")) {
            for (long i = 0; i + 4 <= n;) { uint32_t m; memcpy(&m, buf + i, 4); i += 4; if (i + m > (size_t)n) break; rs[nr].p = buf + i; rs[nr++].n = m; tot += m; i += m; }
        } else { rs[0].p = buf; rs[0].n = n; nr = 1; tot = n; }
        size_t cap = 0; for (int k = 0; k < nr; k++) cap = rs[k].n > cap ? rs[k].n : cap;
        cap = cap + cap / 64 + 4096;
        uint8_t **hc = malloc(nr * sizeof *hc), **kc = malloc(nr * sizeof *kc); size_t *hz = malloc(nr * 8), *kz = malloc(nr * 8); uint8_t *o = malloc(cap);
        size_t HS = 0, KS = 0; double tc[2] = {1e9, 1e9}, td[2] = {1e9, 1e9};
        for (int k = 0; k < nr; k++) { hc[k] = malloc(cap + kolmo_khuf_bound(rs[k].n)); kc[k] = malloc(kolmo_khuf_bound(rs[k].n) + 64); }
        for (int r = 0; r < reps; r++) {
            for (int v = 0; v < 2; v++) {
                int w = (r & 1) ? 1 - v : v; double t = now(); size_t S = 0;
                for (int k = 0; k < nr; k++) {
                    if (w == 0) { hz[k] = huf_c(rs[k].p, rs[k].n, hc[k], cap); S += hz[k]; }
                    else { kz[k] = kolmo_khuf_compress_chunks(rs[k].p, rs[k].n, kc[k], kolmo_khuf_bound(rs[k].n) + 64); S += kz[k]; }
                }
                t = now() - t; if (t < tc[w]) tc[w] = t; if (w == 0) HS = S; else KS = S;
            }
        }
        for (int k = 0; k < nr; k++) {
            if (huf_d(hc[k], hz[k], o, rs[k].n) || memcmp(o, rs[k].p, rs[k].n)) { printf("HUF MISMATCH\n"); return 1; }
            if (kolmo_khuf_decompress_chunks(kc[k], kz[k], o, rs[k].n) || memcmp(o, rs[k].p, rs[k].n)) { printf("KHUF MISMATCH %s rec %d\n", argv[a], k); return 1; }
        }
        for (int r = 0; r < reps * 2; r++) {
            for (int v = 0; v < 2; v++) {
                int w = (r & 1) ? 1 - v : v; double t = now();
                for (int k = 0; k < nr; k++) { if (w == 0) huf_d(hc[k], hz[k], o, rs[k].n); else kolmo_khuf_decompress_chunks(kc[k], kz[k], o, rs[k].n); }
                t = now() - t; if (t < td[w]) td[w] = t;
            }
        }
        const char *nm = strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a];
        printf("%-28s %11zu %11zu %11zu %+7.3f%% | %8.1f %8.1f %6.2f | %8.1f %8.1f %6.2f\n", nm, tot, HS, KS, 100.0 * ((double)KS / HS - 1),
               tot / tc[0] / 1e6, tot / tc[1] / 1e6, tc[0] / tc[1], tot / td[0] / 1e6, tot / td[1] / 1e6, td[0] / td[1]);
        gl[0] += (double)KS / HS; gl[1] += tc[0] / tc[1]; gl[2] += td[0] / td[1]; nf++;
        fflush(stdout);
    }
    printf("mean of %d files: size %+.3f%%, compress x%.3f, decompress x%.3f\n", nf, 100 * (gl[0] / nf - 1), gl[1] / nf, gl[2] / nf);
    return 0;
}
