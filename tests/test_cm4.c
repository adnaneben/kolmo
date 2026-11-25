/* Codec 7 model 4 (codecs/cm.c: 16-byte nibble buckets of bit-history states, a context mask, the 16-bit mixer):
 *   - round trips of text, binary, repetitive and tiny streams, with the text and the binary context sets;
 *   - the same stream at every kernel level this CPU has (C references, x86 SIMD, AArch64 NEON): the mixer kernels may change
 *     speed, never bytes;
 *   - lazily initialised rows give the same model as a fresh one: two codings of a stream in one process are identical;
 *   - the decode working set the header declares (cm_stream_need) is what the decoder reserves;
 *   - damaged headers (a mask outside contexts 1-29, an unknown prior) are refused, and truncated or altered streams
 *     decode without reading or writing out of bounds (run under AddressSanitizer by `make check-cm4`). */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static size_t enc(const uint8_t *in, size_t n, uint8_t *out, size_t cap, uint32_t mask) {
    return cm_compress_raced(in, n, out, cap, 4, 0, 1, mask);
}
static void roundtrip(const char *name, const uint8_t *in, size_t n, uint32_t mask) {
    size_t cap = n + n / 8 + 1024; uint8_t *z = malloc(cap), *z2 = malloc(cap), *out = malloc(n + 1);
    size_t zn = enc(in, n, z, cap, mask);
    CHECK(zn > 0, "%s: encode failed", name);
    if (zn) {
        CHECK(n < 64 || (z[3] >> 4 == 8 && z[4] >> 4 == 9), "%s: not a model-4 stream", name);
        memset(out, 0x5A, n + 1);
        CHECK(!cm_decompress(z, zn, out, n) && !memcmp(in, out, n), "%s: round trip", name);
        size_t z2n = enc(in, n, z2, cap, mask);
        CHECK(z2n == zn && !memcmp(z, z2, zn), "%s: a second coding differs", name);
        if (n >= 64) {
            uint64_t need = cm_stream_need(z, zn, n), want = cm_mem4(n, (z[4] & 15) + 2, mask) + cm4_chain(n);
            CHECK(need == want && need <= cm4_need_bound(n, mask), "%s: working set %llu, expected %llu", name, (unsigned long long)need, (unsigned long long)want);
        }
        /* every kernel level writes the same stream */
        int mx = kolmo_cpu_max_level();
        for (int l = 0; l <= mx; l++) {
            if (mx == KOLMO_ISA_NEON && l != KOLMO_ISA_C && l != KOLMO_ISA_NEON) continue;
            kolmo_kernels k; kolmo_kernels_for_level(l, &k); cm_kk_override = &k;
            size_t zl = enc(in, n, z2, cap, mask);
            CHECK(zl == zn && !memcmp(z, z2, zn), "%s: kernels %s write another stream", name, kolmo_isa_name(l));
            memset(out, 0x5A, n + 1);
            CHECK(!cm_decompress(z, zn, out, n) && !memcmp(in, out, n), "%s: kernels %s do not decode it", name, kolmo_isa_name(l));
            cm_kk_override = NULL;
        }
        printf("%s: %zu -> %zu bytes\n", name, n, zn);
    }
    free(z); free(z2); free(out);
}

int main(void) {
    /* text: words, lines, a few numbers */
    size_t n = 120000; uint8_t *t = malloc(n);
    for (size_t i = 0; i < n; ) {
        int w = (int)(rnd() % 300); size_t l = (size_t)snprintf((char *)t + i, n - i, "%s%d%c", w % 7 ? "word" : "Name", w, rnd() % 9 ? ' ' : '\n');
        i += l < n - i ? l : n - i;
    }
    roundtrip("text", t, n, CM4_MASK_TEXT);
    roundtrip("text, binary contexts", t, n, CM4_MASK_BIN);
    /* binary: a random walk of 16-bit samples and random bytes */
    uint8_t *b = malloc(n); int v = 0;
    for (size_t i = 0; i + 1 < n; i += 2) { v += (int)(rnd() % 61) - 30; b[i] = (uint8_t)v; b[i + 1] = (uint8_t)(v >> 8); }
    roundtrip("walk", b, n, CM4_MASK_BIN);
    for (size_t i = 0; i < 40000; i++) b[i] = (uint8_t)rnd();
    roundtrip("random", b, 40000, CM4_MASK_BIN);
    /* repetitive: long matches, a copy with edits */
    for (size_t i = 0; i < n; i++) b[i] = t[i % 9973];
    for (size_t i = 0; i < n; i += 4099) b[i] ^= 1;
    roundtrip("repeats", b, n, CM4_MASK_TEXT);
    /* every context of model 3 at once, and none */
    roundtrip("all contexts", t, 60000, CM_MASK3);
    roundtrip("no hashed context", t, 60000, 0);
    /* tiny streams */
    for (size_t k = 0; k < 6; k++) { const size_t m = k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 63 : k == 3 ? 64 : k == 4 ? 65 : 4096; char nm[32]; snprintf(nm, sizeof nm, "tiny %zu", m); roundtrip(nm, t, m, CM4_MASK_TEXT); }

    /* damaged headers and streams */
    size_t cap = n + n / 8 + 1024; uint8_t *z = malloc(cap), *out = malloc(n + 64);
    size_t zn = enc(t, 50000, z, cap, CM4_MASK_TEXT);
    uint8_t *d = malloc(zn);
    memcpy(d, z, zn); d[8] |= 1;                        /* mask bit 0 (the order-1 direct model is not hashed) */
    CHECK(cm_decompress(d, zn, out, 50000) != 0 && !cm_stream_need(d, zn, 50000), "mask bit 0 accepted");
    memcpy(d, z, zn); d[11] |= 0x40;                    /* mask bit 30 */
    CHECK(cm_decompress(d, zn, out, 50000) != 0 && !cm_stream_need(d, zn, 50000), "mask bit 30 accepted");
    memcpy(d, z, zn); d[7] = 3;                         /* prior 3 does not exist */
    CHECK(cm_decompress(d, zn, out, 50000) != 0 && !cm_stream_need(d, zn, 50000), "prior 3 accepted");
    /* the model nibble registry (SPEC.md): every unassigned nibble is refused, and nibbles 14/15 refuse b2 values other
     * than 0 and 1 (W3 prior's token-shape model wrote 15 with bit 7 of b2; it must never decode as anything here) */
    for (int nb = 0; nb < 16; nb++) {
        if (nb == 3 || nb == 5 || nb == 6 || nb == 7 || nb == 9 || nb == 14 || nb == 15) continue;
        memcpy(d, z, zn); d[4] = (uint8_t)(nb << 4 | (d[4] & 15));
        CHECK(cm_decompress(d, zn, out, 50000) != 0 && !cm_stream_need(d, zn, 50000), "reserved model nibble %d accepted", nb);
    }
    for (int nb = 14; nb <= 15; nb++) {
        const uint8_t b2s[] = {2, 3, 0x80, 0x81, 0xff};
        for (size_t k = 0; k < sizeof b2s; k++) {
            memcpy(d, z, zn); d[4] = (uint8_t)(nb << 4 | 3); d[7] = b2s[k];
            CHECK(cm_decompress(d, zn, out, 50000) != 0 && !cm_stream_need(d, zn, 50000), "nibble %d with b2 %#x accepted", nb, b2s[k]);
        }
    }
    for (int it = 0; it < 200; it++) {                  /* altered bytes and truncations: any result, no memory error */
        memcpy(d, z, zn); size_t cut = it % 2 ? zn : 12 + rnd() % (zn - 12);
        for (int k = 0; k < 3; k++) d[12 + rnd() % (zn - 12)] ^= (uint8_t)(1 + rnd() % 255);
        (void)cm_decompress(d, cut, out, 50000);
    }
    free(d); free(z); free(out); free(t); free(b);
    printf(fails ? "FAILED: %d checks\n" : "model 4: all checks passed\n", fails);
    return fails != 0;
}
