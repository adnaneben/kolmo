/* kbin: a numeric-array coder for raw little-endian arrays and .npy files (frame magic "KBN1").
 *
 * Method, after the published description of pcodec (Loncaric et al., arXiv 2502.06112), written from the
 * paper and not from pcodec's code:
 *   1. each value is mapped to an unsigned ordinal that keeps its order (signed: flip the sign bit;
 *      float: flip all bits of negatives, set the sign bit of positives);
 *   2. a per-chunk mode splits the ordinals into one or two integer latent streams
 *      (classic; integer multiplier x = m*q + r; float divisor x ~ k / D plus an ordinal adjustment;
 *      float quantisation: high bits and low bits);
 *   3. per latent stream a delta encoding is chosen (consecutive differences of order 0 to 7, or an
 *      integer linear predictor, our addition);
 *   4. the residuals are binned: a few ranges [lower, lower + 2^bits) chosen by dynamic programming over
 *      quantile groups; each value is coded as its bin index, entropy coded with a static order-0 rANS
 *      (four interleaved states, one table per stream), plus the offset inside the bin as raw bits.
 * The decoder uses integer arithmetic only, except one IEEE division per value in the float-divisor mode.
 * Chunks are independent and code in parallel (pthreads). See KBIN.md for the frame layout and results.
 */
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "kbin assumes a little-endian host"
#endif

typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;

#define TOP64 0x8000000000000000ull
#define MAX_LEVEL 12
#define MAX_BINS (2u << MAX_LEVEL)
#define ANS_MAX_LOG 15
#define ANS_L (1u << 16)
#define MAX_ORDER 7
#define MAX_LPC 32
#define BATCH 256
#define MAX_LANES 8
/* interleaved rANS states (lanes) for a stream of n symbols: one for short streams, whose four-byte
 * final states would otherwise cost more than the speed is worth. With the context extension, the
 * context of symbol i is the bin of symbol i - lanes, the previous symbol of the same lane, so that
 * the lanes decode independently. */
static int ans_lanes(size_t n) { return n >= 65536 ? 4 : 1; }

static int g_debug;
static double g_tm[8];
double now_s(void);
#define TM_START double tm0_ = g_debug ? now_s() : 0
#define TM_END(k) do { if (g_debug) g_tm[k] += now_s() - tm0_; } while (0)

enum { T_I16, T_U16, T_I32, T_U32, T_I64, T_U64, T_F32, T_F64, T_COUNT };
static const char *const tname[T_COUNT] = {"i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"};
static const char *const tdescr[T_COUNT] = {"<i2", "<u2", "<i4", "<u4", "<i8", "<u8", "<f4", "<f8"};
static const int twidth[T_COUNT] = {2, 2, 4, 4, 8, 8, 4, 8};
static int t_signed(int t) { return t == T_I16 || t == T_I32 || t == T_I64; }
static int t_float(int t) { return t == T_F32 || t == T_F64; }

enum { M_CLASSIC = 0, M_INTMULT = 1, M_FLOATDIV = 2, M_FLOATQUANT = 3 };
enum { LAY_ROWS = 0, LAY_COLS = 1 };
enum { D_LPC = 8, D_LOOKBACK = 9 };

/* ---------------------------------------------------------------- byte buffers */

typedef struct {
    u8 *p;
    size_t n, cap;
} Buf;

static void die(const char *m) {
    fprintf(stderr, "kbin: %s\n", m);
    exit(1);
}

static void buf_reserve(Buf *b, size_t extra) {
    if (b->n + extra <= b->cap) return;
    size_t c = b->cap ? b->cap : 256;
    while (c < b->n + extra) c *= 2;
    u8 *p = realloc(b->p, c);
    if (!p) die("out of memory");
    b->p = p;
    b->cap = c;
}
static void put_u8(Buf *b, u8 v) {
    buf_reserve(b, 1);
    b->p[b->n++] = v;
}
static void put_bytes(Buf *b, const void *s, size_t n) {
    if (!n) return;
    buf_reserve(b, n);
    memcpy(b->p + b->n, s, n);
    b->n += n;
}
static void put_var(Buf *b, u64 v) {
    buf_reserve(b, 10);
    while (v >= 0x80) {
        b->p[b->n++] = (u8)(v | 0x80);
        v >>= 7;
    }
    b->p[b->n++] = (u8)v;
}
typedef struct {
    const u8 *p, *end;
    int err;
} Rd;
static u8 get_u8(Rd *r) {
    if (r->p >= r->end) {
        r->err = 1;
        return 0;
    }
    return *r->p++;
}
static u64 get_var(Rd *r) {
    u64 v = 0;
    for (int s = 0; s < 70; s += 7) {
        if (r->p >= r->end) {
            r->err = 1;
            return 0;
        }
        u8 c = *r->p++;
        if (s == 63 && c > 1) {
            r->err = 1;
            return 0;
        }
        v |= (u64)(c & 0x7f) << s;
        if (!(c & 0x80)) return v;
    }
    r->err = 1;
    return 0;
}
static const u8 *get_bytes(Rd *r, u64 n) {
    if ((u64)(r->end - r->p) < n) {
        r->err = 1;
        return NULL;
    }
    const u8 *p = r->p;
    r->p += n;
    return p;
}

static inline int bitlen(u64 x) { return x ? 64 - __builtin_clzll(x) : 0; }
static inline u64 mask_bits(int nb) { return nb >= 64 ? ~0ull : ((1ull << nb) - 1); }
static inline u64 load64(const u8 *p) {
    u64 v;
    memcpy(&v, p, 8);
    return v;
}

/* ---------------------------------------------------------------- value mapping */

static inline u64 to_ord(u64 bits, int t) {
    int W = 8 * twidth[t];
    u64 sign = 1ull << (W - 1), m = mask_bits(W);
    if (t_float(t)) return (bits & sign) ? (~bits & m) : (bits | sign);
    if (t_signed(t)) return bits ^ sign;
    return bits;
}
static inline double bits_to_double(u64 b, int t) {
    if (t == T_F64) {
        double d;
        memcpy(&d, &b, 8);
        return d;
    }
    u32 w = (u32)b;
    float f;
    memcpy(&f, &w, 4);
    return f;
}
/* the float the decoder reconstructs from k in the divisor mode, as raw bits of the type */
static inline u64 floatdiv_bits(i64 k, u64 D, int t) {
    double y = (double)k / (double)D;
    if (t == T_F64) {
        u64 b;
        memcpy(&b, &y, 8);
        return b;
    }
    float f = (float)y;
    u32 w;
    memcpy(&w, &f, 4);
    return w;
}

/* ---------------------------------------------------------------- sorting */

static void radix_sort(u64 *a, u64 *tmp, size_t n) {
    if (n < 2) return;
    if (n < 48) {
        for (size_t i = 1; i < n; i++) {
            u64 v = a[i];
            size_t j = i;
            while (j && a[j - 1] > v) a[j] = a[j - 1], j--;
            a[j] = v;
        }
        return;
    }
    u64 mn = a[0], mx = a[0];
    for (size_t i = 1; i < n; i++) {
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }
    int nb = (bitlen(mx - mn) + 10) / 11;
    for (size_t i = 0; i < n; i++) a[i] -= mn;
    u64 *src = a, *dst = tmp;
    static const int R = 2048;
    size_t cnt[2048];
    for (int pass = 0; pass < nb; pass++) {
        int sh = pass * 11;
        memset(cnt, 0, sizeof cnt);
        for (size_t i = 0; i < n; i++) cnt[(src[i] >> sh) & (R - 1)]++;
        size_t s = 0;
        for (int i = 0; i < R; i++) {
            size_t c = cnt[i];
            cnt[i] = s;
            s += c;
        }
        for (size_t i = 0; i < n; i++) dst[cnt[(src[i] >> sh) & (R - 1)]++] = src[i];
        u64 *t = src;
        src = dst;
        dst = t;
    }
    if (src != a) memcpy(a, src, n * sizeof *a);
    for (size_t i = 0; i < n; i++) a[i] += mn;
}

/* ---------------------------------------------------------------- options and scratch */

typedef struct {
    int level;    /* log2 of the number of quantile groups the bin optimiser starts from */
    int lpc;      /* allow the linear-predictor delta */
    int floatdiv; /* allow the float divisor mode */
    int intmult;  /* allow the integer multiplier mode */
    int lookback; /* allow the lookback delta */
    int ctx;      /* context extension: bin index tables chosen by the previous bin */
    int threads;
    int chunk_log;
} Opt;

typedef struct {
    u64 *t1, *t2, *t3, *t4;
    u16 *idx;
    u16 *words;
    u8 *bits;
    size_t cap;
} Scr;

/* encoder scratch: four value arrays, bin indices, rANS words, offset bits. The decoder needs one value
 * array (lookback lags) and one batch of bin indices. */
static void scr_init(Scr *s, size_t n, int dec) {
    memset(s, 0, sizeof *s);
    s->cap = n + 8;
    s->t1 = malloc(s->cap * 8);
    s->idx = malloc((dec ? BATCH : s->cap) * 2);
    if (!s->t1 || !s->idx) die("out of memory");
    if (dec) return;
    s->t2 = malloc(s->cap * 8);
    s->t3 = malloc(s->cap * 8);
    s->t4 = malloc(s->cap * 8);
    s->words = malloc((s->cap + 2 * MAX_LANES) * 2);
    s->bits = malloc(s->cap * 8 + 16);
    if (!s->t2 || !s->t3 || !s->t4 || !s->words || !s->bits) die("out of memory");
}
static void scr_free(Scr *s) {
    free(s->t1);
    free(s->t2);
    free(s->t3);
    free(s->t4);
    free(s->idx);
    free(s->words);
    free(s->bits);
}

/* ---------------------------------------------------------------- cost proxy */

/* Bits for coding v[] with exponential bins around center: entropy of (sign, bit length) plus the bits
 * under the leading one. Used to rank modes and delta orders before the exact bin optimisation. */
static double proxy_cost(const u64 *v, size_t n, u64 center) {
    if (!n) return 0;
    u32 hist[130] = {0};
    u64 raw = 0;
    for (size_t i = 0; i < n; i++) {
        u64 d = v[i] - center;
        int neg = (i64)d < 0;
        u64 a = neg ? (u64)0 - d : d;
        int bl = bitlen(a);
        hist[bl * 2 + neg]++;
        raw += bl > 1 ? (u64)(bl - 1) : 0;
    }
    double bits = (double)raw, ln = log2((double)n);
    for (int i = 0; i < 130; i++)
        if (hist[i]) bits += hist[i] * (ln - log2((double)hist[i]));
    return bits;
}

static u64 approx_median(const u64 *v, size_t n) {
    u64 s[255];
    size_t m = n < 255 ? n : 255;
    for (size_t i = 0; i < m; i++) s[i] = v[(size_t)((double)i * (double)n / (double)m)];
    for (size_t i = 1; i < m; i++) {
        u64 x = s[i];
        size_t j = i;
        while (j && s[j - 1] > x) s[j] = s[j - 1], j--;
        s[j] = x;
    }
    return m ? s[m / 2] : 0;
}

/* ---------------------------------------------------------------- binning */

typedef struct {
    u64 lower;
    u32 count;
    u32 weight;
    u8 ob;
} Bin;

/* Choose bins for the sorted values s[0..n). Returns the bin count and the estimated cost in bits. */
static int optimize_bins(const u64 *s, size_t n, int level, Bin *bins, double *cost_out) {
    size_t G = (size_t)1 << level;
    if (G > n) G = n;
    u64 *lo = malloc((2 * G + 2) * 8), *hi = malloc((2 * G + 2) * 8);
    size_t *pre = malloc((2 * G + 3) * sizeof(size_t));
    double *best = malloc((2 * G + 2) * sizeof(double));
    int *from = malloc((2 * G + 2) * sizeof(int));
    if (!lo || !hi || !pre || !best || !from) die("out of memory");
    size_t m = 0, start = 0;
    pre[0] = 0;
    double step = (double)n / (double)G;
    while (start < n) {
        /* next quantile boundary after start; a run of equal values that crosses it becomes its own group */
        size_t g = (size_t)((double)start / step) + 1;
        size_t end = (size_t)((double)g * step);
        if (end <= start) end = start + 1;
        if (end > n || g >= G) end = n;
        if (end < n && s[end] == s[end - 1]) {
            size_t rs = end - 1;
            while (rs > start && s[rs - 1] == s[end - 1]) rs--;
            if (rs > start) end = rs;
            else
                while (end < n && s[end] == s[end - 1]) end++;
        }
        lo[m] = s[start];
        hi[m] = s[end - 1];
        pre[m + 1] = end;
        m++;
        start = end;
    }
    double ln = log2((double)n);
    int S_est = 10;
    best[0] = 0;
    for (size_t j = 1; j <= m; j++) {
        double bj = 1e300;
        int fj = 0;
        for (size_t i = j; i-- > 0;) {
            size_t c = pre[j] - pre[i];
            int ob = bitlen(hi[j - 1] - lo[i]);
            double cost = best[i] + (double)c * ob + (double)c * (ln - log2((double)c)) + 14 + S_est + ob;
            if (cost < bj) bj = cost, fj = (int)i;
        }
        best[j] = bj;
        from[j] = fj;
    }
    /* backtrack */
    int nb = 0;
    for (size_t j = m; j > 0; j = (size_t)from[j]) nb++;
    int k = nb;
    for (size_t j = m; j > 0; j = (size_t)from[j]) {
        size_t i = (size_t)from[j];
        k--;
        bins[k].lower = lo[i];
        bins[k].count = (u32)(pre[j] - pre[i]);
        bins[k].ob = (u8)bitlen(hi[j - 1] - lo[i]);
        bins[k].weight = 0;
    }
    *cost_out = best[m];
    free(lo);
    free(hi);
    free(pre);
    free(best);
    free(from);
    return nb;
}

static int ceil_log2(u64 x) { return x <= 1 ? 0 : bitlen(x - 1); }

static int choose_ans_log(int nb, size_t n) {
    int S = ceil_log2(n);
    if (S > 12) S = 12;
    int need = ceil_log2((u64)nb) + 2;
    if (S < need) S = need;
    if (S > ANS_MAX_LOG) S = ANS_MAX_LOG;
    return S;
}

static void quantize_weights(Bin *b, int nb, size_t n, int S) {
    u32 M = 1u << S;
    i64 sum = 0;
    int big = 0;
    for (int i = 0; i < nb; i++) {
        double w = (double)b[i].count * M / (double)n;
        u32 q = (u32)(w + 0.5);
        if (q < 1) q = 1;
        b[i].weight = q;
        sum += q;
        if (b[i].count > b[big].count) big = i;
    }
    i64 diff = (i64)M - sum;
    if (diff > 0) b[big].weight += (u32)diff;
    while (diff < 0) {
        /* take from the bins with the most slack relative to their count */
        int pick = -1;
        double bestr = -1;
        for (int i = 0; i < nb; i++)
            if (b[i].weight > 1) {
                double r = (double)b[i].weight / (double)b[i].count;
                if (r > bestr) bestr = r, pick = i;
            }
        if (pick < 0) break;
        u32 take = (u32)((-diff) < (i64)(b[pick].weight - 1) ? (-diff) : (i64)(b[pick].weight - 1));
        if (take > 1 + b[pick].weight / 8) take = 1 + b[pick].weight / 8;
        if (take > b[pick].weight - 1) take = b[pick].weight - 1;
        b[pick].weight -= take;
        diff += take;
    }
}

/* ---------------------------------------------------------------- residual stream: bins + rANS + offsets */

/* quantise counts to weights summing to 2^S; zero counts get weight zero */
static void quantize_counts(const u32 *cnt, int nb, int S, u32 *w) {
    u32 M = 1u << S;
    u64 N = 0;
    for (int i = 0; i < nb; i++) N += cnt[i];
    if (N == 0) {
        memset(w, 0, nb * sizeof *w);
        w[0] = M;
        return;
    }
    Bin tmp[256];
    Bin *b = nb <= 256 ? tmp : malloc((size_t)nb * sizeof(Bin));
    if (!b) die("out of memory");
    int k = 0, map[256];
    int *mp = nb <= 256 ? map : malloc((size_t)nb * sizeof(int));
    if (!mp) die("out of memory");
    for (int i = 0; i < nb; i++)
        if (cnt[i]) b[k].count = cnt[i], mp[k++] = i;
    quantize_weights(b, k, (size_t)N, S);
    memset(w, 0, nb * sizeof *w);
    for (int i = 0; i < k; i++) w[mp[i]] = b[i].weight;
    if (b != tmp) free(b);
    if (mp != map) free(mp);
}

static double row_cost(const u32 *r, int nb) {
    double tot = 0, h = 0;
    for (int b = 0; b < nb; b++) tot += r[b];
    if (tot == 0) return 0;
    double lt = log2(tot);
    for (int b = 0; b < nb; b++)
        if (r[b]) h += r[b] * (lt - log2((double)r[b]));
    return h;
}

/* Context extension (not in the paper): code each bin index with one of up to 8 static tables, picked
 * by the bin of the value `lag` positions earlier. Those bins are clustered greedily; returns the number of
 * contexts (1 = plain order 0) and fills cmap. */
#define CTX_MAX_BINS 64
#define CTX_MAX 8
static int choose_contexts(const u16 *idx, size_t n, int nb, int lag, u8 *cmap) {
    if (nb > CTX_MAX_BINS || n < 2048) return 1;
    static __thread u32 K[CTX_MAX_BINS][CTX_MAX_BINS];
    static __thread double D[CTX_MAX_BINS][CTX_MAX_BINS];
    memset(K, 0, sizeof K);
    for (size_t i = (size_t)lag; i < n; i++) K[idx[i - (size_t)lag]][idx[i]]++;
    int alive[CTX_MAX_BINS], na = 0, owner[CTX_MAX_BINS];
    double cst[CTX_MAX_BINS];
    u32 all[CTX_MAX_BINS] = {0};
    for (int a = 0; a < nb; a++) {
        owner[a] = a;
        for (int b = 0; b < nb; b++) all[b] += K[a][b];
        u64 rs = 0;
        for (int b = 0; b < nb; b++) rs += K[a][b];
        if (rs) alive[na++] = a;
        cst[a] = row_cost(K[a], nb);
    }
    double base = row_cost(all, nb);
    const double HDR = 8.0 * nb; /* bits for one more weight table */
    u32 tmp[CTX_MAX_BINS];
    for (int i = 0; i < na; i++)
        for (int j = i + 1; j < na; j++) {
            int a = alive[i], b = alive[j];
            for (int k = 0; k < nb; k++) tmp[k] = K[a][k] + K[b][k];
            D[a][b] = row_cost(tmp, nb) - cst[a] - cst[b];
        }
    while (na > 1) {
        int bi = -1, bj = -1;
        double bd = 1e300;
        for (int i = 0; i < na; i++)
            for (int j = i + 1; j < na; j++) {
                int a = alive[i], b = alive[j];
                double dd = a < b ? D[a][b] : D[b][a];
                if (dd < bd) bd = dd, bi = i, bj = j;
            }
        if (na <= CTX_MAX && bd > HDR) break;
        int a = alive[bi], b = alive[bj];
        if (a > b) {
            int t = a;
            a = b;
            b = t;
        }
        for (int k = 0; k < nb; k++) K[a][k] += K[b][k];
        cst[a] = row_cost(K[a], nb);
        for (int x = 0; x < nb; x++)
            if (owner[x] == b) owner[x] = a;
        alive[bj] = alive[--na];
        for (int i = 0; i < na; i++) {
            int c = alive[i];
            if (c == a) continue;
            for (int k = 0; k < nb; k++) tmp[k] = K[a][k] + K[c][k];
            double dd = row_cost(tmp, nb) - cst[a] - cst[c];
            if (a < c) D[a][c] = dd;
            else D[c][a] = dd;
        }
    }
    double tot = 0;
    for (int i = 0; i < na; i++) tot += cst[alive[i]];
    if (na <= 1 || base - tot < HDR * (na - 1) + 8.0 * nb + 64) return 1;
    /* number the clusters; bins never seen as a previous value join cluster 0 */
    int id[CTX_MAX_BINS];
    for (int a = 0; a < nb; a++) id[a] = -1;
    for (int i = 0; i < na; i++) id[alive[i]] = i;
    for (int a = 0; a < nb; a++) {
        int r = owner[a];
        cmap[a] = (u8)(id[r] >= 0 ? id[r] : 0);
    }
    return na;
}

static void code_residuals(Buf *o, const u64 *res, size_t n, int level, int ctx, Scr *sc) {
    if (n == 0) {
        put_var(o, 0);
        return;
    }
    u64 *s = sc->t2;
    memcpy(s, res, n * 8);
    TM_START;
    radix_sort(s, sc->t3, n);
    TM_END(1);
    Bin *bins = malloc(MAX_BINS * sizeof(Bin));
    if (!bins) die("out of memory");
    double cost;
    double tb_ = g_debug ? now_s() : 0;
    int nb = optimize_bins(s, n, level, bins, &cost);
    if (g_debug) g_tm[2] += now_s() - tb_;
    if (g_debug) {
        fprintf(stderr, "    bins=%d est=%.0fB\n", nb, cost / 8);
        for (int i = 0; i < nb && i < 20; i++) fprintf(stderr, "      lower=%lld ob=%d count=%u\n", (long long)(bins[i].lower - TOP64), bins[i].ob, bins[i].count);
    }
    put_var(o, (u64)nb);
    for (int i = 0; i < nb; i++) {
        put_var(o, i ? bins[i].lower - bins[i - 1].lower : bins[i].lower);
        put_u8(o, bins[i].ob);
    }
    u16 *idx = sc->idx;
    if (nb > 1) {
        int S = choose_ans_log(nb, n);
        tb_ = g_debug ? now_s() : 0;
        {
            /* bucket table over the value range, then a short forward scan */
            u64 base = bins[0].lower, span = s[n - 1] - base;
            int sh = bitlen(span) > 12 ? bitlen(span) - 12 : 0;
            size_t T = (size_t)(span >> sh) + 1;
            u16 *tab = malloc(T * sizeof(u16));
            if (!tab) die("out of memory");
            int b = 0;
            for (size_t k = 0; k < T; k++) {
                u64 v = base + ((u64)k << sh);
                while (b + 1 < nb && bins[b + 1].lower <= v) b++;
                tab[k] = (u16)b;
            }
            for (size_t i = 0; i < n; i++) {
                u64 v = res[i];
                int q = tab[(v - base) >> sh];
                while (q + 1 < nb && bins[q + 1].lower <= v) q++;
                idx[i] = (u16)q;
            }
            free(tab);
        }
        if (g_debug) g_tm[3] += now_s() - tb_, tb_ = now_s();
        u8 *cmap = calloc((size_t)nb, 1);
        if (!cmap) die("out of memory");
        int NL = ans_lanes(n);
        int C = ctx ? choose_contexts(idx, n, nb, NL, cmap) : 1;
        u32 *wt = calloc((size_t)C * nb, sizeof(u32)), *cumt = calloc((size_t)C * nb, sizeof(u32));
        u32 *cnt = calloc((size_t)C * nb, sizeof(u32));
        if (!wt || !cumt || !cnt) die("out of memory");
        for (size_t i = 0; i < n; i++) cnt[(size_t)(i >= (size_t)NL ? cmap[idx[i - NL]] : 0) * nb + idx[i]]++;
        put_u8(o, (u8)S);
        put_u8(o, (u8)C);
        if (C > 1)
            for (int i = 0; i < nb; i++) put_u8(o, cmap[i]);
        for (int c = 0; c < C; c++) {
            u32 *w = wt + (size_t)c * nb, acc = 0;
            if (C == 1) {
                quantize_weights(bins, nb, n, S);
                for (int i = 0; i < nb; i++) w[i] = bins[i].weight, put_var(o, w[i] - 1);
            } else {
                quantize_counts(cnt + (size_t)c * nb, nb, S, w);
                for (int i = 0; i < nb; i++) put_var(o, w[i]);
            }
            for (int i = 0; i < nb; i++) cumt[(size_t)c * nb + i] = acc, acc += w[i];
        }
        if (g_debug && C > 1) fprintf(stderr, "    contexts=%d\n", C);
        /* rANS with NL interleaved states, each with its own word stream (so the decoder's lanes do not
         * wait on each other), encoded backwards so the decoder reads forwards. Uses the exact
         * reciprocal q = mulhi(x, ceil(2^64 / f)) instead of a division. */
        u32 st[MAX_LANES];
        size_t lcap = n / (size_t)NL + 2, wp[MAX_LANES];
        u16 *w = sc->words;
        for (int k = 0; k < NL; k++) st[k] = ANS_L, wp[k] = (size_t)(k + 1) * lcap;
        u64 *rcp = malloc((size_t)C * nb * sizeof(u64));
        if (!rcp) die("out of memory");
        for (size_t k = 0; k < (size_t)C * nb; k++) rcp[k] = wt[k] > 1 ? (~0ull / wt[k]) + 1 : 0;
        for (size_t i = n; i-- > 0;) {
            int b = idx[i], l = (int)(i & (size_t)(NL - 1));
            size_t tix = (size_t)(i >= (size_t)NL ? cmap[idx[i - NL]] : 0) * nb + b;
            u32 f = wt[tix];
            u32 x = st[l];
            if ((u64)x >= ((u64)f << (32 - S))) {
                w[--wp[l]] = (u16)x;
                x >>= 16;
            }
            u32 q = f > 1 ? (u32)(((unsigned __int128)x * rcp[tix]) >> 64) : x;
            x = (q << S) + (x - q * f) + cumt[tix];
            st[l] = x;
        }
        free(rcp);
        free(cmap);
        free(wt);
        free(cumt);
        free(cnt);
        if (g_debug) g_tm[4] += now_s() - tb_;
        size_t nwords = 0, nw[MAX_LANES];
        for (int k = 0; k < NL; k++) nw[k] = (size_t)(k + 1) * lcap - wp[k], nwords += nw[k];
        put_var(o, (u64)nwords);
        for (int k = 0; k + 1 < NL; k++) put_var(o, (u64)nw[k]);
        buf_reserve(o, 4 * (size_t)NL + nwords * 2);
        for (int k = 0; k < NL; k++) {
            memcpy(o->p + o->n, &st[k], 4);
            o->n += 4;
        }
        for (int k = 0; k < NL; k++) {
            memcpy(o->p + o->n, w + wp[k], nw[k] * 2);
            o->n += nw[k] * 2;
        }
    } else {
        memset(idx, 0, n * 2);
    }
    /* offsets, LSB-first bit packing */
    u8 *bp = sc->bits;
    size_t nbytes = 0;
    u64 acc = 0;
    int na = 0;
    for (size_t i = 0; i < n; i++) {
        const Bin *b = &bins[idx[i]];
        int ob = b->ob;
        if (!ob) continue;
        u64 v = res[i] - b->lower;
        if (ob > 32) {
            acc |= (v & 0xffffffffull) << na;
            na += 32;
            if (na >= 32) {
                memcpy(bp + nbytes, &acc, 4);
                nbytes += 4;
                acc >>= 32;
                na -= 32;
            }
            v >>= 32;
            ob -= 32;
        }
        acc |= v << na;
        na += ob;
        if (na >= 32) {
            u32 lo32 = (u32)acc;
            memcpy(bp + nbytes, &lo32, 4);
            nbytes += 4;
            acc >>= 32;
            na -= 32;
        }
    }
    while (na > 0) {
        bp[nbytes++] = (u8)acc;
        acc >>= 8;
        na -= 8;
    }
    put_var(o, nbytes);
    put_bytes(o, bp, nbytes);
    free(bins);
}

static inline u64 read_bits(const u8 *p, size_t len, u64 pos, int nb) {
    size_t byte = (size_t)(pos >> 3);
    int sh = (int)(pos & 7);
    u64 v;
    if (byte + 8 <= len) v = load64(p + byte);
    else {
        v = 0;
        for (size_t k = 0; byte + k < len && k < 8; k++) v |= (u64)p[byte + k] << (8 * k);
    }
    return (v >> sh) & mask_bits(nb);
}
static inline u64 read_bits_any(const u8 *p, size_t len, u64 pos, int nb) {
    if (nb <= 56) return read_bits(p, len, pos, nb);
    u64 lo = read_bits(p, len, pos, 32);
    return lo | (read_bits(p, len, pos + 32, nb - 32) << 32);
}

typedef struct {
    u16 f, s, b, pad;
} DecEnt;

static int decode_residuals(Rd *r, u64 *res, size_t n, Scr *sc) {
    u64 nbv = get_var(r);
    if (r->err) return -1;
    if (n == 0) return nbv == 0 ? 0 : -1;
    if (nbv < 1 || nbv > MAX_BINS) return -1;
    int nb = (int)nbv;
    u64 lower[MAX_BINS];
    u8 obs[MAX_BINS];
    u64 acc = 0;
    for (int i = 0; i < nb; i++) {
        acc += get_var(r);
        lower[i] = acc;
        obs[i] = get_u8(r);
        if (obs[i] > 64) return -1;
    }
    if (r->err) return -1;
    int S = 0, nctx = 1;
    DecEnt *tbl = NULL;
    u64 nwords = 0;
    const int NL = ans_lanes(n);
    u32 st[MAX_LANES] = {0};
    u64 lw[MAX_LANES] = {0};
    const u8 *lp[MAX_LANES] = {0}, *lend[MAX_LANES] = {0};
    if (nb > 1) {
        S = get_u8(r);
        int C = get_u8(r);
        nctx = C;
        if (S < 1 || S > ANS_MAX_LOG || C < 1 || C > CTX_MAX || (C > 1 && nb > CTX_MAX_BINS)) return -1;
        u8 cmap[MAX_BINS] = {0};
        if (C > 1)
            for (int i = 0; i < nb; i++) {
                cmap[i] = get_u8(r);
                if (cmap[i] >= C) return -1;
            }
        u32 M = 1u << S;
        u32 *wts = malloc((size_t)nb * sizeof(u32));
        tbl = malloc(sizeof(DecEnt) * M * (size_t)C);
        if (!wts || !tbl) die("out of memory");
        for (int c = 0; c < C; c++) {
            u32 sum = 0;
            for (int i = 0; i < nb; i++) {
                u64 w = get_var(r) + (C == 1);
                if (r->err || w > M || sum + w > M) {
                    free(wts);
                    free(tbl);
                    return -1;
                }
                wts[i] = (u32)w;
                sum += (u32)w;
            }
            if (sum != M) {
                free(wts);
                free(tbl);
                return -1;
            }
            DecEnt *t = tbl + (size_t)c * M;
            u32 cc = 0;
            for (int i = 0; i < nb; i++) {
                for (u32 j = 0; j < wts[i]; j++) {
                    t[cc + j].f = (u16)wts[i];
                    t[cc + j].s = (u16)j;
                    t[cc + j].b = (u16)i;
                    t[cc + j].pad = cmap[i];
                }
                cc += wts[i];
            }
        }
        free(wts);
        nwords = get_var(r);
        u64 lsum = 0;
        for (int k = 0; k + 1 < NL; k++) {
            lw[k] = get_var(r);
            lsum += lw[k];
            if (lw[k] > nwords) lsum = nwords + 1;
        }
        if (r->err || nwords > n || lsum > nwords) {
            free(tbl);
            return -1;
        }
        lw[NL - 1] = nwords - lsum;
        const u8 *sp = get_bytes(r, 4 * (u64)NL + nwords * 2);
        if (!sp) {
            free(tbl);
            return -1;
        }
        for (int k = 0; k < NL; k++) memcpy(&st[k], sp + 4 * k, 4);
        const u8 *wbase = sp + 4 * NL;
        for (int k = 0; k < NL; k++) {
            lp[k] = wbase;
            lend[k] = wbase + 2 * lw[k];
            wbase = lend[k];
        }
    }
    u64 off_len = get_var(r);
    const u8 *bits = get_bytes(r, off_len);
    if (r->err || !bits) {
        free(tbl);
        return -1;
    }
    u64 total_bits = off_len * 8, pos = 0;
    typedef struct {
        u64 lower, mask;
        u64 ob;
    } BinD;
    BinD *bdt = malloc((size_t)nb * sizeof(BinD));
    if (!bdt) die("out of memory");
    int maxob = 0;
    for (int i = 0; i < nb; i++) {
        bdt[i].lower = lower[i];
        bdt[i].mask = mask_bits(obs[i]);
        bdt[i].ob = obs[i];
        if (obs[i] > maxob) maxob = obs[i];
    }
    u16 *idx = sc->idx;
    u32 mask = (1u << S) - 1;
    const DecEnt *curl[MAX_LANES];
    for (int k = 0; k < MAX_LANES; k++) curl[k] = tbl;
    for (size_t base = 0; base < n; base += BATCH) {
        size_t len = n - base < BATCH ? n - base : BATCH;
        if (nb == 1) {
            int ob = obs[0];
            if (total_bits - pos < (u64)ob * len) goto bad;
            u64 lw0 = lower[0];
            if (ob == 0)
                for (size_t k = 0; k < len; k++) res[base + k] = lw0;
            else
                for (size_t k = 0; k < len; k++, pos += (u64)ob) res[base + k] = lw0 + read_bits_any(bits, off_len, pos, ob);
            continue;
        }
        int fast = maxob <= 56 && pos + (u64)maxob * len + 64 <= total_bits;
        size_t per = len / (size_t)NL + 1;
        for (int k = 0; k < NL && fast; k++)
            if ((size_t)(lend[k] - lp[k]) < 2 * per) fast = 0;
        if (fast) {
            /* fused fast path: every word and every 8-byte offset load of this batch is in bounds */
#define DECF(L, K)                                                                                     \
    do {                                                                                               \
        u32 X_ = x[L];                                                                                 \
        const DecEnt e = c[L][X_ & mask];                                                              \
        if (CTXON) c[L] = tbl + ((size_t)e.pad << S);                                                  \
        X_ = (u32)e.f * (X_ >> S) + e.s;                                                               \
        u16 wv;                                                                                        \
        memcpy(&wv, p[L], 2);                                                                          \
        int need = X_ < ANS_L;                                                                         \
        x[L] = need ? (X_ << 16) | wv : X_;                                                            \
        p[L] += 2 * need;                                                                              \
        const BinD *bd = &bdt[e.b];                                                                    \
        out[K] = bd->lower + ((load64(bits + (pos >> 3)) >> (pos & 7)) & bd->mask);                   \
        pos += bd->ob;                                                                                 \
    } while (0)
#define DECF_BATCH(NLC)                                                                                \
    do {                                                                                               \
        u32 x[NLC];                                                                                    \
        const u8 *p[NLC];                                                                              \
        const DecEnt *c[NLC];                                                                          \
        for (int q = 0; q < NLC; q++) x[q] = st[q], p[q] = lp[q], c[q] = curl[q];                      \
        size_t k = 0;                                                                                  \
        for (; k + NLC <= len; k += NLC)                                                               \
            for (int q = 0; q < NLC; q++) DECF(q, k + q);                                              \
        for (int q = 0; k < len; q++, k++) DECF(q, k);                                                 \
        for (int q = 0; q < NLC; q++) st[q] = x[q], lp[q] = p[q], curl[q] = c[q];                      \
    } while (0)
            u64 *out = res + base;
            if (nctx > 1) {
#define CTXON 1
                if (NL == 4) DECF_BATCH(4);
                else DECF_BATCH(1);
#undef CTXON
            } else {
#define CTXON 0
                if (NL == 4) DECF_BATCH(4);
                else DECF_BATCH(1);
#undef CTXON
            }
            continue;
        }
        /* checked path, near the end of the streams */
        for (size_t k = 0; k < len; k++) {
            int l = (int)(k & (size_t)(NL - 1));
            u32 *x = &st[l];
            const DecEnt e = curl[l][*x & mask];
            curl[l] = tbl + ((size_t)e.pad << S);
            idx[k] = e.b;
            *x = (u32)e.f * (*x >> S) + e.s;
            if (*x < ANS_L) {
                if (lp[l] >= lend[l]) goto bad;
                u16 wv;
                memcpy(&wv, lp[l], 2);
                lp[l] += 2;
                *x = (*x << 16) | wv;
            }
        }
        u64 need = 0;
        for (size_t k = 0; k < len; k++) need += obs[idx[k]];
        if (total_bits - pos < need) goto bad;
        for (size_t k = 0; k < len; k++) {
            int b = idx[k], ob = obs[b];
            u64 v = lower[b];
            if (ob) {
                v += read_bits_any(bits, off_len, pos, ob);
                pos += (u64)ob;
            }
            res[base + k] = v;
        }
    }
    if (nb > 1) {
        for (int k = 0; k < NL; k++)
            if (st[k] != ANS_L || lp[k] != lend[k]) goto bad;
    }
    if ((pos + 7) / 8 != off_len) goto bad;
    free(tbl);
    free(bdt);
    return 0;
bad:
    free(tbl);
    free(bdt);
    return -1;
}

/* ---------------------------------------------------------------- delta encodings */

typedef struct {
    int kind; /* 0..7 consecutive order, D_LPC */
    int p, shift;
    i32 coef[MAX_LPC];
} Delta;

/* The predictor works on the low 32 bits of the values relative to the first one, with wrapping 32-bit
 * arithmetic, so that it vectorises; the encoder picks the shift so that the sum does not wrap for its
 * data, and encoder and decoder agree bit for bit in any case. crev holds the coefficients oldest first. */
static inline u64 lpc_pred32(const u32 *h, const u32 *crev, int p, int shift) {
    u32 s = 0;
    for (int k = 0; k < p; k++) s += crev[k] * h[k];
    return (u64)(i64)((i32)s >> shift);
}
static void lpc_rev(const Delta *d, u32 *crev) {
    for (int k = 0; k < d->p; k++) crev[k] = (u32)d->coef[d->p - 1 - k];
}
static void lpc_residuals(const u64 *lat, size_t n, const Delta *d, u64 *v, u64 *out) {
    u64 base = lat[0];
    u32 *h = (u32 *)v, crev[MAX_LPC];
    lpc_rev(d, crev);
    for (size_t i = 0; i < n; i++) h[i] = (u32)(lat[i] - base);
    for (size_t i = (size_t)d->p; i < n; i++)
        out[i] = (lat[i] - base) - lpc_pred32(h + i - d->p, crev, d->p, d->shift);
}

/* Levinson-Durbin on the autocorrelation; fills a[p][1..p] for p = 1..maxp and the prediction errors */
static int lpc_analyse(const u64 *lat, size_t n, int maxp, double a[][MAX_LPC + 1], double *err) {
    if (n < (size_t)(4 * maxp + 16)) return 0;
    double *x = malloc(n * sizeof(double));
    if (!x) die("out of memory");
    double mean = 0;
    u64 base = lat[0];
    for (size_t i = 0; i < n; i++) {
        x[i] = (double)(i64)(lat[i] - base);
        mean += x[i];
    }
    mean /= (double)n;
    for (size_t i = 0; i < n; i++) x[i] -= mean;
    double R[MAX_LPC + 1];
    for (int k = 0; k <= maxp; k++) {
        double s = 0;
        for (size_t i = (size_t)k; i < n; i++) s += x[i] * x[i - k];
        R[k] = s;
    }
    free(x);
    if (!(R[0] > 0)) return 0;
    double cur[MAX_LPC + 1] = {0}, prev[MAX_LPC + 1];
    double E = R[0];
    err[0] = E;
    for (int p = 1; p <= maxp; p++) {
        double acc = R[p];
        for (int j = 1; j < p; j++) acc -= cur[j] * R[p - j];
        double k = acc / E;
        memcpy(prev, cur, sizeof cur);
        cur[p] = k;
        for (int j = 1; j < p; j++) cur[j] = prev[j] - k * prev[p - j];
        E *= (1 - k * k);
        if (!(E > 0)) E = 1e-9 * R[0];
        err[p] = E;
        for (int j = 1; j <= p; j++) a[p][j] = cur[j];
    }
    return maxp;
}

static void lpc_quantize(const double *a, int p, double vmax, Delta *d) {
    double cmax = 0;
    for (int j = 1; j <= p; j++)
        if (fabs(a[j]) > cmax) cmax = fabs(a[j]);
    int prec = 14;
    int lg = cmax > 0 ? (int)floor(log2(cmax)) + 1 : 0;
    int shift = prec - lg;
    if (shift > 20) shift = 20;
    if (shift < 0) shift = 0;
    d->kind = D_LPC;
    d->p = p;
    for (;; shift--) {
        d->shift = shift;
        double e = 0, sum = 0;
        for (int j = 1; j <= p; j++) {
            double q = a[j] * (double)(1 << shift) + e;
            long qi = lround(q);
            if (qi > 32767) qi = 32767;
            if (qi < -32768) qi = -32768;
            e = q - (double)qi;
            d->coef[j - 1] = (i32)qi;
            sum += fabs((double)qi);
        }
        if (shift == 0 || sum * vmax < 2147483648.0) break;
    }
}

/* lookback delta: x[i] = x[i - lag[i]] + r[i]. Candidates for each value: the previous lag, the most recent
 * exact match, the most recent values sharing a prefix of the bits (three prefix lengths), and lag 1; the
 * one with the smallest residual wins, the previous lag on ties. */
#define LB_WLOG 12
#define LB_NH 4
static void lookback_lags(const u64 *x, size_t n, int wlog, u32 *lags) {
    size_t W = (size_t)1 << wlog;
    int hl = wlog + 1;
    size_t hs = (size_t)1 << hl;
    u32 *ht = malloc(hs * LB_NH * sizeof(u32));
    if (!ht) die("out of memory");
    memset(ht, 0xff, hs * LB_NH * sizeof(u32));
    u64 mn = x[0], mx = x[0];
    for (size_t i = 0; i < n; i++) {
        if (x[i] < mn) mn = x[i];
        if (x[i] > mx) mx = x[i];
    }
    int bl = bitlen(mx - mn);
    int sh[LB_NH] = {0, bl / 4, bl / 2, (3 * bl) / 4};
    u32 prev = 1;
    for (size_t i = 0; i < n; i++) {
        u64 v = x[i];
        u32 h[LB_NH];
        for (int k = 0; k < LB_NH; k++) h[k] = (u32)((((v - mn) >> sh[k]) * 0x9e3779b97f4a7c15ull) >> (64 - hl));
        if (i > 0) {
            u32 lag = prev <= i ? prev : 1;
            u64 bd = v - x[i - lag];
            bd = (i64)bd < 0 ? (u64)0 - bd : bd;
            if (bd) {
                u64 d1 = v - x[i - 1];
                d1 = (i64)d1 < 0 ? (u64)0 - d1 : d1;
                if (d1 < bd) bd = d1, lag = 1;
                for (int k = 0; k < LB_NH && bd; k++) {
                    u32 pp = ht[(size_t)k * hs + h[k]];
                    if (pp == 0xffffffffu || i - pp > W) continue;
                    u64 d = v - x[pp];
                    d = (i64)d < 0 ? (u64)0 - d : d;
                    if (d < bd) bd = d, lag = (u32)(i - pp);
                }
            }
            lags[i] = lag;
            prev = lag;
        }
        for (int k = 0; k < LB_NH; k++) ht[(size_t)k * hs + h[k]] = (u32)i;
    }
    free(ht);
}

static double lag_entropy(const u32 *lags, size_t n, int wlog) {
    size_t W = (size_t)1 << wlog;
    u32 *h = calloc(W + 1, sizeof(u32));
    if (!h) die("out of memory");
    for (size_t i = 1; i < n; i++) h[lags[i]]++;
    double bits = 0, ln = log2((double)(n - 1));
    size_t used = 0;
    for (size_t l = 1; l <= W; l++)
        if (h[l]) bits += h[l] * (ln - log2((double)h[l])), used++;
    free(h);
    return bits + 24.0 * (double)used;
}

/* a sample of up to 8 slices of `slice` contiguous values (all of v when short); returns its length */
static size_t take_sample(const u64 *v, size_t n, u64 *out, size_t slice) {
    if (n <= 8 * slice) {
        memcpy(out, v, n * 8);
        return n;
    }
    size_t m = 0;
    for (int k = 0; k < 8; k++) {
        size_t st = (size_t)((double)(n - slice) * k / 7.0);
        memcpy(out + m, v + st, slice * 8);
        m += slice;
    }
    return m;
}

/* choose the delta encoding of a latent stream from a sample; scratch t1, t2, t3. The returned cost is
 * scaled to the whole stream. */
static Delta choose_delta(const u64 *lat, size_t n, int max_order, int allow_lpc, int allow_lb, Scr *sc,
                          double *cost_out) {
    Delta d = {0};
    if (n < 4) {
        *cost_out = 0;
        return d;
    }
    u64 *smp = sc->t3;
    size_t m = take_sample(lat, n, smp, 2048);
    double scale = (double)n / (double)m;
    u64 *t = sc->t1;
    memcpy(t, smp, m * 8);
    double best = proxy_cost(t, m, approx_median(t, m));
    int bo = 0, worse = 0;
    double prevc = best;
    for (int k = 1; k <= max_order && (size_t)k < m; k++) {
        for (size_t i = m - 1; i >= (size_t)k; i--) t[i] -= t[i - 1];
        double c = proxy_cost(t + k, m - k, 0) + 64.0 * k / scale;
        if (c < best) best = c, bo = k;
        if (c >= prevc) {
            if (++worse >= 2) break;
        } else
            worse = 0;
        prevc = c;
    }
    d.kind = bo;
    if (allow_lpc && m >= 512) {
        /* a larger sample for the predictor; skip when the values are too wide for 32-bit predictions */
        u64 *ls = sc->t4;
        size_t lm = take_sample(lat, n, ls, 4096);
        double lscale = (double)n / (double)lm;
        double base_cost = best * scale / lscale;
        u64 mn = ls[0], mx = ls[0];
        for (size_t i = 0; i < lm; i++) {
            if (ls[i] < mn) mn = ls[i];
            if (ls[i] > mx) mx = ls[i];
        }
        if (bitlen(mx - mn) <= 30) {
            static __thread double a[MAX_LPC + 1][MAX_LPC + 1];
            double err[MAX_LPC + 1];
            int maxp = MAX_LPC;
            if ((size_t)maxp * 64 > lm) maxp = (int)(lm / 64);
            if (maxp >= 1 && lpc_analyse(ls, lm, maxp, a, err)) {
                int bp = 1;
                double be = 1e300;
                for (int p = 1; p <= maxp; p++) {
                    double e = 0.5 * (double)n * log2(err[p] / (double)lm + 1e-12) + 20.0 * p;
                    if (e < be) be = e, bp = p;
                }
                Delta l;
                lpc_quantize(a[bp], bp, (double)(mx - mn), &l);
                u64 *out = sc->t1;
                lpc_residuals(ls, lm, &l, sc->t2, out);
                double c = proxy_cost(out + l.p, lm - l.p, 0) + (64.0 * l.p + 16.0 * l.p) / lscale;
                if (c < base_cost * 0.985) {
                    best = c * lscale / scale;
                    d = l;
                }
            }
        }
    }
    if (allow_lb && n >= 64) {
        /* on a contiguous block, since lags reach back up to the window */
        size_t mb = n < 32768 ? n : 32768;
        const u64 *blk = lat + (n - mb) / 2;
        double cur = best * (double)mb / (double)m;
        static const int wl[3] = {8, 12, 15};
        u32 *lags = (u32 *)sc->t1;
        u64 *r = sc->t2;
        for (int w = 0; w < 3; w++) {
            if (w > 0 && (((size_t)1 << wl[w - 1]) >= mb || d.kind != D_LOOKBACK)) break;
            lookback_lags(blk, mb, wl[w], lags);
            for (size_t i = 1; i < mb; i++) r[i] = blk[i] - blk[i - lags[i]];
            double c = proxy_cost(r + 1, mb - 1, 0) + lag_entropy(lags, mb, wl[w]);
            if (c < cur * 0.97) {
                cur = c;
                best = c * (double)m / (double)mb;
                memset(&d, 0, sizeof d);
                d.kind = D_LOOKBACK;
                d.p = wl[w];
            }
        }
    }
    *cost_out = best * scale;
    return d;
}

/* encode one latent stream (lat is clobbered) */
static void enc_latent(Buf *o, u64 *lat, size_t n, const Opt *op, Scr *sc, int max_order, int allow_lpc,
                       int allow_lb) {
    double c;
    TM_START;
    Delta d = choose_delta(lat, n, max_order, allow_lpc, allow_lb, sc, &c);
    TM_END(0);
    size_t o0 = o->n;
    put_u8(o, (u8)d.kind);
    size_t warm;
    if (d.kind == D_LPC) {
        put_u8(o, (u8)d.p);
        put_u8(o, (u8)d.shift);
        for (int j = 0; j < d.p; j++) put_var(o, ((u64)(i64)d.coef[j] << 1) ^ (u64)((i64)d.coef[j] >> 63));
        warm = (size_t)d.p;
        u64 *out = sc->t1;
        lpc_residuals(lat, n, &d, sc->t2, out);
        for (size_t i = 0; i < warm; i++) put_var(o, lat[i]);
        for (size_t i = warm; i < n; i++) lat[i] = out[i] + TOP64;
    } else if (d.kind == D_LOOKBACK) {
        put_u8(o, (u8)d.p);
        warm = 1;
        u32 *lags = (u32 *)sc->t1;
        lookback_lags(lat, n, d.p, lags);
        put_var(o, lat[0]);
        u64 *lg = sc->t4;
        for (size_t i = 1; i < n; i++) lg[i - 1] = lags[i] - 1;
        code_residuals(o, lg, n - 1, op->level, op->ctx, sc);
        for (size_t i = n - 1; i >= 1; i--) lat[i] = lat[i] - lat[i - lags[i]] + TOP64;
    } else {
        int k = d.kind;
        warm = (size_t)k < n ? (size_t)k : n;
        for (int pass = 1; pass <= k; pass++)
            for (size_t i = n - 1; i >= (size_t)pass && i < n; i--) lat[i] -= lat[i - 1];
        for (size_t i = 0; i < warm; i++) put_var(o, lat[i]);
        if (k > 0)
            for (size_t i = warm; i < n; i++) lat[i] += TOP64;
    }
    code_residuals(o, lat + warm, n - warm, op->level, op->ctx, sc);
    if (g_debug)
        fprintf(stderr, "  latent n=%zu delta=%d p=%d proxy=%.0fB bytes=%zu\n", n, d.kind, d.p, c / 8, o->n - o0);
}

static int dec_latent(Rd *r, u64 *lat, size_t n, Scr *sc) {
    Delta d = {0};
    d.kind = get_u8(r);
    if (r->err) return -1;
    size_t warm;
    if (d.kind == D_LPC) {
        d.p = get_u8(r);
        d.shift = get_u8(r);
        if (d.p < 1 || d.p > MAX_LPC || d.shift > 30) return -1;
        for (int j = 0; j < d.p; j++) {
            u64 z = get_var(r);
            i64 v = (i64)(z >> 1) ^ -(i64)(z & 1);
            if (v > 32767 || v < -32768) return -1;
            d.coef[j] = (i32)v;
        }
        warm = (size_t)d.p;
    } else if (d.kind == D_LOOKBACK) {
        d.p = get_u8(r);
        if (d.p < 1 || d.p > 24 || n < 2) return -1;
        warm = 1;
    } else if (d.kind <= MAX_ORDER) {
        warm = (size_t)d.kind;
        if (d.kind > 0 && warm >= n) return -1;
    } else
        return -1;
    if (warm > n) warm = n;
    for (size_t i = 0; i < warm; i++) lat[i] = get_var(r);
    if (r->err) return -1;
    if (d.kind == D_LOOKBACK) {
        u64 *lg = sc->t1;
        if (decode_residuals(r, lg, n - 1, sc) || decode_residuals(r, lat + 1, n - 1, sc)) return -1;
        u64 W = (u64)1 << d.p;
        for (size_t i = 1; i < n; i++) {
            u64 l = lg[i - 1] + 1;
            if (l > i || l > W) return -1;
            lat[i] = lat[i - l] + (lat[i] - TOP64);
        }
        return 0;
    }
    double td_ = g_debug ? now_s() : 0;
    if (decode_residuals(r, lat + warm, n - warm, sc)) return -1;
    if (g_debug) g_tm[5] += now_s() - td_, td_ = now_s();
    if (d.kind == D_LPC) {
        if ((size_t)d.p > n) return 0;
        u64 base = lat[0];
        u32 *h = (u32 *)sc->t1, crev[MAX_LPC];
        lpc_rev(&d, crev);
        for (size_t i = 0; i < warm; i++) h[i] = (u32)(lat[i] - base);
        for (size_t i = warm; i < n; i++) {
            u64 v = lat[i] - TOP64 + lpc_pred32(h + i - d.p, crev, d.p, d.shift);
            h[i] = (u32)v;
            lat[i] = v + base;
        }
    } else if (d.kind > 0) {
        for (size_t i = warm; i < n; i++) lat[i] -= TOP64;
        for (int pass = d.kind; pass >= 1; pass--) {
            u64 prev = lat[pass - 1];
            for (size_t i = (size_t)pass; i < n; i++) prev = lat[i] += prev;
        }
    }
    if (g_debug) g_tm[6] += now_s() - td_;
    return 0;
}

/* ---------------------------------------------------------------- modes */

static u64 gcd64(u64 a, u64 b) {
    while (b) {
        u64 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

typedef struct {
    int mode;
    u64 param;
} Mode;

static double quick_cost_stream(const u64 *lat, size_t n, Scr *sc, int lpc) {
    double c;
    (void)choose_delta(lat, n, MAX_ORDER, lpc, 0, sc, &c);
    return c;
}

/* float divisor: k = round(x * D); the adjustment is ord(x) - ord(k / D) */
static void floatdiv_split(const u64 *bits, size_t n, int t, u64 D, u64 *prim, u64 *sec) {
    i64 prev = 0;
    double dd = (double)D;
    for (size_t i = 0; i < n; i++) {
        double x = bits_to_double(bits[i], t);
        double kx = x * dd;
        i64 k;
        if (kx == kx && fabs(kx) < 4.0e18) k = (i64)llround(kx);
        else k = prev;
        prev = k;
        u64 y = floatdiv_bits(k, D, t);
        prim[i] = (u64)k ^ TOP64;
        sec[i] = (to_ord(bits[i], t) - to_ord(y, t)) + TOP64;
    }
}

static u64 detect_floatdiv(const u64 *bits, size_t n, int t) {
    /* sample up to 512 finite values; smallest D (powers of ten, then small integers) where at least 90%
     * of them are exact, else where at least 90% are within float32 rounding of an integer multiple */
    double s[512];
    size_t m = 0;
    size_t step = n > 4096 ? n / 4096 : 1;
    for (size_t i = 0; i < n && m < 512; i += step) {
        double x = bits_to_double(bits[i], t);
        if (isfinite(x) && x != 0) s[m++] = x;
    }
    if (m < 8) return 0;
    u64 D = 1;
    for (int p = 0; p <= 15; p++, D *= 10) {
        size_t ok = 0;
        for (size_t i = 0; i < m; i++) {
            double kx = s[i] * (double)D;
            if (!(fabs(kx) < 9.0e15)) continue;
            i64 k = (i64)llround(kx);
            u64 y = floatdiv_bits(k, D, t);
            u64 xb;
            if (t == T_F64) memcpy(&xb, &s[i], 8);
            else {
                float f = (float)s[i];
                u32 w;
                memcpy(&w, &f, 4);
                xb = w;
            }
            ok += y == xb;
        }
        if (ok * 10 >= m * 9) return D;
    }
    for (u64 d = 1; d <= 4096; d++) {
        size_t ok = 0;
        for (size_t i = 0; i < m; i++) {
            double kx = s[i] * (double)d;
            if (!(fabs(kx) < 9.0e15)) continue;
            double e = fabs(kx - nearbyint(kx));
            if (e <= 1.2e-7 * fabs(kx) + 1e-12) ok++;
        }
        if (ok * 10 >= m * 9) return d;
    }
    return 0;
}

/* quantisation with rounding: hi = round(o / 2^q), lo = o - hi * 2^q (signed, centered) */
static inline u64 quant_hi(u64 o, int q) { return (o >> q) + ((o >> (q - 1)) & 1); }
static inline u64 quant_lo(u64 o, int q) { return o - (quant_hi(o, q) << q) + TOP64; }

static int detect_quant(const u64 *ord, size_t n, int t) {
    int W = 8 * twidth[t];
    u32 hist[65] = {0};
    size_t step = n > 8192 ? n / 8192 : 1, m = 0;
    for (size_t i = 0; i < n; i += step, m++) {
        u64 o = ord[i];
        hist[o ? __builtin_ctzll(o) : 64]++;
    }
    size_t acc = 0;
    for (int q = 64; q >= 1; q--) {
        acc += hist[q];
        if (acc * 10 >= m * 9) return q >= 4 && q < W ? q : 0;
    }
    return 0;
}

/* ---------------------------------------------------------------- chunk jobs */

typedef struct {
    int type, W, layout;
    size_t ncols, nrows, chunk_rows;
    const u8 *data; /* values (encode) */
    u8 *out;        /* values (decode) */
    Opt opt;
} Ctx;

typedef struct {
    const Ctx *cx;
    size_t col, r0, r1;
    Buf enc;
    const u8 *src;
    size_t src_len;
    int err;
    int mode;
} Job;

/* xor that maps an ordinal back to raw bits (and raw bits to an ordinal, for non-floats) */
static inline u64 ord_xor_dec(u64 o, int kind, int W) {
    u64 sign = 1ull << (8 * W - 1), m = mask_bits(8 * W);
    if (kind == 1) return o ^ sign;
    if (kind == 2) return o ^ ((o & sign) ? sign : m);
    return o;
}
static inline int ord_kind(int t) { return t_float(t) ? 2 : t_signed(t) ? 1 : 0; }

static void job_span(const Ctx *c, const Job *j, size_t *base, size_t *stride) {
    if (c->layout == LAY_COLS) *base = j->col * c->nrows + j->r0, *stride = 1;
    else *base = j->r0 * c->ncols + j->col, *stride = c->ncols;
}

#define GATHER(TY)                                                                                     \
    for (size_t i = 0; i < n; i++) {                                                                   \
        TY v;                                                                                          \
        memcpy(&v, p + i * st * sizeof(TY), sizeof(TY));                                               \
        bits[i] = v;                                                                                   \
    }
static void load_values(const Ctx *c, const Job *j, u64 *bits, u64 *ord, size_t n) {
    size_t base, st;
    job_span(c, j, &base, &st);
    const u8 *p = c->data + base * (size_t)c->W;
    if (c->W == 2) { GATHER(u16) }
    else if (c->W == 4) { GATHER(u32) }
    else { GATHER(u64) }
    int t = c->type;
    if (t_float(t)) for (size_t i = 0; i < n; i++) ord[i] = to_ord(bits[i], t);
    else if (t_signed(t)) {
        u64 sign = 1ull << (8 * c->W - 1);
        for (size_t i = 0; i < n; i++) ord[i] = bits[i] ^ sign;
    } else memcpy(ord, bits, n * 8);
}

#define SCATTER(TY)                                                                                    \
    for (size_t i = 0; i < n; i++) {                                                                   \
        TY v = (TY)ord_xor_dec(a[i], kind, (int)sizeof(TY));                                           \
        memcpy(p + i * st * sizeof(TY), &v, sizeof(TY));                                               \
    }
static void store_values(const Ctx *c, const Job *j, const u64 *a, size_t n) {
    size_t base, st;
    job_span(c, j, &base, &st);
    u8 *p = c->out + base * (size_t)c->W;
    int kind = ord_kind(c->type);
    if (c->W == 2) {
        if (kind == 0) { SCATTER(u16) } else if (kind == 1) { SCATTER(u16) } else { SCATTER(u16) }
    } else if (c->W == 4) {
        if (kind == 0) { SCATTER(u32) } else if (kind == 1) { SCATTER(u32) } else { SCATTER(u32) }
    } else {
        if (kind == 0) { SCATTER(u64) } else if (kind == 1) { SCATTER(u64) } else { SCATTER(u64) }
    }
}

/* per-thread value arrays reused across chunks */
typedef struct {
    u64 *a, *b;
    size_t cap;
} TBuf;
static __thread TBuf tbuf;
static void tbuf_get(size_t n, u64 **a, u64 **b) {
    if (tbuf.cap < n + 1) {
        free(tbuf.a);
        free(tbuf.b);
        tbuf.cap = n + 1;
        tbuf.a = malloc(tbuf.cap * 8);
        tbuf.b = malloc(tbuf.cap * 8);
        if (!tbuf.a || !tbuf.b) die("out of memory");
    }
    *a = tbuf.a;
    *b = tbuf.b;
}

static void encode_job(Job *j) {
    const Ctx *c = j->cx;
    size_t n = j->r1 - j->r0;
    int t = c->type;
    Scr sc;
    scr_init(&sc, n, 0);
    u64 *bits = malloc((n + 1) * 8), *a = malloc((n + 1) * 8), *b = malloc((n + 1) * 8);
    if (!bits || !a || !b) die("out of memory");
    load_values(c, j, bits, a, n);
    Buf *o = &j->enc;
    Mode md = {M_CLASSIC, 0};
    if (!t_float(t) && c->opt.intmult && n >= 2) {
        u64 g = 0;
        for (size_t i = 1; i < n && g != 1; i++) g = gcd64(g, a[i] > a[0] ? a[i] - a[0] : a[0] - a[i]);
        if (g >= 2) md.mode = M_INTMULT, md.param = g;
    }
    if (t_float(t) && n >= 16) {
        double best = quick_cost_stream(a, n, &sc, 0);
        u64 D = c->opt.floatdiv ? detect_floatdiv(bits, n, t) : 0;
        if (D) {
            floatdiv_split(bits, n, t, D, b, sc.t3);
            u64 *sec = malloc(n * 8);
            if (!sec) die("out of memory");
            memcpy(sec, sc.t3, n * 8);
            double cst = quick_cost_stream(b, n, &sc, 0) + quick_cost_stream(sec, n, &sc, 0);
            free(sec);
            if (cst < best) best = cst, md.mode = M_FLOATDIV, md.param = D;
        }
        /* quantisation candidates: the detected trailing-zero count, and float32-like splits */
        int qs[5], nq = 0, qd = detect_quant(a, n, t);
        if (qd) qs[nq++] = qd;
        static const int q64[3] = {29, 26, 20}, q32[2] = {13, 10};
        for (int k = 0; k < (t == T_F64 ? 3 : 2); k++) {
            int q = t == T_F64 ? q64[k] : q32[k];
            if (q != qd) qs[nq++] = q;
        }
        for (int k = 0; k < nq; k++) {
            int q = qs[k];
            for (size_t i = 0; i < n; i++) b[i] = quant_hi(a[i], q);
            double cst = quick_cost_stream(b, n, &sc, 0);
            for (size_t i = 0; i < n; i++) b[i] = quant_lo(a[i], q);
            cst += quick_cost_stream(b, n, &sc, 0);
            if (cst < best) best = cst, md.mode = M_FLOATQUANT, md.param = (u64)q;
        }
    }
    j->mode = md.mode;
    if (g_debug) fprintf(stderr, "chunk col=%zu rows=%zu..%zu mode=%d param=%llu\n", j->col, j->r0, j->r1, md.mode, (unsigned long long)md.param);
    put_u8(o, (u8)md.mode);
    int lpc = c->opt.lpc && !t_float(t);
    switch (md.mode) {
    case M_CLASSIC:
        enc_latent(o, a, n, &c->opt, &sc, MAX_ORDER, lpc, c->opt.lookback);
        break;
    case M_INTMULT: {
        u64 m = md.param;
        put_var(o, m);
        for (size_t i = 0; i < n; i++) b[i] = a[i] % m, a[i] /= m;
        enc_latent(o, a, n, &c->opt, &sc, MAX_ORDER, lpc, c->opt.lookback);
        enc_latent(o, b, n, &c->opt, &sc, 1, 0, 0);
        break;
    }
    case M_FLOATDIV: {
        put_var(o, md.param);
        floatdiv_split(bits, n, t, md.param, a, b);
        enc_latent(o, a, n, &c->opt, &sc, MAX_ORDER, c->opt.lpc, c->opt.lookback);
        enc_latent(o, b, n, &c->opt, &sc, 1, 0, 0);
        break;
    }
    case M_FLOATQUANT: {
        int q = (int)md.param;
        put_u8(o, (u8)q);
        for (size_t i = 0; i < n; i++) b[i] = quant_lo(a[i], q), a[i] = quant_hi(a[i], q);
        enc_latent(o, a, n, &c->opt, &sc, MAX_ORDER, 0, c->opt.lookback);
        enc_latent(o, b, n, &c->opt, &sc, 1, 0, 0);
        break;
    }
    }
    free(bits);
    free(a);
    free(b);
    scr_free(&sc);
}

static void decode_job(Job *j) {
    const Ctx *c = j->cx;
    size_t n = j->r1 - j->r0;
    int t = c->type;
    Scr sc;
    scr_init(&sc, n, 1);
    u64 *a, *b;
    tbuf_get(n, &a, &b);
    Rd r = {j->src, j->src + j->src_len, 0};
    int mode = get_u8(&r);
    j->err = 1;
    switch (mode) {
    case M_CLASSIC:
        if (dec_latent(&r, a, n, &sc)) goto out;
        break;
    case M_INTMULT: {
        u64 m = get_var(&r);
        if (r.err || m < 2) goto out;
        if (dec_latent(&r, a, n, &sc) || dec_latent(&r, b, n, &sc)) goto out;
        for (size_t i = 0; i < n; i++) a[i] = a[i] * m + b[i];
        break;
    }
    case M_FLOATDIV: {
        u64 D = get_var(&r);
        if (r.err || D == 0 || !t_float(t)) goto out;
        if (dec_latent(&r, a, n, &sc) || dec_latent(&r, b, n, &sc)) goto out;
        for (size_t i = 0; i < n; i++) {
            i64 k = (i64)(a[i] ^ TOP64);
            u64 y = floatdiv_bits(k, D, t);
            a[i] = to_ord(y, t) + (b[i] - TOP64);
        }
        break;
    }
    case M_FLOATQUANT: {
        int q = get_u8(&r);
        if (r.err || q < 1 || q >= 64) goto out;
        if (dec_latent(&r, a, n, &sc) || dec_latent(&r, b, n, &sc)) goto out;
        for (size_t i = 0; i < n; i++) a[i] = (a[i] << q) + (b[i] - TOP64);
        break;
    }
    default:
        goto out;
    }
    if (r.p != r.end) goto out;
    TM_START;
    store_values(c, j, a, n);
    TM_END(7);
    j->err = 0;
out:
    scr_free(&sc);
}

typedef struct {
    Job *jobs;
    size_t njobs;
    int dec;
    size_t next;
    pthread_mutex_t mu;
} Pool;

static void *worker(void *arg) {
    Pool *p = arg;
    for (;;) {
        pthread_mutex_lock(&p->mu);
        size_t i = p->next++;
        pthread_mutex_unlock(&p->mu);
        if (i >= p->njobs) {
            free(tbuf.a);
            free(tbuf.b);
            memset(&tbuf, 0, sizeof tbuf);
            return NULL;
        }
        if (p->dec) decode_job(&p->jobs[i]);
        else encode_job(&p->jobs[i]);
    }
}

static void run_jobs(Job *jobs, size_t njobs, int threads, int dec) {
    Pool p = {jobs, njobs, dec, 0, PTHREAD_MUTEX_INITIALIZER};
    if (threads <= 1 || njobs <= 1) {
        worker(&p);
        return;
    }
    if ((size_t)threads > njobs) threads = (int)njobs;
    pthread_t th[256];
    if (threads > 256) threads = 256;
    for (int i = 0; i < threads; i++)
        if (pthread_create(&th[i], NULL, worker, &p)) die("pthread_create failed");
    for (int i = 0; i < threads; i++) pthread_join(th[i], NULL);
}

/* ---------------------------------------------------------------- npy headers */

typedef struct {
    int is_npy;
    size_t hdr_len;  /* bytes before the data */
    int fortran;
    int ndim;
    u64 shape[8];
    int type;
    int canon_align; /* 16 or 64 when the header is exactly what numpy writes, else 0 */
} Npy;

static size_t npy_canonical(const Npy *h, int align, char *outbuf, size_t cap) {
    char dict[512], shp[300];
    size_t k = 0;
    shp[k++] = '(';
    for (int i = 0; i < h->ndim; i++) {
        k += (size_t)snprintf(shp + k, sizeof shp - k, "%s%llu", i ? ", " : "", (unsigned long long)h->shape[i]);
        if (k >= sizeof shp - 4) return 0;
    }
    if (h->ndim == 1) shp[k++] = ',';
    shp[k++] = ')';
    shp[k] = 0;
    int dl = snprintf(dict, sizeof dict, "{'descr': '%s', 'fortran_order': %s, 'shape': %s, }", tdescr[h->type],
                      h->fortran ? "True" : "False", shp);
    if (dl < 0 || (size_t)dl >= sizeof dict) return 0;
    size_t total = 10 + (size_t)dl + 1;
    size_t pad = (align - total % (size_t)align) % (size_t)align;
    total += pad;
    if (total > cap || total - 10 > 65535) return 0;
    memcpy(outbuf, "\x93NUMPY\x01\x00", 8);
    u16 hl = (u16)(total - 10);
    outbuf[8] = (char)(hl & 0xff);
    outbuf[9] = (char)(hl >> 8);
    memcpy(outbuf + 10, dict, (size_t)dl);
    memset(outbuf + 10 + dl, ' ', pad);
    outbuf[total - 1] = '\n';
    return total;
}

static int npy_parse(const u8 *p, size_t n, Npy *h) {
    memset(h, 0, sizeof *h);
    if (n < 10 || memcmp(p, "\x93NUMPY", 6)) return 0;
    int major = p[6];
    size_t hl, off;
    if (major == 1) hl = (size_t)p[8] | ((size_t)p[9] << 8), off = 10;
    else if (major == 2 || major == 3) {
        if (n < 12) return -1;
        hl = (size_t)p[8] | ((size_t)p[9] << 8) | ((size_t)p[10] << 16) | ((size_t)p[11] << 24);
        off = 12;
    } else
        return -1;
    if (off + hl > n || hl > 1 << 20) return -1;
    char *s = malloc(hl + 1);
    if (!s) die("out of memory");
    memcpy(s, p + off, hl);
    s[hl] = 0;
    int rc = -1;
    char *d = strstr(s, "'descr':");
    char *f = strstr(s, "'fortran_order':");
    char *sh = strstr(s, "'shape':");
    if (!d || !f || !sh) goto done;
    d = strchr(d + 8, '\'');
    if (!d) goto done;
    h->type = -1;
    for (int t = 0; t < T_COUNT; t++)
        if (!strncmp(d + 1, tdescr[t], 3) && d[4] == '\'') h->type = t;
    if (h->type < 0) goto done;
    f += 16;
    while (*f == ' ') f++;
    if (!strncmp(f, "True", 4)) h->fortran = 1;
    else if (!strncmp(f, "False", 5)) h->fortran = 0;
    else goto done;
    sh = strchr(sh, '(');
    if (!sh) goto done;
    sh++;
    h->ndim = 0;
    for (;;) {
        while (*sh == ' ' || *sh == ',') sh++;
        if (*sh == ')') break;
        if (*sh < '0' || *sh > '9' || h->ndim >= 8) goto done;
        h->shape[h->ndim++] = strtoull(sh, &sh, 10);
    }
    h->is_npy = 1;
    h->hdr_len = off + hl;
    rc = 1;
    /* is the header exactly what numpy would write? */
    char buf[1024];
    for (int al = 0; al < 2; al++) {
        int align = al ? 16 : 64;
        size_t L = npy_canonical(h, align, buf, sizeof buf);
        if (L && L == h->hdr_len && !memcmp(buf, p, L)) {
            h->canon_align = align;
            break;
        }
    }
done:
    free(s);
    return rc;
}

/* ---------------------------------------------------------------- frame */

typedef struct {
    int type;     /* -1: from npy */
    int stride;   /* raw input: interleaved columns; 0 = auto */
    int layout;   /* npy: use 2-D layout */
} InOpt;

static int g_cputime; /* KBIN_CPUTIME: time with the thread's CPU clock (one thread only) */
double now_s(void) {
    struct timespec ts;
    clock_gettime(g_cputime ? CLOCK_THREAD_CPUTIME_ID : CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* pick an interleave for a raw array: compare order-1 delta proxies for strides 1..8 on a sample */
static int auto_stride(const u8 *data, size_t nvals, int t) {
    int W = twidth[t];
    size_t m = nvals < 65536 ? nvals : 65536;
    if (m < 256 || t_float(t)) return 1;
    u64 *v = malloc(m * 8), *d = malloc(m * 8);
    if (!v || !d) die("out of memory");
    size_t start = (nvals - m) / 2;
    for (size_t i = 0; i < m; i++) {
        u64 x = 0;
        memcpy(&x, data + (start + i) * (size_t)W, (size_t)W);
        v[i] = to_ord(x, t);
    }
    double best = 0;
    int bs = 1;
    for (int s = 1; s <= 8; s++) {
        size_t k = 0;
        for (int c = 0; c < s; c++)
            for (size_t i = (size_t)c + (size_t)s; i < m; i += (size_t)s) d[k++] = v[i] - v[i - (size_t)s];
        double cst = proxy_cost(d, k, 0);
        if (s == 1 || cst < best * 0.97) best = cst, bs = s;
    }
    free(v);
    free(d);
    return bs;
}

static int compress_buf(const u8 *in, size_t n, const InOpt *io, const Opt *op, Buf *out) {
    Npy h;
    int rc = npy_parse(in, n, &h);
    if (rc < 0) {
        fprintf(stderr, "kbin: unsupported .npy header\n");
        return -1;
    }
    int t;
    size_t hdr_len = 0;
    if (rc == 1) {
        t = h.type;
        hdr_len = h.hdr_len;
    } else {
        if (io->type < 0) {
            fprintf(stderr, "kbin: raw input needs -t TYPE\n");
            return -1;
        }
        t = io->type;
        memset(&h, 0, sizeof h);
    }
    int W = twidth[t];
    size_t body = n - hdr_len;
    u64 nvals = body / (size_t)W;
    if (h.is_npy) {
        u64 prod = 1;
        for (int i = 0; i < h.ndim; i++) prod *= h.shape[i];
        if (prod <= nvals) nvals = prod;
    }
    size_t ncols = 1;
    int layout = LAY_ROWS;
    if (h.is_npy && io->layout && h.ndim == 2 && h.shape[0] * h.shape[1] == nvals && nvals) {
        if (h.fortran && h.shape[1] > 1) ncols = (size_t)h.shape[1], layout = LAY_COLS;
        else if (!h.fortran && h.shape[1] > 1 && h.shape[1] <= 16) ncols = (size_t)h.shape[1];
    } else if (!h.is_npy) {
        int s = io->stride ? io->stride : auto_stride(in, (size_t)nvals, t);
        if (s > 1 && nvals >= (u64)s) {
            ncols = (size_t)s;
            nvals = nvals / ncols * ncols;
        }
    }
    size_t nrows = (size_t)(nvals / ncols);
    size_t data_bytes = (size_t)nvals * (size_t)W;
    size_t tail = body - data_bytes;
    put_bytes(out, "KBN1", 4);
    put_u8(out, 1);
    put_u8(out, (u8)t);
    if (!h.is_npy) put_u8(out, 0);
    else if (h.canon_align && h.ndim <= 8) {
        put_u8(out, 2);
        put_u8(out, h.canon_align == 64 ? 1 : 0);
        put_u8(out, (u8)h.fortran);
        put_u8(out, (u8)h.ndim);
        for (int i = 0; i < h.ndim; i++) put_var(out, h.shape[i]);
    } else {
        put_u8(out, 1);
        put_var(out, hdr_len);
        put_bytes(out, in, hdr_len);
    }
    put_var(out, nvals);
    put_var(out, ncols);
    put_u8(out, (u8)layout);
    size_t chunk_rows = (size_t)1 << op->chunk_log;
    put_var(out, op->chunk_log);
    put_var(out, tail);
    put_bytes(out, in + hdr_len + data_bytes, tail);
    size_t per_col = nrows ? (nrows + chunk_rows - 1) / chunk_rows : 0;
    size_t njobs = per_col * ncols;
    Ctx cx = {t, W, layout, ncols, nrows, chunk_rows, in + hdr_len, NULL, *op};
    Job *jobs = calloc(njobs ? njobs : 1, sizeof(Job));
    if (!jobs) die("out of memory");
    for (size_t c = 0, k = 0; c < ncols; c++)
        for (size_t q = 0; q < per_col; q++, k++) {
            jobs[k].cx = &cx;
            jobs[k].col = c;
            jobs[k].r0 = q * chunk_rows;
            jobs[k].r1 = q * chunk_rows + chunk_rows < nrows ? q * chunk_rows + chunk_rows : nrows;
        }
    run_jobs(jobs, njobs, op->threads, 0);
    for (size_t k = 0; k < njobs; k++) put_var(out, jobs[k].enc.n);
    for (size_t k = 0; k < njobs; k++) {
        put_bytes(out, jobs[k].enc.p, jobs[k].enc.n);
        free(jobs[k].enc.p);
    }
    free(jobs);
    return 0;
}

static int decompress_buf(const u8 *in, size_t n, int threads, Buf *out) {
    Rd r = {in, in + n, 0};
    const u8 *mg = get_bytes(&r, 4);
    if (!mg || memcmp(mg, "KBN1", 4)) return -1;
    if (get_u8(&r) != 1) return -1;
    int t = get_u8(&r);
    if (r.err || t >= T_COUNT) return -1;
    int W = twidth[t];
    int hk = get_u8(&r);
    out->n = 0;
    if (hk == 1) {
        u64 hl = get_var(&r);
        const u8 *hp = get_bytes(&r, hl);
        if (!hp) return -1;
        put_bytes(out, hp, hl);
    } else if (hk == 2) {
        Npy h;
        memset(&h, 0, sizeof h);
        h.is_npy = 1;
        h.type = t;
        int align = get_u8(&r) ? 64 : 16;
        h.fortran = get_u8(&r) ? 1 : 0;
        h.ndim = get_u8(&r);
        if (h.ndim > 8) return -1;
        for (int i = 0; i < h.ndim; i++) h.shape[i] = get_var(&r);
        if (r.err) return -1;
        char buf[1024];
        size_t L = npy_canonical(&h, align, buf, sizeof buf);
        if (!L) return -1;
        put_bytes(out, buf, L);
    } else if (hk != 0)
        return -1;
    u64 nvals = get_var(&r), ncols = get_var(&r);
    int layout = get_u8(&r);
    u64 chunk_log = get_var(&r);
    u64 tail = get_var(&r);
    if (r.err || ncols == 0 || layout > 1 || chunk_log < 4 || chunk_log > 24 || nvals % ncols) return -1;
    if (nvals > ((u64)1 << 40) || nvals * (u64)W > ((u64)n << 20) + (1u << 20)) return -1; /* no valid frame expands more */
    const u8 *tp = get_bytes(&r, tail);
    if (!tp) return -1;
    size_t nrows = (size_t)(nvals / ncols), chunk_rows = (size_t)1 << chunk_log;
    size_t per_col = nrows ? (nrows + chunk_rows - 1) / chunk_rows : 0;
    if (per_col && ncols > (u64)SIZE_MAX / per_col) return -1;
    size_t njobs = per_col * (size_t)ncols;
    if (njobs > n) return -1;
    size_t hdr = out->n, data_bytes = (size_t)nvals * (size_t)W;
    Ctx cx = {t, W, layout, (size_t)ncols, nrows, chunk_rows, NULL, NULL, {0}};
    Job *jobs = calloc(njobs ? njobs : 1, sizeof(Job));
    if (!jobs) die("out of memory");
    u64 *sizes = malloc((njobs ? njobs : 1) * 8);
    if (!sizes) die("out of memory");
    for (size_t k = 0; k < njobs; k++) sizes[k] = get_var(&r);
    int rc = -1;
    if (r.err) goto done;
    for (size_t c = 0, k = 0; c < ncols; c++)
        for (size_t q = 0; q < per_col; q++, k++) {
            jobs[k].cx = &cx;
            jobs[k].col = c;
            jobs[k].r0 = q * chunk_rows;
            jobs[k].r1 = q * chunk_rows + chunk_rows < nrows ? q * chunk_rows + chunk_rows : nrows;
            jobs[k].src = get_bytes(&r, sizes[k]);
            jobs[k].src_len = (size_t)sizes[k];
            if (!jobs[k].src) goto done;
        }
    if (r.p != r.end) goto done;
    buf_reserve(out, data_bytes + tail);
    cx.out = out->p + hdr;
    run_jobs(jobs, njobs, threads, 1);
    for (size_t k = 0; k < njobs; k++)
        if (jobs[k].err) goto done;
    out->n = hdr + data_bytes;
    put_bytes(out, tp, tail);
    rc = 0;
done:
    free(sizes);
    free(jobs);
    return rc;
}

/* ---------------------------------------------------------------- files and CLI */

static u8 *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "kbin: %s: %s\n", path, strerror(errno));
        return NULL;
    }
    size_t cap = 1 << 20, len = 0;
    u8 *p = malloc(cap);
    if (!p) die("out of memory");
    for (;;) {
        if (len == cap) {
            cap *= 2;
            p = realloc(p, cap);
            if (!p) die("out of memory");
        }
        size_t k = fread(p + len, 1, cap - len, f);
        if (!k) break;
        len += k;
    }
    fclose(f);
    *n = len;
    return p;
}

static int write_file(const char *path, const u8 *p, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(p, 1, n, f) != n) {
        fprintf(stderr, "kbin: %s: %s\n", path, strerror(errno));
        if (f) fclose(f);
        return -1;
    }
    return fclose(f) ? -1 : 0;
}

static int parse_type(const char *s) {
    for (int t = 0; t < T_COUNT; t++)
        if (!strcmp(s, tname[t])) return t;
    return -1;
}

/* ---------------------------------------------------------------- self test */

static u64 rng_state = 0x9e3779b97f4a7c15ull;
static u64 rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static int roundtrip(const u8 *in, size_t n, const InOpt *io, const Opt *op, const char *what) {
    Buf c = {0}, d = {0};
    if (compress_buf(in, n, io, op, &c)) {
        fprintf(stderr, "selftest %s: compress failed\n", what);
        return 1;
    }
    int bad = decompress_buf(c.p, c.n, op->threads, &d) || d.n != n || (n && memcmp(d.p, in, n));
    if (bad) fprintf(stderr, "selftest %s: round trip FAILED (%zu bytes)\n", what, n);
    /* corrupted frames must be rejected or decoded without memory errors */
    for (int k = 0; k < 24 && c.n > 0; k++) {
        Buf e = {0}, f = {0};
        put_bytes(&e, c.p, c.n);
        size_t pos = (size_t)(rng() % c.n);
        if (k % 3 == 0) e.n = pos;
        else e.p[pos] ^= (u8)(1u << (rng() % 8));
        (void)decompress_buf(e.p, e.n, 1, &f);
        free(e.p);
        free(f.p);
    }
    free(c.p);
    free(d.p);
    return bad;
}

static int selftest(void) {
    int fails = 0, cases = 0;
    Opt op = {8, 1, 1, 1, 1, 0, 1, 10};
    size_t sizes[] = {0, 1, 2, 3, 5, 17, 300, 1025, 5000};
    for (int t = 0; t < T_COUNT; t++) {
        int W = twidth[t];
        for (size_t si = 0; si < sizeof sizes / sizeof *sizes; si++) {
            size_t n = sizes[si];
            u8 *buf = malloc(n * (size_t)W + 8);
            for (int pat = 0; pat < 9; pat++) {
                for (size_t i = 0; i < n; i++) {
                    u64 v;
                    switch (pat) {
                    case 0: v = 0; break;                                    /* constant zero */
                    case 1: v = 0x1234567890abcdefull; break;                /* constant */
                    case 2: v = rng(); break;                                 /* random */
                    case 3: v = (i % 2) ? ~0ull : (1ull << (8 * W - 1)); break; /* extremes */
                    case 4: v = (u64)(i64)(1000 + (i64)(i * 7) - (i64)(rng() % 5)); break;
                    case 5: v = (u64)((i64)(rng() % 3) - 1) * 1000; break;  /* multiples */
                    case 6: v = (rng() % 4 == 0) ? rng() : i; break;         /* outliers */
                    default: {
                        double x = (double)(i64)(rng() % 200000 - 100000) / 1000.0;
                        if (pat == 8) {
                            u64 r = rng() % 8;
                            x = r == 0 ? NAN : r == 1 ? INFINITY : r == 2 ? -INFINITY : r == 3 ? -0.0 : r == 4 ? 0.0 : x;
                        }
                        if (t == T_F32) {
                            float f = (float)x;
                            u32 w;
                            memcpy(&w, &f, 4);
                            v = w;
                        } else
                            memcpy(&v, &x, 8);
                        if (!t_float(t)) v = isfinite(x) ? (u64)(i64)(x * 1000) : (u64)-7;
                    }
                    }
                    memcpy(buf + i * (size_t)W, &v, (size_t)W);
                }
                for (int extra = 0; extra < 2; extra++) {
                    size_t len = n * (size_t)W + (extra ? 3 : 0);
                    if (extra) memcpy(buf + n * (size_t)W, "xyz", 3);
                    InOpt io = {t, 0, 1};
                    char what[64];
                    snprintf(what, sizeof what, "%s n=%zu pat=%d tail=%d", tname[t], n, pat, extra);
                    op.threads = (pat & 1) ? 3 : 1;
                    op.lpc = pat != 4 || extra;
                    op.ctx = (int)((si + (size_t)pat + (size_t)extra) & 1);
                    fails += roundtrip(buf, len, &io, &op, what);
                    cases++;
                    io.stride = 3;
                    fails += roundtrip(buf, len, &io, &op, what);
                    cases++;
                }
            }
            free(buf);
        }
    }
    /* long streams (four rANS lanes, lane contexts): a random walk with bursts, as i16, i64 and f64 */
    {
        size_t n = 140000;
        u8 *buf = malloc(n * 8);
        i64 v = 0;
        for (int t = 0; t < T_COUNT; t++) {
            if (t != T_I16 && t != T_I64 && t != T_F64) continue;
            for (size_t i = 0; i < n; i++) {
                v += (i64)(rng() % ((i / 1000) % 7 == 0 ? 401 : 9)) - ((i / 1000) % 7 == 0 ? 200 : 4);
                if (v > 30000 || v < -30000) v /= 2;
                if (t == T_I16) {
                    i16 w = (i16)v;
                    memcpy(buf + 2 * i, &w, 2);
                } else if (t == T_I64) {
                    i64 w = v * 1000 + 7;
                    memcpy(buf + 8 * i, &w, 8);
                } else {
                    double w = (double)v / 100.0;
                    memcpy(buf + 8 * i, &w, 8);
                }
            }
            for (int ctx = 0; ctx < 2; ctx++) {
                InOpt io = {t, 1, 1};
                Opt o2 = op;
                o2.ctx = ctx;
                o2.threads = 2;
                o2.chunk_log = 17;
                fails += roundtrip(buf, n * (size_t)twidth[t], &io, &o2, "long stream");
                cases++;
            }
        }
        free(buf);
    }
    /* special float bit patterns: NaN payloads, signalling NaN, -0.0, subnormals, inf */
    {
        u64 pats[] = {0x7ff8000000000000ull, 0xfff8000000000001ull, 0x7ff0000000000001ull, 0x8000000000000000ull,
                      0x0000000000000001ull, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x3ff0000000000000ull};
        u8 buf[8 * 64];
        for (int i = 0; i < 64; i++) memcpy(buf + 8 * i, &pats[(i * 5) % 8], 8);
        InOpt io = {T_F64, 1, 1};
        fails += roundtrip(buf, sizeof buf, &io, &op, "f64 specials");
        u32 p32[] = {0x7fc00000u, 0xffc00001u, 0x7f800001u, 0x80000000u, 1u, 0x7f800000u, 0xff800000u, 0x3f800000u};
        u8 b32[4 * 64];
        for (int i = 0; i < 64; i++) memcpy(b32 + 4 * i, &p32[(i * 3) % 8], 4);
        io.type = T_F32;
        fails += roundtrip(b32, sizeof b32, &io, &op, "f32 specials");
        cases += 2;
    }
    /* npy: canonical 1-D, 2-D C and Fortran headers, and a non-canonical one */
    {
        const char *hdrs[] = {"{'descr': '<i2', 'fortran_order': False, 'shape': (600,), }",
                              "{'descr': '<i2', 'fortran_order': False, 'shape': (300, 2), }",
                              "{'descr': '<i2', 'fortran_order': True, 'shape': (100, 6), }",
                              "{'descr':'<i2','fortran_order':False,'shape':(600,)}"};
        for (int k = 0; k < 4; k++) {
            u8 file[4096];
            size_t dl = strlen(hdrs[k]), total = 10 + dl + 1;
            total += (64 - total % 64) % 64;
            memcpy(file, "\x93NUMPY\x01\x00", 8);
            file[8] = (u8)(total - 10);
            file[9] = 0;
            memcpy(file + 10, hdrs[k], dl);
            memset(file + 10 + dl, ' ', total - 10 - dl);
            file[total - 1] = '\n';
            for (int i = 0; i < 600; i++) {
                i16 v = (i16)(i * 3 - (int)(rng() % 7));
                memcpy(file + total + 2 * i, &v, 2);
            }
            InOpt io = {-1, 0, 1};
            fails += roundtrip(file, total + 1200, &io, &op, "npy");
            cases++;
        }
    }
    printf("kbin selftest: %d cases, %d failures\n", cases, fails);
    return fails ? 1 : 0;
}

static void usage(void) {
    fprintf(stderr,
            "usage: kbin [options] INPUT [OUTPUT]\n"
            "  -t TYPE   element type of a raw array: i16 u16 i32 u32 i64 u64 f32 f64 (.npy: from the header)\n"
            "  -d        decompress\n"
            "  -b        bench: compress and decompress in memory, check, print JSON\n"
            "  -r N      bench runs (default 5, fastest kept)\n"
            "  -T N      threads (default 1)\n"
            "  -l N      level, log2 of the initial bin count (1..12, default 8)\n"
            "  -s N      raw input: N interleaved channels (default: detect)\n"
            "  -c N      log2 of the chunk length in values (default 18)\n"
            "  -n        no linear-predictor delta (pcodec's set of delta encodings only)\n"
            "  -C        context extension: bin indices coded with up to 8 tables picked by the previous bin\n"
            "  -F        no float divisor mode;  -M no integer multiplier mode;  -L no 2-D npy layout\n"
            "  --selftest  run the built-in round-trip and corruption tests\n");
    exit(2);
}

int main(int argc, char **argv) {
    Opt op = {8, 1, 1, 1, 1, 0, 1, 18};
    InOpt io = {-1, 0, 1};
    int dec = 0, bench = 0, runs = 5;
    const char *inp = NULL, *outp = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--selftest")) return selftest();
        if (a[0] == '-' && a[1] && !a[2]) {
            char c = a[1];
            if (strchr("trTlsc", c)) {
                if (++i >= argc) usage();
                const char *v = argv[i];
                switch (c) {
                case 't':
                    io.type = parse_type(v);
                    if (io.type < 0) usage();
                    break;
                case 'r': runs = atoi(v); break;
                case 'T': op.threads = atoi(v); break;
                case 'l': op.level = atoi(v); break;
                case 's': io.stride = atoi(v); break;
                case 'c': op.chunk_log = atoi(v); break;
                }
            } else if (c == 'd') dec = 1;
            else if (c == 'b') bench = 1;
            else if (c == 'n') op.lpc = 0;
            else if (c == 'C') op.ctx = 1;
            else if (c == 'F') op.floatdiv = 0;
            else if (c == 'M') op.intmult = 0;
            else if (c == 'L') io.layout = 0;
            else usage();
        } else if (!inp) inp = a;
        else if (!outp) outp = a;
        else usage();
    }
    if (!inp || op.level < 1 || op.level > MAX_LEVEL || op.threads < 1 || op.chunk_log < 4 || op.chunk_log > 24 ||
        runs < 1 || io.stride < 0 || io.stride > 64)
        usage();
    g_debug = getenv("KBIN_DEBUG") != NULL;
    g_cputime = getenv("KBIN_CPUTIME") != NULL && op.threads == 1;
    size_t n;
    u8 *in = read_file(inp, &n);
    if (!in) return 1;
    if (bench) {
        Buf c = {0}, d = {0};
        double bc = 1e30, bd = 1e30;
        for (int k = 0; k < runs; k++) {
            c.n = 0;
            double t0 = now_s();
            if (compress_buf(in, n, &io, &op, &c)) return 1;
            double t1 = now_s();
            if (t1 - t0 < bc) bc = t1 - t0;
        }
        for (int k = 0; k < runs; k++) {
            double t0 = now_s();
            if (decompress_buf(c.p, c.n, op.threads, &d)) {
                fprintf(stderr, "kbin: decode failed\n");
                return 1;
            }
            double t1 = now_s();
            if (t1 - t0 < bd) bd = t1 - t0;
        }
        if (d.n != n || memcmp(d.p, in, n)) {
            fprintf(stderr, "kbin: round trip mismatch\n");
            return 1;
        }
        const char *base = strrchr(inp, '/');
        printf("{\"file\": \"%s\", \"in_bytes\": %zu, \"out_bytes\": %zu, \"ratio\": %.4f, \"comp_MBps\": %.1f, "
               "\"dec_MBps\": %.1f, \"threads\": %d, \"level\": %d, \"lpc\": %d, \"runs\": %d}\n",
               base ? base + 1 : inp, n, c.n, c.n ? (double)n / (double)c.n : 0, n / bc / 1e6, n / bd / 1e6,
               op.threads, op.level, op.lpc, runs);
        if (g_debug) fprintf(stderr, "times (all runs): delta %.3f sort %.3f bins %.3f find %.3f ans %.3f | dec res %.3f recon %.3f store %.3f\n", g_tm[0], g_tm[1], g_tm[2], g_tm[3], g_tm[4], g_tm[5], g_tm[6], g_tm[7]);
        free(c.p);
        free(d.p);
        free(in);
        return 0;
    }
    if (!outp) usage();
    Buf o = {0};
    int rc = dec ? decompress_buf(in, n, op.threads, &o) : compress_buf(in, n, &io, &op, &o);
    if (rc) {
        fprintf(stderr, "kbin: %s failed\n", dec ? "decompression" : "compression");
        return 1;
    }
    rc = write_file(outp, o.p, o.n);
    free(o.p);
    free(in);
    return rc ? 1 : 0;
}
