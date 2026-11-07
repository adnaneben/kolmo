/* ---------------------------------------------------------------- table transform v2 (block flag 4)
 * Adds to v1, per numeric column and per block, a predictor chosen by estimated cost:
 *   mode 0  raw values                      mode 1  delta down the column (v1)
 *   mode 2  delta inside an entity group    mode 3  residual against a formula of other columns
 * Formulas (y = k*a/b, y = k*(a[t]-a[t-1]), y = 100k*(a[t]-a[t-1])/a[t-1]) are discovered once per file on a row
 * sample and written into every block header, so blocks stay independent. Prediction is plain IEEE double
 * (/ * - only, no fused multiply-add: compiled with -ffp-contract=off) and rint, identical on encode and decode.
 * Layout: k | nrows | nexc | noNL | gcol i32 | nform (u32) | formulas nform x {y u16, a u16, b u16, form u8, 0, k f64}
 *         | exceptions (row,cnt) u32 | per column: type u8, size u32 | sections
 * Numeric column (type 2): cnt nnum nexc u32 | S w mode fid u8 | kind[cnt] | sd[nnum] | planes | elen u32[nexc] | edata */
#include <math.h>
static inline uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static const double P10D[19] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                1e16, 1e17, 1e18};
enum { FM_RATIO = 1, FM_DIFF = 2, FM_PCT = 3 };
typedef struct { uint16_t y, a, b; uint8_t form; double k; } tformula;   /* b = FM_NONE for lag forms */
typedef struct { int n, gcol; tformula f[256]; } fset_t;
#define FM_NONE 0xFFFF
#define FS_SAMPLE 1024
#ifndef FS_TOL
#define FS_TOL 5e-2
#endif
#define SCREEN 256

static inline double fm_predict(int form, double k, double fa, double fb, double la) {
    if (form == FM_RATIO) return fa / fb * k;
    if (form == FM_DIFF) return (fa - la) * k;
    return (fa - la) / la * 100.0 * k;
}
static inline int fm_int(int form, double k, double fa, double fb, double la, int S, int64_t *pi) {
    double p = fm_predict(form, k, fa, fb, la) * P10D[S];
    if (!(fabs(p) < 4611686018427387904.0)) return 0;                 /* rejects NaN and infinities too */
    *pi = (int64_t)rint(p); return 1;
}
static inline uint64_t zzg(int64_t d) { return ((uint64_t)d << 1) ^ (uint64_t)(d >> 63); }
static inline int64_t unzz(uint64_t z) { return (int64_t)(z >> 1) ^ -(int64_t)(z & 1); }
static inline int bitlen(uint64_t z) { return z ? 64 - __builtin_clzll(z) : 0; }

/* ------------------------------------------------ discovery (file level, encoder only) */
static int split_row(const uint8_t *s, size_t i, size_t n, uint8_t delim, int k, int quotes, size_t *off, uint32_t *len) {
    int j = 0;
    for (;;) {
        int split = j < k - 1;
        size_t e = quotes ? field_end_q(s, i, n, delim, split) : i + (split ? scan2(s + i, n - i, delim, '\n') : scan1(s + i, n - i, '\n'));
        off[j] = i; len[j] = (uint32_t)(e - i); j++;
        if (e >= n || s[e] == '\n' || j == k) return j;
        i = e + 1;
    }
}
static int cmp_dbl(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
typedef struct { double score; int y, a, b, form; double k; } fcand;
static int cmp_fcand(const void *p, const void *q) {
    const fcand *a = p, *b = q;
    if (a->score != b->score) return a->score < b->score ? -1 : 1;
    if (a->y != b->y) return a->y - b->y;
    if (a->form != b->form) return a->form - b->form;
    if (a->a != b->a) return a->a - b->a;
    return a->b - b->b;
}

typedef float v8f __attribute__((vector_size(32)));
static inline v8f ld8(const float *p) { v8f v; memcpy(&v, p, 32); return v; }
/* ((t0 + t4) + (t2 + t6)) + ((t1 + t5) + (t3 + t7)) on every compiler and CPU, so the screen decides alike everywhere */
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 12)
typedef float v4f __attribute__((vector_size(16)));
static inline float hsum8(v8f v) {
    v4f a = __builtin_shufflevector(v, v, 0, 1, 2, 3) + __builtin_shufflevector(v, v, 4, 5, 6, 7);
    a += __builtin_shufflevector(a, a, 2, 3, 0, 1);
    return a[0] + a[1];
}
#else
static inline float hsum8(v8f v) {
    float t[8]; memcpy(t, &v, 32);
    return ((t[0] + t[4]) + (t[2] + t[6])) + ((t[1] + t[5]) + (t[3] + t[7]));
}
#endif
typedef struct { int m, ns, SS, NW, SCNT, SM;
    const int *keys, *vc, *Smax; const double *F, *L, *PD, *PP, *LPD, *LPP; const uint8_t *V;
    const float *Lf, *Vf, *L2f, *Lyr, *LPDf, *LPPf, *VPDf, *VPPf; const uint64_t *VB; } fsctx;
typedef struct { const fsctx *c; int y0, y1; float *Wa, *Da, *D2a; double *ratio; fcand *cands; size_t nc, ccap; } fswork;
/* one worker over a disjoint range of target columns; own scratch + candidate buffer, shared arrays read-only.
 * The total-order cmp_fcand makes the merged candidate set sort identically to the serial version. */
static void *fs_worker(void *arg) {
    fswork *W = arg; const fsctx *c = W->c;
    for (int yi = W->y0; yi < W->y1; yi++) {

            if (c->vc[yi] < 100) continue;
            int Ty = c->keys[yi];
            /* W->ratio screen: exact masked moments of (Ly - La + Lb) on a row subsample, every (a, b) pair (float SIMD) */
            int ta[32], tb[32], tf[32], nt = 0; double tv[12]; int tva[12], tvb[12], ntv = 0;
            {
                const float *ly = c->Lf + (size_t)yi * c->SS; const float *vy = c->Vf + (size_t)yi * c->SS;
                for (int a = 0; a < c->m; a++) {
                    if (a == yi) continue;
                    const float *la = c->Lf + (size_t)a * c->SS, *va = c->Vf + (size_t)a * c->SS;
                    uint64_t wb[SCREEN / 64]; int wc = 0;
                    for (int u = 0; u < c->SS / 64 + (c->SS % 64 != 0); u++) { wb[u] = c->VB[(size_t)yi * c->NW + u] & c->VB[(size_t)a * c->NW + u]; wc += __builtin_popcountll(wb[u]); }
                    if (wc < c->SCNT) continue;
                    for (int i = 0; i < c->SS; i++) { float w = vy[i] * va[i], d = (ly[i] - la[i]) * w; W->Wa[i] = w; W->Da[i] = d; W->D2a[i] = d * d; }
                    for (int b = 0; b < c->m; b++) {
                        if (b == yi || b == a) continue;
                        { int cc = 0; for (int u = 0; u < c->NW; u++) cc += __builtin_popcountll(wb[u] & c->VB[(size_t)b * c->NW + u]); if (cc < c->SCNT) continue; }
                        const float *vb = c->Vf + (size_t)b * c->SS, *lb = c->Lf + (size_t)b * c->SS, *l2 = c->L2f + (size_t)b * c->SS;
                        v8f c0 = {0}, c1 = {0}, c2 = {0}, c3 = {0}, c4 = {0}, c5 = {0};
                        int i = 0;
#define FS_ACC(i) { v8f w_ = ld8(W->Wa + (i)), d_ = ld8(W->Da + (i)), e_ = ld8(W->D2a + (i)), vv = ld8(vb + (i)), ll = ld8(lb + (i)), qq = ld8(l2 + (i)); \
                    c0 += w_ * vv; c1 += d_ * vv; c2 += w_ * ll; c3 += e_ * vv; c4 += d_ * ll; c5 += w_ * qq; }
                        if (c->SS >= 64) {   /* early rejects on the first 8, then 32 rows: a true formula is tight on every subset */
                            FS_ACC(0); i = 8;
                            { float pc = hsum8(c0), p1 = hsum8(c1 + c2), p2 = hsum8(c3 + 2.0f * c4 + c5);
                              if (pc >= 6 && p2 * pc - p1 * p1 >= (float)(16 * FS_TOL * FS_TOL) * pc * pc) continue; }
                            for (; i < 32; i += 8) FS_ACC(i);
                            { float pc = hsum8(c0), p1 = hsum8(c1 + c2), p2 = hsum8(c3 + 2.0f * c4 + c5);
                              if (pc >= 8 && p2 * pc - p1 * p1 >= (float)(16 * FS_TOL * FS_TOL) * pc * pc) continue; }
                        }
                        for (; i < c->SS; i += 8) FS_ACC(i);
#undef FS_ACC
                        double cnt = hsum8(c0), s1 = hsum8(c1 + c2), s2 = hsum8(c3 + 2.0f * c4 + c5);
                        if (cnt < c->SCNT) continue;
                        double mu = s1 / cnt, var = s2 / cnt - mu * mu;
                        if (!(var < 4 * FS_TOL * FS_TOL)) continue;
                        /* exact variance on the full sample */
                        const uint8_t *Vy = c->V + (size_t)yi * c->ns, *Va = c->V + (size_t)a * c->ns, *Vb = c->V + (size_t)b * c->ns;
                        const double *Ly = c->L + (size_t)yi * c->ns, *La = c->L + (size_t)a * c->ns, *Lb = c->L + (size_t)b * c->ns;
                        double e1 = 0, e2 = 0; int ec = 0;
                        for (int i = 0; i < c->ns; i++) if (Vy[i] & Va[i] & Vb[i]) { double d = Ly[i] - La[i] + Lb[i]; e1 += d; e2 += d * d; ec++; }
                        if (ec < 100) continue;
                        double em = e1 / ec, ev = e2 / ec - em * em;
                        if (!(ev < FS_TOL * FS_TOL)) continue;
                        int pos;
                        if (ntv == 12) { if (ev >= tv[11]) continue; pos = 11; } else pos = ntv++;
                        while (pos > 0 && tv[pos - 1] > ev) { tv[pos] = tv[pos - 1]; tva[pos] = tva[pos - 1]; tvb[pos] = tvb[pos - 1]; pos--; }
                        tv[pos] = ev; tva[pos] = a; tvb[pos] = b;
                    }
                }
            }
            for (int q = 0; q < ntv; q++) { ta[nt] = tva[q]; tb[nt] = tvb[q]; tf[nt] = FM_RATIO; nt++; }
            /* lag forms: exact deviation for every source */
            for (int form = FM_DIFF; form <= FM_PCT; form++) {
                const double *PX = form == FM_DIFF ? c->PD : c->PP;
                double sd3[3]; int a3[3], n3 = 0;
                for (int a = 0; a < c->m; a++) {
                    if (a == yi) continue;
                    const double *px = PX + (size_t)a * c->ns, *ly = c->L + (size_t)yi * c->ns; const uint8_t *vy = c->V + (size_t)yi * c->ns;
                    {   /* float screen on the row subsample */
                        const float *lyf = c->Lyr + (size_t)yi * c->SS, *vyf = c->Vf + (size_t)yi * c->SS;
                        const float *lpf = (form == FM_DIFF ? c->LPDf : c->LPPf) + (size_t)a * c->SS, *vpf = (form == FM_DIFF ? c->VPDf : c->VPPf) + (size_t)a * c->SS;
                        float c0[8] = {0}, c1[8] = {0}, c2[8] = {0};
                        for (int i = 0; i < c->SS; i += 8)
                            for (int u = 0; u < 8; u++) { float w = vyf[i + u] * vpf[i + u], d = (lyf[i + u] - lpf[i + u]) * w; c0[u] += w; c1[u] += d; c2[u] += d * d; }
                        double cn = 0, t1 = 0, t2 = 0; for (int u = 0; u < 8; u++) { cn += c0[u]; t1 += c1[u]; t2 += c2[u]; }
                        if (cn < c->SCNT) continue;
                        double mu0 = t1 / cn, v0 = t2 / cn - mu0 * mu0;
                        if (!(v0 < 4 * FS_TOL * FS_TOL)) continue;
                    }
                    double s1 = 0, s2 = 0; int cnt = 0;
                    const double *lp = (form == FM_DIFF ? c->LPD : c->LPP) + (size_t)a * c->ns;
                    for (int i = 0; i < c->ns; i++) { double x = px[i]; if (vy[i] && isfinite(x) && x != 0) { double d = ly[i] - lp[i]; s1 += d; s2 += d * d; cnt++; } }
                    if (cnt < 100) continue;
                    double mu = s1 / cnt, var = s2 / cnt - mu * mu, sd = sqrt(var > 0 ? var : 0);
                    if (!(sd < FS_TOL)) continue;
                    int pos;
                    if (n3 == 3) { if (sd >= sd3[2]) continue; pos = 2; } else pos = n3++;
                    while (pos > 0 && sd3[pos - 1] > sd) { sd3[pos] = sd3[pos - 1]; a3[pos] = a3[pos - 1]; pos--; }
                    sd3[pos] = sd; a3[pos] = a;
                }
                for (int q = 0; q < n3; q++) { ta[nt] = a3[q]; tb[nt] = -1; tf[nt] = form; nt++; }
            }
            /* exactness in units of y's last digit */
            const double *fy = c->F + (size_t)Ty * c->SM; double unit = P10D[c->Smax[Ty]];
            for (int q = 0; q < nt; q++) {
                const double *fa = c->F + (size_t)c->keys[ta[q]] * c->SM, *fb = tb[q] >= 0 ? c->F + (size_t)c->keys[tb[q]] * c->SM : NULL;
                const double *px = tf[q] == FM_DIFF ? c->PD + (size_t)ta[q] * c->ns : tf[q] == FM_PCT ? c->PP + (size_t)ta[q] * c->ns : NULL;
                int cnt = 0;
                for (int i = 0; i < c->ns; i++) {
                    double x = px ? px[i] : fa[i] / fb[i];
                    if (isfinite(x) && isfinite(fy[i]) && x != 0) W->ratio[cnt++] = fy[i] / x;
                }
                if (cnt < 100) continue;
                qsort(W->ratio, cnt, sizeof(double), cmp_dbl);
                double kk = cnt & 1 ? W->ratio[cnt / 2] : (W->ratio[cnt / 2 - 1] + W->ratio[cnt / 2]) / 2;
                int hit = 0, okn = 0;
                for (int i = 0; i < c->ns; i++) {
                    double x = px ? px[i] : fa[i] / fb[i];
                    if (!(isfinite(x) && isfinite(fy[i]) && x != 0)) continue;
                    okn++; hit += fabs(rint(x * kk * unit) - rint(fy[i] * unit)) <= 2;
                }
                double h = (double)hit / okn;
                if (h > 0.5 && isfinite(kk)) {
                    if (W->nc == W->ccap) {
                        if (W->ccap > SIZE_MAX / 2 / sizeof(fcand)) goto done;
                        fcand *grown = realloc(W->cands, sizeof(fcand) * W->ccap * 2);
                        if (!grown) goto done;
                        W->cands = grown; W->ccap *= 2;
                    }
                    W->cands[W->nc++] = (fcand){-h * okn, yi, ta[q], tb[q], tf[q], kk};
                }
            }
        
    }
done:
    zs_trim();
    return NULL;
}

static void fset_discover(const uint8_t *s, size_t n, uint8_t delim, int k, int quotes, int effort, fset_t *fs, int threads) {
    fs->n = 0; fs->gcol = -1;
    const uint8_t *nl = memchr(s, '\n', n);
    if (!nl || k < 2) return;
    size_t body = (size_t)(nl - s) + 1;
    if (n - body < 4096) return;
    const int SM = effort ? FS_SAMPLE : FS_SAMPLE / 2;
    double *F = malloc(sizeof(double) * k * SM), *Fl = malloc(sizeof(double) * k * SM);
    size_t *ro = malloc(sizeof(size_t) * k), *lo = malloc(sizeof(size_t) * k);
    uint32_t *rl = malloc(4 * k), *ll = malloc(4 * k);
    int *nnum = calloc(k, sizeof(int)), *noth = calloc(k, sizeof(int)), *Smax = calloc(k, sizeof(int));
    int g8 = k < 8 ? k : 8; uint8_t *eq = malloc((size_t)g8 * SM); uint8_t *firstdig = calloc(g8, 1);
    int ns = 0; size_t last = (size_t)-1;
    for (int t = 0; t < SM; t++) {
        size_t p = body + (size_t)(((unsigned __int128)(n - body) * (2 * t + 1)) / (2 * SM));
        const uint8_t *q = memchr(s + p, '\n', n - p);
        if (!q) break;
        size_t r0 = (size_t)(q - s) + 1;
        if (r0 >= n || r0 == last) continue;
        size_t l0 = (size_t)(q - s); while (l0 > body && s[l0 - 1] != '\n') l0--;
        last = r0;
        int a = split_row(s, r0, n, delim, k, quotes, ro, rl), b = split_row(s, l0, n, delim, k, quotes, lo, ll);
        for (int j = 0; j < k; j++) {
            int64_t m; int sc;
            double v = NAN, w = NAN;
            if (j < a && rl[j]) { if (parse_dec(s + ro[j], rl[j], &m, &sc)) { v = (double)m / P10D[sc]; nnum[j]++; if (sc > Smax[j]) Smax[j] = sc; } else noth[j]++; }
            if (j < b && ll[j] && parse_dec(s + lo[j], ll[j], &m, &sc)) w = (double)m / P10D[sc];
            F[(size_t)j * SM + ns] = v; Fl[(size_t)j * SM + ns] = w;
        }
        for (int j = 0; j < g8; j++)
            eq[(size_t)j * SM + ns] = j < a && j < b && rl[j] == ll[j] && !memcmp(s + ro[j], s + lo[j], rl[j]);
        if (ns == 0) for (int j = 0; j < g8 && j < a; j++) firstdig[j] = rl[j] && (s[ro[j]] == '-' || (s[ro[j]] >= '0' && s[ro[j]] <= '9'));
        ns++;
    }
    uint8_t *same = malloc(SM + 1);
    int *keys = malloc(sizeof(int) * k), m = 0;
    for (int j = 0; j < k; j++) if (ns >= 200 && nnum[j] * 10 >= ns && noth[j] * 100 <= ns && Smax[j] <= 18) keys[m++] = j;
    if (ns >= 200) {
        for (int j = 0; j < g8 && fs->gcol < 0; j++) {
            if (nnum[j] * 2 >= ns || firstdig[j]) continue;
            int c = 0; for (int i = 0; i < ns; i++) c += eq[(size_t)j * SM + i];
            if (c * 4 >= ns * 3 && c < ns) fs->gcol = j;
        }
    }
    for (int i = 0; i < ns; i++) same[i] = fs->gcol >= 0 ? eq[(size_t)fs->gcol * SM + i] : 1;
    if (m < 2) goto done;
    {
        /* log magnitudes, validity, centred logs, Gram matrix (screen for ratio forms) */
        double *L = malloc(sizeof(double) * m * ns), *Lc = malloc(sizeof(double) * m * ns);
        uint8_t *V = malloc((size_t)m * ns); int *vc = calloc(m, sizeof(int));
        for (int c = 0; c < m; c++) {
            const double *f = F + (size_t)keys[c] * SM; double sum = 0;
            for (int i = 0; i < ns; i++) {
                double x = f[i]; int ok = isfinite(x) && x != 0;
                V[(size_t)c * ns + i] = (uint8_t)ok; L[(size_t)c * ns + i] = ok ? log(fabs(x)) : 0; if (ok) { sum += L[(size_t)c * ns + i]; vc[c]++; }
            }
            double mu = vc[c] ? sum / vc[c] : 0;
            for (int i = 0; i < ns; i++) Lc[(size_t)c * ns + i] = V[(size_t)c * ns + i] ? L[(size_t)c * ns + i] - mu : 0;
        }
        /* float copies of centred logs on a strided row subsample for the ratio screen */
        int SC = effort ? SCREEN : SCREEN / 2, SS = ns < SC ? (ns + 7) / 8 * 8 : SC, SCNT = (int)(100.0 * SS / ns + 0.5);
        float *Lf = calloc((size_t)m * SS, sizeof(float)), *Vf = calloc((size_t)m * SS, sizeof(float)), *L2f = calloc((size_t)m * SS, sizeof(float));
        int NW = (SS + 63) / 64; uint64_t *VB = calloc((size_t)m * NW, 8);
        for (int c = 0; c < m; c++) for (int i = 0; i < SS && i < ns; i++) {
            int src = (int)((int64_t)i * ns / SS);
            if (V[(size_t)c * ns + src]) { float x = (float)Lc[(size_t)c * ns + src]; Vf[(size_t)c * SS + i] = 1; VB[(size_t)c * NW + i / 64] |= 1ull << (i % 64); Lf[(size_t)c * SS + i] = x; L2f[(size_t)c * SS + i] = x * x; }
        }
        /* lag predictors per source: diff and pct */
        double *PD = malloc(sizeof(double) * m * ns), *PP = malloc(sizeof(double) * m * ns), *LPD = malloc(sizeof(double) * m * ns), *LPP = malloc(sizeof(double) * m * ns);
        for (int c = 0; c < m; c++) {
            const double *f = F + (size_t)keys[c] * SM, *fl = Fl + (size_t)keys[c] * SM;
            for (int i = 0; i < ns; i++) {
                double d = same[i] ? f[i] - fl[i] : NAN;
                PD[(size_t)c * ns + i] = d; PP[(size_t)c * ns + i] = d / fl[i] * 100.0;
                LPD[(size_t)c * ns + i] = log(fabs(d)); LPP[(size_t)c * ns + i] = log(fabs(PP[(size_t)c * ns + i]));
            }
        }
        float *LPDf = calloc((size_t)m * SS, sizeof(float)), *LPPf = calloc((size_t)m * SS, sizeof(float));
        float *VPDf = calloc((size_t)m * SS, sizeof(float)), *VPPf = calloc((size_t)m * SS, sizeof(float));
        for (int c = 0; c < m; c++) for (int i = 0; i < SS && i < ns; i++) {
            int src = (int)((int64_t)i * ns / SS);
            double d = PD[(size_t)c * ns + src], q = PP[(size_t)c * ns + src];
            if (isfinite(d) && d != 0) { VPDf[(size_t)c * SS + i] = 1; LPDf[(size_t)c * SS + i] = (float)LPD[(size_t)c * ns + src]; }
            if (isfinite(q) && q != 0) { VPPf[(size_t)c * SS + i] = 1; LPPf[(size_t)c * SS + i] = (float)LPP[(size_t)c * ns + src]; }
        }
        /* lag screen uses y's raw (uncentred) logs */
        float *Lyr = calloc((size_t)m * SS, sizeof(float));
        for (int c = 0; c < m; c++) for (int i = 0; i < SS && i < ns; i++) { int src = (int)((int64_t)i * ns / SS); if (V[(size_t)c * ns + src]) Lyr[(size_t)c * SS + i] = (float)L[(size_t)c * ns + src]; }
        fcand *cands = NULL; size_t nc = 0;
        {   /* formula search over target columns, parallel across disjoint yi ranges (deterministic: see cmp_fcand) */
            fsctx cx = { m, ns, SS, NW, SCNT, SM, keys, vc, Smax, F, L, PD, PP, LPD, LPP, V, Lf, Vf, L2f, Lyr, LPDf, LPPf, VPDf, VPPf, VB };
            int T = threads; if (T < 1) T = 1; if (T > m) T = m; if (T > 16) T = 16;
            fswork wk[16]; pthread_t th[16]; int tok = 1;
            float *WaA = malloc(sizeof(float) * (size_t)SS * T), *DaA = malloc(sizeof(float) * (size_t)SS * T), *D2aA = malloc(sizeof(float) * (size_t)SS * T);
            double *ratA = malloc(sizeof(double) * (size_t)ns * T);
            if (!WaA || !DaA || !D2aA || !ratA) T = 0;                 /* out of memory: fall back below */
            for (int t = 0; t < T; t++) {
                wk[t].c = &cx; wk[t].y0 = (int)((int64_t)t * m / T); wk[t].y1 = (int)((int64_t)(t + 1) * m / T);
                wk[t].Wa = WaA + (size_t)t * SS; wk[t].Da = DaA + (size_t)t * SS; wk[t].D2a = D2aA + (size_t)t * SS;
                wk[t].ratio = ratA + (size_t)t * ns; wk[t].ccap = 64; wk[t].nc = 0; wk[t].cands = malloc(sizeof(fcand) * wk[t].ccap);
                if (!wk[t].cands) { T = t; break; }
            }
            for (int t = 1; t < T; t++) {
                if (pthread_create(&th[t], NULL, fs_worker, &wk[t])) break;
                tok = t + 1;
            }
            if (T > 0) fs_worker(&wk[0]);
            /* Preserve all target ranges when helpers cannot be created. */
            for (int t = tok; t < T; t++) fs_worker(&wk[t]);
            for (int t = 1; t < tok; t++) pthread_join(th[t], NULL);
            size_t tot = 0; for (int t = 0; t < T; t++) tot += wk[t].nc;
            cands = malloc(sizeof(fcand) * (tot ? tot : 1));
            if (cands) for (int t = 0; t < T; t++) { memcpy(cands + nc, wk[t].cands, wk[t].nc * sizeof(fcand)); nc += wk[t].nc; }
            for (int t = 0; t < T; t++) free(wk[t].cands);
            free(WaA); free(DaA); free(D2aA); free(ratA);
            if (!cands) { cands = malloc(sizeof(fcand)); nc = 0; }
        }
        qsort(cands, nc, sizeof(fcand), cmp_fcand);
        uint8_t *derived = calloc(k, 1), *source = calloc(k, 1);
        for (size_t q = 0; q < nc && fs->n < 256; q++) {
            int y = keys[cands[q].y], a = keys[cands[q].a], b = cands[q].b >= 0 ? keys[cands[q].b] : -1;
            if (derived[y] || source[y] || derived[a] || (b >= 0 && derived[b])) continue;
            fs->f[fs->n++] = (tformula){(uint16_t)y, (uint16_t)a, (uint16_t)(b >= 0 ? b : FM_NONE), (uint8_t)cands[q].form, cands[q].k};
            derived[y] = 1; source[a] = 1; if (b >= 0) source[b] = 1;
        }
        free(derived); free(source); free(cands);
        free(L); free(Lc); free(V); free(vc); free(PD); free(PP); free(LPD); free(LPP); free(Lf); free(Vf); free(L2f); free(VB); free(LPDf); free(LPPf); free(VPDf); free(VPPf); free(Lyr);
    }
done:
    free(F); free(Fl); free(ro); free(lo); free(rl); free(ll); free(nnum); free(noth); free(Smax); free(eq); free(firstdig);
    free(same); free(keys);
}

/* ------------------------------------------------ block encoder */
typedef struct { int numeric, S; uint32_t cnt, nnum, nexc, nempty; uint8_t *kind, *sc; int64_t *m, *v; uint32_t *idx; } ncol_t;

static void ncol_parse(const uint8_t *in, size_t n, const uint32_t *off, const uint32_t *len, uint32_t cnt, int keep_mantissa, ncol_t *c) {
    memset(c, 0, sizeof *c); c->cnt = cnt;
    c->kind = malloc(cnt + 1); c->sc = malloc(cnt + 1); c->v = malloc(8 * (cnt + 1)); c->m = keep_mantissa ? malloc(8 * (cnt + 1)) : c->v; c->idx = malloc(4 * (cnt + 1));
    int S = 0;
    for (uint32_t i = 0; i < cnt; i++) {
        int64_t m; int s;
        if (len[i] == 0) { c->kind[i] = 0; c->nempty++; }
        else if (parse_dec_fast(in + off[i], len[i], n - off[i], &m, &s)) { c->kind[i] = 1; c->m[c->nnum] = m; c->sc[c->nnum] = (uint8_t)s; c->idx[c->nnum] = i; c->nnum++; if (s > S) S = s; }
        else { c->kind[i] = 2; c->nexc++; }
    }
    c->S = S;
    int numeric = c->nnum > 0 && c->nexc * 20 <= cnt && c->nnum * 4 >= cnt - c->nempty;
    for (uint32_t i = 0; i < c->nnum && numeric; i++)
        if (__builtin_mul_overflow(c->m[i], P10[S - c->sc[i]], &c->v[i]) || c->v[i] > (1LL << 62) || c->v[i] < -(1LL << 62)) numeric = 0;
    c->numeric = numeric;
}
static void ncol_free(ncol_t *c) { free(c->kind); free(c->sc); if (c->m != c->v) free(c->m); free(c->v); free(c->idx); }

/* per-row float values of a numeric column (NaN where empty or text); rows aligned with fields */
static double *ncol_fvals(const ncol_t *c) {
    double *f = malloc(sizeof(double) * (c->cnt + 1));
    for (uint32_t i = 0; i < c->cnt; i++) f[i] = NAN;
    for (uint32_t i = 0; i < c->nnum; i++) f[c->idx[i]] = (double)c->m[i] / P10D[c->sc[i]];
    return f;
}

static void text_col_put(const uint8_t *in, const uint32_t *off, const uint32_t *len, uint32_t cnt, Buf *out) {
    uint32_t nlong = 0; for (uint32_t i = 0; i < cnt; i++) nlong += len[i] >= 255;
    size_t data = 0; for (uint32_t i = 0; i < cnt; i++) data += len[i];
    uint8_t t = 0; uint32_t size = (uint32_t)(8 + cnt + 4 * nlong + data); bput(out, &t, 1); bput(out, &size, 4);
    bput(out, &cnt, 4); bput(out, &nlong, 4);
    if (out->n + cnt > out->cap) { out->cap = (out->n + cnt) * 2; out->p = realloc(out->p, out->cap); }
    for (uint32_t i = 0; i < cnt; i++) out->p[out->n + i] = len[i] >= 255 ? 255 : (uint8_t)len[i];
    out->n += cnt;
    for (uint32_t i = 0; i < cnt; i++) if (len[i] >= 255) bput(out, &len[i], 4);
    for (uint32_t i = 0; i < cnt; i++) bput(out, in + off[i], len[i]);
}

/* residual stream for a mode; returns its cost estimate in bits */
static uint64_t num_resid(const ncol_t *c, int mode, const uint32_t *gid, const tformula *F, const double *fa, const double *fb,
                          const double *la, uint64_t *z) {
    uint64_t cost = 0; int64_t prev = 0;
    int64_t pd4 = 0; uint32_t pg4 = 0; int hd4 = 0;                 /* mode 4 (grouped delta-of-delta) state */
    int64_t d1_5 = 0, d2_5 = 0; uint32_t pg5 = 0; int nd5 = 0;      /* mode 5 (grouped 3rd-order delta) state */
    for (uint32_t i = 0; i < c->nnum; i++) {
        int64_t v = c->v[i], r;
        switch (mode) {
            case 0: r = v; break;
            case 1: r = (int64_t)((uint64_t)v - (uint64_t)prev); break;
            case 2: r = (i && gid[c->idx[i]] == gid[c->idx[i - 1]]) ? (int64_t)((uint64_t)v - (uint64_t)prev) : v; break;
            case 4: { uint32_t g = gid[c->idx[i]]; int same = i && g == pg4;
                      if (!same) { r = v; hd4 = 0; }
                      else { int64_t d = (int64_t)((uint64_t)v - (uint64_t)prev); r = hd4 ? (int64_t)((uint64_t)d - (uint64_t)pd4) : d; pd4 = d; hd4 = 1; }
                      pg4 = g; break; }
            case 5: { uint32_t g = gid[c->idx[i]]; int same = i && g == pg5;
                      if (!same) { r = v; nd5 = 0; }
                      else { int64_t d = (int64_t)((uint64_t)v - (uint64_t)prev);
                             if (nd5 == 0) { r = d; nd5 = 1; }
                             else { int64_t dd = (int64_t)((uint64_t)d - (uint64_t)d1_5);
                                    r = (nd5 >= 2) ? (int64_t)((uint64_t)dd - (uint64_t)d2_5) : dd; d2_5 = dd; nd5 = 2; }
                             d1_5 = d; }
                      pg5 = g; break; }
            default: { uint32_t row = c->idx[i]; int64_t pi;
                       r = fm_int(F->form, F->k, fa[row], fb ? fb[row] : 0, la ? la[row] : 0, c->S, &pi) ? (int64_t)((uint64_t)v - (uint64_t)pi) : v; }
        }
        prev = v; z[i] = zzg(r); cost += (uint64_t)bitlen(z[i]) + 1;
    }
    return cost;
}

/* costs of residual modes 0 (raw), 1 (delta) and 2 (delta within group) in one pass, as num_resid computes them */
static void num_cost012(const ncol_t *c, const uint32_t *gid, int grouped, uint64_t *c0, uint64_t *c1, uint64_t *c2) {
    uint64_t a0 = 0, a1 = 0, a2 = 0; int64_t prev = 0; uint32_t pg = 0;
    for (uint32_t i = 0; i < c->nnum; i++) {
        int64_t v = c->v[i]; uint64_t d = (uint64_t)v - (uint64_t)prev;
        uint64_t z0 = zzg(v), z1 = zzg((int64_t)d);
        a0 += (uint64_t)bitlen(z0) + 1; a1 += (uint64_t)bitlen(z1) + 1;
        if (grouped) { uint32_t g = gid[c->idx[i]]; a2 += (uint64_t)bitlen((i && g == pg) ? z1 : z0) + 1; pg = g; }
        prev = v;
    }
    *c0 = a0; *c1 = a1; *c2 = a2;
}
static void num_col_put(const uint8_t *in, const uint32_t *off, const uint32_t *len, const ncol_t *c, int mode, int fid, const uint64_t *z, Buf *out) {
    uint64_t mx = 0; for (uint32_t i = 0; i < c->nnum; i++) if (z[i] > mx) mx = z[i];
    int w = 1; while (w < 8 && (mx >> (8 * w))) w++;
    uint32_t elen = 0; if (c->nexc) for (uint32_t i = 0; i < c->cnt; i++) if (c->kind[i] == 2) elen += len[i];
    uint8_t t = 2; uint32_t size = (uint32_t)(12 + 4 + c->cnt + c->nnum + (size_t)c->nnum * w + 4 * c->nexc + elen);
    bput(out, &t, 1); bput(out, &size, 4); bput(out, &c->cnt, 4); bput(out, &c->nnum, 4); bput(out, &c->nexc, 4);
    uint8_t hd[4] = {(uint8_t)c->S, (uint8_t)w, (uint8_t)mode, (uint8_t)fid}; bput(out, hd, 4);
    bput(out, c->kind, c->cnt);
    size_t at = out->n;
    if (out->n + c->nnum + (size_t)c->nnum * w > out->cap) { out->cap = (out->n + c->nnum + (size_t)c->nnum * w) * 2 + 16; out->p = realloc(out->p, out->cap); }
    for (uint32_t i = 0; i < c->nnum; i++) out->p[at + i] = (uint8_t)(c->S - c->sc[i]);
    at += c->nnum;
    for (int b = 0; b < w; b++) for (uint32_t i = 0; i < c->nnum; i++) out->p[at + (size_t)b * c->nnum + i] = (uint8_t)(z[i] >> (8 * b));
    out->n = at + (size_t)c->nnum * w;
    if (!c->nexc) return;
    for (uint32_t i = 0; i < c->cnt; i++) if (c->kind[i] == 2) bput(out, &len[i], 4);
    for (uint32_t i = 0; i < c->cnt; i++) if (c->kind[i] == 2) bput(out, in + off[i], len[i]);
}

/* quote-free tables whose rows all have k fields: terminators found by a SIMD scan, fields written straight to their
 * column-major slots. Same result as the generic splitter; returns 0 (nothing allocated) on any other shape. */
static int split_aligned(const uint8_t *in, int64_t n, uint8_t delim, int k, uint32_t **pcoff, uint32_t **pclen, int64_t *pnr, int64_t *pstride, int *pnoNL) {
    if (n <= 0 || n >= 0xFFFFFFF0ll || k < 2) return 0;
    /* rows: newlines counted by the same SIMD masks (rows of k fields need >= k - 1 delimiters and a newline) */
    size_t nn = 0, i = 0; uint64_t mt, mn;
    for (; i + 64 <= (size_t)n; i += 64) { term_masks64(in + i, delim, &mt, &mn); nn += (size_t)__builtin_popcountll(mn); }
    for (; i < (size_t)n; i++) nn += in[i] == '\n';
    int64_t cap = (int64_t)nn + 1;
    uint32_t *coff = malloc(4 * ((size_t)cap * k + 1)), *clen = malloc(4 * ((size_t)cap * k + 1));
    if (!coff || !clen) { free(coff); free(clen); return 0; }
    int64_t r = 0; int j = 0; uint32_t f0 = 0;
    uint8_t tailbuf[64];
    for (i = 0; i < (size_t)n; i += 64) {
        const uint8_t *p = in + i;
        if (i + 64 > (size_t)n) { memset(tailbuf, 0, 64); memcpy(tailbuf, in + i, (size_t)n - i); p = tailbuf; }
        term_masks64(p, delim, &mt, &mn);
        while (mt) {
            int b = __builtin_ctzll(mt); uint32_t pos = (uint32_t)(i + b); uint64_t bit = mt & -mt; mt ^= bit;
            if (!(mn & bit)) {
                if (j == k - 1) continue;                               /* the last column runs to the end of the line */
                size_t at = (size_t)j * cap + r; coff[at] = f0; clen[at] = pos - f0; j++; f0 = pos + 1;
            } else {
                if (j != k - 1 || r >= cap) goto fail;
                size_t at = (size_t)j * cap + r; coff[at] = f0; clen[at] = pos - f0; r++; j = 0; f0 = pos + 1;
            }
        }
    }
    int noNL = 0;
    if (f0 < (uint64_t)n || j) {                                    /* last line without a newline */
        if (j != k - 1 || r >= cap) goto fail;
        size_t at = (size_t)j * cap + r; coff[at] = f0; clen[at] = (uint32_t)n - f0; r++; noNL = 1;
    }
    if (!r) goto fail;
    *pcoff = coff; *pclen = clen; *pnr = r; *pstride = cap; *pnoNL = noNL; return 1;
fail:
    free(coff); free(clen); return 0;
}

static int64_t tbl2_encode(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, const fset_t *fs, uint8_t *dst, unsigned *newmodes) {
    if (quotes && !has_quotes(in, n)) quotes = 0;
    uint32_t colcnt[256] = {0}; int64_t nf = 0, nr = 0, nexc = 0; int noNL = 0;
    uint32_t *coff = NULL, *clen = NULL, *exc = NULL; int64_t base[256];
    int64_t stride;
    if (!quotes && split_aligned(in, n, delim, k, &coff, &clen, &nr, &stride, &noNL)) {
        exc = malloc(8); nf = nr * k;
        for (int j = 0; j < k; j++) { colcnt[j] = (uint32_t)nr; base[j] = (int64_t)j * stride; }
    } else {
    uint32_t *rec = malloc(4 * (n + 2)); exc = malloc(8 * (n + 2));
    int64_t i = 0;
    while (i < n) {
        int j = 0;
        for (;;) {
            int split = j < k - 1; size_t e;
            if (quotes) e = field_end_q(in, i, n, delim, split);
            else e = i + (split ? scan2(in + i, n - i, delim, '\n') : scan2(in + i, n - i, '\n', '\n'));
            rec[nf++] = (uint32_t)(e - i); colcnt[j]++;
            if (e >= (size_t)n) { noNL = 1; nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } i = n; break; }
            if (in[e] == '\n') { nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } i = e + 1; break; }
            j++; i = e + 1;
            if (i == n) { rec[nf++] = 0; colcnt[j]++; noNL = 1; nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } break; }
        }
    }
    coff = malloc(4 * (nf + 1)); clen = malloc(4 * (nf + 1)); int64_t lpos[256], pos = 0;
    for (int j = 0; j < k; j++) { base[j] = lpos[j] = pos; pos += colcnt[j]; }
    int64_t f = 0, ei = 0, src = 0;
    for (int64_t r = 0; r < nr; r++) {
        int cnt = (ei < nexc && exc[2 * ei] == r) ? (int)exc[2 * ei++ + 1] : k;
        for (int j = 0; j < cnt; j++) { uint32_t L = rec[f++]; coff[lpos[j]] = (uint32_t)src; clen[lpos[j]++] = L; src += L + 1; }
    }
    free(rec);
    }
    int aligned = nexc == 0 && nr > 0;
    int gcol = aligned && fs && fs->gcol >= 0 && fs->gcol < k ? fs->gcol : -1;
    /* entity groups: gid increases where the group column changes */
    uint32_t *gid = malloc(4 * ((size_t)nr + 1));
    { uint32_t g = 0; for (int64_t r = 0; r < nr; r++) {
        if (r && gcol >= 0) { uint32_t a = (uint32_t)(base[gcol] + r), b = a - 1; if (clen[a] != clen[b] || memcmp(in + coff[a], in + coff[b], clen[a])) g++; }
        gid[r] = g; } }
    /* Only formula participants need to coexist. Parse ordinary columns just
     * before emission so their row scratch is reused for the next column. */
    uint8_t needed[256] = {0}, source[256] = {0};
    if (aligned && fs) for (int q = 0; q < fs->n; q++) {
        const tformula *f = &fs->f[q];
        if (f->y >= k || f->a >= k || (f->b != FM_NONE && f->b >= k)) continue;
        needed[f->y] = needed[f->a] = source[f->a] = 1;
        if (f->b != FM_NONE) needed[f->b] = source[f->b] = 1;
    }
    ncol_t C[256] = {0};
    for (int j = 0; j < k; j++) if (needed[j] && j != gcol)
        ncol_parse(in, n, coff + base[j], clen + base[j], colcnt[j], source[j], &C[j]);
    /* formulas usable in this block: target and sources numeric, rows aligned */
    int nform = 0; tformula FB[256]; int fofy[256]; for (int j = 0; j < k; j++) fofy[j] = -1;
    if (aligned && fs) for (int q = 0; q < fs->n; q++) {
        const tformula *F = &fs->f[q];
        if (F->y >= k || F->a >= k || (F->b != FM_NONE && F->b >= k)) continue;
        if (F->y == gcol || F->a == gcol || F->b == gcol || !C[F->y].numeric || !C[F->a].numeric || (F->b != FM_NONE && !C[F->b].numeric)) continue;
        if (F->form == FM_RATIO && F->b == FM_NONE) continue;
        FB[nform] = *F; fofy[F->y] = nform++;
    }
    double **fv = calloc(k, sizeof(double *));
    Buf out = {0}; uint32_t h[4] = {(uint32_t)k, (uint32_t)nr, (uint32_t)nexc, (uint32_t)noNL}; int32_t g32 = gcol; uint32_t nf32 = (uint32_t)nform;
    bput(&out, h, 16); bput(&out, &g32, 4); bput(&out, &nf32, 4);
    for (int q = 0; q < nform; q++) {
        uint8_t e[16]; memset(e, 0, 16); memcpy(e, &FB[q].y, 2); memcpy(e + 2, &FB[q].a, 2); memcpy(e + 4, &FB[q].b, 2); e[6] = FB[q].form; memcpy(e + 8, &FB[q].k, 8);
        bput(&out, e, 16);
    }
    bput(&out, exc, 8 * nexc);
    uint64_t *z = malloc(8 * ((size_t)nr + 2)), *zb = malloc(8 * ((size_t)nr + 2));
    double *la = malloc(sizeof(double) * ((size_t)nr + 1));
    for (int j = 0; j < k; j++) {
        const uint32_t *o = coff + base[j], *l = clen + base[j];
        if (!needed[j] && j != gcol) ncol_parse(in, n, o, l, colcnt[j], 0, &C[j]);
        if (!C[j].numeric) {
            text_col_put(in, o, l, colcnt[j], &out);
            if (!needed[j]) ncol_free(&C[j]);
            continue;
        }
        uint64_t k0, k1, k2, c, *zt; num_cost012(&C[j], gid, gcol >= 0, &k0, &k1, &k2);
        int bm = 1; uint64_t bc = k1;                                   /* same choice as trying 1, 0, 2 in turn */
        if (k0 < bc) { bc = k0; bm = 0; }
        if (gcol >= 0 && k2 < bc) { bc = k2; bm = 2; }
        int zready = 0;
        if (gcol >= 0) {                                             /* grouped delta-of-delta and 3rd-order (only when grouped, like mode 2) */
            uint64_t c4 = num_resid(&C[j], 4, gid, NULL, NULL, NULL, NULL, z);
            if (c4 < bc) { bc = c4; bm = 4; zt = zb; zb = z; z = zt; zready = 1; }
            uint64_t c5 = num_resid(&C[j], 5, gid, NULL, NULL, NULL, NULL, z);
            if (c5 < bc) { bc = c5; bm = 5; zt = zb; zb = z; z = zt; zready = 1; }
        }
        int fid = fofy[j];
        if (fid >= 0) {
            const tformula *F = &FB[fid];
            if (!fv[F->a]) fv[F->a] = ncol_fvals(&C[F->a]);
            if (F->b != FM_NONE && !fv[F->b]) fv[F->b] = ncol_fvals(&C[F->b]);
            const double *fa = fv[F->a];
            if (F->form != FM_RATIO) { la[0] = NAN; for (int64_t r = 1; r < nr; r++) la[r] = gid[r] == gid[r - 1] ? fa[r - 1] : NAN; }
            c = num_resid(&C[j], 3, gid, F, fa, F->b != FM_NONE ? fv[F->b] : NULL, F->form != FM_RATIO ? la : NULL, z);
            if (c < bc) { bc = c; bm = 3; zt = zb; zb = z; z = zt; zready = 1; }
        }
        if (!zready) num_resid(&C[j], bm, gid, NULL, NULL, NULL, NULL, zb);
        if (bm >= 4 && newmodes) *newmodes |= 1u << (bm - 4);
        num_col_put(in, o, l, &C[j], bm, bm == 3 ? fid : 0, zb, &out);
        if (!needed[j]) ncol_free(&C[j]);
    }
    for (int j = 0; j < k; j++) { if (needed[j]) ncol_free(&C[j]); free(fv[j]); }
    free(fv); free(z); free(zb); free(la); free(gid); free(exc); free(coff); free(clen);
    int64_t total = out.n;
    if (total > n + TSLACK - 64) { free(out.p); return -1; }
    memcpy(dst, out.p, total); free(out.p);
    return total;
}

/* ------------------------------------------------ block decoder */
typedef struct { int type; uint32_t cnt, nnum, nexc, nlong; int S, w, mode, fid;
                 const uint8_t *lens, *longs, *data, *dend, *kind, *sd, *pl, *elen, *edata; uint32_t li; int64_t *vals; uint32_t *ridx; } Col2;

/* values of a numeric column at the column scale S, from its residual stream (sd validated by the caller) */
static int num_decode(Col2 *c, const uint32_t *gid, const tformula *F, const double *fa, const double *fb, const double *la, int64_t *vp) {
    uint32_t m = c->nnum; uint64_t *z = (uint64_t *)vp;
    const uint8_t *p0 = c->pl, *p1 = p0 + m, *p2 = p1 + m, *p3 = p2 + m;
    switch (c->w) {                                   /* byte planes -> values, one pass (vectorizes) */
        case 1: for (uint32_t i = 0; i < m; i++) z[i] = p0[i]; break;
        case 2: for (uint32_t i = 0; i < m; i++) z[i] = p0[i] | (uint64_t)p1[i] << 8; break;
        case 3: for (uint32_t i = 0; i < m; i++) z[i] = p0[i] | (uint64_t)p1[i] << 8 | (uint64_t)p2[i] << 16; break;
        case 4: for (uint32_t i = 0; i < m; i++) z[i] = p0[i] | (uint64_t)p1[i] << 8 | (uint64_t)p2[i] << 16 | (uint64_t)p3[i] << 24; break;
        default:
            for (uint32_t i = 0; i < m; i++) z[i] = p0[i] | (uint64_t)p1[i] << 8 | (uint64_t)p2[i] << 16 | (uint64_t)p3[i] << 24;
            for (int b = 4; b < c->w; b++) { const uint8_t *pl = c->pl + (size_t)b * m; for (uint32_t i = 0; i < m; i++) z[i] |= (uint64_t)pl[i] << (8 * b); }
    }
    if (c->mode == 0) { for (uint32_t i = 0; i < m; i++) vp[i] = unzz(z[i]); return 0; }
    if (c->mode == 1) { uint64_t prev = 0; for (uint32_t i = 0; i < m; i++) { prev += (uint64_t)unzz(z[i]); vp[i] = (int64_t)prev; } return 0; }
    if (!m) return 0;
    uint32_t *ri = c->ridx, ni = 0;                            /* rows holding values (counts validated) */
    for (uint32_t t = 0; t < c->cnt; t++) { ri[ni] = t; ni += c->kind[t] == 1; }
    if (c->mode == 2) {
        int64_t prev = 0; uint32_t pg = gid[ri[0]];
        for (uint32_t i = 0; i < m; i++) {
            int64_t r = unzz(z[i]); uint32_t g = gid[ri[i]];
            int64_t v = (i && g == pg) ? (int64_t)((uint64_t)prev + (uint64_t)r) : r;
            prev = v; pg = g; vp[i] = v;
        }
        return 0;
    }
    if (c->mode == 4) {                                            /* grouped delta-of-delta */
        int64_t prev = 0, pd = 0; uint32_t pg = 0; int hd = 0;
        for (uint32_t i = 0; i < m; i++) {
            int64_t r = unzz(z[i]); uint32_t g = gid[ri[i]]; int same = i && g == pg; int64_t v;
            if (!same) { v = r; hd = 0; }
            else { int64_t d = hd ? (int64_t)((uint64_t)r + (uint64_t)pd) : r; v = (int64_t)((uint64_t)prev + (uint64_t)d); pd = d; hd = 1; }
            pg = g; prev = v; vp[i] = v;
        }
        return 0;
    }
    if (c->mode == 5) {                                            /* grouped 3rd-order delta */
        int64_t prev = 0, d1 = 0, d2 = 0; uint32_t pg = 0; int nd = 0;
        for (uint32_t i = 0; i < m; i++) {
            int64_t r = unzz(z[i]); uint32_t g = gid[ri[i]]; int same = i && g == pg; int64_t v;
            if (!same) { v = r; nd = 0; }
            else { int64_t d;
                   if (nd == 0) { d = r; nd = 1; }
                   else { int64_t dd = (nd >= 2) ? (int64_t)((uint64_t)r + (uint64_t)d2) : r; d = (int64_t)((uint64_t)dd + (uint64_t)d1); d2 = dd; nd = 2; }
                   v = (int64_t)((uint64_t)prev + (uint64_t)d); d1 = d; }
            pg = g; prev = v; vp[i] = v;
        }
        return 0;
    }
    for (uint32_t i = 0; i < m; i++) {
        int64_t r = unzz(z[i]), pi; uint32_t t = ri[i];
        vp[i] = fm_int(F->form, F->k, fa[t], fb ? fb[t] : 0, la ? la[t] : 0, c->S, &pi) ? (int64_t)((uint64_t)r + (uint64_t)pi) : r;
    }
    return 0;
}
static double *col2_fvals(const Col2 *c, uint32_t nr) {
    double *f = malloc(sizeof(double) * (nr + 1)); uint32_t ni = 0;
    for (uint32_t i = 0; i < nr; i++) {
        if (c->kind[i] == 1 && ni < c->nnum) { int ds = c->sd[ni]; int64_t mm = ds ? exact_div_p10(c->vals[ni], ds) : c->vals[ni]; f[i] = (double)mm / P10D[c->S - ds]; ni++; }
        else f[i] = NAN;
    }
    return f;
}


#ifdef CZN_PROF
#include <x86intrin.h>
static uint64_t sp_acc[8], sp_last;
#define SP(i) do { uint64_t t_ = __rdtsc(); sp_acc[i] += t_ - sp_last; sp_last = t_; } while (0)
__attribute__((destructor)) static void sp_dump(void) { fprintf(stderr, "vals %.1f vals3 %.1f srcdbl %.1f text %.1f fmt %.1f rows %.1f Mcyc\n",
    sp_acc[0] / 1e6, sp_acc[1] / 1e6, sp_acc[2] / 1e6, sp_acc[3] / 1e6, sp_acc[4] / 1e6, sp_acc[5] / 1e6); }
#else
#define SP(i) ((void)0)
#endif
/* bit j: field j of the row is not empty (n <= 64 lengths) */
static inline uint64_t row_nonempty(const uint16_t *rl, int n) {
    uint64_t m = 0; int j = 0;
#if defined(__SSE2__)
    const __m128i z = _mm_setzero_si128();
    for (; j + 16 <= n; j += 16) {
        __m128i a = _mm_loadu_si128((const __m128i *)(rl + j)), b = _mm_loadu_si128((const __m128i *)(rl + j + 8));
        __m128i e = _mm_packs_epi16(_mm_cmpeq_epi16(a, z), _mm_cmpeq_epi16(b, z));
        m |= (uint64_t)(uint16_t)~_mm_movemask_epi8(e) << j;
    }
#elif defined(__ARM_NEON) && defined(__aarch64__)
    for (; j + 16 <= n; j += 16) {
        uint8x16_t e = vcombine_u8(vmovn_u16(vtstq_u16(vld1q_u16(rl + j), vld1q_u16(rl + j))), vmovn_u16(vtstq_u16(vld1q_u16(rl + j + 8), vld1q_u16(rl + j + 8))));
        uint64_t m4 = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(e), 4)), 0);
        uint64_t mm = 0; while (m4) { int b = __builtin_ctzll(m4); mm |= 1ull << (b >> 2); m4 &= ~(0xFull << (b & ~3)); }
        m |= mm << j;
    }
#endif
    for (; j < n; j++) m |= (uint64_t)(rl[j] != 0) << j;
    return m;
}
/* indices t < R with kd[t] == 1, in order (SIMD masks; R <= 255) */
static inline int compact_rows(const uint8_t *kd, int R, uint8_t *rows) {
    int q = 0, t = 0;
#if defined(__SSE2__)
    const __m128i one = _mm_set1_epi8(1);
    for (; t + 16 <= R; t += 16) {
        uint32_t m = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(kd + t)), one));
        while (m) { rows[q++] = (uint8_t)(t + __builtin_ctz(m)); m &= m - 1; }
    }
#elif defined(__ARM_NEON) && defined(__aarch64__)
    for (; t + 16 <= R; t += 16) {
        uint8x16_t e = vceqq_u8(vld1q_u8(kd + t), vdupq_n_u8(1));
        uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(e), 4)), 0);
        while (m) { int b = __builtin_ctzll(m); rows[q++] = (uint8_t)(t + (b >> 2)); m &= ~(0xFull << (b & ~3)); }
    }
#endif
    for (; t < R; t++) { rows[q] = (uint8_t)t; q += kd[t] == 1; }
    return q;
}
/* Aligned tables (every row has k fields), decoded in chunks of TR rows so that all working data stays in cache:
 * per chunk, numeric values are rebuilt from their streams (plain columns first, then formula targets from their
 * sources' values in the same rows), formatted per column into an arena, and the rows are assembled from the arena
 * and the text columns. Returns 0, or -1 for corrupt input. Same output as the column-at-a-time path. */
enum { TR = 128 };
typedef struct {
    int type, mode, S, w, src, form, fa, fb, q, hasx; double k; uint32_t nnum, ni, xi, li; int64_t prev; uint32_t pg;
    int64_t pd; int hd;                             /* mode 4 (grouped delta-of-delta) state, carried across chunks */
    int64_t d1, d2; int nd;                         /* mode 5 (grouped 3rd-order delta) state, carried across chunks */
    uint8_t *rows;                                  /* chunk rows holding values (q of them) */
    const uint8_t *kind, *sd, *pl, *elen, *x, *lens, *longs, *p, *pe;
    int64_t *cv; double *fv; double fprev;          /* chunk values; source columns: row doubles, fv[-1] = previous row */
} scol_t;
static int tbl2_stream(Col2 *C, int k, int64_t nr, const uint32_t *gid, const tformula *FB, uint32_t nform, uint8_t delim, int noNL,
                       uint8_t *out, int64_t n) {
    int ret = -1;
    scol_t *U = calloc(k, sizeof(scol_t));
    size_t xbytes = 0; for (int j = 0; j < k; j++) if (C[j].type == 2) xbytes += (size_t)(C[j].dend - C[j].edata);
    size_t acap = (size_t)TR * FMT_STRIDE * k + xbytes + 32 * (size_t)k + 128;
    int64_t *cvb = malloc(sizeof(int64_t) * ((size_t)TR + 4) * k);
    double *fvb = malloc(sizeof(double) * ((size_t)TR + 2) * k);
    uint16_t *RL = malloc(2 * (size_t)TR * k); const uint8_t **SPP = malloc(sizeof(uint8_t *) * (size_t)TR * k); uint8_t *arena = malloc(acap), *lv = malloc(TR + 8);
    if (SPP && arena) for (size_t i = 0; i < (size_t)TR * k; i++) SPP[i] = arena;
    uint8_t *rowsb = malloc((size_t)(TR + 8) * k);
    if (!U || !cvb || !fvb || !RL || !SPP || !arena || !lv || !rowsb) goto done;
    for (int j = 0; j < k; j++) {
        Col2 *c = &C[j]; scol_t *u = &U[j]; u->type = c->type; u->fa = u->fb = -1;
        u->cv = cvb + (size_t)j * (TR + 4); u->fv = fvb + (size_t)j * (TR + 2) + 1; u->fprev = NAN; u->rows = rowsb + (size_t)j * (TR + 8);
        if (c->type == 0) { u->lens = c->lens; u->longs = c->longs; u->p = c->data; u->pe = c->dend; continue; }
        u->mode = c->mode; u->S = c->S; u->w = c->w; u->nnum = c->nnum; u->kind = c->kind; u->sd = c->sd; u->pl = c->pl;
        u->elen = c->elen; u->x = c->edata; u->pe = c->dend; u->hasx = c->nexc > 0;
        if (c->mode == 3) {
            if ((uint32_t)c->fid >= nform) goto done;
            const tformula *F = &FB[c->fid];
            if (F->y != j) goto done;
            const Col2 *A = &C[F->a], *B = F->form == FM_RATIO ? &C[F->b] : NULL;
            if (A->type != 2 || A->mode == 3 || (B && (B->type != 2 || B->mode == 3))) goto done;
            u->form = F->form; u->k = F->k; u->fa = F->a; u->fb = B ? F->b : -1;
        }
    }
    for (int j = 0; j < k; j++) if (U[j].fa >= 0) { U[U[j].fa].src = 1; if (U[j].fb >= 0) U[U[j].fb].src = 1; }
    uint8_t *o = out, *oe = out + n;
    for (int64_t r0 = 0; r0 < nr; r0 += TR) {
        int R = (int)(nr - r0 < TR ? nr - r0 : TR);
#ifdef CZN_PROF
        sp_last = __rdtsc();
#endif
        /* values of plain numeric columns, then of formula targets */
        for (int pass = 0; pass < 2; pass++)
            for (int j = 0; j < k; j++) {
                scol_t *u = &U[j]; if (u->type != 2 || (u->mode == 3) != pass) continue;
                const uint8_t *kd = u->kind + r0; uint8_t *rows = u->rows;
                int q = compact_rows(kd, R, rows);
                u->q = q;
                if (u->ni + (uint32_t)q > u->nnum) goto done;
                const uint32_t m = u->nnum, i0 = u->ni; const uint8_t *p0 = u->pl + i0, *p1 = p0 + m, *p2 = p1 + m, *p3 = p2 + m;
                int64_t *cv = u->cv;
                switch (u->w) {                                     /* byte planes -> zigzag residuals */
                    case 1: for (int a = 0; a < q; a++) cv[a] = unzz(p0[a]); break;
                    case 2: for (int a = 0; a < q; a++) cv[a] = unzz(p0[a] | (uint64_t)p1[a] << 8); break;
                    case 3: for (int a = 0; a < q; a++) cv[a] = unzz(p0[a] | (uint64_t)p1[a] << 8 | (uint64_t)p2[a] << 16); break;
                    case 4: for (int a = 0; a < q; a++) cv[a] = unzz(p0[a] | (uint64_t)p1[a] << 8 | (uint64_t)p2[a] << 16 | (uint64_t)p3[a] << 24); break;
                    default:
                        for (int a = 0; a < q; a++) {
                            uint64_t z = p0[a];
                            for (int b = 1; b < u->w; b++) z |= (uint64_t)p0[(size_t)b * m + a] << (8 * b);
                            cv[a] = unzz(z);
                        }
                }
                if (u->mode == 1) { int64_t pv = u->prev; for (int a = 0; a < q; a++) { pv = (int64_t)((uint64_t)pv + (uint64_t)cv[a]); cv[a] = pv; } u->prev = pv; }
                else if (u->mode == 2) {
                    int64_t pv = u->prev; uint32_t pg = u->pg;
                    for (int a = 0; a < q; a++) {
                        uint32_t g = gid[r0 + rows[a]];
                        int64_t v = (i0 + a > 0 && g == pg) ? (int64_t)((uint64_t)pv + (uint64_t)cv[a]) : cv[a];
                        cv[a] = v; pv = v; pg = g;
                    }
                    u->prev = pv; u->pg = pg;
                } else if (u->mode == 4) {
                    int64_t pv = u->prev, pd = u->pd; uint32_t pg = u->pg; int hd = u->hd;
                    for (int a = 0; a < q; a++) {
                        uint32_t g = gid[r0 + rows[a]]; int same = (i0 + a > 0) && g == pg; int64_t v;
                        if (!same) { v = cv[a]; hd = 0; }
                        else { int64_t d = hd ? (int64_t)((uint64_t)cv[a] + (uint64_t)pd) : cv[a]; v = (int64_t)((uint64_t)pv + (uint64_t)d); pd = d; hd = 1; }
                        cv[a] = v; pv = v; pg = g;
                    }
                    u->prev = pv; u->pd = pd; u->pg = pg; u->hd = hd;
                } else if (u->mode == 5) {
                    int64_t pv = u->prev, d1 = u->d1, d2 = u->d2; uint32_t pg = u->pg; int nd = u->nd;
                    for (int a = 0; a < q; a++) {
                        uint32_t g = gid[r0 + rows[a]]; int same = (i0 + a > 0) && g == pg; int64_t v;
                        if (!same) { v = cv[a]; nd = 0; }
                        else { int64_t d;
                               if (nd == 0) { d = cv[a]; nd = 1; }
                               else { int64_t dd = (nd >= 2) ? (int64_t)((uint64_t)cv[a] + (uint64_t)d2) : cv[a]; d = (int64_t)((uint64_t)dd + (uint64_t)d1); d2 = dd; nd = 2; }
                               v = (int64_t)((uint64_t)pv + (uint64_t)d); d1 = d; }
                        cv[a] = v; pv = v; pg = g;
                    }
                    u->prev = pv; u->d1 = d1; u->d2 = d2; u->pg = pg; u->nd = nd;
                } else if (u->mode == 3) {
                    const double *fa = U[u->fa].fv, *fb = u->fb >= 0 ? U[u->fb].fv : NULL;
                    for (int a = 0; a < q; a++) {
                        int t = (int)rows[a]; int64_t row = r0 + t, pi;
                        double la = 0;
                        if (u->form != FM_RATIO) la = (row > 0 && gid[row] == gid[row - 1]) ? fa[t - 1] : NAN;
                        if (fm_int(u->form, u->k, fa[t], fb ? fb[t] : 0, la, u->S, &pi)) cv[a] = (int64_t)((uint64_t)cv[a] + (uint64_t)pi);
                    }
                }
                SP(u->mode == 3 ? 1 : 0);
                if (u->src) {                                      /* row doubles for formula targets (NaN: no value) */
                    double *fv = u->fv; fv[-1] = u->fprev;
                    for (int t = 0; t < R; t++) fv[t] = NAN;
                    const uint8_t *sd = u->sd + i0;
                    for (int a = 0; a < q; a++) { int ds = sd[a]; int64_t mm = ds ? exact_div_p10(cv[a], ds) : cv[a]; fv[rows[a]] = (double)mm / P10D[u->S - ds]; }
                    u->fprev = fv[R - 1];
                    SP(2);
                }
            }
        /* field bytes: numeric columns formatted into fixed arena slots (packed when the chunk holds text), text columns
         * read in place. Per row and field (row-major): length and source. */
        uint8_t *ap = arena; size_t ttot = 0; int safe = 1;
        memset(RL, 0, 2 * (size_t)R * k);                               /* empty fields: length 0, any readable source */
        for (int j = 0; j < k; j++) {
            scol_t *u = &U[j]; uint16_t *rl = RL + j; const uint8_t **sp = SPP + j;
            if (u->type == 0) {
                const uint8_t *l8 = u->lens + r0, *p = u->p;
                for (int t = 0; t < R; t++) {
                    uint32_t L = l8[t];
                    if (L == 255) { L = rd32(u->longs + 4ull * u->li++); if (L > 65535) goto done; }
                    rl[(size_t)t * k] = (uint16_t)L; sp[(size_t)t * k] = p; p += L;
                }
                ttot += (size_t)(p - u->p); u->p = p;
                if (u->pe - p < 16) safe = 0;                           /* 16-byte reads past this chunk's text */
                SP(3);
                continue;
            }
            const uint8_t *kd = u->kind + r0; int n1 = u->q;
            if (!u->hasx || !memchr(kd, 2, (size_t)R)) {               /* no text in this chunk of the column */
                fmt_slots(ap, u->cv, u->sd + u->ni, n1, u->S, lv);
                const uint8_t *rw = u->rows;
                for (int a = 0; a < n1; a++) { size_t at = (size_t)rw[a] * k; rl[at] = lv[a]; sp[at] = ap + (size_t)FMT_STRIDE * a; }
                ap += (size_t)FMT_STRIDE * n1;
            } else {
                int q = 0;
                for (int t = 0; t < R; t++) {
                    uint32_t L = 0; sp[(size_t)t * k] = ap;
                    if (kd[t] == 1) { int ds = u->sd[u->ni + q]; uint8_t *e2 = fmt_dec_swar(ap, u->cv[q], u->S) - ds - (ds == u->S && ds); L = (uint32_t)(e2 - ap); ap = e2; q++; }
                    else if (kd[t] == 2) { L = rd32(u->elen + 4ull * u->xi++); if (L > 65535) goto done; memcpy(ap, u->x, L); u->x += L; ap += L; }
                    rl[(size_t)t * k] = (uint16_t)L;
                }
                ap += 32;                                               /* keep 16-byte reads inside the arena */
            }
            u->ni += (uint32_t)n1;
            SP(4);
        }
        /* rows: one bound check per chunk when it fits; every field copied with one 16-byte move when short */
        int roomy = o + ttot + (size_t)(ap - arena) + (size_t)R * (k + 16) + 64 <= oe;
        for (int t = 0; t < R; t++) {
            int64_t rr = r0 + t; int last = noNL && rr == nr - 1;
            const uint16_t *rl = RL + (size_t)t * k; const uint8_t *const *sp = SPP + (size_t)t * k;
            if (roomy && safe) {
                /* non-empty fields from a SIMD mask of the row's lengths; runs of empty fields are just delimiters */
                uint8_t dl[32]; memset(dl, delim, 32);
                int pos = 0;
                for (int j0 = 0; j0 < k; j0 += 64) {
                    uint64_t m = row_nonempty(rl + j0, k - j0 < 64 ? k - j0 : 64);
                    while (m) {
                        int j = j0 + __builtin_ctzll(m); m &= m - 1;
                        int gap = j - pos;                                  /* empty fields before j, then j's delimiter */
                        if (gap <= 32) memcpy(o, dl, 32); else memset(o, delim, (size_t)gap);
                        o += gap;
                        uint32_t len = rl[j];
                        if (__builtin_expect(len <= 16, 1)) memcpy(o, sp[j], 16); else memcpy(o, sp[j], len);
                        o += len; *o++ = delim; pos = j + 1;
                    }
                }
                if (k - pos <= 32) memcpy(o, dl, 32); else memset(o, delim, (size_t)(k - pos));
                o += k - pos;
                if (!last) o[-1] = '\n'; else o--;
            } else {                                                   /* near the end: exact copies */
                for (int j = 0; j < k; j++) {
                    uint32_t len = rl[j];
                    if (len > (size_t)(oe - o)) goto done;
                    memcpy(o, sp[j], len); o += len;
                    if (j < k - 1) { if (o >= oe) goto done; *o++ = delim; }
                }
                if (!last) { if (o >= oe) goto done; *o++ = '\n'; }
            }
        }
        SP(5);
    }
    if (o != oe) goto done;
    for (int j = 0; j < k; j++) if (U[j].type == 2 && U[j].ni != U[j].nnum) goto done;
    ret = 0;
done:
    free(U); free(cvb); free(fvb); free(RL); free(SPP); free(arena); free(lv); free(rowsb);
    return ret;
}

#ifdef CZN_PROF
#include <x86intrin.h>
static uint64_t prof_acc[8], prof_last, prof_mode[4], prof_cnt[4]; static int prof_ph;
#define PROF_T(ph) do { uint64_t t_ = __rdtsc(); prof_acc[prof_ph] += t_ - prof_last; prof_last = t_; prof_ph = (ph); } while (0)
__attribute__((destructor)) static void prof_dump(void) { const char *nm[8] = {"hdr", "valid", "gid", "numdec", "colpass", "bitmap", "rowpass", "-"};
    for (int i = 0; i < 7; i++) fprintf(stderr, "%-8s %8.1f Mcyc\n", nm[i], prof_acc[i] / 1e6);
    for (int i = 0; i < 4; i++) fprintf(stderr, "mode %d: %8.1f Mcyc %9llu values %.1f cyc/value\n", i, prof_mode[i] / 1e6, (unsigned long long)prof_cnt[i], prof_cnt[i] ? (double)prof_mode[i] / prof_cnt[i] : 0); }
#else
#define PROF_T(ph) ((void)0)
#endif
static int tbl2_decode(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n) {
#ifdef CZN_PROF
    prof_last = __rdtsc(); prof_ph = 0;
#endif
    if (tn < 24) return -1;
    uint32_t h[4]; memcpy(h, in, 16); int k = h[0]; int64_t nr = h[1], nexc = h[2], noNL = h[3];
    int32_t gcol; uint32_t nform; memcpy(&gcol, in + 16, 4); memcpy(&nform, in + 20, 4);
    if (k < 1 || k > 256 || nform > 256 || gcol >= k || gcol < -1) return -1;
    if (24 + 16ull * nform + 8ull * nexc > (uint64_t)tn) return -1;
    if ((uint64_t)nr > (uint64_t)n + 1) return -1;
    tformula FB[256];
    for (uint32_t q = 0; q < nform; q++) {
        const uint8_t *e = in + 24 + 16 * q;
        memcpy(&FB[q].y, e, 2); memcpy(&FB[q].a, e + 2, 2); memcpy(&FB[q].b, e + 4, 2); FB[q].form = e[6]; memcpy(&FB[q].k, e + 8, 8);
        if (FB[q].form < FM_RATIO || FB[q].form > FM_PCT || FB[q].y >= k || FB[q].a >= k || (FB[q].form == FM_RATIO && FB[q].b >= k)) return -1;
    }
    const uint8_t *exc = in + 24 + 16 * nform, *p = exc + 8 * nexc, *end = in + tn;
    Col2 *C = calloc(k, sizeof(Col2)); if (!C) return -1;
    int64_t *vals = NULL; uint32_t *gid = NULL, *ridx = NULL; double **fv = NULL; double *la = NULL;
    uint64_t *bm = NULL; uint16_t *RL = NULL; uint8_t *arena = NULL, *lv = NULL;
    int ret = -1;
    for (int j = 0; j < k; j++) {
        if (p + 5 > end) goto out;
        Col2 *c = &C[j]; c->type = p[0]; uint32_t size; memcpy(&size, p + 1, 4); p += 5;
        if (c->type != 0 && c->type != 2) goto out;
        const uint8_t *q = p, *qe = p + size; if (size > (size_t)(end - p)) goto out;
        if (c->type == 0) {
            if (size < 8) goto out;
            memcpy(&c->cnt, q, 4); memcpy(&c->nlong, q + 4, 4); q += 8;
            if (c->cnt > size || c->nlong > size) goto out;
            c->lens = q; q += c->cnt; c->longs = q; q += 4ull * c->nlong; c->data = q; c->dend = qe;
            if (q > qe) goto out;
        } else {
            if (size < 16) goto out;
            memcpy(&c->cnt, q, 4); memcpy(&c->nnum, q + 4, 4); memcpy(&c->nexc, q + 8, 4);
            c->S = q[12]; c->w = q[13]; c->mode = q[14]; c->fid = q[15]; q += 16;
            if (c->S > 18 || c->w < 1 || c->w > 8 || c->mode > 5 || c->cnt > size || c->nnum > c->cnt || c->nexc > c->cnt) goto out;
            c->kind = q; q += c->cnt; c->sd = q; q += c->nnum; c->pl = q; q += (size_t)c->nnum * c->w;
            c->elen = q; q += 4ull * c->nexc; c->edata = q; c->dend = qe;
            if (q > qe) goto out;
        }
        p = qe;
    }
    PROF_T(0);
    /* validation pre-pass: field counts per column, kinds, text and exception sizes; the emit loop needs no checks
     * except the output bound */
    {
        uint32_t want[256]; for (int j = 0; j < k; j++) want[j] = (uint32_t)nr;
        for (int64_t e = 0; e < nexc; e++) {
            uint32_t er, cc; memcpy(&er, exc + 8 * e, 4); memcpy(&cc, exc + 8 * e + 4, 4);
            if (cc < 1 || cc > (uint32_t)k || er >= (uint64_t)nr || (e && er <= rd32(exc + 8 * (e - 1)))) goto out;
            for (uint32_t j = cc; j < (uint32_t)k; j++) want[j]--;
        }
        for (int j = 0; j < k; j++) {
            Col2 *c = &C[j]; if (c->cnt != want[j]) goto out;
            if (c->type == 0) {
                uint64_t tot = 0; uint32_t nl = 0;
                for (uint32_t i = 0; i < c->cnt; i++) { uint32_t L8 = c->lens[i]; nl += L8 == 255; tot += L8; }
                if (nl != c->nlong) goto out;
                tot -= 255ull * nl;
                for (uint32_t i = 0; i < nl; i++) tot += rd32(c->longs + 4ull * i);
                if (tot > (uint64_t)(c->dend - c->data)) goto out;
            } else {
                uint32_t n1 = 0, n2 = 0, bad = 0;
                for (uint32_t i = 0; i < c->cnt; i++) { uint8_t kd = c->kind[i]; n1 += kd == 1; n2 += kd == 2; bad |= kd > 2; }
                if (bad || n1 != c->nnum || n2 != c->nexc) goto out;
                uint8_t mx = 0; for (uint32_t i = 0; i < c->nnum; i++) mx = c->sd[i] > mx ? c->sd[i] : mx;
                if (mx > c->S) goto out;
                uint64_t tot = 0; for (uint32_t i = 0; i < c->nexc; i++) tot += rd32(c->elen + 4ull * i);
                if (tot > (uint64_t)(c->dend - c->edata)) goto out;
            }
        }
    }
    PROF_T(1);
    int aligned = nexc == 0;
    for (int j = 0; j < k; j++) if (C[j].cnt != (uint32_t)nr) aligned = 0;
    /* group ids from the group column (text) */
    gid = malloc(4 * ((size_t)nr + 1)); if (!gid) goto out;
    if (gcol >= 0) {
        Col2 *g = &C[gcol]; if (g->type != 0 || !aligned) goto out;
        const uint8_t *d = g->data, *pd = NULL; uint32_t pl = 0, li = 0, gg = 0;
        for (int64_t r = 0; r < nr; r++) {
            uint32_t len = g->lens[r];
            if (len == 255) { if (li >= g->nlong) goto out; memcpy(&len, g->longs + 4ull * li++, 4); }
            if (len > (size_t)(g->dend - d)) goto out;
            if (r && (len != pl || memcmp(d, pd, len))) gg++;
            gid[r] = gg; pd = d; pl = len; d += len;
        }
    } else for (int64_t r = 0; r < nr; r++) gid[r] = 0;
    PROF_T(2);
    if (aligned && nr > 0 && !getenv("CZN_OLDTBL")) { ret = tbl2_stream(C, k, nr, gid, FB, nform, delim, (int)noNL, out, n); goto out; }
    /* numeric columns: non-formula first, then formula targets (sources are never targets) */
    size_t totnum = 0; for (int j = 0; j < k; j++) if (C[j].type == 2) totnum += C[j].nnum;
    vals = malloc(8 * (totnum + 1)); fv = calloc(k, sizeof(double *)); la = malloc(sizeof(double) * ((size_t)nr + 1));
    ridx = malloc(4 * ((size_t)nr + 2));
    if (!vals || !fv || !la || !ridx) goto out;
    for (int j = 0; j < k; j++) C[j].ridx = ridx;
    { int64_t *vp = vals;
      for (int j = 0; j < k; j++) if (C[j].type == 2) { C[j].vals = vp; vp += C[j].nnum; } }
    for (int pass = 0; pass < 2; pass++)
        for (int j = 0; j < k; j++) {
            Col2 *c = &C[j]; if (c->type != 2 || (c->mode == 3) != pass) continue;
            if ((c->mode == 2 || c->mode == 3 || c->mode == 4 || c->mode == 5) && !aligned) goto out;
#ifdef CZN_PROF
            uint64_t tq0 = __rdtsc();
#endif
            if (c->mode != 3) { if (num_decode(c, gid, NULL, NULL, NULL, NULL, c->vals)) goto out;
#ifdef CZN_PROF
                prof_mode[c->mode] += __rdtsc() - tq0; prof_cnt[c->mode] += c->nnum;
#endif
                continue; }
            if ((uint32_t)c->fid >= nform) goto out;
            const tformula *F = &FB[c->fid];
            if (F->y != j) goto out;
            const Col2 *A = &C[F->a], *B = F->form == FM_RATIO ? &C[F->b] : NULL;
            if (A->type != 2 || A->mode == 3 || (B && (B->type != 2 || B->mode == 3))) goto out;
            if (!fv[F->a]) fv[F->a] = col2_fvals(A, (uint32_t)nr);
            if (B && !fv[F->b]) fv[F->b] = col2_fvals(B, (uint32_t)nr);
            const double *fa = fv[F->a];
            if (F->form != FM_RATIO) { la[0] = NAN; for (int64_t r = 1; r < nr; r++) la[r] = gid[r] == gid[r - 1] ? fa[r - 1] : NAN; }
            if (num_decode(c, gid, F, fa, B ? fv[F->b] : NULL, F->form != FM_RATIO ? la : NULL, c->vals)) goto out;
#ifdef CZN_PROF
            prof_mode[3] += __rdtsc() - tq0; prof_cnt[3] += c->nnum;
#endif
        }
    PROF_T(3);
    /* single row-major pass: fields written straight to the output, numbers formatted in place */
    {
        typedef struct { const uint8_t *lens, *longs, *p, *pe, *kind, *sd, *elen; const int64_t *v; uint32_t i, li, ni, xi; int type, S; } cur_t;
        cur_t cu[256];
        for (int j = 0; j < k; j++) {
            Col2 *c = &C[j]; cur_t *u = &cu[j]; memset(u, 0, sizeof *u); u->type = c->type;
            if (c->type == 0) { u->lens = c->lens; u->longs = c->longs; u->p = c->data; u->pe = c->dend; }
            else { u->kind = c->kind; u->sd = c->sd; u->v = c->vals; u->S = c->S; u->elen = c->elen; u->p = c->edata; u->pe = c->dend; }
        }
        uint8_t *o = out, *oe = out + n; int64_t e = 0, r = 0;
        if (aligned && nr > 0) {
            /* chunks of RC rows. Column pass: per-row field lengths (u16) and the field bytes of numeric columns
             * formatted in sequence (no per-field branch). Row pass: a bitmap of non-empty fields drives a uniform
             * copy; runs of empty fields become one store of delimiters. */
            enum { RC = 256 };
            int W = (k + 63) / 64;
            size_t xbytes = 0;                   /* exception fields (text in numeric columns) are copied into the arena too */
            for (int j = 0; j < k; j++) if (C[j].type == 2) xbytes += (size_t)(C[j].dend - C[j].edata);
            size_t acap = (size_t)RC * 24 * k + xbytes + 64;
            bm = malloc(8 * (size_t)RC * W); RL = malloc(2 * (size_t)RC * k); arena = malloc(acap); lv = malloc(RC);
            uint32_t rowtot[RC]; int dense_rows = getenv("CZN_DENSE") ? atoi(getenv("CZN_DENSE")) : 1;
            const uint8_t *src[256], *se[256];
            uint8_t dl[32]; memset(dl, delim, 32);
            int okp = bm && RL && arena && lv;
            for (int64_t r0 = 0; okp && r0 < nr; r0 += RC) {
                int64_t r1 = r0 + RC < nr ? r0 + RC : nr; int R = (int)(r1 - r0);
                uint8_t *ap = arena;
                for (int j = 0; j < k && okp; j++) {
                    cur_t *u = &cu[j]; uint16_t *rl = RL + (size_t)j * RC;
                    if (u->type == 0) {
                        const uint8_t *l8 = u->lens + r0; int esc = 0;
                        for (int t = 0; t < R; t++) { rl[t] = l8[t]; esc |= l8[t] == 255; }
                        if (esc) for (int t = 0; t < R; t++) if (l8[t] == 255) { uint32_t L = rd32(u->longs + 4ull * u->li++); if (L > 65535) { okp = 0; break; } rl[t] = (uint16_t)L; }
                        src[j] = u->p; se[j] = u->pe; size_t tot = 0; for (int t = 0; t < R; t++) tot += rl[t]; u->p += tot;
                    } else {
                        const uint8_t *kd = u->kind + r0; int n1 = 0, n2 = 0;
                        for (int t = 0; t < R; t++) { n1 += kd[t] == 1; n2 += kd[t] == 2; }
                        src[j] = ap; se[j] = arena + acap;
                        if (!n2) {
                            const int64_t *v = u->v + u->ni; const uint8_t *sd = u->sd + u->ni; int S = u->S;
                            ap = fmt_run(ap, v, sd, n1, S, lv);
                            u->ni += n1;
                            int q = 0; for (int t = 0; t < R; t++) { int is = kd[t] == 1; rl[t] = is ? lv[q] : 0; q += is; }
                        } else {
                            for (int t = 0; t < R; t++) {
                                if (kd[t] == 1) { uint32_t ni = u->ni++; int ds = u->sd[ni]; uint8_t *e2 = fmt_dec_swar(ap, u->v[ni], u->S) - ds - (ds == u->S && ds); rl[t] = (uint16_t)(e2 - ap); ap = e2; }
                                else if (kd[t] == 2) { uint32_t L = rd32(u->elen + 4ull * u->xi++); if (L > 65535) { okp = 0; break; } memcpy(ap, u->p, L); u->p += L; ap += L; rl[t] = (uint16_t)L; }
                                else rl[t] = 0;
                            }
                        }
                    }
                }
                if (!okp) break;
                PROF_T(4);
                if (dense_rows) {                  /* every field copied in order: no bitmap, one bound check per row */
                    for (int t = 0; t < R; t++) { uint32_t tot = 0; for (int j = 0; j < k; j++) tot += RL[(size_t)j * RC + t]; rowtot[t] = tot; }
                    PROF_T(5);
                    for (int t = 0; t < R; t++) {
                        int64_t rr = r0 + t;
                        if (o + rowtot[t] + (size_t)k + 64 > oe) {                /* near the end of the output: exact copies */
                            for (int j = 0; j < k; j++) {
                                uint32_t len = RL[(size_t)j * RC + t];
                                if (len > (size_t)(oe - o)) goto out;
                                memcpy(o, src[j], len); o += len; src[j] += len;
                                if (j < k - 1) { if (o >= oe) goto out; *o++ = delim; }
                            }
                            if (!(noNL && rr == nr - 1)) { if (o >= oe) goto out; *o++ = '\n'; }
                            r = rr + 1; continue;
                        }
                        for (int j = 0; j < k; j++) {
                            uint32_t len = RL[(size_t)j * RC + t]; const uint8_t *sp = src[j];
                            if (len <= 16 && sp + 16 <= se[j]) memcpy(o, sp, 16); else memcpy(o, sp, len);
                            o += len; src[j] = sp + len; *o++ = delim;
                        }
                        if (!(noNL && rr == nr - 1)) o[-1] = '\n'; else o--;
                        r = rr + 1;
                    }
                    PROF_T(6);
                    if (!okp) break;
                    r = r1; continue;
                }
                memset(bm, 0, 8 * (size_t)R * W);
                for (int j = 0; j < k; j++) {
                    const uint16_t *rl = RL + (size_t)j * RC; uint64_t bit = 1ull << (j & 63); uint64_t *b = bm + (j >> 6);
                    for (int t = 0; t < R; t++) b[(size_t)t * W] |= rl[t] ? bit : 0;
                }
                PROF_T(5);
                for (int t = 0; t < R; t++) {
                    int64_t rr = r0 + t; int pos = 0;
                    for (int w = 0; w < W; w++) {
                        uint64_t mword = bm[(size_t)t * W + w];
                        while (mword) {
                            int j = w * 64 + __builtin_ctzll(mword); mword &= mword - 1;
                            int gap = j - pos;
                            if (gap <= 32 && o + 64 <= oe) { memcpy(o, dl, 32); o += gap; }
                            else { if (gap > oe - o) goto out; memset(o, delim, gap); o += gap; }
                            uint32_t len = RL[(size_t)j * RC + t]; const uint8_t *sp = src[j];
                            if (len <= 16 && o + 64 <= oe && sp + 16 <= se[j]) { memcpy(o, sp, 16); }
                            else if (len <= 32 && o + 64 <= oe && sp + 32 <= se[j]) { memcpy(o, sp, 32); }
                            else { if (len > (size_t)(oe - o)) goto out; memcpy(o, sp, len); }
                            o += len; src[j] = sp + len;
                            if (j < k - 1) { if (o >= oe) goto out; *o++ = delim; }
                            pos = j + 1;
                        }
                    }
                    if (pos <= k - 1) { int gap = k - 1 - pos; if (gap > oe - o) goto out; memset(o, delim, gap); o += gap; }
                    if (!(noNL && rr == nr - 1)) { if (o >= oe) goto out; *o++ = '\n'; }
                }
                PROF_T(6);
                r = r1;
            }
            free(bm); free(RL); free(arena); free(lv); bm = NULL; RL = NULL; arena = NULL; lv = NULL;
#ifdef CZN_TRACE
            fprintf(stderr, "tbl2 aligned path: r %lld of %lld okp %d\n", (long long)r, (long long)nr, okp);
#endif
            if (r < nr) {                       /* rare fallback (very long values): restart the block on the generic path */
                for (int j = 0; j < k; j++) { Col2 *c = &C[j]; cur_t *u = &cu[j]; memset(u, 0, sizeof *u); u->type = c->type;
                    if (c->type == 0) { u->lens = c->lens; u->longs = c->longs; u->p = c->data; u->pe = c->dend; }
                    else { u->kind = c->kind; u->sd = c->sd; u->v = c->vals; u->S = c->S; u->elen = c->elen; u->p = c->edata; u->pe = c->dend; } }
                o = out; r = 0; e = 0;
            }
        }
        for (; r < nr; r++) {
            int last = k - 1;
            if (e < nexc && rd32(exc + 8 * e) == (uint64_t)r) { last = (int)rd32(exc + 8 * e + 4) - 1; e++; }
            for (int j = 0; j <= last; j++) {
                cur_t *u = &cu[j]; uint32_t i = u->i++;
                if (u->type == 0) {
                    uint32_t len = u->lens[i];
                    if (len == 255) len = rd32(u->longs + 4ull * u->li++);
                    if (len <= 16 && o + 16 <= oe && u->p + 16 <= u->pe) memcpy(o, u->p, 16);
                    else { if (len > (size_t)(oe - o)) goto out; memcpy(o, u->p, len); }
                    o += len; u->p += len;
                } else {
                    uint8_t kd = u->kind[i];
                    if (kd == 1) {
                        uint32_t ni = u->ni++; int ds = u->sd[ni], cut = ds + (ds == u->S && ds);   /* trailing zeros (and dot) */
                        if (o + 40 <= oe) o = fmt_dec_swar(o, u->v[ni], u->S) - cut;
                        else { uint8_t t[32]; uint8_t *te = fmt_dec_fast(t, u->v[ni], u->S) - cut; size_t L = (size_t)(te - t); if (L > (size_t)(oe - o)) goto out; memcpy(o, t, L); o += L; }
                    } else if (kd == 2) {
                        uint32_t len = rd32(u->elen + 4ull * u->xi++);
                        if (len > (size_t)(oe - o)) goto out;
                        memcpy(o, u->p, len); o += len; u->p += len;
                    }
                }
                if (j < last) { if (o >= oe) goto out; *o++ = delim; }
            }
            if (!(noNL && r == nr - 1)) { if (o >= oe) goto out; *o++ = '\n'; }
        }
        if (o != oe) goto out;
    }
    ret = 0;
out:
    PROF_T(7);
    if (fv) for (int j = 0; j < k; j++) free(fv[j]);
    free(fv); free(la); free(gid); free(ridx); free(vals); free(C); free(bm); free(RL); free(arena); free(lv);
    return ret;
}

/* ---------------------------------------------------------------- columnar decode
 * For a table block with every row holding k fields (aligned; no short rows), hands back typed columns instead of
 * reassembling CSV rows: numeric columns as int64 values at a common scale, text columns as offset/length into the
 * block's own bytes. This is near-memcpy speed for analytical readers (Arrow, DuckDB, pandas) because the row
 * interleave and decimal formatting -- most of the decode cost -- are skipped. Returns column count, or -1 when the
 * block is not an aligned table (the caller then falls back to the normal row decode). */
typedef struct {
    int type;                 /* 0 = text, 2 = numeric */
    int scale;                /* numeric: value = num[i] / 10^scale */
    const int64_t *num;       /* numeric: nr values at the row index (0 where the cell is empty) */
    const uint8_t *valid;     /* numeric: nr flags, 0 = empty cell (null) */
    const uint8_t *txt;       /* text: the block's bytes */
    uint32_t *off, *len;      /* text: nr offsets/lengths into txt; len 0 = empty cell */
} czcol;
typedef struct { int k; int64_t nr; czcol *c; int64_t *numbuf; uint32_t *offbuf; uint8_t *validbuf; } czcols;

static int tbl2_columns(const uint8_t *in, int64_t tn, czcols *out) {
    memset(out, 0, sizeof *out);
    if (tn < 24) return -1;
    uint32_t h[4]; memcpy(h, in, 16); int k = h[0]; int64_t nr = h[1], nexc = h[2];
    int32_t gcol; uint32_t nform; memcpy(&gcol, in + 16, 4); memcpy(&nform, in + 20, 4);
    if (k < 1 || k > 256 || nform > 256 || gcol >= k || gcol < -1 || nexc != 0 || nr <= 0) return -1;  /* aligned only */
    if (24 + 16ull * nform > (uint64_t)tn) return -1;
    tformula FB[256];
    for (uint32_t q = 0; q < nform; q++) {
        const uint8_t *e = in + 24 + 16 * q;
        memcpy(&FB[q].y, e, 2); memcpy(&FB[q].a, e + 2, 2); memcpy(&FB[q].b, e + 4, 2); FB[q].form = e[6]; memcpy(&FB[q].k, e + 8, 8);
        if (FB[q].form < FM_RATIO || FB[q].form > FM_PCT || FB[q].y >= k || FB[q].a >= k || (FB[q].form == FM_RATIO && FB[q].b >= k)) return -1;
    }
    const uint8_t *p = in + 24 + 16 * nform, *end = in + tn;
    Col2 *C = calloc(k, sizeof(Col2)); if (!C) return -1;
    int64_t *vals = NULL, *numbuf = NULL; uint32_t *gid = NULL, *ridx = NULL, *offbuf = NULL; double **fv = NULL; double *la = NULL;
    uint8_t *validbuf = NULL; czcol *cc = NULL; int ret = -1;
    for (int j = 0; j < k; j++) {
        if (p + 5 > end) goto out;
        Col2 *c = &C[j]; c->type = p[0]; uint32_t size; memcpy(&size, p + 1, 4); p += 5;
        if ((c->type != 0 && c->type != 2) || size > (size_t)(end - p)) goto out;
        const uint8_t *q = p, *qe = p + size;
        if (c->type == 0) {
            if (size < 8) goto out;
            memcpy(&c->cnt, q, 4); memcpy(&c->nlong, q + 4, 4); q += 8;
            if (c->cnt != (uint32_t)nr || c->nlong > size) goto out;
            c->lens = q; q += c->cnt; c->longs = q; q += 4ull * c->nlong; c->data = q; c->dend = qe;
            if (q > qe) goto out;
        } else {
            if (size < 16) goto out;
            memcpy(&c->cnt, q, 4); memcpy(&c->nnum, q + 4, 4); memcpy(&c->nexc, q + 8, 4);
            c->S = q[12]; c->w = q[13]; c->mode = q[14]; c->fid = q[15]; q += 16;
            if (c->S > 18 || c->w < 1 || c->w > 8 || c->mode > 5 || c->cnt != (uint32_t)nr || c->nnum > c->cnt || c->nexc > c->cnt) goto out;
            c->kind = q; q += c->cnt; c->sd = q; q += c->nnum; c->pl = q; q += (size_t)c->nnum * c->w;
            c->elen = q; q += 4ull * c->nexc; c->edata = q; c->dend = qe;
            if (q > qe) goto out;
            uint8_t mx = 0; for (uint32_t i = 0; i < c->nnum; i++) mx = c->sd[i] > mx ? c->sd[i] : mx;
            if (mx > c->S) goto out;
            uint32_t n1 = 0, n2 = 0; for (uint32_t i = 0; i < c->cnt; i++) { uint8_t kd = c->kind[i]; if (kd > 2) goto out; n1 += kd == 1; n2 += kd == 2; }
            if (n1 != c->nnum || n2 != c->nexc) goto out;              /* cells: numeric (1), empty (0), or text (2 -> null) */
        }
        p = qe;
    }
    gid = malloc(4 * ((size_t)nr + 1)); if (!gid) goto out;
    if (gcol >= 0) {
        Col2 *g = &C[gcol]; if (g->type != 0) goto out;
        const uint8_t *d = g->data, *pd = NULL; uint32_t pl = 0, li = 0, gg = 0;
        for (int64_t r = 0; r < nr; r++) {
            uint32_t len = g->lens[r];
            if (len == 255) { if (li >= g->nlong) goto out; memcpy(&len, g->longs + 4ull * li++, 4); }
            if (len > (size_t)(g->dend - d)) goto out;
            if (r && (len != pl || memcmp(d, pd, len))) gg++;
            gid[r] = gg; pd = d; pl = len; d += len;
        }
    } else for (int64_t r = 0; r < nr; r++) gid[r] = 0;
    /* numeric columns decoded dense (nnum values each), then scattered to nr rows with a validity flag */
    int knum = 0; for (int j = 0; j < k; j++) knum += C[j].type == 2;
    size_t totnum = 0; for (int j = 0; j < k; j++) if (C[j].type == 2) totnum += C[j].nnum;
    vals = malloc(8 * (totnum + 1)); fv = calloc(k, sizeof(double *));
    la = malloc(sizeof(double) * ((size_t)nr + 1)); ridx = malloc(4 * ((size_t)nr + 2));
    if (!vals || !fv || !la || !ridx) goto out;
    { int64_t *vp = vals; for (int j = 0; j < k; j++) { C[j].ridx = ridx; if (C[j].type == 2) { C[j].vals = vp; vp += C[j].nnum; } } }
    for (int pass = 0; pass < 2; pass++)
        for (int j = 0; j < k; j++) {
            Col2 *c = &C[j]; if (c->type != 2 || (c->mode == 3) != pass) continue;
            if (c->mode != 3) { if (num_decode(c, gid, NULL, NULL, NULL, NULL, c->vals)) goto out; continue; }
            if ((uint32_t)c->fid >= nform) goto out;
            const tformula *F = &FB[c->fid]; if (F->y != j) goto out;
            const Col2 *A = &C[F->a], *B = F->form == FM_RATIO ? &C[F->b] : NULL;
            if (A->type != 2 || A->mode == 3 || (B && (B->type != 2 || B->mode == 3))) goto out;
            if (!fv[F->a]) fv[F->a] = col2_fvals(A, (uint32_t)nr);
            if (B && !fv[F->b]) fv[F->b] = col2_fvals(B, (uint32_t)nr);
            const double *fa = fv[F->a];
            if (F->form != FM_RATIO) { la[0] = NAN; for (int64_t r = 1; r < nr; r++) la[r] = gid[r] == gid[r - 1] ? fa[r - 1] : NAN; }
            if (num_decode(c, gid, F, fa, B ? fv[F->b] : NULL, F->form != FM_RATIO ? la : NULL, c->vals)) goto out;
        }
    /* build the output columns: numeric scattered to rows with validity, text as (offset, length) */
    { int ktxt = 0; for (int j = 0; j < k; j++) ktxt += C[j].type == 0;
      numbuf = malloc(8 * ((size_t)nr * knum + 1)); validbuf = malloc((size_t)nr * knum + 1);
      offbuf = malloc(8 * ((size_t)nr * ktxt + 1));
      cc = calloc(k, sizeof(czcol));
      if ((knum && (!numbuf || !validbuf)) || (ktxt && !offbuf) || !cc) goto out;
      int64_t *np = numbuf; uint8_t *vp = validbuf; uint32_t *op = offbuf;
      for (int j = 0; j < k; j++) {
          Col2 *c = &C[j];
          if (c->type == 2) {
              cc[j].type = 2; cc[j].scale = c->S; cc[j].num = np; cc[j].valid = vp;
              const int64_t *dense = c->vals; uint32_t ni = 0;
              for (int64_t r = 0; r < nr; r++) { int has = c->kind[r] == 1; vp[r] = (uint8_t)has; np[r] = has ? dense[ni++] : 0; }
              np += nr; vp += nr; continue;
          }
          cc[j].type = 0; cc[j].txt = c->data; cc[j].off = op; cc[j].len = op + nr; op += 2 * nr;
          const uint8_t *d = c->data; uint32_t li = 0;
          for (int64_t r = 0; r < nr; r++) {
              uint32_t len = c->lens[r];
              if (len == 255) { if (li >= c->nlong) goto out; memcpy(&len, c->longs + 4ull * li++, 4); }
              if (len > (size_t)(c->dend - d)) goto out;
              cc[j].off[r] = (uint32_t)(d - c->data); cc[j].len[r] = len; d += len;
          }
      } }
    out->k = k; out->nr = nr; out->c = cc; out->numbuf = numbuf; out->offbuf = offbuf; out->validbuf = validbuf;
    ret = k; cc = NULL; numbuf = NULL; offbuf = NULL; validbuf = NULL;   /* ownership moved to out */
out:
    if (fv) for (int j = 0; j < k; j++) free(fv[j]);
    free(fv); free(la); free(gid); free(ridx); free(vals); free(C); free(cc); free(numbuf); free(offbuf); free(validbuf);
    return ret;
}
static void tbl2_columns_free(czcols *x) { free(x->c); free(x->numbuf); free(x->offbuf); free(x->validbuf); memset(x, 0, sizeof *x); }

int tbl2_columns_pub(const uint8_t *in, int64_t tn, czcols *out) { return tbl2_columns(in, tn, out); }
void tbl2_columns_free_pub(czcols *x) { tbl2_columns_free(x); }
int64_t tbl2_encode_pub(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, const fset_t *fs, uint8_t *dst) { return tbl2_encode(in, n, delim, k, quotes, fs, dst, NULL); }
int tbl2_decode_pub(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n) { return tbl2_decode(in, tn, delim, out, n); }
void fset_discover_pub(const uint8_t *s, size_t n, uint8_t delim, int k, int quotes, int effort, fset_t *fs) { fset_discover(s, n, delim, k, quotes, effort, fs, 1); }
int parse_dec_pub(const uint8_t *p, uint32_t len, int64_t *m, int *sc) { return parse_dec(p, len, m, sc); }
const int64_t *czn_p10_pub(void) { return P10; }
