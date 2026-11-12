/* round-trip, range-read and corruption tests for kolmo */
#include "kolmo.h"
int64_t tbl_encode_pub(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, uint8_t *dst);
int tbl_decode_pub(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n);
typedef struct { uint16_t y, a, b; uint8_t form; double k; } tformula;
typedef struct { int n, gcol; tformula f[256]; } fset_t;
int64_t tbl2_encode_pub(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, const fset_t *fs, uint8_t *dst);
int tbl2_decode_pub(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n);
void fset_discover_pub(const uint8_t *s, size_t n, uint8_t delim, int k, int quotes, int effort, fset_t *fs);
typedef struct { int type; int scale; const long long *num; const unsigned char *valid; const unsigned char *txt; unsigned *off, *len; } czcol;
typedef struct { int k; long long nr; czcol *c; long long *numbuf; unsigned *offbuf; unsigned char *validbuf; } czcols;
int tbl2_columns_pub(const uint8_t *in, int64_t tn, czcols *out);
void tbl2_columns_free_pub(czcols *x);
int parse_dec_pub(const uint8_t *p, uint32_t len, int64_t *m, int *sc);
const int64_t *kolmo_p10_pub(void);
int64_t log_encode_pub(const uint8_t *in, size_t n, uint8_t *dst, size_t cap);
int log_decode_pub(const uint8_t *in, size_t tn, uint8_t *out, size_t n);
int64_t log_encode_json_pub(const uint8_t *in, size_t n, uint8_t *dst, size_t cap);
int64_t sci_encode_pub(const uint8_t *in, size_t n, int image, uint8_t *dst, size_t cap);
int sci_decode_pub(const uint8_t *in, size_t tn, uint8_t *out, size_t n);
int64_t word_encode_pub(const uint8_t *in, size_t n, uint8_t *dst, size_t cap);
int word_decode_pub(const uint8_t *in, size_t tn, uint8_t *out, size_t n);
size_t np_encode_pub(const uint8_t *s, size_t n, int np, uint8_t *out, size_t cap);
int np_decode_pub(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
size_t np_bound_pub(size_t n);
#define NP_GOLDEN_LEN 17372
#define NP_GOLDEN_HASH 0xe9dace4722cab3f7ull   /* FNV-1a 64 of the codec-15 leaf for the fixed planes in main() */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static int fails = 0;
static int64_t check_p(const char *name, const uint8_t *s, size_t n, kolmo_params p, uint8_t **frame);
static void check(const char *name, const uint8_t *s, size_t n, kolmo_mode m, uint32_t B, int T) {
    kolmo_params p = {m, 0, T, B, 1}; check_p(name, s, n, p, NULL);
}
/* as check, with explicit parameters; returns the compressed size (or a negative error); *frame keeps the output */
static int64_t check_p(const char *name, const uint8_t *s, size_t n, kolmo_params p, uint8_t **frame) {
    int T = p.threads; kolmo_mode m = p.mode;
    size_t cap = kolmo_bound(n, &p); uint8_t *d = malloc(cap), *b = malloc(n + 1);
    int64_t c = kolmo_compress(s, n, d, cap, &p);
    if (c < 0) { printf("FAIL compress %s: %s\n", name, kolmo_strerror(c)); fails++; free(d); free(b); return c; }
    int64_t r = kolmo_decompress(d, c, b, n, T);
    if (r != (int64_t)n || memcmp(b, s, n)) { printf("FAIL roundtrip %s mode %d T %d: %lld\n", name, m, T, (long long)r); fails++; }
    for (int k = 0; k < 20 && n; k++) {                      /* range reads */
        size_t off = rnd() % n, len = rnd() % (n - off + 1);
        int64_t q = kolmo_read_range(d, c, off, len, b, T);
        if (q != (int64_t)len || memcmp(b, s + off, len)) { printf("FAIL range %s off %zu len %zu\n", name, off, len); fails++; break; }
    }
    int silent = 0;
    for (int k = 0; k < 300 && c > 0; k++) {                  /* corruption must never return wrong bytes silently */
        uint8_t *e = malloc(c); memcpy(e, d, c); size_t at = rnd() % c; e[at] ^= 1 << (rnd() % 8);
        size_t cl = (rnd() % 5 == 0) ? rnd() % c : (size_t)c;
        int64_t q = kolmo_decompress(e, cl, b, n, T);
        if (q == (int64_t)n && memcmp(b, s, n)) silent++;
        free(e);
    }
    if (silent) { printf("FAIL %s: %d silent corruptions\n", name, silent); fails++; }
    if (frame) *frame = d; else free(d);
    free(b);
    return c;
}
int main(void) {
    size_t N = 3 << 20;
    uint8_t *csv = malloc(N), *bin = malloc(N), *rndb = malloc(N), *txt = malloc(N);
    size_t p = 0; long t = 1700000000;                       /* CSV-like rows */
    while (p + 100 < N) p += sprintf((char *)csv + p, "%ld,S-%d,%.2f,%.1f,%s\n", t += rnd() % 7, (int)(rnd() % 16), (rnd() % 4000) / 100.0, (rnd() % 9999) / 10.0, rnd() % 50 ? "OK" : "WARN");
    size_t ncsv = p;
    float v[4] = {100, 50, 20, 7};
    for (size_t i = 0; i + 16 <= N; i += 16) for (int c = 0; c < 4; c++) { v[c] += ((int)(rnd() % 201) - 100) * 0.0001f; memcpy(bin + i + 4 * c, &v[c], 4); }
    for (size_t i = 0; i < N; i++) rndb[i] = (uint8_t)rnd();
    const char *w[] = {"the ", "data ", "compress ", "of ", "NEOM ", "architecture ", "and ", "\n"};
    for (size_t i = 0; i < N;) { const char *x = w[rnd() % 8]; size_t l = strlen(x); if (i + l > N) break; memcpy(txt + i, x, l); i += l; }
    struct { const char *n; uint8_t *s; size_t len; } in[] = {{"csv", csv, ncsv}, {"float32", bin, N}, {"random", rndb, N}, {"text", txt, N - 8},
        {"empty", csv, 0}, {"tiny", csv, 7}, {"one-block-edge", csv, 1 << 16}};
    for (size_t i = 0; i < sizeof in / sizeof *in; i++)
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check(in[i].n, in[i].s, in[i].len, (kolmo_mode)m, 1 << 18, T);
    check("csv-default-block", csv, ncsv, KOLMO_FAST, 0, 2);
    /* quoted CSV and short rows exercise the slow and exception paths */
    {
        size_t q = 0; uint8_t *qc = malloc(N);
        while (q + 200 < N) q += sprintf((char *)qc + q, (rnd() % 9) ? "%d,\"name, %d\",%.3f,x\n" : "%d,short\n", (int)(rnd() % 1000), (int)(rnd() % 50), (rnd() % 100000) / 1000.0);
        for (int m = 0; m < 5; m++) check("quoted-csv", qc, q, (kolmo_mode)m, 1 << 18, 2);
        free(qc);
    }
    /* tables with derived columns (ratio, change, % change inside entity groups) and blanks: formula path */
    size_t fq = 0; uint8_t *ft = malloc(N);
    {
        fq += sprintf((char *)ft, "country,year,pop,gdp,gdp_pc,gdp_chg,gdp_pct,note\n");
        const char *cn[] = {"Aland", "Borduria", "Carpathia", "Doria", "Elbonia", "Freedonia"};
        for (int c = 0; fq + 4096 < N; c = (c + 1) % 6) {
            double pop = 1e5 + rnd() % 900000, gdp = 1e3 + rnd() % 90000, prev = -1;
            for (int y = 1950; y < 2024 && fq + 300 < N; y++) {
                pop *= 1 + (rnd() % 40) / 1000.0; gdp = (double)(long)(gdp * (1 + ((int)(rnd() % 80) - 30) / 1000.0) * 100) / 100;
                int blank = rnd() % 7 == 0;
                char g[32] = "", pc[32] = "", ch[32] = "", pt[32] = "";
                if (!blank) { sprintf(g, "%.2f", gdp); sprintf(pc, "%.4f", gdp / pop * 1000); }
                if (!blank && prev > 0) { sprintf(ch, "%.2f", gdp - prev); sprintf(pt, "%.3f", (gdp - prev) / prev * 100); }
                fq += sprintf((char *)ft + fq, "%s,%d,%.0f,%s,%s,%s,%s,%s\n", cn[c], y, pop, g, pc, ch, pt, rnd() % 40 ? "" : "rev");
                prev = blank ? -1 : gdp;
            }
        }
        fset_t fs; fset_discover_pub(ft, fq, ',', 8, 1, 1, &fs);
        printf("formula table: %d formulas found, group column %d\n", fs.n, fs.gcol);
        if (fs.n < 2) { printf("FAIL formula discovery\n"); fails++; }
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("formula-table", ft, fq, (kolmo_mode)m, 1 << 18, T);
        check("formula-table-1block", ft, fq, KOLMO_HC, 0, 1);
        /* fuzz the v2 stream itself */
        size_t len = 1 << 18; while (ft[len - 1] != '\n') len--;
        uint8_t *t = malloc(len + 65536), *o = malloc(len + 64), *e = malloc(len + 65536);
        int64_t tn = tbl2_encode_pub(ft, len, ',', 8, 1, &fs, t);
        if (tn <= 0 || tbl2_decode_pub(t, tn, ',', o, len) || memcmp(o, ft, len)) { printf("FAIL tbl2 roundtrip\n"); fails++; }
        for (int it = 0; it < 4000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            size_t el = rnd() % 8 ? (size_t)tn : rnd() % tn;
            tbl2_decode_pub(e, el, ',', o, len);
        }
        printf("table-codec v2 fuzz: 4000 corrupt streams decoded without crashing\n");
        /* columnar decode: every cell matches the source value, and corrupt streams never crash */
        if (tn > 0) {
            czcols cols;
            if (tbl2_columns_pub(t, tn, &cols) < 0) { printf("FAIL columnar decode rejected an aligned table\n"); fails++; }
            else {
                long cbad = 0; size_t ci = 0;
                for (long long rr = 0; rr < cols.nr; rr++) for (int j = 0; j < cols.k; j++) {
                    size_t f0 = ci; while (ci < len && ft[ci] != ',' && ft[ci] != '\n') ci++;
                    unsigned flen = (unsigned)(ci - f0); czcol *c = &cols.c[j];
                    if (c->type == 0) { if (c->len[rr] != flen || memcmp(c->txt + c->off[rr], ft + f0, flen)) cbad++; }
                    else { int64_t m; int sc;
                        if (parse_dec_pub(ft + f0, flen, &m, &sc)) { int64_t want = m * kolmo_p10_pub()[c->scale - sc];
                            if (!c->valid[rr] || c->num[rr] != want) cbad++; }
                        else if (c->valid[rr]) cbad++; }
                    if (ci < len && ft[ci] == ',') ci++;
                    if (j == cols.k - 1 && ci < len && ft[ci] == '\n') ci++;
                }
                if (cbad) { printf("FAIL columnar decode: %ld cell mismatches\n", cbad); fails++; }
                else printf("columnar decode: %lld rows x %d cols, all cells correct\n", cols.nr, cols.k);
                tbl2_columns_free_pub(&cols);
            }
            for (int it = 0; it < 4000; it++) {
                memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
                for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
                czcols cc; if (tbl2_columns_pub(e, rnd() % 8 ? (int64_t)tn : rnd() % tn, &cc) >= 0) tbl2_columns_free_pub(&cc);
            }
            printf("columnar-codec fuzz: 4000 corrupt streams decoded without crashing\n");
        }
        free(t); free(o); free(e); free(ft);
    }
    /* logs: templates, skeleton columns (timestamps, IPs, padded ids), rare lines, no final newline */
    {
        size_t lq = 0; uint8_t *lg = malloc(N); long ts = 1700000000; int ms = 0;
        const char *lv[] = {"INFO", "WARN", "ERROR"};
        while (lq + 400 < N) {
            ts += rnd() % 3; ms = (ms + (int)(rnd() % 997)) % 1000; int k = (int)(rnd() % 10);
            if (k < 5) lq += sprintf((char *)lg + lq, "2024-01-%02ld %02ld:%02ld:%02ld,%03d %s dfs.DataNode: Receiving block blk_%lld src: /10.%d.%d.%d:%d dest: /10.0.0.%d:50010\n",
                                     1 + (ts / 86400) % 28, (ts / 3600) % 24, (ts / 60) % 60, ts % 60, ms, lv[rnd() % 3], (long long)(rnd() % 9000000000000ll) - 4500000000000ll,
                                     (int)(rnd() % 256), (int)(rnd() % 256), (int)(rnd() % 256), (int)(rnd() % 65536), (int)(rnd() % 256));
            else if (k < 8) lq += sprintf((char *)lg + lq, "2024-01-%02ld %02ld:%02ld:%02ld,%03d INFO session 0x%06x closed  after %d ms\n",
                                          1 + (ts / 86400) % 28, (ts / 3600) % 24, (ts / 60) % 60, ts % 60, ms, (unsigned)(rnd() % 0xFFFFFF), (int)(rnd() % 5000));
            else if (k < 9) lq += sprintf((char *)lg + lq, "user%04d logged in from host-%c%c with \x01 marker %d\n", (int)(rnd() % 10000), 'a' + (int)(rnd() % 26), 'a' + (int)(rnd() % 26), (int)(rnd() % 7));
            else { int w = 1 + (int)(rnd() % 12); for (int i = 0; i < w; i++) lq += sprintf((char *)lg + lq, "%s%c", (rnd() % 2) ? "x" : "free-text", i + 1 < w ? ' ' : '\n'); }
        }
        lq += sprintf((char *)lg + lq, "partial last line 42");
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("logs", lg, lq, (kolmo_mode)m, 1 << 18, T);
        check("logs-1block", lg, lq, KOLMO_FAST, 0, 1);
        size_t len = 1 << 18; uint8_t *t = malloc(len + 65536), *o = malloc(len + 64), *e = malloc(len + 65536);
        int64_t tn = log_encode_pub(lg, len, t, len + 65536);
        if (tn <= 0 || log_decode_pub(t, tn, o, len) || memcmp(o, lg, len)) { printf("FAIL log codec roundtrip (%lld)\n", (long long)tn); fails++; }
        else printf("log codec: %.2fx before entropy coding\n", (double)len / tn);
        for (int it = 0; it < 4000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            size_t el = rnd() % 8 ? (size_t)tn : rnd() % tn;
            log_decode_pub(e, el, o, len);
        }
        printf("log codec fuzz: 4000 corrupt streams decoded without crashing\n");
        free(t); free(o); free(e); free(lg);
    }
    /* JSON lines: escapes, nesting, all scalar kinds, odd spacing, a 0x01 line, malformed lines, pretty-printed JSON */
    {
        size_t N = 3u << 20, q = 0; uint8_t *js = malloc(N + 4096); long id = 1000;
        const char *nm[] = {"alpha", "be\\\"ta", "gam ma", "d\\u00e9lta", "\xc3\xa9psilon"};
        while (q + 2000 < N) {
            int k = (int)(rnd() % 20); id += 1 + (long)(rnd() % 5);
            if (k < 12) q += sprintf((char *)js + q, "{\"id\":%ld,\"name\":\"%s\",\"score\":%d.%02d,\"ok\":%s,\"tags\":[\"a\",\"b%d\"],\"geo\":{\"lat\":-%d.%04d,\"lon\":%de%d},\"note\":%s}\n",
                                     id, nm[rnd() % 5], (int)(rnd() % 1000), (int)(rnd() % 100), rnd() % 2 ? "true" : "false", (int)(rnd() % 9),
                                     (int)(rnd() % 90), (int)(rnd() % 10000), (int)(rnd() % 9), (int)(rnd() % 5), rnd() % 3 ? "null" : "\"x: y, z\"");
            else if (k < 16) q += sprintf((char *)js + q, "{ \"id\" : %ld , \"name\" : \"%s\" , \"vals\" : [ %d, %d, -%d ] }\n", id, nm[rnd() % 5], (int)(rnd() % 50), (int)(rnd() % 50), (int)(rnd() % 50));
            else if (k < 17) q += sprintf((char *)js + q, "{\"id\":%ld,\"raw\":\"\x01\",\"n\":%d}\n", id, (int)(rnd() % 10));
            else if (k < 18) q += sprintf((char *)js + q, "{\"id\":%ld,\"broken\":\"unterminated %d\n", id, (int)(rnd() % 10));
            else if (k < 19) q += sprintf((char *)js + q, "[%ld,\"\\\\\",\"\\\\\\\"\",1.5E+10,-0.0,true]\n", id);
            else q += sprintf((char *)js + q, "{\n  \"id\": %ld,\n  \"items\": [\n    {\"k\": \"v%d\", \"n\": %d}\n  ]\n}\n", id, (int)(rnd() % 100), (int)(rnd() % 1000));
        }
        q += sprintf((char *)js + q, "{\"last\":\"no newline\"}");
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("json", js, q, (kolmo_mode)m, 1 << 18, T);
        {   kolmo_params p0 = {KOLMO_FAST, 0, 1, 0, 0}, p1 = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(q, &p0); uint8_t *d = malloc(cap);
            int64_t c0 = kolmo_compress(js, q, d, cap, &p0), c1 = kolmo_compress(js, q, d, cap, &p1);
            if (c1 <= 0 || c0 <= 0 || c1 >= c0) { printf("FAIL json transform not chosen (%lld vs %lld)\n", (long long)c1, (long long)c0); fails++; }
            else printf("json lines: fast %.1f%% of fast without transforms\n", 100.0 * c1 / c0);
            free(d); }
        size_t len = 1 << 18; while (len && js[len - 1] != '\n') len--;
        uint8_t *t = malloc(len + 65536), *o = malloc(len + 64), *e = malloc(len + 65536);
        int64_t tn = log_encode_json_pub(js, len, t, len + 65536);
        if (tn <= 0 || log_decode_pub(t, tn, o, len) || memcmp(o, js, len)) { printf("FAIL json codec roundtrip (%lld)\n", (long long)tn); fails++; }
        for (int it = 0; it < 3000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            log_decode_pub(e, rnd() % 8 ? (size_t)tn : rnd() % tn, o, len);
        }
        printf("json codec fuzz: 3000 corrupt streams decoded without crashing\n");
        free(t); free(o); free(e); free(js);
    }
    /* minified JSON: one line of records (strings with escapes, nesting); small blocks start inside strings */
    {
        size_t N = 2u << 20, q = 0; uint8_t *js = malloc(N + 4096); long id = 5000;
        js[q++] = '[';
        while (q + 600 < N) {
            id += 1 + (long)(rnd() % 3);
            q += sprintf((char *)js + q, "{\"id\":%ld,\"name\":\"n%d, \\\"q\\\" },{x\",\"v\":[%d,{\"a\":%s}],\"f\":%d.%02d},",
                         id, (int)(rnd() % 100), (int)(rnd() % 9), rnd() % 2 ? "true" : "null", (int)(rnd() % 1000), (int)(rnd() % 100));
        }
        q += sprintf((char *)js + q, "{\"end\":1}]");
        for (int m = 1; m < 5; m++) { check("json-min", js, q, (kolmo_mode)m, 1 << 18, 1); check("json-min-odd", js, q, (kolmo_mode)m, 77777, 4); }
        {   kolmo_params p0 = {KOLMO_FAST, 0, 1, 0, 0}, p1 = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(q, &p0); uint8_t *d = malloc(cap);
            int64_t c0 = kolmo_compress(js, q, d, cap, &p0), c1 = kolmo_compress(js, q, d, cap, &p1);
            if (c1 <= 0 || c0 <= 0 || c1 >= c0) { printf("FAIL minified json transform not chosen (%lld vs %lld)\n", (long long)c1, (long long)c0); fails++; }
            else printf("minified json: fast %.1f%% of fast without transforms\n", 100.0 * c1 / c0);
            free(d); }
        free(js);
    }
    /* wide delimited table (300 columns): kolmo graph blocks; odd values, irregular lines, CRLF, no final newline */
    {
        size_t N = 3u << 20, q = 0; uint8_t *w = malloc(N + 65536);
        q += sprintf((char *)w + q, "name;year");
        for (int c = 0; c < 298; c++) q += sprintf((char *)w + q, ";c%d", c);
        w[q++] = '\n';
        long t = 0; int row = 0;
        while (q + 20000 < N) {
            q += sprintf((char *)w + q, "v%d;%d", row % 7, 1900 + row % 120);
            for (int c = 0; c < 298; c++) {
                int k = (int)(rnd() % 100); w[q++] = ';';
                if (k < 15) continue;                                             /* empty */
                else if (k < 70) q += sprintf((char *)w + q, "%ld,%02d", (t + c * 37) % 100000, (int)((c * 7 + row) % 100));
                else if (k < 85) q += sprintf((char *)w + q, "-%d", (int)(rnd() % 5000));
                else if (k < 88) q += sprintf((char *)w + q, "%s", (const char *[]){"-0", "007", "-0,0", "1,", ",5", "12345678901234567890", "1e5", "NaN"}[rnd() % 8]);
                else q += sprintf((char *)w + q, "%d", (int)(rnd() % 100));
            }
            if (row % 997 == 5) q += sprintf((char *)w + q, ";extra");                 /* irregular field count */
            if (row % 1501 == 9) w[q++] = '\r';                                       /* a CRLF line */
            w[q++] = '\n'; row++; t += 1 + (long)(rnd() % 3);
            if (row % 2003 == 0) { q += sprintf((char *)w + q, "a comment line\n\n"); }
        }
        q += sprintf((char *)w + q, "end;1999");                                       /* no final newline */
        for (int m = 1; m < 5; m++) { check("wide", w, q, (kolmo_mode)m, 1 << 20, 1); check("wide-odd", w, q, (kolmo_mode)m, 300001, 4); }
        {   kolmo_params p0 = {KOLMO_FAST, 0, 1, 0, 0}, p1 = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(q, &p0); uint8_t *d = malloc(cap);
            int64_t c0 = kolmo_compress(w, q, d, cap, &p0), c1 = kolmo_compress(w, q, d, cap, &p1);
            if (c1 <= 0 || c0 <= 0 || c1 >= c0 || d[4] != 7) { printf("FAIL wide table graph not chosen (%lld vs %lld, version %d)\n", (long long)c1, (long long)c0, d[4]); fails++; }
            else printf("wide table graph: fast %.1f%% of fast without transforms\n", 100.0 * c1 / c0);
            /* graph decoder fuzz: corrupt payload bytes of every block, decoding must fail cleanly or round-trip */
            uint8_t *bk = malloc(q + 64), *e = malloc(c1);
            for (int it = 0; it < 400; it++) {
                memcpy(e, d, c1); int nf = 1 + (int)(rnd() % 3);
                for (int f = 0; f < nf; f++) e[32 + rnd() % (c1 - 32)] ^= (uint8_t)(1 + rnd() % 255);
                int64_t r = kolmo_decompress(e, c1, bk, q + 64, 1);
                if (r >= 0 && (r != (int64_t)q || memcmp(bk, w, q))) { printf("FAIL wide graph fuzz accepted a wrong output\n"); fails++; break; }
            }
            printf("wide graph fuzz: 400 corrupted frames rejected or exact\n");
            free(bk); free(e); free(d); }
        free(w);
    }
    /* integer arrays: kolmo graph INT node (gcd + delta timestamps with a ragged tail, a .npy int32 file, wrapping
     * int64 extremes, unsigned 16-bit), round trips in every mode and odd block sizes, plus decoder fuzz */
    {
        size_t cnt = 400000, q = cnt * 8 + 5; uint8_t *a = malloc(q + 256);
        int64_t ts = 1700000000000LL;
        for (size_t i = 0; i < cnt; i++) { ts += 1000 * (int64_t)(rnd() % 16 ? 5 : 1 + rnd() % 40); memcpy(a + i * 8, &ts, 8); }
        memcpy(a + cnt * 8, "tail!", 5);
        for (int m = 0; m < 5; m++) { check("int-ts", a, q, (kolmo_mode)m, 1 << 20, 1); check("int-ts-odd", a, q, (kolmo_mode)m, 300001, 4); }
        q = 30000 * 8;                                       /* a small array: the planner picks the graph */
        {   kolmo_params p0 = {KOLMO_FAST, 0, 1, 0, 0}, p1 = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(q, &p0); uint8_t *d = malloc(cap);
            int64_t c0 = kolmo_compress(a, q, d, cap, &p0), c1 = kolmo_compress(a, q, d, cap, &p1);
            if (c1 <= 0 || c0 <= 0 || c1 * 2 >= c0 || d[4] != 7) { printf("FAIL integer graph not chosen (%lld vs %lld, version %d)\n", (long long)c1, (long long)c0, d[4]); fails++; }
            else printf("integer graph: timestamps fast %.1f%% of fast without transforms\n", 100.0 * c1 / c0);
            uint8_t *bk = malloc(q + 64), *e = malloc(c1);
            for (int it = 0; it < 400; it++) {
                memcpy(e, d, c1); int nf = 1 + (int)(rnd() % 3);
                for (int f = 0; f < nf; f++) e[32 + rnd() % (c1 - 32)] ^= (uint8_t)(1 + rnd() % 255);
                int64_t r = kolmo_decompress(e, c1, bk, q + 64, 1);
                if (r >= 0 && (r != (int64_t)q || memcmp(bk, a, q))) { printf("FAIL integer graph fuzz accepted a wrong output\n"); fails++; break; }
            }
            printf("integer graph fuzz: 400 corrupted frames rejected or exact\n");
            free(bk); free(e); free(d); }
        /* .npy int32 (header of 128 bytes), slowly varying with noise */
        size_t h = 128, n4 = 300000; q = h + n4 * 4;
        memset(a, ' ', h); { static const char H[] = "\x93NUMPY\x01\x00\x76\x00{'descr': '<i4', 'fortran_order': False, 'shape': (300000,), }"; memcpy(a, H, sizeof H - 1); } a[h - 1] = '\n';
        int32_t y = 0; for (size_t i = 0; i < n4; i++) { y += (int32_t)(rnd() % 21) - 10; memcpy(a + h + i * 4, &y, 4); }
        for (int m = 0; m < 5; m++) { check("int-npy", a, q, (kolmo_mode)m, 1 << 20, 1); check("int-npy-odd", a, q, (kolmo_mode)m, 65537, 3); }
        /* int64 extremes: deltas overflow and wrap */
        q = 200000 * 8;
        for (size_t i = 0; i < 200000; i++) { int64_t v = (i % 3 == 0) ? INT64_MIN + (int64_t)(rnd() % 7) : (i % 3 == 1) ? INT64_MAX - (int64_t)(rnd() % 7) : (int64_t)(rnd() % 1000); memcpy(a + i * 8, &v, 8); }
        for (int m = 0; m < 5; m++) check("int-wrap", a, q, (kolmo_mode)m, 1 << 20, 1);
        /* unsigned 16-bit with large values (signed and unsigned readings differ) */
        q = 500000 * 2 + 1; uint16_t u = 60000;
        for (size_t i = 0; i < 500000; i++) { u += (uint16_t)(rnd() % 9) - 4; memcpy(a + i * 2, &u, 2); }
        a[q - 1] = 7;
        for (int m = 0; m < 5; m++) check("int-u16", a, q, (kolmo_mode)m, 1 << 20, 2);
        free(a);
    }
    /* float arrays: kolmo graph FLOAT node + Huffman-only leaves (weights-like float32 at an odd offset, .npy float16) */
    {
        size_t cnt = 300000, h = 3, q = h + cnt * 4 + 2; uint8_t *a = malloc(q + 256);
        for (size_t i = 0; i < h; i++) a[i] = (uint8_t)(0x40 + i);
        for (size_t i = 0; i < cnt; i++) {
            double g = 0; for (int k = 0; k < 4; k++) g += (double)(rnd() % 20001) / 10000.0 - 1.0;
            float f = (float)(g * 0.02); if (i % 5000 == 7) f = 0.0f; memcpy(a + h + i * 4, &f, 4);
        }
        a[q - 2] = 1; a[q - 1] = 2;
        for (int m = 0; m < 5; m++) { check("float32", a, q, (kolmo_mode)m, 1 << 20, 1); check("float32-odd", a, q, (kolmo_mode)m, 262147, 3); }
        {   kolmo_params p0 = {KOLMO_FAST, 0, 1, 0, 0}, p1 = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(q, &p0); uint8_t *d = malloc(cap);
            int64_t c0 = kolmo_compress(a, q, d, cap, &p0), c1 = kolmo_compress(a, q, d, cap, &p1);
            if (c1 <= 0 || c0 <= 0 || c1 >= c0 || d[4] != 7) { printf("FAIL float graph not chosen (%lld vs %lld, version %d)\n", (long long)c1, (long long)c0, d[4]); fails++; }
            else printf("float graph: float32 fast %.1f%% of fast without transforms\n", 100.0 * c1 / c0);
            uint8_t *bk = malloc(q + 64), *e = malloc(c1);
            for (int it = 0; it < 300; it++) {
                memcpy(e, d, c1); int nf = 1 + (int)(rnd() % 3);
                for (int f = 0; f < nf; f++) e[32 + rnd() % (c1 - 32)] ^= (uint8_t)(1 + rnd() % 255);
                int64_t r = kolmo_decompress(e, c1, bk, q + 64, 1);
                if (r >= 0 && (r != (int64_t)q || memcmp(bk, a, q))) { printf("FAIL float graph fuzz accepted a wrong output\n"); fails++; break; }
            }
            printf("float graph fuzz: 300 corrupted frames rejected or exact\n");
            free(bk); free(e); free(d); }
        /* .npy float16 */
        size_t n2 = 400000; q = 128 + n2 * 2; memset(a, ' ', 128);
        { static const char H[] = "\x93NUMPY\x01\x00\x76\x00{'descr': '<f2', 'fortran_order': False, 'shape': (400000,), }"; memcpy(a, H, sizeof H - 1); } a[127] = '\n';
        for (size_t i = 0; i < n2; i++) { uint16_t v = (uint16_t)(0x3800 + (rnd() % 2048) - 1024); if (rnd() % 2) v |= 0x8000; memcpy(a + 128 + i * 2, &v, 2); }
        for (int m = 0; m < 5; m++) check("float16-npy", a, q, (kolmo_mode)m, 1 << 20, 2);
        free(a);
    }
    /* 8-bit images: kolmo graph IMG8 node (raw RGB 640 wide with a ragged tail, .npy uint8 2-D), round trips and fuzz */
    {
        size_t W = 640, H = 300, C = 3, q = W * H * C + 7; uint8_t *a = malloc(q + 256);
        for (size_t y = 0; y < H; y++) for (size_t x = 0; x < W; x++) for (size_t c = 0; c < C; c++) {
            int v = (int)((x * 37 + c * 11) % 199) + (int)(y / 4) + (int)(rnd() % 5) - 2;   /* rows alike, columns not */
            a[(y * W + x) * C + c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
        for (size_t i = W * H * C; i < q; i++) a[i] = (uint8_t)i;
        for (int m = 0; m < 5; m++) { check("img8-rgb", a, q, (kolmo_mode)m, 1 << 20, 1); check("img8-rgb-odd", a, q, (kolmo_mode)m, 100003, 3); }
        {   kolmo_params p0 = {KOLMO_FAST, 0, 1, 0, 0}, p1 = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(q, &p0); uint8_t *d = malloc(cap);
            int64_t c0 = kolmo_compress(a, q, d, cap, &p0), c1 = kolmo_compress(a, q, d, cap, &p1);
            if (c1 <= 0 || c0 <= 0 || c1 >= c0 || d[4] != 7) { printf("FAIL image graph not chosen (%lld vs %lld, version %d)\n", (long long)c1, (long long)c0, d[4]); fails++; }
            else printf("image graph: 8-bit RGB fast %.1f%% of fast without transforms\n", 100.0 * c1 / c0);
            uint8_t *bk = malloc(q + 64), *e = malloc(c1);
            for (int it = 0; it < 300; it++) {
                memcpy(e, d, c1); int nf = 1 + (int)(rnd() % 3);
                for (int f = 0; f < nf; f++) e[32 + rnd() % (c1 - 32)] ^= (uint8_t)(1 + rnd() % 255);
                int64_t r = kolmo_decompress(e, c1, bk, q + 64, 1);
                if (r >= 0 && (r != (int64_t)q || memcmp(bk, a, q))) { printf("FAIL image graph fuzz accepted a wrong output\n"); fails++; break; }
            }
            printf("image graph fuzz: 300 corrupted frames rejected or exact\n");
            free(bk); free(e); free(d); }
        /* .npy uint8 (300, 451): a width the raw detector does not list */
        size_t w2 = 451, h2 = 300; q = 128 + w2 * h2; memset(a, ' ', 128);
        { static const char Hd[] = "\x93NUMPY\x01\x00\x76\x00{'descr': '|u1', 'fortran_order': False, 'shape': (300, 451), }"; memcpy(a, Hd, sizeof Hd - 1); } a[127] = '\n';
        for (size_t y = 0; y < h2; y++) for (size_t x = 0; x < w2; x++) a[128 + y * w2 + x] = (uint8_t)((x + y) / 3 + rnd() % 5);
        for (int m = 0; m < 5; m++) check("img8-npy", a, q, (kolmo_mode)m, 1 << 20, 2);
        free(a);
    }
    /* scientific binary: fixed records (header, quantised doubles incl. NaN/huge outliers, small ints, ids) and a
     * 16-bit image (big-endian, odd offset) */
    {
        size_t P = 28, nrec = 200000, rn = 28 + nrec * P + 5; uint8_t *rec = malloc(rn);
        for (size_t i = 0; i < 28; i++) rec[i] = (uint8_t)(i * 37 + 1);
        double ra = 1.0;
        for (size_t r = 0; r < nrec; r++) {
            uint8_t *q = rec + 28 + r * P; ra += (double)(rnd() % 2000) * 4.84813681109536e-8;
            double dec = ((double)(rnd() % 3000000) - 1500000) * 4.84813681109536e-9;
            if (r == 777) dec = 0.0 / 0.0; if (r == 778) dec = 1e300;
            uint16_t mag = (uint16_t)(rnd() % 1200), sp = (uint16_t)("ABFGKM"[rnd() % 6] << 8 | ('0' + rnd() % 10));
            uint32_t id = (uint32_t)(r * 3 + rnd() % 3), fl = (uint32_t)(rnd() % 4);
            memcpy(q, &ra, 8); memcpy(q + 8, &dec, 8); memcpy(q + 16, &mag, 2); memcpy(q + 18, &sp, 2); memcpy(q + 20, &id, 4); memcpy(q + 24, &fl, 4);
        }
        memcpy(rec + rn - 5, "tail!", 5);
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("records", rec, rn, (kolmo_mode)m, 1 << 20, T);
        check("records-1block", rec, rn, KOLMO_HC, 0, 1);
        size_t W = 1024, H = 1600, in_ = 1 + W * H * 2 + 3; uint8_t *im = malloc(in_); im[0] = 0x42;
        for (size_t y = 0; y < H; y++) for (size_t x = 0; x < W; x++) {
            int v = 2000 + (int)(800 * sin(x / 70.0) * cos(y / 90.0)) + (int)(rnd() % 9);
            im[1 + 2 * (y * W + x)] = (uint8_t)(v >> 8); im[2 + 2 * (y * W + x)] = (uint8_t)v;
        }
        memcpy(im + in_ - 3, "end", 3);
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("image16", im, in_, (kolmo_mode)m, 1 << 20, T);
        kolmo_params pp = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(in_, &pp); uint8_t *d = malloc(cap);
        int64_t c1 = kolmo_compress(im, in_, d, cap, &pp); printf("image16 ratio %.2f, ", (double)in_ / c1);
        int64_t c2 = kolmo_compress(rec, rn, d, cap > kolmo_bound(rn, &pp) ? cap : cap, &pp); (void)c2; free(d);
        pp.mode = KOLMO_FAST; cap = kolmo_bound(rn, &pp); d = malloc(cap); c2 = kolmo_compress(rec, rn, d, cap, &pp); printf("records ratio %.2f\n", (double)rn / c2); free(d);
        /* fuzz both decoders on their own streams */
        uint8_t *t = malloc(rn * 3 + 65536), *o = malloc(rn + 64), *e = malloc(rn * 3 + 65536);
        int64_t tn = sci_encode_pub(rec, rn, 0, t, rn * 3 + 65536);
        if (tn <= 0 || sci_decode_pub(t, tn, o, rn) || memcmp(o, rec, rn)) { printf("FAIL records codec roundtrip (%lld)\n", (long long)tn); fails++; }
        for (int it = 0; it < 3000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            sci_decode_pub(e, rnd() % 8 ? (size_t)tn : rnd() % tn, o, rn);
        }
        tn = sci_encode_pub(im, in_, 1, t, rn * 3 + 65536);
        if (tn <= 0 || sci_decode_pub(t, tn, o, in_) || memcmp(o, im, in_)) { printf("FAIL image codec roundtrip (%lld)\n", (long long)tn); fails++; }
        for (int it = 0; it < 3000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            sci_decode_pub(e, rnd() % 8 ? (size_t)tn : rnd() % tn, o, in_);
        }
        printf("records/image codec fuzz: 6000 corrupt streams decoded without crashing\n");
        free(t); free(o); free(e); free(rec); free(im);
    }
    /* tar archive: members get their own plans (text, x86-like code, CSV, binary records); container v2 */
    {
        size_t cap = 24u << 20, q = 0; uint8_t *ar = calloc(cap, 1);
        const char *names[] = {"notes.txt", "app.exe", "data.csv", "small.bin", "tail.txt"};
        for (int mbr = 0; mbr < 5; mbr++) {
            size_t sz = mbr == 3 ? 3000 : (mbr == 4 ? 777 : (size_t)(2u << 20) + (size_t)(rnd() % 100000));
            uint8_t *h = ar + q; memset(h, 0, 512); strcpy((char *)h, names[mbr]); sprintf((char *)h + 124, "%011o", (unsigned)sz);
            h[156] = '0'; memcpy(h + 257, "ustar", 5); q += 512;
            uint8_t *d = ar + q;
            for (size_t i = 0; i < sz;) {
                if (mbr == 0 || mbr == 4) { const char *x = w[rnd() % 8]; size_t l = strlen(x); if (i + l > sz) l = sz - i; memcpy(d + i, x, l); i += l; }
                else if (mbr == 1) { if (i + 5 <= sz && rnd() % 6 == 0) { d[i] = 0xE8; int32_t rel = (int32_t)(rnd() % 4096) * 16 - (int32_t)i; memcpy(d + i + 1, &rel, 4); i += 5; } else d[i++] = (uint8_t)(rnd() % 7 == 0 ? 0xE9 : 0x48 + rnd() % 8); }
                else if (mbr == 2) { char line[64]; int l = sprintf(line, "%d,%d,%.2f\n", (int)(i / 20), (int)(rnd() % 100), (rnd() % 10000) / 100.0); if (i + l > sz) l = (int)(sz - i); memcpy(d + i, line, l); i += l; }
                else d[i++] = (uint8_t)rnd();
            }
            q += (sz + 511) / 512 * 512;
        }
        q += 1024;
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("tar-archive", ar, q, (kolmo_mode)m, 1 << 20, T);
        free(ar);
    }
    /* x86 transform: adversarial bytes (E8/E9 next to 00/FF) must round-trip through every mode */
    {
        size_t n2 = 3u << 20; uint8_t *x = malloc(n2);
        for (size_t i = 0; i < n2; i++) { uint64_t r = rnd() % 5; x[i] = r == 0 ? 0xE8 : r == 1 ? 0xE9 : r == 2 ? 0xFF : r == 3 ? 0x00 : (uint8_t)rnd(); }
        for (int m = 0; m < 5; m++) check("e8e9-adversarial", x, n2, (kolmo_mode)m, 1 << 20, 2);
        free(x);
    }
    /* prose: word dictionary transform (capitalised words, long words, UTF-8, control bytes absent/present) */
    {
        size_t pn = 4u << 20, q = 0; uint8_t *pr = malloc(pn + 64);
        const char *vocab[] = {"the", "The", "architecture", "Architecture", "NEOM", "data", "compression", "of", "and", "Riyadh",
                               "supercalifragilisticexpialidocious", "caf\xc3\xa9", "Zebra", "zebra", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
        while (q + 80 < pn) { const char *x = vocab[rnd() % 15]; size_t l = strlen(x); memcpy(pr + q, x, l); q += l; pr[q++] = (uint8_t)(rnd() % 9 ? ' ' : (rnd() % 2 ? ',' : '\n')); }
        for (int m = 0; m < 5; m++) for (int T = 1; T <= 4; T += 3) check("prose", pr, q, (kolmo_mode)m, 1 << 20, T);
        check("prose-1block", pr, q, KOLMO_HC, 0, 1);
        uint8_t *t = malloc(q + 65536), *o = malloc(q + 64), *e = malloc(q + 65536);
        int64_t tn = word_encode_pub(pr, q, t, q + 65536);
        if (tn <= 0 || word_decode_pub(t, tn, o, q) || memcmp(o, pr, q)) { printf("FAIL word codec roundtrip (%lld)\n", (long long)tn); fails++; }
        else printf("word codec: %.2fx before entropy coding\n", (double)q / tn);
        for (int it = 0; it < 3000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            word_decode_pub(e, rnd() % 8 ? (size_t)tn : rnd() % tn, o, q);
        }
        printf("word codec fuzz: 3000 corrupt streams decoded without crashing\n");
        free(t); free(o); free(e); free(pr);
    }
    /* ultra (context mixing, best-of coder per block): small inputs, the CM decoder is slow */
    {
        size_t un = 300000; uint8_t *u = malloc(un);
        for (size_t i = 0; i < un;) { const char *x = w[rnd() % 8]; size_t l = strlen(x); if (i + l > un) break; memcpy(u + i, x, l); i += l; }
        check("ultra-text", u, un - 8, KOLMO_ULTRA, 1 << 17, 2);
        check("ultra-csv", csv, 400000, KOLMO_ULTRA, 1 << 17, 1);
        check("ultra-random", rndb, 200000, KOLMO_ULTRA, 1 << 17, 1);
        free(u);
    }
    /* native plane-context leaf (codec 15): direct round trips, every truncation rejected, corrupt leaves never
     * crash, and a golden vector (integer-only input; fixes the coded bytes across compilers and architectures) */
    {
        size_t L = 20000, np = 4, n = L * np, bad = 0; uint8_t *src = malloc(n), *o = malloc(n + 64);
        size_t cap = np_bound_pub(n); uint8_t *e = malloc(cap), *f = malloc(cap); uint32_t s = 777;
        for (size_t i = 0; i < L; i++) {                        /* plane 0 noisy, 1 sparse, 2 runs, 3 constant */
            s = s * 1103515245u + 12345u; uint32_t r = (s >> 16) & 0x7fff;
            src[i] = (uint8_t)((r % 23) * 2 + (i & 1)); src[L + i] = r % 97 == 0 ? (uint8_t)(1 + r % 3) : 0;
            src[2 * L + i] = (uint8_t)((i / 300) & 7); src[3 * L + i] = 0;
        }
        for (size_t k = 1; k <= 4; k <<= 1) {
            size_t z = np_encode_pub(src, n, (int)k, e, cap);
            if (!z || z >= n || np_decode_pub(e, z, o, n) || memcmp(o, src, n)) { printf("FAIL native leaf roundtrip np=%zu (%zu)\n", k, z); fails++; }
        }
        size_t z = np_encode_pub(src, n, (int)np, e, cap);
        uint64_t hsh = 1469598103934665603ull; for (size_t i = 0; i < z; i++) hsh = (hsh ^ e[i]) * 1099511628211ull;
        if (z != NP_GOLDEN_LEN || hsh != NP_GOLDEN_HASH) { printf("FAIL native leaf golden vector: %zu %016llx\n", z, (unsigned long long)hsh); fails++; }
        for (size_t l = 0; l < z; l++) if (!np_decode_pub(e, l, o, n)) bad++;          /* every truncation must be rejected */
        if (bad) { printf("FAIL native leaf: %zu truncations accepted\n", bad); fails++; }
        for (int it = 0; it < 3000; it++) {                     /* corrupt leaves: must not crash (the frame checksum catches the rest) */
            memcpy(f, e, z); int nf = 1 + rnd() % 4;
            for (int q = 0; q < nf; q++) f[rnd() % z] ^= (uint8_t)(1 + rnd() % 255);
            np_decode_pub(f, rnd() % 8 ? z : rnd() % z, o, n);
        }
        printf("native leaf: %zu -> %zu bytes; %zu truncations rejected; 3000 corrupt leaves decoded without crashing\n", n, z, z);
        free(src); free(o); free(e); free(f);
    }
    /* native leaf in balanced frames: smaller on an integer series, version 9 only when used, round trip and corruption
     * like every mode, default output unaffected, and a reader must refuse the codec-15 leaf below version 9 */
    {
        size_t cnt = 65536, un = cnt * 2; uint8_t *u = malloc(un); uint32_t s = 12345;
        for (size_t i = 0; i < cnt; i++) {
            int g = 0; for (int k = 0; k < 4; k++) { s = (s * 1103515245u + 12345u) & 0x7fffffffu; g += (int)((s >> 16) & 0x7fff); }
            g = g / 64 - 1024; int ph = (int)(i % 148), tri = ph < 74 ? ph * 4000 / 74 - 2000 : 2000 - (ph - 74) * 4000 / 74;
            int16_t v = (int16_t)(tri + g / 4); memcpy(u + 2 * i, &v, 2);
        }
        kolmo_params pd = {KOLMO_FAST, 0, 1, 0, 1}, pn = {KOLMO_FAST, 0, 1, 0, 1 | KOLMO_T_NATIVE_LEAF};
        uint8_t *fd = NULL, *fn = NULL;
        int64_t cd = check_p("balanced-int16-series", u, un, pd, &fd), cn = check_p("balanced-native-leaf-int16-series", u, un, pn, &fn);
        if (cd > 0 && cn > 0) {
            if (cn >= cd) { printf("FAIL native leaf not smaller: %lld >= %lld\n", (long long)cn, (long long)cd); fails++; }
            if (fd[4] >= 9 || fn[4] != 9) { printf("FAIL native leaf versions: default %d, native %d\n", fd[4], fn[4]); fails++; }
            uint8_t *b = malloc(un + 1);
            for (int v = 7; v <= 8; v++) {                      /* the same frame labelled as an older version must be refused */
                fn[4] = (uint8_t)v; int64_t r = kolmo_decompress(fn, (size_t)cn, b, un, 1);
                if (r >= 0) { printf("FAIL native leaf accepted in a version %d frame\n", v); fails++; }
            }
            fn[4] = 9; free(b);
            printf("native leaf int16 series: %lld -> %lld bytes (version %d)\n", (long long)cd, (long long)cn, fn[4]);
        }
        kolmo_params pr = {KOLMO_MAX, 0, 1, 0, 1 | KOLMO_T_NATIVE_LEAF}; uint8_t *fr = NULL, *fr0 = NULL;   /* ignored outside balanced */
        kolmo_params pr0 = {KOLMO_MAX, 0, 1, 0, 1};
        int64_t a = check_p("max-native-flag-ignored", u, un, pr, &fr), a0 = check_p("max-default", u, un, pr0, &fr0);
        if (a > 0 && a0 > 0 && (a != a0 || memcmp(fr, fr0, (size_t)a))) { printf("FAIL native flag changed a non-balanced mode\n"); fails++; }
        free(fd); free(fn); free(fr); free(fr0); free(u);
    }
    /* table codec fuzz: corrupt the transformed stream itself (bypasses the container CRC) */
    {
        int bad = 0; size_t len = 1 << 18; while (csv[len - 1] != '\n') len--;
        uint8_t *t = malloc(len + 65536), *o = malloc(len + 64), *e = malloc(len + 65536);
        int64_t tn = tbl_encode_pub(csv, len, ',', 5, 1, t);
        for (int it = 0; it < 3000 && tn > 0; it++) {
            memcpy(e, t, tn); int nflip = 1 + rnd() % 4;
            for (int f = 0; f < nflip; f++) e[rnd() % tn] ^= (uint8_t)(1 + rnd() % 255);
            size_t el = rnd() % 8 ? (size_t)tn : rnd() % tn;
            tbl_decode_pub(e, el, ',', o, len);            /* must not crash; result is checked by CRC in real use */
        }
        free(t); free(o); free(e); if (bad) fails++;
        printf("table-codec fuzz: 3000 corrupt streams decoded without crashing\n");
    }
    free(csv); free(bin); free(rndb); free(txt);
    printf(fails ? "FAILED: %d\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
