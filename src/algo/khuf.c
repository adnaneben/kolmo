/* Kolmo Huffman (khuf). Copyright 2026 Adnane Aqartit and contributors, Apache-2.0. Format in khuf.h.
   Written for Kolmo from the textbook algorithms: Huffman code lengths by Moffat and Katajainen's in-place method
   ("In-place calculation of minimum-redundancy codes", 1995), a Kraft-sum repair to the length limit, canonical codes,
   and a table decoder with branchless 64-bit refills. No code from zstd's HUF. */
#include "khuf.h"
#include <string.h>
#include <stdlib.h>

static inline uint64_t kh_rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }   /* little-endian hosts */
static inline void kh_wr64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "khuf: little-endian hosts only (x86-64, AArch64)"
#endif

static size_t kh_put_uv(uint8_t *p, uint64_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
static int kh_get_uv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int sh = 0;
    while (*i < n && sh < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << sh; if (!(c & 128)) { *v = r; return 0; } sh += 7; }
    return -1;
}

static void kh_hist(const uint8_t *p, size_t n, uint32_t c[256]) {
    uint32_t h[4][256]; memset(h, 0, sizeof h); size_t i = 0;
    for (; i + 4 <= n; i += 4) { h[0][p[i]]++; h[1][p[i + 1]]++; h[2][p[i + 2]]++; h[3][p[i + 3]]++; }
    for (; i < n; i++) h[0][p[i]]++;
    for (int s = 0; s < 256; s++) c[s] = h[0][s] + h[1][s] + h[2][s] + h[3][s];
}

/* ---- code lengths ---- */
int kolmo_khuf_lengths(const uint32_t cnt[256], int nsym, int maxlen, uint8_t len[256]) {
    int sym[256], m = 0; uint32_t A[256];
    memset(len, 0, (size_t)nsym);
    for (int s = 0; s < nsym; s++) if (cnt[s]) sym[m++] = s;
    if (m == 0) return 0;
    if (m == 1) { len[sym[0]] = 1; return 1; }
    for (int i = 1; i < m; i++) {                                   /* insertion sort, ascending count, then symbol */
        int s = sym[i], j = i;
        while (j > 0 && (cnt[sym[j - 1]] > cnt[s])) { sym[j] = sym[j - 1]; j--; }
        sym[j] = s;
    }
    for (int i = 0; i < m; i++) A[i] = cnt[sym[i]];
    /* Moffat-Katajainen: A[i] becomes the code length of the i-th lightest symbol */
    { int s = 0, r = 0;
      for (int t = 0; t < m - 1; t++) {
          if (s >= m || (r < t && A[r] < A[s])) { A[t] = A[r]; A[r] = (uint32_t)t; r++; } else { A[t] = A[s]; s++; }
          if (s >= m || (r < t && A[r] < A[s])) { A[t] += A[r]; A[r] = (uint32_t)t; r++; } else { A[t] += A[s]; s++; }
      }
      A[m - 2] = 0;
      for (int t = m - 3; t >= 0; t--) A[t] = A[A[t]] + 1;
      int a = 1, u = 0, d = 0, t = m - 2, x = m - 1;
      while (a > 0) {
          while (t >= 0 && (int)A[t] == d) { u++; t--; }
          while (a > u) { A[x] = (uint32_t)d; x--; a--; }
          a = 2 * u; d++; u = 0;
      } }
    /* length limit: clamp, then lengthen the lightest codes until the Kraft sum fits, then shorten the heaviest codes
     * greedily until the code is complete (always reachable: see khuf.h) */
    const int64_t cap = (int64_t)1 << maxlen; int64_t K = 0;
    for (int i = 0; i < m; i++) { if ((int)A[i] > maxlen) A[i] = (uint32_t)maxlen; K += (int64_t)1 << (maxlen - (int)A[i]); }
    while (K > cap) {
        int b = -1;   /* the lightest symbol among those with the longest length below maxlen */
        for (int i = 0; i < m; i++) if ((int)A[i] < maxlen && (b < 0 || A[i] > A[b])) b = i;
        if (b < 0) return -1;
        K -= (int64_t)1 << (maxlen - (int)A[b] - 1); A[b]++;
    }
    while (K < cap) {
        int64_t d = cap - K; int b = -1;   /* the heaviest symbol whose shortening fits the deficit, longest first */
        for (int i = m - 1; i >= 0; i--) if (A[i] > 1 && ((int64_t)1 << (maxlen - (int)A[i])) <= d && (b < 0 || A[i] > A[b])) b = i;
        if (b < 0) return -1;
        K += (int64_t)1 << (maxlen - (int)A[b]); A[b]--;
    }
    for (int i = 0; i < m; i++) len[sym[i]] = (uint8_t)A[i];
    return m;
}

static inline uint32_t kh_rev(uint32_t c, int l) { uint32_t r = 0; for (int i = 0; i < l; i++) { r = (r << 1) | (c & 1); c >>= 1; } return r; }
/* canonical codes, bit-reversed for least-significant-first output; returns the longest length, or -1 when the Kraft
 * sum is not exactly one */
static int kh_codes(const uint8_t len[256], int nsym, uint16_t code[256]) {
    int bl[KHUF_MAXLEN + 2] = {0}, maxl = 0; uint32_t next[KHUF_MAXLEN + 2];
    for (int s = 0; s < nsym; s++) { if (len[s] > KHUF_MAXLEN) return -1; if (len[s]) { bl[len[s]]++; if (len[s] > maxl) maxl = len[s]; } }
    if (!maxl) return -1;
    uint32_t c = 0; uint64_t kraft = 0;
    for (int l = 1; l <= maxl; l++) { c = (c + (uint32_t)bl[l - 1]) << 1; next[l] = c; kraft += (uint64_t)bl[l] << (maxl - l); }
    if (kraft != (1ull << maxl)) return -1;
    for (int s = 0; s < nsym; s++) if (len[s]) code[s] = (uint16_t)kh_rev(next[len[s]]++, len[s]);
    return maxl;
}

/* ---- table header ---- */
static size_t kh_write_table(const uint8_t len[256], uint8_t *o) {
    int m = 0, last = 0; for (int s = 0; s < 256; s++) if (len[s]) { m++; last = s; }
    o[0] = (uint8_t)(m - 1);
    if (m - 1 < 48) { size_t k = 1; for (int s = 0; s < 256; s++) if (len[s]) { o[k++] = (uint8_t)s; o[k++] = len[s]; } return k; }
    o[1] = (uint8_t)last; size_t k = 2;
    for (int s = 0; s <= last; s += 2) o[k++] = (uint8_t)(len[s] | (s + 1 <= last ? len[s + 1] << 4 : 0));
    return k;
}
static int kh_read_table(const uint8_t *p, size_t n, size_t *i, uint8_t len[256]) {
    memset(len, 0, 256);
    if (*i >= n) return -1;
    int m = p[(*i)++] + 1, got = 0;
    if (m - 1 < 48) {
        if (n - *i < (size_t)m * 2) return -1;
        for (int k = 0; k < m; k++) { int s = p[*i], l = p[*i + 1]; *i += 2; if (len[s] || !l || l > KHUF_MAXLEN) return -1; len[s] = (uint8_t)l; }
        return m >= 2 ? 0 : -1;
    }
    if (*i >= n) return -1;
    int last = p[(*i)++]; size_t nb = (size_t)(last + 2) / 2;
    if (n - *i < nb) return -1;
    for (int s = 0; s <= last; s++) { int l = (p[*i + s / 2] >> (4 * (s & 1))) & 15; if (l > KHUF_MAXLEN) return -1; len[s] = (uint8_t)l; got += l != 0; }
    *i += nb;
    return got == m ? 0 : -1;
}

/* ---- encoder ---- */
/* ce[s] = code | length << 16. kolmo_khuf_compress checks once that all four streams fit, so the loop has no bounds
 * test. */
#define KH_PUT(S) do { uint32_t e_ = ce[S]; b |= (uint64_t)(e_ & 0xFFFF) << c; c += (int)(e_ >> 16); } while (0)
#define KH_FLUSH() do { kh_wr64(p, b); p += c >> 3; b >>= c & ~7; c &= 7; } while (0)
static size_t kh_stream(const uint8_t *s, size_t m, const uint32_t ce[256], uint8_t *o) {
    uint64_t b = 0; int c = 0; uint8_t *p = o; size_t i = 0;
    for (; i + 4 <= m; i += 4) { KH_PUT(s[i]); KH_PUT(s[i + 1]); KH_PUT(s[i + 2]); KH_PUT(s[i + 3]); KH_FLUSH(); }
    for (; i < m; i++) { KH_PUT(s[i]); KH_FLUSH(); }
    if (c) { kh_wr64(p, b); p++; }
    return (size_t)(p - o);
}
size_t kolmo_khuf_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (n < 2 || n > KHUF_BLOCK_MAX || cap < n + 600) return 0;
    uint32_t cnt[256]; uint8_t len[256]; uint16_t code[256];
    kh_hist(in, n, cnt);
    int m = kolmo_khuf_lengths(cnt, 256, KHUF_MAXLEN, len);
    if (m < 2 || kh_codes(len, 256, code) < 0) return 0;
    uint64_t bits = 0; for (int s = 0; s < 256; s++) bits += (uint64_t)cnt[s] * len[s];
    if (bits / 8 + 16 >= n) return 0;
    uint32_t ce[256]; for (int s = 0; s < 256; s++) ce[s] = len[s] ? (uint32_t)code[s] | (uint32_t)len[s] << 16 : 0;
    size_t h = kh_write_table(len, out), seg = (n + 3) / 4, q0 = h + 3 * 5, o = q0, sl[4];
    if (cap - q0 < bits / 8 + 64) return 0;   /* the four streams take at most bits / 8 + 4 bytes, the stores 8 more */
    for (int q = 0; q < 4; q++) {
        size_t a = (size_t)q * seg < n ? (size_t)q * seg : n, b = a + seg < n ? a + seg : n;
        sl[q] = kh_stream(in + a, b - a, ce, out + o);
        o += sl[q];
    }
    size_t hp = h; for (int q = 0; q < 3; q++) hp += kh_put_uv(out + hp, sl[q]);
    memmove(out + hp, out + q0, o - q0);
    o = hp + (o - q0);
    return o < n ? o : 0;
}

/* ---- decoder ---- */
typedef struct { uint64_t b; int c; const uint8_t *p, *s; } kh_br;
#define KH_REFILL(R) do { (R).b |= kh_rd64((R).p) << (R).c; (R).p += (63 - (R).c) >> 3; (R).c |= 56; } while (0)
#define KH_DEC(R, O) do { uint16_t e_ = dt[(R).b & mask]; *(O)++ = (uint8_t)e_; (R).b >>= e_ >> 8; (R).c -= e_ >> 8; } while (0)
static inline void kh_refill_safe(kh_br *r, const uint8_t *e) {
    while (r->c <= 56) { uint64_t v = r->p < e ? *r->p : 0; r->b |= v << r->c; r->p++; r->c += 8; }
}
int kolmo_khuf_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (n < 2 || n > KHUF_BLOCK_MAX) return -1;
    uint8_t len[256]; uint16_t code[256]; size_t i = 0;
    if (kh_read_table(in, cn, &i, len)) return -1;
    int T = kh_codes(len, 256, code); if (T < 1) return -1;
    uint16_t dt[1 << KHUF_MAXLEN]; const uint64_t mask = ((uint64_t)1 << T) - 1;
    for (int s = 0; s < 256; s++) if (len[s]) for (uint32_t k = code[s]; k < (1u << T); k += 1u << len[s]) dt[k] = (uint16_t)(s | len[s] << 8);
    uint64_t L[4]; size_t tot = 0;
    for (int q = 0; q < 3; q++) { if (kh_get_uv(in, cn, &i, &L[q]) || L[q] > cn) return -1; tot += L[q]; }
    if (tot > cn - i) return -1;
    L[3] = cn - i - tot;
    const uint8_t *ie = in + cn; kh_br r[4]; size_t seg = (n + 3) / 4; uint8_t *o[4], *oe[4];
    for (int q = 0, st = (int)i; q < 4; q++) {
        r[q].b = 0; r[q].c = 0; r[q].s = r[q].p = in + st; st += (int)L[q];
        size_t a = (size_t)q * seg < n ? (size_t)q * seg : n, b = a + seg < n ? a + seg : n;
        o[q] = out + a; oe[q] = out + b;
    }
    /* fast loop: five symbols per stream per refill (5 x 11 <= 56 bits); stream 3 is the shortest */
    while (oe[3] - o[3] >= 5 && r[0].p + 8 <= ie && r[1].p + 8 <= ie && r[2].p + 8 <= ie && r[3].p + 8 <= ie) {
        KH_REFILL(r[0]); KH_REFILL(r[1]); KH_REFILL(r[2]); KH_REFILL(r[3]);
        for (int k = 0; k < 5; k++) { KH_DEC(r[0], o[0]); KH_DEC(r[1], o[1]); KH_DEC(r[2], o[2]); KH_DEC(r[3], o[3]); }
    }
    for (int q = 0; q < 4; q++) {
        kh_br *R = &r[q];
        while (o[q] < oe[q]) {
            if (oe[q] - o[q] >= 5 && R->p + 8 <= ie) { KH_REFILL(*R); for (int k = 0; k < 5; k++) KH_DEC(*R, o[q]); }
            else { kh_refill_safe(R, ie); KH_DEC(*R, o[q]); }
        }
        int64_t used = (int64_t)(R->p - R->s) * 8 - R->c;   /* bits consumed */
        if (used > (int64_t)L[q] * 8 || (L[q] && used <= (int64_t)(L[q] - 1) * 8) || (!L[q] && used)) return -1;
    }
    return 0;
}

/* ---- chunked framing (codec 27) ---- */
size_t kolmo_khuf_bound(size_t n) { return n + n / KHUF_BLOCK_MAX * 8 + 16; }
size_t kolmo_khuf_compress_chunks(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    size_t o = 0; uint8_t *tmp = malloc(KHUF_BLOCK_MAX + 1024); if (!tmp) return 0;
    for (size_t i = 0; i < n;) {
        size_t m = n - i < KHUF_BLOCK_MAX ? n - i : KHUF_BLOCK_MAX;
        if (cap - o < m + 16) { free(tmp); return 0; }
        uint8_t *h = out + o; size_t hp = 1 + kh_put_uv(h + 1, m);
        int one = 1; for (size_t k = 1; k < m; k++) if (in[i + k] != in[i]) { one = 0; break; }
        size_t z = one ? 0 : kolmo_khuf_compress(in + i, m, tmp, KHUF_BLOCK_MAX + 1024);
        if (one) { h[0] = 1; h[hp] = in[i]; o += hp + 1; }
        else if (!z || z + 4 >= m) { h[0] = 0; memcpy(h + hp, in + i, m); o += hp + m; }
        else { if (cap - o < hp + 10 + z) { free(tmp); return 0; } h[0] = 2; hp += kh_put_uv(h + hp, z); memcpy(h + hp, tmp, z); o += hp + z; }
        i += m;
    }
    free(tmp); return o;
}
int kolmo_khuf_decompress_chunks(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0, o = 0;
    while (o < n) {
        uint64_t m, z; if (i >= cn) return -1;
        uint8_t mode = in[i++]; if (kh_get_uv(in, cn, &i, &m) || m == 0 || m > KHUF_BLOCK_MAX || m > n - o) return -1;
        if (mode == 0) { if (m > cn - i) return -1; memcpy(out + o, in + i, m); i += m; }
        else if (mode == 1) { if (i >= cn) return -1; memset(out + o, in[i++], m); }
        else if (mode == 2) { if (kh_get_uv(in, cn, &i, &z) || z > cn - i || kolmo_khuf_decompress(in + i, (size_t)z, out + o, (size_t)m)) return -1; i += z; }
        else return -1;
        o += m;
    }
    return i == cn ? 0 : -1;
}
