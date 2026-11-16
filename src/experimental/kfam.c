/* kfam: research tool for the family-stream coder. Apache-2.0, Kolmo contributors.
 *
 * The input is split into streams by family (FASTQ: layout, read names, bases, base exceptions, qualities; CSV:
 * one stream per column; anything else: one stream). Each stream is coded by a small model dedicated to its family
 * and a binary arithmetic coder with 16-bit probabilities, all written for Kolmo (no third-party coder). Large
 * streams are cut into chunks with fresh models, and the chunks are coded on separate threads.
 *
 *   kfam [-T threads] [-C chunk] [-B chunk] [-f auto|fastq|csv|one] in out      compress
 *   kfam -d [-T threads] in out                                        decompress
 *   kfam -b [-T threads] [-C chunk] [-f ...] in                        in-memory timing, JSON line (fastest of 3+ runs)
 *
 * -C: symbols per chunk of a names or qualities stream (and bytes per chunk of a one-stream input), default 4 Mi;
 * -B: bases per chunk of the bases stream, default 64 Mi; 0 means no chunking. -v: compress once and print the coded
 * size of each stream; -S k: code FASTQ stream k alone (model experiments); -S 9: code every job alone, measure its
 * CPU time, and print the speed-up bound of the thread pool's schedule (largest job first) at 4 and 10 threads. The FASTQ family and the frame are in src/kfam.inc, which kolmo uses as block codec
 * 16; this file adds the CSV and one-stream families and the command line. */
#include <stdio.h>
#include <time.h>
#include "../kfam.inc"

/* CSV / TSV (one delimiter, the same field count on every line, no quotes, "\n" endings):
 *   stream 0  layout: delimiter, column count, row count, per column its type (0 text, 1 number), then the header line
 *   stream 1+c column c.
 *     number columns (every non-empty cell is -?(0|[1-9]d*)(.d{1,9})? with at most 17 digits): per cell a header
 *       byte h (0 empty; else 1 + fraction digits, plus 16 when the value is coded as a delta from the column's
 *       previous number with the same fraction digits), then the zigzag mantissa (or delta) as a varint. Contexts:
 *       the previous header for a header, the byte index and the delta flag for a varint byte.
 *     text columns: the cells separated by '\n'. */
static int parse_num(kf_span t, int64_t *mant, int *frac) {
    size_t i = 0; int neg = 0; if (i < t.n && t.p[i] == '-') { neg = 1; i++; }
    size_t a = i; uint64_t r = 0; int nd = 0, fr = 0;
    while (i < t.n && t.p[i] >= '0' && t.p[i] <= '9') { r = r * 10 + (uint64_t)(t.p[i] - '0'); i++; nd++; }
    if (i == a || nd > 17 || (t.p[a] == '0' && i - a > 1)) return 0;
    if (i < t.n && t.p[i] == '.') {
        i++;
        while (i < t.n && t.p[i] >= '0' && t.p[i] <= '9') { r = r * 10 + (uint64_t)(t.p[i] - '0'); i++; nd++; fr++; }
        if (!fr || fr > 9 || nd > 17) return 0;
    }
    if (i != t.n || (neg && r == 0)) return 0;
    *mant = neg ? -(int64_t)r : (int64_t)r; *frac = fr; return 1;
}
static int csv_fields(kf_span l, uint8_t d, kf_span *f, int max) {
    int k = 0; size_t i = 0, s = 0;
    for (;; i++) { if (i == l.n || l.p[i] == d) { if (k >= max) return -1; f[k].p = l.p + s; f[k].n = i - s; k++; s = i + 1; if (i == l.n) break; } }
    return k;
}
enum { MAXCOL = 1024 };
static int csv_split(const uint8_t *s, size_t n, kf_stream **pst, int *pns) {
    if (!n || s[n - 1] != '\n' || memchr(s, '"', n) || memchr(s, '\r', n)) return -1;
    size_t i = 0; kf_span hdr; if (kf_line(s, n, &i, &hdr)) return -1;
    static const uint8_t cand[3] = {',', '\t', ';'}; uint8_t d = 0; size_t best = 0;
    for (int c = 0; c < 3; c++) { size_t k = 0; for (size_t q = 0; q < hdr.n; q++) k += hdr.p[q] == cand[c]; if (k > best) { best = k; d = cand[c]; } }
    if (!best) return -1;
    kf_span *f = malloc(sizeof(kf_span) * MAXCOL); int *type = malloc(sizeof(int) * MAXCOL); int64_t *pm = calloc(MAXCOL, sizeof(int64_t)); int *pf = malloc(sizeof(int) * MAXCOL);
    int nc, rc = -1, *ph = NULL; kf_stream *st = NULL;
    if (!f || !type || !pm || !pf || (nc = csv_fields(hdr, d, f, MAXCOL)) < 2) goto out;
    size_t body = i, rows = 0;
    for (int c = 0; c < nc; c++) type[c] = 1;
    for (size_t j = body; j < n; ) {                         /* pass 1: field counts and column types */
        kf_span l; if (kf_line(s, n, &j, &l) || csv_fields(l, d, f, MAXCOL) != nc) goto out;
        for (int c = 0; c < nc; c++) { int64_t m; int fr; if (type[c] && f[c].n && !parse_num(f[c], &m, &fr)) type[c] = 0; }
        rows++;
    }
    st = calloc((size_t)nc + 1, sizeof *st); if (!st) goto out;
    *pst = st; *pns = nc + 1;
    kf_buf *L = &st[0].data;
    if (kf_byte(L, d) || kf_var(L, (uint64_t)nc) || kf_var(L, rows)) goto out;
    for (int c = 0; c < nc; c++) if (kf_byte(L, (uint8_t)type[c])) goto out;
    if (kf_put(L, hdr.p, hdr.n)) goto out;
    for (int c = 0; c < nc; c++) pf[c] = -1;
    ph = calloc((size_t)nc, sizeof(int)); if (!ph) goto out;
    for (size_t j = body; j < n; ) {
        kf_span l; kf_line(s, n, &j, &l); csv_fields(l, d, f, MAXCOL);
        for (int c = 0; c < nc; c++) {
            kf_buf *D = &st[1 + c].data, *C = &st[1 + c].ctx;
            if (!type[c]) { if (kf_put(D, f[c].p, f[c].n) || kf_byte(D, '\n')) goto out; continue; }
            int h = 0; int64_t m = 0; int fr = 0;
            if (f[c].n) { parse_num(f[c], &m, &fr); h = 1 + fr; if (pf[c] == fr) h += 16; }
            if (kf_byte(D, (uint8_t)h) || kf_byte(C, (uint8_t)(128 + (ph[c] & 31)))) goto out;
            ph[c] = h;
            if (!h) continue;
            uint64_t z = h >= 17 ? kf_zz(m - pm[c]) : kf_zz(m); pm[c] = m; pf[c] = fr;
            size_t before = D->n; if (kf_var(D, z)) goto out;
            for (size_t q = 0; q < D->n - before; q++) if (kf_byte(C, (uint8_t)((q < 9 ? q : 9) + (h >= 17 ? 10 : 0)))) goto out;
        }
    }
    rc = 0;
out:
    free(ph); free(f); free(type); free(pm); free(pf);
    return rc;
}
/* decoder side of the number columns' contexts: a header, then (unless empty) a varint */
typedef struct { int state, byte, delta, ph; } numctx;
static int num_ctx(void *arg, size_t i, const uint8_t *o) {
    numctx *c = arg;
    if (i > 0) {
        uint8_t v = o[i - 1];
        if (c->state == 0) { c->ph = v; if (v) { c->state = 1; c->byte = 0; c->delta = v >= 17; } }
        else if (v & 128) c->byte++;
        else c->state = 0;
    }
    return c->state == 0 ? 128 + (c->ph & 31) : (c->byte < 9 ? c->byte : 9) + (c->delta ? 10 : 0);
}
static int csv_join(kf_stream *st, int ns, kf_buf *out) {
    const uint8_t *L = st[0].data.p; size_t li = 0, ln = st[0].data.n; uint64_t nc, rows;
    if (ln < 1) return -1;
    uint8_t d = L[li++];
    if (kf_getvar(L, ln, &li, &nc) || kf_getvar(L, ln, &li, &rows) || nc + 1 != (uint64_t)ns || nc > MAXCOL || li + nc > ln) return -1;
    const uint8_t *type = L + li; li += nc;
    if (kf_put(out, L + li, ln - li) || kf_byte(out, '\n')) return -1;
    size_t *pos = calloc(nc, sizeof(size_t)); int64_t *pm = calloc(nc, sizeof(int64_t)); int rc = -1;
    if (!pos || !pm) goto done;
    for (uint64_t r = 0; r < rows; r++) {
        for (uint64_t c = 0; c < nc; c++) {
            const uint8_t *D = st[1 + c].data.p; size_t dn = st[1 + c].data.n;
            if (c && kf_byte(out, d)) goto done;
            if (!type[c]) {
                const uint8_t *e = pos[c] < dn ? memchr(D + pos[c], '\n', dn - pos[c]) : NULL; if (!e) goto done;
                if (kf_put(out, D + pos[c], (size_t)(e - (D + pos[c])))) goto done;
                pos[c] = (size_t)(e - D) + 1; continue;
            }
            if (pos[c] >= dn) goto done;
            int h = D[pos[c]++]; if (!h) continue;
            if (h > 26 || (h > 10 && h < 17)) goto done;
            uint64_t z; if (kf_getvar(D, dn, &pos[c], &z)) goto done;
            int64_t m = h >= 17 ? pm[c] + kf_unzz(z) : kf_unzz(z); pm[c] = m;
            int fr = (h >= 17 ? h - 16 : h) - 1; uint64_t a = m < 0 ? (uint64_t)0 - (uint64_t)m : (uint64_t)m;
            char tmp[48]; int l;
            if (!fr) l = snprintf(tmp, sizeof tmp, "%s%llu", m < 0 ? "-" : "", (unsigned long long)a);
            else { uint64_t p10 = 1; for (int k = 0; k < fr; k++) p10 *= 10;
                   l = snprintf(tmp, sizeof tmp, "%s%llu.%0*llu", m < 0 ? "-" : "", (unsigned long long)(a / p10), fr, (unsigned long long)(a % p10)); }
            if (kf_put(out, tmp, (size_t)l)) goto done;
        }
        if (kf_byte(out, '\n')) goto done;
    }
    rc = 0;
done:
    free(pos); free(pm); return rc;
}

static void free_streams(kf_stream *st, int ns) { if (st) for (int i = 0; i < ns; i++) { free(st[i].data.p); free(st[i].ctx.p); free(st[i].par.p); } free(st); }

/* compress with the family asked for (fam < 0: FASTQ, else CSV, else one stream) */
static size_t bchunk = KF_BCHUNK;
static uint8_t *kfam_compress(const uint8_t *s, size_t n, int fam, int threads, size_t chunk, size_t *outn, int *famout) {
    pthread_once(&kf_tab_once, kf_tab_init);
    if (fam == KF_F_FASTQ || fam < 0) {
        uint8_t *o = kf_fastq_compress(s, n, threads, chunk, bchunk, outn);
        if (o || fam == KF_F_FASTQ) { *famout = KF_F_FASTQ; return o; }
    }
    kf_stream *st = NULL; int ns = 0; uint8_t *o = NULL;
    if (fam == KF_F_CSV || fam < 0) {
        if (!csv_split(s, n, &st, &ns)) { o = kf_frame_encode(KF_F_CSV, n, st, ns, NULL, NULL, NULL, threads, outn); free_streams(st, ns); *famout = KF_F_CSV; return o; }
        free_streams(st, ns); st = NULL;
        if (fam == KF_F_CSV) return NULL;
    }
    st = calloc(1, sizeof *st); if (!st) return NULL;
    st[0].data.p = (uint8_t *)(uintptr_t)s; st[0].data.n = n;       /* borrowed */
    kf_cuts cuts = {0};
    for (size_t off = 0; chunk && off < n; off += chunk) if (kf_cut(&cuts, n - off < chunk ? n - off : chunk)) { free(cuts.v); free(st); return NULL; }
    o = kf_frame_encode(KF_F_ONE, n, st, 1, &cuts, NULL, NULL, threads, outn);
    free(cuts.v); free(st); *famout = KF_F_ONE;
    return o;
}
static uint8_t *kfam_decompress(const uint8_t *c, size_t cn, int threads, size_t *outn) {
    pthread_once(&kf_tab_once, kf_tab_init);
    if (cn < 5 || memcmp(c, "KFM2", 4)) return NULL;
    if (c[4] == KF_F_FASTQ) {
        kf_frame f; if (kf_frame_parse(c, cn, &f, 16)) { kf_frame_free(&f); return NULL; }
        uint64_t n = f.orig; kf_frame_free(&f);
        uint8_t *o = malloc(n ? (size_t)n : 1);
        if (o && kf_fastq_decompress(c, cn, o, (size_t)n, threads)) { free(o); o = NULL; }
        if (o) *outn = (size_t)n;
        return o;
    }
    kf_frame f; uint8_t *res = NULL; kf_buf out = {0}; numctx *uc = NULL;
    if (kf_frame_parse(c, cn, &f, 16) || f.fam > KF_F_CSV) goto done;
    uint8_t *want = calloc((size_t)f.ns, 1); if (!want) goto done;
    if (f.fam == KF_F_ONE) {
        if (f.ns != 1) { free(want); goto done; }
        want[0] = 1; if (kf_frame_decode(&f, want, threads)) { free(want); goto done; }
        out.p = f.st[0].data.p; out.n = f.st[0].data.n; f.st[0].data.p = NULL;
    } else {
        want[0] = 1; if (kf_frame_decode(&f, want, threads)) { free(want); goto done; }
        const uint8_t *L = f.st[0].data.p; size_t li = 1, ln = f.st[0].data.n; uint64_t ncol, rows;
        if (ln < 1 || kf_getvar(L, ln, &li, &ncol) || kf_getvar(L, ln, &li, &rows) || ncol + 1 != (uint64_t)f.ns || li + ncol > ln) { free(want); goto done; }
        uc = calloc((size_t)f.nj + 1, sizeof *uc); if (!uc) { free(want); goto done; }
        for (int k = 0; k < f.nj; k++) { kf_job *j = &f.jb[k]; if (j->sid > 0 && L[li + j->sid - 1] != 0) { j->ctxfn = num_ctx; j->ctxarg = &uc[k]; } }
        want[0] = 0; for (int s = 1; s < f.ns; s++) want[s] = 1;
        if (kf_frame_decode(&f, want, threads) || csv_join(f.st, f.ns, &out)) { free(want); goto done; }
    }
    free(want);
    if (out.n != f.orig) goto done;
    res = out.p; out.p = NULL; *outn = (size_t)f.orig;
done:
    free(out.p); free(uc); kf_frame_free(&f);
    return res;
}

/* ------------------------------------------------------------------ command line */
static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); long l = ftell(f); rewind(f);
    uint8_t *b = malloc(l > 0 ? (size_t)l : 1);
    if (!b || fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)l; return b;
}
static int spit(const char *path, const uint8_t *b, size_t n) {
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return -1; }
    int bad = fwrite(b, 1, n, f) != n; if (fclose(f)) bad = 1; if (bad) perror(path); return bad ? -1 : 0;
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static double cpu_now(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec + t.tv_nsec * 1e-9; }   /* CPU time: steadier on a shared machine */
static const char *famname[] = {"one", "fastq", "csv"};

int main(int argc, char **argv) {
    int dec = 0, bench = 0, verbose = 0, only = -1, threads = 1, fam = -1, a = 1; size_t chunk = KF_CHUNK;
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        if (!strcmp(argv[a], "-d")) dec = 1; else if (!strcmp(argv[a], "-b")) bench = 1; else if (!strcmp(argv[a], "-v")) verbose = 1;
        else if (!strcmp(argv[a], "-S") && a + 1 < argc) only = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-T") && a + 1 < argc) threads = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-C") && a + 1 < argc) chunk = (size_t)strtoull(argv[++a], NULL, 0);
        else if (!strcmp(argv[a], "-B") && a + 1 < argc) bchunk = (size_t)strtoull(argv[++a], NULL, 0);
        else if (!strcmp(argv[a], "-f") && a + 1 < argc) { a++; fam = !strcmp(argv[a], "fastq") ? KF_F_FASTQ : !strcmp(argv[a], "csv") ? KF_F_CSV : !strcmp(argv[a], "one") ? KF_F_ONE : -1; }
        else { fprintf(stderr, "kfam: unknown option %s\n", argv[a]); return 2; }
    }
    if (argc - a != (bench || verbose || only >= 0 ? 1 : 2)) { fprintf(stderr, "usage: kfam [-T threads] [-C chunk] [-f auto|fastq|csv|one] in out | -d in out | -b in\n"); return 2; }
    size_t n; uint8_t *s = slurp(argv[a], &n); if (!s) return 1;
    if (only == 9) {                                       /* -S 9: every FASTQ job alone, timed; the schedule bound */
        pthread_once(&kf_tab_once, kf_tab_init);
        kf_fq q; memset(&q, 0, sizeof q); kf_cuts cuts[5]; memset(cuts, 0, sizeof cuts);
        if (kf_fastq_split(s, n, &q) || kf_fq_cuts(&q, chunk, bchunk, cuts)) { fprintf(stderr, "kfam: not FASTQ\n"); return 1; }
        double tj[4096], sum = 0, mx = 0; size_t zt = 0; int nt = 0; kf_fq_setup_arg sa = {&q};
        for (int sid = 0; sid < 5; sid++) {
            int nj = cuts[sid].n ? (int)cuts[sid].n : 1; size_t off = 0;
            for (int k = 0; k < nj && nt < 4096; k++) {
                kf_job j; memset(&j, 0, sizeof j); j.sid = sid; j.kind = q.st[sid].kind; j.n = cuts[sid].n ? cuts[sid].v[k] : q.st[sid].data.n;
                j.raw = q.st[sid].data.p + off; j.ctx = q.st[sid].ctx.n ? q.st[sid].ctx.p + off : NULL; kf_fq_setup(&sa, &j, off); off += j.n;
                double t = cpu_now(); kf_enc_job(&j); t = cpu_now() - t; zt += j.out.n; free(j.out.p);
                tj[nt++] = t; sum += t; if (t > mx) mx = t;
            }
        }
        printf("jobs %d, coded %zu, CPU time %.2fs, largest job %.2fs", nt, zt, sum, mx);
        for (int T = 4; T <= 10; T += 6) {                 /* largest first on T threads, as the pool runs them */
            double ld[64] = {0}; double v[4096]; memcpy(v, tj, sizeof(double) * (size_t)nt);
            for (int i = 1; i < nt; i++) { double x = v[i]; int k = i; while (k > 0 && v[k - 1] < x) { v[k] = v[k - 1]; k--; } v[k] = x; }
            for (int i = 0; i < nt; i++) { int b = 0; for (int t = 1; t < T; t++) if (ld[t] < ld[b]) b = t; ld[b] += v[i]; }
            double span = 0; for (int t = 0; t < T; t++) if (ld[t] > span) span = ld[t];
            printf(", T=%d bound %.2fx", T, sum / span);
        }
        printf("\n");
        return 0;
    }
    if (only >= 0) {                                       /* -S k: code only FASTQ stream k (model experiments) */
        pthread_once(&kf_tab_once, kf_tab_init);
        kf_fq q; memset(&q, 0, sizeof q); kf_cuts cuts[5]; memset(cuts, 0, sizeof cuts);
        if (only > 4 || kf_fastq_split(s, n, &q) || kf_fq_cuts(&q, chunk, bchunk, cuts)) { fprintf(stderr, "kfam: not FASTQ\n"); return 1; }
        int nj = cuts[only].n ? (int)cuts[only].n : 1; kf_job *jb = calloc((size_t)nj, sizeof *jb); kf_fq_setup_arg sa = {&q}; size_t off = 0;
        for (int k = 0; k < nj; k++) {
            kf_job *j = &jb[k]; j->sid = only; j->kind = q.st[only].kind; j->n = cuts[only].n ? cuts[only].v[k] : q.st[only].data.n;
            j->raw = q.st[only].data.p + off; j->ctx = q.st[only].ctx.n ? q.st[only].ctx.p + off : NULL; kf_fq_setup(&sa, j, off); off += j->n;
        }
        struct timespec c0, c1; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c0);
        double t = now(); kf_tp_run(kf_enc_job, jb, nj, threads); t = now() - t;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c1);
        size_t z = 0; for (int k = 0; k < nj; k++) { z += jb[k].out.n; free(jb[k].out.p); }
        printf("stream %d: %zu -> %zu  %.2fs (CPU %.2fs)  %.3f bits/symbol\n", only, q.st[only].data.n, z, t,
               (double)(c1.tv_sec - c0.tv_sec) + (c1.tv_nsec - c0.tv_nsec) * 1e-9, q.st[only].data.n ? 8.0 * z / q.st[only].data.n : 0.0);
        return 0;
    }
    if (verbose && !dec && !bench) {                       /* -v: compress once, print the coded size of each stream */
        size_t z = 0; int ff = 0; double t = now(); uint8_t *c = kfam_compress(s, n, fam, threads, chunk, &z, &ff); t = now() - t;
        kf_frame f; if (!c || kf_frame_parse(c, z, &f, 16)) { fprintf(stderr, "kfam: failed\n"); return 1; }
        printf("%s %zu %.2fs", famname[ff], z, t);
        for (int sidx = 0; sidx < f.ns && sidx < 12; sidx++) { size_t cz = 0; for (int q = 0; q < f.nj; q++) if (f.jb[q].sid == sidx) cz += f.jb[q].cn; printf(" %zu", cz); }
        printf("\n"); kf_frame_free(&f); free(c);
        return 0;
    }
    if (dec) {
        size_t m; uint8_t *o = kfam_decompress(s, n, threads, &m); free(s);
        if (!o) { fprintf(stderr, "kfam: corrupt input\n"); return 1; }
        int r = spit(argv[a + 1], o, m) ? 1 : 0; free(o); return r;
    }
    size_t z = 0; int ff = 0; uint8_t *c = NULL; double ce = 1e30, de = 1e30, t0 = now(); int k = 0;
    do { free(c); double t = now(); c = kfam_compress(s, n, fam, threads, chunk, &z, &ff); double e = now() - t; if (e < ce) ce = e; k++; }
    while (bench && c && (k < 3 || now() - t0 < 1.0));
    if (!c) { fprintf(stderr, "kfam: the input does not fit family %s\n", fam >= 0 ? famname[fam] : "auto"); free(s); return 1; }
    size_t m = 0; uint8_t *back = NULL; t0 = now(); k = 0;
    do { free(back); double t = now(); back = kfam_decompress(c, z, threads, &m); double e = now() - t; if (e < de) de = e; k++; }
    while (bench && back && (k < 3 || now() - t0 < 1.0));
    int same = back && m == n && !memcmp(back, s, n);
    free(back); free(s);
    if (!same) { fprintf(stderr, "kfam: round trip failed\n"); free(c); return 1; }
    if (bench) {
        kf_frame f; int ok = !kf_frame_parse(c, z, &f, 16);
        printf("{\"family\": \"%s\", \"threads\": %d, \"chunk\": %zu, \"bchunk\": %zu, \"input\": %zu, \"compressed\": %zu, \"compress_mbs\": %.3f, \"decompress_mbs\": %.3f, \"jobs\": %d, \"stream_sizes\": [",
               famname[ff], threads, chunk, bchunk, n, z, n / ce / 1e6, n / de / 1e6, ok ? f.nj : 0);
        for (int sidx = 0; ok && sidx < f.ns && sidx < 12; sidx++) {
            size_t cz = 0; for (int q = 0; q < f.nj; q++) if (f.jb[q].sid == sidx) cz += f.jb[q].cn;
            printf("%s[%zu, %zu]", sidx ? ", " : "", f.st[sidx].data.n, cz);
        }
        printf("%s]}\n", ok && f.ns > 12 ? ", ..." : "");
        kf_frame_free(&f); free(c);
        return 0;
    }
    int r = spit(argv[a + 1], c, z) ? 1 : 0; free(c); return r;
}
