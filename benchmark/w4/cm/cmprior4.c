/* Research harness of the w4 CM lane: trains model 4's shared priors (src/codecs/cm_prior.inc, cm_prior4).
 *   cmprior4 t|b [ITER] FILES...   codes each file with model 4 (text or binary contexts), from the flat start (1/4 per
 * weight) or, with ITER > 0, from the prior trained so far; prints the mean trained row of each of the five mixers over
 * the rows that were used, averaged over the files, in the model-3 input layout of cm_prior (weights in 16.16 / 4). */
#include "../../../src/kolmo.c"
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
static double acc[5][CM_NIX3]; static int cnt[5][CM_NIX3];
static int16_t cur[5][CM_NIX3];
int main(int argc, char **argv) {
    if (argc < 4) return 2;
    const int bin = argv[1][0] == 'b'; const uint32_t mask = bin ? CM4_MASK_BIN : CM4_MASK_TEXT; int iters = atoi(argv[2]);
    for (int k = 0; k < 5; k++) for (int j = 0; j < CM_NIX3; j++) cur[k][j] = k < 4 ? 4096 : j < 4 ? 4096 : 0;
    for (int it = 0; it <= iters; it++) {
        memset(acc, 0, sizeof acc); memset(cnt, 0, sizeof cnt); size_t tot = 0;
        for (int a = 3; a < argc; a++) {
            size_t n; uint8_t *s = slurp(argv[a], &n); if (!s) return 1;
            uint8_t *hist = malloc(n + 8), *out = malloc(n + n / 8 + 1024);
            cm_t *m = cm_new_xm(hist, cm4_shift_for(n, mask), 4, (n + 8) * 4, 1, mask, n);
            cm_tune_t tu = cm4_tune; tu.b2 = it > 0; cm_prior_override = it > 0 ? (const int16_t (*)[CM_NIX3])cur : NULL;
            cm_set_tune(m, tu);
            size_t o = 4; cm_enc_run(m, s, n, out, n + n / 8 + 1024, &o, 4); tot += o;
            const int nm = m->nmod; int jm[CM_NIX3]; for (int j = 0; j < CM_NIX3; j++) jm[j] = -1;
            for (int k = 0; k < nm; k++) { jm[k] = m->mid[k]; jm[nm + 3 + k] = CM_NM3 + 3 + m->mid[k]; }
            for (int k = 0; k < 3; k++) jm[nm + k] = CM_NM3 + k;
            int16_t (*rows[4])[CM4_NIP] = {m->q1, m->q2, m->q3, m->q4}; const int nr[4] = {CM_W1, CM_W2, CM_W3, CM_W4}, off[4] = {0, CM_W1, CM_W1 + CM_W2, CM_W1 + CM_W2 + CM_W3};
            for (int k = 0; k < 4; k++) {
                double sum[CM4_NIP] = {0}; long used = 0;
                for (int r = 0; r < nr[k]; r++) { if (!m->rdy[off[k] + r]) continue; used++; for (int j = 0; j < CM4_NIP; j++) sum[j] += rows[k][r][j]; }
                if (used) for (int j = 0; j < CM_NIX3; j++) if (jm[j] >= 0) { acc[k][jm[j]] += sum[j] / used * 4; cnt[k][jm[j]]++; }   /* 4096 = 1.0 -> 16.16 / 4 */
            }
            { double sum[5] = {0}; long used = 0;
              for (int r = 0; r < CM_WF; r++) { if (m->wf[r][0] == 1 << 14 && m->wf[r][1] == 1 << 14 && m->wf[r][4] == 0) continue; used++; for (int j = 0; j < 5; j++) sum[j] += m->wf[r][j]; }
              if (used) for (int j = 0; j < 5; j++) { acc[4][j] += sum[j] / used / 4; cnt[4][j]++; } }
            cm_free(m); free(hist); free(out); free(s);
        }
        fprintf(stderr, "iteration %d (%s prior): %zu bytes\n", it, it ? "trained" : "flat", tot);
        for (int k = 0; k < 5; k++) for (int j = 0; j < CM_NIX3; j++) if (cnt[k][j]) cur[k][j] = (int16_t)lround(acc[k][j] / cnt[k][j]);
    }
    printf("  {");
    for (int k = 0; k < 5; k++) { printf("%s{", k ? "   " : ""); for (int j = 0; j < CM_NIX3; j++) printf("%d%s", cur[k][j], j + 1 < CM_NIX3 ? ", " : ""); printf("}%s\n", k < 4 ? "," : "}"); }
    return 0;
}
