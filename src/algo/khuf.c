/* Kolmo Huffman (khuf). Copyright 2026 Adnane Aqartit and contributors, Apache-2.0. Format in khuf.h.
   Written for Kolmo from the textbook algorithms: optimal length-limited code lengths by package-merge (Larmore and
   Hirschberg, "A fast algorithm for optimal length-limited Huffman codes", 1990), canonical codes, code lengths coded
   with a second canonical code (as in Deflate's code-length code), and a table decoder that reads up to six symbols
   per lookup with branchless 64-bit refills. No code from zstd's HUF. */
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

/* ---- code lengths: package-merge ----
   The m present symbols sorted by (count, symbol) are the leaves. List 1 is the leaves; list l + 1 merges the leaves
   with the pairs (packages) of list l, a leaf before a package of equal weight. The first 2m - 2 items of list
   maxlen are the optimal selection: walking back, a leaf among the first `need` items of a list gains one bit, and
   the packages among them ask for 2 x as many items of the list below. */
int kolmo_khuf_lengths(const uint32_t *cnt, int nsym, int maxlen, uint8_t *len) {
    int sym[256], tmp[256], m = 0; uint32_t mx = 0;
    memset(len, 0, (size_t)nsym);
    for (int s = 0; s < nsym; s++) if (cnt[s]) { sym[m++] = s; mx |= cnt[s]; }
    if (m == 0) return 0;
    if (m == 1) { len[sym[0]] = 1; return 1; }
    if (maxlen > KHUF_MAXLEN) maxlen = KHUF_MAXLEN;
    while ((1 << maxlen) < m) maxlen++;
    /* stable radix sort by count, a byte per pass (symbols enter in increasing order: ties stay by symbol) */
    for (int sh = 0; sh < 32 && (mx >> sh); sh += 8) {
        int c[257] = {0};
        for (int i = 0; i < m; i++) c[((cnt[sym[i]] >> sh) & 255) + 1]++;
        for (int d = 0; d < 256; d++) c[d + 1] += c[d];
        for (int i = 0; i < m; i++) tmp[c[(cnt[sym[i]] >> sh) & 255]++] = sym[i];
        memcpy(sym, tmp, sizeof(int) * (size_t)m);
    }
    /* Huffman lengths (Moffat-Katajainen, in place); optimal when none exceeds maxlen */
    { uint32_t A[256]; for (int i = 0; i < m; i++) A[i] = cnt[sym[i]];
      int s = 0, r = 0;
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
      }
      if ((int)A[0] <= maxlen) { for (int i = 0; i < m; i++) len[sym[i]] = (uint8_t)A[i]; return m; } }
    /* package-merge with branch-free merges: sentinels end both lists */
    uint64_t Lw[257], w[2][520], P[260]; uint8_t pk[KHUF_MAXLEN + 1][520]; int ln[KHUF_MAXLEN + 1];
    for (int i = 0; i < m; i++) { Lw[i] = cnt[sym[i]]; w[0][i] = Lw[i]; pk[0][i] = 0; }
    Lw[m] = UINT64_MAX >> 1;
    ln[0] = m;
    for (int l = 1; l < maxlen; l++) {
        const uint64_t *pw = w[(l - 1) & 1]; uint64_t *cw = w[l & 1]; uint8_t *pl = pk[l];
        const int np = ln[l - 1] / 2;
        for (int j = 0; j < np; j++) P[j] = pw[2 * j] + pw[2 * j + 1];
        P[np] = (UINT64_MAX >> 1) + 1;
        int a = 0, b = 0;
        for (int k = 0; k < m + np; k++) {
            uint64_t x = Lw[a], y = P[b]; int t = x <= y;
            cw[k] = t ? x : y; pl[k] = (uint8_t)!t; a += t; b += !t;
        }
        ln[l] = m + np;
    }
    int D[257] = {0}, need = 2 * m - 2;
    for (int l = maxlen - 1; l >= 0; l--) {
        int np = 0;
        for (int k = 0; k < need; k++) np += pk[l][k];
        D[0]++; D[need - np]--;                                    /* leaves 0 .. need - np - 1 gain a bit */
        need = 2 * np;
    }
    for (int i = 0, A = 0; i < m; i++) { A += D[i]; len[sym[i]] = (uint8_t)A; }
    return m;
}

/* bit reversal of the low l bits (l <= 16) through a byte table (constant data; was a bit loop: decmap lane) */
static const uint8_t KH_REV8[256] = {
#define R2(n) n, n + 2 * 64, n + 1 * 64, n + 3 * 64
#define R4(n) R2(n), R2(n + 2 * 16), R2(n + 1 * 16), R2(n + 3 * 16)
#define R6(n) R4(n), R4(n + 2 * 4), R4(n + 1 * 4), R4(n + 3 * 4)
    R6(0), R6(2), R6(1), R6(3)
#undef R2
#undef R4
#undef R6
};
static inline uint32_t kh_rev(uint32_t c, int l) { return ((uint32_t)KH_REV8[c & 255] << 8 | KH_REV8[(c >> 8) & 255]) >> (16 - l); }
/* canonical codes (shorter codes first, then by symbol), bit-reversed when rev (least-significant-first output);
 * returns the longest length, or -1 when the Kraft sum is not exactly one (a lone symbol of length 1 is allowed: its
 * code is 0) */
static int kh_codes_r(const uint8_t *len, int nsym, int lmax, uint16_t *code, int rev) {
    int bl[KHUF_MAXLEN + 2] = {0}, maxl = 0, m = 0; uint32_t next[KHUF_MAXLEN + 2];
    for (int s = 0; s < nsym; s++) { if (len[s] > lmax) return -1; if (len[s]) { m++; bl[len[s]]++; if (len[s] > maxl) maxl = len[s]; } }
    if (!maxl) return -1;
    uint32_t c = 0; uint64_t kraft = 0;
    for (int l = 1; l <= maxl; l++) { c = (c + (uint32_t)bl[l - 1]) << 1; next[l] = c; kraft += (uint64_t)bl[l] << (maxl - l); }
    if (kraft != (1ull << maxl) && !(m == 1 && maxl == 1)) return -1;
    for (int s = 0; s < nsym; s++) if (len[s]) { uint32_t v = next[len[s]]++; code[s] = (uint16_t)(rev ? kh_rev(v, len[s]) : v); }
    return maxl;
}
static int kh_codes(const uint8_t *len, int nsym, int lmax, uint16_t *code) { return kh_codes_r(len, nsym, lmax, code, 1); }

/* ---- little-endian bit writer and reader for the table header ---- */
typedef struct { uint8_t *p; uint64_t b; int c; } kh_bw;
static inline void bw_put(kh_bw *w, uint32_t v, int n) { w->b |= (uint64_t)v << w->c; w->c += n; while (w->c >= 8) { *w->p++ = (uint8_t)w->b; w->b >>= 8; w->c -= 8; } }
static inline uint8_t *bw_end(kh_bw *w) { if (w->c) *w->p++ = (uint8_t)w->b; return w->p; }
typedef struct { const uint8_t *p, *e; uint64_t b; int c; } kh_brd;
static inline uint32_t br_peek(kh_brd *r, int n) { while (r->c < n) { r->b |= (uint64_t)(r->p < r->e ? *r->p : 0) << r->c; r->p++; r->c += 8; } return (uint32_t)(r->b & ((1u << n) - 1)); }
static inline void br_skip(kh_brd *r, int n) { r->b >>= n; r->c -= n; }

/* ---- table header ----
   byte 0: longest code length (1..12) in bits 0-3, table mode in bits 4-5.
   mode 0 (sparse): count - 1 u8, then count pairs (symbol, length).
   mode 1 (nibbles): last symbol u8, then the lengths of symbols 0..last, two per byte, low nibble first.
   mode 2 (coded): last symbol u8, then a bit stream (least significant bit first): for the code lengths 0..maxlen,
     the length (0..7, 3 bits) of each one's code in a canonical code (the length code), then the lengths of symbols
     0..last in that code. The length code must be complete, or a lone value of length 1. */
#define KH_LC_MAX 7
static size_t kh_write_table(const uint8_t len[256], int maxl, uint8_t *o) {
    int m = 0, last = 0; for (int s = 0; s < 256; s++) if (len[s]) { m++; last = s; }
    size_t best; uint8_t tmp[300];
    /* sparse */
    o[0] = (uint8_t)maxl; o[1] = (uint8_t)(m - 1); best = 2;
    for (int s = 0; s < 256; s++) if (len[s]) { o[best++] = (uint8_t)s; o[best++] = len[s]; }
    /* nibbles */
    size_t k = 2; tmp[0] = (uint8_t)(maxl | 1 << 4); tmp[1] = (uint8_t)last;
    for (int s = 0; s <= last; s += 2) tmp[k++] = (uint8_t)(len[s] | (s + 1 <= last ? len[s + 1] << 4 : 0));
    if (k < best) { memcpy(o, tmp, k); best = k; }
    /* coded */
    uint32_t h[16] = {0}; for (int s = 0; s <= last; s++) h[len[s]]++;
    uint8_t ll[16]; uint16_t lc[16];
    if (kolmo_khuf_lengths(h, maxl + 1, KH_LC_MAX, ll) >= 1 && kh_codes(ll, maxl + 1, KH_LC_MAX, lc) > 0) {
        tmp[0] = (uint8_t)(maxl | 2 << 4); tmp[1] = (uint8_t)last;
        kh_bw w = {tmp + 2, 0, 0};
        for (int v = 0; v <= maxl; v++) bw_put(&w, ll[v], 3);
        for (int s = 0; s <= last; s++) bw_put(&w, lc[len[s]], ll[len[s]]);
        k = (size_t)(bw_end(&w) - tmp);
        if (k < best) { memcpy(o, tmp, k); best = k; }
    }
    return best;
}
static int kh_read_table(const uint8_t *p, size_t n, size_t *i, uint8_t len[256], int *maxl) {
    memset(len, 0, 256);
    if (n - *i < 2) return -1;
    int h = p[(*i)++], mode = h >> 4, got = 0, ml = h & 15;
    if (ml < 1 || ml > KHUF_MAXLEN || mode > 2) return -1;
    *maxl = ml;
    if (mode == 0) {
        int m = p[(*i)++] + 1;
        if (m < 2 || n - *i < (size_t)m * 2) return -1;
        for (int k = 0; k < m; k++) { int s = p[*i], l = p[*i + 1]; *i += 2; if (len[s] || !l || l > ml) return -1; len[s] = (uint8_t)l; }
        return 0;
    }
    int last = p[(*i)++];
    if (mode == 1) {
        size_t nb = (size_t)(last + 2) / 2;
        if (n - *i < nb) return -1;
        for (int s = 0; s <= last; s++) { int l = (p[*i + s / 2] >> (4 * (s & 1))) & 15; if (l > ml) return -1; len[s] = (uint8_t)l; got += l != 0; }
        *i += nb;
        return got >= 2 ? 0 : -1;
    }
    kh_brd r = {p + *i, p + n, 0, 0};
    uint8_t ll[16] = {0}; uint16_t lc[16]; uint8_t lt[1 << KH_LC_MAX]; memset(lt, 0xFF, sizeof lt);
    for (int v = 0; v <= ml; v++) { ll[v] = (uint8_t)br_peek(&r, 3); br_skip(&r, 3); }
    int lm = kh_codes(ll, ml + 1, KH_LC_MAX, lc); if (lm < 1) return -1;
    for (int v = 0; v <= ml; v++) if (ll[v]) for (uint32_t k = lc[v]; k < (1u << lm); k += 1u << ll[v]) lt[k] = (uint8_t)v;
    for (int s = 0; s <= last; s++) { uint32_t v = lt[br_peek(&r, lm)]; if (v > (uint32_t)ml) return -1; br_skip(&r, ll[v]); len[s] = (uint8_t)v; got += v != 0; }
    size_t used = (size_t)(r.p - (p + *i)) - (size_t)(r.c / 8);
    if (used > n - *i) return -1;
    *i += used;
    return got >= 2 ? 0 : -1;
}

/* ---- streams ----
   Blocks of at least KH_MULTI_MIN bytes are cut into KHUF_STREAMS streams, smaller ones are one stream. Stream q holds
   symbols [q * ceil(n/S), min(n, (q+1) * ceil(n/S))). The codes of a stream are written most significant bit first
   (the first code in the top bits of the first byte), so that every code prefix is a contiguous range of the decoder's
   tables. */
static inline int kh_nstreams(size_t n) { return n >= KH_MULTI_MIN ? KHUF_STREAMS : 1; }
static inline uint64_t kh_bswap(uint64_t v) { return __builtin_bswap64(v); }

/* ---- encoder ---- */
/* ce[s] = code | length << 16. kolmo_khuf_compress checks once that all streams fit, so the loop has no bounds test.
 * Four codes (at most 48 bits) are added to at most 7 pending bits before the whole bytes are written. */
#define KH_PUT(S) do { uint32_t e_ = ce[S]; a = a << (e_ >> 16) | (e_ & 0xFFFF); na += (int)(e_ >> 16); } while (0)
#define KH_FLUSH() do { kh_wr64(p, kh_bswap((a << (63 - na)) << 1)); p += na >> 3; na &= 7; } while (0)
static size_t kh_stream(const uint8_t *s, size_t m, const uint32_t ce[256], uint8_t *o) {
    uint64_t a = 0; int na = 0; uint8_t *p = o; size_t i = 0;
    for (; i + 4 <= m; i += 4) { KH_PUT(s[i]); KH_PUT(s[i + 1]); KH_PUT(s[i + 2]); KH_PUT(s[i + 3]); KH_FLUSH(); }
    for (; i < m; i++) { KH_PUT(s[i]); KH_FLUSH(); }
    if (na) { kh_wr64(p, kh_bswap((a << (63 - na)) << 1)); p++; }
    return (size_t)(p - o);
}
/* longest code length for a block of n bytes: 12 bits from KH_LEN12_MIN bytes (the decoder's tables are 4096 entries),
   11 below (2048 entries: their build is a larger share of a small block's decode) */
static inline int kh_maxlen_for(size_t n) { return n >= KH_LEN12_MIN ? 12 : 11; }
/* four streams coded in one loop (four independent chains), each from its last symbol to its first and written from
   its end backwards: a code enters at the top of the 64-bit accumulator, which shifts right by the code's length
   (ce64[s] = code << (64 - length) | length: one shift, one or, one add per symbol; the length bits fall below the
   pending bits, where they are never read). Every stream starts with the zero bits that pad its last byte, so the
   bytes flushed from the bottom of the pending bits are whole stream bytes. The 8-byte stores run up to 7 bytes
   below a stream's start: each stream has 8 bytes of gap before it, closed afterwards. */
static void kh_stream4(const uint8_t *in, size_t seg, size_t n, const uint64_t ce64[256], uint8_t *const oend[4], const uint64_t pad[4]) {
    uint64_t a0 = 0, a1 = 0, a2 = 0, a3 = 0, n0 = pad[0], n1 = pad[1], n2 = pad[2], n3 = pad[3];
    uint8_t *p0 = oend[0], *p1 = oend[1], *p2 = oend[2], *p3 = oend[3];
    const uint8_t *s0 = in, *s1 = in + seg, *s2 = in + 2 * seg, *s3 = in + 3 * seg;
    const size_t m3 = n - 3 * seg;                                  /* the last stream may be shorter */
#define KH_P(q, S) do { uint64_t e_ = ce64[S]; a##q = (a##q >> (e_ & 63)) | e_; n##q += e_; } while (0)
#define KH_F(q) do { n##q &= 255; kh_wr64(p##q - 8, kh_bswap((a##q >> 1) >> (63 - n##q))); p##q -= n##q >> 3; n##q &= 7; } while (0)
    size_t i = seg;
    for (; i > m3; i--) { KH_P(0, s0[i - 1]); KH_F(0); KH_P(1, s1[i - 1]); KH_F(1); KH_P(2, s2[i - 1]); KH_F(2); }
    for (; i >= 4; i -= 4) {
        KH_P(0, s0[i - 1]); KH_P(1, s1[i - 1]); KH_P(2, s2[i - 1]); KH_P(3, s3[i - 1]); KH_P(0, s0[i - 2]); KH_P(1, s1[i - 2]); KH_P(2, s2[i - 2]); KH_P(3, s3[i - 2]);
        KH_P(0, s0[i - 3]); KH_P(1, s1[i - 3]); KH_P(2, s2[i - 3]); KH_P(3, s3[i - 3]); KH_P(0, s0[i - 4]); KH_P(1, s1[i - 4]); KH_P(2, s2[i - 4]); KH_P(3, s3[i - 4]);
        KH_F(0); KH_F(1); KH_F(2); KH_F(3);
    }
    for (; i > 0; i--) { KH_P(0, s0[i - 1]); KH_F(0); KH_P(1, s1[i - 1]); KH_F(1); KH_P(2, s2[i - 1]); KH_F(2); KH_P(3, s3[i - 1]); KH_F(3); }
#undef KH_P
#undef KH_F
}
size_t kolmo_khuf_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (n < 2 || n > KHUF_BLOCK_MAX || cap < n + 600) return 0;
    const int S = kh_nstreams(n);
    const size_t seg = (n + S - 1) / S;
    uint32_t cnt[256], hq[KHUF_STREAMS][256]; uint8_t len[256]; uint16_t code[256];
    if (S == 4 && 3 * seg < n) {      /* one histogram per stream, four interleaved chains */
        uint32_t h2[4][256];                    /* a second table per stream (odd positions): eight chains */
        memset(hq, 0, sizeof(uint32_t) * 4 * 256); memset(h2, 0, sizeof h2);
        const uint8_t *s0 = in, *s1 = in + seg, *s2 = in + 2 * seg, *s3 = in + 3 * seg; const size_t m3 = n - 3 * seg; size_t i = 0;
        for (; i + 2 <= m3; i += 2) {
            hq[0][s0[i]]++; hq[1][s1[i]]++; hq[2][s2[i]]++; hq[3][s3[i]]++;
            h2[0][s0[i + 1]]++; h2[1][s1[i + 1]]++; h2[2][s2[i + 1]]++; h2[3][s3[i + 1]]++;
        }
        for (; i < m3; i++) { hq[0][s0[i]]++; hq[1][s1[i]]++; hq[2][s2[i]]++; hq[3][s3[i]]++; }
        for (; i < seg; i++) { hq[0][s0[i]]++; hq[1][s1[i]]++; hq[2][s2[i]]++; }
        for (int q = 0; q < 4; q++) for (int s = 0; s < 256; s++) hq[q][s] += h2[q][s];
        for (int s = 0; s < 256; s++) cnt[s] = hq[0][s] + hq[1][s] + hq[2][s] + hq[3][s];
    } else kh_hist(in, n, cnt);
    int m = kolmo_khuf_lengths(cnt, 256, kh_maxlen_for(n), len);
    if (m < 2) return 0;
    int maxl = kh_codes_r(len, 256, KHUF_MAXLEN, code, 0);
    if (maxl < 0) return 0;
    uint64_t bits = 0; for (int s = 0; s < 256; s++) bits += (uint64_t)cnt[s] * len[s];
    if (bits / 8 + 16 >= n) return 0;
    uint32_t ce[256]; for (int s = 0; s < 256; s++) ce[s] = len[s] ? (uint32_t)code[s] | (uint32_t)len[s] << 16 : 0;
    size_t h = kh_write_table(len, maxl, out), sl[KHUF_STREAMS];
    if (S == 4 && 3 * seg < n) {
        uint64_t bq[4] = {0}, ce64[256], pad[4]; for (int s = 0; s < 256; s++) if (len[s]) for (int q = 0; q < 4; q++) bq[q] += (uint64_t)hq[q][s] * len[s];
        for (int s = 0; s < 256; s++) ce64[s] = len[s] ? (uint64_t)code[s] << (64 - len[s]) | len[s] : 0;
        size_t hp = h, tot = 0;
        for (int q = 0; q < 4; q++) { sl[q] = (size_t)((bq[q] + 7) / 8); pad[q] = 8 * sl[q] - bq[q]; tot += sl[q]; }
        for (int q = 0; q < 3; q++) hp += kh_put_uv(out + hp, sl[q]);
        if (hp + tot >= n || cap - hp < tot + 4 * 8 + 16) return 0;
        uint8_t *oe[4]; size_t off = hp + 8; for (int q = 0; q < 4; q++) { off += sl[q]; oe[q] = out + off; off += 8; }
        kh_stream4(in, seg, n, ce64, oe, pad);
        for (int q = 0, d = (int)hp; q < 4; q++) { memmove(out + d, oe[q] - sl[q], sl[q]); d += (int)sl[q]; }
        return hp + tot;
    }
    size_t q0 = h + (size_t)(S - 1) * 5, o = q0;
    if (cap - q0 < bits / 8 + 8 * (size_t)S + 16) return 0;   /* the streams take at most bits / 8 + S bytes, the stores 8 more */
    for (int q = 0; q < S; q++) {
        size_t a = (size_t)q * seg < n ? (size_t)q * seg : n, b = a + seg < n ? a + seg : n;
        sl[q] = kh_stream(in + a, b - a, ce, out + o);
        o += sl[q];
    }
    size_t hp = h; for (int q = 0; q < S - 1; q++) hp += kh_put_uv(out + hp, sl[q]);
    memmove(out + hp, out + q0, o - q0);
    o = hp + (o - q0);
    return o < n ? o : 0;
}

/* ---- decoder ----
   The bit buffer b holds the next bits at its top (bit 63 first); c counts them (its low 6 bits do, see below).
   d1[x] (x: the next W bits): the first code's length | its symbol << 8.
   dm[x]: as many whole codes as fit in the next W bits, at most six: bits 0-7 the bits they use, bytes 1-6 the
   symbols, byte 7 their number. It is the last of the tables R_0..R_W, R_w[x] holding the codes that fit in w bits:
   R_0 is empty; in R_w, the range of each code s of length l <= w (contiguous: canonical codes, most significant bit
   first) is s followed by R_{w-l} (whole, in order), and the rest of R_w (prefixes of longer codes) is empty. A
   parallel array keeps for every entry the bits used after each of its first six symbols (4-bit nibbles), so that
   dropping a seventh symbol needs no lookup. Every step is branch-free over contiguous ranges. */
typedef struct { uint64_t b; uint64_t c; const uint8_t *p, *s; } kh_br;
static inline void kh_refill_safe(kh_br *r, const uint8_t *e) {
    r->c &= 63;
    while (r->c <= 56) { uint64_t v = r->p < e ? *r->p : 0; r->b |= v << (56 - r->c); r->p++; r->c += 8; }
}
#define KH_LEVELS ((2u << KHUF_MAXLEN) - 1)
#define KH_CAP 6                                /* symbols per multi-symbol entry */
#define KH_SYMS 0x00FFFFFFFFFF0000ull           /* bytes 2-6: the symbols kept from the rest */
static void kh_build(const uint8_t len[256], int W, uint16_t *d1, uint64_t *R, uint32_t *C) {
    uint8_t order[256]; int first[KHUF_MAXLEN + 2] = {0}, cnt[KHUF_MAXLEN + 2] = {0}, no;   /* symbols in canonical order */
    for (int s = 0; s < 256; s++) cnt[len[s]]++;
    cnt[0] = 0; no = 0;
    for (int l = 1; l <= W; l++) { first[l] = no; no += cnt[l]; }
    { int f[KHUF_MAXLEN + 2]; memcpy(f, first, sizeof f); for (int s = 0; s < 256; s++) if (len[s]) order[f[len[s]]++] = (uint8_t)s; }
    if (!R) {                                   /* one symbol per lookup: d1 only */
        uint32_t x = 0;
        for (int k = 0; k < no; k++) { int s = order[k], l = len[s]; uint16_t v = (uint16_t)(l | s << 8); for (uint32_t e = x + (1u << (W - l)); x < e; x++) d1[x] = v; }
        return;
    }
    /* levels needed: W, and w - l for every needed w and present length l */
    uint8_t need[KHUF_MAXLEN + 1] = {0}; need[W] = 1;
    for (int w = W; w >= 1; w--) if (need[w]) for (int l = 1; l <= w; l++) if (cnt[l]) need[w - l] = 1;
    R[0] = 0; C[0] = 0;
    for (int w = 1; w <= W; w++) {
        if (!need[w]) continue;
        uint64_t *Rw = R + (1u << w) - 1; uint32_t *Cw = C + (1u << w) - 1; uint32_t x = 0;
        for (int l = 1; l <= w; l++) {
            const int m = cnt[l]; if (!m) continue;
            const uint32_t nr = 1u << (w - l);
            const uint64_t *Rr = R + nr - 1; const uint32_t *Cr = C + nr - 1;
            const uint64_t Lb = (uint64_t)l; const uint32_t L111 = (uint32_t)l * 0x111111u;
            /* the entries of the group's first symbol without the symbol, then each symbol's copy with it */
            uint64_t *G = Rw + x; uint32_t *Gc = Cw + x;
            for (uint32_t j = 0; j < nr; j++) {
                uint64_t r = Rr[j], rc = Cr[j], rn = r >> 56, full = rn == KH_CAP;
                uint64_t nb = Lb + (full ? (rc >> (4 * (KH_CAP - 2))) & 15 : r & 255);
                G[j] = nb | ((r << 8) & KH_SYMS) | (rn + 1 - full) << 56;
                if (w < W) Gc[j] = ((uint32_t)rc << 4) + L111;   /* the top level is never a rest */
            }
            const uint8_t *sy = order + first[l];
            for (int k = m - 1; k >= 0; k--) {
                uint64_t *Ro = G + (size_t)k * nr; uint32_t *Co = Gc + (size_t)k * nr; const uint64_t S8 = (uint64_t)sy[k] << 8;
                if (w < W) for (uint32_t j = 0; j < nr; j++) { Ro[j] = G[j] | S8; Co[j] = Gc[j]; }
                else for (uint32_t j = 0; j < nr; j++) Ro[j] = G[j] | S8;
            }
            x += (uint32_t)m * nr;
        }
        for (; x < (1u << w); x++) { Rw[x] = 0; Cw[x] = 0; }   /* (never at the top level: its codes are complete) */
    }
}
/* one stream: rounds of K lookups while there is room (checked per round), then one step at a time with checked
 * refills; multi-symbol lookups while 8 bytes of room are left */
static void kh_dec_tail(kh_br *R, uint8_t *o, uint8_t *oe, const uint8_t *ie, const uint16_t *d1, const uint64_t *dm, const uint8_t *len, int W, int K) {
    const int sh = 64 - W;
    if (dm) {
        uint64_t b = R->b, c = R->c; const uint8_t *p = R->p;
        while (oe - o >= KH_CAP * K + 8 - KH_CAP && ie - p >= 8) {
            c &= 63; b |= kh_bswap(kh_rd64(p)) >> c; p += (63 - c) >> 3; c |= 56;
            for (int k = 0; k < K; k++) { uint64_t e = dm[b >> sh]; kh_wr64(o, e >> 8); o += e >> 56; b <<= (e & 63); c -= e; }
        }
        R->b = b; R->c = c; R->p = p;
    }
    while (o < oe) {
        kh_refill_safe(R, ie);
        if (dm && oe - o >= 8) { uint64_t e = dm[R->b >> sh]; kh_wr64(o, e >> 8); o += e >> 56; R->b <<= (e & 63); R->c -= e & 255; }
        else if (dm) { uint32_t v = (uint8_t)(dm[R->b >> sh] >> 8), l = len[v]; *o++ = (uint8_t)v; R->b <<= l; R->c -= l; }   /* first symbol */
        else { uint32_t e = d1[R->b >> sh]; *o++ = (uint8_t)(e >> 8); R->b <<= (e & 63); R->c -= e & 255; }
    }
}
/* the main loop: S streams, K lookups per refill, rounds counted ahead so the loop has no bounds tests: a round
   writes at most 6K bytes (+2 for the last store of 8) per stream and advances a stream's input by at most 7 bytes.
   c's low 6 bits count the buffered bits: a lookup subtracts its whole entry (bits 0-7: the bits used; the borrow
   only reaches bits above), and the refill masks it. The next refill's address is known a round ahead (it depends on
   c after the last refill only), so its load is off the lookups' dependency chain; a marker bit in place of c (zstd's
   technique) saves a subtraction per lookup but puts that load on the chain: 18% slower here. */
#define KH_REF(q) do { c##q &= 63; b##q |= kh_bswap(kh_rd64(p##q)) >> c##q; p##q += (63 - c##q) >> 3; c##q |= 56; } while (0)
#define KH_LK(q) do { uint64_t e_ = dm[b##q >> sh]; kh_wr64(o##q, e_ >> 8); o##q += e_ >> 56; b##q <<= (e_ & 63); c##q -= e_; } while (0)
static inline size_t kh_rounds(uint8_t *const *o, uint8_t *const *oe, const kh_br *r, const uint8_t *ie, int S, int K) {
    size_t R = (size_t)-1;
    for (int q = 0; q < S; q++) {
        size_t ro = oe[q] - o[q] >= KH_CAP * K + 8 - KH_CAP ? (size_t)(oe[q] - o[q] - (8 - KH_CAP)) / (size_t)(KH_CAP * K) : 0;
        size_t ri = ie - r[q].p >= 8 ? (size_t)(ie - r[q].p - 8) / 7 + 1 : 0;
        if (ro < R) R = ro;
        if (ri < R) R = ri;
    }
    return R;
}
/* The R rounds themselves: kolmo_khuf_dec4_c is the reference of the assembly kernel kolmo_khuf_dec4_x86_64
   (src/arch/x86_64/khuf_x86_64.S, BMI2), compared by tests/test_asm.c. */
void kolmo_khuf_dec4_c(kolmo_khuf_st4 *st, size_t R, const uint64_t *dm, int W) {
    const int sh = 64 - W;
    uint64_t b0 = st->b[0], b1 = st->b[1], b2 = st->b[2], b3 = st->b[3], c0 = st->c[0], c1 = st->c[1], c2 = st->c[2], c3 = st->c[3];
    const uint8_t *p0 = st->p[0], *p1 = st->p[1], *p2 = st->p[2], *p3 = st->p[3];
    uint8_t *o0 = st->o[0], *o1 = st->o[1], *o2 = st->o[2], *o3 = st->o[3];
    if (W == 12) {
        do { KH_REF(0); KH_REF(1); KH_REF(2); KH_REF(3); for (int k = 0; k < 4; k++) { KH_LK(0); KH_LK(1); KH_LK(2); KH_LK(3); } } while (--R);
    } else {
        do { KH_REF(0); KH_REF(1); KH_REF(2); KH_REF(3); for (int k = 0; k < 5; k++) { KH_LK(0); KH_LK(1); KH_LK(2); KH_LK(3); } } while (--R);
    }
    st->b[0] = b0; st->b[1] = b1; st->b[2] = b2; st->b[3] = b3; st->c[0] = c0; st->c[1] = c1; st->c[2] = c2; st->c[3] = c3;
    st->p[0] = p0; st->p[1] = p1; st->p[2] = p2; st->p[3] = p3; st->o[0] = o0; st->o[1] = o1; st->o[2] = o2; st->o[3] = o3;
}
#if !defined(KOLMO_NO_ASM) && defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
void kolmo_khuf_dec4_x86_64(kolmo_khuf_st4 *st, size_t R, const uint64_t *dm, int W);   /* khuf_x86_64.S */
static int kh_use_asm(void) {   /* BMI2, unless KOLMO_CPU asks for a level below avx2 (c, sse2, sse41) */
    static int v = -1; int r = __atomic_load_n(&v, __ATOMIC_RELAXED);
    if (r < 0) {
        const char *e = getenv("KOLMO_CPU");
        __builtin_cpu_init();
        r = __builtin_cpu_supports("bmi2") && !(e && (!strcmp(e, "c") || !strcmp(e, "sse2") || !strcmp(e, "sse41")));
        __atomic_store_n(&v, r, __ATOMIC_RELAXED);
    }
    return r;
}
#define KH_DEC4(st, R, dm, W) (kh_use_asm() ? kolmo_khuf_dec4_x86_64(st, R, dm, W) : kolmo_khuf_dec4_c(st, R, dm, W))
#else
#define KH_DEC4(st, R, dm, W) kolmo_khuf_dec4_c(st, R, dm, W)
#endif
static void kh_dec_main(kh_br *r, uint8_t **o, uint8_t **oe, const uint8_t *ie, const uint64_t *dm, int W, int K) {
    for (;;) {
        size_t R = kh_rounds(o, oe, r, ie, 4, K); if (R < 4) return;
        kolmo_khuf_st4 st;
        for (int q = 0; q < 4; q++) { st.b[q] = r[q].b; st.c[q] = r[q].c; st.p[q] = r[q].p; st.o[q] = o[q]; }
        KH_DEC4(&st, R, dm, W);
        for (int q = 0; q < 4; q++) { r[q].b = st.b[q]; r[q].c = st.c[q]; r[q].p = st.p[q]; o[q] = st.o[q]; }
    }
}
/* one symbol per lookup (high-entropy blocks, where few codes fit in W bits): d1 entries are 2 bytes, each lookup
   writes one byte at a fixed offset of the round, and a stream's output pointer moves once per round */
#define KH_LK1(q, k) do { uint32_t e_ = d1[b##q >> sh]; o##q[k] = (uint8_t)(e_ >> 8); b##q <<= (e_ & 63); c##q -= e_; } while (0)
#define KH_RD1(k) do { KH_LK1(0, k); KH_LK1(1, k); KH_LK1(2, k); KH_LK1(3, k); } while (0)
static void kh_dec_main1(kh_br *r, uint8_t **o, uint8_t **oe, const uint8_t *ie, const uint16_t *d1, int W, int K) {
    const int sh = 64 - W;
    for (;;) {
        size_t R = (size_t)-1;
        for (int q = 0; q < 4; q++) {
            size_t ro = (size_t)(oe[q] - o[q]) / (size_t)K, ri = ie - r[q].p >= 8 ? (size_t)(ie - r[q].p - 8) / 7 + 1 : 0;
            if (ro < R) R = ro;
            if (ri < R) R = ri;
        }
        if (R < 4) return;
        uint64_t b0 = r[0].b, b1 = r[1].b, b2 = r[2].b, b3 = r[3].b, c0 = r[0].c, c1 = r[1].c, c2 = r[2].c, c3 = r[3].c;
        const uint8_t *p0 = r[0].p, *p1 = r[1].p, *p2 = r[2].p, *p3 = r[3].p;
        uint8_t *o0 = o[0], *o1 = o[1], *o2 = o[2], *o3 = o[3];
        if (K == 4) {
            do { KH_REF(0); KH_REF(1); KH_REF(2); KH_REF(3); KH_RD1(0); KH_RD1(1); KH_RD1(2); KH_RD1(3); o0 += 4; o1 += 4; o2 += 4; o3 += 4; } while (--R);
        } else {
            do { KH_REF(0); KH_REF(1); KH_REF(2); KH_REF(3); KH_RD1(0); KH_RD1(1); KH_RD1(2); KH_RD1(3); KH_RD1(4); o0 += 5; o1 += 5; o2 += 5; o3 += 5; } while (--R);
        }
        r[0].b = b0; r[1].b = b1; r[2].b = b2; r[3].b = b3; r[0].c = c0; r[1].c = c1; r[2].c = c2; r[3].c = c3;
        r[0].p = p0; r[1].p = p1; r[2].p = p2; r[3].p = p3; o[0] = o0; o[1] = o1; o[2] = o2; o[3] = o3;
    }
}
#ifndef KH_MULTI_T
#define KH_MULTI_T 21   /* multi-symbol lookups when W / (mean code length) >= KH_MULTI_T / 10 */
#endif
int kolmo_khuf_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (n < 2 || n > KHUF_BLOCK_MAX) return -1;
    uint8_t len[256]; uint16_t code[256]; size_t i = 0; int maxl;
    if (kh_read_table(in, cn, &i, len, &maxl)) return -1;
    if (kh_codes_r(len, 256, maxl, code, 0) < 1) return -1;
    const int S = kh_nstreams(n);
    /* multi-symbol tables pay when several codes fit in a lookup: mean code length (weights 2^-l) against the width */
    uint64_t A = 0; for (int s = 0; s < 256; s++) if (len[s]) A += (uint64_t)len[s] << (KHUF_MAXLEN - len[s]);
    const int Wm = maxl < 11 ? 11 : maxl;
    const int multi = S > 1 && (uint64_t)Wm * 10 * (1u << KHUF_MAXLEN) >= (uint64_t)KH_MULTI_T * A;
    const int W = multi ? Wm : maxl, K = 56 / W;
    uint16_t d1[multi ? 1 : 1 << KHUF_MAXLEN]; uint64_t Rb[multi ? KH_LEVELS : 1]; uint32_t Cb[multi ? KH_LEVELS : 1];
    kh_build(len, W, d1, multi ? Rb : NULL, Cb);
    const uint64_t *dm = multi ? Rb + (1u << W) - 1 : NULL;
    uint64_t L[KHUF_STREAMS]; size_t tot = 0;
    for (int q = 0; q < S - 1; q++) { if (kh_get_uv(in, cn, &i, &L[q]) || L[q] > cn) return -1; tot += L[q]; }
    if (tot > cn - i) return -1;
    L[S - 1] = cn - i - tot;
    const uint8_t *ie = in + cn; kh_br r[KHUF_STREAMS]; size_t seg = (n + S - 1) / S; uint8_t *o[KHUF_STREAMS], *oe[KHUF_STREAMS];
    for (int q = 0; q < S; q++) {
        r[q].b = 0; r[q].c = 0; r[q].s = r[q].p = in + i; i += L[q];
        size_t a = (size_t)q * seg < n ? (size_t)q * seg : n, b = a + seg < n ? a + seg : n;
        o[q] = out + a; oe[q] = out + b;
    }
    if (multi) kh_dec_main(r, o, oe, ie, dm, W, K); else if (S == 4) kh_dec_main1(r, o, oe, ie, d1, W, K);
    for (int q = 0; q < S; q++) {
        kh_dec_tail(&r[q], o[q], oe[q], ie, d1, dm, len, W, K);
        int64_t used = (int64_t)(r[q].p - r[q].s) * 8 - (int64_t)(r[q].c & 63);   /* bits consumed */
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
