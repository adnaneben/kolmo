/* R03 prototype v2 (ideal code length only): direct binary context mixing over the BWT of one block (no LZP).
   Contexts are BWT-domain only: order 0-3 of preceding BWT bytes, run length, and two-rate counters; two mixers
   (selected by run class and by the previous byte) averaged in the logistic domain, then two interpolated APMs.
   usage: bwtcm2 file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "algo/sais/sais.h"
#define NC 6
#define NI (2 * NC + 1)
static float st_t[4096];
static inline float squashf(float x) { if (x > 30) x = 30; if (x < -30) x = -30; return 1.0f / (1.0f + expf(-x)); }
static inline float stretchp(int p16) { return st_t[p16 >> 4]; }
typedef struct { float t[33]; } apm_row;
static inline float apm_get(apm_row *r, float s, int *idx, float *wt) {
    float x = (s < -7.999f ? -7.999f : s > 7.999f ? 7.999f : s) * 2 + 16; int i = (int)x; float w = x - i; *idx = i; *wt = w;
    return r->t[i] * (1 - w) + r->t[i + 1] * w;
}
static inline void apm_upd(apm_row *r, int idx, float wt, int bit, float rate) {
    r->t[idx] += (bit - r->t[idx]) * rate * (1 - wt); r->t[idx + 1] += (bit - r->t[idx + 1]) * rate * wt;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *T = malloc(n + 8), *U = malloc(n + 8); if (fread(T, 1, n, f) != (size_t)n) return 1; fclose(f);
    int32_t *A = malloc((size_t)n * 4 + 16), I[256]; int r = 2; while (((n - 1) / r) > 200) r <<= 1;
    if (kolmo_sais_bwt_aux(T, U, A, (int32_t)n, r, I, 1) != 0) { puts("bwt fail"); return 1; }
    free(A); free(T);
    for (int i = 0; i < 4096; i++) { float p = (i + 0.5f) / 4096; st_t[i] = logf(p / (1 - p)); }
    size_t HS = 1u << 24;
    uint16_t *tf[NC], *ts[NC];
    for (int i = 0; i < NC; i++) { tf[i] = malloc(HS * 2); ts[i] = malloc(HS * 2); for (size_t k = 0; k < HS; k++) tf[i][k] = ts[i][k] = 32768; }
    static float w1[4 * 256][NI], w2[256 * 8][NI];
    for (int i = 0; i < 4 * 256; i++) for (int k = 0; k < NI; k++) w1[i][k] = 0.15f;
    for (int i = 0; i < 256 * 8; i++) for (int k = 0; k < NI; k++) w2[i][k] = 0.15f;
    apm_row *a1 = malloc(sizeof(apm_row) * 256 * 256), *a2 = malloc(sizeof(apm_row) * 65536);
    for (int i = 0; i < 65536; i++) for (int k = 0; k < 33; k++) { a1[i].t[k] = squashf((k - 16) / 2.0f); a2[i].t[k] = squashf((k - 16) / 2.0f); }
    double bits = 0; uint32_t c1 = 0, c2 = 0, c3 = 0; int run = 0;
    for (long i = 0; i < n; i++) {
        int c = U[i];
        int rq = run == 0 ? 0 : run == 1 ? 1 : run < 4 ? 2 : run < 16 ? 3 : run < 64 ? 4 : run < 256 ? 5 : run < 2048 ? 6 : 7;
        int rc = run == 0 ? 0 : run < 3 ? 1 : run < 16 ? 2 : 3;
        uint32_t cx[NC];
        cx[0] = 0;
        cx[1] = (c1 + 1) << 8;
        cx[2] = (0x10000 + (c2 << 8 | c1)) << 8;                       /* exact order 2: 2^16 * 2^8 entries */
        cx[3] = (((c3 << 16 | c2 << 8 | c1) + 0x1234567) * 0x85EBCA6Bu) >> 8 << 8;
        cx[4] = ((uint32_t)(rq << 8 | c1) * 0xC2B2AE35u + 77u) >> 8 << 8;
        cx[5] = ((uint32_t)((c1 == c2) | (c2 == c3) << 1 | (c1 == c3) << 2 | rq << 3) * 0x9E3779B1u + 991u) >> 8 << 8;
        int node = 1;
        for (int b = 7; b >= 0; b--) {
            int bit = (c >> b) & 1; float st[NI]; uint16_t *pf[NC], *ps[NC];
            for (int k = 0; k < NC; k++) {
                size_t ix = (k < 3 ? (cx[k] + node) : (cx[k] + (uint32_t)node * 0x2545F491u)) & (HS - 1);
                pf[k] = &tf[k][ix]; ps[k] = &ts[k][ix]; st[2 * k] = stretchp(*pf[k]); st[2 * k + 1] = stretchp(*ps[k]);
            }
            st[NI - 1] = 0.3f;
            float *wa = w1[rc * 256 + node], *wb = w2[(c1 & 255) * 8 + (node < 8 ? node : 0)];
            /* second mixer selected by previous byte and the top 3 bits of the node path (0 when deeper) */
            int top = node; while (top >= 8) top >>= 1; wb = w2[(c1 & 255) * 8 + top];
            float da = 0, db = 0; for (int k = 0; k < NI; k++) { da += wa[k] * st[k]; db += wb[k] * st[k]; }
            float pa = squashf(da), pb = squashf(db), s = (da + db) * 0.5f, p = squashf(s);
            int i1, i2; float t1, t2;
            float q1 = apm_get(&a1[(c1 << 8 | node) & 65535], s, &i1, &t1);
            float q2 = apm_get(&a2[((c2 * 0x9E37u + c1 * 31u + rq) & 255) << 8 | node], s, &i2, &t2);
            float pfinal = 0.5f * p + 0.25f * q1 + 0.25f * q2;
            if (pfinal < 1.0f / 65536) pfinal = 1.0f / 65536; if (pfinal > 1 - 1.0f / 65536) pfinal = 1 - 1.0f / 65536;
            bits -= log2f(bit ? pfinal : 1 - pfinal);
            float ea = (bit - pa) * 0.015f, eb = (bit - pb) * 0.015f;
            for (int k = 0; k < NI; k++) { wa[k] += ea * st[k]; wb[k] += eb * st[k]; }
            for (int k = 0; k < NC; k++) {
                int v = *pf[k]; if (bit) v += (65535 - v) >> 3; else v -= v >> 3; *pf[k] = (uint16_t)v;
                v = *ps[k]; if (bit) v += (65535 - v) >> 6; else v -= v >> 6; *ps[k] = (uint16_t)v;
            }
            apm_upd(&a1[(c1 << 8 | node) & 65535], i1, t1, bit, 0.02f);
            apm_upd(&a2[((c2 * 0x9E37u + c1 * 31u + rq) & 255) << 8 | node], i2, t2, bit, 0.02f);
            node = node * 2 + bit;
        }
        if (c == (int)c1) run++; else run = 0;
        c3 = c2; c2 = c1; c1 = c;
    }
    printf("%s n=%ld bwtcm2_bytes=%.0f\n", argv[1], n, bits / 8);
    return 0;
}
