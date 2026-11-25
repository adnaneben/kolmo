/* Column transpose of delimited text (T_TP): fields of a row become columns, quote-aware. */
/* ------------------------------------------------------------------ transforms (shared with the first kolmo prototype) */
#define TP_STEP(c) do { \
    if (quotes) { if (c == '"' && !inbr) inq ^= 1; else if (c == '[' && !inq) inbr = 1; else if (c == ']' && !inq) inbr = 0; } \
    if (c == '\n') { j = 0; inq = inbr = 0; } \
    else if (c == delim && !inq && !inbr && j < maxc - 1) j++; } while (0)

static void tp_classes(uint8_t *cls, uint8_t delim, int quotes) {
    memset(cls, 0, 256); cls['\n'] = 1; cls[delim] = 1;
    if (quotes) { cls['"'] = 1; cls['['] = 1; cls[']'] = 1; }
}

static void transpose_fwd(const uint8_t *in, int64_t n, uint8_t delim, int maxc, int quotes, uint8_t *out, int64_t *colsz) {
    int64_t cnt[256] = {0}; uint8_t cls[256]; tp_classes(cls, delim, quotes);
    { int j = 0, inq = 0, inbr = 0; int64_t i = 0;
      while (i < n) { int64_t s = i; while (i < n && !cls[in[i]]) i++; if (i < n) i++; cnt[j] += i - s; uint8_t c = in[i - 1]; TP_STEP(c); } }
    uint8_t *dst[256]; int64_t pos = 0;
    for (int k = 0; k < maxc; k++) { dst[k] = out + pos; colsz[k] = cnt[k]; pos += cnt[k]; }
    { int j = 0, inq = 0, inbr = 0; uint8_t *o = dst[0];
      for (int64_t i = 0; i < n; i++) { uint8_t c = in[i]; *o++ = c; if (cls[c]) { dst[j] = o; TP_STEP(c); o = dst[j]; } } }
}

static int transpose_inv(const uint8_t *in, int64_t n, uint8_t delim, int maxc, int quotes, const int64_t *colsz, uint8_t *out) {
    int64_t off[256], end[256], pos = 0; uint8_t cls[256]; tp_classes(cls, delim, quotes);
    for (int k = 0; k < maxc; k++) { if (colsz[k] < 0 || colsz[k] > n - pos) return -1; off[k] = pos; pos += colsz[k]; end[k] = pos; }
    if (pos != n) return -1;
    int j = 0, inq = 0, inbr = 0; int64_t i = 0;
    while (i < n) {
        const uint8_t *src = in + off[j]; int64_t lim = end[j] - off[j], r = 0;
        while (r < lim && !cls[src[r]]) r++;
        if (r < lim) r++;
        if (r == 0 || i + r > n) return -1;
        memcpy(out + i, src, r); off[j] += r; i += r;
        uint8_t c = out[i - 1];
        if (!cls[c]) continue;
        TP_STEP(c);
    }
    return 0;
}

/* Quote-free fast path: terminator positions found by a branchless scan, fields moved with short copies.
 * Byte-identical to transpose_fwd/inv with quotes = 0. pos needs n + 1 entries. */
#if defined(KOLMO_USE_SSE2)
#include <emmintrin.h>
#elif defined(KOLMO_USE_NEON)
#include <arm_neon.h>
#endif
/* bit masks of the bytes equal to a or '\n' (t) and to '\n' (nl) among the 64 at p */
static inline void term_masks64(const uint8_t *p, uint8_t a, uint64_t *t, uint64_t *nl) {
#if defined(KOLMO_USE_SSE2)
    const __m128i va = _mm_set1_epi8((char)a), vn = _mm_set1_epi8('\n'); uint64_t mt = 0, mn = 0;
    for (int q = 0; q < 4; q++) {
        __m128i x = _mm_loadu_si128((const __m128i *)(p + 16 * q)), en = _mm_cmpeq_epi8(x, vn);
        mn |= (uint64_t)(uint16_t)_mm_movemask_epi8(en) << (16 * q);
        mt |= (uint64_t)(uint16_t)_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(x, va), en)) << (16 * q);
    }
    *t = mt; *nl = mn;
#elif defined(KOLMO_USE_NEON) && defined(__aarch64__)
    static const uint8_t bw[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const uint8x16_t va = vdupq_n_u8(a), vn = vdupq_n_u8('\n'), w = vld1q_u8(bw);
    uint8x16_t n0 = vceqq_u8(vld1q_u8(p), vn), n1 = vceqq_u8(vld1q_u8(p + 16), vn), n2 = vceqq_u8(vld1q_u8(p + 32), vn), n3 = vceqq_u8(vld1q_u8(p + 48), vn);
    uint8x16_t t0 = vorrq_u8(n0, vceqq_u8(vld1q_u8(p), va)), t1 = vorrq_u8(n1, vceqq_u8(vld1q_u8(p + 16), va));
    uint8x16_t t2 = vorrq_u8(n2, vceqq_u8(vld1q_u8(p + 32), va)), t3 = vorrq_u8(n3, vceqq_u8(vld1q_u8(p + 48), va));
#define KOLMO_MM64(r0, r1, r2, r3) ({ uint8x16_t s0 = vpaddq_u8(vandq_u8(r0, w), vandq_u8(r1, w)), s1 = vpaddq_u8(vandq_u8(r2, w), vandq_u8(r3, w)); \
        s0 = vpaddq_u8(s0, s1); s0 = vpaddq_u8(s0, s0); vgetq_lane_u64(vreinterpretq_u64_u8(s0), 0); })
    *t = KOLMO_MM64(t0, t1, t2, t3); *nl = KOLMO_MM64(n0, n1, n2, n3);
#undef KOLMO_MM64
#else
    uint64_t mt = 0, mn = 0;
    for (int q = 0; q < 64; q++) { mt |= (uint64_t)(p[q] == a || p[q] == '\n') << q; mn |= (uint64_t)(p[q] == '\n') << q; }
    *t = mt; *nl = mn;
#endif
}
/* positions of bytes equal to a or '\n' (SIMD compare + bit iteration) */
static size_t scan_terms(const uint8_t *s, size_t n, uint8_t a, uint32_t *pos) {
    size_t c = 0, i = 0;
#if defined(KOLMO_USE_SSE2)
    const __m128i va = _mm_set1_epi8((char)a), vn = _mm_set1_epi8('\n');
    for (; i + 64 <= n; i += 64) {
        uint64_t m = 0;
        for (int q = 0; q < 4; q++) {
            __m128i x = _mm_loadu_si128((const __m128i *)(s + i + 16 * q));
            m |= (uint64_t)(uint16_t)_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(x, va), _mm_cmpeq_epi8(x, vn))) << (16 * q);
        }
        while (m) { pos[c++] = (uint32_t)(i + __builtin_ctzll(m)); m &= m - 1; }
    }
#elif defined(KOLMO_USE_NEON)
    const uint8x16_t va = vdupq_n_u8(a), vn = vdupq_n_u8('\n');
    for (; i + 16 <= n; i += 16) {
        uint8x16_t x = vld1q_u8(s + i);
        uint8x16_t e = vorrq_u8(vceqq_u8(x, va), vceqq_u8(x, vn));
        uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(e), 4)), 0);
        while (m) { int t = __builtin_ctzll(m); pos[c++] = (uint32_t)(i + (t >> 2)); m &= ~(0xFull << (t & ~3)); }
    }
#endif
    for (; i < n; i++) { pos[c] = (uint32_t)i; c += (s[i] == a) | (s[i] == '\n'); }
    return c;
}
static inline void copy_field(uint8_t *d, const uint8_t *s, size_t len, int wild) {
    if (wild && len <= 16) { uint64_t a, b; memcpy(&a, s, 8); memcpy(&b, s + 8, 8); memcpy(d, &a, 8); memcpy(d + 8, &b, 8); }
    else memcpy(d, s, len);
}
/* The first 255 delimiters partition a row. The rest of that row is a
 * single field, including any further delimiters. Two scans avoid retaining
 * a position for every field and copy the long suffix just once. */
static void tpq_wide_pass(const uint8_t *in, size_t n, uint8_t delim, int64_t *sizes,
                          uint8_t **dst, uint8_t **end) {
    unsigned lane = 0;
    size_t start = 0;
    for (size_t off = 0; off < n; off += 64) {
        const uint8_t *p = in + off;
        uint8_t tail[64];
        if (n - off < 64) {
            memset(tail, 0, sizeof(tail)); memcpy(tail, p, n - off); p = tail;
        }
        uint64_t terms, lines;
        term_masks64(p, delim, &terms, &lines);
        if (n - off < 64) terms &= (UINT64_C(1) << (n - off)) - 1;
        while (terms) {
            uint64_t relevant = lane == 255 ? terms & lines : terms;
            if (!relevant) break;
            unsigned bit = (unsigned)__builtin_ctzll(relevant);
            size_t stop = off + bit + 1, len = stop - start;
            if (!dst) sizes[lane] += len;
            else {
                copy_field(dst[lane], in + start, len, n - start >= 16 && end[lane] - dst[lane] >= 16);
                dst[lane] += len;
            }
            lane = (lines & (UINT64_C(1) << bit)) ? 0 : lane + 1;
            start = stop;
            /* Remove every terminator up to and including the one consumed. */
            terms &= ~((relevant & -relevant) - 1);
            terms &= terms - 1;
        }
    }
    if (!dst) sizes[lane] += n - start;
    else memcpy(dst[lane], in + start, n - start);
}

static void tpq_wide_fwd(const uint8_t *in, size_t n, uint8_t delim, uint8_t *out, int64_t *sizes) {
    uint8_t *dst[256], *end[256];
    memset(sizes, 0, 256 * sizeof(*sizes));
    tpq_wide_pass(in, n, delim, sizes, NULL, NULL);
    for (int j = 0; j < 256; j++) { dst[j] = out; out += sizes[j]; end[j] = out; }
    tpq_wide_pass(in, n, delim, sizes, dst, end);
}

static void tpq_fwd(const uint8_t *in, size_t n, uint8_t delim, int maxc, uint8_t *out, int64_t *colsz, uint32_t *pos) {
    size_t T = scan_terms(in, n, delim, pos);
    int64_t cnt[256] = {0}; int j = 0; size_t st = 0;
    for (size_t t = 0; t < T; t++) { size_t p = pos[t]; cnt[j] += p + 1 - st; st = p + 1; if (in[p] == '\n') j = 0; else if (j < maxc - 1) j++; }
    cnt[j] += n - st;
    uint8_t *dst[256], *cend[256]; int64_t o = 0;
    for (int k = 0; k < maxc; k++) { dst[k] = out + o; colsz[k] = cnt[k]; o += cnt[k]; cend[k] = out + o; }
    j = 0; st = 0;
    for (size_t t = 0; t < T; t++) {
        size_t p = pos[t], len = p + 1 - st;
        copy_field(dst[j], in + st, len, st + 16 <= n && dst[j] + 16 <= cend[j]);
        dst[j] += len; st = p + 1;
        if (in[p] == '\n') j = 0; else if (j < maxc - 1) j++;
    }
    memcpy(dst[j], in + st, n - st);
}
static int tpq_inv(const uint8_t *in, size_t n, uint8_t delim, int maxc, const int64_t *colsz, uint8_t *out, uint32_t *pos) {
    size_t off[256], end[256], pi[256], pe[256], o = 0, tot = 0;
    for (int k = 0; k < maxc; k++) { if (colsz[k] < 0 || (size_t)colsz[k] > n - o) return -1; off[k] = o; end[k] = o + colsz[k]; o += colsz[k]; }
    if (o != n) return -1;                                               /* sizes validated before any read */
    for (int k = 0; k < maxc; k++) { pi[k] = tot; tot += scan_terms(in + off[k], colsz[k], delim, pos + tot); pe[k] = tot; }
    size_t w = 0; int j = 0;
    while (w < n) {
        if (pi[j] < pe[j]) {
            size_t base = end[j] - colsz[j], q = base + pos[pi[j]++], len = q + 1 - off[j];
            if (w + len > n) return -1;
            copy_field(out + w, in + off[j], len, w + 16 <= n && off[j] + 16 <= n);
            w += len; off[j] = q + 1;
            if (in[q] == '\n') j = 0; else if (j < maxc - 1) j++;
        } else {                                                     /* last field of the block, no terminator */
            size_t len = end[j] - off[j];
            if (!len || w + len > n) return -1;
            memcpy(out + w, in + off[j], len); w += len; off[j] += len;
        }
    }
    return 0;
}

/* The last lane of a full-width transpose contains the remaining suffix
 * of each row. Copy straight from lane cursors into the row being restored. */
static int tpq_inv_wide(const uint8_t *in, size_t n, uint8_t delim,
                        const size_t *base, uint8_t *out) {
    enum { LAST = 255 };
    if (memchr(in, '\n', base[LAST])) return 1;  /* a short row; use the generic inverse */
    const uint8_t *next[LAST], *end[LAST];
    for (int j = 0; j < LAST; j++) { next[j] = in + base[j]; end[j] = in + base[j + 1]; }
    const uint8_t *suffix = in + base[LAST], *limit = in + n;
    uint8_t *dst = out;
    while (suffix < limit) {
        for (int j = 0; j < LAST; j++) {
            const uint8_t *field = next[j];
            size_t available = (size_t)(end[j] - field);
            if (!available) return 1;
#if defined(KOLMO_USE_SSE2) || (defined(KOLMO_USE_NEON) && defined(__aarch64__))
            if ((size_t)(limit - field) >= 16 && (size_t)(out + n - dst) >= 16) {
#if defined(KOLMO_USE_SSE2)
                __m128i v = _mm_loadu_si128((const __m128i *)field);
                unsigned mask = _mm_movemask_epi8(_mm_cmpeq_epi8(v, _mm_set1_epi8((char)delim)));
                unsigned len = mask ? (unsigned)__builtin_ctz(mask) + 1 : 0;
#else
                uint8x16_t v = vld1q_u8(field);
                uint8x16_t eq = vceqq_u8(v, vdupq_n_u8(delim));
                uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
                unsigned len = mask ? ((unsigned)__builtin_ctzll(mask) >> 2) + 1 : 0;
#endif
                if (len && len <= available) {
                    memcpy(dst, &v, 16); next[j] += len; dst += len; continue;
                }
            }
#endif
            const uint8_t *sep = memchr(field, delim, available);
            if (!sep) return 1;
            size_t len = (size_t)(sep - field) + 1;
            memcpy(dst, field, len); dst += len; next[j] += len;
        }
        const uint8_t *nl = memchr(suffix, '\n', (size_t)(limit - suffix));
        if (!nl) return 1;
        size_t len = (size_t)(nl - suffix) + 1;
        memcpy(dst, suffix, len); dst += len; suffix += len;
    }
    return dst == out + n ? 0 : 1;
}

/* Column-major inverse for uniform blocks (every row has exactly k fields, block ends with a newline).
 * Rows are independent inside each column pass, so the CPU overlaps many field copies.
 * Returns 0 done, 1 not uniform (caller falls back), -1 corrupt. rowbuf needs 2 * (n / 2 + 2) entries. */
static int tpq_inv_cols(const uint8_t *in, size_t n, uint8_t delim, int k, const int64_t *colsz, uint8_t *out,
                        uint32_t *pos, uint32_t *rowbuf) {
    size_t base[256], pb[256], tot = 0, o = 0;
    for (int j = 0; j < k; j++) { if (colsz[j] < 0 || (size_t)colsz[j] > n - o) return -1; base[j] = o; o += colsz[j]; }
    if (o != n || n == 0 || colsz[k - 1] == 0 || in[n - 1] != '\n') return 1;
    if (k == 256) return tpq_inv_wide(in, n, delim, base, out);
    size_t R = 0;
    for (int j = 0; j < k; j++) {
        size_t c = scan_terms(in + base[j], colsz[j], j < k - 1 ? delim : '\n', pos + tot);
        if (j == 0) R = c; else if (c != R) return 1;
        if (!c || pos[tot + c - 1] != colsz[j] - 1) return 1;                 /* column must end on a terminator */
        pb[j] = tot; tot += c;
    }
    if (R > n / 2 + 1) return 1;
    uint32_t *rowoff = rowbuf, *cur = rowbuf + R + 1;
    memset(cur, 0, R * sizeof(uint32_t));
    for (int j = 0; j < k; j++) {
        const uint32_t *p = pos + pb[j]; const uint8_t *col = in + base[j]; uint32_t st = 0;
        uint8_t want = j < k - 1 ? delim : '\n';
        for (size_t r = 0; r < R; r++) {
            uint32_t q = p[r];
            if (col[q] != want) return 1;                                        /* short row: newline inside a field column */
            cur[r] += q + 1 - st; st = q + 1;
        }
    }
    uint32_t acc = 0;
    for (size_t r = 0; r < R; r++) { rowoff[r] = acc; acc += cur[r]; cur[r] = 0; }
    rowoff[R] = acc;
    if (acc != n) return -1;
    for (int j = 0; j < k; j++) {
        const uint32_t *p = pos + pb[j]; const uint8_t *col = in + base[j]; uint32_t st = 0;
        for (size_t r = 0; r < R; r++) {
            uint32_t q = p[r], len = q + 1 - st;
            uint8_t *d = out + rowoff[r] + cur[r];
            copy_field(d, col + st, len, d + 16 <= out + rowoff[r + 1] && base[j] + st + 16 <= n);
            cur[r] += len; st = q + 1;
        }
    }
    return 0;
}


static int tp_fieldcounts(const uint8_t *in, int64_t n, uint8_t delim, int quotes, int *cnt, int maxlines) {
    int j = 0, inq = 0, inbr = 0, maxc = 1 << 30, L = 0;
    for (int64_t i = 0; i < n && L < maxlines; i++) { uint8_t c = in[i]; if (c == '\n') cnt[L++] = j + 1; TP_STEP(c); }
    return L;
}
