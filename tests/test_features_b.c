/* Queryable archives (IDX1, column-split table blocks) and deduplicating archives (DDP1): round trips, every query
 * against an independent reference, the indexed path against the full scan, and damaged, truncated and forged
 * inputs, which must give an error or the right answer, never a wrong one. Built with ASan/UBSan by
 * `make check-features-b`. */
#define _GNU_SOURCE 1
#include "kolmo.h"
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len);
static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint64_t rs = 0x1234567;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }

typedef struct { uint8_t *p; size_t n, cap; } buf;
static void put(buf *b, const void *s, size_t n) {
    if (!n) { if (!b->p) { b->cap = 64; b->p = malloc(b->cap); } return; }
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, n); b->n += n;
}
static void puts_(buf *b, const char *s) { put(b, s, strlen(s)); }
static int same(const void *a, const void *b, size_t n) { return !n || !memcmp(a, b, n); }
static int sink(void *ctx, const uint8_t *p, size_t n) { put(ctx, p, n); return 0; }

/* ---------------------------------------------------------------- data */
static void gen_csv(buf *b, size_t rows, uint8_t d, int quotes, int irregular, int longlines) {
    char t[512];
    snprintf(t, sizeof t, "id%cname%cco2%ctemp%cnote\n", d, d, d, d); puts_(b, t);
    double co2 = 410;
    for (size_t r = 0; r < rows; r++) {
        co2 += (int)(rnd() % 21) - 10;
        const char *name = (const char *[]){"ber", "par", "lis", "osl"}[rnd() % 4];
        char note[256] = "";
        int k = rnd() % 16;
        if (quotes && k == 0) snprintf(note, sizeof note, "\"a%cb \"\"q\"\" %u\"", d, rnd() % 100);
        else if (quotes && k == 1) snprintf(note, sizeof note, "[%u%c%u]", rnd() % 9, d, rnd() % 9);
        else if (k == 2) note[0] = 0;
        else snprintf(note, sizeof note, "n%u", rnd() % 1000);
        if (irregular && rnd() % 50 == 0) snprintf(t, sizeof t, "%zu%c%s\n", r, d, name);                 /* short row */
        else if (irregular && rnd() % 50 == 0) snprintf(t, sizeof t, "%zu%c%s%c%.1f%c%c%s%cextra%cmore\n", r, d, name, d, co2 / 10, d, d, note, d, d);
        else if (rnd() % 30 == 0) snprintf(t, sizeof t, "%zu%c%s%c%c%d.%02u%c%s\n", r, d, name, d, d, (int)(rnd() % 40) - 10, rnd() % 100, d, note);
        else snprintf(t, sizeof t, "%zu%c%s%c%.1f%c%d.%02u%c%s\n", r, d, name, d, co2 / 10, d, (int)(rnd() % 40) - 10, rnd() % 100, d, note);
        puts_(b, t);
        if (longlines && rnd() % 2000 == 0) {                                  /* a line longer than a small block */
            snprintf(t, sizeof t, "%zu%cbig%c1%c2%c", r, d, d, d, d); puts_(b, t);
            size_t L = 40000 + rnd() % 40000; for (size_t i = 0; i < L; i++) { char c = (char)('a' + rnd() % 26); put(b, &c, 1); }
            puts_(b, "\n");
        }
    }
}
static void gen_json(buf *b, size_t rows, buf *expect) {
    char t[512], v[128];
    for (size_t r = 0; r < rows; r++) {
        int k = rnd() % 8;
        if (k == 0) snprintf(v, sizeof v, "\"s\\\"%u,}\"", rnd() % 100);
        else if (k == 1) snprintf(v, sizeof v, "{\"a\":[1,2,{\"co2\":%u}],\"b\":\"}\"}", rnd() % 9);
        else if (k == 2) snprintf(v, sizeof v, "null");
        else snprintf(v, sizeof v, "%u.%u", 400 + rnd() % 50, rnd() % 10);
        if (rnd() % 20 == 0) { snprintf(t, sizeof t, "{\"ts\":%zu,\"host\":\"h%u\"}\n", r, rnd() % 9); puts_(expect, "\n"); }
        else { snprintf(t, sizeof t, "{\"ts\":%zu, \"host\" : \"h%u\",\"co2\": %s,\"tag\":\"x co2 y\"}\n", r, rnd() % 9, v); puts_(expect, v); puts_(expect, "\n"); }
        puts_(b, t);
    }
}
static void gen_log(buf *b, size_t rows) {
    char t[512];
    for (size_t r = 0; r < rows; r++) {
        const char *lv = (const char *[]){"INFO", "INFO", "WARN", "ERROR"}[rnd() % 4];
        snprintf(t, sizeof t, "081109 %06zu %u %s dfs.DataNode: Receiving block blk_%s%u%u src: /10.250.%u.%u:%u\n",
                 r, rnd() % 200, lv, rnd() % 2 ? "-" : "", rnd(), rnd() % 100000, rnd() % 256, rnd() % 256, 50000 + rnd() % 10000);
        puts_(b, t);
    }
}

/* ---------------------------------------------------------------- reference */
typedef struct { const uint8_t *p; size_t n; } span;
static size_t split_lines(const buf *b, span **out) {
    size_t n = 0, cap = 1024; span *L = malloc(cap * sizeof(span));
    for (size_t i = 0; i < b->n;) {
        const uint8_t *e = memchr(b->p + i, '\n', b->n - i); size_t end = e ? (size_t)(e - b->p) : b->n;
        if (n == cap) { cap *= 2; L = realloc(L, cap * sizeof(span)); }
        L[n++] = (span){b->p + i, end - i}; i = end + 1;
    }
    *out = L; return n;
}
static int ref_field(span l, uint8_t d, int quotes, uint32_t k, uint32_t j, span *f) {
    size_t i = 0;
    for (uint32_t c = 0;; c++) {
        size_t e = i;
        if (c + 1 >= k) e = l.n;
        else { int inq = 0, inb = 0;
            for (; e < l.n; e++) { uint8_t x = l.p[e];
                if (quotes && x == '"' && !inb) inq ^= 1; else if (quotes && x == '[' && !inq) inb = 1; else if (quotes && x == ']' && !inq) inb = 0;
                else if (x == d && !inq && !inb) break; } }
        if (c == j) { *f = (span){l.p + i, e - i}; return 1; }
        if (e >= l.n) return 0;
        i = e + 1;
    }
}
static int ref_ws(span l, uint32_t j, span *f) {
    size_t n = l.n; if (n && l.p[n - 1] == '\r') n--;
    size_t i = 0;
    for (uint32_t c = 0;; c++) {
        while (i < n && (l.p[i] == ' ' || l.p[i] == '\t' || l.p[i] == '\r')) i++;
        if (i >= n) return 0;
        size_t s = i; while (i < n && l.p[i] != ' ' && l.p[i] != '\t' && l.p[i] != '\r') i++;
        if (c == j) { *f = (span){l.p + s, i - s}; return 1; }
    }
}
static int word_ch(uint8_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
static int ref_match(span l, const char *pat, int flags, regex_t *re) {
    char *z = malloc(l.n + 1); memcpy(z, l.p, l.n); z[l.n] = 0; int m = 0;
    if (flags & KOLMO_Q_FIXED) {
        size_t pl = strlen(pat);
        for (size_t i = 0; !m && i + pl <= l.n; i++) {
            int eq = (flags & KOLMO_Q_ICASE) ? !strncasecmp(z + i, pat, pl) : same(z + i, pat, pl);
            if (eq && (!(flags & KOLMO_Q_WORD) || ((!i || !word_ch((uint8_t)z[i - 1])) && (i + pl >= l.n || !word_ch((uint8_t)z[i + pl]))))) m = 1;
        }
    } else m = !regexec(re, z, 0, NULL, 0);
    free(z); return m;
}

/* the expected output of a query (no index needed) */
static void ref_query(const buf *src, span *L, size_t nl, const kolmo_query *q, int colkind, uint8_t d, uint32_t k, uint32_t col,
                      const buf *jsonexp, buf *out, uint64_t *rows) {
    regex_t re; int hasre = 0;
    if (q->pattern && !(q->flags & KOLMO_Q_FIXED)) {
        char w[512]; const char *p = q->pattern;
        if (q->flags & KOLMO_Q_WORD) { snprintf(w, sizeof w, "(^|[^[:alnum:]_])(%s)([^[:alnum:]_]|$)", p); p = w; }
        regcomp(&re, p, REG_EXTENDED | REG_NOSUB | ((q->flags & KOLMO_Q_ICASE) ? REG_ICASE : 0)); hasre = 1;
    }
    span *J = NULL; size_t nj = 0; if (jsonexp) nj = split_lines(jsonexp, &J);
    uint64_t hi = q->row_count ? q->row_first + q->row_count : UINT64_MAX; *rows = 0;
    for (size_t r = q->row_first; r < nl && r < hi; r++) {
        if (q->pattern && !ref_match(L[r], q->pattern, q->flags, &re)) continue;
        (*rows)++;
        if (q->flags & KOLMO_Q_COUNT) continue;
        if (q->flags & KOLMO_Q_LINENO) { char t[32]; snprintf(t, sizeof t, "%zu:", r + 1); puts_(out, t); }
        span f = L[r]; int has = 1;
        if (colkind == 1) has = ref_field(L[r], d, d == ',' || d == ';', k, col, &f);
        else if (colkind == 2) { has = r < nj; if (has) f = J[r]; }
        else if (colkind == 3) has = ref_ws(L[r], col, &f);
        if (has) put(out, f.p, f.n);
        puts_(out, "\n");
    }
    if (hasre) regfree(&re);
    free(J); (void)src;
}

/* runs q on the indexed and the scan path and compares both with the reference */
static void run_q(const char *what, const uint8_t *z, size_t zn, const buf *src, span *L, size_t nl, kolmo_query q,
                  int colkind, uint8_t d, uint32_t k, uint32_t col, const buf *jsonexp) {
    buf want = {0}; uint64_t wr; ref_query(src, L, nl, &q, colkind, d, k, col, jsonexp, &want, &wr);
    for (int scan = 0; scan < 2; scan++) {
        kolmo_query qq = q; if (scan) qq.flags |= KOLMO_Q_NOINDEX;
        buf got = {0}; kolmo_query_stats st;
        int64_t r = kolmo_query_run(z, zn, &qq, sink, &got, &st);
        CHECK(r == (int64_t)wr && got.n == want.n && (!got.n || same(got.p, want.p, got.n)),
              "%s (%s): rows %lld want %llu, %zu bytes want %zu [col %s pat %s rows %llu+%llu flags %d]", what, scan ? "scan" : "index",
              (long long)r, (unsigned long long)wr, got.n, want.n, q.column ? q.column : "-", q.pattern ? q.pattern : "-",
              (unsigned long long)q.row_first, (unsigned long long)q.row_count, q.flags);
        free(got.p);
    }
    free(want.p);
}

static void put_crc_section(uint8_t *z, size_t zn, const char tag[4]) {   /* recompute a section's CRC after editing it */
    const uint8_t *t = z + zn - 24; uint64_t total; memcpy(&total, t + 12, 8);
    size_t p = zn - 24 - (size_t)total;
    while (p < zn - 24) {
        uint64_t l; memcpy(&l, z + p + 4, 8);
        if (same(z + p, tag, 4)) { uint32_t c = kolmo_crc32c(0, z + p, 12 + (size_t)l); memcpy(z + p + 12 + l, &c, 4); }
        p += 16 + (size_t)l;
    }
}
static const uint8_t *find_section(uint8_t *z, size_t zn, const char tag[4], uint64_t *len) {
    const uint8_t *t = z + zn - 24; uint64_t total; memcpy(&total, t + 12, 8);
    size_t p = zn - 24 - (size_t)total;
    while (p < zn - 24) { uint64_t l; memcpy(&l, z + p + 4, 8); if (same(z + p, tag, 4)) { *len = l; return z + p + 12; } p += 16 + (size_t)l; }
    return NULL;
}

/* ---------------------------------------------------------------- IDX1 */
static void test_index_one(const char *name, buf *src, int kind, uint8_t d, uint32_t k, const char *colname, uint32_t colidx, const buf *jsonexp,
                           kolmo_mode mode, uint32_t block, int threads) {
    kolmo_params P = {mode, 0, threads, block, 1};
    kolmo_index_params I = {KOLMO_IDX_AUTO, 0, 1, 1 + (int)(rnd() % 6)};
    size_t cap = kolmo_indexed_bound(src->n, &P); uint8_t *z = malloc(cap);
    int64_t zn = kolmo_compress_indexed(src->p, src->n, z, cap, &P, &I);
    char what[160]; snprintf(what, sizeof what, "%s mode %d block %u", name, (int)mode, block);
    CHECK(zn > 0, "%s: compress_indexed %lld", what, (long long)zn);
    if (zn <= 0) { free(z); return; }
    uint8_t *dec = malloc(src->n + 1);
    CHECK(kolmo_decompress(z, (size_t)zn, dec, src->n, threads) == (int64_t)src->n && same(dec, src->p, src->n), "%s: round trip", what);
    free(dec);
    int ik; uint8_t idl; char names[1024];
    int64_t rows = kolmo_index_info(z, (size_t)zn, &ik, &idl, names, sizeof names);
    span *L; size_t nl = split_lines(src, &L);
    CHECK(rows == (int64_t)nl && ik == kind && (kind != KOLMO_IDX_TABLE || idl == d), "%s: info rows %lld/%zu kind %d/%d", what, (long long)rows, nl, ik, kind);
    int colkind = kind == KOLMO_IDX_TABLE ? 1 : kind == KOLMO_IDX_JSON ? 2 : 3;
    for (int t = 0; t < 14; t++) {
        kolmo_query q = {0}; q.threads = threads;
        if (t % 3) { q.row_first = nl ? rnd() % (nl + 2) : 0; q.row_count = rnd() % 3 ? rnd() % (nl + 3) : 0; }
        uint32_t col = 0; int ck = 0; char cn[32];
        if (t % 2 && colname) {
            if (kind == KOLMO_IDX_JSON) { q.column = colname; ck = 2; }
            else if (kind == KOLMO_IDX_TABLE && t % 4 == 1) { q.column = colname; col = colidx; ck = 1; }
            else if (kind != KOLMO_IDX_JSON) { col = rnd() % 7; snprintf(cn, sizeof cn, "#%u", col + 1); q.column = cn; ck = colkind; }
        }
        char pat[64];
        if (t >= 6) {
            if (nl && t % 3 == 0) {                                         /* a piece of a line: fixed, maybe a word */
                span l = L[rnd() % nl]; size_t a = l.n ? rnd() % l.n : 0, len = 3 + rnd() % 8; if (a + len > l.n) len = l.n - a;
                size_t o = 0; for (size_t i = 0; i < len && o < 60; i++) if (l.p[a + i] != '\n' && l.p[a + i] != 0) pat[o++] = (char)l.p[a + i];
                pat[o] = 0; q.pattern = pat; q.flags |= KOLMO_Q_FIXED | (rnd() % 2 ? KOLMO_Q_ICASE : 0) | (rnd() % 3 == 0 ? KOLMO_Q_WORD : 0);
            } else if (t % 3 == 1) { q.pattern = rnd() % 2 ? "co2|ERROR" : "b[a-z]g?[0-9]+"; q.flags |= rnd() % 2 ? KOLMO_Q_WORD : 0; }
            else { q.pattern = rnd() % 2 ? "absentzzq" : "ERRor"; q.flags |= KOLMO_Q_FIXED | (rnd() % 2 ? KOLMO_Q_ICASE | KOLMO_Q_WORD : 0); }
            if (!q.pattern[0]) q.pattern = "x";
        }
        if (rnd() % 3 == 0) q.flags |= KOLMO_Q_LINENO;
        if (rnd() % 5 == 0) q.flags |= KOLMO_Q_COUNT;
        if (ck == 0) q.column = NULL;
        run_q(what, z, (size_t)zn, src, L, nl, q, ck, d, k, col, jsonexp);
    }
    /* a column of a table is read through the column streams, not by decoding whole blocks */
    if (kind == KOLMO_IDX_TABLE && colname && src->n > 65536 && block == 0) {
        kolmo_query q = {0}; q.column = colname; q.threads = threads; kolmo_query_stats st;
        int64_t r = kolmo_query_run(z, (size_t)zn, &q, NULL, NULL, &st);
        CHECK(r == (int64_t)nl && st.blocks_columnar > 0 && st.bytes_decoded < src->n, "%s: column path not taken (%u columnar, %u whole, %llu bytes decoded)",
              what, st.blocks_columnar, st.blocks_decoded, (unsigned long long)st.bytes_decoded);
    }
    /* unknown column: an error, not an empty answer */
    if (kind == KOLMO_IDX_TABLE) { kolmo_query q = {0}; q.column = "no-such-column"; CHECK(kolmo_query_run(z, (size_t)zn, &q, NULL, NULL, NULL) == KOLMO_ERR_QUERY, "%s: unknown column", what); }
    {   kolmo_query q = {0}; q.pattern = "(unclosed"; CHECK(kolmo_query_run(z, (size_t)zn, &q, NULL, NULL, NULL) == KOLMO_ERR_QUERY, "%s: bad regex", what); }
    /* damaged copies: every query result is an error or the right answer */
    uint8_t *y = malloc((size_t)zn);
    for (int t = 0; t < 40; t++) {
        memcpy(y, z, (size_t)zn); size_t yn = (size_t)zn;
        int how = t % 4;
        if (how == 0) { size_t at = (size_t)zn - 1 - rnd() % ((size_t)zn < 4096 ? (size_t)zn : 4096); y[at] ^= (uint8_t)(1 + rnd() % 255); }   /* near the end: index, trailer */
        else if (how == 1) { size_t at = rnd() % (size_t)zn; y[at] ^= (uint8_t)(1 + rnd() % 255); }
        else if (how == 2) yn = rnd() % (size_t)zn;                                                                  /* truncated */
        else {                                                    /* forged index: a line count changed, CRC recomputed */
            /* only the line counts and flags: a forged filter (valid CRC) may legitimately rule out a block (SPEC.md) */
            uint64_t len; uint8_t *ix = (uint8_t *)find_section(y, yn, "IDX1", &len);
            if (!ix || len < 24) continue;
            size_t i = 20, e; uint32_t nb; memcpy(&nb, ix + 8, 4);
            uint64_t v = 0; int sh = 0; while (ix[i] & 128) { v |= (uint64_t)(ix[i++] & 127) << sh; sh += 7; } v |= (uint64_t)ix[i++] << sh;
            for (uint64_t q = 0; q < v; q++) { uint64_t l = 0; sh = 0; while (ix[i] & 128) { l |= (uint64_t)(ix[i++] & 127) << sh; sh += 7; } l |= (uint64_t)ix[i++] << sh; i += l; }
            size_t s0 = i; for (uint32_t b = 0; b < nb; b++) { i++; while (ix[i++] & 128) {} }
            e = i; if (e <= s0) continue;
            size_t at = s0 + rnd() % (e - s0); ix[at] ^= (uint8_t)(1 << (rnd() % 8));
            put_crc_section(y, yn, "IDX1");
        }
        kolmo_query q = {0}; q.threads = threads;
        if (t % 2 && kind == KOLMO_IDX_TABLE && colname) q.column = colname;
        if (t % 3 == 1) { q.pattern = "co2"; q.flags = KOLMO_Q_FIXED; }
        buf want = {0}; uint64_t wr; ref_query(src, L, nl, &q, q.column ? colkind : 0, d, k, colidx, jsonexp, &want, &wr);
        buf got = {0}; int64_t r = kolmo_query_run(y, yn, &q, sink, &got, NULL);
        CHECK(r < 0 || (r == (int64_t)wr && got.n == want.n && same(got.p, want.p, got.n)),
              "%s: damaged copy (how %d) gave a wrong answer: %lld rows, %zu bytes (want %llu, %zu)", what, how, (long long)r, got.n, (unsigned long long)wr, want.n);
        free(got.p); free(want.p);
    }
    free(y); free(L); free(z);
}

static void test_index(void) {
    static const kolmo_mode modes[] = {KOLMO_FAST, KOLMO_HC, KOLMO_MAX};
    static const uint32_t blocks[] = {4096, 65536, 0};
    for (int m = 0; m < 3; m++) for (int bi = 0; bi < 3; bi++) {
        int threads = 1 + (int)(rnd() % 4);
        buf c = {0}; gen_csv(&c, 3000 + rnd() % 20000, ',', 1, 1, bi == 1); test_index_one("csv", &c, KOLMO_IDX_TABLE, ',', 5, "co2", 2, NULL, modes[m], blocks[bi], threads); free(c.p);
        if (m == 2 && bi == 2) continue;
        buf tsv = {0}; gen_csv(&tsv, 2000 + rnd() % 9000, '\t', 0, bi == 0, 0); if (rnd() % 2 && tsv.n) tsv.n--;   /* maybe no final newline */
        test_index_one("tsv", &tsv, KOLMO_IDX_TABLE, '\t', 5, "co2", 2, NULL, modes[m], blocks[bi], threads); free(tsv.p);
        buf j = {0}, je = {0}; gen_json(&j, 2000 + rnd() % 9000, &je); test_index_one("json", &j, KOLMO_IDX_JSON, 0, 0, "co2", 0, &je, modes[m], blocks[bi], threads); free(j.p); free(je.p);
        buf l = {0}; gen_log(&l, 3000 + rnd() % 9000); test_index_one("log", &l, KOLMO_IDX_LINES, 0, 0, "#4", 3, NULL, modes[m], blocks[bi], threads); free(l.p);
    }
    /* edge cases: empty, one line without newline, only newlines, binary */
    const char *edge[] = {"", "one line", "\n\n\n", "a,b\n", "a,b\n1,2"};
    for (int e = 0; e < 5; e++) { buf b = {0}; put(&b, edge[e], strlen(edge[e])); test_index_one("edge", &b, e >= 4 ? KOLMO_IDX_TABLE : KOLMO_IDX_LINES, ',', 2, e >= 4 ? "b" : NULL, 1, NULL, KOLMO_FAST, 0, 2); free(b.p); }
    buf bin = {0}; for (int i = 0; i < 300000; i++) { uint8_t x = (uint8_t)rnd(); put(&bin, &x, 1); }
    test_index_one("binary", &bin, KOLMO_IDX_LINES, 0, 0, "#2", 1, NULL, KOLMO_FAST, 65536, 3); free(bin.p);
    /* a plain frame (no index) answers through the scan path */
    {   buf c = {0}; gen_csv(&c, 5000, ',', 1, 1, 0); kolmo_params P = {KOLMO_FAST, 0, 2, 65536, 1};
        size_t cap = kolmo_bound(c.n, &P); uint8_t *z = malloc(cap); int64_t zn = kolmo_compress(c.p, c.n, z, cap, &P);
        span *L; size_t nl = split_lines(&c, &L);
        kolmo_query q = {0}; q.column = "co2"; q.row_first = 10; q.row_count = 1000;
        run_q("plain frame", z, (size_t)zn, &c, L, nl, q, 1, ',', 5, 2, NULL);
        q.column = NULL; q.pattern = "ber"; q.flags = KOLMO_Q_FIXED | KOLMO_Q_LINENO;
        run_q("plain frame", z, (size_t)zn, &c, L, nl, q, 0, ',', 5, 0, NULL);
        free(L); free(z); free(c.p); }
}

/* column-split table blocks: round trips in every mode, and damaged payloads never decode to wrong bytes */
static void test_colsplit(void) {
    for (int m = 0; m <= 4; m++) {
        buf c = {0}; gen_csv(&c, 20000, m % 2 ? ';' : ',', 1, m >= 2, 0);
        kolmo_params P = {(kolmo_mode)m, 0, 2, 65536, 1 | KOLMO_T_COLUMNS};
        size_t cap = kolmo_bound(c.n, &P); uint8_t *z = malloc(cap), *d = malloc(c.n + 1);
        int64_t zn = kolmo_compress(c.p, c.n, z, cap, &P);
        CHECK(zn > 0 && kolmo_decompress(z, (size_t)zn, d, c.n, 2) == (int64_t)c.n && same(d, c.p, c.n), "column-split mode %d round trip", m);
        for (int t = 0; zn > 0 && t < 60; t++) {
            uint8_t *y = malloc((size_t)zn); memcpy(y, z, (size_t)zn);
            y[32 + rnd() % ((size_t)zn - 32)] ^= (uint8_t)(1 + rnd() % 255);
            int64_t r = kolmo_decompress(y, (size_t)zn, d, c.n, 1);
            CHECK(r < 0 || (r == (int64_t)c.n && same(d, c.p, c.n)), "column-split mode %d: damaged frame decoded to wrong bytes", m);
            free(y);
        }
        free(z); free(d); free(c.p);
    }
}

/* ---------------------------------------------------------------- DDP1 */
static int collect(void *ctx, const char *name, const uint8_t *data, uint64_t size, uint32_t mode, int64_t mtime) {
    buf *b = ctx; char t[64]; snprintf(t, sizeof t, "|%s|%llu|%o|%lld|", name, (unsigned long long)size, mode, (long long)mtime);
    puts_(b, t); put(b, data, (size_t)size); return 0;
}
static void test_dedup(void) {
    for (int round = 0; round < 6; round++) {
        int nf = 1 + (int)(rnd() % 12); buf *F = calloc((size_t)nf, sizeof(buf)); kolmo_archive_file *K = calloc((size_t)nf, sizeof(kolmo_archive_file));
        char (*names)[32] = malloc((size_t)nf * 32);
        buf base = {0}; if (round % 2) gen_csv(&base, 4000 + rnd() % 8000, ',', 1, 0, 0); else for (int i = 0; i < 200000; i++) { uint8_t x = (uint8_t)(rnd() % 7); put(&base, &x, 1); }
        for (int f = 0; f < nf; f++) {
            int how = rnd() % 5;
            if (how == 0 || !f) put(&F[f], base.p, base.n);                                         /* a copy */
            else if (how == 1) { buf *s = &F[rnd() % f]; size_t a = s->n ? rnd() % s->n : 0;             /* an insertion */
                put(&F[f], s->p, a); for (int i = 0; i < 100; i++) { uint8_t x = (uint8_t)rnd(); put(&F[f], &x, 1); } put(&F[f], s->p + a, s->n - a); }
            else if (how == 2) { buf *s = &F[rnd() % f]; put(&F[f], s->p, s->n); for (int e = 0; e < 5 && F[f].n; e++) F[f].p[rnd() % F[f].n] ^= 1; }  /* edits */
            else if (how == 3) { }                                                                    /* empty */
            else { buf *s = &F[rnd() % f]; size_t a = s->n ? rnd() % s->n : 0; put(&F[f], s->p + a, s->n - a); put(&F[f], base.p, base.n / 3); }
            snprintf(names[f], 32, "dir%d/file%d.dat", f % 3, f);
            K[f] = (kolmo_archive_file){names[f], F[f].p ? F[f].p : (const uint8_t *)"", F[f].n, 0644, 1700000000 + f};
        }
        kolmo_params P = {round % 3 == 2 ? KOLMO_HC : KOLMO_FAST, 0, 1 + (int)(rnd() % 4), round % 2 ? 65536 : 0, 1};
        kolmo_archive_params A = {256u << (rnd() % 8), (int)(rnd() % 2)};
        size_t cap = kolmo_archive_bound(K, (size_t)nf, &P); uint8_t *z = malloc(cap);
        kolmo_archive_stats st; int64_t zn = kolmo_archive_create(K, (size_t)nf, z, cap, &P, &A, &st);
        CHECK(zn > 0, "archive create %lld", (long long)zn);
        if (zn <= 0) goto next;
        CHECK(kolmo_archive_count(z, (size_t)zn) == nf, "archive count");
        buf want = {0};
        for (int f = 0; f < nf; f++) {
            char nm[64]; uint64_t sz; uint32_t md; int64_t mt;
            CHECK(!kolmo_archive_stat(z, (size_t)zn, (uint64_t)f, nm, sizeof nm, &sz, &md, &mt) && !strcmp(nm, names[f]) && sz == F[f].n && md == 0644 && mt == 1700000000 + f, "stat %d", f);
            uint8_t *d = malloc(F[f].n + 1);
            CHECK(kolmo_archive_extract(z, (size_t)zn, (uint64_t)f, d, F[f].n, 2) == (int64_t)F[f].n && same(d, F[f].p, F[f].n), "extract %d", f);
            free(d);
            collect(&want, names[f], F[f].p, F[f].n, 0644, 1700000000 + f);
        }
        buf all = {0};
        CHECK(kolmo_archive_extract_all(z, (size_t)zn, collect, &all, 3) == nf && all.n == want.n && same(all.p, want.p, all.n), "extract all");
        free(all.p);
        /* damaged copies: an error or the right bytes */
        for (int t = 0; t < 40; t++) {
            uint8_t *y = malloc((size_t)zn); memcpy(y, z, (size_t)zn); size_t yn = (size_t)zn;
            if (t % 3 == 2) yn = rnd() % (size_t)zn; else y[t % 3 ? (size_t)zn - 1 - rnd() % ((size_t)zn < 8192 ? (size_t)zn : 8192) : rnd() % (size_t)zn] ^= (uint8_t)(1 + rnd() % 255);
            buf got = {0}; int64_t r = kolmo_archive_extract_all(y, yn, collect, &got, 1);
            CHECK(r < 0 || (got.n == want.n && same(got.p, want.p, got.n)), "damaged archive gave wrong files");
            free(got.p);
            int f = (int)(rnd() % (unsigned)nf); uint8_t *d = malloc(F[f].n + 1);
            r = kolmo_archive_extract(y, yn, (uint64_t)f, d, F[f].n, 1);
            CHECK(r < 0 || (r == (int64_t)F[f].n && same(d, F[f].p, F[f].n)), "damaged archive gave a wrong file");
            free(d); free(y);
        }
        free(want.p);
    next:
        for (int f = 0; f < nf; f++) free(F[f].p);
        free(F); free(K); free(names); free(base.p); free(z);
    }
    /* parameter checks */
    kolmo_archive_file one = {"x", (const uint8_t *)"abc", 3, 0, 0}; kolmo_archive_params bad = {1000, 1}; uint8_t z[4096];
    CHECK(kolmo_archive_create(&one, 1, z, sizeof z, NULL, &bad, NULL) == KOLMO_ERR_LIMIT, "chunk size not a power of two");
    CHECK(kolmo_archive_count(z, 10) < 0, "not an archive");
}

int main(int argc, char **argv) {
    if (argc > 1) rs = strtoull(argv[1], NULL, 0) | 1;
    test_colsplit();
    test_index();
    test_dedup();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
