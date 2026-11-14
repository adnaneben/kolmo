/* P2-E6: type-first MIXED CSV pipeline (research only; development). Includes the frozen E02C harness (main renamed),
 * production src/ through it, and P2-E2's fastint.inc. Extends P2-E5 from all-integer tables to typed columns:
 *
 * Exact typing inside the timed encode. File: first line = header (raw bytes), then >= 16 rows, each with the same
 * number k of comma-separated fields ending in '\n'; the file ends right after a '\n'; no '"', '\r' or NUL anywhere
 * after the header (quoted/CRLF/binary CSV is opaque: whole-input zstd at the level, scan charged). Each column is:
 *   INT  every field is a canonical int64 decimal  -?(0|[1-9][0-9]*), no "-0";
 *   DEC  every field is a canonical fixed-point decimal with the same scale d (1..18): -?(0|[1-9][0-9]*)\.[0-9]{d},
 *        mantissa in int64, no negative zero mantissa; stored as the int64 mantissa (exact, no binary float);
 *   TEXT otherwise (including empty fields): its field bytes, each followed by '\n', in one shared text stream.
 * INT/DEC columns go through fi_forward (byte-identical to production's INT program) + an E02C leaf; when there are
 * >= 16 numeric columns and < 4096 rows they form one column-major stream (P2-E5 rule). The text stream gets the
 * E02C production-leaf rule. The original's production block checksum is stored and verified.
 * Container: flag u8 (0 opaque, 1 typed), uv n, u32 checksum; flag 1: uv header length, header, uv k, uv rows,
 * k type bytes (0 INT, 1..18 DEC scale, 255 TEXT), u8 layout, numeric streams (uv plen, par, uv blob length, blob),
 * then uv text-blob length, text blob (present only if a TEXT column exists). */
#define main e02c_harness_main
#include "../E02C-harness/harness/e02c.c"
#undef main
#include "../P2-E2/fastint.inc"

typedef struct { const char *name; char kind; int level; const char *leaf; const char *tleaf; } cand_t;
static const cand_t CAND[] = {
    {"zstd3", 'Z', 3, NULL, NULL}, {"mcsv_planes_huf", 'M', 3, "planes+huf", "exist+prod3"},
    {"mcsv_prod3", 'M', 3, "exist+prod3", "exist+prod3"}, {"kolmo_fast", 'K', 3, NULL, NULL}, {"kolmo_fast_Q", 'Q', 3, NULL, NULL},
    {"zstd19", 'Z', 19, NULL, NULL}, {"mcsv_prod19", 'M', 19, "planes+prod19", "exist+prod19"}, {"kolmo_hc", 'H', 19, NULL, NULL},
};
#define NC ((int)(sizeof CAND / sizeof CAND[0]))
static int conf_by_name(const char *want) { char nb[32]; for (int c = 0; c < NCONF; c++) if (!strcmp(conf_name(c, nb), want)) return c; DIE("no config %s", want); }
static int CONF[NC], TCONF[NC];
static uint8_t *put_uv(uint8_t *q, uint64_t v) { while (v >= 128) { *q++ = (uint8_t)(v | 128); v >>= 7; } *q++ = (uint8_t)v; return q; }
static const uint64_t MP10[19] = {1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull, 10000000ull, 100000000ull, 1000000000ull,
    10000000000ull, 100000000000ull, 1000000000000ull, 10000000000000ull, 100000000000000ull, 1000000000000000ull,
    10000000000000000ull, 100000000000000000ull, 1000000000000000000ull};

/* parse one field as canonical INT (scale 0) or DEC (scale d); returns scale or -1 */
static int num_field(const uint8_t *p, size_t l, int64_t *out) {
    size_t i = 0; int neg = 0; if (i < l && p[i] == '-') { neg = 1; i++; }
    size_t st = i; uint64_t v = 0;
    while (i < l && (unsigned)(p[i] - '0') < 10) { if (i - st >= 19) return -1; v = v * 10 + (uint64_t)(p[i] - '0'); i++; }
    if (i == st || (p[st] == '0' && i - st > 1)) return -1;
    int d = 0;
    if (i < l) {
        if (p[i] != '.') return -1; i++; size_t fs = i;
        while (i < l && (unsigned)(p[i] - '0') < 10) i++;
        d = (int)(i - fs); if (i != l || d < 1 || d > 18) return -1;
        if (i - st - 1 > 19) return -1;
        for (size_t q = fs; q < l; q++) { if (v > (UINT64_MAX - 9) / 10) return -1; v = v * 10 + (uint64_t)(p[q] - '0'); }
    }
    if ((neg && v == 0) || (neg ? v > (uint64_t)INT64_MAX + 1 : v > (uint64_t)INT64_MAX)) return -1;
    *out = neg ? (int64_t)(0 - v) : (int64_t)v; return d;
}
/* SWAR: number of leading ASCII digits in the 8 bytes at p (little-endian load; carries/borrows only move upward, so
 * the lowest non-digit byte is found exactly), and their value. Caller guarantees 8 readable bytes. */
static inline int mcsv_swar8(const uint8_t *p, uint64_t *val) {
    uint64_t x; memcpy(&x, p, 8);
    uint64_t m = ((x + 0x4646464646464646ull) | (x - 0x3030303030303030ull)) & 0x8080808080808080ull;
    int k = m ? __builtin_ctzll(m) >> 3 : 8;
    if (!k) { *val = 0; return 0; }
    if (k < 8) { int sh = 8 * (8 - k); x = (x << sh) | (0x3030303030303030ull >> (64 - sh)); }
    x -= 0x3030303030303030ull; x = x * 10 + (x >> 8);
    x = (((x & 0x000000FF000000FFull) * (100 + (1000000ull << 32))) + (((x >> 16) & 0x000000FF000000FFull) * (1 + (10000ull << 32)))) >> 32;
    *val = x; return k;
}
static const uint64_t SW10[9] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
/* digit run starting at s[i] (up to lim digits): advances i, returns the count, accumulates into *u (no overflow for
 * <= 19 digits since the caller bounds the total) */
static inline size_t digits_run(const uint8_t *s, size_t n, size_t *i, uint64_t *u, size_t lim) {
    size_t c = 0;
    while (c < lim) {
        if (*i + 8 <= n) { uint64_t v; int k = mcsv_swar8(s + *i, &v); if ((size_t)k > lim - c) break;
                           *u = *u * SW10[k] + v; *i += (size_t)k; c += (size_t)k; if (k < 8) return c; }
        else { if (*i < n && (unsigned)(s[*i] - '0') < 10) { *u = *u * 10 + (uint64_t)(s[*i] - '0'); (*i)++; c++; } else return c; }
    }
    while (c < lim && *i < n && (unsigned)(s[*i] - '0') < 10) { *u = *u * 10 + (uint64_t)(s[*i] - '0'); (*i)++; c++; }   /* finish */
    return c;
}
typedef struct { size_t hl, k, rows; uint32_t *roff; uint8_t *type; int64_t *num; size_t nnum; size_t *nidx; } table_t;
static void tfree(table_t *T) { free(T->roff); free(T->type); free(T->num); free(T->nidx); memset(T, 0, sizeof *T); }
/* single pass: structure + per-column running type (scale, or 255 = TEXT once any field disagrees); numeric values
 * land in a provisional k-column buffer; columns demoted to TEXT are dropped at the end. Row start offsets (one per row)
 * let the encoder re-walk TEXT fields. */
static int mcsv_type(const uint8_t *s, size_t n, table_t *T) {
    memset(T, 0, sizeof *T);
    if (n > 0xFFFFFFF0u) return -1;
    const uint8_t *e = memchr(s, '\n', n); if (!e) return -1;
    size_t i = (size_t)(e - s) + 1, k = 1; T->hl = i;
    { const uint8_t *e2 = memchr(s + i, '\n', n - i); if (!e2) return -1; for (const uint8_t *q = s + i; q < e2; q++) k += *q == ','; }
    if (k > 4096) return -1;
    size_t cap = (n - i) / (2 * k) + 2, rows = 0;
    T->roff = malloc((cap + 1) * sizeof(uint32_t)); T->type = malloc(k); int64_t *v = malloc(cap * k * sizeof(int64_t)); uint8_t *seen = calloc(k, 1);
    if (!T->roff || !T->type || !v || !seen) { free(v); free(seen); goto bad; }
    while (i < n) {
        if (rows == cap) { free(v); free(seen); goto bad; }
        T->roff[rows] = (uint32_t)i;
        for (size_t j = 0; j < k; j++) {
            size_t a = i;
            if (T->type[j] != 255 || !seen[j]) {                         /* numeric fast path while the column is numeric */
                int neg = 0; uint64_t u = 0; size_t st, nd;
                if (i < n && s[i] == '-') { neg = 1; i++; }
                st = i;
                digits_run(s, n, &i, &u, 18);
                int ok = i > st && !(s[st] == '0' && i - st > 1), sc = 0; nd = i - st;
                if (ok && i < n && (unsigned)(s[i] - '0') < 10) { u = u * 10 + (uint64_t)(s[i] - '0'); i++; nd++;       /* 19th digit: fits u64 */
                                                                  if (i < n && (unsigned)(s[i] - '0') < 10) ok = 0; }   /* 20+: TEXT */
                if (ok && i < n && s[i] == '.') {
                    i++; size_t fs = i;
                    while (ok && i < n && (unsigned)(s[i] - '0') < 10) {
                        if (nd >= 19 || u > (UINT64_MAX - 9) / 10) ok = 0; else { u = u * 10 + (uint64_t)(s[i] - '0'); i++; nd++; } }
                    sc = (int)(i - fs); if (sc < 1 || sc > 18) ok = 0;
                }
                if (ok && (neg ? u > (uint64_t)INT64_MAX + 1 : u > (uint64_t)INT64_MAX)) ok = 0;
                if (ok && neg && u == 0) ok = 0;
                if (ok && (i >= n || (s[i] != ',' && s[i] != '\n'))) ok = 0;
                if (ok && seen[j] && T->type[j] != sc) ok = 0;
                if (ok) { T->type[j] = (uint8_t)sc; seen[j] = 1; v[j * cap + rows] = (int64_t)(neg ? (uint64_t)0 - u : u); }
                else { T->type[j] = 255; seen[j] = 1; i = a; }
            }
            while (i < n && s[i] != ',' && s[i] != '\n') { if (s[i] == '"' || s[i] == '\r' || !s[i]) { free(v); free(seen); goto bad; } i++; }
            if (i >= n || s[i] != (j + 1 < k ? ',' : '\n')) { free(v); free(seen); goto bad; }
            i++;
        }
        rows++;
    }
    free(seen);
    if (rows < 16) { free(v); goto bad; }
    T->roff[rows] = (uint32_t)n; T->k = k; T->rows = rows;
    T->nidx = malloc(k * sizeof(size_t)); if (!T->nidx) { free(v); goto bad; }
    size_t nn = 0;
    for (size_t j = 0; j < k; j++) if (T->type[j] != 255) { if (nn != j || cap != rows) memmove(v + nn * rows, v + j * cap, rows * sizeof(int64_t)); T->nidx[j] = nn++; }
    T->num = v; T->nnum = nn; return 0;
bad:
    tfree(T); return -1;
}
/* bytes of field j of row r (TEXT columns only; walks the row) */
static inline const uint8_t *field_at(const uint8_t *s, const table_t *T, size_t r, size_t j, size_t *len) {
    const uint8_t *p = s + T->roff[r]; for (size_t q = 0; q < j; q++) p = (const uint8_t *)memchr(p, ',', s + T->roff[r + 1] - p) + 1;
    const uint8_t *e = p; while (*e != ',' && *e != '\n') e++; *len = (size_t)(e - p); return p;
}
static size_t enc(const uint8_t *src, size_t n, int ci, uint8_t **dst) {
    const cand_t *c = &CAND[ci];
    if (c->kind == 'Z') {
        size_t cap = ZSTD_compressBound(n); uint8_t *d = malloc(cap); ZSTD_CCtx *z = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(z, ZSTD_c_compressionLevel, c->level); ZSTD_CCtx_setParameter(z, ZSTD_c_nbWorkers, 0);
        size_t r = ZSTD_compress2(z, d, cap, src, n); if (ZSTD_isError(r)) DIE("zstd"); ZSTD_freeCCtx(z); *dst = d; return r;
    }
    if (c->kind == 'K' || c->kind == 'Q' || c->kind == 'H') {
        kolmo_params p = {c->kind == 'H' ? KOLMO_HC : KOLMO_FAST, 0, 1, 0, c->kind == 'Q' ? 3 : 1};
        size_t cap = kolmo_bound(n, &p); uint8_t *f = malloc(cap); int64_t fn = kolmo_compress(src, n, f, cap, &p); if (fn <= 0) DIE("kolmo"); *dst = f; return (size_t)fn;
    }
    size_t cap = n + n / 4 + ZSTD_compressBound(n) + 65536; uint8_t *d = malloc(cap), *q = d; table_t T; codec_ctx x = {0};
    *q++ = 0; q = put_uv(q, n); put32(q, blk_sum(2, src, n)); q += 4;
    if (!mcsv_type(src, n, &T)) {
        uint8_t *s = q; s = put_uv(s, T.hl); memcpy(s, src, T.hl); s += T.hl; s = put_uv(s, T.k); s = put_uv(s, T.rows);
        memcpy(s, T.type, T.k); s += T.k;
        int cat = T.nnum >= 16 && T.rows < 4096; *s++ = (uint8_t)cat; int ok = 1;
        size_t ns = cat ? (T.nnum ? 1 : 0) : T.nnum, len = cat ? T.nnum * T.rows : T.rows;
        for (size_t j = 0; j < ns && ok; j++) {
            fiprog P; kbuf blob = {0};
            if (fi_forward((const uint8_t *)(T.num + j * T.rows), len * 8, 8, 0, &P) || conf_encode(&x, CONF[ci], P.res.p, P.m, P.ow, P.pln, &blob, NULL)) ok = 0;
            else if ((size_t)(s - d) + P.par.n + blob.n + 32 > cap) ok = 0;
            else { s = put_uv(s, P.par.n); memcpy(s, P.par.p, P.par.n); s += P.par.n; s = put_uv(s, blob.n); memcpy(s, blob.p, blob.n); s += blob.n; }
            fi_free(&P); free(blob.p);
        }
        if (ok && T.nnum < T.k) {                                   /* text stream: TEXT fields, column-major, '\n'-ended */
            kbuf ts = {0}, tb = {0};
            for (size_t j = 0; j < T.k && ok; j++) if (T.type[j] == 255) for (size_t r = 0; r < T.rows; r++) {
                size_t l; const uint8_t *a = field_at(src, &T, r, j, &l);
                if (kb_put(&ts, a, l) || kb_byte(&ts, '\n')) ok = 0;
            }
            if (ok && (conf_encode(&x, TCONF[ci], ts.p, ts.n, 1, 0, &tb, NULL) || (size_t)(s - d) + tb.n + 16 > cap)) ok = 0;
            if (ok) { s = put_uv(s, ts.n); s = put_uv(s, tb.n); memcpy(s, tb.p, tb.n); s += tb.n; }
            free(ts.p); free(tb.p);
        }
        tfree(&T);
        if (ok) { d[0] = 1; codec_ctx_free(&x); *dst = d; return (size_t)(s - d); }
    }
    size_t r = ZSTD_compress(q, cap - (size_t)(q - d), src, n, c->level); if (ZSTD_isError(r)) DIE("zstd");
    codec_ctx_free(&x); *dst = d; return (size_t)(q - d) + r;
}
static inline int ndig(uint64_t u) {
    if (u < 100000000ull) return u < 10000 ? (u < 100 ? 1 + (u >= 10) : 3 + (u >= 1000)) : (u < 1000000 ? 5 + (u >= 100000) : 7 + (u >= 10000000));
    int d = 9; for (uint64_t t = 1000000000ull; d < 20 && u >= t; d++) { if (t > UINT64_MAX / 10) { d++; break; } t *= 10; } return d;
}
static inline uint8_t *utoa_w(uint8_t *o, uint64_t u, int d) {    /* exactly d digits (zero-padded), two per step */
    uint8_t *e = o + d, *q = e;
    while (q - o >= 2) { unsigned r = (unsigned)(u % 100); u /= 100; q -= 2; q[0] = (uint8_t)KG_D2[2 * r]; q[1] = (uint8_t)KG_D2[2 * r + 1]; }
    if (q > o) *--q = (uint8_t)('0' + u % 10);
    return e;
}
static inline uint8_t *num_print(uint8_t *o, int64_t v, int sc) {
    uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v; if (v < 0) *o++ = '-';
    if (!sc) return utoa_w(o, u, ndig(u));
    uint64_t ip = u / MP10[sc], fp = u % MP10[sc];
    o = utoa_w(o, ip, ndig(ip)); *o++ = '.'; return utoa_w(o, fp, sc);
}
static int dec(const uint8_t *f, size_t fn, int ci, uint8_t *out, size_t n) {
    const cand_t *c = &CAND[ci];
    if (c->kind == 'Z') { ZSTD_DCtx *z = ZSTD_createDCtx(); size_t r = ZSTD_decompressDCtx(z, out, n + 64, f, fn); ZSTD_freeDCtx(z); return ZSTD_isError(r) || r != n; }
    if (c->kind == 'K' || c->kind == 'Q' || c->kind == 'H') return kolmo_decompress(f, fn, out, n + 64, 1) != (int64_t)n;
    size_t i = 1; uint64_t len, hl, k, rows; int rc = -1; codec_ctx x = {0}; int64_t *num = NULL; kbuf txt = {0};
    const uint8_t *ty = NULL; size_t *nidx = NULL;
    if (fn < 6 || kg_getuv(f, fn, &i, &len) || len != n || i + 4 > fn) return -1;
    uint32_t sum = get32(f + i); i += 4;
    if (f[0] != 1) { size_t r = ZSTD_decompress(out, n + 64, f + i, fn - i); rc = ZSTD_isError(r) || r != n; goto done; }
    if (kg_getuv(f, fn, &i, &hl) || hl > fn - i || hl > n) goto done;
    memcpy(out, f + i, hl); i += hl;
    if (kg_getuv(f, fn, &i, &k) || kg_getuv(f, fn, &i, &rows) || !k || k > 4096 || rows > n || k * rows > n || k > fn - i) goto done;
    ty = f + i; i += k; if (i >= fn || f[i] > 1) goto done; int cat = f[i++];
    size_t nnum = 0, ntxt = 0; nidx = malloc(k * sizeof(size_t)); if (!nidx) goto done;
    for (size_t j = 0; j < k; j++) { if (ty[j] == 255) ntxt++; else if (ty[j] <= 18) nidx[j] = nnum++; else goto done; }
    if (cat != (nnum >= 16 && rows < 4096)) goto done;
    num = malloc((nnum * rows + 1) * sizeof(int64_t)); if (!num) goto done;
    { size_t ns = cat ? (nnum ? 1 : 0) : nnum, slen = cat ? nnum * rows : rows;
      for (size_t j = 0; j < ns; j++) {
        uint64_t pl, bl; static uint8_t empty[1]; kbuf res = {0}, o = {0}, rest = {empty, 0, 0};
        if (kg_getuv(f, fn, &i, &pl) || pl > fn - i || pl < 3) goto done; const uint8_t *par = f + i; i += pl;
        if (kg_getuv(f, fn, &i, &bl) || bl > fn - i) goto done;
        size_t pi = 3; uint64_t hh, cnt; if (kg_getuv(par, pl, &pi, &hh) || kg_getuv(par, pl, &pi, &cnt) || cnt != slen || hh) goto done;
        int ord = (par[1] >> 1) & 3, ow = par[2], pln = (par[1] >> 3) & 1; if ((uint64_t)ord > cnt) goto done;
        int bad = e2c_decode(&x, f + i, (size_t)bl, (size_t)(cnt - (uint64_t)ord), ow, pln, slen * 8 * 4 + 1024, &res)
                  || fi_inverse(par, pl, &res, &rest, slen * 8 + 1024, &o) || o.n != slen * 8;
        if (!bad) memcpy(num + j * rows, o.p, slen * 8);
        free(res.p); free(o.p); if (bad) goto done; i += bl;
      } }
    if (ntxt) {
        uint64_t tl, bl; if (kg_getuv(f, fn, &i, &tl) || kg_getuv(f, fn, &i, &bl) || bl > fn - i || tl > n) goto done;
        if (e2c_decode(&x, f + i, (size_t)bl, (size_t)tl, 1, 0, n + 1024, &txt) || txt.n != tl) goto done; i += bl;
    }
    if (i != fn) goto done;
    {   uint8_t *o = out + hl, *end = out + n; size_t *tp = NULL, tpos = 0;
        if (ntxt) { tp = malloc(k * sizeof(size_t)); if (!tp) goto done;      /* start of each TEXT column in txt */
                    for (size_t j = 0; j < k; j++) if (ty[j] == 255) { tp[j] = tpos; for (size_t r = 0; r < rows; r++) { const uint8_t *e = memchr(txt.p + tpos, '\n', txt.n - tpos); if (!e) { free(tp); goto done; } tpos = (size_t)(e - txt.p) + 1; } }
                    if (tpos != txt.n) { free(tp); goto done; } }
        for (size_t r = 0; r < rows; r++) for (size_t j = 0; j < k; j++) {
            if (o > end) { free(tp); goto done; }
            if (ty[j] == 255) { const uint8_t *a = txt.p + tp[j], *e = memchr(a, '\n', txt.n - tp[j]); size_t l = (size_t)(e - a);
                                if ((size_t)(end - o) < l + 1) { free(tp); goto done; } memcpy(o, a, l); o += l; tp[j] += l + 1; }
            else o = num_print(o, num[nidx[j] * rows + r], ty[j]);
            *o++ = j + 1 < k ? ',' : '\n';
        }
        free(tp); if (o != end) goto done; }
    rc = 0;
done:
    free(num); free(txt.p); free(nidx); codec_ctx_free(&x);
    return rc || blk_sum(2, out, n) != sum;
}

int main(int argc, char **argv) {
    { uint16_t one = 1; if (*(uint8_t *)&one != 1) DIE("little-endian host required"); }
    if (argc < 4) DIE("usage: p2e6 run FILE TAG ROUNDS SEED | p2e6 mem FILE CANDIDATE");
    for (int c = 0; c < NC; c++) { CONF[c] = CAND[c].leaf ? conf_by_name(CAND[c].leaf) : -1; TCONF[c] = CAND[c].tleaf ? conf_by_name(CAND[c].tleaf) : -1; }
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("read");
    uint8_t *out = malloc(n + 64);
    if (!strcmp(argv[1], "types")) {                 /* typing decisions only (for comparison with an independent oracle) */
        table_t T; if (mcsv_type(src, n, &T)) { printf("opaque\n"); free(out); free(src); return 0; }
        for (size_t j = 0; j < T.k; j++) { if (T.type[j] == 255) printf("T"); else if (!T.type[j]) printf("I"); else printf("D%d", T.type[j]); }
        printf("\n"); tfree(&T); free(out); free(src); return 0;
    }
    if (!strcmp(argv[1], "mem")) {
        int ci = -1; for (int c = 0; c < NC; c++) if (!strcmp(argv[3], CAND[c].name)) ci = c; if (ci < 0) DIE("candidate");
        if (hwm_reset()) DIE("clear_refs");
        uint8_t *f; size_t fn = enc(src, n, ci, &f); memset(out, 0, n); if (dec(f, fn, ci, out, n) || memcmp(out, src, n)) DIE("mem verify");
        printf("{\"kind\":\"p2e6_mem\",\"input\":\"%s\",\"candidate\":\"%s\",\"stored\":%zu,\"vmhwm_kb\":%ld}\n", argv[2], CAND[ci].name, fn, status_kb("VmHWM:"));
        free(f); free(out); free(src); return 0;
    }
    const char *tag = argv[3]; int rounds = argc > 4 ? atoi(argv[4]) : 5; xs = (uint64_t)(argc > 5 ? atoll(argv[5]) : 1) * 2654435761ull + 7;
    uint8_t *F[NC]; size_t FN[NC]; int Be[NC], Bd[NC], ord[NC];
    for (int c = 0; c < NC; c++) {
        FN[c] = enc(src, n, c, &F[c]); uint8_t *f2; size_t f2n = enc(src, n, c, &f2);
        if (f2n != FN[c] || memcmp(f2, F[c], FN[c])) DIE("%s not deterministic", CAND[c].name); free(f2);
        memset(out, 0, n); if (dec(F[c], FN[c], c, out, n) || memcmp(out, src, n)) DIE("%s does not reconstruct", CAND[c].name);
        printf("{\"kind\":\"p2e6_size\",\"input\":\"%s\",\"bytes\":%zu,\"candidate\":\"%s\",\"stored\":%zu,\"typed_table\":%d,\"exact\":true}\n",
               tag, n, CAND[c].name, FN[c], CAND[c].kind == 'M' ? F[c][0] == 1 : -1);
        uint64_t t0 = ns_of(CLOCK_MONOTONIC); { uint8_t *t; enc(src, n, c, &t); free(t); } uint64_t t1 = ns_of(CLOCK_MONOTONIC); dec(F[c], FN[c], c, out, n); uint64_t t2 = ns_of(CLOCK_MONOTONIC);
        uint64_t te = t1 - t0 ? t1 - t0 : 1, td = t2 - t1 ? t2 - t1 : 1;
        Be[c] = te >= 20000000 ? 1 : (int)(20000000 / te) + 1; Bd[c] = td >= 20000000 ? 1 : (int)(20000000 / td) + 1;
        if (Be[c] > 500) Be[c] = 500; if (Bd[c] > 500) Bd[c] = 500;
    }
    for (int r = 0; r < rounds; r++) {
        for (int c = 0; c < NC; c++) ord[c] = (c + r) % NC;
        shuffle(ord, NC);
        for (int oi = 0; oi < NC; oi++) {
            int c = ord[oi]; uint64_t w0, c0, w1, c1, w2, c2;
            w0 = ns_of(CLOCK_MONOTONIC); c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Be[c]; it++) { uint8_t *t; if (enc(src, n, c, &t) != FN[c]) DIE("encode drift"); free(t); }
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            double ew = (double)(w1 - w0) / Be[c], ec = (double)(c1 - c0) / Be[c];
            memset(out, 0, n);
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Bd[c]; it++) if (dec(F[c], FN[c], c, out, n)) DIE("decode");
            w2 = ns_of(CLOCK_MONOTONIC); c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            if (memcmp(out, src, n)) DIE("timed decode mismatch %s", CAND[c].name);
            printf("{\"kind\":\"p2e6_time\",\"input\":\"%s\",\"candidate\":\"%s\",\"pair_id\":\"r%d\",\"slot\":%d,\"enc_reps\":%d,\"dec_reps\":%d,"
                   "\"encode_wall_s\":%.9f,\"encode_cpu_s\":%.9f,\"decode_wall_s\":%.9f,\"decode_cpu_s\":%.9f}\n",
                   tag, CAND[c].name, r, oi, Be[c], Bd[c], ew * 1e-9, ec * 1e-9, (double)(w2 - w1) / Bd[c] * 1e-9, (double)(c2 - c1) / Bd[c] * 1e-9);
        }
    }
    for (int c = 0; c < NC; c++) free(F[c]);
    free(out); free(src); return 0;
}
