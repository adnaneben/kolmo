/* Ultra+ (UPX): the cold-storage context-mixing engine, carried as codec 7 model nibble 11 (FORMAT.md §25).
 *
 * Opt-in only (KOLMO_T_ULTRA_PLUS with mode ultra; CLI --ultra-plus). A stream holds, per bit, a mix of:
 *   - Kolmo's model 4 (this file's includer, cm.c), unchanged: its final probability, its four mixer outputs and all of
 *     its 2*nm+3 inputs;
 *   - a bank of hashed context models with a two-rate counter, a bit-history state map, a count-state map and a run
 *     (byte) model per context (orders 1-32, words, columns, sparse, indirect, record and text-structure contexts);
 *   - two match models (minimum lengths par ML0 and ML1);
 *   - optionally a byte LSTM (flags bits 8-15 = cells; PR #116's arithmetic);
 *   - a two-layer mixer (weight sets chosen by nine contexts, then a final mixer of their outputs), five APMs, and a
 *     32-bit binary arithmetic coder with 16-bit probabilities.
 *
 * Determinism (the decoder must reproduce every probability exactly): all float arithmetic is IEEE 754 binary32 in a
 * fixed order (sums run by index ascending), with no fused multiply-add (-ffp-contract=off) and no -ffast-math. exp is
 * upx_exp (Cody-Waite reduction and a degree-6 polynomial, no libm); stretch is a 4096-entry table built by bisection on
 * upx_sig. Probabilities reach the coder as uint32 (p * 65536, truncated, clamped to [1, 65535]).
 *
 * Stream (codec 7, extended header): size u32 (n | 8 << 28) | 0xB0 | flags u32 LE | par[16] u16 LE | coded bytes.
 * flags: bit 0 model 4 on; bit 1 model 4 with every model-3 context; bit 2 model 4's binary mask (else text);
 * bits 8-15 LSTM cells (0 = none); bits 16-23 model 4's table shift. */
enum { UPX_ML0, UPX_ML1, UPX_T, UPX_LLR, UPX_LRO, UPX_LR, UPX_LR2, UPX_SETS, UPX_LRD, UPX_W0, UPX_RL, UPX_PBS, UPX_APR, UPX_FM, UPX_NPAR = 16 };
static const char *upx_par_name[UPX_NPAR] = {"UP_ML0", "UP_ML1", "UP_T", "UP_LLR", "UP_LRO", "UP_LR", "UP_LR2", "UP_SETS", "UP_LRD", "UP_W0", "UP_RL", "UP_PBS", "UP_APR", "UP_FM"};
static const int upx_par_def[UPX_NPAR] = {6, 20, 30, 50, 1000, 5, 5, 0x1ff, 100, 0, 60, 2, 20, 0};
/* ---------------- deterministic float helpers ---------------- */
static inline float upx_exp(float x) {
    if (x > 87.f) x = 87.f; if (x < -87.f) x = -87.f;
    float t = x * 1.44269504f; int k = (int)(t >= 0 ? t + 0.5f : t - 0.5f);
    float r = x - (float)k * 0.693145752f; r = r - (float)k * 1.42860677e-6f;
    float p = 1.f + r * (1.f + r * (0.5f + r * (0.166666672f + r * (0.0416666679f + r * (0.00833333377f + r * 0.00138888892f)))));
    union { uint32_t u; float f; } s; s.u = (uint32_t)(k + 127) << 23; return p * s.f;
}
static inline float upx_sig(float x) { return 1.f / (1.f + upx_exp(-x)); }
static inline float upx_tanh(float x) { return 2.f * upx_sig(2.f * x) - 1.f; }
static float upx_ST[4096]; static pthread_once_t upx_st_once = PTHREAD_ONCE_INIT;          /* stretch(p/4096) for 12-bit p, built by bisection on upx_sig */
static void upx_st_init(void) {
    for (int i = 0; i < 4096; i++) {
        float target = (i + 0.5f) / 4096.f, lo = -12.f, hi = 12.f;
        for (int it = 0; it < 40; it++) { float mid = 0.5f * (lo + hi); if (upx_sig(mid) < target) lo = mid; else hi = mid; }
        upx_ST[i] = 0.5f * (lo + hi);
    }
}
static inline float upx_stf(float p) { int i = (int)(p * 4096.f); if (i < 0) i = 0; if (i > 4095) i = 4095; return upx_ST[i]; }   /* p in (0,1) */
static inline float upx_st12(int p) { return upx_ST[p < 0 ? 0 : p > 4095 ? 4095 : p]; }
static inline float upx_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

/* ---------------- LSTM (PR #116, unchanged arithmetic) ---------------- */
enum { UPX_NB = 256, UPX_TMAX = 64 };
typedef struct {
    int H, T, pos; float lr, lro; uint32_t rng; long steps;
    float *Wx, *Wh, *Wy, *mWx, *vWx, *mWh, *vWh, *gWx, *gWh; uint8_t touched[UPX_NB];
    float *h, *s; int xb[UPX_TMAX]; float *hp, *sp, *F, *C, *O, *S, *TS, *Hh, *dho;
    float probs[UPX_NB], tree[2 * UPX_NB];
} upx_lstm;
static float upx_lrand(upx_lstm *L) { L->rng ^= L->rng << 13; L->rng ^= L->rng >> 17; L->rng ^= L->rng << 5; return (float)(L->rng >> 8) / 16777216.f * 2.f - 1.f; }
static void upx_lstm_forward(upx_lstm *L, int b);
static upx_lstm *upx_lstm_new(int H, int T, float lr, float lro) {
    upx_lstm *L = calloc(1, sizeof *L); if (!L) return NULL; L->H = H; L->T = T; L->lr = lr; L->lro = lro; L->rng = 0x9E3779B9u;
    size_t nx = (size_t)3 * UPX_NB * H, nh = (size_t)3 * H * (H + 1), ny = (size_t)UPX_NB * (H + 1), w = (size_t)T * H;
    L->Wx = calloc(nx, 4); L->mWx = calloc(nx, 4); L->vWx = calloc(nx, 4); L->gWx = calloc(nx, 4);
    L->Wh = calloc(nh, 4); L->mWh = calloc(nh, 4); L->vWh = calloc(nh, 4); L->gWh = calloc(nh, 4);
    L->Wy = calloc(ny, 4); L->h = calloc(H, 4); L->s = calloc(H, 4);
    L->hp = calloc(w, 4); L->sp = calloc(w, 4); L->F = calloc(w, 4); L->C = calloc(w, 4); L->O = calloc(w, 4);
    L->S = calloc(w, 4); L->TS = calloc(w, 4); L->Hh = calloc(w, 4); L->dho = calloc(w, 4);
    {   void *q[] = {L->Wx, L->mWx, L->vWx, L->gWx, L->Wh, L->mWh, L->vWh, L->gWh, L->Wy, L->h, L->s, L->hp, L->sp, L->F, L->C, L->O, L->S, L->TS, L->Hh, L->dho};
        for (size_t i = 0; i < sizeof q / sizeof *q; i++) if (!q[i]) { for (size_t j = 0; j < sizeof q / sizeof *q; j++) free(q[j]); free(L); return NULL; } }
    float r = 1.f / sqrtf((float)H + 1.f);
    for (size_t i = 0; i < nx; i++) L->Wx[i] = upx_lrand(L) * r;
    for (size_t i = 0; i < nh; i++) L->Wh[i] = upx_lrand(L) * r;
    for (int g = 0; g < 3; g++) for (int i = 0; i < H; i++) L->Wh[((size_t)g * H + i) * (H + 1) + H] = g == 0 ? 1.f : 0.f;
    upx_lstm_forward(L, 0); return L;
}
static void upx_lstm_forward(upx_lstm *L, int b) {
    const int H = L->H, k = L->pos; float *hp = L->hp + k * H, *sp = L->sp + k * H;
    memcpy(hp, L->h, H * 4); memcpy(sp, L->s, H * 4); L->xb[k] = b;
    float *F = L->F + k * H, *C = L->C + k * H, *O = L->O + k * H, *S = L->S + k * H, *TS = L->TS + k * H, *Hh = L->Hh + k * H;
    for (int g = 0; g < 3; g++) {
        const float *wx = L->Wx + ((size_t)g * UPX_NB + b) * H; float *dst = g == 0 ? F : g == 1 ? C : O;
        for (int i = 0; i < H; i++) {
            const float *w = L->Wh + ((size_t)g * H + i) * (H + 1); float z = wx[i] + w[H];
            for (int j = 0; j < H; j++) z += w[j] * hp[j];
            dst[i] = g == 1 ? upx_tanh(z) : upx_sig(z);
        }
    }
    for (int i = 0; i < H; i++) { S[i] = F[i] * sp[i] + (1.f - F[i]) * C[i]; TS[i] = upx_tanh(S[i]); Hh[i] = O[i] * TS[i]; }
    memcpy(L->h, Hh, H * 4); memcpy(L->s, S, H * 4);
    float mx = -1e30f;
    for (int j = 0; j < UPX_NB; j++) { const float *w = L->Wy + (size_t)j * (H + 1); float z = w[H]; for (int i = 0; i < H; i++) z += w[i] * Hh[i]; L->probs[j] = z; if (z > mx) mx = z; }
    float sum = 0; for (int j = 0; j < UPX_NB; j++) { L->probs[j] = upx_exp(L->probs[j] - mx); sum += L->probs[j]; }
    float inv = 1.f / sum; for (int j = 0; j < UPX_NB; j++) L->probs[j] = L->probs[j] * inv + 1e-6f;
    for (int j = 0; j < UPX_NB; j++) L->tree[UPX_NB + j] = L->probs[j];
    for (int i = UPX_NB - 1; i >= 1; i--) L->tree[i] = L->tree[2 * i] + L->tree[2 * i + 1];
}
static inline void upx_adam(float *w, float *m, float *v, float g, float lr) {
    if (g > 5.f) g = 5.f; if (g < -5.f) g = -5.f;
    *m = 0.9f * *m + 0.1f * g; *v = 0.999f * *v + 0.001f * g * g; *w -= lr * *m / (sqrtf(*v) + 1e-6f);
}
static void upx_lstm_bptt(upx_lstm *L) {
    const int H = L->H; float dhn[256] = {0}, dsn[256] = {0}, zf[256], zc[256], zo[256];
    for (int k = L->T - 1; k >= 0; k--) {
        const float *F = L->F + k * H, *C = L->C + k * H, *O = L->O + k * H, *TS = L->TS + k * H, *sp = L->sp + k * H, *hp = L->hp + k * H, *dho = L->dho + k * H;
        for (int i = 0; i < H; i++) {
            float dh = dho[i] + dhn[i], dO = dh * TS[i], ds = dh * O[i] * (1.f - TS[i] * TS[i]) + dsn[i];
            if (ds > 10.f) ds = 10.f; if (ds < -10.f) ds = -10.f;
            float df = ds * (sp[i] - C[i]), dc = ds * (1.f - F[i]);
            dsn[i] = ds * F[i];
            zf[i] = df * F[i] * (1.f - F[i]); zc[i] = dc * (1.f - C[i] * C[i]); zo[i] = dO * O[i] * (1.f - O[i]);
        }
        const int b = L->xb[k]; L->touched[b] = 1;
        for (int j = 0; j < H; j++) dhn[j] = 0;
        for (int g = 0; g < 3; g++) {
            const float *z = g == 0 ? zf : g == 1 ? zc : zo; float *gx = L->gWx + ((size_t)g * UPX_NB + b) * H;
            for (int i = 0; i < H; i++) {
                gx[i] += z[i];
                float *gw = L->gWh + ((size_t)g * H + i) * (H + 1); const float *w = L->Wh + ((size_t)g * H + i) * (H + 1);
                for (int j = 0; j < H; j++) { gw[j] += z[i] * hp[j]; dhn[j] += w[j] * z[i]; }
                gw[H] += z[i];
            }
        }
    }
    const size_t nh = (size_t)3 * H * (H + 1);
    for (size_t i = 0; i < nh; i++) { upx_adam(L->Wh + i, L->mWh + i, L->vWh + i, L->gWh[i], L->lr); L->gWh[i] = 0; }
    for (int g = 0; g < 3; g++) for (int b = 0; b < UPX_NB; b++) if (L->touched[b]) {
        size_t o = ((size_t)g * UPX_NB + b) * H;
        for (int i = 0; i < H; i++) { upx_adam(L->Wx + o + i, L->mWx + o + i, L->vWx + o + i, L->gWx[o + i], L->lr); L->gWx[o + i] = 0; }
    }
    memset(L->touched, 0, sizeof L->touched);
}
static void upx_lstm_byte(upx_lstm *L, int b) {
    const int H = L->H, k = L->pos; const float *Hh = L->Hh + k * H; float *dho = L->dho + k * H;
    for (int i = 0; i < H; i++) dho[i] = 0;
    for (int j = 0; j < UPX_NB; j++) {
        float dy = L->probs[j] - (j == b ? 1.f : 0.f); float *w = L->Wy + (size_t)j * (H + 1);
        for (int i = 0; i < H; i++) { dho[i] += w[i] * dy; w[i] -= L->lro * dy * Hh[i]; }
        w[H] -= L->lro * dy;
    }
    L->steps++;
    if (++L->pos == L->T) { upx_lstm_bptt(L); L->pos = 0; }
    upx_lstm_forward(L, b);
}
static inline float upx_lstm_p1(const upx_lstm *L, int node) { float a = L->tree[node]; return L->tree[2 * node + 1] / a; }

/* ---------------- hashed context bank ---------------- */
/* slot: chk u16 | pa u16 (adaptive rate) | pb u16 (rate 1/4) | n u8 | bh u8 (last <= 7 bits, marker above) */
typedef struct { uint16_t chk, pa, pb; uint8_t n, bh, cs, pad; } upx_slot;   /* cs: n0 (4 bits) | n1 (4 bits), halved on overflow of the other */
enum { UPX_KMAX = 48 };
typedef struct {
    int K; uint32_t bits; upx_slot *t[UPX_KMAX]; uint32_t h[UPX_KMAX]; upx_slot *cur[UPX_KMAX];
    uint16_t sm[UPX_KMAX][2][256]; uint8_t smn[UPX_KMAX][2][256]; uint16_t sc[UPX_KMAX][256]; uint8_t scn[UPX_KMAX][256];      /* state map per context: [bp>=4?][bh] -> p16 */
    uint16_t rate[256];
    uint32_t *rt[UPX_KMAX]; uint32_t *rcur[UPX_KMAX]; uint16_t rp[UPX_KMAX][16][8]; int rq[UPX_KMAX], reb[UPX_KMAX];   /* run model: chk16|byte8|cnt8 */
    int order;
} upx_bank;
static inline uint32_t upx_hmix(uint32_t a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return a; }
static void upx_bank_init(upx_bank *B, int K, uint32_t bits) {
    B->K = K; B->bits = bits;
    for (int k = 0; k < K; k++) { B->t[k] = cm_big(sizeof(upx_slot) << bits); for (int i = 0; i < 2; i++) for (int j = 0; j < 256; j++) { B->sm[k][i][j] = 32768; B->smn[k][i][j] = 0; B->sc[k][j] = 32768; B->scn[k][j] = 0; } }
    for (int k = 0; k < K; k++) { B->rt[k] = cm_big(4u << (bits - 2)); for (int q = 0; q < 16; q++) for (int b = 0; b < 8; b++) B->rp[k][q][b] = 49152; }
    for (int n = 0; n < 256; n++) B->rate[n] = (uint16_t)(65536 / (n + 1.6));
}
static inline upx_slot *upx_bank_find(upx_bank *B, int k, uint32_t c0) {
    uint32_t h = upx_hmix(B->h[k] ^ (c0 * 0x9E3779B1u) ^ (uint32_t)k * 0x85EBCA6Bu);
    upx_slot *s = &B->t[k][h >> (32 - B->bits)]; uint16_t chk = (uint16_t)(h & 0xffff) | 1;
    if (s->chk != chk) { s->chk = chk; s->pa = s->pb = 32768; s->n = 0; s->bh = 1; s->cs = 0; }
    return s;
}

/* ---------------- adaptive probability helpers ---------------- */
static inline void upx_upd16(uint16_t *p, int y, int sh) { int g = y ? 65535 : 0; *p = (uint16_t)(*p + ((g - *p) >> sh)); }
typedef struct { float t[33]; } upx_apm_row;   /* APM over stretch(p): 33 knots in [-8, 8] */
typedef struct { upx_apm_row *r; int n, idx, lo; float w; float rate; } upx_apm;
static void upx_apm_init(upx_apm *a, int n, float rate) {
    a->n = n; a->rate = rate; a->r = malloc(sizeof(upx_apm_row) * n); if (!a->r) return;
    for (int i = 0; i < n; i++) for (int j = 0; j < 33; j++) a->r[i].t[j] = upx_sig((j - 16) * 0.5f);
}
static inline float upx_apm_p(upx_apm *a, float s, int idx) {
    s = upx_clamp(s, -7.999f, 7.999f); float u = (s + 8.f) * 2.f; int lo = (int)u; float w = u - lo;
    a->idx = idx; a->lo = lo; a->w = w;
    return a->r[idx].t[lo] * (1.f - w) + a->r[idx].t[lo + 1] * w;
}
static inline void upx_apm_upd(upx_apm *a, int y) {
    float *t = a->r[a->idx].t; float g = (float)y;
    t[a->lo] += (g - t[a->lo]) * a->rate * (1.f - a->w);
    t[a->lo + 1] += (g - t[a->lo + 1]) * a->rate * a->w;
}

/* ---------------- mixer ---------------- */
/* The mixer's dot product: eight partial sums s[j] over the indices i = j (mod 8), each by i ascending from 0, then
 * ((s0 + s4) + (s1 + s5)) + ((s2 + s6) + (s3 + s7)). The inputs are zero-padded to a multiple of 8 (x[ni..] = 0 and the
 * weights there are never updated from 0). This order is part of the stream contract (FORMAT.md §25); it lets two
 * 4-lane vectors carry the partial sums. */
static inline float upx_dot(const float *w, const float *x, int ni) {
    float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < ni; i += 8) for (int j = 0; j < 8; j++) s[j] += w[i + j] * x[i + j];
    return ((s[0] + s[4]) + (s[1] + s[5])) + ((s[2] + s[6]) + (s[3] + s[7]));
}
enum { UPX_NIMAX = 512, UPX_SMAX = 12 };
typedef struct { int ni, ns; uint32_t smask; float x[UPX_NIMAX]; float *w[UPX_SMAX]; int rows[UPX_SMAX], sel[UPX_SMAX]; float out[UPX_SMAX], pout[UPX_SMAX]; float lr, lr2, lrd; long t; float *w2; int rows2, sel2; float p2; } upx_mix;
static void upx_mix_init(upx_mix *M, int ns, const int *rows, int rows2, float lr, float lr2) {
    M->ns = ns; M->lr = lr; M->lr2 = lr2; M->rows2 = rows2;
    for (int s = 0; s < ns; s++) { M->rows[s] = rows[s]; M->w[s] = calloc((size_t)rows[s] * UPX_NIMAX, sizeof(float)); }
    M->w2 = malloc(sizeof(float) * rows2 * UPX_SMAX); if (!M->w2) return;
    for (int i = 0; i < rows2 * UPX_SMAX; i++) M->w2[i] = 1.f / ns;
}

/* ---------------- the model ---------------- */
typedef struct {
    uint16_t par[UPX_NPAR];
    const uint8_t *buf; size_t pos, n;      /* bytes coded so far: buf[0..pos) */
    cm_t *m4; int use_m4;
    upx_bank B; upx_lstm *L; upx_mix M;
    upx_apm a1, a2, a3, a4, a5;
    uint32_t c0, c4, c8; int bp;
    uint32_t word0, word1, word2, word3, wnum, lastnl, col, above_line, prevnl, nest, shape, wfirst;
    uint32_t pos_of[1 << 16]; int word0prev;
    uint32_t inq, qh, lastq, lastkey, lastns, fidx, fst[65], pfst[65], pfn;   /* structure: quoted strings, keys, fields of this and the previous line */   /* indirect: last position after a 2-byte context */
    uint8_t ind1[1 << 16], ind2[1 << 16];   /* indirect: bytes that followed order-1 / order-2 contexts */
    uint32_t rec_last[256]; uint32_t rec_len, rec_cnt[4096]; uint32_t rec_best;
    /* match models */
    uint32_t *mh[2]; uint32_t mbits; int mminl[2]; size_t mptr[2]; int mlen[2]; int mexp[2];
    uint16_t mp[2][64][2]; float mlast_p[2];
    float pr;                                  /* final probability (0,1) */
    float fx[8], fw[8][8], fp;                 /* learned final mixer (par FM) */
    int ninp; int nm4;
    double bits_m4, bits_mix, bits_final;
} upx_t;

static void upx_ctx_byte(upx_t *U) {   /* recompute the byte contexts after a byte */
    const uint8_t *b = U->buf; size_t p = U->pos;
    uint32_t c = U->c4 & 255;
#define BY(i) ((p >= (size_t)(i)) ? (uint32_t)b[p - (i)] : 0u)
    /* text state */
    int letter = (c | 32) >= 'a' && (c | 32) <= 'z';
    if (letter || c >= 128) { U->word0 = (U->word0 + ((c | 32) + 1)) * 0x2F0B4F35u; }
    else if (U->word0) { U->word3 = U->word2; U->word2 = U->word1; U->word1 = U->word0; U->word0 = 0; }
    if (c >= '0' && c <= '9') U->wnum = (U->wnum + c) * 0x1F351E2Bu; else U->wnum = 0;
    if (c == 10) { U->prevnl = U->lastnl; U->lastnl = (uint32_t)p; }
    U->col = (uint32_t)(p - U->lastnl);
    if (c == '(' || c == '[' || c == '{' || c == '<') U->nest = (U->nest * 4 + (c == '(' ? 1 : c == '[' ? 2 : c == '{' ? 3 : 0)) & 0xfff, U->nest += 0x10000;
    else if ((c == ')' || c == ']' || c == '}' || c == '>') && U->nest >= 0x10000) U->nest = ((U->nest - 0x10000) & 0xffff0000u) | ((U->nest & 0xfff) >> 2);
    if (c == '"') U->nest ^= 0x8000;
    U->shape = (U->shape << 2 | (letter ? 0u : (c >= '0' && c <= '9') ? 1u : c == ' ' ? 2u : 3u)) & 0xfff;
    if (letter && !U->word0prev) U->wfirst = (U->wfirst << 8 | (c | 32)) & 0xffffff;
    U->word0prev = letter;
    /* keys (a quoted string followed by ':') and fields (separated by space, tab, ',', ';', '|' or '=') */
    if (c == '"') { if (U->inq) { U->lastq = U->qh; U->inq = 0; } else { U->inq = 1; U->qh = 0x811C9DC5u; } }
    else if (U->inq) U->qh = (U->qh ^ c) * 0x01000193u;
    if (c == ':' && U->lastns == '"') U->lastkey = U->lastq;
    if (c != ' ' && c != '\t') U->lastns = c;
    if (c == 10) { memcpy(U->pfst, U->fst, sizeof U->fst); U->pfn = U->fidx; U->fidx = 0; U->fst[0] = (uint32_t)p; }
    else if ((c == ' ' || c == '\t' || c == ',' || c == ';' || c == '|' || c == '=') && !U->inq && U->fidx < 63) U->fst[++U->fidx] = (uint32_t)p;
    uint32_t above = (U->prevnl + U->col < U->lastnl) ? b[U->prevnl + U->col] : 0;
    /* record length: distance to the last occurrence of this byte, voted */
    uint32_t d = (uint32_t)p - U->rec_last[c]; U->rec_last[c] = (uint32_t)p;
    if (d > 1 && d < 4096) { if (++U->rec_cnt[d] > U->rec_cnt[U->rec_best] + 2) U->rec_best = d; }
    uint32_t R = U->rec_best ? U->rec_best : 1, ab = p >= R ? b[p - R] : 0, ab2 = p >= 2 * R ? b[p - 2 * R] : 0;
    /* indirect */
    uint32_t c2 = U->c4 & 0xffff;
    uint32_t i1 = U->ind1[(U->c4 >> 8) & 0xff], i2 = U->ind2[(U->c4 >> 8) & 0xffff];
    U->ind1[(U->c4 >> 8) & 0xff] = (uint8_t)c; U->ind2[(U->c4 >> 8) & 0xffff] = (uint8_t)c;
    (void)c2;
    uint32_t *h = U->B.h; int k = 0; uint32_t hh = 0;
    /* orders 1..7, 9, 12, 16, 20, 24, 32 */
    static const int ord[] = {1, 2, 3, 4, 5, 6, 7, 9, 12, 16, 20, 24, 32};
    int oi = 0;
    for (int i = 1; i <= 32; i++) { hh = (hh + BY(i) + 1) * 0x01000193u ^ (uint32_t)i; if (oi < 13 && i == ord[oi]) { h[k++] = hh + (uint32_t)i * 0x9E3779B9u; oi++; } }
    /* word contexts */
    h[k++] = upx_hmix(U->word0 + 0x100);
    h[k++] = upx_hmix(U->word0 ^ upx_hmix(U->word1 + 0x200));
    h[k++] = upx_hmix(U->word0 ^ upx_hmix(U->word1 ^ upx_hmix(U->word2 + 0x300)));
    h[k++] = upx_hmix(U->word0 ^ upx_hmix(U->word2 + 0x400));               /* skip word */
    h[k++] = upx_hmix(U->word0 ^ upx_hmix(c + 0x500));                        /* word + last char */
    h[k++] = upx_hmix(U->word1 ^ upx_hmix(c + (BY(2) << 8) + 0x600));       /* previous word + 2 chars */
    h[k++] = upx_hmix(U->wnum ^ 0x700 ^ (U->word0 << 1));
    /* text column */
    h[k++] = upx_hmix(above | (c << 8) | 0x800000u);
    h[k++] = upx_hmix(above | (U->col < 255 ? U->col : 255) << 8 | 0x900000u);
    /* sparse */
    h[k++] = upx_hmix(BY(2) | 0xA00u << 8);
    h[k++] = upx_hmix(BY(3) | 0xB00u << 8);
    h[k++] = upx_hmix(BY(2) | BY(3) << 8 | 0xC0u << 24);
    h[k++] = upx_hmix(BY(1) | BY(3) << 8 | 0xD0u << 24);
    h[k++] = upx_hmix(BY(4) | BY(8) << 8 | 0xE0u << 24);
    h[k++] = upx_hmix((c & 0xF0) | (BY(2) & 0xF0) << 8 | (BY(3) & 0xF0) << 16 | 0x0Fu << 28);
    /* indirect */
    h[k++] = upx_hmix(i1 | c << 8 | 0x11u << 24);
    h[k++] = upx_hmix(i2 | c2 << 8 | 0x12u << 24);
    /* record */
    h[k++] = upx_hmix(ab | ab2 << 8 | R << 16 | 0x13u << 28);
    h[k++] = upx_hmix(ab | c << 8 | (R & 0xff) << 16 | 0x14u << 24);
    h[k++] = upx_hmix((uint32_t)(p % R) | R << 12 | 0x15u << 28);
    h[k++] = upx_hmix(U->nest * 0x3C6EF372u ^ c ^ 0x16000000u);
    h[k++] = upx_hmix(U->word1 ^ upx_hmix(U->word2 + 0x1700));
    h[k++] = upx_hmix(U->word0 ^ (U->col < 32 ? U->col : 32) * 0x9E3779B1u ^ 0x18000000u);
    h[k++] = upx_hmix(U->shape | c << 12 | 0x19u << 24);
    h[k++] = upx_hmix(U->wfirst ^ upx_hmix(U->word0 + 0x1A00));
    {   uint32_t f = U->fidx, off = (uint32_t)p - U->fst[f], ab = 0;
        if (f <= U->pfn) { uint32_t q = U->pfst[f] + off, e = f < U->pfn ? U->pfst[f + 1] : U->lastnl; if (q < e && q < p) ab = b[q]; }
        h[k++] = upx_hmix(U->lastkey ^ upx_hmix(U->word0 + 0x1B00));
        h[k++] = upx_hmix(U->lastkey ^ upx_hmix((c | BY(2) << 8) + 0x1C0000));
        h[k++] = upx_hmix(f * 0x9E3779B1u ^ upx_hmix(U->word0 + 0x1D00));
        h[k++] = upx_hmix(ab | f << 8 | (off < 15 ? off : 15) << 16 | 0x1Eu << 24);
    }
    {   /* binary structure: position mod 4 with the last two bytes; x86-like prefix/escape bytes (0x0F, 0x66, REX
         * 0x40-0x4F) with the bytes after them; the bytes 4 and 8 back with the position mod 4 */
        uint32_t c2b = BY(2), pfx = c2b == 0x0F || c2b == 0x66 || (c2b & 0xF0) == 0x40 ? c2b : 0;
        h[k++] = upx_hmix((uint32_t)(p & 3) | c << 8 | c2b << 16 | 0x1Fu << 26);
        h[k++] = upx_hmix(pfx | c << 8 | (pfx ? BY(3) : 0) << 16 | 0x20u << 24);
        h[k++] = upx_hmix((uint32_t)(p & 3) | BY(4) << 8 | BY(8) << 16 | 0x21u << 26);
    }
    U->B.K = k;
#undef BY
}

static void upx_match_byte(upx_t *U) {
    const uint8_t *b = U->buf; size_t p = U->pos;
    for (int j = 0; j < 2; j++) {
        int L = U->mminl[j];
        if (U->mlen[j] > 0) {   /* extend or break */
            if (b[U->mptr[j]] == b[p - 1]) { U->mlen[j]++; U->mptr[j]++; } else U->mlen[j] = 0;
        }
        if (p >= (size_t)L) {
            uint32_t h = 0; for (int i = 1; i <= L; i++) h = (h + b[p - i] + 1) * 0x2F0B4F35u;
            h = upx_hmix(h + (uint32_t)j) >> (32 - U->mbits);
            if (U->mlen[j] == 0) {
                uint32_t q = U->mh[j][h];
                if (q > 0 && q < p) {
                    int l = 0; while (l < 65535 && (size_t)l < q && b[q - 1 - l] == b[p - 1 - l]) l++;
                    if (l >= L) { U->mlen[j] = l; U->mptr[j] = q; }
                }
            }
            U->mh[j][h] = (uint32_t)p;
        }
        U->mexp[j] = U->mlen[j] > 0 && U->mptr[j] < p ? b[U->mptr[j]] : -1;
    }
}
static inline int upx_mlen_q(int l) { return l == 0 ? 0 : l < 16 ? l : l < 32 ? 16 + (l - 16) / 4 : l < 64 ? 20 + (l - 32) / 8 : l < 512 ? 24 + (l - 64) / 64 : 31; }

static float upx_predict(upx_t *U) {
    upx_mix *M = &U->M; int ni = 0; float *x = M->x;
    const int bp = U->bp; uint32_t c0 = U->c0;
    int p4 = 2048;
    if (U->use_m4) {
        p4 = cm_predict(U->m4, 4);
        cm_t *m = U->m4; int nm = m->nmod, n4 = 2 * nm + 3;
        x[ni++] = upx_st12(p4) ; x[ni++] = upx_st12(m->pr1); x[ni++] = upx_st12(m->pr2); x[ni++] = upx_st12(m->pr3); x[ni++] = upx_st12(m->pr4); x[ni++] = upx_st12(m->prf);
        for (int i = 0; i < n4; i++) x[ni++] = m->x16[i] * (1.f / 256.f);
    }
    /* bank */
    upx_bank *B = &U->B;
    if (bp == 0) {
        for (int k = 0; k < B->K; k++) {
            uint32_t h = upx_hmix(B->h[k] * 0x2545F491u + (uint32_t)k); uint32_t *r = &B->rt[k][h >> (34 - B->bits)];
            if ((*r >> 16) != (h & 0xffff)) *r = (h & 0xffff) << 16;
            B->rcur[k] = r;
        }
    }
    for (int k = 0; k < B->K; k++) {
        upx_slot *s = B->cur[k] = upx_bank_find(B, k, c0);
        int hi = bp >= 4; uint16_t q = B->sm[k][hi][s->bh];
        if (s->n == 0) { x[ni++] = 0; x[ni++] = 0; x[ni++] = 0; x[ni++] = 0; x[ni++] = 0; }
        else {
            float sa = upx_stf(s->pa / 65536.f), sb = upx_stf(s->pb / 65536.f);
            x[ni++] = sa; x[ni++] = sb; x[ni++] = upx_stf(q / 65536.f); x[ni++] = upx_stf(B->sc[k][s->cs] / 65536.f);
            float conf = s->n > 15 ? 1.f : s->n / 15.f;
            x[ni++] = (s->pa / 65536.f - 0.5f) * 4.f * conf;
        }
    }
    {   int ord = 0;
        for (int k = 0; k < B->K; k++) {
            uint32_t r = *B->rcur[k]; int cnt = (int)(r & 255), e = (int)(r >> 8 & 255);
            if (cnt && ((uint32_t)(e + 256) >> (8 - bp)) == c0) {
                int eb = (e >> (7 - bp)) & 1, q = cnt > 15 ? 15 : cnt; B->rq[k] = q; B->reb[k] = eb;
                float sv = upx_stf(B->rp[k][q][bp] / 65536.f); x[ni++] = eb ? sv : -sv;
                if (k < 13) ord = k + 1;
            } else { B->rq[k] = -1; x[ni++] = 0; }
        }
        B->order = ord;
    }
    /* match models */
    for (int j = 0; j < 2; j++) {
        int e = U->mexp[j];
        if (e >= 0 && ((uint32_t)(e + 256) >> (8 - bp)) == c0) {
            int eb = (e >> (7 - bp)) & 1, lq = upx_mlen_q(U->mlen[j]);
            uint16_t q = U->mp[j][lq][eb];
            float sq = upx_stf(q / 65536.f); x[ni++] = sq; x[ni++] = (eb ? 1.f : -1.f) * (lq / 8.f);
        } else { x[ni++] = 0; x[ni++] = 0; }
    }
    /* lstm */
    float pl = 0.5f;
    if (U->L) { int node = (int)c0; pl = upx_lstm_p1(U->L, node); pl = upx_clamp(pl, 1e-4f, 1.f - 1e-4f); x[ni++] = upx_stf(pl); }
    x[ni++] = 0.25f;  /* bias */
    while (ni & 7) x[ni++] = 0.f;   /* padding for upx_dot */
    U->ninp = ni;
    /* layer 1: S weight sets */
    uint32_t c1 = U->c4 & 255, c2 = (U->c4 >> 8) & 255;
    int ml = U->mlen[0] == 0 ? 0 : U->mlen[0] < 16 ? 1 : U->mlen[0] < 32 ? 2 : 3;
    int sel[UPX_SMAX]; int ns = 0;
    sel[ns++] = (int)c0;
    sel[ns++] = (int)(c1 << 3 | (uint32_t)bp);
    sel[ns++] = (int)((upx_hmix(c2 | c1 << 8) >> 21) << 3 | (uint32_t)bp);       /* 2048*8 */
    sel[ns++] = ml * 8 + bp + 32 * (U->mlen[1] > 0);
    sel[ns++] = (int)((U->word0 ? 1 : 0) | (c1 >= 'a' && c1 <= 'z' ? 2 : 0) | (U->col < 3 ? 4 : 0)) * 8 + bp;
    sel[ns++] = (int)(upx_hmix(U->c4 & 0xffffff) >> 22) * 8 + bp;   /* 1024*8 */
    sel[ns++] = U->rec_best ? (int)((U->pos % (U->rec_best)) & 255) * 8 + bp : bp;
    if (U->L) { float c = pl > 0.5f ? pl : 1.f - pl; int q = (int)(c * 16.f); if (q > 15) q = 15; sel[ns++] = q * 8 + bp; }
    else sel[ns++] = bp;
    sel[ns++] = B->order * 8 + bp;
    float x2[UPX_SMAX + 4]; int n2 = 0;
    for (int s = 0; s < M->ns; s++) {
        if (!(M->smask >> s & 1)) { M->out[s] = 0; M->pout[s] = 0.5f; x2[n2++] = 0; continue; }
        int r = sel[s] % M->rows[s]; M->sel[s] = r;
        const float *w = M->w[s] + (size_t)r * UPX_NIMAX; float dot = 0.f;
        dot = upx_dot(w, x, ni);
        dot = upx_clamp(dot, -15.f, 15.f); M->out[s] = dot; M->pout[s] = upx_sig(dot); x2[n2++] = dot;
    }
    /* layer 2 */
    int r2 = (int)c0 % M->rows2; M->sel2 = r2; const float *w2 = M->w2 + r2 * UPX_SMAX; float d2 = 0.f;
    for (int s = 0; s < M->ns; s++) d2 += w2[s] * x2[s];
    d2 = upx_clamp(d2, -15.f, 15.f); M->p2 = upx_sig(d2);
    /* APM chain */
    float pa1 = upx_apm_p(&U->a1, d2, (int)c0);
    float pa2 = upx_apm_p(&U->a2, d2, (int)(c0 | c1 << 8));
    float pa3 = upx_apm_p(&U->a3, d2, (int)(upx_hmix(c0 | (U->c4 & 0xffff) << 8) >> 16));
    float pa4 = upx_apm_p(&U->a4, d2, (int)(upx_mlen_q(U->mlen[0]) * 2 + (U->mexp[0] >= 0 ? ((U->mexp[0] >> (7 - bp)) & 1) : 0)) * 256 + (int)c0);
    float pa5 = upx_apm_p(&U->a5, d2, (int)(upx_hmix(c0 ^ U->word0 * 31u) >> 16));
    float pf = (2.f * M->p2 + pa1 + pa2 + 2.f * pa3 + pa4 + pa5) * 0.125f;
    if (U->par[UPX_FM]) {   /* learned final mixer over the stretched mixer and APM outputs, a row per bit position */
        float *fx = U->fx, *fw = U->fw[bp]; float d = 0.f;
        fx[0] = d2; fx[1] = upx_stf(pa1); fx[2] = upx_stf(pa2); fx[3] = upx_stf(pa3); fx[4] = upx_stf(pa4); fx[5] = upx_stf(pa5); fx[6] = upx_stf(pf);
        for (int i = 0; i < 7; i++) d += fw[i] * fx[i];
        U->fp = upx_sig(upx_clamp(d, -15.f, 15.f)); pf = 0.5f * (pf + U->fp);
    }
    (void)p4;
    pf = upx_clamp(pf, 1.f / 65536.f, 1.f - 1.f / 65536.f);
    return U->pr = pf;
}

static void upx_update(upx_t *U, int y) {
    upx_mix *M = &U->M; const int ni = U->ninp; float *x = M->x;
    /* layer 2 */
    float e2 = (float)y - M->p2; float *w2 = M->w2 + M->sel2 * UPX_SMAX;
    for (int s = 0; s < M->ns; s++) w2[s] += M->lr2 * e2 * M->out[s];
    float lrm = 1.f + M->lrd * 65536.f / (65536.f + (float)(M->t++));
    /* layer 1 */
    for (int s = 0; s < M->ns; s++) {
        if (!(M->smask >> s & 1)) continue;
        float e = ((float)y - M->pout[s]) * M->lr * lrm; float *w = M->w[s] + (size_t)M->sel[s] * UPX_NIMAX;
        for (int i = 0; i < ni; i++) w[i] += e * x[i];
    }
    if (U->par[UPX_FM]) { float e = ((float)y - U->fp) * (U->par[UPX_FM] * 1e-4f); float *fw = U->fw[U->bp]; for (int i = 0; i < 7; i++) fw[i] += e * U->fx[i]; }
    upx_apm_upd(&U->a1, y); upx_apm_upd(&U->a2, y); upx_apm_upd(&U->a3, y); upx_apm_upd(&U->a4, y); upx_apm_upd(&U->a5, y);
    /* bank */
    upx_bank *B = &U->B;
    for (int k = 0; k < B->K; k++) {
        upx_slot *s = B->cur[k]; int hi = U->bp >= 4;
        uint16_t *q = &B->sm[k][hi][s->bh]; uint8_t *qn = &B->smn[k][hi][s->bh];
        { int g = y ? 65535 : 0; int r = 65536 / (*qn + 2); *q = (uint16_t)(*q + (((g - *q) * r) >> 16)); if (*qn < 254) (*qn)++; }
        { int g = y ? 65535 : 0; int r = B->rate[s->n < U->par[UPX_RL] ? s->n : U->par[UPX_RL]]; int v = s->pa + (((g - s->pa) * r) >> 16); s->pa = (uint16_t)(v < 32 ? 32 : v > 65503 ? 65503 : v); }
        upx_upd16(&s->pb, y, U->par[UPX_PBS]); if (s->pb < 32) s->pb = 32; if (s->pb > 65503) s->pb = 65503;
        { uint16_t *c = &B->sc[k][s->cs]; uint8_t *cn = &B->scn[k][s->cs]; int g = y ? 65535 : 0; int r = 65536 / (*cn + 2); *c = (uint16_t)(*c + (((g - *c) * r) >> 16)); if (*cn < 254) (*cn)++; }
        { int n0 = s->cs >> 4, n1 = s->cs & 15;
          if (y) { if (n1 < 15) n1++; if (n0 > 2) n0 = (n0 + 1) / 2; } else { if (n0 < 15) n0++; if (n1 > 2) n1 = (n1 + 1) / 2; }
          s->cs = (uint8_t)(n0 << 4 | n1); }
        if (s->n < 255) s->n++;
        s->bh = s->bh < 128 ? (uint8_t)(s->bh << 1 | y) : (uint8_t)(((s->bh << 1 | y) & 127) | 128);
        if (B->rq[k] >= 0) upx_upd16(&B->rp[k][B->rq[k]][U->bp], y == B->reb[k], 6);
    }
    if (U->bp == 7) {
        uint32_t c = (U->c0 << 1 | (uint32_t)y) & 255;
        for (int k = 0; k < B->K; k++) {
            uint32_t r = *B->rcur[k], cnt = r & 255, e = r >> 8 & 255;
            if (cnt && e == c) { if (cnt < 255) cnt++; } else { e = c; cnt = 1; }
            *B->rcur[k] = (r & 0xffff0000u) | e << 8 | cnt;
        }
    }
    /* match */
    for (int j = 0; j < 2; j++) {
        int e = U->mexp[j];
        if (e >= 0 && ((uint32_t)(e + 256) >> (8 - U->bp)) == U->c0) {
            int eb = (e >> (7 - U->bp)) & 1, lq = upx_mlen_q(U->mlen[j]);
            upx_upd16(&U->mp[j][lq][eb], y, 5);
            if (eb != y) U->mexp[j] = -1;
        }
    }
    if (U->use_m4) cm_update(U->m4, y, 4);
    U->c0 = U->c0 << 1 | (uint32_t)y; U->bp++;
    if (U->bp == 8) {
        uint8_t c = (uint8_t)U->c0; ((uint8_t *)U->buf)[U->pos++] = c;
        U->c8 = U->c8 << 8 | U->c4 >> 24; U->c4 = U->c4 << 8 | c; U->c0 = 1; U->bp = 0;
        upx_match_byte(U); upx_ctx_byte(U);
        if (U->L) upx_lstm_byte(U->L, c);
    }
}

static void upx_free(upx_t *U);
static upx_t *upx_new(uint8_t *buf, size_t n, uint32_t flags, const uint16_t *par) {
    upx_t *U = calloc(1, sizeof *U); if (!U) return NULL; memcpy(U->par, par, sizeof U->par);
    for (int b = 0; b < 8; b++) for (int i = 0; i < 8; i++) U->fw[b][i] = i == 6 ? 1.f : 0.f; U->buf = buf; U->n = n; U->c0 = 1;
    U->use_m4 = flags & 1;
    if (U->use_m4) {
        uint32_t mask = (flags & 2) ? CM_MASK3 : ((flags & 4) ? CM4_MASK_BIN : CM4_MASK_TEXT);
        cm_tune_t tune = cm4_tune; if (tune.b2) tune.b2 = cm4_prior_for(mask);
        U->m4 = cm_new_xm(buf, (int)(flags >> 16 & 255), 4, cm4_chain(n), 1, mask, n); if (!U->m4) { free(U); return NULL; } cm_set_tune(U->m4, tune);
    }
    uint32_t bits = 16; while (bits < 22 && ((size_t)1 << bits) < n * 2) bits++;
    upx_bank_init(&U->B, 45, bits);
    U->mbits = bits; for (int j = 0; j < 2; j++) U->mh[j] = calloc((size_t)1 << bits, 4);
    U->mminl[0] = U->par[UPX_ML0]; U->mminl[1] = U->par[UPX_ML1];
    for (int j = 0; j < 2; j++) for (int l = 0; l < 64; l++) { U->mp[j][l][0] = 16384; U->mp[j][l][1] = 49152; }
    int H = (flags >> 8) & 255;
    if (H) U->L = upx_lstm_new(H, U->par[UPX_T], U->par[UPX_LLR] * 1e-4f, U->par[UPX_LRO] * 1e-4f);
    int rows[UPX_SMAX] = {256, 2048, 2048 * 8, 64, 64, 1024 * 8, 256 * 8, 128, 14 * 8};
    upx_mix_init(&U->M, 9, rows, 256, U->par[UPX_LR] * 1e-4f, U->par[UPX_LR2] * 1e-4f);
    U->M.smask = (uint32_t)U->par[UPX_SETS]; U->M.lrd = U->par[UPX_LRD] * 0.1f;
    float ar = U->par[UPX_APR] * 1e-3f; upx_apm_init(&U->a1, 256, ar); upx_apm_init(&U->a2, 65536, ar); upx_apm_init(&U->a3, 65536, ar); upx_apm_init(&U->a4, 64 * 256, ar); upx_apm_init(&U->a5, 65536, ar);
    {   /* every allocation checked: on any failure the stream is declined (encoder) or refused (decoder) */
        int bad = (H && !U->L) || !U->M.w2 || !U->mh[0] || !U->mh[1] || !U->a1.r || !U->a2.r || !U->a3.r || !U->a4.r || !U->a5.r;
        for (int s = 0; s < U->M.ns; s++) bad |= !U->M.w[s];
        for (int k = 0; k < U->B.K; k++) bad |= !U->B.t[k] || !U->B.rt[k];
        if (bad) { upx_free(U); return NULL; }
    }
    if (U->use_m4) for (int s = 0; s < U->M.ns; s++) for (int r = 0; r < U->M.rows[s]; r++) U->M.w[s][(size_t)r * UPX_NIMAX] = U->par[UPX_W0] * 0.01f;
    upx_ctx_byte(U); upx_match_byte(U);
    return U;
}

/* ---------------- arithmetic coder: 32-bit, 16-bit probabilities ---------------- */
typedef struct { uint32_t x1, x2, x; uint8_t *o; size_t on, cap; const uint8_t *in; size_t ip, in_n; } upx_ac;
static inline uint32_t upx_ac_p16(float p) { uint32_t q = (uint32_t)(p * 65536.f); return q < 1 ? 1 : q > 65535 ? 65535 : q; }
static inline void upx_ac_enc(upx_ac *a, int y, uint32_t p) {   /* p = P(1) * 65536 */
    uint32_t xm = a->x1 + (uint32_t)(((uint64_t)(a->x2 - a->x1) * p) >> 16);
    if (y) a->x2 = xm; else a->x1 = xm + 1;
    while (((a->x1 ^ a->x2) & 0xff000000u) == 0) { if (a->on < a->cap) a->o[a->on++] = (uint8_t)(a->x2 >> 24); a->x1 <<= 8; a->x2 = a->x2 << 8 | 255; }
}
static inline int upx_ac_dec(upx_ac *a, uint32_t p) {
    uint32_t xm = a->x1 + (uint32_t)(((uint64_t)(a->x2 - a->x1) * p) >> 16); int y = a->x <= xm;
    if (y) a->x2 = xm; else a->x1 = xm + 1;
    while (((a->x1 ^ a->x2) & 0xff000000u) == 0) { a->x1 <<= 8; a->x2 = a->x2 << 8 | 255; a->x = a->x << 8 | (a->ip < a->in_n ? a->in[a->ip++] : 255); }
    return y;
}

static void upx_free(upx_t *U) {
    if (!U) return;
    if (U->m4) cm_free(U->m4);
    for (int k = 0; k < UPX_KMAX; k++) { if (U->B.t[k]) cm_big_free(U->B.t[k], sizeof(upx_slot) << U->B.bits); if (U->B.rt[k]) cm_big_free(U->B.rt[k], 4u << (U->B.bits - 2)); }
    for (int s = 0; s < U->M.ns; s++) free(U->M.w[s]);
    free(U->M.w2); free(U->a1.r); free(U->a2.r); free(U->a3.r); free(U->a4.r); free(U->a5.r);
    for (int j = 0; j < 2; j++) free(U->mh[j]);
    if (U->L) { upx_lstm *L = U->L; void *p[] = {L->Wx, L->mWx, L->vWx, L->gWx, L->Wh, L->mWh, L->vWh, L->gWh, L->Wy, L->h, L->s, L->hp, L->sp, L->F, L->C, L->O, L->S, L->TS, L->Hh, L->dho};
                for (size_t i = 0; i < sizeof p / sizeof *p; i++) free(p[i]); free(L); }
    free(U);
}
enum { UPX_HDR = 4 + 1 + 4 + 2 * UPX_NPAR };
/* the stream's decode working set (bytes), from n and flags: tables, mixers, APMs, model 4 */
static uint64_t upx_need(size_t n, uint32_t flags) {
    uint32_t bits = 16; while (bits < 22 && ((size_t)1 << bits) < n * 2) bits++;
    uint64_t b = (uint64_t)UPX_KMAX * ((sizeof(upx_slot) << bits) + (4u << (bits - 2))) + 2 * ((uint64_t)4 << bits) + sizeof(upx_t);
    b += (uint64_t)(256 + 2048 + 16384 + 64 + 64 + 8192 + 2048 + 128 + 112) * UPX_NIMAX * 4 + (uint64_t)(256 + 3 * 65536 + 16384) * sizeof(upx_apm_row);
    if (flags & 1) { uint32_t mk = (flags & 2) ? CM_MASK3 : (flags & 4) ? CM4_MASK_BIN : CM4_MASK_TEXT; b += cm_mem4(n, (int)(flags >> 16 & 255), mk) + cm4_chain(n); }
    uint64_t H = flags >> 8 & 255; if (H) b += 4 * (8 * (3 * 256 * H + 3 * H * (H + 1)) + 256 * (H + 1) + 64 * 9 * H);
    return b + (n + 8);
}
/* code buf[0..n) (encoder: in != NULL) or decode it from z (decoder); returns the coded size or 0 / -1 */
static int64_t upx_run(const uint8_t *in, uint8_t *buf, size_t n, uint32_t flags, const uint16_t *par, uint8_t *o, size_t cap, const uint8_t *z, size_t zn) {
    pthread_once(&upx_st_once, upx_st_init);
    /* memory: decoding refuses a stream larger than the frame declares, before allocating; both sides reserve the
     * working set with the governor (it waits rather than fails, so the encoder's choice never depends on load) */
    uint64_t need = upx_need(n, flags);
    if (!in && kmem_tls_dec_limit && need > kmem_tls_dec_limit) return -1;
    need = cm_acquire(need);   /* 0 inside a block job: the job's reservation covers UPX (core/blocks.c) */
    upx_t *U = upx_new(buf, n, flags, par); if (!U) { cm_release(need); return in ? 0 : -1; }
    upx_ac a = {0, 0xffffffffu, 0, o, 0, cap, z, 0, zn};
    if (!in) for (int i = 0; i < 4; i++) a.x = a.x << 8 | (a.ip < a.in_n ? a.in[a.ip++] : 255);
    for (size_t i = 0; i < n; i++) {
        int c = in ? in[i] : 0;
        for (int b = 7; b >= 0; b--) {
            uint32_t q = upx_ac_p16(upx_predict(U)); int y;
            if (in) { y = (c >> b) & 1; upx_ac_enc(&a, y, q); } else y = upx_ac_dec(&a, q);
            upx_update(U, y);
        }
        if (in && a.on >= cap) { upx_free(U); cm_release(need); return 0; }
    }
    if (in) { for (int i = 0; i < 4; i++) { if (a.on < cap) a.o[a.on++] = (uint8_t)(a.x1 >> 24); a.x1 <<= 8; } }
    upx_free(U); cm_release(need);
    return in ? (a.on <= cap ? (int64_t)a.on : 0) : 0;
}
/* encoder defaults: model 4's text or binary mask; the LSTM (H = 128) on text streams of 1 MiB and more, where it was
 * measured smaller (dickens: -0.43% at 1 MB, -0.52% at 4 MB, at 2.3-3.2x the coding time; RESULTS.md) */
static uint32_t upx_default_flags(int txt, size_t n) {
    uint32_t f = 1 | (txt ? 0 : 4) | (txt && n >= ((size_t)1 << 20) ? 128u << 8 : 0);
    uint32_t mk = (f & 4) ? CM4_MASK_BIN : CM4_MASK_TEXT;
    return f | (uint32_t)cm4_shift_for(n, mk) << 16;
}
/* a whole codec-7 stream (extended header, model nibble 11); 0 when it does not fit in cap or n >= 2^28 */
static size_t upx_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, uint32_t flags, const uint16_t *par) {
    if (n >= ((size_t)1 << 28) || cap < UPX_HDR + 8) return 0;
    uint32_t n32 = (uint32_t)n | 8u << 28; memcpy(out, &n32, 4); out[4] = 0xB0;
    for (int k = 0; k < 4; k++) out[5 + k] = (uint8_t)(flags >> 8 * k);
    for (int i = 0; i < UPX_NPAR; i++) { out[9 + 2 * i] = (uint8_t)par[i]; out[10 + 2 * i] = (uint8_t)(par[i] >> 8); }
    uint8_t *buf = malloc(n + 8); if (!buf) return 0;
    int64_t z = upx_run(in, buf, n, flags, par, out + UPX_HDR, cap - UPX_HDR, NULL, 0);
    free(buf);
    return z > 0 ? (size_t)z + UPX_HDR : 0;
}
static int upx_header(const uint8_t *in, size_t cn, size_t n, uint32_t *flags, uint16_t *par) {
    if (cn < UPX_HDR || n >= ((size_t)1 << 28)) return -1;
    uint32_t n32; memcpy(&n32, in, 4);
    if (n32 >> 28 != 8 || (n32 & 0x0FFFFFFFu) != n || in[4] != 0xB0) return -1;
    *flags = 0; for (int k = 0; k < 4; k++) *flags |= (uint32_t)in[5 + k] << 8 * k;
    if (*flags & 0xFF000008u) return -1;
    if ((*flags & 1) && ((*flags >> 16 & 255) < 2 || (*flags >> 16 & 255) > 16)) return -1;
    for (int i = 0; i < UPX_NPAR; i++) par[i] = (uint16_t)(in[9 + 2 * i] | in[10 + 2 * i] << 8);
    if (par[UPX_ML0] < 2 || par[UPX_ML0] > 64 || par[UPX_ML1] < 2 || par[UPX_ML1] > 64 || par[UPX_T] < 1 || par[UPX_T] > UPX_TMAX
        || par[UPX_RL] > 255 || par[UPX_PBS] > 15 || par[UPX_SETS] > 0x1ff) return -1;
    if (par[UPX_FM] > 64) return -1;
    for (int i = UPX_FM + 1; i < UPX_NPAR; i++) if (par[i]) return -1;
    return 0;
}
/* n: an upper bound of the stream's length (the block's transformed-length bound); the header gives the length */
static uint64_t upx_stream_need(const uint8_t *in, size_t cn, size_t n) {
    uint32_t f, n32; uint16_t par[UPX_NPAR]; if (cn < 4) return 0;
    memcpy(&n32, in, 4); size_t m = n32 & 0x0FFFFFFFu;
    return m > n || upx_header(in, cn, m, &f, par) ? 0 : upx_need(m, f);
}
static int upx_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    uint32_t f; uint16_t par[UPX_NPAR]; if (upx_header(in, cn, n, &f, par)) return -1;
    return upx_run(NULL, out, n, f, par, NULL, 0, in + UPX_HDR, cn - UPX_HDR) < 0 ? -1 : 0;
}
static void upx_default_par(uint16_t *par) { for (int i = 0; i < UPX_NPAR; i++) par[i] = (uint16_t)(i <= UPX_FM ? upx_par_def[i] : 0); }

/* Encoder-only gate (from #88's sampled pre-trial): code two 16 KiB windows (the start and the middle) with UPX and with
 * model 4; the full UPX pass runs unless UPX is 3% or more larger on the windows. Streams up to 64 KiB always try UPX. The gate
 * reads only the stream's bytes, so the output never depends on threads or memory. */
static int upx_gate(const uint8_t *in, size_t n, int txt) {
    if (n <= ((size_t)64 << 10)) return 1;
    const size_t w = (size_t)16 << 10, at[2] = {0, (n / 2) & ~(size_t)63};
    size_t cap = w + w / 4 + 1024, zu = 0, zc = 0; uint8_t *t = malloc(cap); if (!t) return 0;
    uint16_t par[UPX_NPAR]; upx_default_par(par);
    for (int k = 0; k < 2; k++) {
        size_t c = cm_compress_tm(in + at[k], w, t, cap, 4, 0, cm4_tune, txt ? CM4_MASK_TEXT : CM4_MASK_BIN);
        size_t u = upx_compress(in + at[k], w, t, cap, upx_default_flags(txt, w), par);
        zc += c ? c : w; zu += u ? u : w + 64;
    }
    free(t);
    return (uint64_t)zu * 100 < (uint64_t)zc * 103;   /* UPX learns slower at first: within 3% on the windows still tries */
}
