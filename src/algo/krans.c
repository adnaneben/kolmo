/* KRANS: static order-0 rANS, four interleaved states (format: krans.h). Copyright 2026 Adnane Aqartit and
   contributors, Apache-2.0. Compiled on its own (no Kolmo headers). */
#include "krans.h"
#include <stdlib.h>
#include <string.h>

#define KR_PB 12
#define KR_M (1u << KR_PB)
#define KR_N 8            /* interleaved states, each with its own word stream: eight independent decode chains */
#define KR_L (1u << 16)   /* lower bound; 16-bit renormalisation: one read at most per symbol */

size_t kolmo_krans_bound(size_t n) { return n + n / 8 + 256 * 3 + 32; }

static size_t kr_put_uv(uint8_t *p, uint32_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
static int kr_get_uv(const uint8_t *p, size_t n, size_t *i, uint32_t *v) {
    uint32_t r = 0;
    for (int s = 0; s < 35; s += 7) { if (*i >= n) return -1; uint8_t b = p[(*i)++]; r |= (uint32_t)(b & 127) << s; if (!(b & 128)) { *v = r; return 0; } }
    return -1;
}

/* counts -> frequencies summing to KR_M, every present symbol >= 1: scale, then move the rounding error onto the
   symbols where it costs least (the most frequent first) */
static void kr_norm(const uint32_t cnt[256], size_t n, int m, uint32_t f[256]) {
    int64_t tot = 0; int big = 0;
    for (int s = 0; s <= m; s++) {
        f[s] = cnt[s] ? (uint32_t)(((uint64_t)cnt[s] * KR_M + n / 2) / n) : 0;
        if (cnt[s] && !f[s]) f[s] = 1;
        tot += f[s]; if (cnt[s] > cnt[big]) big = s;
    }
    while (tot != KR_M) {
        if (tot < KR_M) { f[big] += (uint32_t)(KR_M - tot); tot = KR_M; break; }
        /* too much: take from the largest frequencies above 1, one step at a time from the biggest */
        int b = -1; for (int s = 0; s <= m; s++) if (f[s] > 1 && (b < 0 || f[s] > f[b])) b = s;
        uint32_t d = (uint32_t)(tot - KR_M), room = f[b] - 1, k = d < room ? d : room;
        if (k > f[b] / 8 + 1) k = f[b] / 8 + 1;
        f[b] -= k; tot -= k;
    }
}

size_t kolmo_krans_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { return kolmo_krans_compress_lim(in, n, out, cap, (size_t)-1); }
/* 16 * log2(v) for v in 1..4096, by the leading bit and a 4-bit table of the next bits (error under 1/16 bit) */
static uint32_t kr_lg16(uint32_t v) { static const uint8_t F[16] = {0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15}; int h = 31 - __builtin_clz(v); uint32_t fr = h >= 4 ? (v >> (h - 4)) & 15 : (v << (4 - h)) & 15; return 16u * (uint32_t)h + F[fr]; }
size_t kolmo_krans_compress_lim(const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t limit) {
    if (!n || cap < kolmo_krans_bound(n)) return 0;
    uint32_t cnt[256] = {0}, f[256] = {0}, c0[257]; int m = 0;
    for (size_t i = 0; i < n; i++) cnt[in[i]]++;
    for (int s = 0; s < 256; s++) if (cnt[s]) m = s;
    kr_norm(cnt, n, m, f);
    size_t o = 0; out[o++] = (uint8_t)m;
    for (int s = 0; s <= m; s++) o += kr_put_uv(out + o, f[s]);
    int used = 0; for (int s = 0; s <= m; s++) used += f[s] != 0;
    if (used == 1) return o;
    if (limit != (size_t)-1) {   /* estimate the coded size from the frequencies; give up when it cannot beat limit */
        uint64_t bits16 = 0; for (int s = 0; s <= m; s++) if (cnt[s]) bits16 += (uint64_t)cnt[s] * (16u * KR_PB - kr_lg16(f[s]));
        if (o + 4 * KR_N + 2 * (KR_N - 1) + bits16 / 128 >= limit) return 0;
    }
    c0[0] = 0; for (int s = 0; s <= m; s++) c0[s + 1] = c0[s] + f[s];
    uint64_t rcp[256]; for (int s = 0; s <= m; s++) rcp[s] = f[s] ? ((1ull << 44) + f[s] - 1) / f[s] : 0;
    /* encode backwards, state k into its own word stream (each one grows down from the end of its region of tmp) */
    const size_t rg = 2 * (n / KR_N + 2);   /* at most one 16-bit word per symbol */
    uint8_t *tmp = malloc(KR_N * rg); if (!tmp) return 0;
    uint8_t *p[KR_N], *e[KR_N]; uint32_t x[KR_N];
    for (int k = 0; k < KR_N; k++) { p[k] = e[k] = tmp + (size_t)(k + 1) * rg; x[k] = KR_L; }
    for (size_t i = n; i-- > 0;) {
        const unsigned s = in[i], k = i & (KR_N - 1); const uint32_t fs = f[s]; uint32_t v = x[k];
        if (v >= ((KR_L >> KR_PB) << 16) * fs) { p[k] -= 2; p[k][0] = (uint8_t)(v >> 8); p[k][1] = (uint8_t)v; v >>= 16; }
        const uint32_t qd = (uint32_t)(((uint64_t)v * rcp[s]) >> 44);   /* = v / fs exactly: v * fs < 2^44 */
        x[k] = (qd << KR_PB) + (v - qd * fs) + c0[s];
    }
    size_t L[KR_N], need = o + 4 * KR_N + 5 * (KR_N - 1); for (int k = 0; k < KR_N; k++) { L[k] = (size_t)(e[k] - p[k]); need += L[k]; }
    if (need > cap) { free(tmp); return 0; }
    for (int k = 0; k < KR_N; k++) { out[o++] = (uint8_t)x[k]; out[o++] = (uint8_t)(x[k] >> 8); out[o++] = (uint8_t)(x[k] >> 16); out[o++] = (uint8_t)(x[k] >> 24); }
    for (int k = 0; k < KR_N - 1; k++) o += kr_put_uv(out + o, (uint32_t)L[k]);
    for (int k = 0; k < KR_N; k++) { memcpy(out + o, p[k], L[k]); o += L[k]; }
    free(tmp); return o;
}

int kolmo_krans_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (!n || cn < 2) return -1;
    size_t i = 0; const int m = in[i++]; uint32_t f[256] = {0}, tot = 0; int used = 0, only = 0;
    for (int s = 0; s <= m; s++) { if (kr_get_uv(in, cn, &i, &f[s]) || f[s] > KR_M) return -1; tot += f[s]; if (f[s]) { used++; only = s; } }
    if (tot != KR_M) return -1;
    if (used == 1) { if (i != cn) return -1; memset(out, only, n); return 0; }
    /* slot table, one 32-bit entry per slot: symbol (bits 0-7), frequency (8-19), slot - start of the symbol (20-31) */
    uint32_t tb[KR_M];
    for (uint32_t s = 0, c = 0; s <= (uint32_t)m; s++) for (uint32_t k = 0; k < f[s]; k++, c++) tb[c] = s | f[s] << 8 | k << 20;
    if (cn - i < 4 * KR_N) return -1;
    uint32_t x[KR_N]; const uint8_t *q = in + i;
    for (int k = 0; k < KR_N; k++, q += 4) { x[k] = (uint32_t)q[0] | (uint32_t)q[1] << 8 | (uint32_t)q[2] << 16 | (uint32_t)q[3] << 24; if (x[k] < KR_L) return -1; }
    i += 4 * KR_N;
    uint32_t L[KR_N - 1]; for (int k = 0; k < KR_N - 1; k++) if (kr_get_uv(in, cn, &i, &L[k])) return -1;
    const uint8_t *p[KR_N], *pe[KR_N];
    for (int k = 0; k < KR_N; k++) {
        size_t len = k < KR_N - 1 ? L[k] : cn - i; if (len > cn - i) return -1;
        p[k] = in + i; pe[k] = in + i + len; i += len;
    }
    size_t j = 0;
    /* fast loop, branch-free, KR_N independent chains: after a step the state is >= 2^16 >> 12 = 16, so one 16-bit
     * read restores >= 2^16 (masks, not a select: a branch here mispredicts). A run of groups starts only when every
     * stream holds a word for each group of the run. */
#define KR_STEP(K) do { const uint32_t e_ = tb[x[K] & (KR_M - 1)]; o8[K] = (uint8_t)e_; \
        uint32_t v_ = (e_ >> 8 & (KR_M - 1)) * (x[K] >> KR_PB) + (e_ >> 20); \
        const uint32_t r_ = v_ < KR_L, w_ = ((uint32_t)p[K][0] << 8 | p[K][1]) & (0u - r_); x[K] = v_ << (r_ << 4) | w_; p[K] += 2 * r_; } while (0)
    for (;;) {
        size_t g = (size_t)(pe[0] - p[0]);
        for (int k = 1; k < KR_N; k++) if ((size_t)(pe[k] - p[k]) < g) g = (size_t)(pe[k] - p[k]);
        g /= 2; if (g > (n - j) / KR_N) g = (n - j) / KR_N;
        if (!g) break;
        for (size_t e = j + KR_N * g; j < e; j += KR_N) {
            uint8_t o8[KR_N];
            KR_STEP(0); KR_STEP(1); KR_STEP(2); KR_STEP(3); KR_STEP(4); KR_STEP(5); KR_STEP(6); KR_STEP(7);
            memcpy(out + j, o8, KR_N);
        }
    }
#undef KR_STEP
    for (; j < n; j++) {
        const unsigned k = j & (KR_N - 1); uint32_t v = x[k]; const uint32_t e = tb[v & (KR_M - 1)]; out[j] = (uint8_t)e;
        v = (e >> 8 & (KR_M - 1)) * (v >> KR_PB) + (e >> 20);
        if (v < KR_L) { if (pe[k] - p[k] < 2) return -1; v = v << 16 | (uint32_t)p[k][0] << 8 | p[k][1]; p[k] += 2; }
        x[k] = v;
    }
    for (int k = 0; k < KR_N; k++) if (x[k] != KR_L || p[k] != pe[k]) return -1;
    return 0;
}
