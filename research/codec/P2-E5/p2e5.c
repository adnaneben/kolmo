/* P2-E5: type-first canonical-integer CSV pipeline (research only; development). Includes the frozen E02C harness
 * (main renamed), production src/ through it, and P2-E2's fastint.inc.
 *
 * Typing (inside the timed encode, exact): the input is accepted as a typed table only if it is
 *   header line (any bytes except '\n') '\n', then >= 16 rows, each of k >= 1 comma-separated fields, each field the
 *   canonical decimal spelling of an int64 (-?(0|[1-9][0-9]*), no "-0", in range), each row ending in '\n', and the
 *   file ending right after the last '\n'. Any deviation: opaque (whole-input zstd at the level; the scan is charged).
 * Canonical spelling makes reconstruction exact by construction; the original's production block checksum is also
 * stored and verified. Each column (int64 little-endian values) goes through fi_forward (byte-identical to
 * production's INT program) and an E02C residual leaf; a column whose typed coding is not smaller than zstd on its own
 * 8-byte values... is not special-cased: every column takes the same path (fixed pipeline, no trials).
 * Container: flag u8 (0 opaque zstd, 1 typed table), uv n, u32 checksum; flag 1: uv header length, header,
 * uv k, uv rows, then per column uv plen, par, uv blob length, blob.
 * Kinds: Z zstd (rival at level 3), C typed CSV with the named leaf. */
#define main e02c_harness_main
#include "../E02C-harness/harness/e02c.c"
#undef main
#include "../P2-E2/fastint.inc"

typedef struct { const char *name; char kind; int level; const char *leaf; } cand_t;
static const cand_t CAND[] = {
    {"zstd3", 'Z', 3, NULL}, {"csv_planes_huf", 'C', 3, "planes+huf"}, {"csv_huf", 'C', 3, "exist+huf"},
    {"csv_prod3", 'C', 3, "exist+prod3"}, {"kolmo_fast", 'K', 3, NULL}, {"kolmo_fast_Q", 'Q', 3, NULL},
    {"csv_planes_huf_lay", 'L', 3, "planes+huf"}, {"csv_prod3_lay", 'L', 3, "exist+prod3"},
    {"zstd19", 'Z', 19, NULL}, {"csv_planes_prod19", 'C', 19, "planes+prod19"}, {"kolmo_hc", 'H', 19, NULL},
};
#define NC ((int)(sizeof CAND / sizeof CAND[0]))
static int conf_by_name(const char *want) { char nb[32]; for (int c = 0; c < NCONF; c++) if (!strcmp(conf_name(c, nb), want)) return c; DIE("no config %s", want); }
static int CONF[NC];
static uint8_t *put_uv(uint8_t *q, uint64_t v) { while (v >= 128) { *q++ = (uint8_t)(v | 128); v >>= 7; } *q++ = (uint8_t)v; return q; }

/* exact canonical-integer table scan; on success fills cols[k][rows] (caller frees *cols) */
static int csv_type(const uint8_t *s, size_t n, size_t *hl, size_t *kk, size_t *rr, int64_t **cols) {
    const uint8_t *e = memchr(s, '\n', n); if (!e) return -1;
    size_t i = (size_t)(e - s) + 1, k = 1; *hl = i;
    { const uint8_t *e2 = memchr(s + i, '\n', n - i); if (!e2) return -1; for (const uint8_t *q = s + i; q < e2; q++) k += *q == ','; }
    if (k > 4096) return -1;
    size_t cap = (n - i) / (2 * k) + 2; int64_t *c = malloc(cap * k * sizeof(int64_t)); size_t rows = 0; if (!c) return -1;
    while (i < n) {
        if (rows == cap) { free(c); return -1; }
        for (size_t j = 0; j < k; j++) {
            int neg = 0; if (i < n && s[i] == '-') { neg = 1; i++; }
            if (i >= n || s[i] < '0' || s[i] > '9') { free(c); return -1; }
            size_t st = i; uint64_t v = 0;
            while (i < n && (unsigned)(s[i] - '0') < 10 && i - st < 18) { v = v * 10 + (uint64_t)(s[i] - '0'); i++; }   /* < 10^18: no overflow */
            while (i < n && (unsigned)(s[i] - '0') < 10) { if (v > (UINT64_MAX - 9) / 10) { free(c); return -1; } v = v * 10 + (uint64_t)(s[i] - '0'); i++; }
            if ((s[st] == '0' && i - st > 1) || (neg && v == 0) || (neg ? v > (uint64_t)INT64_MAX + 1 : v > (uint64_t)INT64_MAX)) { free(c); return -1; }
            c[j * cap + rows] = neg ? (int64_t)(0 - v) : (int64_t)v;
            if (i >= n || s[i] != (j + 1 < k ? ',' : '\n')) { free(c); return -1; }
            i++;
        }
        rows++;
    }
    if (rows < 16) { free(c); return -1; }
    if (cap != rows) for (size_t j = 1; j < k; j++) memmove(c + j * rows, c + j * cap, rows * sizeof(int64_t));
    *kk = k; *rr = rows; *cols = c; return 0;
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
    size_t cap = n + n / 4 + ZSTD_compressBound(n) + 4096; uint8_t *d = malloc(cap), *q = d; size_t hl, k, rows; int64_t *cols = NULL; codec_ctx x = {0};
    *q++ = 0; q = put_uv(q, n); put32(q, blk_sum(2, src, n)); q += 4;
    if (!csv_type(src, n, &hl, &k, &rows, &cols)) {
        uint8_t *s = q; s = put_uv(s, hl); memcpy(s, src, hl); s += hl; s = put_uv(s, k); s = put_uv(s, rows); int ok = 1;
        int cat = c->kind == 'L' && k >= 16 && rows < 4096; size_t ns = cat ? 1 : k, len = cat ? k * rows : rows;   /* layout: one stream */
        if (c->kind == 'L') *s++ = (uint8_t)cat;
        for (size_t j = 0; j < ns && ok; j++) {
            fiprog P; kbuf blob = {0};
            if (fi_forward((const uint8_t *)(cols + j * rows), len * 8, 8, 0, &P) || conf_encode(&x, CONF[ci], P.res.p, P.m, P.ow, P.pln, &blob, NULL)) ok = 0;
            else if ((size_t)(s - d) + P.par.n + blob.n + 32 > cap) ok = 0;
            else { s = put_uv(s, P.par.n); memcpy(s, P.par.p, P.par.n); s += P.par.n; s = put_uv(s, blob.n); memcpy(s, blob.p, blob.n); s += blob.n; }
            fi_free(&P); free(blob.p);
        }
        if (ok) { d[0] = 1; free(cols); codec_ctx_free(&x); *dst = d; return (size_t)(s - d); }
    }
    free(cols);
    size_t r = ZSTD_compress(q, cap - (size_t)(q - d), src, n, c->level); if (ZSTD_isError(r)) DIE("zstd");
    codec_ctx_free(&x); *dst = d; return (size_t)(q - d) + r;
}
static inline int ndig(uint64_t u) { int d = 1; while (u >= 10000) { u /= 10000; d += 4; } return d + (u >= 10) + (u >= 100) + (u >= 1000); }
static inline uint8_t *itoa64(uint8_t *o, int64_t v) {          /* two digits per step from production's KG_D2 table */
    uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    if (v < 0) *o++ = '-';
    int d = ndig(u); uint8_t *e = o + d, *q = e;
    while (u >= 100) { unsigned r = (unsigned)(u % 100); u /= 100; q -= 2; q[0] = (uint8_t)KG_D2[2 * r]; q[1] = (uint8_t)KG_D2[2 * r + 1]; }
    if (u >= 10) { q -= 2; q[0] = (uint8_t)KG_D2[2 * u]; q[1] = (uint8_t)KG_D2[2 * u + 1]; } else *--q = (uint8_t)('0' + u);
    return e;
}
static int dec(const uint8_t *f, size_t fn, int ci, uint8_t *out, size_t n) {
    const cand_t *c = &CAND[ci];
    if (c->kind == 'Z') { ZSTD_DCtx *z = ZSTD_createDCtx(); size_t r = ZSTD_decompressDCtx(z, out, n + 64, f, fn); ZSTD_freeDCtx(z); return ZSTD_isError(r) || r != n; }
    if (c->kind == 'K' || c->kind == 'Q' || c->kind == 'H') return kolmo_decompress(f, fn, out, n + 64, 1) != (int64_t)n;
    size_t i = 1; uint64_t len, hl, k, rows; int rc = -1; codec_ctx x = {0}; int64_t *cols = NULL;
    if (fn < 6 || kg_getuv(f, fn, &i, &len) || len != n || i + 4 > fn) return -1;
    uint32_t sum = get32(f + i); i += 4;
    if (f[0] != 1) { size_t r = ZSTD_decompress(out, n + 64, f + i, fn - i); rc = ZSTD_isError(r) || r != n; goto done; }
    if (kg_getuv(f, fn, &i, &hl) || hl > fn - i || hl > n) goto done;
    memcpy(out, f + i, hl); i += hl;
    if (kg_getuv(f, fn, &i, &k) || kg_getuv(f, fn, &i, &rows) || !k || k > 4096 || rows > n / 2 + 1 || k * rows > n) goto done;
    cols = malloc(k * rows * sizeof(int64_t)); if (!cols) goto done;
    int cat = 0; if (c->kind == 'L') { if (i >= fn || f[i] > 1) goto done; cat = f[i++]; }
    size_t ns = cat ? 1 : k, slen = cat ? k * rows : rows;
    for (size_t j = 0; j < ns; j++) {
        uint64_t pl, bl; static uint8_t empty[1]; kbuf res = {0}, o = {0}, rest = {empty, 0, 0};   /* non-null: zero-length memcpy */
        if (kg_getuv(f, fn, &i, &pl) || pl > fn - i || pl < 3) goto done; const uint8_t *par = f + i; i += pl;
        if (kg_getuv(f, fn, &i, &bl) || bl > fn - i) goto done;
        size_t pi = 3; uint64_t hh, cnt; if (kg_getuv(par, pl, &pi, &hh) || kg_getuv(par, pl, &pi, &cnt) || cnt != slen || hh) goto done;
        int ord = (par[1] >> 1) & 3, ow = par[2], pln = (par[1] >> 3) & 1; if ((uint64_t)ord > cnt) goto done;
        int bad = e2c_decode(&x, f + i, (size_t)bl, (size_t)(cnt - (uint64_t)ord), ow, pln, slen * 8 * 4 + 1024, &res)
                  || fi_inverse(par, pl, &res, &rest, slen * 8 + 1024, &o) || o.n != slen * 8;
        if (!bad) memcpy(cols + j * rows, o.p, slen * 8);
        free(res.p); free(o.p); if (bad) goto done; i += bl;
    }
    if (i != fn) goto done;
    {   uint8_t *o = out + hl, *end = out + n;
        for (size_t r = 0; r < rows; r++) for (size_t j = 0; j < k; j++) {
            if (o > end) goto done;                              /* out has >= 64 bytes of slack; o == end checked below */
            o = itoa64(o, cols[j * rows + r]); *o++ = j + 1 < k ? ',' : '\n';
        }
        if (o != end) goto done; }
    rc = 0;
done:
    free(cols); codec_ctx_free(&x);
    return rc || blk_sum(2, out, n) != sum;
}

int main(int argc, char **argv) {
    { uint16_t one = 1; if (*(uint8_t *)&one != 1) DIE("little-endian host required"); }
    if (argc < 4) DIE("usage: p2e5 run FILE TAG ROUNDS SEED | p2e5 mem FILE CANDIDATE");
    for (int c = 0; c < NC; c++) CONF[c] = CAND[c].leaf ? conf_by_name(CAND[c].leaf) : -1;
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("read");
    uint8_t *out = malloc(n + 64);
    if (!strcmp(argv[1], "mem")) {
        int ci = -1; for (int c = 0; c < NC; c++) if (!strcmp(argv[3], CAND[c].name)) ci = c; if (ci < 0) DIE("candidate");
        if (hwm_reset()) DIE("clear_refs");
        uint8_t *f; size_t fn = enc(src, n, ci, &f); memset(out, 0, n); if (dec(f, fn, ci, out, n) || memcmp(out, src, n)) DIE("mem verify");
        printf("{\"kind\":\"p2e5_mem\",\"input\":\"%s\",\"candidate\":\"%s\",\"stored\":%zu,\"vmhwm_kb\":%ld}\n", argv[2], CAND[ci].name, fn, status_kb("VmHWM:"));
        free(f); free(out); free(src); return 0;
    }
    const char *tag = argv[3]; int rounds = argc > 4 ? atoi(argv[4]) : 5; xs = (uint64_t)(argc > 5 ? atoll(argv[5]) : 1) * 2654435761ull + 7;
    uint8_t *F[NC]; size_t FN[NC]; int Be[NC], Bd[NC], ord[NC];
    for (int c = 0; c < NC; c++) {
        FN[c] = enc(src, n, c, &F[c]); uint8_t *f2; size_t f2n = enc(src, n, c, &f2);
        if (f2n != FN[c] || memcmp(f2, F[c], FN[c])) DIE("%s not deterministic", CAND[c].name); free(f2);
        memset(out, 0, n); if (dec(F[c], FN[c], c, out, n) || memcmp(out, src, n)) DIE("%s does not reconstruct", CAND[c].name);
        printf("{\"kind\":\"p2e5_size\",\"input\":\"%s\",\"bytes\":%zu,\"candidate\":\"%s\",\"stored\":%zu,\"typed_table\":%d,\"exact\":true}\n",
               tag, n, CAND[c].name, FN[c], (CAND[c].kind == 'C' || CAND[c].kind == 'L') ? F[c][0] == 1 : -1);
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
            printf("{\"kind\":\"p2e5_time\",\"input\":\"%s\",\"candidate\":\"%s\",\"pair_id\":\"r%d\",\"slot\":%d,\"enc_reps\":%d,\"dec_reps\":%d,"
                   "\"encode_wall_s\":%.9f,\"encode_cpu_s\":%.9f,\"decode_wall_s\":%.9f,\"decode_cpu_s\":%.9f}\n",
                   tag, CAND[c].name, r, oi, Be[c], Bd[c], ew * 1e-9, ec * 1e-9, (double)(w2 - w1) / Bd[c] * 1e-9, (double)(c2 - c1) / Bd[c] * 1e-9);
        }
    }
    for (int c = 0; c < NC; c++) free(F[c]);
    free(out); free(src); return 0;
}
