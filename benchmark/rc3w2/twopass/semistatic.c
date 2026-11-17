/* Research harness of the twopass lane, question 4: semi-static context models. Counts over the whole input give one
 * frequency table per context (12-bit, every seen symbol at least 1); the tables are stored (here: zstd -19 of the
 * 16-bit frequency arrays, counted in the size) and the bytes are coded with static rANS (32-bit state, byte output).
 *   semistatic FILE   prints, for order 0, order 1 and a hashed order 2 (H contexts), the coded size with tables, encode
 *                     and decode speed (round trip checked), against zstd -3 and zstd -19 of the same bytes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <zstd.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#define PB 12
#define PT (1u << PB)
#define RL (1u << 23)
static uint32_t ctx_of(const uint8_t *s, size_t i, int order, int hb) {
    if (order == 0) return 0;
    uint32_t a = i ? s[i - 1] : 0; if (order == 1) return a;
    uint32_t b = i > 1 ? s[i - 2] : 0; return ((a << 8 | b) * 0x9E3779B1u) >> (32 - hb);
}
static void normalize(const uint32_t *cnt, uint16_t *f) {
    uint64_t tot = 0; int nz = 0; for (int k = 0; k < 256; k++) { tot += cnt[k]; nz += cnt[k] > 0; }
    if (!tot) { memset(f, 0, 512); return; }
    int sum = 0, big = 0;
    for (int k = 0; k < 256; k++) { if (!cnt[k]) { f[k] = 0; continue; } uint32_t v = (uint32_t)((uint64_t)cnt[k] * PT / tot); if (!v) v = 1; f[k] = (uint16_t)v; sum += v; if (f[k] > f[big]) big = k; }
    while (sum > (int)PT) { int b = 0; for (int k = 0; k < 256; k++) if (f[k] > f[b]) b = k; int d = sum - PT; int take = f[b] - 1 < d ? f[b] - 1 : d; f[b] -= take; sum -= take; }
    f[big] += PT - sum;
}
int main(int argc, char **argv) {
    FILE *fp = fopen(argv[1], "rb"); fseek(fp, 0, SEEK_END); size_t n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *s = malloc(n); if (fread(s, 1, n, fp) != n) return 1; fclose(fp);
    size_t zc = ZSTD_compressBound(n); uint8_t *z = malloc(zc), *d = malloc(n);
    double t0 = now(); size_t z3 = ZSTD_compress(z, zc, s, n, 3); double e3 = now() - t0;
    t0 = now(); ZSTD_decompress(d, n, z, z3); double d3 = now() - t0;
    t0 = now(); size_t z19 = ZSTD_compress(z, zc, s, n, 19); double e19 = now() - t0;
    t0 = now(); ZSTD_decompress(d, n, z, z19); double d19 = now() - t0;
    printf("%s n=%zu zstd3=%zu (%.0f/%.0f MB/s) zstd19=%zu (%.1f/%.0f MB/s)", argv[1], n, z3, n / e3 / 1e6, n / d3 / 1e6, z19, n / e19 / 1e6, n / d19 / 1e6);
    const int orders[3] = {0, 1, 2}; const int hbs[3] = {0, 8, 12};
    for (int oi = 0; oi < 3; oi++) {
        int order = orders[oi], hb = hbs[oi]; size_t C = order == 0 ? 1 : (size_t)1 << hb;
        t0 = now();
        uint32_t *cnt = calloc(C * 256, 4); for (size_t i = 0; i < n; i++) cnt[ctx_of(s, i, order, hb) * 256 + s[i]]++;
        uint16_t *f = malloc(C * 256 * 2); uint32_t *cum = malloc(C * 257 * 4);
        for (size_t c = 0; c < C; c++) { normalize(cnt + c * 256, f + c * 256); uint32_t a = 0; for (int k = 0; k < 256; k++) { cum[c * 257 + k] = a; a += f[c * 256 + k]; } cum[c * 257 + 256] = a; }
        size_t tz = ZSTD_compress(z, zc, f, C * 512, 19);              /* the stored tables */
        uint8_t *out = malloc(n + 16), *o = out + n + 16; uint32_t x = RL;
        for (size_t i = n; i-- > 0;) {
            uint32_t c = ctx_of(s, i, order, hb); uint32_t fr = f[c * 256 + s[i]], st = cum[c * 257 + s[i]];
            uint32_t xm = ((RL >> PB) << 8) * fr; while (x >= xm) { *--o = (uint8_t)x; x >>= 8; }
            x = ((x / fr) << PB) + (x % fr) + st;
        }
        o -= 4; memcpy(o, &x, 4); size_t cz = (size_t)(out + n + 16 - o); double te = now() - t0;
        /* decode: slot -> symbol table per context */
        uint8_t *sym = malloc(C * PT); for (size_t c = 0; c < C; c++) for (int k = 0; k < 256; k++) memset(sym + c * PT + cum[c * 257 + k], k, f[c * 256 + k]);
        t0 = now(); const uint8_t *p = o; memcpy(&x, p, 4); p += 4;
        for (size_t i = 0; i < n; i++) {
            uint32_t c = ctx_of(d, i, order, hb), slot = x & (PT - 1); uint8_t k = sym[c * PT + slot];
            d[i] = k; x = f[c * 256 + k] * (x >> PB) + slot - cum[c * 257 + k];
            while (x < RL) x = x << 8 | *p++;
        }
        double td = now() - t0; if (memcmp(d, s, n)) { printf(" ROUNDTRIP FAIL"); return 2; }
        printf(" | o%d%s: %zu+%zu=%zu (%.0f/%.0f MB/s)", order, order == 2 ? "h12" : "", cz, tz, cz + tz, n / te / 1e6, n / td / 1e6);
        free(cnt); free(f); free(cum); free(out); free(sym);
    }
    printf("\n"); return 0;
}
