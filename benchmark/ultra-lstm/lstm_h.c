/* Research harness (not part of any Kolmo build target): Ultra model 4 plus a byte-level LSTM as an extra mixer input.
 * This is the claim-68 experiment ("cm-lstm-input", review40 R16).
 *
 * It includes the production src/codecs/cm.c unchanged and does not modify it. The production writer is untouched.
 * The candidate stream is a research container ("KLS1"), not a Kolmo frame, and no Kolmo reader accepts it.
 *
 *   lstm_h enc0 IN OUT    production model-4 stream (cm_compress_tm, the text mask and cm4_tune), inside "KLS0"
 *   lstm_h dec0 IN OUT    its decoder (cm_decompress)
 *   lstm_h enc  IN OUT    candidate: model 4 + LSTM + a final 3-input mixer, inside "KLS1"
 *   lstm_h dec  IN OUT    candidate decoder
 *
 * Settings for encoding come from the environment (LSTM_H, LSTM_T, LSTM_LR, LSTM_LRO, LSTM_SEL). The encoder writes
 * them into the header, and the decoder reads them from it. Each process writes one JSON line to stderr: init
 * seconds (model allocation and LSTM initialisation), coding seconds, and parameter count.
 *
 * Determinism: the LSTM uses binary32 arithmetic in a fixed order, compiled with -ffp-contract=off and without
 * -ffast-math. kexp() is a local exp built from additions, multiplications and exponent-bit assembly, with no libm
 * call. sqrtf and division are correctly rounded under IEEE 754. A same-binary round trip is therefore exact. A
 * cross-ISA contract would need the same op order on every ISA; see WIRE-CONTRACT.md. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "codecs/cm.c"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

/* ---- deterministic float helpers ---- */
static inline float kexp(float x) {               /* e^x for x in [-87, 87]: 2^k * poly(r), r = x - k ln2 */
    if (x > 87.f) x = 87.f; if (x < -87.f) x = -87.f;
    float t = x * 1.44269504f; int k = (int)(t >= 0 ? t + 0.5f : t - 0.5f);
    float r = x - (float)k * 0.693145752f; r = r - (float)k * 1.42860677e-6f;
    float p = 1.f + r * (1.f + r * (0.5f + r * (0.166666672f + r * (0.0416666679f + r * (0.00833333377f + r * 0.00138888892f)))));
    union { uint32_t u; float f; } s; s.u = (uint32_t)(k + 127) << 23; return p * s.f;
}
static inline float ksig(float x) { return 1.f / (1.f + kexp(-x)); }
static inline float ktanh(float x) { return 2.f * ksig(2.f * x) - 1.f; }

/* ---- LSTM: one layer, coupled input/forget gate (as in cmix/lstm-compress), softmax over 256 bytes ---- */
enum { NB = 256, TMAX = 64 };
typedef struct {
    int H, T, pos; float lr, lro; uint32_t rng; long steps;
    float *Wx, *Wh, *Wy;                 /* Wx[g][byte][H], Wh[g][H][H+1] (last column: bias), Wy[256][H+1] */
    float *mWx, *vWx, *mWh, *vWh, *gWx, *gWh; uint8_t touched[NB];
    float *h, *s;                        /* current hidden and cell state */
    /* window history: per step k */
    int xb[TMAX]; float *hp, *sp, *F, *C, *O, *S, *TS, *Hh, *dho;
    float probs[NB], tree[2 * NB];
} lstm_t;
static float lrand(lstm_t *L) { L->rng ^= L->rng << 13; L->rng ^= L->rng >> 17; L->rng ^= L->rng << 5; return (float)(L->rng >> 8) / 16777216.f * 2.f - 1.f; }
static size_t lstm_params(int H) { return (size_t)3 * NB * H + (size_t)3 * H * (H + 1) + (size_t)NB * (H + 1); }
static void lstm_forward(lstm_t *L, int b);
static lstm_t *lstm_new(int H, int T, float lr, float lro) {
    lstm_t *L = calloc(1, sizeof *L); L->H = H; L->T = T; L->lr = lr; L->lro = lro; L->rng = 0x9E3779B9u;
    size_t nx = (size_t)3 * NB * H, nh = (size_t)3 * H * (H + 1), ny = (size_t)NB * (H + 1), w = (size_t)T * H;
    L->Wx = calloc(nx, 4); L->mWx = calloc(nx, 4); L->vWx = calloc(nx, 4); L->gWx = calloc(nx, 4);
    L->Wh = calloc(nh, 4); L->mWh = calloc(nh, 4); L->vWh = calloc(nh, 4); L->gWh = calloc(nh, 4);
    L->Wy = calloc(ny, 4); L->h = calloc(H, 4); L->s = calloc(H, 4);
    L->hp = calloc(w, 4); L->sp = calloc(w, 4); L->F = calloc(w, 4); L->C = calloc(w, 4); L->O = calloc(w, 4);
    L->S = calloc(w, 4); L->TS = calloc(w, 4); L->Hh = calloc(w, 4); L->dho = calloc(w, 4);
    float r = 1.f / sqrtf((float)H + 1.f);
    for (size_t i = 0; i < nx; i++) L->Wx[i] = lrand(L) * r;
    for (size_t i = 0; i < nh; i++) L->Wh[i] = lrand(L) * r;
    for (int g = 0; g < 3; g++) for (int i = 0; i < H; i++) L->Wh[((size_t)g * H + i) * (H + 1) + H] = g == 0 ? 1.f : 0.f;  /* forget bias 1 */
    lstm_forward(L, 0);                  /* the prediction of the first byte (previous byte taken as 0) */
    return L;
}
static void lstm_tree(lstm_t *L) {
    for (int b = 0; b < NB; b++) L->tree[NB + b] = L->probs[b];
    for (int i = NB - 1; i >= 1; i--) L->tree[i] = L->tree[2 * i] + L->tree[2 * i + 1];
}
static void lstm_forward(lstm_t *L, int b) {
    const int H = L->H, k = L->pos; float *hp = L->hp + k * H, *sp = L->sp + k * H;
    memcpy(hp, L->h, H * 4); memcpy(sp, L->s, H * 4); L->xb[k] = b;
    float *F = L->F + k * H, *C = L->C + k * H, *O = L->O + k * H, *S = L->S + k * H, *TS = L->TS + k * H, *Hh = L->Hh + k * H;
    for (int g = 0; g < 3; g++) {
        const float *wx = L->Wx + ((size_t)g * NB + b) * H; float *dst = g == 0 ? F : g == 1 ? C : O;
        for (int i = 0; i < H; i++) {
            const float *w = L->Wh + ((size_t)g * H + i) * (H + 1); float z = wx[i] + w[H];
            for (int j = 0; j < H; j++) z += w[j] * hp[j];
            dst[i] = g == 1 ? ktanh(z) : ksig(z);
        }
    }
    for (int i = 0; i < H; i++) { S[i] = F[i] * sp[i] + (1.f - F[i]) * C[i]; TS[i] = ktanh(S[i]); Hh[i] = O[i] * TS[i]; }
    memcpy(L->h, Hh, H * 4); memcpy(L->s, S, H * 4);
    float mx = -1e30f;
    for (int j = 0; j < NB; j++) { const float *w = L->Wy + (size_t)j * (H + 1); float z = w[H]; for (int i = 0; i < H; i++) z += w[i] * Hh[i]; L->probs[j] = z; if (z > mx) mx = z; }
    float sum = 0; for (int j = 0; j < NB; j++) { L->probs[j] = kexp(L->probs[j] - mx); sum += L->probs[j]; }
    float inv = 1.f / sum; for (int j = 0; j < NB; j++) L->probs[j] = L->probs[j] * inv + 1e-6f;
    lstm_tree(L);
}
static inline void adam(float *w, float *m, float *v, float g, float lr) {
    if (g > 5.f) g = 5.f; if (g < -5.f) g = -5.f;
    *m = 0.9f * *m + 0.1f * g; *v = 0.999f * *v + 0.001f * g * g; *w -= lr * *m / (sqrtf(*v) + 1e-6f);
}
static void lstm_bptt(lstm_t *L) {
    const int H = L->H; float dhn[TMAX > 0 ? 256 : 1] = {0}, dsn[256] = {0}, zf[256], zc[256], zo[256];
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
            const float *z = g == 0 ? zf : g == 1 ? zc : zo; float *gx = L->gWx + ((size_t)g * NB + b) * H;
            for (int i = 0; i < H; i++) {
                gx[i] += z[i];
                float *gw = L->gWh + ((size_t)g * H + i) * (H + 1); const float *w = L->Wh + ((size_t)g * H + i) * (H + 1);
                for (int j = 0; j < H; j++) { gw[j] += z[i] * hp[j]; dhn[j] += w[j] * z[i]; }
                gw[H] += z[i];
            }
        }
    }
    const size_t nh = (size_t)3 * H * (H + 1);
    for (size_t i = 0; i < nh; i++) { adam(L->Wh + i, L->mWh + i, L->vWh + i, L->gWh[i], L->lr); L->gWh[i] = 0; }
    for (int g = 0; g < 3; g++) for (int b = 0; b < NB; b++) if (L->touched[b]) {
        size_t o = ((size_t)g * NB + b) * H;
        for (int i = 0; i < H; i++) { adam(L->Wx + o + i, L->mWx + o + i, L->vWx + o + i, L->gWx[o + i], L->lr); L->gWx[o + i] = 0; }
    }
    memset(L->touched, 0, sizeof L->touched);
}
/* byte b was the actual next byte: train the output layer, maybe run BPTT, then predict the following byte */
static void lstm_byte(lstm_t *L, int b) {
    const int H = L->H, k = L->pos; const float *Hh = L->Hh + k * H; float *dho = L->dho + k * H;
    for (int i = 0; i < H; i++) dho[i] = 0;
    for (int j = 0; j < NB; j++) {
        float dy = L->probs[j] - (j == b ? 1.f : 0.f); float *w = L->Wy + (size_t)j * (H + 1);
        for (int i = 0; i < H; i++) { dho[i] += w[i] * dy; w[i] -= L->lro * dy * Hh[i]; }
        w[H] -= L->lro * dy;
    }
    L->steps++;
    if (++L->pos == L->T) { lstm_bptt(L); L->pos = 0; }
    lstm_forward(L, b);
}
static inline int lstm_bitp(const lstm_t *L, int node) {   /* P(next bit = 1) in 1..4095, node = 1 followed by the bits so far */
    float a = L->tree[node], q = L->tree[2 * node + 1] / a; int p = (int)(q * 4096.f);
    return p < 1 ? 1 : p > 4095 ? 4095 : p;
}
static inline int lstm_conf(const lstm_t *L, int node) { float a = L->tree[node], c = a < 1e-9f ? 0 : (L->tree[2 * node + 1] > L->tree[2 * node] ? L->tree[2 * node + 1] : L->tree[2 * node]) / a; int q = (int)(c * 8.f); return q > 7 ? 7 : q; }

/* ---- final mixer: st(p_cm), st(p_lstm), bias, and with ext (lrm >> 4 = 1) also model 4's five internal mixer outputs ---- */
enum { FMAX = 8 };
typedef struct { int32_t w[2048][FMAX]; int x[FMAX], ni, ws, pr; int lr; } fmix_t;
static void fmix_init(fmix_t *f, int lrm) {
    f->lr = lrm & 15; f->ni = (lrm >> 4) == 1 ? 8 : 3;
    for (int i = 0; i < 2048; i++) for (int k = 0; k < FMAX; k++) f->w[i][k] = k == 0 ? 1 << 16 : 0;
}
static inline int fmix_p(fmix_t *f, cm_t *m, int pc, int pl, int set) {
    f->x[0] = m->st[pc]; f->x[1] = m->st[pl]; f->x[2] = 256; f->ws = set;
    if (f->ni == 8) { f->x[3] = m->st[m->pr1]; f->x[4] = m->st[m->pr2]; f->x[5] = m->st[m->pr3]; f->x[6] = m->st[m->pr4]; f->x[7] = m->st[m->prf]; }
    int64_t d = 0; for (int i = 0; i < f->ni; i++) d += (int64_t)f->w[set][i] * f->x[i];
    int s = (int)(d >> 16); if (s > 2047) s = 2047; if (s < -2047) s = -2047;
    int p = cm_SQ(m->sq, s); if (p < 1) p = 1; if (p > 4095) p = 4095; return f->pr = p;
}
static inline void fmix_upd(fmix_t *f, int y) {
    int err = ((y << 12) - f->pr) * f->lr;
    for (int i = 0; i < f->ni; i++) f->w[f->ws][i] += (int32_t)(((int64_t)f->x[i] * err) >> 14);
}

/* ---- I/O ---- */
static uint8_t *readf(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*n + 16); if (*n && fread(b, 1, *n, f) != *n) exit(2); fclose(f); return b;
}
static void writef(const char *p, const uint8_t *b, size_t n) { FILE *f = fopen(p, "wb"); if (!f || fwrite(b, 1, n, f) != n) exit(2); fclose(f); }
static int envi(const char *k, int d) { const char *v = getenv(k); return v && *v ? atoi(v) : d; }

enum { HDR = 4 + 4 + 8 + 8 };   /* magic | n | H T lr(u16) lro(u16) sel lrm | bb b0 b1 b2 mask(u32) */
static int sel_of(int sel, const lstm_t *L, cm_t *m, int node, int bp) {
    (void)m;
    switch (sel) { case 1: return node; case 2: return lstm_conf(L, node) * 8 + bp; default: return lstm_conf(L, node) * 256 + node; }
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: lstm_h enc0|dec0|enc|dec IN OUT\n"); return 2; }
    const char *mode = argv[1]; size_t n; double t0 = now(); uint8_t *in = readf(argv[2], &n);
    const uint32_t mask = CM4_MASK_TEXT;
    if (!strcmp(mode, "enc0")) {
        size_t cap = n + n / 16 + 64; uint8_t *out = malloc(cap + 4); memcpy(out, "KLS0", 4);
        double t1 = now(); size_t z = cm_compress_tm(in, n, out + 4, cap, 4, 0, cm4_tune, mask); double t2 = now();
        if (!z) return 1; writef(argv[3], out, z + 4);
        fprintf(stderr, "{\"mode\":\"enc0\",\"n\":%zu,\"z\":%zu,\"read_s\":%.6f,\"code_s\":%.6f}\n", n, z + 4, t1 - t0, t2 - t1); return 0;
    }
    if (!strcmp(mode, "dec0")) {
        if (n < 8 || memcmp(in, "KLS0", 4)) return 1;
        uint32_t n32; memcpy(&n32, in + 4, 4); size_t on = n32 & 0x0FFFFFFFu; uint8_t *out = malloc(on + 8);
        double t1 = now(); int r = cm_decompress(in + 4, n - 4, out, on); double t2 = now();
        if (r) return 1; writef(argv[3], out, on);
        fprintf(stderr, "{\"mode\":\"dec0\",\"n\":%zu,\"read_s\":%.6f,\"code_s\":%.6f}\n", on, t1 - t0, t2 - t1); return 0;
    }
    int enc = !strcmp(mode, "enc"); if (!enc && strcmp(mode, "dec")) return 2;
    int H, T, lr, lro, sel, lrm, bb; size_t N; cm_tune_t tune = cm4_tune; uint8_t *buf; uint8_t *out; size_t cap = 0;
    if (enc) {
        N = n; H = envi("LSTM_H", 32); T = envi("LSTM_T", 10); lr = envi("LSTM_LR", 20); lro = envi("LSTM_LRO", 20); sel = envi("LSTM_SEL", 0); lrm = envi("LSTM_LRM", 2);
        if (H < 1 || H > 255 || T < 1 || T > TMAX || N >= ((size_t)1 << 28)) return 2;
        bb = cm4_shift_for(N, mask); if (tune.b2) tune.b2 = cm4_prior_for(mask);
        cap = N + N / 8 + 1024; out = malloc(cap); buf = malloc(N + 8);
        memcpy(out, "KLS1", 4); uint32_t n32 = (uint32_t)N; memcpy(out + 4, &n32, 4);
        out[8] = (uint8_t)H; out[9] = (uint8_t)T; out[10] = (uint8_t)lr; out[11] = (uint8_t)(lr >> 8); out[12] = (uint8_t)lro; out[13] = (uint8_t)(lro >> 8); out[14] = (uint8_t)sel; out[15] = (uint8_t)lrm;
        out[16] = (uint8_t)bb; out[17] = tune.b0; out[18] = tune.b1; out[19] = tune.b2; for (int k = 0; k < 4; k++) out[20 + k] = (uint8_t)(mask >> 8 * k);
    } else {
        if (n < HDR + 4 || memcmp(in, "KLS1", 4)) return 1;
        uint32_t n32; memcpy(&n32, in + 4, 4); N = n32; H = in[8]; T = in[9]; lr = in[10] | in[11] << 8; lro = in[12] | in[13] << 8; sel = in[14]; lrm = in[15];
        bb = in[16]; tune.b0 = in[17]; tune.b1 = in[18]; tune.b2 = in[19];
        if (H < 1 || T < 1 || T > TMAX || N >= ((size_t)1 << 28)) return 1;
        out = malloc(N + 8); buf = out;
    }
    double t1 = now();
    cm_t *m = cm_new_xm(buf, bb, 4, cm4_chain(N), 1, mask, N); if (!m) return 1; cm_set_tune(m, tune);
    lstm_t *L = lstm_new(H, T, lr * 1e-4f, lro * 1e-4f);
    fmix_t *F = malloc(sizeof *F); fmix_init(F, lrm);
    double t2 = now();
    const int pass = enc && envi("LSTM_PASS", 0); double bc = 0, bl = 0, bf = 0;   /* pass: control, code with model 4 alone */
    cm_ac ac = {0, 0xffffffffu}; size_t o = HDR; uint32_t code = 0; size_t ip = HDR;
    if (!enc) for (int k = 0; k < 4; k++) code = (code << 8) | (ip < n ? in[ip++] : 0xffu);
    for (size_t i = 0; i < N; i++) {
        int c = enc ? in[i] : 0, node = 1;
        for (int b = 7; b >= 0; b--) {
            int pc = cm_predict(m, 4), pl = lstm_bitp(L, node), p = fmix_p(F, m, pc, pl, sel_of(sel, L, m, node, 7 - b));
            if (pass) p = pc;
            uint32_t mid = cm_ac_mid(&ac, p); int y;
            if (enc) { y = (c >> b) & 1; bc -= log2((y ? pc : 4096 - pc) / 4096.0); bl -= log2((y ? pl : 4096 - pl) / 4096.0); bf -= log2((y ? p : 4096 - p) / 4096.0); } else y = code <= mid;
            if (y) ac.high = mid; else ac.low = mid + 1;
            cm_update(m, y, 4); fmix_upd(F, y); node = node * 2 + y;
            for (; cm_ac_settled(&ac); cm_ac_shift(&ac)) {
                if (enc) { if (o >= cap) return 1; out[o++] = (uint8_t)(ac.high >> 24); }
                else code = (code << 8) | (ip < n ? in[ip++] : 0xffu);
            }
        }
        if (!enc) out[i] = (uint8_t)(node & 255);
        lstm_byte(L, node & 255);
    }
    double t3 = now();
    if (enc) { out[o++] = (uint8_t)(ac.low >> 24); out[o++] = 0xff; out[o++] = 0xff; out[o++] = 0xff; writef(argv[3], out, o); }
    else writef(argv[3], out, N);
    fprintf(stderr, "{\"mode\":\"%s\",\"n\":%zu,\"z\":%zu,\"H\":%d,\"T\":%d,\"lr\":%d,\"lro\":%d,\"sel\":%d,\"lrm\":%d,\"params\":%zu,\"read_s\":%.6f,\"init_s\":%.6f,\"code_s\":%.6f,\"bytes_cm\":%.0f,\"bytes_lstm\":%.0f,\"bytes_final\":%.0f}\n",
            mode, N, enc ? o : n, H, T, lr, lro, sel, lrm, lstm_params(H), t1 - t0, t2 - t1, t3 - t2, bc / 8, bl / 8, bf / 8);
    return 0;
}
