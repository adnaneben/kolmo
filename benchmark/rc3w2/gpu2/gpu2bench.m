/* gpu2bench: GPU (Metal) against CPU threads on the candidate stages of rc3 wave 2, with byte-identical results.
 * Apache-2.0, Kolmo contributors. Build: benchmark/rc3w2/gpu2/build.sh. Results: benchmark/rc3w2/gpu2/RESULTS.md.
 *
 *   gpu2bench check [files...]                 exactness of every kernel against its CPU reference
 *   gpu2bench table FILE csv|jsonl [chunkMB]   (a) fused table front end; and pipelined with a zstd coder stage
 *   gpu2bench formula FILE                     (b) GR6+ formula search (map on CPU threads or the GPU)
 *   gpu2bench krw FILE [segKB] [lanes]         (c) wide interleaved rANS encode and decode
 *   gpu2bench hist FILE [blockKB]              (d) per-block histograms + entropy, order-1 histogram
 *   gpu2bench video FILE.y4m [gop]             (e) kvid strip prediction; and pipelined with a zstd coder stage
 *   gpu2bench batch FILE                       (f) many small files (FILE lists paths): histograms and krw per file
 *   gpu2bench sa FILE MB                       (g) BWT of one block of MB megabytes: libsais 1/10 threads vs the GPU
 * Every timing prints one JSON line per variant. GPU2_LOCK=path: stop (exit 3) while that file exists.
 * GPU2_ROUNDS (default 5), GPU2_T (default 10). GPU2_CHECK_ONLY=1: the exactness checks of a command, no timing. */
#import <Foundation/Foundation.h>
#include "common.h"
#include "g6wrap.h"
#include "vidref.h"
#include "../../../src/arch/metal/gpu2.h"
#include "../../../src/arch/metal/metal.h"
#include "../../../src/algo/sais/sais.h"
#include "zstd.h"

static int ROUNDS = 5, T = 10;
static const char *g_file = "";
static double g_gpu;                               /* GPU kernel seconds of the current measured call */

/* ======================= interleaved A/B/C... measurement ======================= */
typedef struct { const char *name; void (*fn)(void *ctx); } variant;
static void bench(const char *test, size_t n, variant *v, int nv, void *ctx, const char *extra) {
    enum { MAXV = 12, MAXR = 64 };
    double wall[MAXV][MAXR], cpu[MAXV][MAXR], nj[MAXV][MAXR], util[MAXV][MAXR], gk[MAXV][MAXR];
    int R = ROUNDS > MAXR ? MAXR : ROUNDS;
    if (getenv("GPU2_CHECK_ONLY")) { fprintf(stderr, "%s: exact (no timing)\n", test); return; }
    for (int k = 0; k < nv; k++) v[k].fn(ctx);    /* warm-up (also pages, pipelines, pools) */
    for (int r = 0; r < R; r++) {
        if (lock_present()) { fprintf(stderr, "gpu2bench: benchmark lock present, stopping\n"); exit(3); }
        for (int k = 0; k < nv; k++) {
            int i = (r & 1) ? (nv - 1 - k + r) % nv : (k + r) % nv;
            meas_t m; g_gpu = 0;
            meas_begin(&m); v[i].fn(ctx); meas_end(&m);
            wall[i][r] = m.wall; cpu[i][r] = m.cpu; nj[i][r] = (double)m.nj; util[i][r] = m.gpu_util; gk[i][r] = g_gpu;
        }
    }
    for (int i = 0; i < nv; i++) {
        double mn = 1e30; for (int r = 0; r < R; r++) if (wall[i][r] < mn) mn = wall[i][r];
        double w = median(wall[i], R), c = median(cpu[i], R), e = median(nj[i], R), u = median(util[i], R), g = median(gk[i], R);
        printf("{\"test\": \"%s\", \"file\": \"%s\", \"n\": %zu, \"variant\": \"%s\", \"rounds\": %d, \"wall_ms\": %.3f, \"wall_min_ms\": %.3f, "
               "\"mbs\": %.1f, \"cpu_s\": %.4f, \"cores\": %.2f, \"cpu_mj\": %.2f, \"gpu_util\": %.1f, \"gpu_kernel_ms\": %.3f%s%s}\n",
               test, g_file, n, v[i].name, R, w * 1e3, mn * 1e3, n / w / 1e6, c, c / w, e / 1e6, u, g * 1e3, extra ? ", " : "", extra ? extra : "");
        fflush(stdout);
    }
}

static void *xalloc(size_t n) { return kg2_metal_alloc(n); }

/* ======================= (a) table front end ======================= */
typedef struct { const uint8_t *d; size_t n; int mode; uint8_t sep; kg2_table *t; int T; } fe_job;
static void fe_lines(void *c, uint32_t i);
typedef struct { fe_job *J; uint32_t parts; size_t *cuts; uint32_t *cnt, *at; } fe_lines_ctx;
static void fe_count(void *c, uint32_t i) {
    fe_lines_ctx *L = c; const uint8_t *d = L->J->d; size_t n = L->J->n, b = L->cuts[i], e = L->cuts[i + 1];
    if (e > n) e = n;
    uint32_t k = (b == 0 && e > 0) ? 1 : 0; size_t q = b ? b - 1 : 0;
    while (q + 1 < e) { const uint8_t *x = memchr(d + q, '\n', e - 1 - q); if (!x) break; q = (size_t)(x - d) + 1; k++; }
    L->cnt[i] = k;
}
static void fe_lines(void *c, uint32_t i) {
    fe_lines_ctx *L = c; kg2_ref_line_starts(L->J->d, L->J->n, L->cuts[i], L->cuts[i + 1], L->J->t->line_start + L->at[i]);
}
static void fe_fields(void *c, uint32_t i) {
    fe_job *J = c; uint32_t R = J->t->rows, per = (R + 255) / 256, r0 = i * per, r1 = r0 + per < R ? r0 + per : R;
    if (r0 < r1) kg2_ref_fields(J->d, J->n, J->mode, J->sep, J->t, r0, r1);
}
static void fe_resid(void *c, uint32_t i) {
    fe_job *J = c; uint32_t R = J->t->rows, per = (R + 255) / 256, r0 = i * per, r1 = r0 + per < R ? r0 + per : R;
    if (r0 < r1) kg2_ref_resid(J->d, J->t, r0, r1);
}
/* CPU front end on threads: line index (count, prefix, write), fields, residuals */
static int fe_cpu(int threads, const uint8_t *d, size_t n, int mode, uint8_t sep, kg2_table *t) {
    fe_job J = {d, n, mode, sep, t, threads};
    uint32_t P = threads <= 1 ? 1 : (uint32_t)threads * 4;
    size_t cuts[257]; uint32_t cnt[256], at[257];
    for (uint32_t i = 0; i <= P; i++) cuts[i] = n * i / P;
    fe_lines_ctx L = {&J, P, cuts, cnt, at};
    if (P == 1) { cnt[0] = 0; at[0] = 0; }
    else par_run(threads, P, fe_count, &L);
    at[0] = 0;
    if (P == 1) at[1] = kg2_ref_line_starts(d, n, 0, n, t->line_start);
    else { for (uint32_t i = 0; i < P; i++) at[i + 1] = at[i] + cnt[i]; if (at[P] > t->cap_rows) return -1; par_run(threads, P, fe_lines, &L); }
    t->rows = at[P];
    if (t->rows > t->cap_rows) return -1;
    t->line_start[t->rows] = (uint32_t)(n && d[n - 1] == '\n' ? n : n + 1);
    par_run(threads, 256, fe_fields, &J);
    par_run(threads, 256, fe_resid, &J);
    return 0;
}
static void tb_alloc(kg2_table *t, uint32_t cap_rows, uint32_t cols) {
    memset(t, 0, sizeof *t);
    size_t F = (size_t)cap_rows * cols;
    t->cap_rows = cap_rows; t->cols = cols;
    t->line_start = xalloc(((size_t)cap_rows + 1) * 4); t->off = xalloc(F * 4); t->len = xalloc(F * 2); t->hash = xalloc(F * 4);
    t->val = xalloc(F * 8); t->fmt = xalloc(F); t->rowflag = xalloc(cap_rows); t->fl = xalloc(F); t->res = xalloc(F * 8);
}
static int tb_same(const kg2_table *a, const kg2_table *b) {
    if (a->rows != b->rows || a->cols != b->cols) { fprintf(stderr, "rows %u/%u cols %u/%u\n", a->rows, b->rows, a->cols, b->cols); return 0; }
    size_t F = (size_t)a->rows * a->cols;
#define CMPF(f, sz) if (memcmp(a->f, b->f, sz)) { fprintf(stderr, "table: %s differs\n", #f); return 0; }
    CMPF(line_start, ((size_t)a->rows + 1) * 4) CMPF(off, F * 4) CMPF(len, F * 2) CMPF(hash, F * 4) CMPF(val, F * 8)
    CMPF(fmt, F) CMPF(rowflag, a->rows) CMPF(fl, F) CMPF(res, F * 8)
#undef CMPF
    return 1;
}

typedef struct {
    const uint8_t *d; size_t n; int mode; uint8_t sep; uint32_t K;
    size_t nchunk, *cb; uint32_t maxrows;
    kg2_table slot[2];
    /* coder stage */
    uint8_t *zout[2]; size_t zcap, zsize[2], ztotal;
    int mode_coder;
} tbx;
static void tbx_cpu1(void *c) { tbx *X = c; for (size_t i = 0; i < X->nchunk; i++) fe_cpu(1, X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[0]); }
static void tbx_cpuT(void *c) { tbx *X = c; for (size_t i = 0; i < X->nchunk; i++) fe_cpu(T, X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[0]); }
static void tbx_gpu(void *c) {
    tbx *X = c;
    for (size_t i = 0; i < X->nchunk; i++) {
        if (kg2_metal_table(X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[0])) { fprintf(stderr, "gpu table failed\n"); exit(1); }
        g_gpu += kg2_metal_last_gpu_seconds();
    }
}
/* GPU and CPU threads on the same stage: both take chunks from one counter (GPU into slot 1, CPU into slot 0) */
typedef struct { tbx *X; atomic_size_t next; double gs; } thyb;
static void *thyb_gpu(void *p) {
    thyb *H = p; tbx *X = H->X; size_t i;
    while ((i = atomic_fetch_add(&H->next, 1)) < X->nchunk) {
        kg2_metal_table(X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[1]); H->gs += kg2_metal_last_gpu_seconds();
    }
    return NULL;
}
static void tbx_hyb(void *c) {
    tbx *X = c; thyb H; H.X = X; atomic_store(&H.next, 0); H.gs = 0;
    pthread_t th; pthread_create(&th, NULL, thyb_gpu, &H);
    size_t i;
    while ((i = atomic_fetch_add(&H.next, 1)) < X->nchunk) fe_cpu(T, X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[0]);
    pthread_join(th, NULL); g_gpu += H.gs;
}
/* the coder stage: zstd level 1 of every column's flags and residuals (2 K jobs) */
typedef struct { tbx *X; int s; size_t *sz; } zjob;
static void z_one(void *c, uint32_t i) {
    zjob *Z = c; kg2_table *t = &Z->X->slot[Z->s]; uint32_t col = i / 2; size_t R = t->rows;
    size_t cap = Z->X->zcap / (2 * Z->X->K); uint8_t *o = Z->X->zout[Z->s] + (size_t)i * cap;
    const void *src = i & 1 ? (const void *)(t->res + col * R) : (const void *)(t->fl + col * R);
    size_t m = i & 1 ? R * 8 : R;
    size_t r = ZSTD_compress(o, cap, src, m, 1);
    Z->sz[i] = ZSTD_isError(r) ? 0 : r;
}
static size_t z_stage(tbx *X, int s) {
    size_t sz[2 * 4096]; zjob Z = {X, s, sz};
    par_run(T, 2 * X->K, z_one, &Z);
    size_t tot = 0; for (uint32_t i = 0; i < 2 * X->K; i++) tot += sz[i];
    return tot;
}
static void tbx_cpu_serial(void *c) {
    tbx *X = c; X->ztotal = 0;
    for (size_t i = 0; i < X->nchunk; i++) { fe_cpu(T, X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[0]); X->ztotal += z_stage(X, 0); }
}
static void tbx_gpu_serial(void *c) {
    tbx *X = c; X->ztotal = 0;
    for (size_t i = 0; i < X->nchunk; i++) {
        kg2_metal_table(X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[0]); g_gpu += kg2_metal_last_gpu_seconds();
        X->ztotal += z_stage(X, 0);
    }
}
/* GPU front end of chunk i + 1 while the CPU threads code chunk i (two table slots) */
typedef struct { tbx *X; dispatch_semaphore_t ready[2], free_[2]; double gs; } pipe_ctx;
static void *tbx_feeder(void *c) {
    pipe_ctx *P = c; tbx *X = P->X;
    for (size_t i = 0; i < X->nchunk; i++) {
        int s = (int)(i & 1);
        dispatch_semaphore_wait(P->free_[s], DISPATCH_TIME_FOREVER);
        kg2_metal_table(X->d + X->cb[i], X->cb[i + 1] - X->cb[i], X->mode, X->sep, &X->slot[s]); P->gs += kg2_metal_last_gpu_seconds();
        dispatch_semaphore_signal(P->ready[s]);
    }
    return NULL;
}
static void tbx_pipe(void *c) {
    tbx *X = c; X->ztotal = 0;
    pipe_ctx P = {X, {dispatch_semaphore_create(0), dispatch_semaphore_create(0)}, {dispatch_semaphore_create(1), dispatch_semaphore_create(1)}, 0};
    pthread_t th; pthread_create(&th, NULL, tbx_feeder, &P);
    for (size_t i = 0; i < X->nchunk; i++) {
        int s = (int)(i & 1);
        dispatch_semaphore_wait(P.ready[s], DISPATCH_TIME_FOREVER);
        X->ztotal += z_stage(X, s);
        dispatch_semaphore_signal(P.free_[s]);
    }
    pthread_join(th, NULL); g_gpu += P.gs;
}
static size_t *line_chunks(const uint8_t *d, size_t n, size_t chunk, size_t *nchunk) {
    size_t cap = n / chunk + 2, k = 0, *cb = malloc((cap + 1) * sizeof *cb);
    cb[k++] = 0;
    size_t b = 0;
    while (b < n) {
        size_t e = b + chunk;
        if (e >= n) e = n;
        else { const uint8_t *x = memchr(d + e, '\n', n - e); e = x ? (size_t)(x - d) + 1 : n; }
        cb[k++] = e; b = e;
    }
    *nchunk = k - 1; return cb;
}
static int cmd_table(int argc, char **argv) {
    if (argc < 4) return 2;
    size_t n; uint8_t *d = slurp_aligned(argv[2], &n, xalloc); if (!d) return 1;
    int mode = !strcmp(argv[3], "jsonl") ? KG2_JSONL : KG2_CSV; uint8_t sep = ',';
    size_t chunk = (argc > 4 ? (size_t)atoi(argv[4]) : 16) << 20;
    tbx X = {0}; X.d = d; X.n = n; X.mode = mode; X.sep = sep; X.K = kg2_ref_first_nf(d, n, mode, sep);
    X.cb = line_chunks(d, n, chunk, &X.nchunk);
    for (size_t i = 0; i < X.nchunk; i++) { uint32_t r = kg2_ref_count_lines(d + X.cb[i], X.cb[i + 1] - X.cb[i]); if (r > X.maxrows) X.maxrows = r; }
    if (!kg2_metal_available()) { fprintf(stderr, "no Metal\n"); return 1; }
    tb_alloc(&X.slot[0], X.maxrows, X.K); tb_alloc(&X.slot[1], X.maxrows, X.K);
    /* exactness on every chunk: CPU 1 thread, CPU T threads, GPU */
    kg2_table c1; tb_alloc(&c1, X.maxrows, X.K);
    for (size_t i = 0; i < X.nchunk; i++) {
        const uint8_t *p = d + X.cb[i]; size_t m = X.cb[i + 1] - X.cb[i];
        fe_cpu(1, p, m, mode, sep, &c1); fe_cpu(T, p, m, mode, sep, &X.slot[0]);
        if (kg2_metal_table(p, m, mode, sep, &X.slot[1])) { fprintf(stderr, "gpu table failed\n"); return 1; }
        if (!tb_same(&c1, &X.slot[0]) || !tb_same(&c1, &X.slot[1])) { fprintf(stderr, "table: chunk %zu differs\n", i); return 1; }
    }
    uint64_t fields = 0;
    for (size_t i = 0; i < X.nchunk; i++) fields += (uint64_t)kg2_ref_count_lines(d + X.cb[i], X.cb[i + 1] - X.cb[i]) * X.K;
    char extra[256]; snprintf(extra, sizeof extra, "\"cols\": %u, \"fields\": %llu, \"chunk_mb\": %zu, \"chunks\": %zu, \"identical\": true",
                              X.K, (unsigned long long)fields, chunk >> 20, X.nchunk);
    variant v[] = {{"cpu1", tbx_cpu1}, {"cpu10", tbx_cpuT}, {"gpu", tbx_gpu}, {"gpu+cpu10_chunks", tbx_hyb}};
    bench("table_frontend", n, v, 4, &X, extra);
    /* with the coder stage */
    X.zcap = 2 * X.K * (ZSTD_compressBound((size_t)X.maxrows * 8) + 64);
    X.zout[0] = malloc(X.zcap); X.zout[1] = malloc(X.zcap);
    tbx_cpu_serial(&X); size_t z1 = X.ztotal; tbx_pipe(&X); size_t z2 = X.ztotal; tbx_gpu_serial(&X); size_t z3 = X.ztotal;
    if (z1 != z2 || z1 != z3) { fprintf(stderr, "coder sizes differ %zu %zu %zu\n", z1, z2, z3); return 1; }
    snprintf(extra, sizeof extra, "\"cols\": %u, \"chunk_mb\": %zu, \"coded\": %zu, \"identical\": true", X.K, chunk >> 20, z1);
    variant w[] = {{"cpu10_fe+zstd", tbx_cpu_serial}, {"gpu_fe+cpu10_zstd_serial", tbx_gpu_serial}, {"gpu_fe+cpu10_zstd_pipelined", tbx_pipe}};
    bench("table_pipeline", n, w, 3, &X, extra);
    return 0;
}

/* ======================= (b) formula search ======================= */
typedef struct {
    const uint8_t *d; size_t n; uint8_t sep; g6w_sample_t S; kg2_cand *c; uint32_t nc; int threads, gpu;
    kg2_score *scr, *sc, *bound; uint32_t *pass_idx; kg2_cand *pc; kg2_score *ps; g6w_formula F[256]; int nf;
    double t_sample, t_screen, t_seed, t_score;
} fmx;
typedef struct { fmx *X; int score; uint32_t lim, n; const kg2_cand *c; kg2_score *out; const kg2_score *bound; } fm_par;
static void fm_part(void *c, uint32_t i) {
    fm_par *P = c; uint32_t i0 = i * 2048, i1 = i0 + 2048 < P->n ? i0 + 2048 : P->n;
    if (P->score) kg2_ref_formula_score(P->X->S.V, P->X->S.D, P->X->S.OK, G6W_R, P->lim, P->c, i0, i1, P->bound, P->out);
    else kg2_ref_formula_screen(P->X->S.V, P->X->S.D, P->X->S.OK, G6W_R, P->X->S.rows, P->c, i0, i1, P->out);
}
typedef struct { fmx *X; int score; uint32_t lim; const kg2_cand *c; uint32_t n; const kg2_score *bound; kg2_score *out; int rc; double gs; } hyb_ctx;
static void *hyb_gpu(void *p) {
    hyb_ctx *H = p; fmx *X = H->X;
    if (!H->n) return NULL;
    H->rc = H->score ? kg2_metal_formula_score(X->S.V, X->S.D, X->S.OK, X->S.K, G6W_R, X->S.rows, H->lim, H->c, H->n, H->bound, H->out)
                     : kg2_metal_formula_screen(X->S.V, X->S.D, X->S.OK, X->S.K, G6W_R, X->S.rows, H->c, H->n, H->out);
    H->gs = kg2_metal_last_gpu_seconds();
    return NULL;
}
static void fm_map(fmx *X, int score, uint32_t lim, const kg2_cand *c, uint32_t n, const kg2_score *bound, kg2_score *out) {
    if (!n) return;
    if (X->gpu == 2 && n >= 1024) {                  /* GPU and CPU threads together: the GPU takes the first share */
        double share = getenv("GPU2_SPLIT") ? atof(getenv("GPU2_SPLIT")) : 0.5;
        uint32_t k = (uint32_t)(n * share);
        hyb_ctx H = {X, score, lim, c, k, bound, out, 0};
        pthread_t th; pthread_create(&th, NULL, hyb_gpu, &H);
        fm_par P = {X, score, lim, n - k, c + k, out + k, bound};
        par_run(T, (n - k + 2047) / 2048, fm_part, &P);
        pthread_join(th, NULL);
        if (H.rc) { fprintf(stderr, "gpu formula failed\n"); exit(1); }
        g_gpu += H.gs;
        return;
    }
    if (X->gpu == 1 && n >= 1024) {                  /* a few hundred candidates (the seeds): one CPU thread is faster */
        int rc = score ? kg2_metal_formula_score(X->S.V, X->S.D, X->S.OK, X->S.K, G6W_R, X->S.rows, lim, c, n, bound, out)
                       : kg2_metal_formula_screen(X->S.V, X->S.D, X->S.OK, X->S.K, G6W_R, X->S.rows, c, n, out);
        if (rc) { fprintf(stderr, "gpu formula failed\n"); exit(1); }
        g_gpu += kg2_metal_last_gpu_seconds();
        return;
    }
    fm_par P = {X, score, lim, n, c, out, bound};
    par_run(X->threads, (n + 2047) / 2048, fm_part, &P);
}
/* the whole search: sample, candidates, screen, seeds, score with bounds, reduce */
static void fm_search(fmx *X) {
    double t0 = now_s();
    g6w_sample_free(&X->S);
    if (g6w_sample(X->d, X->n, X->sep, &X->S) != 1) { X->nf = 0; return; }
    free(X->c); X->nc = g6w_candidates(&X->S, &X->c);
    if (X->gpu) kg2_metal_formula_reset();
    double t1 = now_s();
    uint32_t K = X->S.K, nc = X->nc, lim = X->S.rows < 8000 ? X->S.rows : 8000;
    X->scr = realloc(X->scr, (size_t)nc * sizeof *X->scr); X->sc = realloc(X->sc, (size_t)nc * sizeof *X->sc);
    X->pass_idx = realloc(X->pass_idx, (size_t)nc * 4); X->pc = realloc(X->pc, (size_t)nc * sizeof *X->pc); X->ps = realloc(X->ps, (size_t)nc * sizeof *X->ps);
    X->bound = realloc(X->bound, (size_t)K * sizeof *X->bound);
    fm_map(X, 0, 0, X->c, nc, NULL, X->scr);
    double t2 = now_s();
    /* seeds: per target, the passing candidate with the best screen rate (first on ties) */
    uint32_t np = 0, ns = 0; kg2_cand seeds[4096]; uint32_t seed_t[4096];
    memset(X->bound, 0, (size_t)K * sizeof *X->bound);
    for (uint32_t i = 0; i < nc;) {
        uint32_t t = X->c[i].t; int64_t bh = -1, bs = 1; uint32_t best = UINT32_MAX;
        for (; i < nc && X->c[i].t == t; i++) {
            X->sc[i] = (kg2_score){0, 0};
            if (X->scr[i].seen < 0) continue;
            X->pass_idx[np] = i; X->pc[np++] = X->c[i];
            if ((int64_t)X->scr[i].hit * bs > bh * X->scr[i].seen) { bh = X->scr[i].hit; bs = X->scr[i].seen; best = i; }
        }
        if (best != UINT32_MAX) { seeds[ns] = X->c[best]; seed_t[ns++] = t; }
    }
    kg2_score sres[4096];
    fm_map(X, 1, lim, seeds, ns, X->bound, sres);            /* bound all zero: no cut */
    for (uint32_t k = 0; k < ns; k++)
        if (sres[k].seen >= 32 && 2 * sres[k].hit > sres[k].seen) X->bound[seed_t[k]] = sres[k];
    double t3 = now_s();
    fm_map(X, 1, lim, X->pc, np, X->bound, X->ps);
    for (uint32_t k = 0; k < np; k++) X->sc[X->pass_idx[k]] = X->ps[k];
    X->nf = g6w_reduce(X->c, nc, X->sc, X->F);
    double t4 = now_s();
    X->t_sample = t1 - t0; X->t_screen = t2 - t1; X->t_seed = t3 - t2; X->t_score = t4 - t3;
}
static fmx *g_fm;
static g6w_formula g_refF[256]; static int g_refn;
static void fm_ref(void *c) { fmx *X = c; g_refn = g6w_reference(X->d, X->n, X->sep, g_refF); }
static void fm_cpu1(void *c) { fmx *X = c; X->threads = 1; X->gpu = 0; fm_search(X); }
static void fm_cpuT(void *c) { fmx *X = c; X->threads = T; X->gpu = 0; fm_search(X); }
static void fm_gpu(void *c) { fmx *X = c; X->threads = 1; X->gpu = 1; fm_search(X); }
static void fm_hyb(void *c) { fmx *X = c; X->threads = 1; X->gpu = 2; fm_search(X); }
static void fm_gpu_rm(void *c) { setenv("GPU2_FM_ROWMAJOR", "1", 1); fm_gpu(c); unsetenv("GPU2_FM_ROWMAJOR"); }
static int fm_same(const g6w_formula *a, int na, const g6w_formula *b, int nb) {
    if (na != nb) return 0;
    for (int i = 0; i < na; i++) if (a[i].t != b[i].t || a[i].a != b[i].a || a[i].b != b[i].b || a[i].ke != b[i].ke || a[i].type != b[i].type) return 0;
    return 1;
}
static int cmd_formula(int argc, char **argv) {
    if (argc < 3) return 2;
    fmx X; memset(&X, 0, sizeof X); g_fm = &X;
    X.d = slurp_aligned(argv[2], &X.n, xalloc); if (!X.d) return 1;
    X.sep = g6w_sep(X.d, X.n);
    if (!kg2_metal_available()) return 1;
    fm_ref(&X);
    fm_cpu1(&X); int ok1 = fm_same(g_refF, g_refn, X.F, X.nf);
    kg2_score *cpu_sc = malloc((size_t)X.nc * sizeof *cpu_sc), *cpu_scr = malloc((size_t)X.nc * sizeof *cpu_scr);
    memcpy(cpu_sc, X.sc, (size_t)X.nc * sizeof *cpu_sc); memcpy(cpu_scr, X.scr, (size_t)X.nc * sizeof *cpu_scr);
    fm_cpuT(&X); int okT = fm_same(g_refF, g_refn, X.F, X.nf) && !memcmp(cpu_sc, X.sc, (size_t)X.nc * sizeof *cpu_sc);
    fm_gpu(&X); int okG = fm_same(g_refF, g_refn, X.F, X.nf) && !memcmp(cpu_sc, X.sc, (size_t)X.nc * sizeof *cpu_sc)
                          && !memcmp(cpu_scr, X.scr, (size_t)X.nc * sizeof *cpu_scr);
    uint32_t np = 0; for (uint32_t i = 0; i < X.nc; i++) np += X.scr[i].seen >= 0;
    fprintf(stderr, "formula: %u columns, %u rows, %u candidates, %u pass the screen, %d formulas; cpu1 %s, cpu%d %s, gpu %s\n",
            X.S.K, X.S.rows, X.nc, np, g_refn, ok1 ? "same" : "DIFFERENT", T, okT ? "same" : "DIFFERENT", okG ? "same" : "DIFFERENT");
    if (!ok1 || !okT || !okG) return 1;
    char extra[256]; snprintf(extra, sizeof extra, "\"cols\": %u, \"rows\": %u, \"candidates\": %u, \"screened_in\": %u, \"formulas\": %d, \"identical\": true",
                              X.S.K, X.S.rows, X.nc, np, g_refn);
    fm_gpu_rm(&X); if (!fm_same(g_refF, g_refn, X.F, X.nf) || memcmp(cpu_sc, X.sc, (size_t)X.nc * sizeof *cpu_sc)) { fprintf(stderr, "row-major gpu differs\n"); return 1; }
    variant v[] = {{"g6_formulas_1thread", fm_ref}, {"mapreduce_cpu1", fm_cpu1}, {"mapreduce_cpu10", fm_cpuT}, {"mapreduce_gpu", fm_gpu},
                   {"mapreduce_gpu_rowmajor", fm_gpu_rm}, {"mapreduce_gpu+cpu10", fm_hyb}};
    fm_hyb(&X); if (!fm_same(g_refF, g_refn, X.F, X.nf) || memcmp(cpu_sc, X.sc, (size_t)X.nc * sizeof *cpu_sc)) { fprintf(stderr, "hybrid differs\n"); return 1; }
    bench("formula_search", X.n, v, 6, &X, extra);
    for (int k = 0; k < 3; k++) {   /* stage split of the last runs */
        if (k == 0) fm_cpu1(&X); else if (k == 1) fm_cpuT(&X); else fm_gpu(&X);
        printf("{\"test\": \"formula_stages\", \"file\": \"%s\", \"variant\": \"%s\", \"sample_ms\": %.2f, \"screen_ms\": %.2f, \"seed_ms\": %.2f, \"score_ms\": %.2f}\n",
               g_file, k == 0 ? "cpu1" : k == 1 ? "cpu10" : "gpu", X.t_sample * 1e3, X.t_screen * 1e3, X.t_seed * 1e3, X.t_score * 1e3);
    }
    return 0;
}

/* ======================= (c) krw and (d) histograms ======================= */
typedef struct {
    const uint8_t *d; size_t n; uint32_t seg, L, nseg; uint64_t *off, *sout; uint8_t *out, *segbuf, *dec; uint64_t cap, segcap, *ssz, total;
    uint32_t *h; uint64_t *bits; uint32_t tab[1024]; uint32_t *o1, (*o1t)[65536];
} kx;
static void krw_seg_job(void *c, uint32_t s) { kx *X = c; krw_ref_encode_range(X->d, X->n, X->seg, X->L, s, s + 1, X->segbuf, X->segcap, X->ssz); }
static void krw_cpu(kx *X, int threads) {
    par_run(threads, X->nseg, krw_seg_job, X);
    uint64_t o = 0; for (uint32_t s = 0; s < X->nseg; s++) { X->sout[s] = o; o += X->ssz[s]; }
    X->sout[X->nseg] = o; X->total = o;
}
static void krw_pack_job(void *c, uint32_t s) { kx *X = c; memcpy(X->out + X->sout[s], X->segbuf + (size_t)s * X->segcap, X->ssz[s]); }
static void krw_cpu1(void *c) { kx *X = c; krw_cpu(X, 1); par_run(1, X->nseg, krw_pack_job, X); }
static void krw_cpuT(void *c) { kx *X = c; krw_cpu(X, T); par_run(T, X->nseg, krw_pack_job, X); }
static void krw_gpu(void *c) {
    kx *X = c;
    if (kg2_metal_krw_encode(X->d, X->n, X->off, X->nseg, X->L, X->out, X->cap, X->sout)) { fprintf(stderr, "gpu krw failed\n"); exit(1); }
    X->total = X->sout[X->nseg]; g_gpu += kg2_metal_last_gpu_seconds();
}
/* GPU and CPU threads together: the GPU codes the first share of the segments, the CPU threads the rest */
typedef struct { kx *X; uint32_t k; int rc; double gs; } khyb;
static void *khyb_gpu(void *p) {
    khyb *H = p; kx *X = H->X;
    if (H->k) H->rc = kg2_metal_krw_encode(X->d, X->n, X->off, H->k, X->L, X->out, X->cap, X->sout), H->gs = kg2_metal_last_gpu_seconds();
    return NULL;
}
typedef struct { kx *X; uint32_t k; } kcpu;
static void kcpu_job(void *c, uint32_t i) { kcpu *C = c; krw_seg_job(C->X, C->k + i); }
static void kcpu_pack(void *c, uint32_t i) { kcpu *C = c; krw_pack_job(C->X, C->k + i); }
static void krw_hyb(void *c) {
    kx *X = c; double share = getenv("GPU2_SPLIT") ? atof(getenv("GPU2_SPLIT")) : 0.5;
    uint32_t k = (uint32_t)(X->nseg * share);
    khyb H = {X, k, 0, 0}; pthread_t th; pthread_create(&th, NULL, khyb_gpu, &H);
    kcpu C = {X, k};
    par_run(T, X->nseg - k, kcpu_job, &C);
    pthread_join(th, NULL);
    if (H.rc) { fprintf(stderr, "gpu krw failed\n"); exit(1); }
    uint64_t o = k ? X->sout[k] : 0;
    for (uint32_t s = k; s < X->nseg; s++) { X->sout[s] = o; o += X->ssz[s]; }
    X->sout[X->nseg] = o; X->total = o;
    par_run(T, X->nseg - k, kcpu_pack, &C);
    g_gpu += H.gs;
}
/* Kolmo's rANS on the same input, cut into T parts (context: other format, other bytes) */
typedef struct { kx *X; int parts; size_t *zs; uint8_t *buf; size_t pcap; } kr;
static void kr_job(void *c, uint32_t i) {
    kr *R = c; size_t per = (R->X->n + R->parts - 1) / R->parts, b = i * per, e = b + per < R->X->n ? b + per : R->X->n;
    R->zs[i] = b < e ? kw_rans_compress(R->X->d + b, e - b, R->buf + i * R->pcap, R->pcap) : 0;
}
static kr g_kr;
static void kr_cpu1(void *c) { (void)c; g_kr.parts = 1; kr_job(&g_kr, 0); }
static void kr_cpuT(void *c) { (void)c; g_kr.parts = T; par_run(T, T, kr_job, &g_kr); }
static void krw_dec_job(void *c, uint32_t s) {
    kx *X = c;
    if (krw_ref_decode_seg(X->out + X->sout[s], X->sout[s + 1] - X->sout[s], X->L, X->dec + X->off[s], (uint32_t)(X->off[s + 1] - X->off[s])))
        fprintf(stderr, "krw decode error in segment %u\n", s);
}
static void krwd_cpu1(void *c) { par_run(1, ((kx *)c)->nseg, krw_dec_job, c); }
static void krwd_cpuT(void *c) { par_run(T, ((kx *)c)->nseg, krw_dec_job, c); }
static void krwd_gpu(void *c) {
    kx *X = c;
    if (kg2_metal_krw_decode(X->out, X->total, X->sout, X->off, X->nseg, X->L, X->dec, X->n)) { fprintf(stderr, "gpu krw decode failed\n"); exit(1); }
    g_gpu += kg2_metal_last_gpu_seconds();
}
static void kx_init(kx *X, const uint8_t *d, size_t n, uint32_t seg, uint32_t L) {
    memset(X, 0, sizeof *X); X->d = d; X->n = n; X->seg = seg; X->L = L; X->nseg = (uint32_t)((n + seg - 1) / seg);
    X->off = malloc(((size_t)X->nseg + 1) * 8); for (uint32_t s = 0; s <= X->nseg; s++) X->off[s] = (uint64_t)s * seg < n ? (uint64_t)s * seg : n;
    X->sout = malloc(((size_t)X->nseg + 1) * 8); X->ssz = malloc((size_t)X->nseg * 8);
    X->cap = krw_bound(n, seg, L); X->out = xalloc(X->cap); X->segcap = krw_seg_header(L) + 2ull * seg + 64;
    X->segbuf = xalloc(X->segcap * X->nseg); X->dec = xalloc(n + 64);
    X->h = xalloc((size_t)X->nseg * 1024); X->bits = xalloc((size_t)X->nseg * 8); kg2_log_table(X->tab);
}
static int cmd_krw(int argc, char **argv) {
    if (argc < 3) return 2;
    size_t n; uint8_t *d = slurp_aligned(argv[2], &n, xalloc); if (!d) return 1;
    uint32_t seg = (argc > 3 ? (uint32_t)atoi(argv[3]) : 1024) << 10, L = argc > 4 ? (uint32_t)atoi(argv[4]) : 256;
    if (!kg2_metal_available()) return 1;
    kx X; kx_init(&X, d, n, seg, L);
    krw_cpu1(&X); uint64_t t1 = X.total; uint8_t *ref = malloc(t1); memcpy(ref, X.out, t1);
    krw_cpuT(&X); int okT = X.total == t1 && !memcmp(ref, X.out, t1);
    krw_gpu(&X); int okG = X.total == t1 && !memcmp(ref, X.out, t1);
    memset(X.dec, 0, n); krwd_gpu(&X); int okD = !memcmp(X.dec, d, n);
    memset(X.dec, 0, n); krwd_cpuT(&X); int okDc = !memcmp(X.dec, d, n);
    fprintf(stderr, "krw: %zu -> %llu bytes (%.3f), cpu%d %s, gpu %s, gpu decode %s, cpu decode %s\n", n, (unsigned long long)t1, (double)n / t1,
            T, okT ? "same" : "DIFFERENT", okG ? "same" : "DIFFERENT", okD ? "exact" : "WRONG", okDc ? "exact" : "WRONG");
    if (!okT || !okG || !okD || !okDc) return 1;
    char extra[256]; snprintf(extra, sizeof extra, "\"seg_kb\": %u, \"lanes\": %u, \"coded\": %llu, \"identical\": true", seg >> 10, L, (unsigned long long)t1);
    krw_hyb(&X); if (X.total != t1 || memcmp(ref, X.out, t1)) { fprintf(stderr, "krw: gpu+cpu differs\n"); return 1; }
    variant v[] = {{"cpu1", krw_cpu1}, {"cpu10", krw_cpuT}, {"gpu", krw_gpu}, {"gpu+cpu10", krw_hyb}};
    bench("krw_encode", n, v, 4, &X, extra);
    g_kr.X = &X; g_kr.pcap = n + n / 8 + (1 << 20); g_kr.buf = malloc(g_kr.pcap * T); g_kr.zs = calloc(T, sizeof(size_t));
    kr_cpu1(NULL); size_t kz = g_kr.zs[0];
    char ex2[160]; snprintf(ex2, sizeof ex2, "\"kolmo_rans_coded\": %zu, \"note\": \"Kolmo rans.c, another format\"", kz);
    variant u[] = {{"kolmo_rans_cpu1", kr_cpu1}, {"kolmo_rans_cpu10", kr_cpuT}};
    bench("kolmo_rans_encode", n, u, 2, &X, ex2);
    variant w[] = {{"cpu1", krwd_cpu1}, {"cpu10", krwd_cpuT}, {"gpu", krwd_gpu}};
    bench("krw_decode", n, w, 3, &X, extra);
    return 0;
}

static void hs_job(void *c, uint32_t s) { kx *X = c; kg2_ref_hist_segs(X->d, X->off, s, s + 1, X->h, X->bits, X->tab); }
static void hs_cpu1(void *c) { par_run(1, ((kx *)c)->nseg, hs_job, c); }
static void hs_cpuT(void *c) { par_run(T, ((kx *)c)->nseg, hs_job, c); }
static void hs_gpu(void *c) { kx *X = c; if (kg2_metal_hist_segs(X->d, X->n, X->off, X->nseg, X->h, X->bits)) exit(1); g_gpu += kg2_metal_last_gpu_seconds(); }
static void o1_job(void *c, uint32_t i) {
    kx *X = c; size_t per = (X->n + 63) / 64, b = i * per, e = b + per < X->n ? b + per : X->n;
    memset(X->o1t[i], 0, sizeof X->o1t[i]); if (b < e) kg2_ref_hist_o1(X->d, b, e, X->o1t[i]);
}
static void o1_sum(kx *X) { for (uint32_t k = 0; k < 65536; k++) { uint32_t s = 0; for (int i = 0; i < 64; i++) s += X->o1t[i][k]; X->o1[k] = s; } }
static void o1_cpu1(void *c) { kx *X = c; memset(X->o1, 0, 65536 * 4); kg2_ref_hist_o1(X->d, 0, X->n, X->o1); }
static void o1_cpuT(void *c) { kx *X = c; par_run(T, 64, o1_job, c); o1_sum(X); }
static void o1_gpu(void *c) { kx *X = c; if (kg2_metal_hist_o1(X->d, X->n, X->o1)) exit(1); g_gpu += kg2_metal_last_gpu_seconds(); }
static int cmd_hist(int argc, char **argv) {
    if (argc < 3) return 2;
    size_t n; uint8_t *d = slurp_aligned(argv[2], &n, xalloc); if (!d) return 1;
    uint32_t blk = (argc > 3 ? (uint32_t)atoi(argv[3]) : 64) << 10;
    if (!kg2_metal_available()) return 1;
    kx X; kx_init(&X, d, n, blk, 32);
    uint32_t *h1 = malloc((size_t)X.nseg * 1024); uint64_t *b1 = malloc((size_t)X.nseg * 8);
    hs_cpu1(&X); memcpy(h1, X.h, (size_t)X.nseg * 1024); memcpy(b1, X.bits, (size_t)X.nseg * 8);
    hs_cpuT(&X); int okT = !memcmp(h1, X.h, (size_t)X.nseg * 1024) && !memcmp(b1, X.bits, (size_t)X.nseg * 8);
    hs_gpu(&X); int okG = !memcmp(h1, X.h, (size_t)X.nseg * 1024) && !memcmp(b1, X.bits, (size_t)X.nseg * 8);
    X.o1 = xalloc(65536 * 4); X.o1t = malloc(64 * sizeof *X.o1t); uint32_t *o1r = malloc(65536 * 4);
    o1_cpu1(&X); memcpy(o1r, X.o1, 65536 * 4); o1_cpuT(&X); int okO = !memcmp(o1r, X.o1, 65536 * 4); o1_gpu(&X); int okOG = !memcmp(o1r, X.o1, 65536 * 4);
    fprintf(stderr, "hist: %u blocks; cpu%d %s, gpu %s; order-1 cpu%d %s, gpu %s\n", X.nseg, T, okT ? "same" : "DIFFERENT", okG ? "same" : "DIFFERENT",
            T, okO ? "same" : "DIFFERENT", okOG ? "same" : "DIFFERENT");
    if (!okT || !okG || !okO || !okOG) return 1;
    char extra[128]; snprintf(extra, sizeof extra, "\"block_kb\": %u, \"blocks\": %u, \"identical\": true", blk >> 10, X.nseg);
    variant v[] = {{"cpu1", hs_cpu1}, {"cpu10", hs_cpuT}, {"gpu", hs_gpu}};
    bench("hist_blocks_entropy", n, v, 3, &X, extra);
    variant w[] = {{"cpu1", o1_cpu1}, {"cpu10", o1_cpuT}, {"gpu", o1_gpu}};
    bench("hist_order1", n, w, 3, &X, "\"identical\": true");
    return 0;
}

/* ======================= (e) video ======================= */
typedef struct { vid_t V; const uint8_t *file; size_t n; uint64_t *at; uint8_t *res, *flags; size_t gop; uint8_t *z[2]; size_t zcap, ztotal; vid_t Vg; } vx;
typedef struct { vx *X; const uint64_t *at; const vid_t *V; uint8_t *res, *flags; } vjob;
static void v_job(void *c, uint32_t j) { vjob *J = c; vid_job(J->V, J->X->file, J->at, j, J->res, J->flags); }
static void v_cpu(vx *X, int threads, const vid_t *V, const uint64_t *at, uint8_t *res, uint8_t *flags) {
    vjob J = {X, at, V, res, flags}; par_run(threads, (uint32_t)vid_jobs(V), v_job, &J);
}
static void v_gpu1(vx *X, const vid_t *V, const uint64_t *at, uint8_t *res, uint8_t *flags) {
    if (kg2_metal_kvid(X->file, X->n, at, (uint32_t)V->frames, V->np, V->w, V->h, res, V->res_bytes, flags, V->frames * V->strips_per_frame)) { fprintf(stderr, "gpu kvid failed\n"); exit(1); }
    g_gpu += kg2_metal_last_gpu_seconds();
}
static void vx_cpu1(void *c) { vx *X = c; v_cpu(X, 1, &X->V, X->at, X->res, X->flags); }
static void vx_cpuT(void *c) { vx *X = c; v_cpu(X, T, &X->V, X->at, X->res, X->flags); }
static void vx_gpu(void *c) { vx *X = c; v_gpu1(X, &X->V, X->at, X->res, X->flags); }
/* GOPs of gop frames, each predicted independently (first frame intra); coder stage zstd 1 per (plane, 4 MiB slice) */
typedef struct { vx *X; uint8_t *res; size_t len; int s; size_t *sz; } vz;
static void vz_one(void *c, uint32_t i) {
    vz *Z = c; size_t sl = 4u << 20, b = (size_t)i * sl, m = Z->len - b < sl ? Z->len - b : sl;
    size_t cap = ZSTD_compressBound(sl) + 64; size_t r = ZSTD_compress(Z->X->z[Z->s] + i * cap, cap, Z->res + b, m, 1);
    Z->sz[i] = ZSTD_isError(r) ? 0 : r;
}
static size_t vz_stage(vx *X, uint8_t *res, size_t len, int s) {
    uint32_t nj = (uint32_t)((len + (4u << 20) - 1) / (4u << 20)); size_t sz[4096]; vz Z = {X, res, len, s, sz};
    par_run(T, nj, vz_one, &Z); size_t t = 0; for (uint32_t i = 0; i < nj; i++) t += sz[i]; return t;
}
static size_t gops(vx *X) { return (X->V.frames + X->gop - 1) / X->gop; }
static void gop_geom(vx *X, size_t g, vid_t *G, const uint64_t **at) { *G = X->V; G->frames = X->V.frames - g * X->gop < X->gop ? X->V.frames - g * X->gop : X->gop; vid_geom(G); *at = X->at + g * X->gop; }
static uint8_t *gop_res(vx *X, int s) { return X->res + (size_t)s * X->Vg.res_bytes; }
static uint8_t *gop_flags(vx *X, int s) { return X->flags + (size_t)s * X->Vg.frames * X->Vg.strips_per_frame; }
static void vx_cpu_serial(void *c) {
    vx *X = c; X->ztotal = 0;
    for (size_t g = 0; g < gops(X); g++) { vid_t G; const uint64_t *at; gop_geom(X, g, &G, &at); v_cpu(X, T, &G, at, gop_res(X, 0), gop_flags(X, 0)); X->ztotal += vz_stage(X, gop_res(X, 0), G.res_bytes, 0); }
}
static void vx_gpu_serial(void *c) {
    vx *X = c; X->ztotal = 0;
    for (size_t g = 0; g < gops(X); g++) { vid_t G; const uint64_t *at; gop_geom(X, g, &G, &at); v_gpu1(X, &G, at, gop_res(X, 0), gop_flags(X, 0)); X->ztotal += vz_stage(X, gop_res(X, 0), G.res_bytes, 0); }
}
typedef struct { vx *X; dispatch_semaphore_t ready[2], free_[2]; } vpipe;
static void *v_feeder(void *c) {
    vpipe *P = c; vx *X = P->X;
    for (size_t g = 0; g < gops(X); g++) {
        int s = (int)(g & 1); vid_t G; const uint64_t *at; gop_geom(X, g, &G, &at);
        dispatch_semaphore_wait(P->free_[s], DISPATCH_TIME_FOREVER);
        v_gpu1(X, &G, at, gop_res(X, s), gop_flags(X, s));
        dispatch_semaphore_signal(P->ready[s]);
    }
    return NULL;
}
static void vx_pipe(void *c) {
    vx *X = c; X->ztotal = 0;
    vpipe P = {X, {dispatch_semaphore_create(0), dispatch_semaphore_create(0)}, {dispatch_semaphore_create(1), dispatch_semaphore_create(1)}};
    pthread_t th; pthread_create(&th, NULL, v_feeder, &P);
    for (size_t g = 0; g < gops(X); g++) {
        int s = (int)(g & 1); vid_t G; const uint64_t *at; gop_geom(X, g, &G, &at);
        dispatch_semaphore_wait(P.ready[s], DISPATCH_TIME_FOREVER);
        X->ztotal += vz_stage(X, gop_res(X, s), G.res_bytes, s);
        dispatch_semaphore_signal(P.free_[s]);
    }
    pthread_join(th, NULL);
}
static int y4m_parse(const uint8_t *s, size_t n, vx *X) {
    const uint8_t *e = memchr(s, '\n', n < 4096 ? n : 4096); if (!e || memcmp(s, "YUV4MPEG2 ", 10)) return -1;
    size_t W = 0, H = 0; int c444 = 0;
    for (const uint8_t *p = s; p < e; p++) if (*p == ' ') {
        if (p[1] == 'W') W = (size_t)atol((const char *)p + 2);
        else if (p[1] == 'H') H = (size_t)atol((const char *)p + 2);
        else if (p[1] == 'C' && !memcmp(p + 2, "444", 3)) c444 = 1;
    }
    X->V.np = 3; X->V.w[0] = W; X->V.h[0] = H;
    X->V.w[1] = X->V.w[2] = c444 ? W : (W + 1) / 2; X->V.h[1] = X->V.h[2] = c444 ? H : (H + 1) / 2;
    vid_geom(&X->V);
    size_t fb = X->V.frame_samples, i = (size_t)(e - s) + 1, f = 0;
    X->at = malloc((n / fb + 1) * 8);
    while (i + 6 <= n && !memcmp(s + i, "FRAME", 5)) {
        const uint8_t *nl = memchr(s + i, '\n', n - i); if (!nl) break;
        i = (size_t)(nl - s) + 1; if (i + fb > n) break;
        X->at[f++] = i; i += fb;
    }
    X->V.frames = f; vid_geom(&X->V);
    return f ? 0 : -1;
}
static int cmd_video(int argc, char **argv) {
    if (argc < 3) return 2;
    vx X; memset(&X, 0, sizeof X);
    X.file = slurp_aligned(argv[2], &X.n, xalloc); if (!X.file || y4m_parse(X.file, X.n, &X)) return 1;
    X.gop = argc > 3 ? (size_t)atoi(argv[3]) : 8;
    if (!kg2_metal_available()) return 1;
    X.res = xalloc(X.V.res_bytes); X.flags = xalloc(X.V.frames * X.V.strips_per_frame + 64);
    uint8_t *r1 = malloc(X.V.res_bytes), *f1 = malloc(X.V.frames * X.V.strips_per_frame);
    vx_cpu1(&X); memcpy(r1, X.res, X.V.res_bytes); memcpy(f1, X.flags, X.V.frames * X.V.strips_per_frame);
    vx_cpuT(&X); int okT = !memcmp(r1, X.res, X.V.res_bytes) && !memcmp(f1, X.flags, X.V.frames * X.V.strips_per_frame);
    memset(X.res, 0, X.V.res_bytes); vx_gpu(&X); int okG = !memcmp(r1, X.res, X.V.res_bytes) && !memcmp(f1, X.flags, X.V.frames * X.V.strips_per_frame);
    fprintf(stderr, "video: %zux%zu, %zu frames; cpu%d %s, gpu %s\n", X.V.w[0], X.V.h[0], X.V.frames, T, okT ? "same" : "DIFFERENT", okG ? "same" : "DIFFERENT");
    if (!okT || !okG) return 1;
    char extra[160]; snprintf(extra, sizeof extra, "\"width\": %zu, \"height\": %zu, \"frames\": %zu, \"identical\": true", X.V.w[0], X.V.h[0], X.V.frames);
    variant v[] = {{"cpu1_neon", vx_cpu1}, {"cpu10_neon", vx_cpuT}, {"gpu", vx_gpu}};
    bench("video_predict", X.n, v, 3, &X, extra);
    /* pipeline over GOPs */
    { vid_t G; const uint64_t *at; gop_geom(&X, 0, &G, &at); X.Vg = G; }
    kg2_metal_free(X.res, X.V.res_bytes); X.res = xalloc(2 * X.Vg.res_bytes); kg2_metal_free(X.flags, X.V.frames * X.V.strips_per_frame + 64); X.flags = xalloc(2 * X.Vg.frames * X.Vg.strips_per_frame + 64);
    X.zcap = (X.Vg.res_bytes / (4u << 20) + 2) * (ZSTD_compressBound(4u << 20) + 64); X.z[0] = malloc(X.zcap); X.z[1] = malloc(X.zcap);
    vx_cpu_serial(&X); size_t z1 = X.ztotal; vx_pipe(&X); size_t z2 = X.ztotal; vx_gpu_serial(&X); size_t z3 = X.ztotal;
    if (z1 != z2 || z1 != z3) { fprintf(stderr, "video coder sizes differ\n"); return 1; }
    snprintf(extra, sizeof extra, "\"width\": %zu, \"frames\": %zu, \"gop\": %zu, \"coded\": %zu, \"identical\": true", X.V.w[0], X.V.frames, X.gop, z1);
    variant w[] = {{"cpu10_predict+zstd", vx_cpu_serial}, {"gpu_predict+cpu10_zstd_serial", vx_gpu_serial}, {"gpu_predict+cpu10_zstd_pipelined", vx_pipe}};
    bench("video_pipeline", X.n, w, 3, &X, extra);
    return 0;
}

/* ======================= (f) many small files ======================= */
typedef struct { kx K; uint32_t nfiles; } bx;
static void bx_krw_job(void *c, uint32_t s) {
    kx *X = c; uint32_t m = (uint32_t)(X->off[s + 1] - X->off[s]); uint8_t *o = X->segbuf + X->sout[s];
    uint32_t cnt[256] = {0}; for (uint32_t i = 0; i < m; i++) cnt[X->d[X->off[s] + i]]++;
    uint16_t f[256]; krw_norm(cnt, m, f); memcpy(o, f, 512);
    uint32_t *st = (uint32_t *)(o + 512); uint16_t scratch[1 << 16];
    krw_ref_encode_seg(X->d + X->off[s], m, X->L, f, st, st + X->L, (uint16_t *)(st + 2 * X->L), scratch);
}
static void bx_pre(kx *X) { uint64_t o = 0; for (uint32_t s = 0; s < X->nseg; s++) { X->sout[s] = o; o += krw_seg_header(X->L) + 2 * (X->off[s + 1] - X->off[s]) + 8; } }
static void bx_cpu1(void *c) { kx *X = c; par_run(1, X->nseg, hs_job, X); par_run(1, X->nseg, bx_krw_job, X); }
static void bx_cpuT(void *c) { kx *X = c; par_run(T, X->nseg, hs_job, X); par_run(T, X->nseg, bx_krw_job, X); }
static uint64_t *g_bsout;
static void bx_gpu(void *c) {
    kx *X = c;
    if (kg2_metal_hist_segs(X->d, X->n, X->off, X->nseg, X->h, X->bits)) exit(1);
    g_gpu += kg2_metal_last_gpu_seconds();
    if (kg2_metal_krw_encode(X->d, X->n, X->off, X->nseg, X->L, X->out, X->cap, g_bsout)) exit(1);
    g_gpu += kg2_metal_last_gpu_seconds();
}
static void bx_gpu_each(void *c) {   /* one GPU call per file: the per-call cost */
    kx *X = c; uint64_t so[2];
    for (uint32_t s = 0; s < X->nseg; s++) {
        uint64_t o2[2] = {0, X->off[s + 1] - X->off[s]};
        if (kg2_metal_hist_segs(X->d + X->off[s], o2[1], o2, 1, X->h + 256 * (size_t)s, X->bits + s)) exit(1);
        g_gpu += kg2_metal_last_gpu_seconds();
        if (kg2_metal_krw_encode(X->d + X->off[s], o2[1], o2, 1, X->L, X->out, X->cap, so)) exit(1);
        g_gpu += kg2_metal_last_gpu_seconds();
    }
}
static int cmd_batch(int argc, char **argv) {
    if (argc < 3) return 2;
    FILE *lf = fopen(argv[2], "r"); if (!lf) return 1;
    char path[4096]; size_t tot = 0, cap = 1u << 20; uint32_t nf = 0, fcap = 1024;
    uint64_t *off = malloc((fcap + 1) * 8); uint8_t *buf = malloc(cap); off[0] = 0;
    while (fgets(path, sizeof path, lf)) {
        path[strcspn(path, "\n")] = 0; FILE *f = fopen(path, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long m = ftell(f); fseek(f, 0, SEEK_SET);
        if (m <= 0 || m > (1 << 20)) { fclose(f); continue; }
        size_t a = tot;
        while (a + (size_t)m > cap) { cap *= 2; buf = realloc(buf, cap); }
        if (fread(buf + a, 1, (size_t)m, f) != (size_t)m) { fclose(f); continue; }
        fclose(f);
        if (nf == fcap) { fcap *= 2; off = realloc(off, (fcap + 1) * 8); }
        off[nf] = a; tot = a + (size_t)m; off[++nf] = tot;
    }
    fclose(lf);
    if (!nf) return 1;
    uint8_t *d = xalloc(tot + 64); memcpy(d, buf, tot); free(buf);
    kx X; memset(&X, 0, sizeof X); X.d = d; X.n = tot; X.nseg = nf; X.L = 32; X.off = off; off[nf] = tot;
    X.sout = malloc((nf + 1) * 8); X.h = xalloc((size_t)nf * 1024); X.bits = xalloc((size_t)nf * 8); kg2_log_table(X.tab);
    X.cap = (uint64_t)nf * (krw_seg_header(X.L) + 8) + 2 * tot + 64; X.out = xalloc(X.cap);
    bx_pre(&X); X.segbuf = xalloc(X.sout[nf - 1] + krw_seg_header(X.L) + 2 * (off[nf] - off[nf - 1]) + 64);
    g_bsout = malloc((nf + 1) * 8);
    if (!kg2_metal_available()) return 1;
    /* exactness: GPU segments equal the CPU's */
    bx_cpu1(&X); uint32_t *h1 = malloc((size_t)nf * 1024); memcpy(h1, X.h, (size_t)nf * 1024);
    bx_gpu(&X); int okh = !memcmp(h1, X.h, (size_t)nf * 1024), okk = 1;
    for (uint32_t s = 0; s < nf && okk; s++) {
        uint64_t len = g_bsout[s + 1] - g_bsout[s], words = (len - krw_seg_header(X.L)) / 2;
        uint8_t *cpu = X.segbuf + X.sout[s]; const uint32_t *cnts = (const uint32_t *)(cpu + 512) + X.L; uint64_t cw = 0;
        for (uint32_t l = 0; l < X.L; l++) cw += cnts[l];
        okk = cw == words && !memcmp(cpu, X.out + g_bsout[s], len);
    }
    uint64_t mn = UINT64_MAX, mx = 0; for (uint32_t s = 0; s < nf; s++) { uint64_t m = off[s + 1] - off[s]; if (m < mn) mn = m; if (m > mx) mx = m; }
    fprintf(stderr, "batch: %u files, %zu bytes (%llu..%llu per file); histograms %s, krw %s\n", nf, tot, (unsigned long long)mn, (unsigned long long)mx,
            okh ? "same" : "DIFFERENT", okk ? "same" : "DIFFERENT");
    if (!okh || !okk) return 1;
    char extra[160]; snprintf(extra, sizeof extra, "\"files\": %u, \"lanes\": %u, \"identical\": true", nf, X.L);
    variant v[] = {{"cpu1", bx_cpu1}, {"cpu10", bx_cpuT}, {"gpu_one_batch", bx_gpu}, {"gpu_call_per_file", bx_gpu_each}};
    int saveR = ROUNDS; if (nf > 2000) ROUNDS = ROUNDS < 3 ? ROUNDS : 3;
    bench("batch_hist_krw", tot, v, 4, &X, extra);
    ROUNDS = saveR;
    return 0;
}

/* ======================= (g) BWT of one large block ======================= */
typedef struct { uint8_t *t, *u; int32_t *A, *I; int32_t n, r; } sx;
static void sa_cpu(sx *X, int th) { if (kolmo_sais_bwt_aux(X->t, X->u, X->A, X->n, X->r, X->I, th)) { fprintf(stderr, "sais failed\n"); exit(1); } }
static void sa_cpu1(void *c) { sa_cpu(c, 1); }
static void sa_cpuT(void *c) { sa_cpu(c, T); }
static void sa_gpu(void *c) {
    sx *X = c;
    if (kolmo_metal_bwt_aux(X->t, X->u, X->n, X->r, X->I)) { fprintf(stderr, "gpu bwt failed\n"); exit(1); }
    g_gpu += kolmo_metal_last_gpu_seconds();
}
static int cmd_sa(int argc, char **argv) {
    if (argc < 4) return 2;
    size_t n; uint8_t *d = slurp_aligned(argv[2], &n, xalloc); if (!d) return 1;
    size_t m = (size_t)atoi(argv[3]) << 20; if (m > n) m = n;
    sx X = {d, malloc(m), malloc(m * 4), NULL, (int32_t)m, 1 << 20};
    X.I = malloc(((m - 1) / X.r + 1) * 4 + 16);
    if (!kolmo_metal_available()) return 1;
    size_t need = kolmo_metal_sa_bytes(m), budget = kolmo_metal_budget();
    fprintf(stderr, "sa: %zu MB, GPU needs %.1f GB, budget %.1f GB\n", m >> 20, need / 1e9, budget / 1e9);
    sa_cpuT(&X); uint8_t *u1 = malloc(m); int32_t *I1 = malloc(((m - 1) / X.r + 1) * 4); memcpy(u1, X.u, m); memcpy(I1, X.I, ((m - 1) / X.r + 1) * 4);
    double t0 = now_s(); sa_gpu(&X); double tg = now_s() - t0;
    int ok = !memcmp(u1, X.u, m) && !memcmp(I1, X.I, ((m - 1) / X.r + 1) * 4);
    fprintf(stderr, "sa: gpu %s (%.2f s, %d rounds)\n", ok ? "same" : "DIFFERENT", tg, kolmo_metal_last_rounds());
    if (!ok) return 1;
    char extra[128]; snprintf(extra, sizeof extra, "\"block_mb\": %zu, \"rounds\": %d, \"identical\": true", m >> 20, kolmo_metal_last_rounds());
    variant v[] = {{"libsais_cpu1", sa_cpu1}, {"libsais_cpu10", sa_cpuT}, {"gpu", sa_gpu}};
    int saveR = ROUNDS; ROUNDS = ROUNDS < 3 ? ROUNDS : 3;
    bench("bwt_block", m, v, 3, &X, extra);
    ROUNDS = saveR;
    return 0;
}

/* ======================= check ======================= */
static uint64_t rng = 88172645463325252ull;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static int check_table_buf(const uint8_t *d, size_t n, int mode) {
    uint32_t K = kg2_ref_first_nf(d, n, mode, ','), rows = kg2_ref_count_lines(d, n);
    kg2_table a, b, c; tb_alloc(&a, rows, K); tb_alloc(&b, rows, K); tb_alloc(&c, rows, K);
    fe_cpu(1, d, n, mode, ',', &a); fe_cpu(T, d, n, mode, ',', &b);
    int g = kg2_metal_table(d, n, mode, ',', &c);
    return g == 0 && tb_same(&a, &b) && tb_same(&a, &c);
}
static int cmd_check(int argc, char **argv) {
    if (!kg2_metal_available()) { fprintf(stderr, "no Metal device\n"); return 1; }
    fprintf(stderr, "Metal set-up %.1f ms\n", kg2_metal_setup_seconds() * 1e3);
    int fails = 0, cases = 0;
    /* table: random CSV and JSON lines with quotes, numbers of every shape, ragged rows */
    static const char *atoms[] = {"0", "-0", "007", "12", "-3.5", "1.", ".5", "3.14159", "-0.0", "123456789012345678", "1234567890123456789",
                                  "\"a,b\"", "\"x\"\"y\"", "abc", "", "1e5", "-", "99.990", "\"{:}\"", "\\\"q"};
    for (int it = 0; it < 120; it++) {
        int mode = it & 1; size_t cap = 1 << 18, n = 0; uint8_t *d = xalloc(cap);
        int K = 1 + (int)(rnd() % 12), lines = 1 + (int)(rnd() % (it < 60 ? 50 : 3000));
        for (int l = 0; l < lines && n + 4096 < cap; l++) {
            int k = K + ((rnd() % 9 == 0) ? (int)(rnd() % 5) - 2 : 0); if (k < 1) k = 1;
            if (mode) d[n++] = '{';
            for (int f = 0; f < k; f++) {
                if (f) d[n++] = (mode && (f & 1)) ? ':' : ',';
                const char *a = atoms[rnd() % 20];
                if (rnd() % 3 == 0) { char t[24]; int m = snprintf(t, sizeof t, "%lld", (long long)(rnd() % 2000) - 1000); memcpy(d + n, t, (size_t)m); n += (size_t)m; }
                else { size_t m = strlen(a); memcpy(d + n, a, m); n += m; }
            }
            if (mode) d[n++] = '}';
            if (l + 1 < lines || rnd() % 2) d[n++] = '\n';
        }
        if (!n) d[n++] = 'x';
        cases++;
        if (!check_table_buf(d, n, mode)) {
            fails++; fprintf(stderr, "table case %d (mode %d, %zu bytes) FAILED\n", it, mode, n);
            if (getenv("GPU2_DUMP")) { FILE *f = fopen(getenv("GPU2_DUMP"), "wb"); if (f) { fwrite(d, 1, n, f); fclose(f); } }
        }
        kg2_metal_free(d, cap);
    }
    for (int a = 2; a < argc; a++) {
        size_t n; uint8_t *d = slurp_aligned(argv[a], &n, xalloc); if (!d) continue;
        int mode = n && d[0] == '{';
        cases++; if (!check_table_buf(d, n, mode)) { fails++; fprintf(stderr, "table %s FAILED\n", argv[a]); }
    }
    /* histograms and krw: random segments, skewed data, single-symbol and empty segments */
    for (int it = 0; it < 60; it++) {
        size_t n = (size_t)(rnd() % (it < 30 ? 70000 : 3000000)) + 1; uint8_t *d = xalloc(n + 64);
        int kind = it % 4;
        for (size_t i = 0; i < n; i++) d[i] = kind == 0 ? (uint8_t)rnd() : kind == 1 ? (uint8_t)(rnd() % 3 ? 'a' : rnd() % 7) : kind == 2 ? 7 : (uint8_t)("hello world "[i % 12]);
        uint32_t nseg = 1 + (uint32_t)(rnd() % 40); uint64_t *off = malloc((nseg + 1) * 8); off[0] = 0;
        for (uint32_t s = 1; s < nseg; s++) { uint64_t x = rnd() % (n + 1); off[s] = x < off[s - 1] ? off[s - 1] : x; if (rnd() % 7 == 0) off[s] = off[s - 1]; }
        off[nseg] = n;
        uint32_t tab[1024]; kg2_log_table(tab);
        uint32_t *h1 = malloc((size_t)nseg * 1024), *h2 = malloc((size_t)nseg * 1024); uint64_t *b1 = malloc(nseg * 8), *b2 = malloc(nseg * 8);
        kg2_ref_hist_segs(d, off, 0, nseg, h1, b1, tab);
        cases++; if (kg2_metal_hist_segs(d, n, off, nseg, h2, b2) || memcmp(h1, h2, (size_t)nseg * 1024) || memcmp(b1, b2, nseg * 8)) { fails++; fprintf(stderr, "hist case %d FAILED\n", it); }
        uint32_t *o1 = calloc(65536, 4), *o2 = calloc(65536, 4); kg2_ref_hist_o1(d, 0, n, o1);
        cases++; if (kg2_metal_hist_o1(d, n, o2) || memcmp(o1, o2, 65536 * 4)) { fails++; fprintf(stderr, "o1 case %d FAILED\n", it); }
        static const uint32_t Ls[4] = {32, 64, 256, 1024};
        uint32_t L = Ls[it % 4]; uint64_t maxseg = 0; for (uint32_t s = 0; s < nseg; s++) if (off[s + 1] - off[s] > maxseg) maxseg = off[s + 1] - off[s];
        if (maxseg < (1u << 24)) {
            uint64_t cap = (uint64_t)nseg * krw_seg_header(L) + 2 * n + 64; uint8_t *g = xalloc(cap), *c = malloc(cap), *dec = xalloc(n + 64);
            uint64_t *so = malloc((nseg + 1) * 8); uint16_t *scr = malloc((maxseg / L + 4) * 2);
            int bad = kg2_metal_krw_encode(d, n, off, nseg, L, g, cap, so) != 0;
            uint64_t o = 0;
            for (uint32_t s = 0; s < nseg && !bad; s++) {
                uint32_t m = (uint32_t)(off[s + 1] - off[s]), cnt[256] = {0}; for (uint32_t i = 0; i < m; i++) cnt[d[off[s] + i]]++;
                uint16_t f[256]; krw_norm(cnt, m, f); memcpy(c + o, f, 512);
                uint32_t *st = (uint32_t *)(c + o + 512);
                uint64_t w = krw_ref_encode_seg(d + off[s], m, L, f, st, st + L, (uint16_t *)(st + 2 * L), scr);
                uint64_t len = krw_seg_header(L) + 2 * w;
                if (so[s] != o || memcmp(c + o, g + o, len)) bad = 1;
                if (!bad && krw_ref_decode_seg(c + o, len, L, dec + off[s], m)) bad = 2;
                o += len;
            }
            if (!bad && (so[nseg] != o || memcmp(dec, d, n))) bad = 3;
            if (!bad) { memset(dec, 0, n); if (kg2_metal_krw_decode(g, o, so, off, nseg, L, dec, n) || memcmp(dec, d, n)) bad = 4; }
            cases++; if (bad) { fails++; fprintf(stderr, "krw case %d (L %u, %u segments) FAILED (%d)\n", it, L, nseg, bad); }
            kg2_metal_free(g, cap); free(c); kg2_metal_free(dec, n + 64); free(so); free(scr);
        }
        free(h1); free(h2); free(b1); free(b2); free(o1); free(o2); free(off); kg2_metal_free(d, n + 64);
    }
    /* formulas on the given files (tables): the GPU's screen and scores equal the CPU's, the formulas g6_formulas' */
    for (int a = 2; a < argc; a++) {
        fmx X; memset(&X, 0, sizeof X);
        X.d = slurp_aligned(argv[a], &X.n, xalloc); if (!X.d) continue;
        X.sep = g6w_sep(X.d, X.n);
        g6w_formula F[256]; int nf = g6w_reference(X.d, X.n, X.sep, F);
        fm_cpu1(&X); int ok = fm_same(F, nf, X.F, X.nf);
        kg2_score *s1 = malloc((size_t)X.nc * 8 + 8), *q1 = malloc((size_t)X.nc * 8 + 8);
        if (X.nc) { memcpy(s1, X.sc, (size_t)X.nc * 8); memcpy(q1, X.scr, (size_t)X.nc * 8); }
        fm_gpu(&X); ok = ok && fm_same(F, nf, X.F, X.nf) && (!X.nc || (!memcmp(s1, X.sc, (size_t)X.nc * 8) && !memcmp(q1, X.scr, (size_t)X.nc * 8)));
        fprintf(stderr, "formula %s: %d formulas, %u candidates: %s\n", argv[a], nf, X.nc, ok ? "same" : "DIFFERENT");
        cases++; if (!ok) fails++;
        free(s1); free(q1);
    }
    /* kg2_fpred against g6_fpred is covered through the formulas: the reference is g6_formulas itself */
    fprintf(stderr, "check: %d cases, %d failed\n", cases, fails);
    return fails ? 1 : 0;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc < 2) { fprintf(stderr, "usage: gpu2bench check|table|formula|krw|hist|video|batch|sa ...\n"); return 2; }
        if (getenv("GPU2_ROUNDS")) ROUNDS = atoi(getenv("GPU2_ROUNDS"));
        if (getenv("GPU2_T")) T = atoi(getenv("GPU2_T"));
        if (argc > 2) { const char *s = strrchr(argv[2], '/'); g_file = s ? s + 1 : argv[2]; }
        util_start();
        const char *c = argv[1];
        if (!strcmp(c, "check")) return cmd_check(argc, argv);
        if (!getenv("GPU2_CHECK_ONLY") && lock_present()) { fprintf(stderr, "gpu2bench: benchmark lock present, not timing\n"); return 3; }
        if (!strcmp(c, "table")) return cmd_table(argc, argv);
        if (!strcmp(c, "formula")) return cmd_formula(argc, argv);
        if (!strcmp(c, "krw")) return cmd_krw(argc, argv);
        if (!strcmp(c, "hist")) return cmd_hist(argc, argv);
        if (!strcmp(c, "video")) return cmd_video(argc, argv);
        if (!strcmp(c, "batch")) return cmd_batch(argc, argv);
        if (!strcmp(c, "sa")) return cmd_sa(argc, argv);
        return 2;
    }
}
