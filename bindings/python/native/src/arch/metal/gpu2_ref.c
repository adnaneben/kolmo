/* CPU references of the gpu2 kernels (gpu2.h). Apache-2.0, Kolmo contributors. Portable C, integer arithmetic only.
 * These are the specification: gpu2.metal computes the same bytes. Research code, not in the default build. */
#include "gpu2.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- table front end ---------------- */
uint32_t kg2_ref_count_lines(const uint8_t *d, size_t n) {
    if (!n) return 0;
    uint32_t c = 0;
    for (const uint8_t *p = d, *e = d + n; (p = memchr(p, '\n', (size_t)(e - p))) != NULL; p++) c++;
    return c + (d[n - 1] != '\n');
}
/* line starts p in [b, e): p = 0, or d[p - 1] = '\n' and p < n */
uint32_t kg2_ref_line_starts(const uint8_t *d, size_t n, size_t b, size_t e, uint32_t *out) {
    uint32_t k = 0;
    if (e > n) e = n;
    if (b == 0 && e > 0) out[k++] = 0;
    size_t q = b ? b - 1 : 0;                       /* newlines q with b <= q + 1 < e */
    while (q + 1 < e) {
        const uint8_t *x = memchr(d + q, '\n', e - 1 - q);
        if (!x) break;
        q = (size_t)(x - d); out[k++] = (uint32_t)(q + 1); q++;
    }
    return k;
}

static inline int kg2_is_sep(int mode, uint8_t c, uint8_t sep) { return mode == KG2_JSONL ? (c == ',' || c == ':') : c == sep; }

/* the number grammar: -?(0|[1-9][0-9]*)(.[0-9]+)?, at most 18 digits, no negative zero */
static inline uint8_t kg2_parse(const uint8_t *s, uint32_t n, int64_t *v) {
    uint32_t i = 0; int neg = 0;
    *v = 0;
    if (n == 0 || n > 40) return 0;
    if (s[0] == '-') { neg = 1; i = 1; }
    uint32_t is = i;
    uint64_t x = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { x = x * 10 + (uint64_t)(s[i] - '0'); i++; }
    uint32_t ni = i - is, d = 0;
    if (ni == 0 || (ni > 1 && s[is] == '0')) return 0;
    if (i < n && s[i] == '.') {
        i++; uint32_t fs = i;
        while (i < n && s[i] >= '0' && s[i] <= '9') { x = x * 10 + (uint64_t)(s[i] - '0'); i++; }
        d = i - fs; if (d == 0) return 0;
    }
    if (i != n || ni + d > 18) return 0;
    if (neg && x == 0) return 0;
    *v = neg ? -(int64_t)x : (int64_t)x;
    return (uint8_t)(0x80 | d);
}

uint32_t kg2_ref_first_nf(const uint8_t *d, size_t n, int mode, uint8_t sep) {
    size_t e = 0; while (e < n && d[e] != '\n') e++;
    size_t b = 0;
    if (mode == KG2_JSONL) { if (b < e && d[b] == '{') b++; if (e > b && d[e - 1] == '}') e--; }
    uint32_t nf = 1; int q = 0;
    for (size_t i = b; i < e; i++) {
        uint8_t c = d[i];
        if (q && mode == KG2_JSONL && c == '\\') { i++; continue; }
        if (c == '"') q = !q;
        else if (!q && kg2_is_sep(mode, c, sep)) nf++;
    }
    return nf;
}

/* one line: the fused pass. JSON lines drop one leading '{' and one trailing '}'. Separators inside double quotes do
 * not split (CSV: "" inside a quoted field toggles twice; JSON: a backslash skips the next byte inside a string). */
static void kg2_line(const uint8_t *d, uint32_t b, uint32_t e, int mode, uint8_t sep, kg2_table *t, uint32_t r) {
    const uint32_t R = t->rows, K = t->cols;
    if (mode == KG2_JSONL) { if (b < e && d[b] == '{') b++; if (e > b && d[e - 1] == '}') e--; }
    uint32_t f = 0, fs = b, h = 2166136261u; int q = 0;
    for (uint32_t i = b;; i++) {
        int end = i >= e;
        uint8_t c = end ? 0 : d[i];
        if (end || (!q && kg2_is_sep(mode, c, sep))) {
            if (f < K) {
                size_t o = (size_t)f * R + r; uint32_t L = i - fs;
                t->off[o] = fs; t->len[o] = (uint16_t)(L > 65535 ? 65535 : L); t->hash[o] = h;
                t->fmt[o] = kg2_parse(d + fs, L, &t->val[o]);
            }
            f++;
            if (end) break;
            fs = i + 1; h = 2166136261u; continue;
        }
        h = (h ^ c) * 16777619u;
        if (q && mode == KG2_JSONL && c == '\\' && i + 1 < e) { i++; h = (h ^ d[i]) * 16777619u; continue; }
        if (c == '"') q = !q;
    }
    t->rowflag[r] = f != K;
    for (; f < K; f++) {                              /* missing fields: empty, at the line end */
        size_t o = (size_t)f * R + r;
        t->off[o] = e; t->len[o] = 0; t->hash[o] = 2166136261u; t->fmt[o] = 0; t->val[o] = 0;
    }
}

void kg2_ref_fields(const uint8_t *d, size_t n, int mode, uint8_t sep, kg2_table *t, uint32_t r0, uint32_t r1) {
    for (uint32_t r = r0; r < r1; r++) {
        uint32_t b = t->line_start[r], e = t->line_start[r + 1] - 1;   /* line_start[rows]: see gpu2.h */
        kg2_line(d, b, e, mode, sep, t, r);
    }
}

void kg2_ref_resid(const uint8_t *d, kg2_table *t, uint32_t r0, uint32_t r1) {
    const uint32_t R = t->rows, K = t->cols;
    for (uint32_t c = 0; c < K; c++)
        for (uint32_t r = r0; r < r1; r++) {
            size_t o = (size_t)c * R + r;
            uint8_t fm = t->fmt[o], fl = (uint8_t)((fm >> 7) | (fm & 31) << 3);
            int64_t v = t->val[o], res = v;
            if (r) {
                size_t p = o - 1;
                if (t->len[o] == t->len[p] && t->len[o] < 65535 && t->hash[o] == t->hash[p]
                    && !memcmp(d + t->off[o], d + t->off[p], t->len[o])) fl |= 2;
                if ((fm & 0x80) && fm == t->fmt[p]) { fl |= 4; res = (int64_t)((uint64_t)v - (uint64_t)t->val[p]); }
            }
            t->fl[o] = fl; t->res[o] = res;
        }
}

/* ---------------- histograms and entropy ---------------- */
void kg2_log_table(uint32_t tab[1024]) {
    for (int m = 0; m < 1024; m++) tab[m] = (uint32_t)floor(log2(1.0 + m / 1024.0) * 65536.0 + 0.5);
}
uint64_t kg2_ref_log2q16(const uint32_t tab[1024], uint64_t x) {
    if (!x) return 0;
    int k = 63 - __builtin_clzll(x);
    uint32_t m = (uint32_t)(k >= 10 ? (x >> (k - 10)) & 1023 : (x << (10 - k)) & 1023);
    return ((uint64_t)k << 16) + tab[m];
}
void kg2_ref_hist_segs(const uint8_t *d, const uint64_t *off, uint32_t s0, uint32_t s1, uint32_t *h, uint64_t *bits,
                       const uint32_t tab[1024]) {
    for (uint32_t s = s0; s < s1; s++) {
        uint32_t h0[256] = {0}, h1[256] = {0}, h2[256] = {0}, h3[256] = {0};
        const uint8_t *p = d + off[s]; uint64_t n = off[s + 1] - off[s], i = 0;
        for (; i + 4 <= n; i += 4) { h0[p[i]]++; h1[p[i + 1]]++; h2[p[i + 2]]++; h3[p[i + 3]]++; }
        for (; i < n; i++) h0[p[i]]++;
        uint32_t *o = h + 256 * (size_t)s; uint64_t ln = kg2_ref_log2q16(tab, n), b = 0;
        for (int c = 0; c < 256; c++) {
            o[c] = h0[c] + h1[c] + h2[c] + h3[c];
            if (o[c]) b += (uint64_t)o[c] * (ln - kg2_ref_log2q16(tab, o[c]));
        }
        bits[s] = b;
    }
}
void kg2_ref_hist_o1(const uint8_t *d, size_t b, size_t e, uint32_t *h) {
    for (size_t i = b ? b : 1; i < e; i++) h[(uint32_t)d[i - 1] << 8 | d[i]]++;
}

/* ---------------- krw ---------------- */
void krw_norm(const uint32_t cnt[256], uint64_t n, uint16_t f[256]) {
    memset(f, 0, 256 * sizeof f[0]);
    if (!n) return;
    uint32_t sum = 0; int mx = -1;
    for (int s = 0; s < 256; s++) {
        if (!cnt[s]) continue;
        uint32_t v = (uint32_t)(((uint64_t)cnt[s] * KRW_M) / n);
        f[s] = (uint16_t)(v ? v : 1); sum += f[s];
        if (mx < 0 || cnt[s] > cnt[mx]) mx = s;
    }
    if (sum < KRW_M) { f[mx] = (uint16_t)(f[mx] + (KRW_M - sum)); return; }
    if (sum > KRW_M) { uint32_t d = sum - KRW_M, r = f[mx] - 1u; if (d > r) d = r; f[mx] = (uint16_t)(f[mx] - d); sum -= d; }
    for (int s = 0; s < 256 && sum > KRW_M; s++)
        if (f[s] > 1) { uint32_t d = sum - KRW_M, r = f[s] - 1u; if (d > r) d = r; f[s] = (uint16_t)(f[s] - d); sum -= d; }
}

uint64_t krw_ref_encode_seg(const uint8_t *src, uint32_t m, uint32_t L, const uint16_t f[256], uint32_t *states,
                            uint32_t *counts, uint16_t *words, uint16_t *scratch) {
    uint32_t cum[256], c = 0;
    for (int s = 0; s < 256; s++) { cum[s] = c; c += f[s]; }
    uint32_t cap = (m + L - 1) / L + 2;              /* words per lane: at most one per symbol */
    uint64_t tot = 0;
    for (uint32_t l = 0; l < L; l++) {
        uint32_t x = KRW_LOW, k = 0; uint16_t *e = scratch + cap;  /* lane l's words, written backwards */
        uint32_t cntl = l < m ? (m - l + L - 1) / L : 0;
        for (uint32_t j = cntl; j-- > 0;) {
            uint8_t s = src[(size_t)j * L + l]; uint32_t fs = f[s];
            if (fs < KRW_M && x >= (fs << 20)) { *--e = (uint16_t)x; x >>= 16; k++; }
            x = ((x / fs) << KRW_PROB) + (x % fs) + cum[s];
        }
        states[l] = x; counts[l] = k;
        memcpy(words + tot, e, (size_t)k * 2); tot += k;
    }
    return tot;
}

uint64_t krw_bound(uint64_t n, uint32_t seg, uint32_t L) {
    uint64_t ns = (n + seg - 1) / seg;
    return ns * krw_seg_header(L) + 2 * n + 4 * L * ns + 64;
}

/* segments [s0, s1) of a whole buffer, each into seg_buf + s * seg_cap; seg_size[s] its coded size */
uint64_t krw_ref_encode_range(const uint8_t *in, uint64_t n, uint32_t seg, uint32_t L, uint32_t s0, uint32_t s1,
                              uint8_t *seg_buf, uint64_t seg_cap, uint64_t *seg_size) {
    uint64_t tot = 0;
    uint16_t *scratch = (uint16_t *)malloc(((size_t)seg / L + 4) * 2 * L);
    for (uint32_t s = s0; s < s1; s++) {
        uint64_t b = (uint64_t)s * seg; uint32_t m = (uint32_t)(n - b < seg ? n - b : seg);
        uint32_t cnt[256] = {0}; for (uint32_t i = 0; i < m; i++) cnt[in[b + i]]++;
        uint8_t *o = seg_buf + (size_t)s * seg_cap; uint16_t f[256];
        krw_norm(cnt, m, f); memcpy(o, f, 512);
        uint32_t *states = (uint32_t *)(o + 512), *counts = states + L;
        uint64_t w = krw_fast_encode_seg(in + b, m, L, f, states, counts, (uint16_t *)(counts + L), scratch);
        seg_size[s] = krw_seg_header(L) + 2 * w; tot += seg_size[s];
    }
    free(scratch);
    return tot;
}

int krw_ref_decode_seg(const uint8_t *code, uint64_t code_n, uint32_t L, uint8_t *dst, uint32_t m) {
    if (code_n < krw_seg_header(L)) return -1;
    uint16_t f[256]; memcpy(f, code, 512);
    uint32_t cum[257], c = 0; uint8_t sym[KRW_M];
    for (int s = 0; s < 256; s++) { cum[s] = c; for (uint32_t k = 0; k < f[s] && c + k < KRW_M; k++) sym[c + k] = (uint8_t)s; c += f[s]; }
    if (m && c != KRW_M) return -1;
    const uint32_t *states = (const uint32_t *)(code + 512), *counts = states + L;
    const uint16_t *w = (const uint16_t *)(counts + L);
    uint64_t avail = (code_n - krw_seg_header(L)) / 2, at = 0;
    for (uint32_t l = 0; l < L; l++) {
        uint32_t x = states[l], k = counts[l];
        if (at + k > avail) return -1;
        const uint16_t *p = w + at, *pe = p + k; at += k;
        uint32_t cntl = l < m ? (m - l + L - 1) / L : 0;
        for (uint32_t j = 0; j < cntl; j++) {
            uint32_t slot = x & (KRW_M - 1); uint8_t s = sym[slot];
            x = f[s] * (x >> KRW_PROB) + slot - cum[s];
            if (x < KRW_LOW) { if (p == pe) return -1; x = x << 16 | *p++; }
            dst[(size_t)j * L + l] = s;
        }
        if (p != pe || x != KRW_LOW) return -1;
    }
    return 0;
}

/* The same bytes, organised for a CPU: all lanes advance together over rows of L symbols (sequential reads and
 * writes), the division is a multiplication by floor(2^32 / f) and at most two corrections (as on the GPU), and each
 * lane's words go to its own region of scratch (L * (m / L + 3) words). */
uint64_t krw_fast_encode_seg(const uint8_t *src, uint32_t m, uint32_t L, const uint16_t f[256], uint32_t *states,
                             uint32_t *counts, uint16_t *words, uint16_t *scratch) {
    uint32_t cum[256], rcp[256], c = 0, x[1024], e[1024];
    if (L > 1024) return 0;
    for (int s = 0; s < 256; s++) { cum[s] = c; c += f[s]; rcp[s] = f[s] > 1 ? (uint32_t)(0x100000000ull / f[s]) : 0; }
    uint32_t cap = m / L + 3, rows = m / L, rem = m % L;
    for (uint32_t l = 0; l < L; l++) { x[l] = KRW_LOW; e[l] = cap; }
#define KRW_PUT(l, sym) do { uint32_t fs_ = f[sym], xx = x[l]; \
        if (fs_ < KRW_M && xx >= (fs_ << 20)) { scratch[(size_t)(l) * cap + --e[l]] = (uint16_t)xx; xx >>= 16; } \
        uint32_t q_ = fs_ > 1 ? (uint32_t)(((uint64_t)xx * rcp[sym]) >> 32) : xx, r_ = xx - q_ * fs_; \
        while (r_ >= fs_) { q_++; r_ -= fs_; } \
        x[l] = (q_ << KRW_PROB) + r_ + cum[sym]; } while (0)
    const uint8_t *row = src + (size_t)rows * L;
    for (uint32_t l = 0; l < rem; l++) KRW_PUT(l, row[l]);
    for (uint32_t j = rows; j-- > 0;) { row = src + (size_t)j * L; for (uint32_t l = 0; l < L; l++) KRW_PUT(l, row[l]); }
#undef KRW_PUT
    uint64_t tot = 0;
    for (uint32_t l = 0; l < L; l++) {
        uint32_t k = cap - e[l];
        states[l] = x[l]; counts[l] = k;
        memcpy(words + tot, scratch + (size_t)l * cap + e[l], (size_t)k * 2); tot += k;
    }
    return tot;
}
int krw_fast_decode_seg(const uint8_t *code, uint64_t code_n, uint32_t L, uint8_t *dst, uint32_t m) {
    if (code_n < krw_seg_header(L) || L > 1024) return -1;
    uint16_t f[256]; memcpy(f, code, 512);
    uint32_t cum[256], c = 0; uint8_t sym[KRW_M];
    for (int s = 0; s < 256; s++) { cum[s] = c; for (uint32_t k = 0; k < f[s] && c + k < KRW_M; k++) sym[c + k] = (uint8_t)s; c += f[s]; }
    if (m && c != KRW_M) return -1;
    const uint32_t *states = (const uint32_t *)(code + 512), *counts = states + L;
    const uint16_t *w = (const uint16_t *)(counts + L);
    uint64_t avail = (code_n - krw_seg_header(L)) / 2, at = 0;
    uint32_t x[1024]; const uint16_t *p[1024];
    for (uint32_t l = 0; l < L; l++) { x[l] = states[l]; p[l] = w + at; at += counts[l]; }
    if (at > avail) return -1;
    uint32_t rows = m / L, rem = m % L;
#define KRW_GET(l, out) do { uint32_t xx = x[l], slot = xx & (KRW_M - 1); uint8_t s_ = sym[slot]; \
        xx = f[s_] * (xx >> KRW_PROB) + slot - cum[s_]; if (xx < KRW_LOW) xx = xx << 16 | *p[l]++; x[l] = xx; (out) = s_; } while (0)
    for (uint32_t j = 0; j < rows; j++) { uint8_t *o = dst + (size_t)j * L; for (uint32_t l = 0; l < L; l++) KRW_GET(l, o[l]); }
    for (uint32_t l = 0; l < rem; l++) KRW_GET(l, dst[(size_t)rows * L + l]);
#undef KRW_GET
    for (uint32_t l = 0, a = 0; l < L; a += counts[l], l++) if (p[l] != w + a + counts[l] || x[l] != KRW_LOW) return -1;
    return 0;
}

/* ---------------- formulas (g6_fpred without __int128) ---------------- */
static const int64_t KG2_P10[19] = {1LL, 10LL, 100LL, 1000LL, 10000LL, 100000LL, 1000000LL, 10000000LL, 100000000LL,
    1000000000LL, 10000000000LL, 100000000000LL, 1000000000000LL, 10000000000000LL, 100000000000000LL,
    1000000000000000LL, 10000000000000000LL, 100000000000000000LL, 1000000000000000000LL};
typedef struct { uint64_t hi, lo; } kg2_u128;
static inline kg2_u128 kg2_mul(uint64_t a, uint64_t b) {
    uint64_t a0 = (uint32_t)a, a1 = a >> 32, b0 = (uint32_t)b, b1 = b >> 32;
    uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
    kg2_u128 r = {p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32), (mid << 32) | (uint32_t)p00};
    return r;
}
static inline kg2_u128 kg2_add(kg2_u128 a, kg2_u128 b) { kg2_u128 r = {a.hi + b.hi, a.lo + b.lo}; r.hi += r.lo < a.lo; return r; }
static inline kg2_u128 kg2_sub(kg2_u128 a, kg2_u128 b) { kg2_u128 r = {a.hi - b.hi - (a.lo < b.lo), a.lo - b.lo}; return r; }
static inline int kg2_ge(kg2_u128 a, kg2_u128 b) { return a.hi > b.hi || (a.hi == b.hi && a.lo >= b.lo); }
static inline kg2_u128 kg2_shl(kg2_u128 a, int s) {   /* 0 <= s < 64 */
    if (!s) return a;
    kg2_u128 r = {a.hi << s | a.lo >> (64 - s), a.lo << s}; return r;
}
/* floor(a / b) when it is at most 2^62, else returns 0 (b > 0) */
static inline int kg2_div(kg2_u128 a, kg2_u128 b, uint64_t *q) {
    if (!a.hi && !b.hi) { *q = a.lo / b.lo; return *q <= (1ull << 62); }
    if (b.hi < 2 && kg2_ge(a, kg2_shl(b, 63))) return 0;   /* quotient >= 2^63 (b << 63 fits when b < 2^65) */
    uint64_t r = 0;
    for (int i = 62; i >= 0; i--) {
        if (i && (b.hi >> (64 - i))) continue;              /* b << i >= 2^128 > a */
        kg2_u128 t = kg2_shl(b, i);
        if (kg2_ge(a, t)) { a = kg2_sub(a, t); r |= 1ull << i; }
    }
    *q = r; return r <= (1ull << 62);
}
static int kg2_ratio(int64_t va, int da, int64_t vb, int db, int ke, int dt, int64_t *out) {
    int e = db + dt + ke; if (vb == 0 || e > 18 || da > 18 || e < 0) return 0;
    if (va > (1LL << 60) || va < -(1LL << 60) || vb > (1LL << 60) || vb < -(1LL << 60)) return 0;
    int neg = (va < 0) != (vb < 0) && va != 0;
    uint64_t aa = va < 0 ? (uint64_t)-va : (uint64_t)va, ab = vb < 0 ? (uint64_t)-vb : (uint64_t)vb;
    kg2_u128 num = kg2_mul(aa, (uint64_t)KG2_P10[e]), den = kg2_mul(ab, (uint64_t)KG2_P10[da]);
    uint64_t q;
    if (!kg2_div(kg2_add(kg2_shl(num, 1), den), kg2_shl(den, 1), &q)) return 0;
    *out = neg ? -(int64_t)q : (int64_t)q; return 1;
}
static int kg2_rescale(int64_t v, int from, int to, int64_t *out) {
    if (to >= from) { int e = to - from; if (e > 18) return 0; int64_t lim = INT64_MAX / KG2_P10[e] / 2; if (v > lim || v < -lim) return 0; *out = v * KG2_P10[e]; return 1; }
    int e = from - to; if (e > 18 || v == INT64_MIN) return 0; int64_t q = KG2_P10[e], a = v < 0 ? -v : v, r = (a + q / 2) / q; *out = v < 0 ? -r : r; return 1;
}
static inline int64_t kg2_isub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
int kg2_fpred(int type, int ke, int64_t va, int da, int okl, int64_t vla, int dla, int64_t vb, int db, int oklt,
              int64_t vlt, int dlt, int dt, int64_t *out) {
    switch (type) {
    case 0: return kg2_ratio(va, da, vb, db, ke, dt, out);
    case 1: return okl && da == dla && kg2_rescale(kg2_isub(va, vla), da, dt, out);
    case 2: return okl && da == dla && kg2_ratio(kg2_isub(va, vla), da, vla, da, 2, dt, out);
    case 3: { int64_t r; if (!oklt || dlt != dt || !kg2_rescale(va, da, dt, &r)) return 0; *out = (int64_t)((uint64_t)vlt + (uint64_t)r); return 1; }
    case 4: return kg2_rescale(va, da, dt, out);
    case 5: {
        if (va > (1LL << 40) || va < -(1LL << 40) || vb > (1LL << 40) || vb < -(1LL << 40)) return 0;
        int e = da + db + ke - dt; if (e < 0 || e > 18) return 0;
        int neg = (va < 0) != (vb < 0) && va != 0 && vb != 0;
        uint64_t aa = va < 0 ? (uint64_t)-va : (uint64_t)va, ab = vb < 0 ? (uint64_t)-vb : (uint64_t)vb;
        uint64_t q = (uint64_t)KG2_P10[e], r;
        kg2_u128 m = kg2_mul(aa, ab), h = {0, q / 2}, qq = {0, q};
        if (!kg2_div(kg2_add(m, h), qq, &r)) return 0;
        *out = neg ? -(int64_t)r : (int64_t)r; return 1; }
    }
    return 0;
}

#define KV(c, r) V[(size_t)(c) * R + (r)]
#define KD(c, r) D[(size_t)(c) * R + (r)]
#define KOK(c, r) OK[(size_t)(c) * R + (r)]
static inline int kg2_hit(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t R, int ty, int t, int a,
                          int b, int ke, uint32_t r) {
    int64_t p;
    if (!kg2_fpred(ty, ke, KV(a, r), KD(a, r), KOK(a, r - 1), KV(a, r - 1), KD(a, r - 1), KV(b, r), KD(b, r),
                   KOK(t, r - 1), KV(t, r - 1), KD(t, r - 1), KD(t, r), &p)) return 0;
    int64_t v = KV(t, r);
    return p - v <= 2 && v - p <= 2;
}
static const int KG2_KE[5] = {0, 2, 3, 6, 9};
void kg2_ref_formula_screen(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t R, uint32_t rows,
                            const kg2_cand *cand, uint32_t i0, uint32_t i1, kg2_score *out) {
    for (uint32_t i = i0; i < i1; i++) {
        int t = cand[i].t, a = cand[i].a, b = cand[i].b, ty = cand[i].tq >> 8, ke = KG2_KE[cand[i].tq & 255];
        int hit = 0, miss = 0, seen = 0;
        for (uint32_t r = 1; r < rows && r < 3000 && seen < 64; r++) {
            if (!KOK(t, r) || !KOK(a, r) || !KOK(b, r)) continue;
            seen++;
            if (kg2_hit(V, D, OK, R, ty, t, a, b, ke, r)) hit++;
            else if (++miss > 24 || (miss >= 6 && hit * 2 < miss)) break;
        }
        int fail = seen < 16 || miss > 24 || hit * 2 < miss || hit < seen / 2;
        out[i].hit = hit; out[i].seen = fail ? -1 : seen;
    }
}
void kg2_ref_formula_score(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t R, uint32_t lim,
                           const kg2_cand *cand, uint32_t i0, uint32_t i1, const kg2_score *bound, kg2_score *out) {
    for (uint32_t i = i0; i < i1; i++) {
        int t = cand[i].t, a = cand[i].a, b = cand[i].b, ty = cand[i].tq >> 8, ke = KG2_KE[cand[i].tq & 255];
        int64_t bh = bound[t].hit, bs = bound[t].seen;
        int hit = 0, seen = 0, cut = 0;
        for (uint32_t r = 1; r < lim; r++) {
            if ((r & 63) == 0) {
                int64_t rem = lim - r;
                if (2 * hit + rem <= seen) break;
                if (bs > 0 && (hit + rem) * bs < bh * (seen + rem)) { cut = 1; break; }
            }
            if (!KOK(t, r) || !KOK(a, r) || !KOK(b, r)) continue;
            seen++;
            hit += kg2_hit(V, D, OK, R, ty, t, a, b, ke, r);
        }
        out[i].hit = cut ? -1 : hit; out[i].seen = cut ? -1 : seen;
    }
}
