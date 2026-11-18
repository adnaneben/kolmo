/* Research harness of the twopass lane (benchmark/rc3w2/twopass/RESULTS.md): codec 7 alone, with chosen learning
 * settings (cm_tune_t), and a coordinate search of those settings on a file or on a sample of it.
 *   cmtune eval FILE MODEL B0 B1          size and time of one setting (round trip checked)
 *   cmtune search FILE MODEL SAMPLE [PASSES [PRIOR]]  search on the first SAMPLE bytes, then code the whole file with the
 *                                         default and with the setting found
 * Build: see build.sh. Not part of the library. */
#include "../../../src/kolmo.c"
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
static size_t code(const uint8_t *in, size_t n, int model, cm_tune_t t, double *te, double *td, int check) {
    size_t cap = n + n / 8 + 1024; uint8_t *o = malloc(cap);
    double t0 = now(); size_t z = cm_compress_t(in, n, o, cap, model, 0, t); double t1 = now();
    if (te) *te = t1 - t0;
    if (check && z) {
        uint8_t *d = malloc(n + 8); t0 = now(); int r = cm_decompress(o, z, d, n); t1 = now(); if (td) *td = t1 - t0;
        if (r || memcmp(d, in, n)) { fprintf(stderr, "ROUND TRIP FAILED\n"); exit(2); }
        free(d);
    }
    free(o); return z;
}
static cm_tune_t search(const uint8_t *s, size_t n, int model, int passes, int *trials, int prior) {
    cm_tune_t best = {0, 0, (uint8_t)prior}; size_t bz = code(s, n, model, best, NULL, NULL, 0); *trials = 1;
    for (int pass = 0; pass < passes; pass++) {
        int changed = 0;
        for (int f = 0; f < 8; f++) {                    /* 8 fields of 2 bits */
            int byte = f / 4, sh = (f % 4) * 2;
            for (int v = 0; v < 4; v++) {
                cm_tune_t t = best; uint8_t *b = byte ? &t.b1 : &t.b0; if (((*b >> sh) & 3) == v) continue;
                *b = (uint8_t)((*b & ~(3 << sh)) | v << sh);
                size_t z = code(s, n, model, t, NULL, NULL, 0); (*trials)++;
                if (z < bz) { bz = z; best = t; changed = 1; }
            }
        }
        if (!changed) break;
    }
    return best;
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: cmtune eval|search FILE MODEL ...\n"); return 1; }
    size_t n; uint8_t *s = slurp(argv[2], &n); if (!s) return 1; int model = atoi(argv[3]);
    if (!strcmp(argv[1], "eval") && argc >= 6) {
        cm_tune_t t = {(uint8_t)strtol(argv[4], 0, 0), (uint8_t)strtol(argv[5], 0, 0)}; double te, td;
        size_t z = code(s, n, model, t, &te, &td, 1);
        printf("%s model=%d tune=%02x%02x size=%zu enc=%.3f dec=%.3f\n", argv[2], model, t.b0, t.b1, z, te, td);
    } else if (!strcmp(argv[1], "search") && argc >= 5) {
        size_t L = (size_t)strtoull(argv[4], 0, 0); if (!L || L > n) L = n; int passes = argc > 5 ? atoi(argv[5]) : 2, prior = argc > 6 ? atoi(argv[6]) : 0, tr;
        double t0 = now(); cm_tune_t t = search(s, L, model, passes, &tr, prior); double ts = now() - t0, te0, te1, td0, td1;
        size_t z0 = code(s, n, model, (cm_tune_t){0, 0, 0}, &te0, &td0, 1), z1 = code(s, n, model, t, &te1, &td1, 1);
        printf("%s model=%d sample=%zu trials=%d search=%.2fs tune=%02x%02x%02x default=%zu tuned=%zu gain=%.3f%% enc0=%.2f enc1=%.2f dec0=%.2f dec1=%.2f\n",
               argv[2], model, L, tr, ts, t.b0, t.b1, t.b2, z0, z1, 100.0 * ((double)z0 - (double)z1) / (double)z0, te0, te1, td0, td1);
    } else return 1;
    free(s); return 0;
}
