/* --target: choose the setting of each chunk by a cost model (benchmark/TRADEOFFS.md) and calibrated speeds.
 *
 * Part of the src/kolmo.c translation unit. Public entry points are in
 * src/api/features.c; the cost model, the speed tables, the chunk classifier and the per-chunk choice are here.
 *
 * A target is one of
 *   size                         smallest predicted output
 *   decode>=R, encode>=R         speed floors per thread (R: 500MB/s, 1GB/s, 2GiB/s, 800Mbit, ...), joined by ','
 *   cost:<formula>               minimise a formula of size, n, enc, dec; presets cost:cloud[=months], cost:link=R
 * and floors may be combined with size or a cost (`cost:cloud,decode>=500MB/s`). The prediction for a chunk and a
 * setting is: size from trial compressions of the chunk (whole up to 256 KiB, else three 64 KiB slices scaled to the
 * chunk), and encode and decode time = setup + chunk bytes / rate of that setting on that class of data, from the
 * calibration table (setup: the fixed cost of a call, which dominates small chunks and the context-mixing settings).
 * A setting is tried only if it meets the floors and, for a cost, if its time alone (at size 0) plus the price of the
 * trial itself does not already cost more than the best setting found so far (the formula must not decrease as size
 * or time grow): with a cost, trials are part of what is paid for. */

#define KT_NSET 9
#define KT_NCLS 6
typedef struct { const char *name; kolmo_mode mode; int level, transforms; } kt_setting;
static const kt_setting KT_SET[KT_NSET] = {
    {"speed", KOLMO_RT, 0, 3}, {"balanced-n", KOLMO_FAST, 0, 0}, {"balanced", KOLMO_FAST, 0, 1}, {"eco", KOLMO_ECO, 0, 1},
    {"hc", KOLMO_HC, 0, 1}, {"ratio-l1", KOLMO_MAX, 1, 1}, {"ratio", KOLMO_MAX, 0, 1}, {"x-l1", KOLMO_ULTRA, 1, 1},
    {"x", KOLMO_ULTRA, 0, 1}};
static const char *const KT_CLS[KT_NCLS] = {"text", "table", "log", "json", "numeric", "binary"};

/* Built-in speed table {encode MB/s, decode MB/s, encode setup ms, decode setup ms} (one thread, MB = 10^6 bytes):
 * `kolmo calibrate` on the reference cloud VM (x86-64, 4 vCPU), 16 November 2026, on the built-in synthetic data.
 * Replaced, entry by entry, by a calibration table when one is given. */
static const float KT_DEFAULT[KT_NSET][KT_NCLS][4] = {
#include "target_table.inc"
};

/* formula: reverse Polish, compiled once */
enum { KT_NUM, KT_VAR, KT_ADD, KT_SUB, KT_MUL, KT_DIV, KT_NEG };
typedef struct { uint8_t op, var; double v; } kt_op;
#define KT_MAXOP 128
struct kolmo_target {
    int objective;                 /* 0: size, 1: cost formula */
    double min_dec, min_enc;       /* bytes per second, 0 = none */
    kt_op code[KT_MAXOP]; int ncode;
    double enc[KT_NSET][KT_NCLS], dec[KT_NSET][KT_NCLS];   /* bytes per second */
    double encs[KT_NSET][KT_NCLS], decs[KT_NSET][KT_NCLS]; /* fixed seconds per call */
};

/* ---------------------------------------------------------------- parsing */
static int kt_ieq(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) { char x = a[i], y = b[i]; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
    return 1;
}
/* "1GB/s", "500 MB/s", "2GiB/s", "800Mbit", "1Gbit/s", "10Mbps", "1e9" -> bytes per second; < 0 on error */
static double kt_rate(const char *s, size_t n) {
    char tmp[64]; if (n == 0 || n >= sizeof tmp) return -1;
    memcpy(tmp, s, n); tmp[n] = 0;
    char *e; double v = strtod(tmp, &e);
    if (e == tmp || !isfinite(v) || v <= 0) return -1;
    while (*e == ' ') e++;
    size_t m = strlen(e);
    static const struct { const char *u; double f; } U[] = {
        {"", 1}, {"B/s", 1}, {"KB/s", 1e3}, {"MB/s", 1e6}, {"GB/s", 1e9}, {"KiB/s", 1024.0}, {"MiB/s", 1048576.0},
        {"GiB/s", 1073741824.0}, {"kbit", 125}, {"Mbit", 125e3}, {"Gbit", 125e6}, {"kbit/s", 125}, {"Mbit/s", 125e3},
        {"Gbit/s", 125e6}, {"kbps", 125}, {"Mbps", 125e3}, {"Gbps", 125e6}};
    for (size_t i = 0; i < sizeof U / sizeof U[0]; i++)
        if (strlen(U[i].u) == m && kt_ieq(e, U[i].u, m)) return v * U[i].f;
    return -1;
}

typedef struct { const char *s; size_t i, n; kt_op *code; int nc, err; } kt_parser;
static void kt_emit(kt_parser *p, uint8_t op, uint8_t var, double v) {
    if (p->nc >= KT_MAXOP) { p->err = 1; return; }
    p->code[p->nc++] = (kt_op){op, var, v};
}
static void kt_ws(kt_parser *p) { while (p->i < p->n && (p->s[p->i] == ' ' || p->s[p->i] == '\t')) p->i++; }
static void kt_expr(kt_parser *p, int depth);
static void kt_atom(kt_parser *p, int depth) {
    kt_ws(p);
    if (p->i >= p->n || depth > 32) { p->err = 1; return; }
    char c = p->s[p->i];
    if (c == '(') { p->i++; kt_expr(p, depth + 1); kt_ws(p); if (p->i >= p->n || p->s[p->i] != ')') p->err = 1; else p->i++; return; }
    if (c == '-') { p->i++; kt_atom(p, depth + 1); kt_emit(p, KT_NEG, 0, 0); return; }
    if ((c >= '0' && c <= '9') || c == '.') {
        char tmp[64]; size_t k = 0;
        while (p->i < p->n && k < 63 && ((p->s[p->i] >= '0' && p->s[p->i] <= '9') || p->s[p->i] == '.' || p->s[p->i] == 'e' || p->s[p->i] == 'E' ||
               ((p->s[p->i] == '-' || p->s[p->i] == '+') && k && (tmp[k - 1] == 'e' || tmp[k - 1] == 'E')))) tmp[k++] = p->s[p->i++];
        tmp[k] = 0; char *e; double v = strtod(tmp, &e);
        if (*e || !isfinite(v)) { p->err = 1; return; }
        kt_emit(p, KT_NUM, 0, v); return;
    }
    static const char *const VARS[4] = {"size", "n", "enc", "dec"};
    for (int v = 0; v < 4; v++) {
        size_t l = strlen(VARS[v]);
        if (p->n - p->i >= l && !memcmp(p->s + p->i, VARS[v], l) &&
            (p->i + l == p->n || !((p->s[p->i + l] | 32) >= 'a' && (p->s[p->i + l] | 32) <= 'z'))) { p->i += l; kt_emit(p, KT_VAR, (uint8_t)v, 0); return; }
    }
    p->err = 1;
}
static void kt_term(kt_parser *p, int depth) {
    kt_atom(p, depth);
    for (;;) {
        kt_ws(p); if (p->err || p->i >= p->n) return;
        char c = p->s[p->i]; if (c != '*' && c != '/') return;
        p->i++; kt_atom(p, depth); kt_emit(p, c == '*' ? KT_MUL : KT_DIV, 0, 0);
    }
}
static void kt_expr(kt_parser *p, int depth) {
    kt_term(p, depth);
    for (;;) {
        kt_ws(p); if (p->err || p->i >= p->n) return;
        char c = p->s[p->i]; if (c != '+' && c != '-') return;
        p->i++; kt_term(p, depth); kt_emit(p, c == '+' ? KT_ADD : KT_SUB, 0, 0);
    }
}
static double kt_eval(const struct kolmo_target *t, double size, double n, double enc, double dec) {
    double st[KT_MAXOP]; int sp = 0; const double var[4] = {size, n, enc, dec};
    for (int i = 0; i < t->ncode; i++) {
        const kt_op *o = &t->code[i];
        switch (o->op) {
            case KT_NUM: st[sp++] = o->v; break;
            case KT_VAR: st[sp++] = var[o->var]; break;
            case KT_NEG: st[sp - 1] = -st[sp - 1]; break;
            default: { double b = st[--sp], a = st[sp - 1];
                st[sp - 1] = o->op == KT_ADD ? a + b : o->op == KT_SUB ? a - b : o->op == KT_MUL ? a * b : a / b; }
        }
    }
    return sp == 1 && isfinite(st[0]) ? st[0] : HUGE_VAL;
}
/* stack depth check: every program the parser accepts leaves exactly one value */
static int kt_check(const kt_op *c, int n) {
    int sp = 0;
    for (int i = 0; i < n; i++) {
        if (c[i].op == KT_NUM || c[i].op == KT_VAR) sp++;
        else if (c[i].op == KT_NEG) { if (sp < 1) return 0; }
        else { if (sp < 2) return 0; sp--; }
    }
    return sp == 1;
}

static void kt_free_target(struct kolmo_target *t) { free(t); }
static void kt_default_table(struct kolmo_target *t) {
    for (int s = 0; s < KT_NSET; s++)
        for (int c = 0; c < KT_NCLS; c++) {
            t->enc[s][c] = KT_DEFAULT[s][c][0] * 1e6; t->dec[s][c] = KT_DEFAULT[s][c][1] * 1e6;
            t->encs[s][c] = KT_DEFAULT[s][c][2] * 1e-3; t->decs[s][c] = KT_DEFAULT[s][c][3] * 1e-3;
        }
}
/* predicted seconds for n bytes */
static double kt_enc_time(const struct kolmo_target *t, int s, int c, double n) { return t->encs[s][c] + n / t->enc[s][c]; }
static double kt_dec_time(const struct kolmo_target *t, int s, int c, double n) { return t->decs[s][c] + n / t->dec[s][c]; }

static int kt_parse(const char *spec, struct kolmo_target **out) {
    *out = NULL;
    if (!spec) return KOLMO_ERR_TARGET;
    struct kolmo_target *t = calloc(1, sizeof *t);
    if (!t) return KOLMO_ERR_ALLOC;
    kt_default_table(t);
    int have_obj = 0, have_floor = 0;
    const char *s = spec;
    char formula[512];
    while (*s) {
        const char *e = strchr(s, ','); size_t l = e ? (size_t)(e - s) : strlen(s);
        while (l && *s == ' ') { s++; l--; }
        while (l && s[l - 1] == ' ') l--;
        if (l == 4 && !memcmp(s, "size", 4) && !have_obj) { t->objective = 0; have_obj = 1; }
        else if (l > 8 && (!memcmp(s, "decode>=", 8) || !memcmp(s, "encode>=", 8))) {
            double r = kt_rate(s + 8, l - 8); if (r <= 0) goto bad;
            if (s[0] == 'd') t->min_dec = r; else t->min_enc = r;
            have_floor = 1;
        } else if (l > 5 && !memcmp(s, "cost:", 5) && !have_obj) {
            const char *f = s + 5; size_t fl = l - 5;
            if (fl >= 5 && !memcmp(f, "cloud", 5)) {
                double months = 1;
                if (fl > 5) {
                    if (f[5] != '=' || fl - 6 >= 32) goto bad;
                    char tmp[32]; memcpy(tmp, f + 6, fl - 6); tmp[fl - 6] = 0; char *x; months = strtod(tmp, &x);
                    if (*x || !(months > 0) || !isfinite(months)) goto bad;
                }
                /* TRADEOFFS.md model 3: storage for the lifetime, CPU for one compression and 3.3 decompressions */
                snprintf(formula, sizeof formula, "size/1e9*0.023*%.17g + (enc + 3.3*dec)*0.054/3600", months);
            } else if (fl > 5 && !memcmp(f, "link=", 5)) {
                double r = kt_rate(f + 5, fl - 5); if (r <= 0) goto bad;
                snprintf(formula, sizeof formula, "enc + size/%.17g + dec", r);   /* TRADEOFFS.md model 2 */
            } else {
                if (fl >= sizeof formula) goto bad;
                memcpy(formula, f, fl); formula[fl] = 0;
            }
            kt_parser p = {formula, 0, strlen(formula), t->code, 0, 0};
            kt_expr(&p, 0); kt_ws(&p);
            if (p.err || p.i != p.n || !kt_check(t->code, p.nc)) goto bad;
            t->ncode = p.nc; t->objective = 1; have_obj = 1;
        } else goto bad;
        if (!e) break;
        s = e + 1;
    }
    if (!have_obj && !have_floor) goto bad;
    *out = t; return 0;
bad:
    free(t); return KOLMO_ERR_TARGET;
}

/* calibration table text: "kolmo-calibration 1", then lines
 * "<setting> <class> <encode MB/s> <decode MB/s> [<encode setup ms> <decode setup ms>]";
 * '#' starts a comment; entries not listed keep their built-in values; setup times default to 0 */
static int kt_set_calibration(struct kolmo_target *t, const char *tab, size_t n) {
    double enc[KT_NSET][KT_NCLS], dec[KT_NSET][KT_NCLS], encs[KT_NSET][KT_NCLS], decs[KT_NSET][KT_NCLS];
    memcpy(enc, t->enc, sizeof enc); memcpy(dec, t->dec, sizeof dec); memcpy(encs, t->encs, sizeof encs); memcpy(decs, t->decs, sizeof decs);
    size_t i = 0; int header = 0;
    while (i < n) {
        size_t e = i; while (e < n && tab[e] != '\n') e++;
        char line[256]; size_t l = e - i < sizeof line - 1 ? e - i : sizeof line - 1;
        memcpy(line, tab + i, l); line[l] = 0; i = e + 1;
        char *h = strchr(line, '#'); if (h) *h = 0;
        char a[64], b[64], extra[8]; double x, y, xs = 0, ys = 0;
        int k = sscanf(line, "%63s %63s %lf %lf %lf %lf %7s", a, b, &x, &y, &xs, &ys, extra);
        if (k <= 0) continue;
        if (!header) { if (k == 2 && !strcmp(a, "kolmo-calibration") && !strcmp(b, "1")) { header = 1; continue; } return KOLMO_ERR_TARGET; }
        if ((k != 4 && k != 6) || !isfinite(x) || !isfinite(y) || x <= 0 || y <= 0 || !isfinite(xs) || !isfinite(ys) ||
            xs < 0 || ys < 0 || xs > 1e6 || ys > 1e6) return KOLMO_ERR_TARGET;
        int s = -1, c = -1;
        for (int j = 0; j < KT_NSET; j++) if (!strcmp(a, KT_SET[j].name)) s = j;
        for (int j = 0; j < KT_NCLS; j++) if (!strcmp(b, KT_CLS[j])) c = j;
        if (s < 0 || c < 0) return KOLMO_ERR_TARGET;
        enc[s][c] = x * 1e6; dec[s][c] = y * 1e6; encs[s][c] = xs * 1e-3; decs[s][c] = ys * 1e-3;
    }
    if (!header) return KOLMO_ERR_TARGET;
    memcpy(t->enc, enc, sizeof enc); memcpy(t->dec, dec, sizeof dec); memcpy(t->encs, encs, sizeof encs); memcpy(t->decs, decs, sizeof decs);
    return 0;
}

/* ---------------------------------------------------------------- classes of data */
/* text or binary: at most 5% of the first 4 KiB outside printable ASCII, tabs, line ends and UTF-8 lead bytes is text */
static int kt_is_binary(const uint8_t *s, size_t n) {
    size_t h = n < 4096 ? n : 4096, pr = 0;
    for (size_t i = 0; i < h; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 0xC2; }
    return pr * 100 < h * 95;
}
static int kt_class(const uint8_t *s, size_t n) {
    size_t h = n < 65536 ? n : 65536;
    if (!h) return 0;
    if (n >= 6 && !memcmp(s, "\x93NUMPY", 6)) return 4;                /* a NumPy array */
    if (kt_is_binary(s, n)) {
        /* numeric arrays repeat their high bytes at a stride of 4 or 8; other binary data mostly does not */
        size_t eq1 = 0, eq4 = 0, eq8 = 0;
        for (size_t i = 8; i < h; i++) { eq1 += s[i] == s[i - 1]; eq4 += s[i] == s[i - 4]; eq8 += s[i] == s[i - 8]; }
        size_t best = eq4 > eq8 ? eq4 : eq8;
        return best * 100 > h * 25 && best > eq1 + eq1 / 4 ? 4 : 5;
    }
    size_t i = 0; while (i < h && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
    if (i < h && (s[i] == '{' || s[i] == '[')) return 3;
    /* lines: a fixed count (>= 2) of one delimiter is a table; lines that start with a digit or a bracket
     * (timestamps, levels), or whose first 10 bytes have the shape of the first line's (digits as 0, letters as a:
     * "Dec 10 06:55" and "Dec 11 07:01" match, prose does not), are a log */
    static const uint8_t D[4] = {',', ';', '\t', '|'};
    size_t lines = 0, logl = 0, same[4] = {0}; int k0[4] = {0};
    size_t p = 0; uint8_t shape0[10]; size_t sl0 = 0;
    while (p < h && lines < 2000) {
        const uint8_t *e = memchr(s + p, '\n', h - p); if (!e) break;
        size_t q = (size_t)(e - s);
        for (int d = 0; d < 4; d++) {
            int f = 0; for (size_t j = p; j < q; j++) f += s[j] == D[d];
            if (lines == 1) k0[d] = f;
            if (lines >= 1 && f == k0[d] && f >= 2) same[d]++;
        }
        uint8_t sh[10]; size_t sl = q - p < 10 ? q - p : 10;
        for (size_t j = 0; j < sl; j++) { uint8_t c = s[p + j]; sh[j] = c >= '0' && c <= '9' ? '0' : ((c | 32) >= 'a' && (c | 32) <= 'z') ? 'a' : c; }
        if (!lines) { memcpy(shape0, sh, sl); sl0 = sl; }
        if (q > p && ((s[p] >= '0' && s[p] <= '9') || s[p] == '[' || s[p] == '<' || (sl0 == 10 && sl == 10 && !memcmp(sh, shape0, 10)))) logl++;
        lines++; p = q + 1;
    }
    if (lines >= 4) {
        for (int d = 0; d < 4; d++) if (same[d] * 10 >= (lines - 1) * 9) return 1;
        if (logl * 2 >= lines) return 2;
    }
    return 0;
}

/* ---------------------------------------------------------------- one chunk */
typedef struct {
    const uint8_t *src; const uint64_t *starts; uint32_t nchunks; int inner_threads;
    const struct kolmo_target *t;
    uint8_t **frames; size_t *flen; int *chosen; double *pcost, *penc, *pdec; uint64_t *psize;
    atomic_uint next, fallback_chunks; atomic_int err;
} kt_job;

static int64_t kt_compress_setting(const uint8_t *s, size_t n, int setting, int threads, uint8_t **out) {
    kolmo_params q = {KT_SET[setting].mode, KT_SET[setting].level, threads, 0, KT_SET[setting].transforms};
    size_t cap = kolmo_bound(n, &q); uint8_t *d = malloc(cap ? cap : 1);
    if (!d) return KOLMO_ERR_ALLOC;
    int64_t r = compress_impl(s, -1, n, d, -1, cap, &q);
    if (r < 0) { free(d); return r; }
    *out = d; return r;
}

/* trial sample: the whole chunk up to 256 KiB (its trial frame is then the output), else three 64 KiB slices. Larger
 * samples (1 MiB whole, 256 KiB slices) chose as well but cost more: with a cost target, trials are paid for
 * (benchmark/rc3/features-a/RESULTS.md) */
#define KT_WHOLE (256u << 10)
#define KT_SLICE (64u << 10)
static void *kt_worker(void *arg) {
    kt_job *J = arg; const struct kolmo_target *t = J->t;
    for (;;) {
        uint32_t c = atomic_fetch_add(&J->next, 1); if (c >= J->nchunks || atomic_load(&J->err)) break;
        const uint8_t *s = J->src + J->starts[c]; size_t n = (size_t)(J->starts[c + 1] - J->starts[c]);
        int cls = kt_class(s, n), text = cls <= 3;
        /* trial slices: the whole chunk up to 1 MiB, otherwise start, middle and end (at line starts for text) */
        size_t so[3], sl[3]; int ns = 0;
        if (n <= KT_WHOLE) { so[0] = 0; sl[0] = n; ns = 1; }
        else for (int k = 0; k < 3; k++) {
            size_t a = k == 0 ? 0 : k == 1 ? (n - KT_SLICE) / 2 : n - KT_SLICE, b = a + KT_SLICE;
            if (text && k) { const uint8_t *nl = memchr(s + a, '\n', KT_SLICE / 2); if (nl) a = (size_t)(nl - s) + 1; }
            if (text && k < 2) { size_t e = b; while (e > a + KT_SLICE / 2 && s[e - 1] != '\n') e--; if (s[e - 1] == '\n') b = e; }
            so[ns] = a; sl[ns] = b - a; ns++;
        }
        size_t sample = 0; for (int k = 0; k < ns; k++) sample += sl[k];
        /* candidates in decreasing encode speed: cheap ones first, so the time bound prunes the slow ones */
        int order[KT_NSET]; for (int k = 0; k < KT_NSET; k++) order[k] = k;
        for (int a = 1; a < KT_NSET; a++)
            for (int b = a; b > 0 && kt_enc_time(t, order[b], cls, n) < kt_enc_time(t, order[b - 1], cls, n); b--) { int x = order[b]; order[b] = order[b - 1]; order[b - 1] = x; }
        int best = -1, fastest = 0; double bestv = HUGE_VAL, bsize = 0; uint8_t *bframe = NULL; size_t bflen = 0;
        for (int k = 0; k < KT_NSET; k++) if (kt_dec_time(t, k, cls, n) < kt_dec_time(t, fastest, cls, n)) fastest = k;
        for (int oi = 0; oi < KT_NSET; oi++) {
            int st = order[oi];
            double enc = kt_enc_time(t, st, cls, n), dec = kt_dec_time(t, st, cls, n);
            if (t->min_dec > 0 && n > 0 && n / dec < t->min_dec) continue;     /* floors: the predicted speed on this chunk */
            if (t->min_enc > 0 && n > 0 && n / enc < t->min_enc) continue;
            if (t->objective == 1 && best >= 0) {
                /* the trial is paid for too: try only if this setting can still win by more than the trial costs */
                double tr = kt_enc_time(t, st, cls, (double)sample);
                double lb = kt_eval(t, 0, n, enc, dec) + (kt_eval(t, 0, n, tr, 0) - kt_eval(t, 0, n, 0, 0));
                if (lb >= bestv) continue;
            }
            double size = 0; uint8_t *frame = NULL; int64_t r = 0;
            for (int k = 0; k < ns && r >= 0; k++) {
                uint8_t *f = NULL;
                r = kt_compress_setting(s + so[k], sl[k], st, 1, &f);
                if (r >= 0) { size += (double)r; if (ns == 1) frame = f; else free(f); }
            }
            if (r < 0) { atomic_store(&J->err, (int)r); free(frame); break; }
            size = ns == 1 ? size : size * (double)n / (double)sample;
            double v = t->objective == 1 ? kt_eval(t, size, n, enc, dec) : size;
            if (best < 0 || v < bestv) {
                free(bframe); bframe = frame; bflen = ns == 1 ? (size_t)size : 0;
                best = st; bestv = v; bsize = size;
            } else free(frame);
        }
        if (atomic_load(&J->err)) { free(bframe); break; }
        int fallback = best < 0;
        if (fallback) best = fastest;       /* no setting meets the floors: use the fastest predicted decoder */
        if (bframe) {                       /* the whole-chunk trial already produced the output frame */
            J->frames[c] = bframe; J->flen[c] = bflen;
        } else {
            int64_t r = kt_compress_setting(s, n, best, J->inner_threads, &J->frames[c]);
            if (r < 0) { atomic_store(&J->err, (int)r); break; }
            J->flen[c] = (size_t)r;
        }
        J->chosen[c] = best;
        J->penc[c] = kt_enc_time(t, best, cls, n); J->pdec[c] = kt_dec_time(t, best, cls, n);
        if (fallback) {
            atomic_fetch_add(&J->fallback_chunks, 1);
            /* No eligible setting had a size trial. Use the emitted chunk frame for its report. */
            bsize = (double)J->flen[c];
            bestv = t->objective ? kt_eval(t, bsize, n, J->penc[c], J->pdec[c]) : bsize;
        }
        J->psize[c] = (uint64_t)bsize; J->pcost[c] = bestv;
    }
    return NULL;
}
