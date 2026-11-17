/* GR6+ formula search: the reference (g6_formulas of src/codecs/gr6.c, reached by including src/kolmo.c as the tests
 * do) and its map-reduce form, whose map runs on CPU threads or on the GPU (src/arch/metal/gpu2.h). Apache-2.0, Kolmo
 * contributors. Research harness of benchmark/rc3w2/gpu2. */
#include "../../../src/kolmo.c"
#include "../../../src/arch/metal/gpu2.h"
#include "g6wrap.h"

uint8_t g6w_sep(const uint8_t *d, size_t n) {           /* gr6_compress's choice */
    static const uint8_t SEPS[5] = {',', '\t', ';', '|', ' '};
    size_t c[5] = {0}, m = n < (1u << 20) ? n : (1u << 20);
    for (size_t i = 0; i < m; i++) for (int k = 0; k < 5; k++) c[k] += d[i] == SEPS[k];
    int b = 0; for (int k = 1; k < 5; k++) if (c[k] > c[b]) b = k;
    return SEPS[b];
}

int g6w_reference(const uint8_t *d, size_t n, uint8_t sep, g6w_formula *out) {
    pthread_once(&cm_once, cm_init_tables);
    g6_heap *H = calloc(1, sizeof *H); g6_formula *F = malloc(sizeof(g6_formula) * G6_MAXFORM);
    int nf = -1;
    if (H && F) {
        if (!setjmp(H->jb)) {
            nf = g6_formulas(H, d, n, sep, F);
            for (int i = 0; i < nf; i++) { out[i].t = F[i].t; out[i].a = F[i].a; out[i].b = F[i].b; out[i].ke = F[i].ke; out[i].type = F[i].type; }
        }
        g6_free_all(H);
    }
    free(H); free(F);
    return nf;
}

/* the sample of g6_formulas (same rows, same parse), column-major with stride G6W_R */
int g6w_sample(const uint8_t *d, size_t n, uint8_t sep, g6w_sample_t *S) {
    size_t i = 0, line = 0; int K = -1; const int R = G6W_R;
    int64_t *V = NULL; uint8_t *D = NULL, *OK = NULL; int rows = 0;
    memset(S, 0, sizeof *S);
    while (i < n && rows < R) {
        const uint8_t *nl = memchr(d + i, '\n', n - i); size_t e = nl ? (size_t)(nl - d) : n;
        if (line > 0) {
            int nf = 1; for (size_t k = i; k < e && nf <= 512; k++) nf += d[k] == sep;
            if (K < 0) {
                if (nf > 512 || nf <= 2) return 0;
                K = nf; V = calloc((size_t)K * R, 8); D = calloc((size_t)K * R, 1); OK = calloc((size_t)K * R, 1);
                if (!V || !D || !OK) { free(V); free(D); free(OK); return -1; }
            }
            if (nf == K) {
                size_t fs = i; int f = 0;
                for (size_t k = i; k <= e; k++) if (k == e || d[k] == sep) {
                    g6_fmt fm = {0, 0}; int64_t v = 0; int ok = g6_parse(d + fs, k - fs, &v, &fm);
                    V[(size_t)f * R + rows] = v; D[(size_t)f * R + rows] = fm.d; OK[(size_t)f * R + rows] = (uint8_t)ok; f++; fs = k + 1;
                }
                rows++;
            }
        }
        line++; i = e + 1;
    }
    S->V = V; S->D = D; S->OK = OK; S->K = K < 0 ? 0 : (uint32_t)K; S->rows = (uint32_t)rows;
    return K > 2 && rows >= 50;
}
void g6w_sample_free(g6w_sample_t *S) { free(S->V); free(S->D); free(S->OK); memset(S, 0, sizeof *S); }

/* candidates in g6_formulas' loop order, for every target that passes its usable-row test */
uint32_t g6w_candidates(const g6w_sample_t *S, kg2_cand **out) {
    const uint32_t K = S->K, R = G6W_R; uint32_t rows = S->rows;
    uint64_t cap = 0;
    for (uint32_t t = 1; t < K; t++) cap += 2ull * t * (t - 1) * 5 + 4ull * t;
    kg2_cand *c = malloc(cap * sizeof *c); uint32_t nc = 0;
    if (!c) return 0;
    for (uint32_t t = 1; t < K; t++) {
        int cnt = 0; for (uint32_t r = 0; r < rows; r++) cnt += S->OK[(size_t)t * R + r];
        if (cnt < (int)rows / 10 || cnt < 50) continue;
        for (int ty = 0; ty < 6; ty++) {
            int two = ty == 0 || ty == 5;
            for (uint32_t a = 0; a < t; a++) for (uint32_t b = 0; b < (two ? t : 1); b++) for (int q = 0; q < (two ? 5 : 1); q++) {
                if (two && a == b) continue;
                c[nc++] = (kg2_cand){(uint16_t)t, (uint16_t)a, (uint16_t)(two ? b : a), (uint16_t)(ty << 8 | q)};
            }
        }
    }
    *out = c;
    return nc;
}

/* the reduce: per target, in order, the first candidate with the best rate (seen >= 32, 2 hit > seen) */
int g6w_reduce(const kg2_cand *c, uint32_t nc, const kg2_score *sc, g6w_formula *out) {
    static const int KE[5] = {0, 2, 3, 6, 9};
    int nout = 0;
    for (uint32_t i = 0; i < nc && nout < G6_MAXFORM;) {
        uint32_t t = c[i].t; int64_t bh = 0, bs = 1; int32_t best = -1;
        for (; i < nc && c[i].t == t; i++) {
            int64_t h = sc[i].hit, s = sc[i].seen;
            if (s >= 32 && 2 * h > s && h * bs > bh * s) { bh = h; bs = s; best = (int32_t)i; }
        }
        if (best >= 0) out[nout++] = (g6w_formula){(int)t, c[best].a, c[best].b, KE[c[best].tq & 255], c[best].tq >> 8};
    }
    return nout;
}
