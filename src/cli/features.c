/* Command line for queryable and deduplicating archives (called from cli.c before its own option parsing):
 *
 *   kolmo --index [LEVEL] [-T n] [--block-size MiB] [--no-filter] [--token-bits K] [--delim C] [--kind lines|table|json] in out.kol
 *   kolmo cat  [--col NAME|#N] [--rows A-B] [-n] [-c] [--no-index] [--stats] [-T n] file.kol
 *   kolmo grep [-F] [-i] [-w] [-n] [-c] [--col NAME|#N] [--rows A-B] [--no-index] [--stats] [-T n] PATTERN file.kol
 *   kolmo archive  [LEVEL] [-T n] [--block-size MiB] [--chunk KiB] [--no-line-cuts] [--stats] archive.kol files-or-dirs...
 *   kolmo extract  [-T n] [-C dir] archive.kol [names...]
 *   kolmo contents archive.kol
 *
 * LEVEL: -1 .. -9, -F -L -B -C -X -U, --level N|ultra (default 3, light), --codec-level N, as in the main command line.
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
#include <dirent.h>
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

/* Level options of --index and a, as in the main command line (docs/LEVELS.md): -1..-9, -F -L -B -C -X -U,
 * --level N|ultra, --fastest .. --ultra, --codec-level N, --block-size MiB. The 0.0.1 forms (-m, -x, -l N, -B MiB)
 * fail with the hint of the main command line. Returns 1 when s (with a value in argv[*a + 1]) was a level option,
 * 0 when it was not, or -2 after reporting a usage error (exit status 2). */
static int fc_level_opt(const char *s, int argc, char **argv, int *a, int *level, int *codec_level, uint32_t *block) {
    static const char *const names[] = {"fastest", "light", "balanced", "compact", "max", "ultra"};
    static const int nlv[] = {1, 3, 5, 7, 9, KOLMO_LEVEL_ULTRA};
    if (s[0] == '-' && s[1] >= '1' && s[1] <= '9' && !s[2]) { *level = s[1] - '0'; return 1; }
    if (s[0] == '-' && s[1] && !s[2] && strchr("FLBCXU", s[1])) {
        if (s[1] == 'B' && *a + 1 < argc && argv[*a + 1][0] >= '0' && argv[*a + 1][0] <= '9' && strspn(argv[*a + 1], "0123456789.") == strlen(argv[*a + 1])) {
            fprintf(stderr, "kolmo: -B MiB was removed in 0.0.2; -B is now --balanced (level 5); use --block-size MiB\n"); return -2;
        }
        static const int lv[] = {1, 3, 5, 7, 9, KOLMO_LEVEL_ULTRA};
        *level = lv[strchr("FLBCXU", s[1]) - "FLBCXU"]; return 1;
    }
    if (!strcmp(s, "-m")) { fprintf(stderr, "kolmo: -m was removed in 0.0.2; use a level: -1 (fastest) .. -9 (max), -3 light is the default, -U ultra\n"); return -2; }
    if (!strcmp(s, "-x")) { fprintf(stderr, "kolmo: -x was removed in 0.0.2; use -U (--ultra)\n"); return -2; }
    if (!strcmp(s, "-l")) { fprintf(stderr, "kolmo: -l LEVEL was removed in 0.0.2; use a level -1 .. -9 (expert: --codec-level N)\n"); return -2; }
    if (strncmp(s, "--", 2)) return 0;
    for (int i = 0; i < 6; i++) if (!strcmp(s + 2, names[i])) { *level = nlv[i]; return 1; }
    const char *eq = strchr(s, '='), *v = eq ? eq + 1 : NULL;
    size_t nl = eq ? (size_t)(eq - s) : strlen(s);
    int which = nl == 7 && !strncmp(s, "--level", 7) ? 0 : nl == 13 && !strncmp(s, "--codec-level", 13) ? 1 :
                nl == 12 && !strncmp(s, "--block-size", 12) ? 2 : -1;
    if (which < 0) return 0;
    if (!v) { if (*a + 1 >= argc) { fprintf(stderr, "kolmo: %.*s needs a value\n", (int)nl, s); return -2; } v = argv[++*a]; }
    char *e; double d = strtod(v, &e);
    if (which == 0 && !strcmp(v, "ultra")) { *level = KOLMO_LEVEL_ULTRA; return 1; }
    if (which == 0) for (int i = 0; i < 6; i++) if (!strcmp(v, names[i])) { *level = nlv[i]; return 1; }
    if (*e || !*v || (which == 0 && (d < 1 || d > 9 || d != (int)d)) || (which == 1 && (d < 1 || d > 1000 || d != (int)d)) ||
        (which == 2 && !(d > 0 && d <= 4095))) {
        fprintf(stderr, "kolmo: %.*s: bad value '%s'\n", (int)nl, s, v); return -2;
    }
    if (which == 0) *level = (int)d; else if (which == 1) *codec_level = (int)d; else *block = (uint32_t)(d * 1048576);
    return 1;
}
/* the parameters of a level and the expert overrides */
static void fc_params(kolmo_params *P, int level, int codec_level, uint32_t block) {
    kolmo_params_for_level(level, P);
    if (codec_level) P->level = codec_level;
    if (block) P->block = block;
}
static void fc_usage(void) {
    fprintf(stderr,
        "usage: kolmo --index [LEVEL] [--block-size MiB] [--no-filter] [--token-bits K] [--delim C] [--kind lines|table|json] in out.kol\n"
        "       kolmo cat  [--col NAME|#N] [--rows A-B] [-n] [-c] [--no-index] [--stats] file.kol\n"
        "       kolmo grep [-F] [-i] [-w] [-n] [-c] [--col NAME|#N] [--rows A-B] [--no-index] [--stats] PATTERN file.kol\n"
        "       kolmo archive [LEVEL] [--chunk KiB] [--no-line-cuts] [--stats] archive.kol files-or-directories...\n"
        "       kolmo extract [-C dir] archive.kol [names...]      kolmo contents archive.kol\n");
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
        else if (!strcmp(s, "-E") && grep) ;                    /* extended regex is the default */
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
    const char *in = NULL, *out = NULL; int level = KOLMO_LEVEL_DEFAULT, clevel = 0; uint32_t block = 0;
    for (int a = 1; a < argc; a++) {
        const char *s = argv[a]; int lo;
        if (!strcmp(s, "--index")) continue;
        if ((lo = fc_level_opt(s, argc, argv, &a, &level, &clevel, &block)) < 0) return 2;
        else if (lo) ;
        else if (!strcmp(s, "-T") && a + 1 < argc) P.threads = atoi(argv[++a]);
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
    fc_params(&P, level, clevel, block);
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
/* the regular files under path (path itself when it is one), in name order, appended to *list */
typedef struct { char **v; int n, cap; } fc_list_t;
static int fc_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int fc_walk(const char *path, fc_list_t *L, int depth) {
    struct stat st; if (lstat(path, &st)) { perror(path); return -1; }
    if (S_ISREG(st.st_mode)) {
        if (L->n == L->cap) { L->cap = L->cap ? L->cap * 2 : 64; char **g = realloc(L->v, sizeof(char *) * (size_t)L->cap); if (!g) return -1; L->v = g; }
        if (!(L->v[L->n] = strdup(path))) return -1;
        L->n++; return 0;
    }
    if (!S_ISDIR(st.st_mode) || depth > 64) return 0;              /* symlinks and special files are not archived */
    DIR *d = opendir(path); if (!d) { perror(path); return -1; }
    fc_list_t sub = {0}; struct dirent *e; int r = 0;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        size_t l = strlen(path) + strlen(e->d_name) + 2; char *p = malloc(l); if (!p) { r = -1; break; }
        snprintf(p, l, "%s%s%s", path, path[strlen(path) - 1] == '/' ? "" : "/", e->d_name);
        if (sub.n == sub.cap) { sub.cap = sub.cap ? sub.cap * 2 : 64; char **g = realloc(sub.v, sizeof(char *) * (size_t)sub.cap); if (!g) { free(p); r = -1; break; } sub.v = g; }
        sub.v[sub.n++] = p;
    }
    closedir(d);
    if (sub.n) qsort(sub.v, (size_t)sub.n, sizeof(char *), fc_cmp);
    for (int i = 0; i < sub.n; i++) { if (!r) r = fc_walk(sub.v[i], L, depth + 1); free(sub.v[i]); }
    free(sub.v); return r;
}
static int fc_archive(int argc, char **argv) {
    kolmo_params P = {KOLMO_FAST, 0, 0, 0, 1}; kolmo_archive_params A = {0, 1}; int stats = 0, a = 2;
    int level = KOLMO_LEVEL_DEFAULT, clevel = 0; uint32_t block = 0;
    for (; a < argc && argv[a][0] == '-'; a++) {
        const char *s = argv[a]; int lo;
        if ((lo = fc_level_opt(s, argc, argv, &a, &level, &clevel, &block)) < 0) return 2;
        else if (lo) ;
        else if (!strcmp(s, "-T") && a + 1 < argc) P.threads = atoi(argv[++a]);
        else if (!strcmp(s, "--chunk") && a + 1 < argc) A.avg_chunk = (uint32_t)atoi(argv[++a]) * 1024;
        else if (!strcmp(s, "--no-line-cuts")) A.line_cuts = 0;
        else if (!strcmp(s, "--stats")) stats = 1;
        else { fc_usage(); return 2; }
    }
    if (argc - a < 2) { fc_usage(); return 2; }
    fc_params(&P, level, clevel, block);
    const char *out = argv[a++];
    fc_list_t L = {0};                                       /* directories are archived with the files under them */
    for (int i = a; i < argc; i++) if (fc_walk(argv[i], &L, 0)) { for (int k = 0; k < L.n; k++) free(L.v[k]); free(L.v); return 1; }
    int nf = L.n; char **paths = L.v;
    fc_file *F = calloc((size_t)nf, sizeof(fc_file)); kolmo_archive_file *K = calloc((size_t)nf, sizeof(kolmo_archive_file));
    if (!F || !K) { free(F); free(K); for (int i = 0; i < L.n; i++) free(L.v[i]); free(L.v); return 1; }
    int ret = 1;
    for (int i = 0; i < nf; i++) {
        if (fc_open(paths[i], &F[i])) { nf = i; goto out; }
        struct stat st; stat(paths[i], &st);
        const char *nm = paths[i]; while (nm[0] == '/') nm++;
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
    for (int i = 0; i < L.n; i++) free(L.v[i]);
    free(L.v); free(F); free(K); return ret;
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

/* -T4 and -Cdir (kolmo x) as the separate arguments the parsers below read (the main command line accepts both) */
static char **fc_split(int *pargc, char **argv) {
    int argc = *pargc; char **v = malloc(sizeof(char *) * (size_t)(2 * argc + 1)); int n = 0;
    if (!v) return argv;
    for (int a = 0; a < argc; a++) {
        const char *s = argv[a];
        if (a && s[0] == '-' && (s[1] == 'T' || (s[1] == 'C' && !strcmp(argv[1], "extract"))) && s[2]) { static char opt[2][3] = {"-T", "-C"};
            v[n++] = opt[s[1] == 'C']; v[n++] = (char *)s + 2; }
        else if (a && s[0] == '-' && s[1] && s[1] != '-' && strchr("mlBx", s[1]) && s[2] && strcmp(argv[1], "cat") && strcmp(argv[1], "grep")) {
            static char opt[4][3] = {"-m", "-l", "-B", "-x"};   /* 0.0.1 clusters (-mhc, -l2, -B0.5): refused by fc_level_opt */
            v[n++] = opt[strchr("mlBx", s[1]) - "mlBx"]; v[n++] = (char *)s + 2; }
        else v[n++] = argv[a];
    }
    v[n] = NULL; *pargc = n; return v;
}

/* -1 when the arguments are not one of these commands */
int kolmo_cli_features(int argc, char **argv) {
    if (argc < 2) return -1;
    /* subcommands are recognised only as the first argument (a file of that name: kolmo -- NAME or kolmo ./NAME) */
    static const char *const old[3][2] = {{"a", "archive"}, {"x", "extract"}, {"l", "contents"}};
    for (int i = 0; i < 3; i++) if (!strcmp(argv[1], old[i][0])) {
        fprintf(stderr, "kolmo: 'kolmo %s' was removed in 0.0.2; use 'kolmo %s' (a file named %s: kolmo ./%s)\n", old[i][0], old[i][1], old[i][0], old[i][0]);
        return 2;
    }
    int known = !strcmp(argv[1], "cat") || !strcmp(argv[1], "grep") || !strcmp(argv[1], "archive") || !strcmp(argv[1], "extract") || !strcmp(argv[1], "contents");
    for (int a = 1; a < argc && !known && strcmp(argv[a], "--"); a++) known = !strcmp(argv[a], "--index");
    if (!known) return -1;
    argv = fc_split(&argc, argv);
    if (!strcmp(argv[1], "cat")) return fc_query(argc, argv, 0);
    if (!strcmp(argv[1], "grep")) return fc_query(argc, argv, 1);
    if (!strcmp(argv[1], "archive")) return fc_archive(argc, argv);
    if (!strcmp(argv[1], "extract")) return fc_extract(argc, argv);
    if (!strcmp(argv[1], "contents")) return fc_list(argc, argv);
    for (int a = 1; a < argc && strcmp(argv[a], "--"); a++) if (!strcmp(argv[a], "--index")) return fc_index(argc, argv);
    return -1;
}
