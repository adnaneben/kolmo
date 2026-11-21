/* Codec 31: kimg, lossless 8-bit images (ratio and ultra; container version 11). Pixels in rows of iw bytes, ich
 * interleaved channels (1-4). Each sample is predicted by a blend of eight predictors (spatial gradients and, from the
 * second channel on, the earlier channel's residual carried over), weighted by their recent errors at the
 * neighbouring pixels (integer arithmetic, so every platform decodes the same bytes), then corrected by a per-context
 * bias. The folded residual is coded bit by bit with an adaptive binary range coder: zero flag, bit length, mantissa,
 * each bit under contexts of the expected error (the neighbours' residuals) and the channel.
 *
 * Stream: version u8 (1) | ich u8 | reserved u16 (0) | iw u32 | phase u32 (byte column of the first pixel byte) |
 *         head u32 (bytes before the pixels, stored) | npx u32 (pixel bytes coded) | head bytes | tail bytes
 *         (len - head - npx, stored) | coded pixel bytes.
 * Decoders before this codec refuse id 31 (not in their registry); version bytes other than 1 are refused.
 * Part of the single translation unit built from src/kolmo.c. */
#define KIMG_ES 12                     /* error entries per byte column: KIMG_NP + the residual, padded */
#define KIMG_NP 10                     /* predictors: 7 spatial, 3 with the earlier channel's error carried over */
#define KIMG_NA 12                     /* expected-error buckets */
#define KIMG_HDR 24
#ifndef KIMG_SEL_MIN
#define KIMG_SEL_MIN (256u << 10)      /* pixel bytes from which the transform is chosen on a sample */
#endif
static inline void kimg_p32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static inline uint32_t kimg_g32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

#define KIMG_AI __attribute__((always_inline)) static inline
/* binary range coder (carry-less, 32-bit, 12-bit probabilities of a 1) */
typedef struct { uint32_t x1, x2, x; const uint8_t *in, *end; uint8_t *out, *oend; int err; } kimg_rc;
KIMG_AI void kimg_put(kimg_rc *r, int p, int bit) {
    uint32_t xm = r->x1 + ((r->x2 - r->x1) >> 12) * (uint32_t)p;
    if (bit) r->x2 = xm; else r->x1 = xm + 1;
    while (((r->x1 ^ r->x2) & 0xff000000u) == 0) {
        if (r->out < r->oend) *r->out++ = (uint8_t)(r->x2 >> 24); else r->err = 1;
        r->x1 <<= 8; r->x2 = (r->x2 << 8) | 255;
    }
}
KIMG_AI int kimg_get(kimg_rc *r, int p) {
    uint32_t xm = r->x1 + ((r->x2 - r->x1) >> 12) * (uint32_t)p;
    int bit = r->x <= xm;
    if (bit) r->x2 = xm; else r->x1 = xm + 1;
    while (((r->x1 ^ r->x2) & 0xff000000u) == 0) {
        r->x1 <<= 8; r->x2 = (r->x2 << 8) | 255; r->x = (r->x << 8) | (r->in < r->end ? *r->in++ : 0);
    }
    return bit;
}
/* logistic mixing (as lpaq): stretch(p) = ln(p / (1 - p)), squash its inverse, 12-bit fixed point */
static int16_t kimg_st[4096]; static int16_t kimg_sq[4096]; static int kimg_tabs;
static inline int kimg_squash(int d) { if (d > 2047) d = 2047; if (d < -2047) d = -2047; return kimg_sq[d + 2048]; }
static void kimg_init_tabs(void) {
    if (__atomic_load_n(&kimg_tabs, __ATOMIC_ACQUIRE)) return;
    for (int i = 0; i < 4096; i++) {   /* integer squash: 4096 / (1 + e^-x), x = (i - 2048) / 256, by interpolation of 33 points */
        static const int t[33] = {1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101, 1546, 2047,
                                  2549, 2994, 3348, 3607, 3785, 3901, 3975, 4024, 4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094};
        int d = i - 2048, w = d & 127; int k = (d >> 7) + 16;
        kimg_sq[i] = (int16_t)(k >= 32 ? 4095 : (t[k] * (128 - w) + t[k + 1] * w + 64) >> 7);
    }
    int pi = 0;
    for (int x = -2047; x <= 2047; x++) { int v = kimg_squash(x); for (int j = pi; j <= v; j++) kimg_st[j] = (int16_t)x; pi = v + 1; }
    for (int j = pi; j < 4096; j++) kimg_st[j] = 2047;
    __atomic_store_n(&kimg_tabs, 1, __ATOMIC_RELEASE);
}
/* adaptive counter: 16-bit probability and a count (rate 1/(n+1.5), from 1/1.5 down to 1/KIMG_LIM) */
#define KIMG_LIM 255
#ifndef KIMG_LR
#define KIMG_LR 1
#endif
#ifndef KIMG_LS
#define KIMG_LS 12
#endif
#ifndef KIMG_WP
#define KIMG_WP 2
#endif
#ifndef KIMG_SW
#define KIMG_SW 1u
#endif
#ifndef KIMG_CLIM
#define KIMG_CLIM 250
#endif
#ifndef KIMG_WSEL
#define KIMG_WSEL m->w[cx[0]][cx[1]][node]
#endif
static const uint16_t *kimg_dt(void) {
    static uint16_t dt[KIMG_LIM + 1]; static int ready;
    if (!__atomic_load_n(&ready, __ATOMIC_ACQUIRE)) { for (int i = 0; i <= KIMG_LIM; i++) dt[i] = (uint16_t)(65536.0 * 2 / (2 * i + 3)); __atomic_store_n(&ready, 1, __ATOMIC_RELEASE); }
    return dt;
}
typedef struct { uint16_t p; uint8_t n, lim; } kimg_ctr;
KIMG_AI void kimg_upd(kimg_ctr *c, int bit, const uint16_t *dt) {
    int t = bit ? 65535 : 0; c->p = (uint16_t)(c->p + (((t - (int)c->p) * (int)dt[c->n]) >> 16));
    if (c->n < c->lim) c->n++;
}
#define KIMG_NODES 27          /* zero flag (0), bit length steps (1-8), first mantissa bit by length (10-17), second (19-26) */
#define KIMG_NI 5              /* mixer inputs: four context models and a bias */
typedef struct {
    kimg_ctr t0[4][KIMG_NA][KIMG_NODES];
    kimg_ctr t1[4][8][8][KIMG_NODES];
    kimg_ctr t2[4][27][6][KIMG_NODES];
    kimg_ctr t3[4][8][8][KIMG_NODES];
    kimg_ctr low[4][9][9];                        /* the lower mantissa bits: channel, bit length, position */
    int32_t w[4][KIMG_NA][KIMG_NODES][KIMG_NI];   /* mixer weights (16.16) by channel, error bucket and node */
    int32_t bsum[4][KIMG_NA][9]; int32_t bcnt[4][KIMG_NA][9];   /* bias: sum of errors, count, per context */
} kimg_model;
static void kimg_model_init(kimg_model *m) {
    kimg_ctr *c = (kimg_ctr *)m; size_t nc = offsetof(kimg_model, w) / sizeof(kimg_ctr);
    for (size_t i = 0; i < nc; i++) { c[i].p = 32768; c[i].n = 0; c[i].lim = KIMG_CLIM; }
    for (int i = 0; i < 4 * 9 * 9; i++) ((kimg_ctr *)m->low)[i].lim = KIMG_LIM;
    int32_t *w = &m->w[0][0][0][0];
    for (size_t i = 0; i < sizeof m->w / sizeof(int32_t); i++) w[i] = (i % KIMG_NI) == KIMG_NI - 1 ? 0 : (1 << 16) * 3 / 10;
    memset(m->bsum, 0, sizeof m->bsum); memset(m->bcnt, 0, sizeof m->bcnt);
}
typedef struct { kimg_ctr *c[4]; int32_t *w; int st[KIMG_NI]; int p; } kimg_mix;
KIMG_AI int kimg_mix_p(kimg_mix *x) {
    int64_t dot = (int64_t)x->w[4] * 256; x->st[4] = 256;
    for (int i = 0; i < 4; i++) { x->st[i] = kimg_st[x->c[i]->p >> 4]; dot += (int64_t)x->w[i] * x->st[i]; }
    int p = kimg_squash((int)(dot >> 16)); x->p = p < 1 ? 1 : p > 4094 ? 4094 : p;
    return x->p;
}
KIMG_AI void kimg_mix_upd(kimg_mix *x, int bit, const uint16_t *dt) {
    const int err = ((bit << 12) - x->p) * KIMG_LR;
    for (int i = 0; i < KIMG_NI; i++) x->w[i] += (x->st[i] * err) >> KIMG_LS;
    for (int i = 0; i < 4; i++) kimg_upd(x->c[i], bit, dt);
}
static inline int kimg_bucket(uint32_t e) {       /* expected error (sum of neighbour residual magnitudes) */
    static const uint32_t T[KIMG_NA - 1] = {1, 3, 6, 10, 16, 24, 36, 54, 80, 120, 200};
    int b = 0; while (b < KIMG_NA - 1 && e >= T[b]) b++;
    return b;
}
static inline int kimg_q8(int v) { v = v < 0 ? -v : v; return v == 0 ? 0 : v <= 1 ? 1 : v <= 2 ? 2 : v <= 4 ? 3 : v <= 8 ? 4 : v <= 16 ? 5 : v <= 32 ? 6 : 7; }
/* the four context rows and the weight row of one sample; a node indexes into each */
typedef struct { kimg_ctr *b[4]; int32_t (*w)[KIMG_NI]; } kimg_cx;
KIMG_AI int kimg_code(int enc, kimg_rc *r, const kimg_cx *X, int node, int bit, const uint16_t *dt) {
    kimg_mix x;
    x.c[0] = X->b[0] + node; x.c[1] = X->b[1] + node; x.c[2] = X->b[2] + node; x.c[3] = X->b[3] + node; x.w = X->w[node];
    const int p = kimg_mix_p(&x);
    if (enc) kimg_put(r, p, bit); else bit = kimg_get(r, p);
    kimg_mix_upd(&x, bit, dt);
    return bit;
}
KIMG_AI int kimg_code1(int enc, kimg_rc *r, kimg_ctr *c, int bit, const uint16_t *dt) {
    int p = c->p >> 4; p = p < 1 ? 1 : p > 4094 ? 4094 : p;
    if (enc) kimg_put(r, p, bit); else bit = kimg_get(r, p);
    kimg_upd(c, bit, dt); return bit;
}
static inline int kimg_clamp(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static inline uint32_t kimg_fold(int r, int P, int V) {  /* residual in [-P, V-P] -> 0..V */
    int m = P < V - P ? P : V - P, a = r < 0 ? -r : r;
    if (!r) return 0;
    if (a <= m) return (uint32_t)(r > 0 ? 2 * a - 1 : 2 * a);
    return (uint32_t)(m + a);
}
static inline int kimg_unfold(uint32_t u, int P, int V) {
    int m = P < V - P ? P : V - P;
    if (!u) return 0;
    if ((int)u <= 2 * m) { int k = (int)(u + 1) >> 1; return (u & 1) ? k : -k; }
    int a = (int)u - m; return P < V - P ? a : -a;     /* only the side with room is left */
}

/* the shared coding loop: enc = 1 codes px[0..npx) into r, enc = 0 decodes into px */
KIMG_AI int kimg_run_t(const int enc, int16_t *px, size_t npx, uint32_t iw, uint32_t ich, uint32_t phase, const int *V, kimg_rc *r) {
    if (!iw || !ich || ich > 4 || iw % ich || phase >= iw) return -1;
    kimg_model *m = malloc(sizeof *m);
    /* per predictor errors and the residual magnitude for two rows, indexed by byte column */
    uint16_t *err = calloc((size_t)2 * KIMG_ES * iw + KIMG_ES, sizeof(uint16_t));   /* + one zero entry (absent neighbours) */
    if (!m || !err) { free(m); free(err); return -1; }
    kimg_model_init(m); kimg_init_tabs(); const uint16_t *dt = kimg_dt();
#define EP(row, x) (err + ((size_t)(row) * iw + (x)) * KIMG_ES)   /* the KIMG_NP errors and the residual at one byte column */
    const uint16_t *const ez = err + (size_t)2 * KIMG_ES * iw;
    int prevres = 0, prevE[3] = {0, 0, 0};   /* the earlier channel of this pixel: its residual, its spatial errors */
    static uint64_t WT[16384]; static int wt_ready;   /* weight of an error sum s: 2^44 / s^KIMG_WP */
    if (!__atomic_load_n(&wt_ready, __ATOMIC_ACQUIRE)) { for (uint64_t s = 1; s < 16384; s++) WT[s] = ((uint64_t)1 << 44) / (KIMG_WP == 3 ? s * s * s : s * s); WT[0] = WT[1]; __atomic_store_n(&wt_ready, 1, __ATOMIC_RELEASE); }
    uint32_t xx = phase, rr = 0, cc = phase % ich;                  /* byte column, row parity, channel */
    for (size_t k = 0; k < npx; k++) {
        const uint32_t x = xx, row = rr, c = cc;
        if (++xx == iw) { xx = 0; rr ^= 1; cc = 0; } else if (++cc == ich) cc = 0;
        const int hasW = x >= ich && k >= ich, hasN = k >= iw, hasNW = hasN && x >= ich && k >= iw + ich;
        const int hasNE = k + ich >= iw && x + ich < iw && k >= iw - ich, hasWW = x >= 2 * ich && k >= 2 * ich, hasNN = k >= 2 * (size_t)iw;
        const int N = hasN ? px[k - iw] : (hasW ? px[k - ich] : 0);
        const int W = hasW ? px[k - ich] : N;
        const int NW = hasNW ? px[k - iw - ich] : N, NE = hasNE ? px[k - iw + ich] : N;
        const int WW = hasWW ? px[k - 2 * ich] : W, NN = hasNN ? px[k - 2 * (size_t)iw] : N;
        const int NNE = hasNN && hasNE ? px[k - 2 * (size_t)iw + ich] : NE;
        int P[KIMG_NP];
        P[0] = W + N - NW;
        { int mn = W < N ? W : N, mx = W < N ? N : W; P[1] = NW >= mx ? mn : NW <= mn ? mx : W + N - NW; }
        P[2] = (3 * W + 3 * N - 2 * NW + NE - NN + WW + 2) >> 2;     /* a smoother gradient (W and N trends) */
        P[3] = N + NE - NNE;
        P[4] = W + NE - N;
        P[5] = N;
        P[6] = W;
        P[7] = P[0] + prevE[0]; P[8] = P[1] + prevE[1]; P[9] = P[2] + prevE[2];
                                                     /* inter-channel: the earlier channel's error under the same
                                                      * spatial predictor carries over (7-9) */
        /* weights from each predictor's errors at W, N, NW, NE (this channel) */
        const uint32_t prow = row ^ 1;
        const uint16_t *eW = hasW ? EP(row, x - ich) : ez, *eN = hasN ? EP(prow, x) : ez;
        const uint16_t *eNW = hasNW ? EP(prow, x - ich) : ez, *eNE = hasNE ? EP(prow, x + ich) : ez;
        uint32_t sv[KIMG_ES];
        for (int i = 0; i < KIMG_ES; i++) sv[i] = 1u + KIMG_SW * eW[i] + KIMG_SW * eN[i] + eNW[i] + eNE[i];   /* vectorised */
        int64_t num = 0, den = 0;
        for (int i = 0; i < KIMG_NP; i++) {
            const uint32_t s = sv[i] > 16383 ? 16383 : sv[i];
            const int64_t w = (int64_t)WT[s];
            num += w * (int64_t)P[i]; den += w;
        }
        const int pr0 = (int)((num + (den >> 1)) / den); int pr = pr0;
        /* expected error: residual magnitudes at the neighbours and the earlier channel of this pixel */
        uint32_t ee = 2u * eW[KIMG_NP] + 2u * eN[KIMG_NP] + eNW[KIMG_NP] + eNE[KIMG_NP];
        if (c) ee += 2u * (uint32_t)(prevres < 0 ? -prevres : prevres);
        const int a = kimg_bucket(ee >> 1);
        /* bias context: the sign pattern of the local gradient */
        const int g1 = N - NW, g2 = W - NW, sc = (g1 > 0) - (g1 < 0) + 1 + 3 * ((g2 > 0) - (g2 < 0) + 1);
        int32_t *bs = &m->bsum[c][a][sc], *bc = &m->bcnt[c][a][sc];
        if (*bc) pr += (*bs >= 0 ? (*bs + (*bc >> 1)) : (*bs - (*bc >> 1))) / *bc;
        const int Vc = V[c], Pc = pr < 0 ? 0 : pr > Vc ? Vc : pr;
        const int rW = eW[KIMG_NP], rN = eN[KIMG_NP], rNE = eNE[KIMG_NP], rNW = eNW[KIMG_NP];
        const int g3 = NE - N, tx = (g1 > 0) - (g1 < 0) + 1 + 3 * ((g2 > 0) - (g2 < 0) + 1) + 9 * ((g3 > 0) - (g3 < 0) + 1);
        const kimg_cx X = { { m->t0[c][a], m->t1[c][kimg_q8(rW)][kimg_q8(rN)], m->t2[c][tx][a >> 1], m->t3[c][c ? kimg_q8(prevres) : kimg_q8(rNW)][kimg_q8(rNE)] }, m->w[c][a] };
        uint32_t u = enc ? kimg_fold((int)px[k] - Pc, Pc, Vc) : 0;
        if (!kimg_code(enc, r, &X, 0, u == 0, dt)) {
            const int LM = Vc > 255 ? 8 : 7;
            int L = enc ? 31 - __builtin_clz(u) : 0;                             /* 0..LM */
            if (enc) { for (int b = 0; b < LM; b++) { kimg_code(1, r, &X, 1 + b, L > b, dt); if (L <= b) break; } }
            else while (L < LM && kimg_code(0, r, &X, 1 + L, 0, dt)) L++;
            if (!enc) u = 1u << L;
            if (L >= 1) {
                int b1 = kimg_code(enc, r, &X, 9 + L, (u >> (L - 1)) & 1, dt); if (!enc) u |= (uint32_t)b1 << (L - 1);
                if (L >= 2) { int b2 = kimg_code(enc, r, &X, 18 + L, (u >> (L - 2)) & 1, dt); if (!enc) u |= (uint32_t)b2 << (L - 2); }
                for (int b = L - 3; b >= 0; b--) { int bb = kimg_code1(enc, r, &m->low[c][L][b], (u >> b) & 1, dt); if (!enc) u |= (uint32_t)bb << b; }
            }
        }
        if (!enc) { if ((int)u > Vc) { free(m); free(err); return -1; } px[k] = (int16_t)(Pc + kimg_unfold(u, Pc, Vc)); }
        const int v = px[k], res = v - Pc;
        /* bias update (halved at 64 samples, as LOCO-I) */
        *bs += v - pr0; (*bc)++;
        if (*bc >= 64) { *bs /= 2; *bc /= 2; }
        uint16_t *eo = EP(row, x);
        for (int i = 0; i < KIMG_NP; i++) { int q = P[i] < 0 ? 0 : P[i] > Vc ? Vc : P[i]; int e = v - q; eo[i] = (uint16_t)(e < 0 ? -e : e); }
        eo[KIMG_NP] = (uint16_t)(res < 0 ? -res : res);
        if (c + 1 < ich) { prevE[0] = v - P[0]; prevE[1] = v - P[1]; prevE[2] = v - P[2]; prevres = res; }
        else { prevE[0] = prevE[1] = prevE[2] = 0; prevres = 0; }
    }
#undef EP
    free(m); free(err);
    return 0;
}

/* colour transforms of the first three channels (whole pixels only): 0 none; 1 subtract green (G, R - G, B - G);
 * 2 YCoCg-R (Y, Co, Cg). Shifted to be non-negative: the ranges of the coded channels are in kimg_ranges. */
static int kimg_run(int enc, int16_t *px, size_t npx, uint32_t iw, uint32_t ich, uint32_t phase, const int *V, kimg_rc *r) {
    return enc ? kimg_run_t(1, px, npx, iw, ich, phase, V, r) : kimg_run_t(0, px, npx, iw, ich, phase, V, r);   /* two specialised loops */
}
static void kimg_ranges(int t, int *V) { for (int c = 0; c < 4; c++) V[c] = 255; if (t) { V[1] = V[2] = 510; } }
static void kimg_fwd(int t, const uint8_t *p, size_t npx, uint32_t ich, int16_t *s) {
    if (!t) { for (size_t k = 0; k < npx; k++) s[k] = p[k]; return; }
    for (size_t k = 0; k + ich <= npx; k += ich) {
        const int R = p[k], G = p[k + 1], B = p[k + 2];
        if (t == 1) { s[k] = (int16_t)G; s[k + 1] = (int16_t)(R - G + 255); s[k + 2] = (int16_t)(B - G + 255); }
        else { const int Co = R - B, tt = B + (Co >> 1), Cg = G - tt, Y = tt + (Cg >> 1);
               s[k] = (int16_t)Y; s[k + 1] = (int16_t)(Co + 255); s[k + 2] = (int16_t)(Cg + 255); }
        for (uint32_t c = 3; c < ich; c++) s[k + c] = p[k + c];
    }
}
static int kimg_inv(int t, const int16_t *s, size_t npx, uint32_t ich, uint8_t *p) {
    if (!t) { for (size_t k = 0; k < npx; k++) p[k] = (uint8_t)s[k]; return 0; }
    for (size_t k = 0; k + ich <= npx; k += ich) {
        int R, G, B;
        if (t == 1) { G = s[k]; R = s[k + 1] - 255 + G; B = s[k + 2] - 255 + G; }
        else { const int Y = s[k], Co = s[k + 1] - 255, Cg = s[k + 2] - 255, tt = Y - (Cg >> 1); G = Cg + tt; B = tt - (Co >> 1); R = B + Co; }
        if ((unsigned)R > 255 || (unsigned)G > 255 || (unsigned)B > 255) return -1;
        p[k] = (uint8_t)R; p[k + 1] = (uint8_t)G; p[k + 2] = (uint8_t)B;
        for (uint32_t c = 3; c < ich; c++) p[k + c] = (uint8_t)s[k + c];
    }
    return 0;
}
static size_t kimg_code_one(int t, const uint8_t *in, size_t head, size_t npx, size_t tail, uint32_t iw, uint32_t ich, uint32_t phase,
                            int16_t *sb, uint8_t *out, size_t cap) {
    out[0] = 1; out[1] = (uint8_t)ich; out[2] = (uint8_t)t; out[3] = 0;
    kimg_p32(out + 4, iw); kimg_p32(out + 8, phase); kimg_p32(out + 12, (uint32_t)head); kimg_p32(out + 16, (uint32_t)npx); kimg_p32(out + 20, 0);
    memcpy(out + KIMG_HDR, in, head); memcpy(out + KIMG_HDR + head, in + head + npx, tail);
    size_t o = KIMG_HDR + head + tail;
    kimg_rc r = { 0, 0xffffffffu, 0, NULL, NULL, out + o, out + cap, 0 };
    int V[4]; kimg_ranges(t, V); kimg_fwd(t, in + head, npx, ich, sb);
    if (kimg_run(1, sb, npx, iw, ich, phase, V, &r) || r.err) return 0;
    for (int i = 0; i < 4; i++) { if (r.out < r.oend) *r.out++ = (uint8_t)(r.x1 >> 24); else return 0; r.x1 <<= 8; }   /* flush */
    return (size_t)(r.out - out);
}
/* every colour transform that applies is coded and the smallest kept (ratio and ultra only) */
static size_t kimg_compress(const uint8_t *in, size_t n, uint32_t iw, uint32_t ich, uint32_t phase, size_t head, size_t npx,
                            uint8_t *out, size_t cap) {
    if (n >= ((size_t)1 << 31) || head > n || npx > n - head || !iw || !ich || ich > 4 || iw % ich || phase >= iw) return 0;
    const size_t tail = n - head - npx;
    if (cap < KIMG_HDR + head + tail + 16) return 0;
    int16_t *sb = malloc((npx ? npx : 1) * sizeof(int16_t)); uint8_t *alt = malloc(cap);
    if (!sb || !alt) { free(sb); free(alt); return 0; }
    const int nt = ich >= 3 && phase % ich == 0 && npx % ich == 0 ? 3 : 1;
    size_t best = 0; int tb = 0;
    if (nt > 1 && phase == 0 && npx >= (size_t)KIMG_SEL_MIN) {
        /* the transform from a sample: every eighth band of 16 rows, coded with each transform (3/8 of a full pass
         * instead of 2 extra passes); the image is then coded once */
        const size_t rows = npx / iw, band = 16; size_t sn = 0;
        uint8_t *smp = malloc(npx);
        if (smp) {
            for (size_t y = 0; y < rows; y += 8 * band) { size_t h = rows - y < band ? rows - y : band; memcpy(smp + sn, in + head + y * iw, h * iw); sn += h * iw; }
            size_t bz = 0;
            for (int t = 0; t < nt; t++) { size_t z = kimg_code_one(t, smp, 0, sn, 0, iw, ich, 0, sb, alt, cap); if (z && (!bz || z < bz)) { bz = z; tb = t; } }
            free(smp);
            best = kimg_code_one(tb, in, head, npx, tail, iw, ich, phase, sb, out, cap);
            free(sb); free(alt); return best;
        }
    }
    for (int t = 0; t < nt; t++) {
        size_t z = kimg_code_one(t, in, head, npx, tail, iw, ich, phase, sb, best ? alt : out, cap);
        if (z && (!best || z < best)) { if (best) memcpy(out, alt, z); best = z; }
    }
    free(sb); free(alt);
    return best;
}
static int kimg_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < KIMG_HDR || in[0] != 1 || in[2] > 2 || in[3] || kimg_g32(in + 20)) return -1;
    const uint32_t ich = in[1], iw = kimg_g32(in + 4), phase = kimg_g32(in + 8); const size_t head = kimg_g32(in + 12), npx = kimg_g32(in + 16);
    const int t = in[2];
    if (!ich || ich > 4 || !iw || iw % ich || phase >= iw || head > n || npx > n - head) return -1;
    if (t && (ich < 3 || phase % ich || npx % ich)) return -1;
    const size_t tail = n - head - npx;
    if (cn < KIMG_HDR + head + tail) return -1;
    memcpy(out, in + KIMG_HDR, head); memcpy(out + head + npx, in + KIMG_HDR + head, tail);
    const uint8_t *s = in + KIMG_HDR + head + tail;
    kimg_rc r = { 0, 0xffffffffu, 0, s, in + cn, NULL, NULL, 0 };
    for (int i = 0; i < 4; i++) r.x = (r.x << 8) | (r.in < r.end ? *r.in++ : 0);
    int16_t *sb = malloc((npx ? npx : 1) * sizeof(int16_t)); if (!sb) return -1;
    int V[4]; kimg_ranges(t, V);
    int e = kimg_run(0, sb, npx, iw, ich, phase, V, &r) || kimg_inv(t, sb, npx, ich, out + head);
    free(sb);
    return e ? -1 : 0;
}
#ifdef KOLMO_CODEC_REGISTRY
static int kimg_block_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return kimg_decompress(in, cn, out, n); }
static const kolmo_codec codec_kimg = { C_KIMG, "kimg", NULL, NULL, kimg_block_decompress, NULL };   /* written at block level */
#endif
