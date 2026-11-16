/* kpbench: trained profiles against untrained Kolmo and zstd dictionaries, in one process (no start-up time).
 *
 *   kpbench [-T threads] [-r rounds] [-z level] [-D dictsize] -m MODE TRAIN_DIR TEST_DIR [profile.kpf]
 *
 * Each test file is compressed and decompressed on its own (a message, a log rotation, a monthly export), every
 * round trip is checked, and the fastest of `rounds` passes over the whole set is kept. Rows:
 *   kolmo          kolmo_compress with -m MODE (no profile)
 *   kolmo-P        kolmo_compress_profile with the profile (trained here from TRAIN_DIR unless one is given)
 *   zstd-N         libzstd level N, no dictionary, frames as the zstd CLI writes them (checksum, content size)
 *   zstd-N-dict    the same with a dictionary from ZDICT_trainFromBuffer over the training files (what
 *                  `zstd --train -B16K` runs, default 112640 bytes), compressed and decompressed with prepared
 *                  dictionaries (ZSTD_CDict / ZSTD_DDict), as a server would keep them
 *   lz4, lz4-dict  (with -m speed) LZ4 acceleration 1 without and with the same dictionary file as LZ4 dictionary
 *                  (its last 64 KiB, as `lz4 -D`), frames reduced to the block (no frame header or checksum)
 * Output: one JSON object per row on stdout. Build: see Makefile target build/kpbench. */
#define _GNU_SOURCE
#include "kolmo.h"
#include "zstd.h"
#include "zdict.h"
#include "lz4.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct { uint8_t **b; size_t *n; size_t k; size_t total; } set_t;
static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void load(const char *dir, set_t *S) {
    DIR *d = opendir(dir); if (!d) { perror(dir); exit(1); }
    char **names = NULL; size_t nn = 0; struct dirent *e;
    while ((e = readdir(d))) if (e->d_name[0] != '.') { names = realloc(names, (nn + 1) * sizeof *names); names[nn++] = strdup(e->d_name); }
    closedir(d);
    qsort(names, nn, sizeof *names, cmp);
    S->b = calloc(nn, sizeof *S->b); S->n = calloc(nn, sizeof *S->n); S->k = 0; S->total = 0;
    for (size_t i = 0; i < nn; i++) {
        char p[4096]; snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
        fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
        S->b[S->k] = malloc(s ? s : 1); if (fread(S->b[S->k], 1, s, f) != (size_t)s) exit(1); fclose(f);
        S->n[S->k] = s; S->total += s; S->k++; free(names[i]);
    }
    free(names);
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

typedef struct { const char *name; double size, ct, dt; } row_t;
static void report(const char *mode, const set_t *T, row_t r) {
    printf("{\"row\":\"%s\",\"mode\":\"%s\",\"files\":%zu,\"bytes\":%zu,\"size\":%.0f,\"ratio\":%.4f,\"c_MBps\":%.2f,\"d_MBps\":%.2f}\n",
           r.name, mode, T->k, T->total, r.size, T->total / r.size, T->total / r.ct / 1e6, T->total / r.dt / 1e6);
    fflush(stdout);
}

int main(int argc, char **argv) {
    int threads = 1, rounds = 3, zl = 3, opt; size_t dsz = 112640; const char *mode = "balanced";
    while ((opt = getopt(argc, argv, "T:r:z:D:m:")) != -1) switch (opt) {
        case 'T': threads = atoi(optarg); break; case 'r': rounds = atoi(optarg); break; case 'z': zl = atoi(optarg); break;
        case 'D': dsz = (size_t)atol(optarg); break; case 'm': mode = optarg; break;
        default: return 2;
    }
    if (argc - optind < 2) { fprintf(stderr, "usage: kpbench [-T n] [-r n] [-z level] [-D dictsize] -m MODE TRAIN TEST [p.kpf]\n"); return 2; }
    set_t Tr, Te; load(argv[optind], &Tr); load(argv[optind + 1], &Te);
    kolmo_params P = {KOLMO_FAST, 0, threads, 0, 1};
    if (!strcmp(mode, "speed")) { P.mode = KOLMO_RT; P.transforms = 3; }
    else if (!strcmp(mode, "eco")) P.mode = KOLMO_ECO;
    else if (!strcmp(mode, "hc")) P.mode = KOLMO_HC;
    else if (!strcmp(mode, "ratio")) P.mode = KOLMO_MAX;
    else if (!strcmp(mode, "x")) P.mode = KOLMO_ULTRA;
    size_t maxn = 0; for (size_t i = 0; i < Te.k; i++) if (Te.n[i] > maxn) maxn = Te.n[i];
    size_t cap = kolmo_bound(maxn, &P) + ZSTD_compressBound(maxn) + 1024;
    uint8_t *o = malloc(cap), *d = malloc(maxn + 1);
    uint8_t **comp = calloc(Te.k, sizeof *comp); size_t *cl = calloc(Te.k, sizeof *cl);

    /* profile */
    kolmo_profile *prof = NULL;
    uint8_t *cat = malloc(Tr.total ? Tr.total : 1); size_t off = 0;
    for (size_t i = 0; i < Tr.k; i++) { memcpy(cat + off, Tr.b[i], Tr.n[i]); off += Tr.n[i]; }
    if (argc - optind >= 3) {
        FILE *f = fopen(argv[optind + 2], "rb"); fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *pb = malloc(s); if (fread(pb, 1, s, f) != (size_t)s) return 1; fclose(f);
        if (kolmo_profile_load(pb, s, &prof)) { fprintf(stderr, "bad profile\n"); return 1; }
        free(pb);
    } else {
        kolmo_train_params tp = {P, 0, 0, NULL, getenv("KP_QUICK") != NULL}; tp.params.threads = 0;
        size_t pc = kolmo_profile_bound(&tp); uint8_t *pb = malloc(pc);
        double t0 = now(); int64_t pl = kolmo_profile_train(cat, Tr.n, Tr.k, &tp, pb, pc); double t1 = now();
        if (pl < 0 || kolmo_profile_load(pb, pl, &prof)) { fprintf(stderr, "train failed %lld\n", (long long)pl); return 1; }
        fprintf(stderr, "trained %lld-byte profile in %.2f s\n", (long long)pl, t1 - t0);
        free(pb);
    }
    /* zstd dictionary, as zstd --train */
    /* samples cut into 16 KiB pieces, as `zstd --train -B16K` (without -B, zstd --train rejects fewer than 5 samples) */
    size_t np = 0; for (size_t i = 0; i < Tr.k; i++) np += (Tr.n[i] + 16383) / 16384;
    size_t *psz = malloc((np ? np : 1) * sizeof *psz); np = 0;
    for (size_t i = 0; i < Tr.k; i++) for (size_t o2 = 0; o2 < Tr.n[i]; o2 += 16384) psz[np++] = Tr.n[i] - o2 < 16384 ? Tr.n[i] - o2 : 16384;
    double t0 = now(); uint8_t *zd = malloc(dsz); size_t zdl = ZDICT_trainFromBuffer(zd, dsz, cat, psz, (unsigned)np); double t1 = now();
    if (ZDICT_isError(zdl)) { fprintf(stderr, "zdict: %s\n", ZDICT_getErrorName(zdl)); zdl = 0; }
    else fprintf(stderr, "zstd dictionary %zu bytes in %.2f s\n", zdl, t1 - t0);

    int nvar = !strcmp(mode, "speed") ? 6 : 4;
    for (int variant = 0; variant < nvar; variant++) {
        if ((variant == 3 || variant == 5) && !zdl) continue;
        row_t r = {variant == 0 ? "kolmo" : variant == 1 ? "kolmo-P" : variant == 2 ? "zstd" : variant == 3 ? "zstd-dict" : variant == 4 ? "lz4" : "lz4-dict", 0, 1e30, 1e30};
        size_t l4n = zdl < 65536 ? zdl : 65536; const char *l4d = (const char *)zd + zdl - l4n;
        ZSTD_CCtx *zc = ZSTD_createCCtx(); ZSTD_DCtx *zdx = ZSTD_createDCtx();
        ZSTD_CDict *cd = variant == 3 ? ZSTD_createCDict(zd, zdl, zl) : NULL; ZSTD_DDict *dd = variant == 3 ? ZSTD_createDDict(zd, zdl) : NULL;
        for (int rr = 0; rr < rounds; rr++) {
            double size = 0, a = now();
            for (size_t i = 0; i < Te.k; i++) {
                int64_t z;
                if (variant == 0) z = kolmo_compress(Te.b[i], Te.n[i], o, cap, &P);
                else if (variant == 1) z = kolmo_compress_profile(Te.b[i], Te.n[i], o, cap, &P, prof);
                else if (variant >= 4) {
                    LZ4_stream_t st; LZ4_initStream(&st, sizeof st);
                    if (variant == 5) LZ4_loadDict(&st, l4d, (int)l4n);
                    int q = LZ4_compress_fast_continue(&st, (const char *)Te.b[i], (char *)o, (int)Te.n[i], (int)cap, 1); z = q > 0 ? q : -1;
                } else {
                    ZSTD_CCtx_reset(zc, ZSTD_reset_session_and_parameters);
                    ZSTD_CCtx_setParameter(zc, ZSTD_c_checksumFlag, 1);
                    if (cd) ZSTD_CCtx_refCDict(zc, cd); else ZSTD_CCtx_setParameter(zc, ZSTD_c_compressionLevel, zl);
                    size_t q = ZSTD_compress2(zc, o, cap, Te.b[i], Te.n[i]); z = ZSTD_isError(q) ? -1 : (int64_t)q;
                }
                if (z < 0) { fprintf(stderr, "%s: compress error %lld on file %zu\n", r.name, (long long)z, i); return 1; }
                if (!rr) { comp[i] = realloc(comp[i], z); cl[i] = z; }
                memcpy(comp[i], o, z); size += z;
            }
            double b = now();
            for (size_t i = 0; i < Te.k; i++) {
                int64_t u;
                if (variant == 0) u = kolmo_decompress(comp[i], cl[i], d, maxn + 1, threads);
                else if (variant == 1) u = kolmo_decompress_profile(comp[i], cl[i], d, maxn + 1, threads, prof);
                else if (variant >= 4) {
                    int q = variant == 5 ? LZ4_decompress_safe_usingDict((const char *)comp[i], (char *)d, (int)cl[i], (int)maxn + 1, l4d, (int)l4n)
                                         : LZ4_decompress_safe((const char *)comp[i], (char *)d, (int)cl[i], (int)maxn + 1);
                    u = q;
                } else { size_t q = dd ? ZSTD_decompress_usingDDict(zdx, d, maxn + 1, comp[i], cl[i], dd) : ZSTD_decompressDCtx(zdx, d, maxn + 1, comp[i], cl[i]); u = ZSTD_isError(q) ? -1 : (int64_t)q; }
                if (u != (int64_t)Te.n[i] || memcmp(d, Te.b[i], Te.n[i])) { fprintf(stderr, "%s: round trip failed on file %zu\n", r.name, i); return 1; }
            }
            double c = now();
            r.size = size; if (b - a < r.ct) r.ct = b - a; if (c - b < r.dt) r.dt = c - b;
        }
        char nm[32]; if (variant == 2 || variant == 3) { snprintf(nm, sizeof nm, "%s-%d%s", "zstd", zl, variant == 3 ? "-dict" : ""); r.name = nm; }
        report(mode, &Te, r);
        ZSTD_freeCCtx(zc); ZSTD_freeDCtx(zdx); ZSTD_freeCDict(cd); ZSTD_freeDDict(dd);
    }
    return 0;
}
