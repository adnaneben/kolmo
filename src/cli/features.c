/* Command line for queryable and deduplicating archives (called from cli.c before its own option parsing):
 *
 *   kolmo --index [-m mode] [-l level] [-T n] [-B MiB] [--no-filter] [--token-bits K] [--delim C] [--kind lines|table|json] in out.kol
 *   kolmo cat  [--col NAME|#N] [--rows A-B] [-n] [-c] [--no-index] [--stats] [-T n] file.kol
 *   kolmo grep [-F] [-i] [-w] [-n] [-c] [--col NAME|#N] [--rows A-B] [--no-index] [--stats] [-T n] PATTERN file.kol
 *   kolmo a    [-m mode] [-l level] [-T n] [-B MiB] [--chunk KiB] [--no-line-cuts] [--stats] archive.kol files...
 *   kolmo x    [-T n] [-C dir] archive.kol [names...]
 *   kolmo l    archive.kol
 *
 * Rows are numbered from 1 here (line 1 is the first line of the file); A-B is inclusive, A- runs to the end.
 * Licence: Apache-2.0. */
#define _GNU_SOURCE 1
#include "kolmo.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static double fc_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

typedef struct { uint8_t *p; size_t n; int mapped; } fc_file;
static int fc_open(const char *path, fc_file *f) {
    memset(f, 0, sizeof *f);
    int fd = open(path, O_RDONLY); if (fd < 0) { perror(path); return -1; }
    struct stat st; if (fstat(fd, &st)) { perror(path); close(fd); return -1; }
    f->n = (size_t)st.st_size;
    if (f->n) {
        void *m = mmap(NULL, f->n, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) { f->p = m; f->mapped = 1; }
        else {
            f->p = malloc(f->n);
            size_t got = 0; while (f->p && got < f->n) { ssize_t r = read(fd, f->p + got, f->n - got); if (r <= 0) break; got += (size_t)r; }
            if (!f->p || got != f->n) { perror(path); free(f->p); close(fd); return -1; }
        }
    }
    close(fd); return 0;
}
static void fc_close(fc_file *f) { if (f->mapped) munmap(f->p, f->n); else free(f->p); }
static int fc_write(const char *path, const uint8_t *p, size_t n) {
    FILE *o = fopen(path, "wb"); if (!o) { perror(path); return -1; }
    int bad = n && fwrite(p, 1, n, o) != n; if (fclose(o)) bad = 1;
    if (bad) perror(path);
    return bad ? -1 : 0;
}
static int fc_sink(void *ctx, const uint8_t *p, size_t n) { return fwrite(p, 1, n, (FILE *)ctx) != n; }

static int fc_mode(const char *s, kolmo_params *P) {
    P->mode = !strcmp(s, "speed") || !strcmp(s, "rt") ? KOLMO_RT : !strcmp(s, "balanced") || !strcmp(s, "fast") ? KOLMO_FAST :
              !strcmp(s, "ratio") || !strcmp(s, "max") ? KOLMO_MAX : !strcmp(s, "eco") ? KOLMO_ECO : !strcmp(s, "hc") ? KOLMO_HC :
              !strcmp(s, "ultra") ? KOLMO_ULTRA : (kolmo_mode)-1;
    if ((int)P->mode < 0) { fprintf(stderr, "kolmo: unknown mode '%s'\n", s); return -1; }
    return 0;
}
static void fc_usage(void) {
    fprintf(stderr,
        "usage: kolmo --index [-m mode] [-B MiB] [--no-filter] [--token-bits K] [--delim C] [--kind lines|table|json] in out.kol\n"
        "       kolmo cat  [--col NAME|#N] [--rows A-B] [-n] [-c] [--no-index] [--stats] file.kol\n"
        "       kolmo grep [-F] [-i] [-w] [-n] [-c] [--col NAME|#N] [--rows A-B] [--no-index] [--stats] PATTERN file.kol\n"
        "       kolmo a [-m mode] [--chunk KiB] [--no-line-cuts] [--stats] archive.kol files...\n"
        "       kolmo x [-C dir] archive.kol [names...]      kolmo l archive.kol\n");
}
static void fc_stats(const kolmo_query_stats *s, double t) {
    fprintf(stderr, "kolmo: %u blocks, %u decoded whole, %u read by column, %u ruled out by the filter; "
                    "decoded %llu of %llu bytes (%.2f%%), read %llu stored bytes; index %llu bytes; %llu rows; %.3f s\n",
            s->blocks, s->blocks_decoded, s->blocks_columnar, s->blocks_skipped_filter,
            (unsigned long long)s->bytes_decoded, (unsigned long long)s->bytes_total,
            s->bytes_total ? 100.0 * (double)s->bytes_decoded / (double)s->bytes_total : 0.0,
            (unsigned long long)s->stored_read, (unsigned long long)s->index_bytes, (unsigned long long)s->rows_out, t);
}

static int fc_query(int argc, char **argv, int grep) {
    kolmo_query q = {0}; int stats = 0; const char *path = NULL; int a = 2;
    for (; a < argc; a++) {
        const char *s = argv[a];
        if (!strcmp(s, "--col") && a + 1 < argc) q.column = argv[++a];
        else if (!strcmp(s, "--rows") && a + 1 < argc) {
            char *e; unsigned long long lo = strtoull(argv[++a], &e, 10), hi = 0;
            if (*e != '-' || lo < 1) { fprintf(stderr, "kolmo: --rows A-B (from 1)\n"); return 2; }
            if (e[1]) { hi = strtoull(e + 1, &e, 10); if (*e || hi < lo) { fprintf(stderr, "kolmo: --rows A-B (from 1)\n"); return 2; } }
            q.row_first = lo - 1; q.row_count = hi ? hi - lo + 1 : 0;
        }
        else if (!strcmp(s, "-n")) q.flags |= KOLMO_Q_LINENO;
        else if (!strcmp(s, "-c")) q.flags |= KOLMO_Q_COUNT;
        else if (!strcmp(s, "-F") && grep) q.flags |= KOLMO_Q_FIXED;
        else if (!strcmp(s, "-i") && grep) q.flags |= KOLMO_Q_ICASE;
        else if (!strcmp(s, "-w") && grep) q.flags |= KOLMO_Q_WORD;
        else if (!strcmp(s, "--no-index")) q.flags |= KOLMO_Q_NOINDEX;
        else if (!strcmp(s, "--stats")) stats = 1;
        else if (!strcmp(s, "-T") && a + 1 < argc) q.threads = atoi(argv[++a]);
        else if (s[0] == '-' && s[1]) { fc_usage(); return 2; }
        else if (grep && !q.pattern) q.pattern = s;
        else if (!path) path = s;
        else { fc_usage(); return 2; }
    }
    if (!path || (grep && !q.pattern)) { fc_usage(); return 2; }
    fc_file f; if (fc_open(path, &f)) return 1;
    kolmo_query_stats st; double t0 = fc_now();
    int64_t r = kolmo_query_run(f.p, f.n, &q, fc_sink, stdout, &st);
    double t = fc_now() - t0;
    fc_close(&f);
    if (fflush(stdout)) r = r < 0 ? r : KOLMO_ERR_IO;
    if (r < 0) { fprintf(stderr, "kolmo: %s%s\n", kolmo_strerror(r),
                         r == KOLMO_ERR_CORRUPT || r == KOLMO_ERR_CHECKSUM ? " (try --no-index if only the index is damaged)" : ""); return 1; }
    if (q.flags & KOLMO_Q_COUNT) printf("%lld\n", (long long)r);
    if (stats) fc_stats(&st, t);
    return grep && r == 0 ? 1 : 0;
}

static int fc_index(int argc, char **argv) {
    kolmo_params P = {KOLMO_FAST, 0, 0, 0, 1}; kolmo_index_params I = {KOLMO_IDX_AUTO, 0, 1, 0};
    const char *in = NULL, *out = NULL;
    for (int a = 1; a < argc; a++) {
        const char *s = argv[a];
        if (!strcmp(s, "--index")) continue;
        if (!strcmp(s, "-m") && a + 1 < argc) { if (fc_mode(argv[++a], &P)) return 2; }
        else if (!strcmp(s, "-x")) P.mode = KOLMO_ULTRA;
        else if (!strcmp(s, "-l") && a + 1 < argc) P.level = atoi(argv[++a]);
        else if (!strcmp(s, "-T") && a + 1 < argc) P.threads = atoi(argv[++a]);
        else if (!strcmp(s, "-B") && a + 1 < argc) P.block = (uint32_t)(atof(argv[++a]) * 1048576);
        else if (!strcmp(s, "--no-filter")) I.filter = 0;
        else if (!strcmp(s, "--token-bits") && a + 1 < argc) I.token_bits = atoi(argv[++a]);
        else if (!strcmp(s, "--delim") && a + 1 < argc) { const char *d = argv[++a]; I.delim = !strcmp(d, "\\t") || !strcmp(d, "tab") ? '\t' : (uint8_t)d[0]; if (!I.kind) I.kind = KOLMO_IDX_TABLE; }
        else if (!strcmp(s, "--kind") && a + 1 < argc) {
            const char *k = argv[++a];
            I.kind = !strcmp(k, "lines") ? KOLMO_IDX_LINES : !strcmp(k, "table") ? KOLMO_IDX_TABLE : !strcmp(k, "json") ? KOLMO_IDX_JSON : -1;
            if (I.kind < 0) { fc_usage(); return 2; }
        }
        else if (s[0] == '-' && s[1]) { fc_usage(); return 2; }
        else if (!in) in = s; else if (!out) out = s; else { fc_usage(); return 2; }
    }
    if (!in || !out) { fc_usage(); return 2; }
    fc_file f; if (fc_open(in, &f)) return 1;
    size_t cap = kolmo_indexed_bound(f.n, &P); uint8_t *o = malloc(cap ? cap : 1);
    if (!o) { fc_close(&f); fprintf(stderr, "kolmo: out of memory\n"); return 1; }
    int64_t r = kolmo_compress_indexed(f.p, f.n, o, cap, &P, &I);
    fc_close(&f);
    if (r < 0) { free(o); fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r)); return 1; }
    int bad = fc_write(out, o, (size_t)r); free(o);
    return bad ? 1 : 0;
}

/* ---- archives ---- */
static int fc_mkdirs(char *path) {          /* the directories of path (not its last component) */
    for (char *p = path + 1; *p; p++) if (*p == '/') { *p = 0; if (mkdir(path, 0777) && errno != EEXIST) { *p = '/'; return -1; } *p = '/'; }
    return 0;
}
/* a stored name is extracted only if it is relative and has no ".." component */
static int fc_safe(const char *name) {
    if (!name[0] || name[0] == '/') return 0;
    for (const char *p = name; *p;) {
        const char *e = strchr(p, '/'); size_t l = e ? (size_t)(e - p) : strlen(p);
        if (l == 2 && p[0] == '.' && p[1] == '.') return 0;
        if (!e) break; p = e + 1;
    }
    return 1;
}
typedef struct { const char *dir; char **names; int nnames; int done, bad; } fc_x;
static int fc_put(void *ctx, const char *name, const uint8_t *data, uint64_t size, uint32_t mode, int64_t mtime) {
    fc_x *X = ctx; (void)mtime;
    if (X->nnames) { int hit = 0; for (int i = 0; i < X->nnames; i++) hit |= !strcmp(X->names[i], name); if (!hit) return 0; }
    if (!fc_safe(name)) { fprintf(stderr, "kolmo: skipping unsafe name '%s'\n", name); X->bad = 1; return 0; }
    size_t L = strlen(X->dir) + strlen(name) + 2; char *path = malloc(L); if (!path) return 1;
    snprintf(path, L, "%s/%s", X->dir, name);
    if (fc_mkdirs(path) || fc_write(path, data, (size_t)size)) { free(path); X->bad = 1; return 1; }
    if (mode & 0777) chmod(path, mode & 0777);
    free(path); X->done++; return 0;
}
static int fc_archive(int argc, char **argv) {
    kolmo_params P = {KOLMO_FAST, 0, 0, 0, 1}; kolmo_archive_params A = {0, 1}; int stats = 0, a = 2;
    for (; a < argc && argv[a][0] == '-'; a++) {
        const char *s = argv[a];
        if (!strcmp(s, "-m") && a + 1 < argc) { if (fc_mode(argv[++a], &P)) return 2; }
        else if (!strcmp(s, "-x")) P.mode = KOLMO_ULTRA;
        else if (!strcmp(s, "-l") && a + 1 < argc) P.level = atoi(argv[++a]);
        else if (!strcmp(s, "-T") && a + 1 < argc) P.threads = atoi(argv[++a]);
        else if (!strcmp(s, "-B") && a + 1 < argc) P.block = (uint32_t)(atof(argv[++a]) * 1048576);
        else if (!strcmp(s, "--chunk") && a + 1 < argc) A.avg_chunk = (uint32_t)atoi(argv[++a]) * 1024;
        else if (!strcmp(s, "--no-line-cuts")) A.line_cuts = 0;
        else if (!strcmp(s, "--stats")) stats = 1;
        else { fc_usage(); return 2; }
    }
    if (argc - a < 2) { fc_usage(); return 2; }
    const char *out = argv[a++]; int nf = argc - a;
    fc_file *F = calloc((size_t)nf, sizeof(fc_file)); kolmo_archive_file *K = calloc((size_t)nf, sizeof(kolmo_archive_file));
    if (!F || !K) return 1;
    int ret = 1;
    for (int i = 0; i < nf; i++) {
        if (fc_open(argv[a + i], &F[i])) { nf = i; goto out; }
        struct stat st; stat(argv[a + i], &st);
        const char *nm = argv[a + i]; while (nm[0] == '/') nm++;
        while (!strncmp(nm, "./", 2)) nm += 2;
        K[i] = (kolmo_archive_file){nm, F[i].p, F[i].n, (uint32_t)st.st_mode & 07777, (int64_t)st.st_mtime};
    }
    {   size_t cap = kolmo_archive_bound(K, (size_t)nf, &P); uint8_t *o = malloc(cap ? cap : 1);
        if (!o) { fprintf(stderr, "kolmo: out of memory\n"); goto out; }
        kolmo_archive_stats s; double t0 = fc_now();
        int64_t r = kolmo_archive_create(K, (size_t)nf, o, cap, &P, &A, &s);
        if (r < 0) { free(o); fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r)); goto out; }
        if (stats) fprintf(stderr, "kolmo: %llu files, %llu bytes, %llu chunks, %llu distinct (%llu bytes, %.2f%%), store frame %llu, table %llu, archive %lld; %.3f s\n",
                           (unsigned long long)s.files, (unsigned long long)s.input_bytes, (unsigned long long)s.chunks, (unsigned long long)s.unique_chunks,
                           (unsigned long long)s.unique_bytes, s.input_bytes ? 100.0 * s.unique_bytes / s.input_bytes : 0.0,
                           (unsigned long long)s.store_frame, (unsigned long long)s.table_bytes, (long long)r, fc_now() - t0);
        ret = fc_write(out, o, (size_t)r) ? 1 : 0; free(o); }
out:
    for (int i = 0; i < nf; i++) fc_close(&F[i]);
    free(F); free(K); return ret;
}
static int fc_extract(int argc, char **argv) {
    fc_x X = {".", NULL, 0, 0, 0}; int threads = 0, a = 2;
    for (; a < argc && argv[a][0] == '-'; a++) {
        if (!strcmp(argv[a], "-C") && a + 1 < argc) X.dir = argv[++a];
        else if (!strcmp(argv[a], "-T") && a + 1 < argc) threads = atoi(argv[++a]);
        else { fc_usage(); return 2; }
    }
    if (a >= argc) { fc_usage(); return 2; }
    fc_file f; if (fc_open(argv[a], &f)) return 1;
    X.names = argv + a + 1; X.nnames = argc - a - 1;
    int64_t r;
    if (X.nnames) {                          /* named files: each decodes only the store blocks it needs */
        r = kolmo_archive_count(f.p, f.n);
        for (int64_t i = 0; r >= 0 && i < r; i++) {
            char name[4097]; uint64_t size; uint32_t mode; int64_t mt;
            int e = kolmo_archive_stat(f.p, f.n, (uint64_t)i, name, sizeof name, &size, &mode, &mt); if (e) { r = e; break; }
            int hit = 0; for (int k = 0; k < X.nnames; k++) hit |= !strcmp(X.names[k], name);
            if (!hit) continue;
            uint8_t *d = malloc(size ? (size_t)size : 1); if (!d) { r = KOLMO_ERR_ALLOC; break; }
            int64_t g = kolmo_archive_extract(f.p, f.n, (uint64_t)i, d, (size_t)size, threads);
            if (g < 0) { free(d); r = g; break; }
            int stop = fc_put(&X, name, d, size, mode, mt); free(d);
            if (stop) { r = KOLMO_ERR_IO; break; }
        }
    } else r = kolmo_archive_extract_all(f.p, f.n, fc_put, &X, threads);
    fc_close(&f);
    if (r < 0) { fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r)); return 1; }
    if (X.nnames && X.done < X.nnames) { fprintf(stderr, "kolmo: %d of %d names not in the archive\n", X.nnames - X.done, X.nnames); return 1; }
    return X.bad;
}
static int fc_list(int argc, char **argv) {
    if (argc != 3) { fc_usage(); return 2; }
    fc_file f; if (fc_open(argv[2], &f)) return 1;
    int64_t r = kolmo_archive_count(f.p, f.n);
    for (int64_t i = 0; r >= 0 && i < r; i++) {
        char name[4097]; uint64_t size; uint32_t mode; int64_t mt;
        int e = kolmo_archive_stat(f.p, f.n, (uint64_t)i, name, sizeof name, &size, &mode, &mt);
        if (e) { r = e; break; }
        printf("%12llu  %04o  %s\n", (unsigned long long)size, mode, name);
    }
    fc_close(&f);
    if (r < 0) { fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r)); return 1; }
    return 0;
}

/* -1 when the arguments are not one of these commands */
int kolmo_cli_features(int argc, char **argv) {
    if (argc < 2) return -1;
    if (!strcmp(argv[1], "cat")) return fc_query(argc, argv, 0);
    if (!strcmp(argv[1], "grep")) return fc_query(argc, argv, 1);
    if (!strcmp(argv[1], "a")) return fc_archive(argc, argv);
    if (!strcmp(argv[1], "x")) return fc_extract(argc, argv);
    if (!strcmp(argv[1], "l")) return fc_list(argc, argv);
    for (int a = 1; a < argc; a++) if (!strcmp(argv[a], "--index")) return fc_index(argc, argv);
    return -1;
}
