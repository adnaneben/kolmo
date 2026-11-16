/* nncm: bit-level neural context-mixing coder (research prototype).
 *
 * Each input symbol is coded MSB-first as `bits` binary decisions (8 for bytes, 6 for k6 symbol streams).
 * For every bit, N context models each give a probability from a hashed table of 12-bit counters; a
 * single-layer neural network (logistic mixer, weights selected by a small context) combines their
 * stretched outputs; an adaptive probability map (APM) refines the result; a binary arithmetic coder codes it.
 *
 * Models (level):  0  order-0 only (no network: a single adaptive bit model)
 *                  1  orders 1-4 + order 0, mixed by the network
 *                  2  level 1 + word context + table-column context (field index on the line, and
 *                     the bytes above in the previous line at the same field)
 * usage: nncm c|d level bits in out      (prints sizes and MB/s on stderr)
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TB 22                       /* log2 slots per hashed table */
#define NM 9
static uint16_t *T[NM];
static float W[256 * 64][NM + 1];   /* mixer weights selected by (model level, bit-tree node) */
static uint16_t apm[1024][33];
static float st_tab[4096];

static inline float stretch(int p) { return st_tab[p]; }
static inline int squash(float x) { if (x > 20) x = 20; if (x < -20) x = -20; int p = (int)(4096.0f / (1.0f + expf(-x))); return p < 1 ? 1 : p > 4095 ? 4095 : p; }

/* ------------------------------------------------------------------ binary arithmetic coder (lpaq style) */
typedef struct { uint32_t x1, x2, x; uint8_t *buf; size_t pos, n; } ac;
static void ac_enc(ac *a, int bit, int p12) {
    uint32_t xm = a->x1 + (uint32_t)(((uint64_t)(a->x2 - a->x1) * p12) >> 12);
    if (bit) a->x2 = xm; else a->x1 = xm + 1;
    while (((a->x1 ^ a->x2) & 0xff000000u) == 0) { a->buf[a->pos++] = a->x2 >> 24; a->x1 <<= 8; a->x2 = (a->x2 << 8) | 255; }
}
static int ac_dec(ac *a, int p12) {
    uint32_t xm = a->x1 + (uint32_t)(((uint64_t)(a->x2 - a->x1) * p12) >> 12);
    int bit = a->x <= xm;
    if (bit) a->x2 = xm; else a->x1 = xm + 1;
    while (((a->x1 ^ a->x2) & 0xff000000u) == 0) {
        a->x1 <<= 8; a->x2 = (a->x2 << 8) | 255; a->x = (a->x << 8) | (a->pos < a->n ? a->buf[a->pos++] : 0);
    }
    return bit;
}

/* ------------------------------------------------------------------ model */
typedef struct {
    int level, bits, nm;
    uint32_t ctx[NM];         /* per-symbol context hashes */
    uint32_t idx[NM];         /* current slot per model */
    float s[NM + 1];
    int pr, pm, apm_i;
    float apm_w;
    uint8_t hist[8];
    uint32_t word, field, col, lastnl;
    /* previous line's field starts, for the column context */
    uint32_t prevline_field_hash[256], curline_field_hash[256];
} model;

static inline uint32_t hsh(uint32_t a, uint32_t b) { uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u; return h ^ h >> 15; }

static void model_symbol_ctx(model *m) {
    uint8_t *h = m->hist;
    m->ctx[0] = 0;
    m->ctx[1] = hsh(1, h[0]);
    m->ctx[2] = hsh(2, h[0] | h[1] << 8);
    m->ctx[3] = hsh(3, h[0] | h[1] << 8 | h[2] << 16);
    m->ctx[4] = hsh(4, h[0] | h[1] << 8 | h[2] << 16 | (uint32_t)h[3] << 24);
    m->ctx[5] = hsh(hsh(5, h[4] | h[5] << 8), h[0] | h[1] << 8 | h[2] << 16 | (uint32_t)h[3] << 24);
    m->ctx[6] = hsh(6, m->word);
    m->ctx[7] = hsh(hsh(7, m->col), m->prevline_field_hash[m->col & 255]);
    m->ctx[8] = hsh(hsh(8, m->col), m->field);
}

static void model_update_symbol(model *m, int c, int nl, int sep) {
    memmove(m->hist + 1, m->hist, 7); m->hist[0] = c;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 128) m->word = hsh(m->word, c);
    else m->word = 0;
    if (c == nl) {
        m->curline_field_hash[m->col & 255] = m->field;
        memcpy(m->prevline_field_hash, m->curline_field_hash, sizeof m->curline_field_hash);
        m->col = 0; m->field = 0;
    } else if (c == sep) {
        m->curline_field_hash[m->col & 255] = m->field;
        m->col++; m->field = 0;
    } else m->field = hsh(m->field, c);
    model_symbol_ctx(m);
}

/* probability (12 bit) that the next bit is 1, given the partial symbol node (1..2^bits-1) */
static int predict(model *m, int node) {
    float dot = 0;
    float *w = W[(node & 255) + 256 * 0];
    for (int i = 0; i < m->nm; i++) {
        m->idx[i] = (hsh(m->ctx[i], node)) >> (32 - TB);
        m->s[i] = stretch(T[i][m->idx[i]] >> 4);
        dot += w[i] * m->s[i];
    }
    m->s[m->nm] = 0.3f; dot += w[m->nm] * m->s[m->nm];
    m->pm = squash(dot);
    if (m->level == 0) { m->pr = T[0][m->idx[0]] >> 4; if (m->pr < 1) m->pr = 1; if (m->pr > 4095) m->pr = 4095; return m->pr; }
    /* APM on order-1 x node, interpolating 33 buckets of the stretched mixer output */
    float sx = (stretch(m->pm) + 8.0f) * 2.0f; if (sx < 0) sx = 0; if (sx > 31.999f) sx = 31.999f;
    int lo = (int)sx; m->apm_w = sx - lo;
    m->apm_i = ((m->hist[0] * 4 + (node & 3)) & 1023);
    int pa = (int)(apm[m->apm_i][lo] * (1 - m->apm_w) + apm[m->apm_i][lo + 1] * m->apm_w) >> 4;
    m->pr = (m->pm + 3 * pa) >> 2;
    if (m->pr < 1) m->pr = 1; if (m->pr > 4095) m->pr = 4095;
    return m->pr;
}

static void update(model *m, int node, int bit) {
    for (int i = 0; i < m->nm; i++) {
        uint16_t *t = &T[i][m->idx[i]];
        if (bit) *t += (65535 - *t) >> 4; else *t -= *t >> 4;
    }
    if (m->level == 0) return;
    float err = ((bit << 12) - m->pm) * (0.0025f / 4096.0f) * 8;
    float *w = W[(node & 255)];
    for (int i = 0; i <= m->nm; i++) w[i] += err * m->s[i];
    int lo = (int)((stretch(m->pm) + 8.0f) * 2.0f); if (lo < 0) lo = 0; if (lo > 31) lo = 31;
    uint16_t tgt = bit ? 65535 : 0;
    uint16_t *a = &apm[m->apm_i][lo];
    a[0] += ((int)tgt - a[0]) * (1 - m->apm_w) / 32; a[1] += ((int)tgt - a[1]) * m->apm_w / 32;
}

static void model_init(model *m, int level, int bits) {
    memset(m, 0, sizeof *m);
    m->level = level; m->bits = bits; m->nm = level == 0 ? 1 : level == 1 ? 6 : 9;
    for (int i = 0; i < 4096; i++) st_tab[i] = logf((i + 0.5f) / (4096 - i - 0.5f));
    for (int i = 0; i < NM; i++) { T[i] = malloc(sizeof(uint16_t) << TB); for (size_t j = 0; j < (1u << TB); j++) T[i][j] = 32768; }
    for (int i = 0; i < 256 * 64; i++) for (int j = 0; j <= NM; j++) W[i][j] = 0.3f;
    for (int i = 0; i < 1024; i++) for (int j = 0; j < 33; j++) apm[i][j] = (uint16_t)(65535.0 / (1 + exp(-((j - 16) / 2.0))));
    model_symbol_ctx(m);
}

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
    if (argc != 6) { fprintf(stderr, "usage: nncm c|d level bits in out\n"); return 2; }
    int level = atoi(argv[2]), bits = atoi(argv[3]);
    FILE *f = fopen(argv[4], "rb"); if (!f) { perror(argv[4]); return 1; }
    fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *in = malloc(n + 8); if (fread(in, 1, n, f) != n) return 1; fclose(f);
    /* in 6-bit mode the k6 fixed table puts '\n' at code 38 and ',' at 37 */
    int nl = bits == 6 ? 38 : '\n', sep = bits == 6 ? 37 : ',';
    model m; model_init(&m, level, bits);
    double t0 = now();
    if (argv[1][0] == 'c') {
        uint8_t *out = malloc(n * 2 + 64); ac a = { 0, 0xffffffffu, 0, out + 8, 0, 0 };
        memcpy(out, &n, 8);
        for (size_t i = 0; i < n; i++) {
            int c = in[i], node = 1;
            for (int b = bits - 1; b >= 0; b--) {
                int bit = c >> b & 1, p = predict(&m, node);
                ac_enc(&a, bit, p); update(&m, node, bit); node = node * 2 + bit;
            }
            model_update_symbol(&m, c, nl, sep);
        }
        out[8 + a.pos++] = a.x1 >> 24; out[8 + a.pos++] = 255; out[8 + a.pos++] = 255; out[8 + a.pos++] = 255;
        f = fopen(argv[5], "wb"); fwrite(out, 1, a.pos + 8, f); fclose(f);
        double t = now() - t0;
        fprintf(stderr, "%zu -> %zu  ratio %.3f  %.3f bits/sym  %.2f MB/s\n", n, a.pos + 8, (double)n / (a.pos + 8), 8.0 * (a.pos + 8) / n, n / t / 1e6);
    } else {
        size_t N; memcpy(&N, in, 8);
        uint8_t *out = malloc(N + 8);
        ac a = { 0, 0xffffffffu, 0, in + 8, 0, n - 8 };
        for (int i = 0; i < 4; i++) a.x = (a.x << 8) | a.buf[a.pos++];
        for (size_t i = 0; i < N; i++) {
            int node = 1;
            for (int b = bits - 1; b >= 0; b--) {
                int p = predict(&m, node), bit = ac_dec(&a, p);
                update(&m, node, bit); node = node * 2 + bit;
            }
            int c = node - (1 << bits); out[i] = c;
            model_update_symbol(&m, c, nl, sep);
        }
        f = fopen(argv[5], "wb"); fwrite(out, 1, N, f); fclose(f);
        fprintf(stderr, "decoded %zu  %.2f MB/s\n", N, N / (now() - t0) / 1e6);
    }
    return 0;
}
