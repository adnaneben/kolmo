/* Queryable archives: the IDX1 column and row index and the query engine (SPEC.md, "Column and row index").
 *
 * Writing: the input is compressed with KOLMO_T_COLUMNS (text blocks end at line ends, delimited tables become
 * column-split table blocks), then an IDX1 section records for every block whether it starts and ends at a line
 * boundary and how many lines start in it, the kind of text and a table's column names, and optionally a trigram
 * filter (a bitmap over the case-folded trigrams and whole alphanumeric tokens of the lines that start in the block).
 *
 * Reading: a query selects the blocks whose lines meet its row range and, for a pattern, whose filter holds every
 * trigram of a literal the pattern requires. Each selected block is decoded on its own (blocks are independent), on
 * all threads, or, for a column of a column-split table, only that column's streams are decoded. The index is
 * checked against what is decoded: a block whose line count disagrees with its entry is reported as corrupt.
 *
 * Licence: Apache-2.0. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "features.h"
#include <math.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IX_DEFAULT_BLOCK (4u << 20)
#define IX_T_TBL 3                       /* plan type of table blocks (kolmo.c) */
#define IX_BF_START 1                    /* block starts at a line start */
#define IX_BF_END 2                      /* block ends at a line end (or at the end of the input) */

static const uint8_t *ix_memrchr(const uint8_t *p, uint8_t c, size_t n) { while (n) { if (p[--n] == c) return p + n; } return NULL; }
/* at most 64: the callers keep their worker handles in pthread_t th[64] */
static int ix_threads(int t) { if (t > 0) return t > 64 ? 64 : t; long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)(n > 64 ? 64 : n) : 1; }
static inline uint8_t ix_lc(uint8_t c) { return (uint8_t)(c >= 'A' && c <= 'Z' ? c + 32 : c); }

/* ------------------------------------------------------------------ fields */
/* field j of a line as the table transform splits it: fields before the last one end at the delimiter (outside
 * quotes and brackets when quotes are on), field k - 1 runs to the end of the line. */
static int ix_table_field(const uint8_t *l, size_t n, uint8_t d, int quotes, uint32_t k, uint32_t j, size_t *o, size_t *len) {
    size_t i = 0;
    for (uint32_t f = 0;; f++) {
        size_t e;
        if (f + 1 >= k) e = n;
        else if (!quotes) { const uint8_t *q = memchr(l + i, d, n - i); e = q ? (size_t)(q - l) : n; }
        else {
            int inq = 0, inbr = 0; e = i;
            for (; e < n; e++) {
                uint8_t c = l[e];
                if (c == '"' && !inbr) inq ^= 1; else if (c == '[' && !inq) inbr = 1; else if (c == ']' && !inq) inbr = 0;
                else if (c == d && !inq && !inbr) break;
            }
        }
        if (f == j) { *o = i; *len = e - i; return 1; }
        if (e >= n) return 0;
        i = e + 1;
    }
}
static int ix_ws_field(const uint8_t *l, size_t n, uint32_t j, size_t *o, size_t *len) {
    size_t i = 0;
    for (uint32_t f = 0;; f++) {
        while (i < n && (l[i] == ' ' || l[i] == '\t' || l[i] == '\r')) i++;
        if (i >= n) return 0;
        size_t s = i; while (i < n && l[i] != ' ' && l[i] != '\t' && l[i] != '\r') i++;
        if (f == j) { *o = s; *len = i - s; return 1; }
    }
}
/* the end of the JSON value starting at i (string, number, literal, object or array), or 0 when malformed */
static size_t ix_json_value(const uint8_t *l, size_t n, size_t i) {
    if (i >= n) return 0;
    if (l[i] == '"') { for (i++; i < n; i++) { if (l[i] == '\\') i++; else if (l[i] == '"') return i + 1; } return 0; }
    if (l[i] == '{' || l[i] == '[') {
        int depth = 0;
        for (; i < n; i++) {
            uint8_t c = l[i];
            if (c == '"') { for (i++; i < n && l[i] != '"'; i++) if (l[i] == '\\') i++; if (i >= n) return 0; }
            else if (c == '{' || c == '[') depth++;
            else if ((c == '}' || c == ']') && --depth == 0) return i + 1;
        }
        return 0;
    }
    size_t s = i; while (i < n && l[i] != ',' && l[i] != '}' && l[i] != ']' && l[i] != ' ' && l[i] != '\t' && l[i] != '\r') i++;
    return i > s ? i : 0;
}
static int ix_json_field(const uint8_t *l, size_t n, const char *key, size_t kl, size_t *o, size_t *len) {
    size_t i = 0;
#define WS while (i < n && (l[i] == ' ' || l[i] == '\t' || l[i] == '\r')) i++
    WS; if (i >= n || l[i] != '{') return 0; i++;
    for (;;) {
        WS; if (i >= n || l[i] != '"') return 0;
        size_t ks = i + 1, ke = ix_json_value(l, n, i); if (!ke) return 0;
        i = ke; ke--; WS; if (i >= n || l[i] != ':') return 0; i++; WS;
        size_t vs = i, ve = ix_json_value(l, n, i); if (!ve) return 0;
        if (ke - ks == kl && !memcmp(l + ks, key, kl)) { *o = vs; *len = ve - vs; return 1; }
        i = ve; WS; if (i >= n || l[i] != ',') return 0; i++;
    }
#undef WS
}

/* ------------------------------------------------------------------ detection */
static int ix_detect(const uint8_t *s, size_t n, uint8_t *delim) {
    size_t h = n < (1u << 20) ? n : (1u << 20), lines = 0, json = 0;
    for (size_t i = 0; i < h && lines < 2000;) {
        const uint8_t *e = memchr(s + i, '\n', h - i); size_t end = e ? (size_t)(e - s) : h;
        size_t a = i; while (a < end && (s[a] == ' ' || s[a] == '\t')) a++;
        json += a < end && s[a] == '{' && end > a && s[end - 1 - (s[end - 1] == '\r')] == '}';
        lines++; i = end + 1;
    }
    if (lines && json * 10 >= lines * 9) return KOLMO_IDX_JSON;
    /* a table: the first line has at least two fields and 80% of the lines (the last one may lack its '\n') have as
     * many, counted as the table transform splits them (quotes and brackets group fields with ',' and ';') */
    static const uint8_t D[4] = {',', '\t', ';', '|'};
    for (int di = 0; di < 4; di++) {
        if (*delim && *delim != D[di]) continue;
        int qt = D[di] == ',' || D[di] == ';';
        size_t ln = 0, same = 0; uint32_t k0 = 0;
        for (size_t i = 0; i < h && ln < 2000;) {
            const uint8_t *e = memchr(s + i, '\n', h - i); size_t end = e ? (size_t)(e - s) : h;
            if (!e && h < n) break;                                   /* a line cut by the sample */
            size_t L = end - i, fo, fl; if (L && s[i + L - 1] == '\r') L--;
            uint32_t f = 0; while (f < 4096 && ix_table_field(s + i, L, D[di], qt, 0xFFFFFFFFu, f, &fo, &fl)) f++;
            if (ln == 0) k0 = f; else same += f == k0;
            ln++; i = end + 1;
        }
        if (ln >= 2 && k0 >= 2 && same * 100 >= (ln - 1) * 80) { *delim = D[di]; return KOLMO_IDX_TABLE; }
    }
    if (*delim) return KOLMO_IDX_TABLE;
    return KOLMO_IDX_LINES;
}

/* ------------------------------------------------------------------ trigram filter */
static inline uint32_t ix_tri(const uint8_t *p) { return (uint32_t)ix_lc(p[0]) | (uint32_t)ix_lc(p[1]) << 8 | (uint32_t)ix_lc(p[2]) << 16; }
static inline uint32_t ix_hash(uint32_t t) { return (uint32_t)(((uint64_t)t * 0x9E3779B97F4A7C15ull) >> 32); }
static inline uint64_t ix_slot(uint32_t h, uint64_t m) { return ((uint64_t)h * m) >> 32; }
static inline int ix_alnum(uint8_t c) { return (uint8_t)((c | 32) - 'a') < 26 || (uint8_t)(c - '0') < 10; }
#define IX_TOKEN_MIN 3
/* a whole token (a maximal run of ASCII letters and digits, case-folded, IX_TOKEN_MIN bytes or more) */
static inline uint32_t ix_token(const uint8_t *p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ull ^ 0x746f6b31ull;
    for (size_t i = 0; i < n; i++) { h ^= ix_lc(p[i]); h *= 0x100000001b3ull; }
    h ^= h >> 29; h *= 0xbf58476d1ce4e5b9ull; h ^= h >> 32;
    return (uint32_t)h;
}
/* every key of s[0..n): TRI for the trigrams without '\n', TOK for the whole tokens (h_ is the key's hash) */
#define IX_KEYS(s, n, TRI, TOK) do { \
    size_t ts_ = 0; int in_ = 0; \
    for (size_t i_ = 0; i_ < (n); i_++) { \
        if (i_ + 2 < (n) && (s)[i_] != '\n' && (s)[i_ + 1] != '\n' && (s)[i_ + 2] != '\n') { uint32_t h_ = ix_hash(ix_tri((s) + i_)); TRI; } \
        int a_ = ix_alnum((s)[i_]); \
        if (a_ && !in_) { ts_ = i_; in_ = 1; } \
        else if (!a_ && in_) { in_ = 0; if (i_ - ts_ >= IX_TOKEN_MIN) { uint32_t h_ = ix_token((s) + ts_, i_ - ts_); TOK; } } \
    } \
    if (in_ && (n) - ts_ >= IX_TOKEN_MIN) { uint32_t h_ = ix_token((s) + ts_, (n) - ts_); TOK; } \
} while (0)
/* bit j of a token's k bits (double hashing) */
static inline uint32_t ix_tokbit(uint32_t h, int j) { return h + (uint32_t)j * ((h * 0x9E3779B1u) | 1u); }
#define IX_SCRATCH_BITS (1u << 23)
static double ix_distinct(const uint64_t *sc) {
    uint64_t set = 0; for (size_t w = 0; w < IX_SCRATCH_BITS / 64; w++) set += (uint64_t)__builtin_popcountll(sc[w]);
    double M = IX_SCRATCH_BITS; return set >= IX_SCRATCH_BITS ? M * 8 : -M * log(1.0 - (double)set / M);
}
/* bitmap of m bits over the keys of s[0..n): one bit per trigram, k bits per token, m sized so that about half the
 * bits are set (1.44 bits per bit set), from distinct counts estimated on two 2^23-bit scratch bitmaps. Returns m (0
 * when there is no key) and the bytes in *bm. */
static uint64_t ix_filter(const uint8_t *s, size_t n, int k, uint64_t *scratch, uint8_t **bm) {
    *bm = NULL; if (!n) return 0;
    uint64_t *st = scratch, *sk = scratch + IX_SCRATCH_BITS / 64;
    memset(scratch, 0, IX_SCRATCH_BITS / 4);
    IX_KEYS(s, n, { uint64_t b_ = ix_slot(h_, IX_SCRATCH_BITS); st[b_ >> 6] |= 1ull << (b_ & 63); },
                  { uint64_t b_ = ix_slot(h_, IX_SCRATCH_BITS); sk[b_ >> 6] |= 1ull << (b_ & 63); });
    double d = ix_distinct(st) + k * ix_distinct(sk);
    if (d < 0.5) return 0;
    uint64_t m = (uint64_t)ceil(d * 1.44 / 64.0) * 64; if (m < 64) m = 64; if (m > (1ull << 32)) m = 1ull << 32;
    uint64_t *f = calloc((size_t)(m / 64), 8); if (!f) return UINT64_MAX;
    IX_KEYS(s, n, { uint64_t b_ = ix_slot(h_, m); f[b_ >> 6] |= 1ull << (b_ & 63); },
                  { for (int j_ = 0; j_ < k; j_++) { uint64_t b_ = ix_slot(ix_tokbit(h_, j_), m); f[b_ >> 6] |= 1ull << (b_ & 63); } });
    uint8_t *o = malloc((size_t)(m / 8)); if (!o) { free(f); return UINT64_MAX; }
    for (size_t w = 0; w < m / 64; w++) kx_put64(o + 8 * w, f[w]);
    free(f); *bm = o; return m;
}
/* query keys: the trigrams of the literal, and its tokens that are whole: bounded inside the literal on both sides,
 * or, in word mode (every token of a fixed pattern matched as a word), all of them */
typedef struct { uint32_t *h; size_t n; } ix_keys;
static int ix_query_keys(const uint8_t *lit, size_t ln, int word, int k, ix_keys *K) {
    K->n = 0; K->h = malloc(sizeof(uint32_t) * ((size_t)(k + 1) * ln + 2)); if (!K->h) return -1;
    for (size_t i = 0; i + 2 < ln; i++) if (lit[i] != '\n' && lit[i + 1] != '\n' && lit[i + 2] != '\n') K->h[K->n++] = ix_hash(ix_tri(lit + i));
    for (size_t i = 0; i < ln;) {
        if (!ix_alnum(lit[i])) { i++; continue; }
        size_t s = i; while (i < ln && ix_alnum(lit[i])) i++;
        if (i - s >= IX_TOKEN_MIN && (word || (s > 0 && i < ln))) { uint32_t h = ix_token(lit + s, i - s); for (int j = 0; j < k; j++) K->h[K->n++] = ix_tokbit(h, j); }
    }
    return 0;
}
static int ix_filter_has(const uint8_t *bm, uint64_t m, const ix_keys *K) {
    if (!m) return 1;
    for (size_t i = 0; i < K->n; i++) { uint64_t b = ix_slot(K->h[i], m); if (!(bm[b >> 3] >> (b & 7) & 1)) return 0; }
    return 1;
}

/* ------------------------------------------------------------------ writing */
size_t kolmo_indexed_bound(size_t n, const kolmo_params *p) {
    kolmo_params q = p ? *p : (kolmo_params){KOLMO_FAST, 0, 0, 0, 1};
    if (!q.block) q.block = IX_DEFAULT_BLOCK;
    size_t nb = n / (q.block / 2 + 1) + 2;
    /* filters: 1.44 bits per bit set; at most one trigram per byte and one token per 4 bytes at up to 16 bits each */
    return kolmo_bound(n, &q) + 4096 + nb * 96 + n + n / 2;
}

typedef struct {
    const uint8_t *src; size_t n; const kx_frame *f; int tokk; int filter;
    uint64_t *lines; uint8_t *flags; uint8_t **bm; uint64_t *m; uint32_t next; pthread_mutex_t mu; int err;
} ix_build;
static void *ix_build_worker(void *arg) {
    ix_build *B = arg; uint64_t *scratch = B->filter ? malloc(IX_SCRATCH_BITS / 4) : NULL;
    if (B->filter && !scratch) { pthread_mutex_lock(&B->mu); B->err = KOLMO_ERR_ALLOC; pthread_mutex_unlock(&B->mu); return NULL; }
    for (;;) {
        pthread_mutex_lock(&B->mu); uint32_t b = B->next++; pthread_mutex_unlock(&B->mu);
        if (b >= B->f->nb) break;
        const uint8_t *s = B->src; size_t lo = (size_t)B->f->roff[b], hi = (size_t)B->f->roff[b + 1];
        uint8_t fl = 0;
        if (lo == 0 || s[lo - 1] == '\n') fl |= IX_BF_START;
        if (hi == B->n || (hi > lo && s[hi - 1] == '\n')) fl |= IX_BF_END;
        /* lines that start in the block: at lo when it is a line start, and after every '\n' before hi - 1 or EOF */
        uint64_t L = (fl & IX_BF_START) && lo < B->n ? 1 : 0; size_t own = lo;
        int seen = 0;
        for (const uint8_t *q = s + lo, *e = s + hi; (q = memchr(q, '\n', (size_t)(e - q))); q++) {
            size_t at = (size_t)(q - s);
            if (!seen && !(fl & IX_BF_START)) own = at + 1;
            seen = 1;
            if (at + 1 < hi) L++;                                     /* a '\n' at hi - 1 starts the next block's line */
        }
        if (!(fl & IX_BF_START) && !seen) own = hi;                  /* a block inside one line owns nothing */
        B->lines[b] = L; B->flags[b] = fl;
        if (B->filter && L) {
            size_t end = hi;                                         /* the last owned line runs to its '\n' */
            if (!(fl & IX_BF_END)) { const uint8_t *q = memchr(s + hi, '\n', B->n - hi); end = q ? (size_t)(q - s) : B->n; }
            uint64_t m = ix_filter(s + own, end - own, B->tokk, scratch, &B->bm[b]);
            if (m == UINT64_MAX) { pthread_mutex_lock(&B->mu); B->err = KOLMO_ERR_ALLOC; pthread_mutex_unlock(&B->mu); break; }
            B->m[b] = m;
        }
    }
    free(scratch); return NULL;
}

int64_t kolmo_compress_indexed(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                               const kolmo_index_params *ip) {
    kolmo_params q = p ? *p : (kolmo_params){KOLMO_FAST, 0, 0, 0, 1};
    kolmo_index_params I = ip ? *ip : (kolmo_index_params){KOLMO_IDX_AUTO, 0, 1, 0};
    if (I.kind < 0 || I.kind > KOLMO_IDX_JSON || I.token_bits < 0 || I.token_bits > 16) return KOLMO_ERR_LIMIT;
    if (!q.block) q.block = IX_DEFAULT_BLOCK;
    uint8_t delim = I.delim; int kind = I.kind;
    if (kind == KOLMO_IDX_AUTO || (kind == KOLMO_IDX_TABLE && !delim)) {
        int k2 = ix_detect(src, n, &delim);
        if (kind == KOLMO_IDX_AUTO) kind = k2;
        else if (k2 != KOLMO_IDX_TABLE) delim = ',';
    }
    if (q.transforms) q.transforms |= KOLMO_T_COLUMNS;
    int64_t fe = kolmo_compress(src, n, dst, cap, &q);
    if (fe < 0) return fe;
    kx_frame f; int r = kx_frame_open(dst, (size_t)fe, &f); if (r) return r;
    ix_build B = {src, n, &f, I.token_bits > 0 ? I.token_bits : 4, I.filter != 0, NULL, NULL, NULL, NULL, 0,
                  PTHREAD_MUTEX_INITIALIZER, 0};
    B.lines = calloc((size_t)f.nb + 1, 8); B.flags = calloc((size_t)f.nb + 1, 1);
    B.bm = calloc((size_t)f.nb + 1, sizeof(uint8_t *)); B.m = calloc((size_t)f.nb + 1, 8);
    kx_buf o = {0}; int64_t ret = KOLMO_ERR_ALLOC;
    if (!B.lines || !B.flags || !B.bm || !B.m) goto done;
    {   int T = ix_threads(q.threads); if ((uint32_t)T > f.nb) T = (int)f.nb; if (T < 1) T = 1;
        pthread_t th[64]; int started = 0;
        for (int t = 1; t < T; t++) if (!pthread_create(&th[started], NULL, ix_build_worker, &B)) started++;
        ix_build_worker(&B);
        for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
        if (B.err) { ret = B.err; goto done; } }
    uint64_t rows = 0; for (uint32_t b = 0; b < f.nb; b++) rows += B.lines[b];
    uint8_t h[8] = {'I', 'X', 1, (uint8_t)kind, kind == KOLMO_IDX_TABLE ? delim : 0, (uint8_t)(B.filter ? 1 : 0), 0, 0};
    kx_bytes(&o, h, 8);
    uint8_t w[16]; kx_put32(w, f.nb); kx_put64(w + 4, rows); kx_bytes(&o, w, 12);
    /* column names: the fields of the first line (tables), quotes around a name removed */
    if (kind == KOLMO_IDX_TABLE && n) {
        const uint8_t *e = memchr(src, '\n', n); size_t ln = e ? (size_t)(e - src) : n;
        if (ln && src[ln - 1] == '\r') ln--;
        uint32_t k = 1; for (size_t i = 0; i < ln; i++) k += src[i] == delim;
        int qt = delim == ',' || delim == ';'; uint32_t got = 0;
        kx_buf names = {0};
        for (uint32_t j = 0; j < k; j++) {
            size_t fo, fl; if (!ix_table_field(src, ln, delim, qt, 0xFFFFFFFFu, j, &fo, &fl)) break;
            if (fl >= 2 && src[fo] == '"' && src[fo + fl - 1] == '"') { fo++; fl -= 2; }
            kx_uv(&names, fl); kx_bytes(&names, src + fo, fl); got++;
        }
        kx_uv(&o, got); kx_bytes(&o, names.p, names.n); free(names.p);
        if (names.oom) o.oom = 1;
    } else kx_uv(&o, 0);
    for (uint32_t b = 0; b < f.nb; b++) { kx_bytes(&o, &B.flags[b], 1); kx_uv(&o, B.lines[b]); }
    if (B.filter) {
        uint8_t g[2] = {3, (uint8_t)B.tokk}; kx_bytes(&o, g, 2);
        for (uint32_t b = 0; b < f.nb; b++) { kx_uv(&o, B.m[b]); kx_bytes(&o, B.bm[b], (size_t)(B.m[b] / 8)); }
    }
    if (o.oom) goto done;
    {   static const char tag[1][4] = {{'I', 'D', 'X', '1'}};
        const uint8_t *bodies[1] = {o.p}; uint64_t lens[1] = {o.n};
        ret = kx_append(dst, cap, f.end, 1, tag, bodies, lens); }
done:
    if (B.bm) for (uint32_t b = 0; b < f.nb; b++) free(B.bm[b]);
    free(B.lines); free(B.flags); free(B.bm); free(B.m); free(o.p);
    kx_frame_close(&f);
    return ret;
}

/* ------------------------------------------------------------------ reading the index */
typedef struct {
    int kind; uint8_t delim; int has_filter, tokk; uint32_t nb, k; uint64_t rows;
    const uint8_t *names; size_t nnames_bytes; uint32_t nnames;
    uint8_t *flags; uint64_t *lines, *row0; const uint8_t **bm; uint64_t *m; uint64_t size;
} ix_index;
static void ix_free(ix_index *X) { free(X->flags); free(X->lines); free(X->row0); free(X->bm); free(X->m); memset(X, 0, sizeof *X); }
/* 0 with X filled, 1 when the file has no IDX1, or an error */
static int ix_load(const uint8_t *s, size_t n, const kx_frame *f, ix_index *X) {
    memset(X, 0, sizeof *X);
    uint64_t L; int err; const uint8_t *p = kx_find(s, n, f->end, "IDX1", &L, &err);
    if (err) return err;
    if (!p) return 1;
    X->size = L;
    if (L < 20 || p[0] != 'I' || p[1] != 'X' || p[2] != 1 || p[3] < KOLMO_IDX_LINES || p[3] > KOLMO_IDX_JSON || p[5] > 1 || p[6] || p[7]) return KOLMO_ERR_CORRUPT;
    X->kind = p[3]; X->delim = p[4]; X->has_filter = p[5];
    if ((X->kind == KOLMO_IDX_TABLE) != (X->delim != 0)) return KOLMO_ERR_CORRUPT;
    X->nb = kx_get32(p + 8); X->rows = kx_get64(p + 12);
    if (X->nb != f->nb) return KOLMO_ERR_CORRUPT;
    size_t i = 20; uint64_t v;
    if (kx_getuv(p, (size_t)L, &i, &v) || v > L) return KOLMO_ERR_CORRUPT;
    X->nnames = (uint32_t)v; X->names = p + i;
    for (uint32_t j = 0; j < X->nnames; j++) { if (kx_getuv(p, (size_t)L, &i, &v) || v > L - i) return KOLMO_ERR_CORRUPT; i += (size_t)v; }
    X->nnames_bytes = (size_t)(p + i - X->names); X->k = X->nnames;
    X->flags = malloc((size_t)X->nb + 1); X->lines = malloc(8 * ((size_t)X->nb + 1)); X->row0 = malloc(8 * ((size_t)X->nb + 1));
    X->bm = calloc((size_t)X->nb + 1, sizeof(uint8_t *)); X->m = calloc((size_t)X->nb + 1, 8);
    if (!X->flags || !X->lines || !X->row0 || !X->bm || !X->m) { ix_free(X); return KOLMO_ERR_ALLOC; }
    uint64_t r = 0;
    for (uint32_t b = 0; b < X->nb; b++) {
        if (i >= L) { ix_free(X); return KOLMO_ERR_CORRUPT; }
        X->flags[b] = p[i++];
        if (X->flags[b] > 3 || kx_getuv(p, (size_t)L, &i, &v)) { ix_free(X); return KOLMO_ERR_CORRUPT; }
        uint64_t raw = f->roff[b + 1] - f->roff[b];
        if (v > raw + 1) { ix_free(X); return KOLMO_ERR_CORRUPT; }   /* a block holds at most raw line starts (+0) */
        X->lines[b] = v; X->row0[b] = r; r += v;
    }
    X->row0[X->nb] = r;
    if (r != X->rows) { ix_free(X); return KOLMO_ERR_CORRUPT; }
    if (X->has_filter) {
        if (L - i < 2 || p[i] != 3 || p[i + 1] < 1 || p[i + 1] > 16) { ix_free(X); return KOLMO_ERR_CORRUPT; }
        X->tokk = p[i + 1]; i += 2;
        for (uint32_t b = 0; b < X->nb; b++) {
            if (kx_getuv(p, (size_t)L, &i, &v) || v % 64 || v > (1ull << 32) || v / 8 > L - i) { ix_free(X); return KOLMO_ERR_CORRUPT; }
            X->m[b] = v; X->bm[b] = p + i; i += (size_t)(v / 8);
        }
    }
    if (i != L) { ix_free(X); return KOLMO_ERR_CORRUPT; }
    return 0;
}

int64_t kolmo_index_info(const uint8_t *src, size_t n, int *kind, uint8_t *delim, char *names, size_t cap) {
    *kind = 0; *delim = 0; if (names && cap) names[0] = 0;
    kx_frame f; int r = kx_frame_open(src, n, &f); if (r) return r;
    ix_index X; r = ix_load(src, n, &f, &X); kx_frame_close(&f);
    if (r == 1) return 0;
    if (r) return r;
    *kind = X.kind; *delim = X.delim;
    if (names && cap) {
        size_t i = 0, o = 0; uint64_t v;
        for (uint32_t j = 0; j < X.nnames; j++) {
            kx_getuv(X.names, X.nnames_bytes, &i, &v);
            if (o + v + 2 <= cap) { memcpy(names + o, X.names + i, (size_t)v); o += (size_t)v; names[o++] = '\n'; }
            i += (size_t)v;
        }
        names[o < cap ? o : cap - 1] = 0;
    }
    int64_t rows = (int64_t)X.rows; ix_free(&X);
    return rows;
}

/* ------------------------------------------------------------------ queries */
typedef struct {
    const uint8_t *src; size_t n; const kx_frame *f; const ix_index *X; const kolmo_query *q;
    int kind; uint8_t delim; uint32_t k;               /* resolved text kind; tables: delimiter and field count */
    int colmode;                                       /* 0 lines, 1 table field, 2 JSON key, 3 whitespace field */
    uint32_t col; const char *key; size_t keylen;
    int pat;                                           /* 0 none, 1 fixed, 2 regex */
    regex_t re; uint8_t *fixed; size_t fixlen;         /* fixed: the pattern (lower case with ICASE) */
    uint8_t *lit; size_t litlen;                       /* a literal every match contains (filter and prefilter) */
    uint64_t lo, hi;                                   /* rows [lo, hi) */
} ix_q;
typedef struct { kx_buf out; uint64_t rows, dec, stored; int err, columnar, decoded; } ix_res;

/* is the literal (lower case when ICASE) in the line? */
static const uint8_t *ix_find(const uint8_t *h, size_t hn, const uint8_t *nd, size_t nn, int icase) {
    if (!nn) return h;
    if (nn > hn) return NULL;
    if (!icase) return memmem(h, hn, nd, nn);
    uint8_t c0 = nd[0], c1 = (uint8_t)(c0 >= 'a' && c0 <= 'z' ? c0 - 32 : c0);
    for (size_t i = 0; i + nn <= hn; i++) {
        const uint8_t *a = memchr(h + i, c0, hn - nn + 1 - i), *b = c1 != c0 ? memchr(h + i, c1, hn - nn + 1 - i) : NULL;
        const uint8_t *x = !a ? b : !b ? a : a < b ? a : b; if (!x) return NULL;
        i = (size_t)(x - h); size_t j = 1; while (j < nn && ix_lc(h[i + j]) == nd[j]) j++;
        if (j == nn) return h + i;
    }
    return NULL;
}
static int ix_match(const ix_q *Q, const uint8_t *l, size_t n, kx_buf *tmp) {
    int icase = (Q->q->flags & KOLMO_Q_ICASE) != 0;
    if (Q->pat == 1 && !(Q->q->flags & KOLMO_Q_WORD)) return ix_find(l, n, Q->fixed, Q->fixlen, icase) != NULL;
    if (Q->pat == 1) {                                         /* a whole word: no word character on either side */
        for (size_t i = 0; i + Q->fixlen <= n;) {
            const uint8_t *h = ix_find(l + i, n - i, Q->fixed, Q->fixlen, icase); if (!h) return 0;
            size_t a = (size_t)(h - l), e = a + Q->fixlen;
            if ((!a || !(ix_alnum(l[a - 1]) || l[a - 1] == '_')) && (e >= n || !(ix_alnum(l[e]) || l[e] == '_'))) return 1;
            i = a + 1;
        }
        return 0;
    }
    if (Q->litlen && !ix_find(l, n, Q->lit, Q->litlen, icase)) return 0;
    tmp->n = 0; kx_bytes(tmp, l, n); kx_bytes(tmp, "", 1); if (tmp->oom) return -1;
    return regexec(&Q->re, (const char *)tmp->p, 0, NULL, 0) == 0;
}
/* the lines of R[0..rn) starting at global row r0: select, extract, write to res */
static void ix_lines(const ix_q *Q, const uint8_t *R, size_t rn, uint64_t r0, uint64_t expect, int check, int qhint, ix_res *res) {
    kx_buf tmp = {0}; uint64_t r = r0; size_t i = 0; int cnt = (Q->q->flags & KOLMO_Q_COUNT) != 0, ln = (Q->q->flags & KOLMO_Q_LINENO) != 0;
    /* quotes and brackets group fields with ',' and ';' (as the table transform does); a line without them splits the
     * same either way, so the rule holds line by line, whatever block the line is in */
    int quotes = Q->colmode == 1 && qhint && (Q->delim == ',' || Q->delim == ';');
    while (i < rn) {
        const uint8_t *e = memchr(R + i, '\n', rn - i); size_t end = e ? (size_t)(e - R) : rn, L = end - i;
        if (r >= Q->hi && !check) break;
        if (r >= Q->lo && r < Q->hi) {
            const uint8_t *l = R + i; int keep = 1;
            if (Q->pat) { keep = ix_match(Q, l, L, &tmp); if (keep < 0) { res->err = KOLMO_ERR_ALLOC; break; } }
            if (keep) {
                res->rows++;
                if (!cnt) {
                    if (ln) { char t[24]; int k = snprintf(t, sizeof t, "%llu:", (unsigned long long)r + 1); kx_bytes(&res->out, t, (size_t)k); }
                    size_t fo = 0, fl = L; int has = 1;
                    size_t LL = L && l[L - 1] == '\r' && Q->colmode ? L - 1 : L;
                    if (Q->colmode == 1) has = ix_table_field(l, L, Q->delim, quotes, Q->k, Q->col, &fo, &fl);
                    else if (Q->colmode == 2) has = ix_json_field(l, LL, Q->key, Q->keylen, &fo, &fl);
                    else if (Q->colmode == 3) has = ix_ws_field(l, LL, Q->col, &fo, &fl);
                    if (has) kx_bytes(&res->out, l + fo, fl);
                    kx_bytes(&res->out, "\n", 1);
                }
            }
        }
        r++; i = end + 1;
    }
    if (check && r - r0 != expect && !res->err) res->err = KOLMO_ERR_CORRUPT;   /* index and block disagree */
    if (res->out.oom && !res->err) res->err = KOLMO_ERR_ALLOC;
    free(tmp.p);
}
static uint8_t *ix_decode(const ix_q *Q, uint32_t b, ix_res *res, int *err) {
    uint64_t lo = Q->f->roff[b], hi = Q->f->roff[b + 1]; size_t len = (size_t)(hi - lo);
    uint8_t *d = malloc(len + 1); if (!d) { *err = KOLMO_ERR_ALLOC; return NULL; }
    if (len) { int64_t r = kolmo_read_range(Q->src, Q->n, lo, len, d, 1); if (r < 0) { free(d); *err = (int)r; return NULL; } }
    res->dec += len; res->stored += Q->f->poff[b + 1] - Q->f->poff[b]; res->decoded++;
    return d;
}
/* one block of an indexed file: its owned lines, through the column streams when that is possible */
static void ix_block(const ix_q *Q, uint32_t b, ix_res *res) {
    const ix_index *X = Q->X; uint64_t r0 = X->row0[b], L = X->lines[b]; uint8_t fl = X->flags[b];
    if (!L) return;
    int full = (fl & (IX_BF_START | IX_BF_END)) == (IX_BF_START | IX_BF_END);
    if (Q->colmode == 1 && !Q->pat && full) {
        const uint8_t *pay = Q->src + Q->f->poff[b];
        uint32_t k = Q->f->poff[b + 1] - Q->f->poff[b] >= 4 && pay[0] == IX_T_TBL ? (uint32_t)pay[3] + 1 : 0;
        uint8_t *c = NULL; size_t cn = 0; uint64_t dec = 0, rd = 0; int r = 1;
        if (k && Q->col < k) r = kolmo__block_column(Q->src, Q->n, b, (int)Q->col, &c, &cn, &dec, &rd);
        else if (k && pay[1] == Q->delim) r = 0;                     /* a table without this column: empty fields */
        if (r < 0) { res->err = r; return; }
        if (r == 0) {
            if (!c) {                                                /* every row prints an empty field */
                kx_buf e = {0}; for (uint64_t t = 0; t < L; t++) kx_bytes(&e, "\n", 1);
                if (e.oom) { res->err = KOLMO_ERR_ALLOC; return; }
                c = e.p; cn = e.n;
            }
            res->columnar++; res->dec += dec; res->stored += rd;
            ix_q Q2 = *Q; Q2.colmode = 0;                            /* the lines are the column already */
            ix_lines(&Q2, c, cn, r0, L, 1, 1, res); free(c); return;
        }
    }
    int err = 0; uint8_t *d = ix_decode(Q, b, res, &err); if (!d) { res->err = err; return; }
    size_t len = (size_t)(Q->f->roff[b + 1] - Q->f->roff[b]), s = 0;
    if (!(fl & IX_BF_START)) { const uint8_t *q = memchr(d, '\n', len); if (!q) { free(d); res->err = KOLMO_ERR_CORRUPT; return; } s = (size_t)(q - d) + 1; }
    kx_buf R = {0};
    const uint8_t *reg = d + s; size_t rn = len - s;
    if (!(fl & IX_BF_END)) {                                         /* the last line continues in the next blocks */
        kx_bytes(&R, reg, rn);
        for (uint32_t c = b + 1; c < Q->f->nb && !R.oom; c++) {
            uint8_t *e = ix_decode(Q, c, res, &err); if (!e) { free(d); free(R.p); res->err = err; return; }
            size_t el = (size_t)(Q->f->roff[c + 1] - Q->f->roff[c]); const uint8_t *nl = memchr(e, '\n', el);
            kx_bytes(&R, e, nl ? (size_t)(nl - e) + 1 : el); free(e);
            if (nl) break;
        }
        if (R.oom) { free(d); free(R.p); res->err = KOLMO_ERR_ALLOC; return; }
        reg = R.p; rn = R.n;
    }
    ix_lines(Q, reg, rn, r0, L, 1, 1, res);
    free(d); free(R.p);
}

typedef struct { ix_q *Q; const uint32_t *list; ix_res *res; uint32_t count, next; pthread_mutex_t mu; } ix_pool;
static void *ix_worker(void *arg) {
    ix_pool *P = arg;
    for (;;) {
        pthread_mutex_lock(&P->mu); uint32_t i = P->next++; pthread_mutex_unlock(&P->mu);
        if (i >= P->count) break;
        ix_block(P->Q, P->list[i], &P->res[i]);
    }
    return NULL;
}

typedef struct { ix_q *Q; uint32_t b0; uint8_t **d; ix_res *res; uint32_t count, next; pthread_mutex_t mu; } ix_dpool;
static void *ix_dworker(void *arg) {
    ix_dpool *P = arg;
    for (;;) {
        pthread_mutex_lock(&P->mu); uint32_t i = P->next++; pthread_mutex_unlock(&P->mu);
        if (i >= P->count) break;
        int err = 0; P->d[i] = ix_decode(P->Q, P->b0 + i, &P->res[i], &err); if (!P->d[i]) P->res[i].err = err;
    }
    return NULL;
}

/* the longest run of literal characters an extended regex requires (none with alternation or inside groups) */
static void ix_literal(const char *p, int icase, uint8_t **lit, size_t *ln) {
    *lit = NULL; *ln = 0;
    size_t n = strlen(p), bs = 0, bl = 0; uint8_t *cur = malloc(n + 1), *best = malloc(n + 1); size_t cl = 0; int depth = 0;
    if (!cur || !best) { free(cur); free(best); return; }
#define END_RUN do { if (cl > bl) { memcpy(best, cur, cl); bl = cl; } cl = 0; } while (0)
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c == '|') { free(cur); free(best); return; }
        if (c == '(') { END_RUN; depth++; continue; }
        if (c == ')') { END_RUN; if (depth) depth--; continue; }
        if (c == '[') {                                            /* bracket expression: skip it */
            END_RUN; i++; if (i < n && p[i] == '^') i++; if (i < n && p[i] == ']') i++;
            while (i < n && p[i] != ']') { if (p[i] == '[' && i + 1 < n && (p[i + 1] == ':' || p[i + 1] == '.' || p[i + 1] == '=')) {
                    const char *e = strstr(p + i + 2, p[i + 1] == ':' ? ":]" : p[i + 1] == '.' ? ".]" : "=]"); i = e ? (size_t)(e - p) + 1 : n; }
                i++; }
            continue;
        }
        if (c == '*' || c == '?' || c == '{') { if (cl) cl--; END_RUN; if (c == '{') { while (i < n && p[i] != '}') i++; } continue; }
        if (c == '+') { END_RUN; continue; }
        if (c == '.' || c == '^' || c == '$') { END_RUN; continue; }
        uint8_t lc;
        if (c == '\\') {
            if (i + 1 >= n) break;
            char e = p[++i];
            if (!strchr(".[]()*+?{}|^$\\/", e)) { END_RUN; continue; }
            lc = (uint8_t)e;
        } else lc = (uint8_t)c;
        /* the character just read is optional when a quantifier follows: handled when the quantifier is seen */
        if (depth) continue;
        cur[cl++] = icase ? ix_lc(lc) : lc;
    }
    END_RUN;
#undef END_RUN
    (void)bs; free(cur);
    if (bl) { *lit = best; *ln = bl; } else free(best);
}

int64_t kolmo_query_run(const uint8_t *src, size_t n, const kolmo_query *q, kolmo_sink out, void *ctx,
                        kolmo_query_stats *st) {
    kolmo_query_stats S0; if (!st) st = &S0; memset(st, 0, sizeof *st);
    if (!q) return KOLMO_ERR_QUERY;
    kx_frame f; int r = kx_frame_open(src, n, &f); if (r) return r;
    st->blocks = f.nb; st->bytes_total = f.orig;
    ix_index X; memset(&X, 0, sizeof X);
    int have = 0;
    if (!(q->flags & KOLMO_Q_NOINDEX)) { r = ix_load(src, n, &f, &X); if (r < 0) { kx_frame_close(&f); return r; } have = r == 0; }
    st->index_bytes = have ? X.size : 0;
    ix_q Q; memset(&Q, 0, sizeof Q); Q.src = src; Q.n = n; Q.f = &f; Q.X = &X; Q.q = q;
    int64_t ret = 0; uint8_t *first = NULL; size_t firstn = 0; ix_keys keys = {NULL, 0};
    int icase = (q->flags & KOLMO_Q_ICASE) != 0;
    /* the text kind: from the index, else from the first block */
    if (have) { Q.kind = X.kind; Q.delim = X.delim; Q.k = X.k ? X.k : 0xFFFFFFFFu; }
    else if (q->column && f.nb) {
        size_t len = (size_t)(f.roff[1] - f.roff[0]); first = malloc(len + 1);
        if (!first) { ret = KOLMO_ERR_ALLOC; goto done; }
        if (len && (ret = kolmo_read_range(src, n, 0, len, first, 1)) < 0) goto done;
        ret = 0; firstn = len; Q.kind = ix_detect(first, len, &Q.delim);
        if (Q.kind == KOLMO_IDX_TABLE) {                         /* fields of the first line, as the index counts them */
            const uint8_t *e = memchr(first, '\n', len); size_t ln = e ? (size_t)(e - first) : len, fo, fl;
            if (ln && first[ln - 1] == '\r') ln--;
            Q.k = 0; while (ix_table_field(first, ln, Q.delim, Q.delim == ',' || Q.delim == ';', 0xFFFFFFFFu, Q.k, &fo, &fl)) Q.k++;
            if (!Q.k) Q.k = 0xFFFFFFFFu; }
    }
    if (q->column) {
        const char *c = q->column;
        if (c[0] == '#') {
            char *e; unsigned long v = strtoul(c + 1, &e, 10);
            if (!c[1] || *e || v < 1 || v > 0xFFFFFFFFul) { ret = KOLMO_ERR_QUERY; goto done; }
            Q.col = (uint32_t)(v - 1); Q.colmode = Q.kind == KOLMO_IDX_TABLE ? 1 : 3;
            if (Q.kind == KOLMO_IDX_JSON) { ret = KOLMO_ERR_QUERY; goto done; }
        } else if (Q.kind == KOLMO_IDX_JSON) { Q.colmode = 2; Q.key = c; Q.keylen = strlen(c); }
        else if (Q.kind == KOLMO_IDX_TABLE) {
            int found = 0; size_t cl = strlen(c);
            if (have) {
                size_t i = 0; uint64_t v;
                for (uint32_t j = 0; j < X.nnames && !found; j++) {
                    kx_getuv(X.names, X.nnames_bytes, &i, &v);
                    if (v == cl && !memcmp(X.names + i, c, cl)) { Q.col = j; found = 1; }
                    i += (size_t)v;
                }
            } else {
                const uint8_t *e = memchr(first, '\n', firstn); size_t ln = e ? (size_t)(e - first) : firstn;
                if (ln && first[ln - 1] == '\r') ln--;
                int qt = Q.delim == ',' || Q.delim == ';';
                for (uint32_t j = 0; !found; j++) {
                    size_t fo, fl; if (!ix_table_field(first, ln, Q.delim, qt, 0xFFFFFFFFu, j, &fo, &fl)) break;
                    if (fl >= 2 && first[fo] == '"' && first[fo + fl - 1] == '"') { fo++; fl -= 2; }
                    if (fl == cl && !memcmp(first + fo, c, cl)) { Q.col = j; found = 1; }
                }
            }
            if (!found) { ret = KOLMO_ERR_QUERY; goto done; }
            Q.colmode = 1;
        } else { ret = KOLMO_ERR_QUERY; goto done; }
    }
    if (q->pattern) {
        if (q->flags & KOLMO_Q_FIXED) {
            Q.pat = 1; Q.fixlen = strlen(q->pattern); Q.fixed = malloc(Q.fixlen + 1);
            if (!Q.fixed) { ret = KOLMO_ERR_ALLOC; goto done; }
            for (size_t i = 0; i < Q.fixlen; i++) Q.fixed[i] = icase ? ix_lc((uint8_t)q->pattern[i]) : (uint8_t)q->pattern[i];
            Q.lit = Q.fixed; Q.litlen = Q.fixlen;
        } else {
            const char *rp = q->pattern; char *wrapped = NULL;
            if (q->flags & KOLMO_Q_WORD) {                           /* a whole word: no word character on either side */
                size_t pl = strlen(rp); wrapped = malloc(pl + 64); if (!wrapped) { ret = KOLMO_ERR_ALLOC; goto done; }
                snprintf(wrapped, pl + 64, "(^|[^[:alnum:]_])(%s)([^[:alnum:]_]|$)", rp); rp = wrapped;
            }
            int bad = regcomp(&Q.re, rp, REG_EXTENDED | REG_NOSUB | (icase ? REG_ICASE : 0)); free(wrapped);
            if (bad) { ret = KOLMO_ERR_QUERY; goto done; }
            Q.pat = 2; ix_literal(q->pattern, icase, &Q.lit, &Q.litlen);
        }
    }
    Q.lo = q->row_first; Q.hi = q->row_count ? q->row_first + q->row_count : UINT64_MAX;
    if (Q.hi < Q.lo) Q.hi = UINT64_MAX;
    int T = ix_threads(q->threads);
    if (have) {
        /* candidate blocks: owned rows meet the range, filter holds the literal */
        uint32_t *list = malloc(sizeof(uint32_t) * ((size_t)f.nb + 1)), cnt = 0;
        if (!list) { ret = KOLMO_ERR_ALLOC; goto done; }
        if (Q.pat && X.has_filter && Q.litlen && ix_query_keys(Q.lit, Q.litlen, Q.pat == 1 && (q->flags & KOLMO_Q_WORD), X.tokk, &keys)) { free(list); ret = KOLMO_ERR_ALLOC; goto done; }
        for (uint32_t b = 0; b < f.nb; b++) {
            if (!X.lines[b] || X.row0[b] + X.lines[b] <= Q.lo || X.row0[b] >= Q.hi) continue;
            if (Q.pat && X.has_filter && keys.n && !ix_filter_has(X.bm[b], X.m[b], &keys)) { st->blocks_skipped_filter++; continue; }
            list[cnt++] = b;
        }
        /* waves of a few blocks per thread: decoded in parallel, written in order */
        uint32_t wave = (uint32_t)T * 2;
        for (uint32_t w0 = 0; w0 < cnt && !ret; w0 += wave) {
            uint32_t wn = cnt - w0 < wave ? cnt - w0 : wave;
            ix_res *res = calloc(wn, sizeof(ix_res)); if (!res) { ret = KOLMO_ERR_ALLOC; break; }
            ix_pool P = {&Q, list + w0, res, wn, 0, PTHREAD_MUTEX_INITIALIZER};
            pthread_t th[64]; int started = 0, want = T < (int)wn ? T : (int)wn;
            for (int t = 1; t < want; t++) if (!pthread_create(&th[started], NULL, ix_worker, &P)) started++;
            ix_worker(&P);
            for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
            for (uint32_t i = 0; i < wn; i++) {
                if (res[i].err && !ret) ret = res[i].err;
                st->bytes_decoded += res[i].dec; st->stored_read += res[i].stored; st->rows_out += res[i].rows;
                st->blocks_decoded += (uint32_t)res[i].decoded; st->blocks_columnar += (uint32_t)res[i].columnar;
                if (!ret && res[i].out.n && out && out(ctx, res[i].out.p, res[i].out.n)) ret = KOLMO_ERR_IO;
                free(res[i].out.p);
            }
            free(res);
        }
        free(list);
    } else {
        /* no index: every block in order, decoded a wave at a time; lines carried across block ends */
        kx_buf carry = {0}; uint64_t row = 0;
        for (uint32_t b = 0; b < f.nb && !ret; ) {
            uint32_t wn = f.nb - b < (uint32_t)T ? f.nb - b : (uint32_t)T;
            uint8_t **d = calloc(wn, sizeof(uint8_t *)); ix_res *res = calloc(wn, sizeof(ix_res));
            if (!d || !res) { free(d); free(res); ret = KOLMO_ERR_ALLOC; break; }
            ix_dpool DP = {&Q, b, d, res, wn, 0, PTHREAD_MUTEX_INITIALIZER};
            {   pthread_t th[64]; int started = 0;
                for (uint32_t t = 1; t < wn; t++) if (!pthread_create(&th[started], NULL, ix_dworker, &DP)) started++;
                ix_dworker(&DP);
                for (int t = 0; t < started; t++) pthread_join(th[t], NULL); }
            for (uint32_t i = 0; i < wn && !ret; i++) {
                if (res[i].err) { ret = res[i].err; break; }
                st->bytes_decoded += res[i].dec; st->stored_read += res[i].stored; st->blocks_decoded++;
                size_t len = (size_t)(f.roff[b + i + 1] - f.roff[b + i]);
                const uint8_t *last = len ? ix_memrchr(d[i], '\n', len) : NULL;
                int final = b + i + 1 == f.nb;
                size_t cut = final ? len : last ? (size_t)(last - d[i]) + 1 : 0;
                kx_bytes(&carry, d[i], cut);
                if (carry.oom) { ret = KOLMO_ERR_ALLOC; break; }
                if (carry.n && (cut || final)) {
                    ix_res one = {0}; uint64_t nl = 0;
                    for (const uint8_t *p = carry.p, *e = carry.p + carry.n; (p = memchr(p, '\n', (size_t)(e - p))); p++) nl++;
                    ix_lines(&Q, carry.p, carry.n, row, 0, 0, 1, &one);
                    row += nl + (carry.p[carry.n - 1] != '\n');
                    st->rows_out += one.rows;
                    if (one.err) ret = one.err;
                    else if (one.out.n && out && out(ctx, one.out.p, one.out.n)) ret = KOLMO_ERR_IO;
                    free(one.out.p); carry.n = 0;
                }
                kx_bytes(&carry, d[i] + cut, len - cut);
            }
            for (uint32_t i = 0; i < wn; i++) free(d[i]);
            free(d); free(res); b += wn;
            if (row >= Q.hi) break;
        }
        free(carry.p);
    }
    if (!ret) ret = (int64_t)st->rows_out;
done:
    if (Q.pat == 2) regfree(&Q.re);
    if (Q.lit != Q.fixed) free(Q.lit);
    free(Q.fixed); free(first); free(keys.h);
    if (have) ix_free(&X);
    kx_frame_close(&f);
    return ret;
}
