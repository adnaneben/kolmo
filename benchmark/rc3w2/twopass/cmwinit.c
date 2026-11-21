/* Research harness of the twopass lane, question 3: mixer weight initialisation.
 *   cmwinit FILE MODEL [PRIOR_FILE]
 * Codes FILE with codec 7 (model 2 or 3) three or four ways:
 *   default    every mixer row starts at 1/4 per input (what every stream does)
 *   twopass    pass 1 codes the file; the mean trained row of each of the five mixers (the rows that were used) is
 *              quantised to 8 bits and pass 2 starts every row of that mixer from it; the header cost (5 x 63 bytes)
 *              is added
 *   oracle16   the same with 16-bit means (no quantisation loss), header cost 2 x 5 x 63 bytes
 *   prior      (with PRIOR_FILE) the means trained on another file, no header cost: a shared prior shipped in the code
 * Sizes only (the decoder would initialise the same way; nothing else changes). */
#include "../../../src/kolmo.c"
#include <time.h>
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
enum { NI = CM_NIX3 };
typedef struct { double w[5][NI]; } means_t;
static void get_means(cm_t *m, means_t *mm) {
    int32_t (*rows[4])[NI] = {m->w1, m->w2, m->w3, m->w4}; size_t nr[4] = {CM_W1, CM_W2, CM_W3, CM_W4};
    for (int k = 0; k < 4; k++) {
        double sum[NI] = {0}; size_t used = 0;
        for (size_t r = 0; r < nr[k]; r++) {
            int u = 0; for (int j = 0; j < NI; j++) if (rows[k][r][j] != 1 << 14) { u = 1; break; }
            if (!u) continue; used++; for (int j = 0; j < NI; j++) sum[j] += rows[k][r][j];
        }
        for (int j = 0; j < NI; j++) mm->w[k][j] = used ? sum[j] / used : 1 << 14;
    }
    double sum[5] = {0}; size_t used = 0;
    for (int r = 0; r < CM_WF; r++) { if (m->wf[r][0] == 1 << 14 && m->wf[r][1] == 1 << 14 && m->wf[r][4] == 0) continue; used++; for (int j = 0; j < 5; j++) sum[j] += m->wf[r][j]; }
    for (int j = 0; j < NI; j++) mm->w[4][j] = j < 5 ? (used ? sum[j] / used : (j < 4 ? 1 << 14 : 0)) : 0;
}
/* quantise: 8 bits per weight, step 2^10 (weights of 16.16 lie within about +-2^17); bits = 0 keeps them exact */
static void set_means(cm_t *m, const means_t *mm, int bits) {
    int32_t (*rows[4])[NI] = {m->w1, m->w2, m->w3, m->w4}; size_t nr[4] = {CM_W1, CM_W2, CM_W3, CM_W4};
    for (int k = 0; k < 5; k++) for (int j = 0; j < NI; j++) {
        double v = mm->w[k][j]; int32_t q;
        if (bits == 8) { long t = lround(v / 1024.0); if (t < -128) t = -128; if (t > 127) t = 127; q = (int32_t)(t * 1024); }
        else q = (int32_t)lround(v);
        if (k < 4) for (size_t r = 0; r < nr[k]; r++) rows[k][r][j] = q;
        else if (j < 5) for (int r = 0; r < CM_WF; r++) m->wf[r][j] = q;
    }
}
static size_t code(const uint8_t *in, size_t n, int model, const means_t *init, int bits, means_t *out_means) {
    int v2 = model == 3 ? 3 : 1, bb = cm_bits_for(n);
    uint8_t *hist = malloc(n + 8); cm_t *m = cm_new_x(hist, bb, v2, v2 == 3 ? (n + 8) * 4 : 0, 1);
    if (init) set_means(m, init, bits);
    size_t cap = n + n / 8 + 1024, o = 4; uint8_t *out = malloc(cap);
    int r = cm_enc_run(m, in, n, out, cap, &o, v2);
    if (out_means) get_means(m, out_means);
    cm_free(m); free(hist); free(out);
    return r ? 0 : o + 1;
}
int main(int argc, char **argv) {
    if (argc < 3) return 1;
    size_t n; uint8_t *s = slurp(argv[1], &n); int model = atoi(argv[2]);
    means_t mm, pm; size_t z0 = code(s, n, model, NULL, 0, &mm);
    size_t z8 = code(s, n, model, &mm, 8, NULL) + 5 * NI, z16 = code(s, n, model, &mm, 0, NULL) + 2 * 5 * NI;
    printf("%s model=%d default=%zu twopass8=%zu (%+.3f%%) twopass16=%zu (%+.3f%%)", argv[1], model, z0, z8, 100.0 * ((double)z8 - z0) / z0, z16, 100.0 * ((double)z16 - z0) / z0);
    if (argc > 3) {
        size_t pn; uint8_t *ps = slurp(argv[3], &pn); code(ps, pn, model, NULL, 0, &pm); free(ps);
        size_t zp = code(s, n, model, &pm, 0, NULL) + 1;
        printf(" prior=%zu (%+.3f%%)", zp, 100.0 * ((double)zp - z0) / z0);
    }
    printf("\n"); return 0;
}
