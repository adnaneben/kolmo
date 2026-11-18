/* Per-kind priors and model 4 of codec 7 (docs/PRIORS.md). For generated samples of each kind of text: the kind
 * detector's answer, the codec-7 stream written at -x (prior id and model in tune byte 2), identical frames at 1 and 4
 * threads, and the round trip. Every prior id and both models through cm_compress_t / cm_decompress directly. Then
 * streams with an out-of-range prior id, or a per-kind prior on model 2, must be refused; damaged model-4 streams
 * must fail or decode without reading or writing out of bounds (run under AddressSanitizer by `make check-priors`). */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
static size_t gen(int kind, uint8_t *b, size_t cap) {
    size_t n = 0; static const char *w[8] = {"alpha", "beta", "gamma", "delta", "error", "value", "count", "node"};
    for (int i = 0; n + 512 < cap; i++) {
        const char *a = w[rnd() % 8], *c = w[rnd() % 8]; unsigned v = rnd() % 100000, h = rnd() % 24, mi = rnd() % 60;
        switch (kind) {
        case TT_MARKUP: n += (size_t)snprintf((char *)b + n, cap - n, "%s<item id=\"%u\"><name>%s</name><v>%s %u</v></item>\n", i ? "" : "<?xml version=\"1.0\"?>\n<root>\n", v, a, c, v / 7); break;
        case TT_JSON: n += (size_t)snprintf((char *)b + n, cap - n, "{\"id\": %u, \"type\": \"%s\", \"actor\": {\"login\": \"%s%u\"}, \"public\": true}\n", v, a, c, v % 97); break;
        case TT_CSV: n += (size_t)snprintf((char *)b + n, cap - n, "%s,%u,%u.%02u,%s,%u\n", a, 1980 + i % 40, v, v % 100, c, h); break;
        case TT_LOG: n += (size_t)snprintf((char *)b + n, cap - n, "2026-11-18 %02u:%02u:%02u,%03u INFO [%s-%u] %s: %s %u\n", h, mi, (unsigned)i % 60, v % 1000, a, v % 9, c, a, v); break;
        case TT_C: n += (size_t)snprintf((char *)b + n, cap - n, "%sstatic int %s_%u(struct %s *p, int n) {\n    if (p->%s > n) return %u;\n    return n;\n}\n", i ? "" : "#include <stdio.h>\n#define N 4\n", a, i, c, a, v); break;
        case TT_PY: n += (size_t)snprintf((char *)b + n, cap - n, "%sdef %s_%u(self, n):\n    if self.%s is None:\n        return %u\n    return n\n\n", i ? "" : "import os\n\n", a, i, c, v); break;
        case TT_JS: n += (size_t)snprintf((char *)b + n, cap - n, "function %s%u(x) {\n  const %s = this.%s === undefined ? %u : x;\n  return %s => %s;\n}\n", a, i, c, a, v, c, c); break;
        default: n += (size_t)snprintf((char *)b + n, cap - n, "The %s of the %s was %s, and the %s came %s %s.\n", a, c, a, c, w[rnd() % 8], c); break;
        }
    }
    return n;
}
int main(void) {
    int fails = 0; const size_t cap = 40000; uint8_t *in = malloc(cap), *z = malloc(2 * cap), *z4 = malloc(2 * cap), *out = malloc(cap + 64);
    static const int kinds[] = {TT_MARKUP, TT_JSON, TT_CSV, TT_LOG, TT_C, TT_PY, TT_JS, TT_TEXT};
    for (size_t k = 0; k < sizeof kinds / sizeof *kinds; k++) {
        const int kind = kinds[k]; size_t n = gen(kind, in, cap);
        int got = text_kind(in, n);
        if (got != kind) { printf("%s: detected as %s\n", tt_name[kind], tt_name[got]); fails++; }
        kolmo_params p = {KOLMO_ULTRA, 0, 1, 0, 1};
        int64_t a = kolmo_compress(in, n, z, 2 * cap, &p); p.threads = 4;
        int64_t b = kolmo_compress(in, n, z4, 2 * cap, &p);
        if (a <= 0 || a != b || memcmp(z, z4, (size_t)a)) { printf("%s: frames differ between 1 and 4 threads\n", tt_name[kind]); fails++; continue; }
        if (kolmo_decompress(z, (size_t)a, out, n, 1) != (int64_t)n || memcmp(in, out, n)) { printf("%s: -x round trip failed\n", tt_name[kind]); fails++; }
        /* the codec directly: every prior id with model 3 and 4, one learning preset */
        for (int pid = 0; pid < CM_NPRIOR; pid++) for (int model = 3; model <= 4; model++) {
            cm_tune_t t = {0x1c, 0xa5, (uint8_t)pid}; size_t m = n < 12000 ? n : 12000;
            size_t c = cm_compress_t(in, m, z, 2 * cap, model, 0, t);
            if (!c || cm_decompress(z, c, out, m) || memcmp(in, out, m)) { printf("%s: prior %d model %d failed\n", tt_name[kind], pid, model); fails++; continue; }
            if (model == 4 && !(z[7] & 0x80)) { printf("model 4 not marked\n"); fails++; }
            if (pid == 3 && model == 4 && k == 0) {                   /* damaged streams: refused or decoded in bounds */
                uint8_t *d = malloc(c);
                for (int r = 0; r < 60; r++) {
                    memcpy(d, z, c); int e = 1 + (int)(rnd() % 4);
                    for (int q = 0; q < e; q++) d[rnd() % c] ^= (uint8_t)(1 + rnd() % 255);
                    (void)cm_decompress(d, c, out, m);
                }
                memcpy(d, z, c); d[7] = (uint8_t)(0x80 | CM_NPRIOR);   /* prior id out of range */
                if (!cm_decompress(d, c, out, m)) { printf("prior id %d accepted\n", CM_NPRIOR); fails++; }
                memcpy(d, z, c); d[4] = (uint8_t)(14 << 4 | (d[4] & 15)); d[7] = 3;   /* model 2 with a per-kind prior */
                if (!cm_decompress(d, c, out, m)) { printf("model 2 with a per-kind prior accepted\n"); fails++; }
                d[7] = 0x83;                                                   /* model 4 bit on model 2 */
                if (!cm_decompress(d, c, out, m)) { printf("model 4 bit on model 2 accepted\n"); fails++; }
                free(d);
            }
        }
        printf("%s: %zu -> %lld bytes at -x\n", tt_name[kind], n, (long long)a);
    }
    free(in); free(z); free(z4); free(out);
    printf(fails ? "PRIOR TESTS FAILED: %d\n" : "PRIOR TESTS PASSED\n", fails);
    return fails != 0;
}
