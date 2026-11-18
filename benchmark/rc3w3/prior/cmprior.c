/* Research harness of the rc3w3 prior lane: per-type warm starts for codec 7 (model 3, the -x text model).
 *   cmprior snap TRAIN OUT.snap              codes TRAIN (default rates) and writes every table of the model that does
 *                                            not depend on the hash of a context (mixers, APMs, state maps, match-model
 *                                            counters, order-1 direct table); the hashed tables are not kept
 *   cmprior eval FILE MASK CLAMP SNAP [TUNE] [PRIMER K]
 *                                            codes FILE with model 3 starting from the parts of SNAP in MASK, counts of
 *                                            adaptive counters clamped to CLAMP; TUNE as cmrace (hex b0b1b2, default
 *                                            000000); PRIMER: the model first learns the last K bytes of PRIMER (no
 *                                            output: a dictionary for context mixing). Prints the size; round trip checked.
 * MASK bits: 1 mixers w1 w2 w3 wf (every row), 2 mean rows of every mixer (the twopass method, per type),
 *            4 APM a1, 8 APMs a2 a3, 16 state maps, 32 match-model counters, 64 order-1 direct table, 128 mixer w4.
 * Build: benchmark/rc3w3/prior/build.sh cmprior (after make). */
#include "../../../src/kolmo.c"
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
enum { NI = CM_NIX4 };
typedef struct {
    int32_t w1[CM_W1][NI], w2[CM_W2][NI], w3[CM_W3][NI], wf[CM_WF][5], mean[5][NI];
    uint16_t a1[256][33];
    uint32_t sm[CM_NM4][256 * 8], mc[64], o1[65536];
} snap_t;
static snap_t *SN; static uint16_t (*SA2)[33], (*SA3)[33]; static int32_t (*SW4)[NI];
static void snap_take(cm_t *m) {
    SN = calloc(1, sizeof *SN); SA2 = malloc(sizeof(uint16_t) * 33 * 65536); SA3 = malloc(sizeof(uint16_t) * 33 * CM_A3); SW4 = malloc(sizeof(int32_t) * NI * CM_W4);
    memcpy(SN->w1, m->w1, sizeof SN->w1); memcpy(SN->w2, m->w2, sizeof SN->w2); memcpy(SN->w3, m->w3, sizeof SN->w3); memcpy(SN->wf, m->wf, sizeof SN->wf);
    memcpy(SW4, m->w4, sizeof(int32_t) * NI * CM_W4);
    memcpy(SN->a1, m->a1, sizeof SN->a1); memcpy(SA2, m->a2, sizeof(uint16_t) * 33 * 65536); memcpy(SA3, m->a3, sizeof(uint16_t) * 33 * CM_A3);
    memcpy(SN->sm, m->sm, sizeof SN->sm); memcpy(SN->mc, m->mc, sizeof SN->mc); memcpy(SN->o1, m->o1, sizeof SN->o1);
    /* mean trained row of each mixer (rows that moved from their start), as cmrace train */
    int32_t (*rows[4])[NI] = {m->w1, m->w2, m->w3, m->w4}; size_t nr[4] = {CM_W1, CM_W2, CM_W3, CM_W4};
    for (int k = 0; k < 5; k++) {
        double sum[NI] = {0}; size_t used = 0; const int nj = k < 4 ? NI : 5;
        for (size_t r = 0; r < (k < 4 ? nr[k] : CM_WF); r++) {
            int u = 0; for (int j = 0; j < nj; j++) { int32_t w = k < 4 ? rows[k][r][j] : m->wf[r][j]; if (w != (k == 4 && j == 4 ? 0 : 1 << 14)) { u = 1; break; } }
            if (!u) continue; used++;
            for (int j = 0; j < nj; j++) sum[j] += k < 4 ? rows[k][r][j] : m->wf[r][j];
        }
        for (int j = 0; j < NI; j++) SN->mean[k][j] = j >= nj ? 0 : used ? (int32_t)lround(sum[j] / used) : (k == 4 && j == 4 ? 0 : 1 << 14);
    }
}
static int MODEL = 3, MASK, CLAMP = 1023, QW, QS = 4, QP;
/* a probability of `bits` bits through stretch, rounded to QP bits over [-2047, 2047], and back */
static uint32_t qp(uint32_t p, int bits) {
    int p12 = (int)(p >> (bits - 12)); if (p12 < 1) p12 = 1; if (p12 > 4095) p12 = 4095;
    int x = cm_stx[p12], st = 4096 >> QP; int q = (int)lround((double)x / st) * st; if (q > 2047) q = 2047; if (q < -2047) q = -2047;
    int r = cm_sqx[q + 2048]; return (uint32_t)r << (bits - 12);
}
/* MAP: the snapshot is model 3's; give model 4 the same rows with its two extra models at their start values */
static void snap_map(void) {
    static int done; if (done) return; done = 1;
    const int nm = CM_NM4, ni = 2 * nm + 3;
    int32_t (*w[3])[NI] = {SN->w1, SN->w2, SN->w3}; int rows[3] = {CM_W1, CM_W2, CM_W3};
    for (int k = 0; k < 3; k++) for (int r = 0; r < rows[k]; r++) {
        int32_t t[NI]; memcpy(t, w[k][r], sizeof t);
        for (int j = 0; j < ni; j++) { int jp = j < CM_NM3 ? j : j < nm ? -1 : j - nm + CM_NM3 < CM_NIX3 ? j - nm + CM_NM3 : -1; w[k][r][j] = jp < 0 ? 1 << 14 : t[jp]; }
    }
    for (int k = 0; k < 4; k++) { int32_t t[NI]; memcpy(t, SN->mean[k], sizeof t);
        for (int j = 0; j < ni; j++) { int jp = j < CM_NM3 ? j : j < nm ? -1 : j - nm + CM_NM3 < CM_NIX3 ? j - nm + CM_NM3 : -1; SN->mean[k][j] = jp < 0 ? 1 << 14 : t[jp]; } }
    for (int i = CM_NM3; i < CM_NM4; i++) for (int j = 0; j < 2048; j++) {
        int st = j & 255; uint32_t n0 = st < bh_ns ? bh_n0[st] : 0, n1 = st < bh_ns ? bh_n1[st] : 0;
        SN->sm[i][j] = (uint32_t)(((2ull * n1 + 1) << 22) / (2ull * (n0 + n1) + 2)) << 10;
    }
}
static void snap_apply(cm_t *m) {
    if (!SN || !MASK) return;
    if (getenv("MAP") && m->v2 == 4) snap_map();
    if (MASK & 2) {
        for (int j = 0; j < NI; j++) {
            for (int i = 0; i < CM_W1; i++) m->w1[i][j] = SN->mean[0][j];
            for (int i = 0; i < CM_W2; i++) m->w2[i][j] = SN->mean[1][j];
            for (int i = 0; i < CM_W3; i++) m->w3[i][j] = SN->mean[2][j];
            for (int i = 0; i < CM_W4; i++) m->w4[i][j] = SN->mean[3][j];
        }
        for (int i = 0; i < CM_WF; i++) for (int j = 0; j < 5; j++) m->wf[i][j] = SN->mean[4][j];
    }
    if (MASK & (1 | 256)) memcpy(m->w1, SN->w1, sizeof SN->w1);
    if (MASK & (1 | 512)) memcpy(m->w2, SN->w2, sizeof SN->w2);
    if (MASK & (1 | 1024)) memcpy(m->w3, SN->w3, sizeof SN->w3);
    if (MASK & (1 | 2048)) memcpy(m->wf, SN->wf, sizeof SN->wf);
    if (QW) {   /* weights quantised to QW bits with a step of QS (16.16 units), as a compiled-in table would hold them */
        int32_t *t[4] = {&m->w1[0][0], &m->w2[0][0], &m->w3[0][0], &m->wf[0][0]}; size_t c[4] = {CM_W1 * NI, CM_W2 * NI, CM_W3 * NI, CM_WF * 5};
        const int32_t lim = (1 << (QW - 1)) - 1;
        for (int k = 0; k < 4; k++) for (size_t i = 0; i < c[k]; i++) { int32_t q = (int32_t)lround((double)t[k][i] / QS); if (q > lim) q = lim; if (q < -lim) q = -lim; t[k][i] = q * QS; }
    }
    if (QP) {   /* probabilities (state maps, APM a1, order-1 table) quantised to QP bits in the stretched domain */
        for (int i = 0; i < CM_NM4; i++) for (int j = 0; j < 2048; j++) { uint32_t v = m->sm[i][j]; m->sm[i][j] = (uint32_t)qp(v >> 10, 22) << 10 | (v & 1023); }
        for (int i = 0; i < 256; i++) for (int j = 0; j < 33; j++) m->a1[i][j] = (uint16_t)qp(m->a1[i][j], 16);
        for (int i = 0; i < 65536; i++) { uint32_t v = m->o1[i]; m->o1[i] = (uint32_t)qp(v >> 16, 16) << 16 | (v & 0xffff); }
    }
    if (MASK & 128) memcpy(m->w4, SW4, sizeof(int32_t) * NI * CM_W4);
    if (MASK & 4) memcpy(m->a1, SN->a1, sizeof SN->a1);
    if (MASK & 8) { memcpy(m->a2, SA2, sizeof(uint16_t) * 33 * 65536); memcpy(m->a3, SA3, sizeof(uint16_t) * 33 * CM_A3); }
    if (MASK & 16) for (int i = 0; i < CM_NM4; i++) for (int j = 0; j < 2048; j++) {
        uint32_t v = SN->sm[i][j], n = v & 1023; if (n > (uint32_t)CLAMP) n = (uint32_t)CLAMP; m->sm[i][j] = (v & ~1023u) | n; }
    if (MASK & 32) memcpy(m->mc, SN->mc, sizeof SN->mc);
    if (MASK & 64) for (int i = 0; i < 65536; i++) { uint32_t v = SN->o1[i], n = v & 15; if (n > (uint32_t)(CLAMP < 15 ? CLAMP : 15)) n = (uint32_t)(CLAMP < 15 ? CLAMP : 15); m->o1[i] = (v & ~15u) | n; }
}
static int write_snap(const char *p) {
    FILE *f = fopen(p, "wb"); if (!f) return -1;
    fwrite(SN, sizeof *SN, 1, f); fwrite(SA2, sizeof(uint16_t) * 33, 65536, f); fwrite(SA3, sizeof(uint16_t) * 33, CM_A3, f); fwrite(SW4, sizeof(int32_t) * NI, CM_W4, f);
    fclose(f); return 0;
}
static int read_snap(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return -1;
    SN = malloc(sizeof *SN); SA2 = malloc(sizeof(uint16_t) * 33 * 65536); SA3 = malloc(sizeof(uint16_t) * 33 * CM_A3); SW4 = malloc(sizeof(int32_t) * NI * CM_W4);
    size_t r = fread(SN, sizeof *SN, 1, f) + fread(SA2, sizeof(uint16_t) * 33, 65536, f) + fread(SA3, sizeof(uint16_t) * 33, CM_A3, f) + fread(SW4, sizeof(int32_t) * NI, CM_W4, f);
    fclose(f); return r == 1 + 65536 + CM_A3 + CM_W4 ? 0 : -1;
}
/* model 3 with exact tables and the given tune, its buffer holding the primer first (pk bytes) */
static cm_t *mk(uint8_t *hist, size_t n, cm_tune_t t, const uint8_t *pr, size_t pk) {
    int bb = cm_bits_for(n + pk);
    cm_t *m = cm_new_x(hist, bb, MODEL, (n + pk + 8) * 4, 1);
    cm_set_tune(m, t); snap_apply(m);
    if (pk) { size_t cap = pk + pk / 8 + 1024, o = 0; uint8_t *junk = malloc(cap); cm_enc_run(m, pr, pk, junk, cap, &o, MODEL); free(junk); }
    return m;
}
int main(int argc, char **argv) {
    if (argc >= 4 && !strcmp(argv[1], "snap")) {
        size_t n; uint8_t *s = slurp(argv[2], &n); if (!s) return 1;
        uint8_t *hist = malloc(n + 8); cm_t *m = cm_new_x(hist, cm_bits_for(n), 3, (n + 8) * 4, 1);
        size_t cap = n + n / 8 + 1024, o = 4; uint8_t *out = malloc(cap); double t0 = now(); cm_enc_run(m, s, n, out, cap, &o, MODEL);
        fprintf(stderr, "trained %s: %zu -> %zu in %.1fs\n", argv[2], n, o, now() - t0);
        snap_take(m); return write_snap(argv[3]) ? 1 : 0;
    }
    if (argc >= 6 && !strcmp(argv[1], "eval")) {
        size_t n; uint8_t *s = slurp(argv[2], &n); if (!s) return 1;
        MASK = (int)strtol(argv[3], 0, 0); CLAMP = atoi(argv[4]); if (getenv("QW")) QW = atoi(getenv("QW")); if (getenv("QS")) QS = atoi(getenv("QS")); if (getenv("QP")) QP = atoi(getenv("QP")); if (getenv("MODEL")) MODEL = atoi(getenv("MODEL"));
        if (strcmp(argv[5], "-") && read_snap(argv[5])) { fprintf(stderr, "bad snap\n"); return 1; }
        cm_tune_t t = {0, 0, 0};
        if (argc > 6) { unsigned v = (unsigned)strtoul(argv[6], 0, 16); t = (cm_tune_t){(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; }
        uint8_t *pr = NULL; size_t pk = 0;
        if (argc > 8) { size_t pn; uint8_t *p = slurp(argv[7], &pn); pk = (size_t)strtoull(argv[8], 0, 0); if (pk > pn) pk = pn; pr = p + pn - pk; }
        uint8_t *hist = malloc(n + pk + 8); if (pk) memcpy(hist, pr, pk);
        double t0 = now(); cm_t *m = mk(hist, n, t, pr, pk); double tp = now() - t0;
        size_t cap = n + n / 8 + 1024, o = 4; uint8_t *out = malloc(cap);
        t0 = now(); cm_enc_run(m, s, n, out, cap, &o, MODEL); double te = now() - t0; cm_free(m);
        /* decode: same start, then the decoder's loop writes into the buffer after the primer */
        uint8_t *h2 = malloc(n + pk + 8); if (pk) memcpy(h2, pr, pk);
        t0 = now(); cm_t *d = mk(h2, n, t, pr, pk); cm_dec_run(d, out, o, NULL, n, MODEL); double td = now() - t0; cm_free(d);
        if (memcmp(h2 + pk, s, n)) { fprintf(stderr, "ROUND TRIP FAILED %s\n", argv[2]); return 2; }
        printf("%s mask=%d clamp=%d size=%zu setup=%.3f enc=%.3f dec=%.3f\n", argv[2], MASK, CLAMP, o - 4, tp, te, td);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "export")) {   /* export SNAP OUT.bin: the quantised tables a compiled-in prior would hold */
        if (read_snap(argv[2])) return 1;
        if (getenv("MODEL")) MODEL = atoi(getenv("MODEL"));
        const int nm = CM_NMOD(MODEL), ni = 2 * nm + 3, T = getenv("TRANSPOSE") ? 1 : 0;
        pthread_once(&cm_once, cm_init_tables);
        FILE *f = fopen(argv[3], "wb");
        int32_t *t[4] = {&SN->w1[0][0], &SN->w2[0][0], &SN->w3[0][0], &SN->wf[0][0]}; int rows[4] = {CM_W1, CM_W2, CM_W3, CM_WF}, cols[4] = {NI, NI, NI, 5}, use[4] = {ni, ni, ni, 5};
        for (int k = 0; k < 4; k++) {
            if (T) { for (int j = 0; j < use[k]; j++) for (int r = 0; r < rows[k]; r++) { long q = lround(t[k][(size_t)r * cols[k] + j] / 512.0); if (q > 127) q = 127; if (q < -127) q = -127; fputc((int)(int8_t)q, f); } }
            else for (int r = 0; r < rows[k]; r++) for (int j = 0; j < use[k]; j++) { long q = lround(t[k][(size_t)r * cols[k] + j] / 512.0); if (q > 127) q = 127; if (q < -127) q = -127; fputc((int)(int8_t)q, f); }
        }
        for (int i = 0; i < nm; i++) for (int b = 0; b < 8; b++) for (int st = 0; st < bh_ns; st++) {
            uint32_t p = SN->sm[i][b << 8 | st] >> 10; int p12 = (int)(p >> 10); if (p12 < 1) p12 = 1; if (p12 > 4095) p12 = 4095;
            fputc((int)lround(cm_stx[p12] / 64.0) + 32, f); }
        for (int i = 0; i < 65536; i++) { uint32_t v = SN->o1[i]; int p12 = (int)(v >> 20); if (p12 < 1) p12 = 1; if (p12 > 4095) p12 = 4095; fputc((int)lround(cm_stx[p12] / 64.0) + 32, f); fputc((int)(v >> 8 & 255), f); }
        for (int i = 0; i < 64; i++) { int p12 = (int)(SN->mc[i] >> 4); if (p12 < 1) p12 = 1; if (p12 > 4095) p12 = 4095; fputc((int)lround(cm_stx[p12] / 64.0) + 32, f); }
        fclose(f); return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "blob")) {   /* blob SNAP OUT.bin: a model-3 snapshot as the compiled-in prior holds it (cm.c, cm_kprior_unpack) */
        if (read_snap(argv[2])) return 1;
        pthread_once(&cm_once, cm_init_tables);
        FILE *f = fopen(argv[3], "wb");
        const int ni = CM_NIX3;
#define QW5(v) do { long q_ = lround((double)(v) / 8192.0); if (q_ > 7) q_ = 7; if (q_ < -7) q_ = -7; fputc((int)(int8_t)q_ & 255, f); } while (0)   /* 4 bits: steps of 1/8 */
        for (int r = 0; r < CM_W1; r++) for (int j = 0; j < ni; j++) QW5(SN->w1[r][j]);
        for (int r = 0; r < CM_W2; r++) for (int j = 0; j < ni; j++) QW5(SN->w2[r][j]);
        for (int r = 0; r < CM_W3; r++) for (int j = 0; j < ni; j++) QW5(SN->w3[r][j]);
        for (int j = 0; j < ni; j++) QW5(SN->mean[3][j]);
        for (int r = 0; r < CM_WF; r++) for (int j = 0; j < 5; j++) QW5(SN->wf[r][j]);
        /* a probability: stretch rounded to a multiple of 256 (17 levels, 0..16) | count class << 5 */
#define QP4(p12, cls) do { int p_ = (p12) < 1 ? 1 : (p12) > 4095 ? 4095 : (p12); long q_ = lround(cm_stx[p_] / 256.0) + 8; if (q_ < 0) q_ = 0; if (q_ > 16) q_ = 16; fputc((int)q_ | (cls) << 5, f); } while (0)
        for (int i = 0; i < CM_NM3; i++) for (int b = 0; b < 8; b++) for (int st = 0; st < bh_ns; st++) {
            uint32_t v = SN->sm[i][b << 8 | st], n = v & 1023; QP4((int)(v >> 20), n < 2 ? 0 : n < 8 ? 1 : n < 30 ? 2 : 3); }
        for (int i = 0; i < 65536; i++) { uint32_t v = SN->o1[i], n = v & 15; QP4((int)(v >> 20), n < 1 ? 0 : n < 4 ? 1 : n < 10 ? 2 : 3); }
        for (int i = 0; i < 65536; i++) fputc((int)(SN->o1[i] >> 8 & 255), f);
        for (int i = 0; i < 64; i++) { int p12 = (int)(SN->mc[i] >> 4); if (p12 < 1) p12 = 1; if (p12 > 4095) p12 = 4095; fputc((int)lround(cm_stx[p12] / 64.0) + 32, f); }
        fclose(f); return 0;
    }
    fprintf(stderr, "usage: see the header of cmprior.c\n"); return 1;
}
