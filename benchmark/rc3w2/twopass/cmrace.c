/* Research harness of the twopass lane: shared priors and the preset race for codec 7 (models 2 and 3).
 *   cmrace train MODEL OUT.inc FILES...   codes each file, averages the mean trained row of each mixer over the files
 *                                         (each file counts once) and writes the prior as C (weights / 4)
 *   cmrace eval FILE MODEL TUNE [PRIOR.inc] TUNE = b0b1b2 in hex (e.g. 000001); the size, timings, round trip checked
 *   cmrace race FILE MODEL SAMPLE PRIOR.inc TUNE...   codes the first SAMPLE bytes with every TUNE, then the whole
 *                                         file with the default and with the winner; also every TUNE on the whole
 *                                         file (the oracle among the presets) */
#include "../../../src/kolmo.c"
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
enum { NI = CM_NIX3 };
static int16_t P[5][NI];
static void load_prior(const char *path) {   /* "-": the compiled-in prior. Otherwise reads the numbers of an .inc written by train (model index ignored: one prior) */
    if (!strcmp(path, "-")) return;
    size_t n; char *t = (char *)slurp(path, &n); if (!t) { fprintf(stderr, "no prior %s\n", path); exit(1); } t[n] = 0;
    char *q = strchr(t, '='); int k = 0; q = q ? q + 1 : t;
    while (*q && k < 5 * NI) { if (*q == '-' || (*q >= '0' && *q <= '9')) { P[k / NI][k % NI] = (int16_t)strtol(q, &q, 10); k++; } else q++; }
    free(t); cm_prior_override = P;
}
static cm_tune_t tparse(const char *h) { unsigned v = (unsigned)strtoul(h, 0, 16); return (cm_tune_t){(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; }
static size_t code(const uint8_t *in, size_t n, int model, cm_tune_t t, double *te, double *td, int check) {
    size_t cap = n + n / 8 + 1024; uint8_t *o = malloc(cap);
    double t0 = now(); size_t z = cm_compress_t(in, n, o, cap, model, 0, t); if (te) *te = now() - t0;
    if (check && z) {
        uint8_t *d = malloc(n + 8); t0 = now(); int r = cm_decompress(o, z, d, n); if (td) *td = now() - t0;
        if (r || memcmp(d, in, n)) { fprintf(stderr, "ROUND TRIP FAILED\n"); exit(2); }
        free(d);
    }
    free(o); return z;
}
int main(int argc, char **argv) {
    if (argc < 4) return 1;
    if (!strcmp(argv[1], "train")) {
        int model = atoi(argv[2]), v2 = model == 3 ? 3 : 1; double acc[5][NI] = {{0}}; int nf = 0;
        for (int a = 4; a < argc; a++) {
            size_t n; uint8_t *s = slurp(argv[a], &n); if (!s) continue;
            uint8_t *hist = malloc(n + 8); cm_t *m = cm_new_x(hist, cm_bits_for(n), v2, v2 == 3 ? (n + 8) * 4 : 0, 1);
            size_t cap = n + n / 8 + 1024, o = 4; uint8_t *out = malloc(cap); cm_enc_run(m, s, n, out, cap, &o, v2);
            int32_t (*rows[4])[NI] = {m->w1, m->w2, m->w3, m->w4}; size_t nr[4] = {CM_W1, CM_W2, CM_W3, CM_W4};
            for (int k = 0; k < 5; k++) {
                double sum[NI] = {0}; size_t used = 0;
                for (size_t r = 0; r < (k < 4 ? nr[k] : CM_WF); r++) {
                    int u = 0; for (int j = 0; j < (k < 4 ? NI : 5); j++) { int32_t w = k < 4 ? rows[k][r][j] : m->wf[r][j]; if (w != (k == 4 && j == 4 ? 0 : 1 << 14)) { u = 1; break; } }
                    if (!u) continue; used++;
                    for (int j = 0; j < (k < 4 ? NI : 5); j++) sum[j] += k < 4 ? rows[k][r][j] : m->wf[r][j];
                }
                for (int j = 0; j < NI; j++) acc[k][j] += used ? sum[j] / used : (k == 4 && j >= 4 ? 0 : 1 << 14);
            }
            nf++; cm_free(m); free(hist); free(out); free(s);
            fprintf(stderr, "trained %s\n", argv[a]);
        }
        FILE *f = fopen(argv[3], "w");
        fprintf(f, "{");
        for (int k = 0; k < 5; k++) { fprintf(f, "%s{", k ? ",\n " : ""); for (int j = 0; j < NI; j++) { double v = acc[k][j] / nf / 4.0; if (k == 4 && j >= 5) v = 0; long q = lround(v); if (q > 32767) q = 32767; if (q < -32768) q = -32768; fprintf(f, "%s%ld", j ? ", " : "", q); } fprintf(f, "}"); }
        fprintf(f, "}\n"); fclose(f); return 0;
    }
    size_t n; uint8_t *s = slurp(argv[2], &n); if (!s) return 1; int model = atoi(argv[3]);
    if (!strcmp(argv[1], "eval")) {
        if (argc > 5) load_prior(argv[5]);
        cm_tune_t t = tparse(argv[4]); double te, td; size_t z = code(s, n, model, t, &te, &td, 1);
        printf("%s model=%d tune=%s size=%zu enc=%.3f dec=%.3f\n", argv[2], model, argv[4], z, te, td);
    } else if (!strcmp(argv[1], "race")) {
        size_t L = (size_t)strtoull(argv[4], 0, 0); if (!L || L > n) L = n; load_prior(argv[5]);
        int np = argc - 6; size_t best = SIZE_MAX, bfull = SIZE_MAX; int bi = 0, bfi = 0; double tr = 0;
        printf("%s model=%d sample=%zu", argv[2], model, L);
        for (int i = 0; i < np; i++) { double te; size_t z = code(s, L, model, tparse(argv[6 + i]), &te, NULL, 0); tr += te; if (z < best) { best = z; bi = i; } }
        double te0, te1, td0, td1; size_t z0 = code(s, n, model, (cm_tune_t){0, 0, 0}, &te0, &td0, 1);
        size_t z1 = L == n ? best : code(s, n, model, tparse(argv[6 + bi]), &te1, &td1, 1);
        if (L == n) { te1 = te0; td1 = td0; }
        for (int i = 0; i < np; i++) { size_t z = L == n && i == bi ? best : code(s, n, model, tparse(argv[6 + i]), NULL, NULL, 0); printf(" %s=%zu", argv[6 + i], z); if (z < bfull) { bfull = z; bfi = i; } }
        printf(" | default=%zu race=%s:%zu gain=%.3f%% oracle=%s:%zu gain=%.3f%% race_time=%.2fs enc0=%.2fs dec0=%.2fs dec1=%.2fs\n", z0, argv[6 + bi], z1,
               100.0 * ((double)z0 - z1) / z0, argv[6 + bfi], bfull, 100.0 * ((double)z0 - bfull) / z0, tr, te0, td0, td1);
    }
    return 0;
}
