/* ---------------------------------------------------------------- scientific binary: 16-bit images and fixed records
 * IMG16 (block flag 6): samples of a 16-bit image (either byte order, either byte offset) are predicted from their
 *   neighbours (left, average of left and up, or MED as in LOCO-I) with the row stride found by search; residuals are
 *   zigzagged and split into low/high byte planes.
 *   Block: be u8 | skip u8 | mode u8 | 0 | W u32 | nsamp u32 | skip bytes | lo plane | hi plane | tail bytes
 * REC (block flag 7): fixed-size records split into fields of 1/2/4/8 bytes (layout chosen by a recursive cost search
 *   at plan time); each field stored raw, as wrapping deltas, as dictionary tokens, or (doubles) as integer multiples
 *   of a discovered quantum plus an exact bit-level correction.
 *   Block: P u16 | nf u8 | 0 | nrec u32 | pre u32 | tail u32 | fields nf x {o u8, w u8, kind u8, 0, size u32}
 *          | pre bytes | tail bytes | field bodies */
#define IMG_MAXW 4096
#define REC_MAXP 64
enum { RK_RAW = 0, RK_DELTA = 1, RK_TOK = 2, RK_QUANT = 3 };

/* ------------------------------------------------ IMG16 */
static inline int pred16(const uint16_t *x, int64_t i, int64_t W, int mode) {
    int L = i >= 1 ? x[i - 1] : 0, U = i >= W ? x[i - W] : 0, UL = i >= W + 1 ? x[i - W - 1] : 0;
    if (mode == 0) return L;
    if (mode == 1) return (L + U) >> 1;
    int mx = L > U ? L : U, mn = L < U ? L : U;
    if (UL >= mx) return mn;
    if (UL <= mn) return mx;
    return L + U - UL;
}
static inline int pred16_in(const uint16_t *x, int64_t i, int64_t W, int mode) {      /* i > W: no bounds checks */
    int L = x[i - 1], U = x[i - W], UL = x[i - W - 1];
    if (mode == 0) return L;
    if (mode == 1) return (L + U) >> 1;
    int mx = L > U ? L : U, mn = L < U ? L : U;
    return UL >= mx ? mn : UL <= mn ? mx : L + U - UL;
}
static void img16_fwd(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi) {
    for (int64_t i = 0; i < n; i++) {
        int p = i > W ? pred16_in(a, i, W, mode) : pred16(a, i, W, mode);
        uint16_t r = (uint16_t)(a[i] - p); uint16_t z = (uint16_t)((uint16_t)(r << 1) ^ (uint16_t)-(uint16_t)(r >> 15));
        lo[i] = (uint8_t)z; hi[i] = (uint8_t)(z >> 8);
    }
}
static void img16_inv(const uint8_t *lo, const uint8_t *hi, int64_t n, int64_t W, int mode, uint16_t *out) {
    int64_t i = 0, head = n < W + 1 ? n : W + 1;
    for (; i < head; i++) { uint16_t z = (uint16_t)(lo[i] | hi[i] << 8); int16_t r = (int16_t)((z >> 1) ^ (uint16_t)-(int16_t)(z & 1)); out[i] = (uint16_t)(pred16(out, i, W, mode) + r); }
    if (i >= n) return;
    /* one loop per predictor, the left neighbour kept in a register (the serial chain is the decode speed) */
    uint16_t L = out[i - 1];
#define IMG_R(i) ((uint16_t)(((uint16_t)(lo[i] | hi[i] << 8) >> 1) ^ (uint16_t)-(uint16_t)(lo[i] & 1)))
    if (mode == 0) for (; i < n; i++) { L = (uint16_t)(L + IMG_R(i)); out[i] = L; }
    else if (mode == 1) for (; i < n; i++) { L = (uint16_t)(((L + out[i - W]) >> 1) + IMG_R(i)); out[i] = L; }
    else for (; i < n; i++) {
        int U = out[i - W], UL = out[i - W - 1], l = L;
        int mx = l > U ? l : U, mn = l < U ? l : U;
        int p = UL >= mx ? mn : UL <= mn ? mx : l + U - UL;
        L = (uint16_t)(p + IMG_R(i)); out[i] = L;
    }
#undef IMG_R
}
static double h0(const uint8_t *b, size_t n) {
    size_t c[256] = {0}; for (size_t i = 0; i < n; i++) c[b[i]]++;
    double t = 0; for (int k = 0; k < 256; k++) if (c[k]) t -= c[k] * log2((double)c[k] / n);
    return t / 8;
}
typedef struct { int be, off, mode; int64_t W; } img_plan_t;

/* returns 1 and fills P when the data looks like a 16-bit image that prediction helps */
static int img16_detect(const uint8_t *s, size_t n, img_plan_t *P) {
    if (n < (1u << 18)) return 0;
    int64_t m = 1 << 19; if ((int64_t)(n / 2) - 2 < m) m = (int64_t)(n / 2) - 2;
    uint16_t *a = malloc(2 * (size_t)m); uint8_t *lo = malloc(m), *hi = malloc(m);
    double best = 1e300; int found = 0;
    size_t mid = n / 2 & ~(size_t)1;
    size_t st = mid > (size_t)m ? mid - (size_t)m : 0; st &= ~(size_t)1;
    for (int be = 0; be < 2; be++) for (int off = 0; off < 2; off++) {
        const uint8_t *q = s + st + off;
        for (int64_t i = 0; i < m; i++) a[i] = be ? (uint16_t)(q[2 * i] << 8 | q[2 * i + 1]) : (uint16_t)(q[2 * i] | q[2 * i + 1] << 8);
        int64_t big = 0; for (int64_t i = m / 4; i < m / 4 + 65536 && i < m; i++) big += a[i] >> 13 != 0;
        if (big > 1000) continue;
        /* row stride: lag with the smallest mean absolute difference, on a 64K window */
        int64_t w0 = m / 2, wl = 65536 < m / 2 ? 65536 : m / 2; double lag1 = 0;
        for (int64_t i = 1; i < wl; i++) lag1 += abs((int)a[w0 + i] - (int)a[w0 + i - 1]);
        lag1 /= wl; int64_t W = 1; double wv = lag1;
        /* coarse score for every lag on a short contiguous run (vectorizes), then the exact score of the best few;
         * the exact rule is the one of a full scan: the smallest score below 1.5x lag 1, earliest lag on ties */
        {   enum { NC = 24, CW = 4096 };
            int64_t cw[NC]; uint64_t cs[NC]; int nc = 0;
            for (int64_t w = 16; w <= IMG_MAXW && w < wl / 2; w++) {
                uint64_t sc = 0; const uint16_t *x = a + w0 + w, *y = a + w0;
                int64_t lim = wl - w < CW ? wl - w : CW;
                for (int64_t i = 0; i < lim; i++) sc += (uint64_t)abs((int)x[i] - (int)y[i]);
                sc = sc * CW / (uint64_t)(lim > 0 ? lim : 1);
                if (nc < NC) { cw[nc] = w; cs[nc] = sc; nc++; }
                else { int mx = 0; for (int q = 1; q < NC; q++) if (cs[q] > cs[mx]) mx = q; if (sc < cs[mx]) { cw[mx] = w; cs[mx] = sc; } }
            }
            for (int q = 1; q < nc; q++) for (int r = q; r > 0 && cw[r] < cw[r - 1]; r--) { int64_t t = cw[r]; cw[r] = cw[r - 1]; cw[r - 1] = t; uint64_t u = cs[r]; cs[r] = cs[r - 1]; cs[r - 1] = u; }
            for (int q = 0; q < nc; q++) {
                int64_t w = cw[q];
                double v = 0; for (int64_t i = w; i < wl; i += 4) v += abs((int)a[w0 + i] - (int)a[w0 + i - w]);
                v /= (double)((wl - w + 3) / 4);
                if (v < wv * 1.5 && (W == 1 || v < wv)) { W = w; wv = v; }
            }
        }
        for (int mode = 0; mode < (W > 1 ? 3 : 1); mode++) {
            img16_fwd(a, m, W, mode, lo, hi);
            double c = h0(lo, m) + h0(hi, m);
            if (c < best) { best = c; P->be = be; P->off = off; P->mode = mode; P->W = W; found = 1; }
        }
    }
    if (found) {
        uint8_t *ev = malloc(m), *od = malloc(m);
        for (int64_t i = 0; i < m; i++) { ev[i] = s[st + 2 * i]; od[i] = s[st + 2 * i + 1]; }
        double raw = h0(ev, m) + h0(od, m);
        if (best > 0.9 * raw) found = 0;
        free(ev); free(od);
    }
    free(a); free(lo); free(hi);
    return found;
}
/* block encode: skip = bytes before the first sample in this block */
static int64_t img16_encode(const uint8_t *in, size_t len, size_t blockstart, const img_plan_t *P, uint8_t *dst) {
    size_t skip = (blockstart & 1) != (size_t)P->off ? 1 : 0; if (skip > len) skip = len;
    size_t ns = (len - skip) / 2, tail = len - skip - 2 * ns;
    uint16_t *a = malloc(2 * ns + 2); if (!a) return -1;
    const uint8_t *q = in + skip;
    for (size_t i = 0; i < ns; i++) a[i] = P->be ? (uint16_t)(q[2 * i] << 8 | q[2 * i + 1]) : (uint16_t)(q[2 * i] | q[2 * i + 1] << 8);
    uint8_t *o = dst; o[0] = (uint8_t)P->be; o[1] = (uint8_t)skip; o[2] = (uint8_t)P->mode; o[3] = 0;
    uint32_t W = (uint32_t)P->W, N = (uint32_t)ns; memcpy(o + 4, &W, 4); memcpy(o + 8, &N, 4); o += 12;
    memcpy(o, in, skip); o += skip;
    img16_fwd(a, (int64_t)ns, P->W, P->mode, o, o + ns); o += 2 * ns;
    memcpy(o, in + skip + 2 * ns, tail); o += tail;
    free(a);
    return (int64_t)(o - dst);
}
static int img16_decode(const uint8_t *in, size_t tn, uint8_t *out, size_t len) {
    if (tn < 12) return -1;
    int be = in[0], skip = in[1], mode = in[2]; uint32_t W, N; memcpy(&W, in + 4, 4); memcpy(&N, in + 8, 4);
    if (be > 1 || skip > 1 || mode > 2 || W < 1 || W > IMG_MAXW || (size_t)skip + 2ull * N > len) return -1;
    size_t tail = len - skip - 2ull * N;
    if (12 + skip + 2ull * N + tail != tn) return -1;
    const uint8_t *p = in + 12;
    memcpy(out, p, skip); p += skip;
    uint16_t *a = malloc(2ull * N + 2); if (!a) return -1;
    img16_inv(p, p + N, N, W, mode, a); p += 2ull * N;
    uint8_t *o = out + skip;
    if (be) for (uint32_t i = 0; i < N; i++) { o[2 * i] = (uint8_t)(a[i] >> 8); o[2 * i + 1] = (uint8_t)a[i]; }
    else memcpy(o, a, 2ull * N);                                   /* little-endian host */
    memcpy(o + 2ull * N, p, tail);
    free(a);
    return 0;
}

/* ------------------------------------------------ REC */
typedef struct { uint8_t o, w, kind; } rfield_t;
typedef struct { int P, h, nf; rfield_t f[REC_MAXP]; double quant[REC_MAXP]; } rec_plan_t;

static inline uint64_t ldwn(const uint8_t *p, int w) {               /* little-endian host; fixed-size copies inline */
    switch (w) {
        case 1: return p[0];
        case 2: { uint16_t v; memcpy(&v, p, 2); return v; }
        case 4: { uint32_t v; memcpy(&v, p, 4); return v; }
        case 8: { uint64_t v; memcpy(&v, p, 8); return v; }
        default: { uint64_t v = 0; memcpy(&v, p, (size_t)w); return v; }
    }
}
static inline void stwn(uint8_t *p, uint64_t v, int w) {
    switch (w) {
        case 1: p[0] = (uint8_t)v; return;
        case 2: { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); return; }
        case 4: { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); return; }
        case 8: memcpy(p, &v, 8); return;
        default: memcpy(p, &v, (size_t)w);
    }
}
/* field values of n records (stride P, offset o, width w) */
static void ld_field(uint64_t *v, const uint8_t *A, size_t n, int P, int o, int w) {
    const uint8_t *p = A + o;
    switch (w) {
        case 1: for (size_t i = 0; i < n; i++) v[i] = p[i * P]; break;
        case 2: for (size_t i = 0; i < n; i++) { uint16_t x; memcpy(&x, p + i * P, 2); v[i] = x; } break;
        case 4: for (size_t i = 0; i < n; i++) { uint32_t x; memcpy(&x, p + i * P, 4); v[i] = x; } break;
        case 8: for (size_t i = 0; i < n; i++) memcpy(&v[i], p + i * P, 8); break;
        default: for (size_t i = 0; i < n; i++) v[i] = ldwn(p + i * P, w);
    }
}
static void planes_put(uint8_t *o, const uint64_t *v, size_t n, int w) { for (int b = 0; b < w; b++) for (size_t i = 0; i < n; i++) o[(size_t)b * n + i] = (uint8_t)(v[i] >> (8 * b)); }
static void planes_get(const uint8_t *p, uint64_t *v, size_t n, int w) {
    const uint8_t *p0 = p, *p1 = p + n, *p2 = p1 + n, *p3 = p2 + n;
    switch (w) {
        case 1: for (size_t i = 0; i < n; i++) v[i] = p0[i]; return;
        case 2: for (size_t i = 0; i < n; i++) v[i] = p0[i] | (uint64_t)p1[i] << 8; return;
        case 4: for (size_t i = 0; i < n; i++) v[i] = p0[i] | (uint64_t)p1[i] << 8 | (uint64_t)p2[i] << 16 | (uint64_t)p3[i] << 24; return;
        default:
            for (size_t i = 0; i < n; i++) v[i] = 0;
            for (int b = 0; b < w; b++) for (size_t i = 0; i < n; i++) v[i] |= (uint64_t)p[(size_t)b * n + i] << (8 * b);
    }
}
static int zw(uint64_t mx) { int w = 1; while (w < 8 && (mx >> (8 * w))) w++; return w; }
static int cmp_u64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }
/* ascending sort of 64-bit keys: LSD radix by bytes, skipping bytes that are the same in every key. Same result as
 * qsort with cmp_u64 on any platform (the libc sorts differ, the sorted keys do not). */
static void sort_u64(uint64_t *a, size_t n) {
    if (n < 64) { qsort(a, n, 8, cmp_u64); return; }
    uint64_t *t = malloc(8 * n); if (!t) { qsort(a, n, 8, cmp_u64); return; }
    size_t (*H)[256] = calloc(8, sizeof *H); if (!H) { free(t); qsort(a, n, 8, cmp_u64); return; }
    for (size_t i = 0; i < n; i++) { uint64_t x = a[i]; for (int b = 0; b < 8; b++) H[b][(x >> (8 * b)) & 255]++; }
    uint64_t *src = a, *dst = t;
    for (int b = 0; b < 8; b++) {
        size_t *h = H[b]; int one = 0; for (int c = 0; c < 256; c++) if (h[c] == n) { one = 1; break; }
        if (one) continue;
        size_t sum = 0; for (int c = 0; c < 256; c++) { size_t v = h[c]; h[c] = sum; sum += v; }
        for (size_t i = 0; i < n; i++) { uint64_t x = src[i]; dst[h[(x >> (8 * b)) & 255]++] = x; }
        uint64_t *q = src; src = dst; dst = q;
    }
    if (src != a) memcpy(a, src, 8 * n);
    free(t); free(H);
}

/* patched planes: width chosen to minimise planes + exceptions (index u32, value u64); outliers stored aside */
static uint8_t *pfor_put(uint8_t *o, const uint64_t *z, size_t n) {
    size_t cnt[9] = {0};                                   /* values needing more than w bytes, per w */
    for (size_t i = 0; i < n; i++) { int b = z[i] ? (64 - __builtin_clzll(z[i]) + 7) / 8 : 0; for (int w = 1; w < b; w++) cnt[w]++; }
    int bw = 8; size_t bc = n * 8;
    for (int w = 1; w <= 8; w++) { size_t c = n * w + cnt[w] * 12; if (c < bc) { bc = c; bw = w; } }
    uint64_t lim = bw == 8 ? ~0ull : (1ull << (8 * bw)) - 1; uint32_t ne = (uint32_t)cnt[bw];
    o[0] = (uint8_t)bw; memcpy(o + 1, &ne, 4); o += 5;
    for (int b = 0; b < bw; b++) for (size_t i = 0; i < n; i++) o[(size_t)b * n + i] = z[i] > lim ? 0 : (uint8_t)(z[i] >> (8 * b));
    o += (size_t)bw * n;
    for (size_t i = 0; i < n; i++) if (z[i] > lim) { uint32_t ix = (uint32_t)i; memcpy(o, &ix, 4); memcpy(o + 4, &z[i], 8); o += 12; }
    return o;
}
static const uint8_t *pfor_get(const uint8_t *p, const uint8_t *end, uint64_t *z, size_t n) {
    if (end - p < 5) return NULL;
    int w = p[0]; uint32_t ne; memcpy(&ne, p + 1, 4); p += 5;
    if (w < 1 || w > 8 || (uint64_t)(end - p) < (uint64_t)n * w + 12ull * ne) return NULL;
    planes_get(p, z, n, w); p += (size_t)n * w;
    for (uint32_t k = 0; k < ne; k++) { uint32_t ix; memcpy(&ix, p, 4); if (ix >= n) return NULL; memcpy(&z[ix], p + 4, 8); p += 12; }
    return p;
}

/* doubles that are integer multiples of an unknown constant c (to within a few ulps) -> c, else 0 */
static double find_quantum(const double *x, size_t n) {
    for (size_t i = 0; i < n; i++) if (!isfinite(x[i])) return 0;
    double *u = malloc(sizeof(double) * (n + 1)), *dif = malloc(sizeof(double) * (n + 1)); size_t k = 0, nd = 0;
    for (size_t i = 0; i < n; i++) u[i] = fabs(x[i]);
    sort_u64((uint64_t *)u, n);                     /* finite and >= 0: bit patterns sort as the values */
    for (size_t i = 0; i < n; i++) if (!k || u[i] != u[k - 1]) u[k++] = u[i];
    double c = 0;
    if (k < 17) goto out;
    double xm = u[k - 1], sp = nextafter(xm, INFINITY) - xm;
    for (size_t i = 1; i < k; i++) { double d = u[i] - u[i - 1]; if (d > sp * 256) dif[nd++] = d; }
    if (nd < 16) goto out;
    double d0 = dif[0]; for (size_t i = 1; i < nd; i++) if (dif[i] < d0) d0 = dif[i];
    for (int m = 1; m <= 64; m++) {
        double cc = d0 / m;
        if (xm / cc > 4503599627370496.0) break;
        for (double t = 10.0; t * cc < xm; t *= 10) {
            double tgt = t * cc; size_t lo = 0, hi = k;
            while (lo < hi) { size_t md = (lo + hi) / 2; if (u[md] < tgt) lo = md + 1; else hi = md; }
            size_t j = lo < k ? lo : k - 1;
            double qi = rint(u[j] / cc); if (qi != 0) cc = u[j] / qi;
        }
        double qi = rint(xm / cc); if (qi != 0) cc = xm / qi;
        size_t good = 0; for (size_t i = 0; i < n; i++) { double q = x[i] / cc; good += fabs(q - rint(q)) < 1e-3; }
        if (good > 0.999 * n) { c = cc; break; }
    }
out:
    free(u); free(dif); return c;
}

/* encodes one field of n records (stride P at offset o); returns body size or -1 (kind not applicable) */
static int64_t rec_field_enc(const uint8_t *A, size_t n, int P, int o, int w, int kind, double c, uint8_t *dst) {
    uint64_t *v = malloc(8 * (n + 1)); if (!v) return -1;
    ld_field(v, A, n, P, o, w);
    uint64_t mask = w == 8 ? ~0ull : (1ull << (8 * w)) - 1; int64_t r = -1; uint8_t *o0 = dst;
    if (kind == RK_RAW) { planes_put(dst, v, n, w); r = (int64_t)n * w; }
    else if (kind == RK_DELTA) {
        uint64_t prev = 0; for (size_t i = 0; i < n; i++) { uint64_t x = v[i]; v[i] = (x - prev) & mask; prev = x; }
        planes_put(dst, v, n, w); r = (int64_t)n * w;
    } else if (kind == RK_TOK) {
        uint64_t *u = malloc(8 * (n + 1)); memcpy(u, v, 8 * n); sort_u64(u, n);
        size_t k = 0; for (size_t i = 0; i < n; i++) if (!k || u[i] != u[k - 1]) u[k++] = u[i];
        if (k > 65536 * 256ull || k > n) { free(u); free(v); return -1; }
        int iw = k <= 256 ? 1 : k <= 65536 ? 2 : 4;
        uint32_t k32 = (uint32_t)k; memcpy(dst, &k32, 4); dst[4] = (uint8_t)iw; dst += 5;
        uint64_t *du = malloc(8 * (k + 1)); uint64_t prev = 0; for (size_t i = 0; i < k; i++) { du[i] = (u[i] - prev) & mask; prev = u[i]; }
        planes_put(dst, du, k, w); dst += k * w; free(du);
        {   /* value -> index in the sorted dictionary: open-addressing table (binary search per value is far slower) */
            size_t tc = 16; while (tc < 2 * k) tc <<= 1;
            uint64_t *tk = malloc(8 * tc); uint32_t *ti = malloc(4 * tc);
            if (!tk || !ti) { free(tk); free(ti); free(u); free(v); return -1; }
            memset(ti, 0xFF, 4 * tc);
            int sh = 64 - __builtin_ctzll(tc);
            for (size_t i = 0; i < k; i++) { size_t h = (size_t)((u[i] * 0x9E3779B97F4A7C15ull) >> sh); while (ti[h] != 0xFFFFFFFFu) h = (h + 1) & (tc - 1); tk[h] = u[i]; ti[h] = (uint32_t)i; }
            for (size_t i = 0; i < n; i++) { size_t h = (size_t)((v[i] * 0x9E3779B97F4A7C15ull) >> sh); while (tk[h] != v[i] || ti[h] == 0xFFFFFFFFu) h = (h + 1) & (tc - 1); v[i] = ti[h]; }
            free(tk); free(ti);
        }
        planes_put(dst, v, n, iw); dst += n * iw; free(u);
        r = (int64_t)(dst - o0);
    } else if (kind == RK_QUANT && w == 8 && c > 0) {
        int64_t *q = malloc(8 * (n + 1)); int ok = 1;
        for (size_t i = 0; i < n && ok; i++) {
            double x; memcpy(&x, &v[i], 8); double t = x / c;
            q[i] = fabs(t) < 4.0e18 ? (int64_t)rint(t) : 0;          /* NaN, inf, huge: q = 0, the correction holds the bits */
            double y = (double)q[i] * c; uint64_t yb; memcpy(&yb, &y, 8);
            v[i] = zzg((int64_t)(v[i] - yb));                      /* exact bit-level correction */
        }
        if (ok) {
            memcpy(dst, &c, 8); dst += 8;
            uint64_t m0 = 0, m1 = 0, c0 = 0, c1 = 0; int64_t prev = 0;
            for (size_t i = 0; i < n; i++) { uint64_t a0 = zzg(q[i]), a1 = zzg((int64_t)((uint64_t)q[i] - (uint64_t)prev)); prev = q[i]; c0 += bitlen(a0); c1 += bitlen(a1); if (a0 > m0) m0 = a0; if (a1 > m1) m1 = a1; }
            int dm = c1 < c0, wq = zw(dm ? m1 : m0); uint64_t *z = malloc(8 * (n + 1)); prev = 0;
            for (size_t i = 0; i < n; i++) { z[i] = dm ? zzg((int64_t)((uint64_t)q[i] - (uint64_t)prev)) : zzg(q[i]); prev = q[i]; }
            (void)wq; dst[0] = (uint8_t)dm; dst++; dst = pfor_put(dst, z, n); free(z);
            dst = pfor_put(dst, v, n);
            r = (int64_t)(dst - o0);
        }
        free(q);
    }
    free(v); return r;
}
static size_t rec_field_bound(size_t n, int w) { return 64 + n * 8 * 2 + n * w + 16; }

static int rec_field_dec(const uint8_t *p, size_t sz, size_t n, int P, int o, int w, int kind, uint8_t *A) {
    uint64_t *v = malloc(8 * (n + 1)); if (!v) return -1;
    uint64_t mask = w == 8 ? ~0ull : (1ull << (8 * w)) - 1; int ret = -1;
    if (kind == RK_RAW || kind == RK_DELTA) {
        if (sz != n * w) goto out;
        planes_get(p, v, n, w);
        if (kind == RK_DELTA) { uint64_t prev = 0; for (size_t i = 0; i < n; i++) { prev = (prev + v[i]) & mask; v[i] = prev; } }
    } else if (kind == RK_TOK) {
        if (sz < 5) goto out;
        uint32_t k; memcpy(&k, p, 4); int iw = p[4];
        if ((iw != 1 && iw != 2 && iw != 4) || k == 0 || 5 + (uint64_t)k * w + (uint64_t)n * iw != sz) goto out;
        uint64_t *u = malloc(8 * ((size_t)k + 1)); if (!u) goto out;
        planes_get(p + 5, u, k, w); uint64_t prev = 0; for (uint32_t i = 0; i < k; i++) { prev = (prev + u[i]) & mask; u[i] = prev; }
        planes_get(p + 5 + (size_t)k * w, v, n, iw);
        for (size_t i = 0; i < n; i++) { if (v[i] >= k) { free(u); goto out; } v[i] = u[v[i]]; }
        free(u);
    } else if (kind == RK_QUANT && w == 8) {
        if (sz < 19) goto out;
        double c; memcpy(&c, p, 8); int dm = p[8];
        if (dm > 1 || !(c > 0) || !isfinite(c)) goto out;
        int64_t *q = malloc(8 * (n + 1)); if (!q) goto out;
        const uint8_t *pe = p + sz, *pr = pfor_get(p + 9, pe, (uint64_t *)q, n);
        if (!pr) { free(q); goto out; }
        int64_t prev = 0; for (size_t i = 0; i < n; i++) { int64_t d = unzz((uint64_t)q[i]); if (dm) { prev = (int64_t)((uint64_t)prev + (uint64_t)d); q[i] = prev; } else q[i] = d; }
        if (pfor_get(pr, pe, v, n) != pe) { free(q); goto out; }
        for (size_t i = 0; i < n; i++) { double y = (double)q[i] * c; uint64_t yb; memcpy(&yb, &y, 8); v[i] = yb + (uint64_t)unzz(v[i]); }
        free(q);
    } else goto out;
    for (size_t i = 0; i < n; i++) stwn(A + i * P + o, v[i], w);
    ret = 0;
out:
    free(v); return ret;
}

/* record period by self-similarity at lag p (smallest divisor with a similar score) */
static int rec_period(const uint8_t *s, size_t n) {
    if (n < (1u << 16)) return 0;
    size_t m = n - REC_MAXP < (1u << 18) ? n - REC_MAXP : (1u << 18);
    int sc[REC_MAXP + 1] = {0}, best = 0, bp = 0;
    for (int p = 2; p <= REC_MAXP; p++) { int c = 0; for (size_t i = 0; i < m; i++) c += s[i + p] == s[i]; sc[p] = c; if (c > best) { best = c; bp = p; } }
    int tmp[REC_MAXP]; int nt = 0; for (int p = 2; p <= REC_MAXP; p++) tmp[nt++] = sc[p];
    for (int i = 1; i < nt; i++) for (int j = i; j > 0 && tmp[j - 1] > tmp[j]; j--) { int t = tmp[j]; tmp[j] = tmp[j - 1]; tmp[j - 1] = t; }
    int med = tmp[nt / 2];
    if (best < 2 * med) return 0;
    for (int q = 2; q <= REC_MAXP; q++) if (bp % q == 0 && sc[q] > 0.9 * best) return q;
    return bp;
}

static int zcost_level = 3;
static size_t zcost(ZSTD_CCtx *zc, const uint8_t *b, size_t n, uint8_t *tmp, size_t cap) { size_t r = ZSTD_compressCCtx(zc, tmp, cap, b, n, zcost_level); return ZSTD_isError(r) ? n : r; }

typedef struct { const uint8_t *A; size_t n; int P; ZSTD_CCtx *zc; uint8_t *eb, *zb; size_t ecap, zcap; } rsearch_t;
static size_t rec_best(rsearch_t *S, int o, int w, int *kind, double *qc) {
    size_t best = (size_t)-1; *kind = RK_RAW; *qc = 0;
    int kinds[4] = {RK_RAW, RK_DELTA, -1, -1}, nk = 2;
    {   /* tokens only for low-cardinality fields */
        size_t m = S->n < 32768 ? S->n : 32768; uint64_t *u = malloc(8 * (m + 1));
        ld_field(u, S->A, m, S->P, o, w);
        sort_u64(u, m); size_t k = 0; for (size_t i = 0; i < m; i++) if (!k || u[i] != u[k - 1]) u[k++] = u[i];
        free(u); if (k < m / 4) kinds[nk++] = RK_TOK;
    }
    double c = 0;
    if (w == 8) {
        size_t m = S->n < 131072 ? S->n : 131072; double *x = malloc(sizeof(double) * (m + 1));
        for (size_t i = 0; i < m; i++) memcpy(&x[i], S->A + i * S->P + o, 8);
        c = find_quantum(x, m); free(x); if (c > 0) kinds[nk++] = RK_QUANT;
    }
    for (int q = 0; q < nk; q++) {
        int64_t e = rec_field_enc(S->A, S->n, S->P, o, w, kinds[q], c, S->eb);
        if (e < 0) continue;
        size_t z = zcost(S->zc, S->eb, (size_t)e, S->zb, S->zcap);
        if (z < best) { best = z; *kind = kinds[q]; *qc = kinds[q] == RK_QUANT ? c : 0; }
    }
    return best;
}
static size_t rec_layout(rsearch_t *S, int o, int w, rfield_t *out, double *qout, int *nf) {
    int k; double qc; size_t c = rec_best(S, o, w, &k, &qc);
    if (w == 1) { out[*nf] = (rfield_t){(uint8_t)o, 1, (uint8_t)k}; qout[*nf] = qc; (*nf)++; return c; }
    rfield_t t1[REC_MAXP], t2[REC_MAXP]; double q1[REC_MAXP], q2[REC_MAXP]; int n1 = 0, n2 = 0, h = w / 2;
    size_t c1 = rec_layout(S, o, h, t1, q1, &n1), c2 = rec_layout(S, o + h, h, t2, q2, &n2);
    if (c <= c1 + c2) { out[*nf] = (rfield_t){(uint8_t)o, (uint8_t)w, (uint8_t)k}; qout[*nf] = qc; (*nf)++; return c; }
    for (int i = 0; i < n1; i++) { out[*nf] = t1[i]; qout[*nf] = q1[i]; (*nf)++; }
    for (int i = 0; i < n2; i++) { out[*nf] = t2[i]; qout[*nf] = q2[i]; (*nf)++; }
    return c1 + c2;
}
/* plan: period, header length (0 or one record), field layout; returns estimated cost or 0 */
static size_t rec_detect(const uint8_t *s, size_t n, int effort, rec_plan_t *R) {
    int P = rec_period(s, n); if (!P) return 0;
    size_t bestc = (size_t)-1; ZSTD_CCtx *zc = ZSTD_createCCtx();
    for (int h = 0; h <= P; h += P) {
        size_t cap_m = effort ? 32768 : 4096, nrec = (n - h) / P, m = nrec < cap_m ? nrec : cap_m;
        if (m < 1024) continue;
        zcost_level = effort ? 3 : 1;
        rsearch_t S = {s + h, m, P, zc, NULL, NULL, 0, 0};
        S.ecap = rec_field_bound(m, 8); S.eb = malloc(S.ecap); S.zcap = ZSTD_compressBound(S.ecap); S.zb = malloc(S.zcap);
        rec_plan_t T = {P, h, 0, {{0}}, {0}}; size_t tot = 0; int o = 0;
        while (o < P) { int w = 8; while (o % w || o + w > P) w /= 2; tot += rec_layout(&S, o, w, T.f, T.quant, &T.nf); o += w; }
        if (tot < bestc) { bestc = tot; *R = T; }
        free(S.eb); free(S.zb); zcost_level = 3;
    }
    ZSTD_freeCCtx(zc);
    return bestc == (size_t)-1 ? 0 : bestc;
}
static int64_t rec_encode(const uint8_t *in, size_t len, size_t blockstart, const rec_plan_t *R, uint8_t *dst, size_t cap) {
    size_t pre = blockstart == 0 ? (size_t)R->h : 0; if (pre > len) pre = len;
    size_t nrec = (len - pre) / R->P, tail = len - pre - nrec * R->P;
    uint8_t *o = dst; uint16_t P16 = (uint16_t)R->P; memcpy(o, &P16, 2); o[2] = (uint8_t)R->nf; o[3] = 0;
    uint32_t h3[3] = {(uint32_t)nrec, (uint32_t)pre, (uint32_t)tail}; memcpy(o + 4, h3, 12); o += 16;
    uint8_t *fh = o; o += 8 * R->nf;
    memcpy(o, in, pre); o += pre; memcpy(o, in + pre + nrec * R->P, tail); o += tail;
    for (int f = 0; f < R->nf; f++) {
        const rfield_t *F = &R->f[f]; int kind = F->kind;
        uint8_t *tmp = malloc(rec_field_bound(nrec, F->w)); if (!tmp) return -1;
        int64_t e = rec_field_enc(in + pre, nrec, R->P, F->o, F->w, kind, R->quant[f], tmp);
        if (e < 0 || e > (int64_t)nrec * F->w + 64) { kind = RK_RAW; e = rec_field_enc(in + pre, nrec, R->P, F->o, F->w, kind, 0, tmp); }
        if ((size_t)(o - dst) + (size_t)e > cap) { free(tmp); return -1; }
        memcpy(o, tmp, (size_t)e); free(tmp);
        uint32_t sz = (uint32_t)e; fh[8 * f] = F->o; fh[8 * f + 1] = F->w; fh[8 * f + 2] = (uint8_t)kind; fh[8 * f + 3] = 0; memcpy(fh + 8 * f + 4, &sz, 4);
        o += e;
    }
    return (int64_t)(o - dst);
}
static int rec_decode(const uint8_t *in, size_t tn, uint8_t *out, size_t len) {
    if (tn < 16) return -1;
    uint16_t P; memcpy(&P, in, 2); int nf = in[2]; uint32_t h3[3]; memcpy(h3, in + 4, 12);
    size_t nrec = h3[0], pre = h3[1], tail = h3[2];
    if (P < 2 || P > REC_MAXP || nf < 1 || nf > REC_MAXP || pre > len || tail > len || (uint64_t)nrec * P + pre + tail != len) return -1;
    if (16 + 8ull * nf + pre + tail > tn) return -1;
    const uint8_t *fh = in + 16, *p = fh + 8 * nf, *end = in + tn;
    memcpy(out, p, pre); p += pre;
    memcpy(out + pre + nrec * P, p, tail); p += tail;
    uint8_t cover[REC_MAXP] = {0};
    for (int f = 0; f < nf; f++) {
        int o = fh[8 * f], w = fh[8 * f + 1], kind = fh[8 * f + 2]; uint32_t sz; memcpy(&sz, fh + 8 * f + 4, 4);
        if ((w != 1 && w != 2 && w != 4 && w != 8) || o + w > P || sz > (size_t)(end - p)) return -1;
        for (int b = o; b < o + w; b++) { if (cover[b]) return -1; cover[b] = 1; }
        if (rec_field_dec(p, sz, nrec, P, o, w, kind, out + pre)) return -1;
        p += sz;
    }
    for (int b = 0; b < P; b++) if (!cover[b]) return -1;
    return p == end ? 0 : -1;
}

/* test hooks: whole-buffer encode with a detected plan (type byte 0 = records, 1 = image) */
int64_t sci_encode_pub(const uint8_t *in, size_t n, int image, uint8_t *dst, size_t cap) {
    if (image) { img_plan_t P; if (!img16_detect(in, n, &P)) return -1; dst[0] = 1; int64_t r = img16_encode(in, n, 0, &P, dst + 1); return r < 0 ? r : r + 1; }
    rec_plan_t R; if (!rec_detect(in, n, 1, &R)) return -1; dst[0] = 0; int64_t r = rec_encode(in, n, 0, &R, dst + 1, cap - 1); return r < 0 ? r : r + 1;
}
int sci_decode_pub(const uint8_t *in, size_t tn, uint8_t *out, size_t n) {
    if (tn < 1) return -1;
    return in[0] == 1 ? img16_decode(in + 1, tn - 1, out, n) : in[0] == 0 ? rec_decode(in + 1, tn - 1, out, n) : -1;
}
