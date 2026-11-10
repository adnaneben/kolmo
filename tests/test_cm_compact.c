/* Build once for the reference functions and once for the compact test driver. */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef KOLMO_CM_COMPACT
static void *test_calloc(size_t count, size_t size);
#define calloc test_calloc
#endif
#include "../src/cm.inc"
#ifdef KOLMO_CM_COMPACT
#undef calloc
#endif

static size_t encode(const uint8_t *data, size_t n, uint8_t *out, size_t cap, int model) {
    if (model == 2) return cm_compress(data, n, out, cap);
    if (model == 1) return cm_compress_m(data, n, out, cap, 1);
    /* Unflagged model-1 archives predate the block-dependent table size. */
    uint8_t *history = malloc(n + 8);
    assert(history);
    cm_t *m = cm_new(history, CM_BB, 0);
    assert(m);
    uint32_t size_field = (uint32_t)n;
    memcpy(out, &size_field, sizeof size_field);
    size_t bytes = 4;
    int error = cm_enc_loop(m, data, n, out, cap, &bytes, 0);
    cm_free(m);
    free(history);
    return error ? 0 : bytes;
}

#ifndef KOLMO_CM_COMPACT
size_t cm_reference_encode(const uint8_t *data, size_t n, uint8_t *out, size_t cap, int model) {
    return encode(data, n, out, cap, model);
}
int cm_reference_decode(const uint8_t *src, size_t bytes, uint8_t *out, size_t n) {
    return cm_decompress(src, bytes, out, n);
}
#else
size_t cm_reference_encode(const uint8_t *, size_t, uint8_t *, size_t, int);
int cm_reference_decode(const uint8_t *, size_t, uint8_t *, size_t);
static unsigned fail_scratch;

static void *test_calloc(size_t count, size_t size) {
    if (fail_scratch && count == 1 && size == sizeof(cm_pair_scratch) && --fail_scratch == 0)
        return NULL;
    return calloc(count, size);
}

static void check(size_t n, int model) {
    size_t cap = 2 * n + 1024;
    uint8_t *src = malloc(n), *decoded = malloc(n), *regular = malloc(cap), *compact = malloc(cap);
    assert(src && decoded && regular && compact);
    /* Alternate all previous-byte states, next nibbles and pseudo-random runs. */
    uint32_t state = 0x817ab22u;
    for (size_t i = 0; i < n; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        unsigned previous = (unsigned)(i >> 5) & 255, nibble = (unsigned)(i >> 1) & 15;
        uint8_t paired = (uint8_t)(i & 1 ? (nibble << 4) | (previous & 15) : previous);
        src[i] = i & 8192 ? (uint8_t)(state >> 24) : paired;
    }
    src[0] = (uint8_t)((n % 19) * 16 + model);
    size_t a = cm_reference_encode(src, n, regular, cap, model);
    size_t b = encode(src, n, compact, cap, model);
    assert(a && a == b && !memcmp(regular, compact, a));
    assert(!cm_decompress(regular, a, decoded, n) && !memcmp(src, decoded, n));
    memset(decoded, 0, n);
    assert(!cm_reference_decode(compact, b, decoded, n) && !memcmp(src, decoded, n));
    free(src); free(decoded); free(regular); free(compact);
}

int main(void) {
    uint8_t history[1];
    for (unsigned failure = 1; failure <= 2; ++failure) {
        fail_scratch = failure;
        assert(!cm_new(history, 16, 1) && !cm_used);
    }
    check(4096, 0);
    const size_t sizes[] = {4096, 65537, 1u << 20};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
        for (int model = 1; model <= 2; ++model) check(sizes[i], model);
    puts("Compact CM agrees with both reference models and unflagged legacy frames");
    return 0;
}
#endif
