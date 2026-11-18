/* ebench: order-0 entropy coders on the same 128 KiB chunks (w4 entropy lane): khuf (codec 27), zstd's HUF (codec 13's
 * framing), zstd's FSE (tANS: FSE_normalizeCount / writeNCount / buildCTable / compress_usingCTable, decoded by
 * FSE_decompress_wksp_bmi2; table log FSE_optimalTableLog up to 12) and Kolmo's rANS leaf (codec 14: 4 x 64-bit
 * states, 16-bit frequencies). Sizes include each coder's chunk headers; decode MB/s best of REPS, every output
 * checked. usage: ebench REPS FILE...  (.chunks files: records u32 length + bytes, as hbench; other files: 128 KiB records) */
#define FSE_STATIC_LINKING_ONLY
#define HUF_STATIC_LINKING_ONLY
#include "../../../src/kolmo.c"
#include "common/fse.h"
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#define CH (128u << 10)
static size_t kh_uv(uint8_t *p, uint64_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
/* FSE in chunks: mode u8 (0 stored, 1 rle, 2 FSE) | raw length uv | coded length uv | data */
static size_t fse_c(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    static FSE_CTable ct[FSE_CTABLE_SIZE_U32(12, 255)]; static U32 wk[FSE_BUILD_CTABLE_WORKSPACE_SIZE_U32(255, 12)];
    size_t o = 0;
    for (size_t i = 0; i < n;) {
        size_t m = n - i < CH ? n - i : CH; unsigned cnt[256] = {0}, mx = 0; short norm[256];
        for (size_t k = 0; k < m; k++) cnt[in[i + k]]++;
        for (unsigned s = 0; s < 256; s++) if (cnt[s]) mx = s;
        uint8_t *h = out + o; size_t hp = 1 + kh_uv(h + 1, m); int rle = 0;
        for (unsigned s = 0; s < 256; s++) if (cnt[s] == m) rle = 1;
        size_t z = 0; uint8_t tmp[CH + 512];
        if (!rle) {
            unsigned tl = FSE_optimalTableLog(12, m, mx);
            size_t e = FSE_normalizeCount(norm, tl, cnt, m, mx, 1);
            if (!FSE_isError(e)) {
                size_t hn = FSE_writeNCount(tmp, sizeof tmp, norm, mx, tl);
                if (!FSE_isError(hn) && !FSE_isError(FSE_buildCTable_wksp(ct, norm, mx, tl, wk, sizeof wk))) {
                    size_t r = FSE_compress_usingCTable(tmp + hn, sizeof tmp - hn, in + i, m, ct);
                    if (!FSE_isError(r) && r) z = hn + r;
                }
            }
        }
        if (cap - o < m + 32) return 0;
        if (rle) { h[0] = 1; h[hp] = in[i]; o += hp + 1; }
        else if (!z || z >= m) { h[0] = 0; memcpy(h + hp, in + i, m); o += hp + m; }
        else { h[0] = 2; hp += kh_uv(h + hp, z); memcpy(h + hp, tmp, z); o += hp + z; }
        i += m;
    }
    return o;
}
static int fse_d(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    static U32 wk[FSE_DECOMPRESS_WKSP_SIZE_U32(12, 255)]; size_t i = 0, o = 0;
    while (o < n) {
        uint64_t m, z; uint8_t mode = in[i++]; if (kgu_get(in, cn, &i, &m)) return -1;
        if (mode == 0) { memcpy(out + o, in + i, m); i += m; }
        else if (mode == 1) memset(out + o, in[i++], m);
        else { if (kgu_get(in, cn, &i, &z)) return -1; size_t r = FSE_decompress_wksp_bmi2(out + o, m, in + i, z, 12, wk, sizeof wk, 1); if (FSE_isError(r) || r != m) return -1; i += z; }
        o += m;
    }
    return 0;
}
typedef size_t (*cfn)(const uint8_t *, size_t, uint8_t *, size_t);
typedef int (*dfn)(const uint8_t *, size_t, uint8_t *, size_t);
static size_t rans_c(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { size_t r = rans_compress_all_x(in, n, out, cap, 1); return r; }
static const struct { const char *name; cfn c; dfn d; } C[] = {
    {"khuf", kolmo_khuf_compress_chunks, kolmo_khuf_decompress_chunks}, {"HUF", huf_compress_all, huf_decompress_all},
    {"FSE", fse_c, fse_d}, {"rANS14", rans_c, rans_decompress_all}};
int main(int argc, char **argv) {
    int reps = atoi(argv[1]);
    printf("%-22s %10s | %-28s | %-28s | %-28s | %-28s\n", "file", "bytes", "khuf size, d MB/s", "HUF", "FSE", "rANS14");
    for (int a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *buf = malloc(n + 8); if (fread(buf, 1, n, f) != (size_t)n) return 1; fclose(f);
        const uint8_t *rp[100000]; size_t rn[100000]; int nr = 0; size_t tot = 0, L = strlen(argv[a]);
        if (L > 7 && !strcmp(argv[a] + L - 7, ".chunks")) {
            for (long i = 0; i + 4 <= n && nr < 100000;) { uint32_t m; memcpy(&m, buf + i, 4); i += 4; rp[nr] = buf + i; rn[nr++] = m; tot += m; i += m; }
        } else { for (long i = 0; i < n && nr < 100000; i += CH) { rp[nr] = buf + i; rn[nr++] = n - i < (long)CH ? (size_t)(n - i) : CH; } tot = n; }   /* 128 KiB records */
        printf("%-22s %10zu", strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a], tot);
        for (int k = 0; k < 4; k++) {
            uint8_t **z = malloc(nr * sizeof *z); size_t *zn = malloc(nr * 8), S = 0; uint8_t *o = malloc(CH * 8 + 64); int ok = 1;
            for (int r = 0; r < nr; r++) { z[r] = malloc(rn[r] + rn[r] / 16 + 4096); zn[r] = C[k].c(rp[r], rn[r], z[r], rn[r] + rn[r] / 16 + 4096); S += zn[r] ? zn[r] : rn[r]; }
            for (int r = 0; r < nr; r++) if (zn[r] && (rn[r] > CH * 8 || C[k].d(z[r], zn[r], o, rn[r]) || memcmp(o, rp[r], rn[r]))) ok = 0;
            double best = 1e9;
            for (int q = 0; q < reps; q++) { double t = now(); for (int r = 0; r < nr; r++) if (zn[r] && rn[r] <= CH * 8) C[k].d(z[r], zn[r], o, rn[r]); t = now() - t; if (t < best) best = t; }
            printf(" | %10zu %+6.2f%% %7.1f%s", S, 100.0 * ((double)S / tot - 1), tot / best / 1e6, ok ? "" : " BAD");
            for (int r = 0; r < nr; r++) free(z[r]);
            free(z); free(zn); free(o);
        }
        printf("\n"); fflush(stdout); free(buf);
    }
    return 0;
}
