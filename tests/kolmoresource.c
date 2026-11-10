/* Deterministic resource-failure tests. Compile this translation unit alone:
 * it includes the core to reach test-only hooks and private pool fallback paths. */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int create_budget = -1, joined, live, fail_setspecific;
static pthread_t created[256];
static int test_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
    if (!create_budget) return EAGAIN;
    if (create_budget > 0) create_budget--;
    int r = pthread_create(t, a, fn, arg);
    if (!r) { assert(live < 256); created[live++] = *t; }
    return r;
}
static int test_join(pthread_t t, void **result) {
    int i; for (i = 0; i < live && !pthread_equal(t, created[i]); i++);
    assert(i < live); /* Joining a failed/uninitialized thread is a regression. */
    created[i] = created[--live]; joined++;
    return pthread_join(t, result);
}
static int test_setspecific(pthread_key_t key, const void *value) {
    return fail_setspecific ? ENOMEM : pthread_setspecific(key, value);
}
#define KOLMO_TESTING
#define pthread_create test_create
#define pthread_join test_join
#define pthread_setspecific test_setspecific
#include "../src/kolmo.c"
#undef pthread_create
#undef pthread_join
#undef pthread_setspecific
#include "legacy_frames.h"

typedef struct { atomic_uint next, completed; } work;
static void *count_work(void *arg) {
    work *w = arg;
    while (atomic_fetch_add(&w->next, 1) < 1000) atomic_fetch_add(&w->completed, 1);
    return NULL;
}
static void pool_failures(void) {
    for (int budget = 0; budget <= 2; budget++) {
        work w; atomic_init(&w.next, 0); atomic_init(&w.completed, 0);
        create_budget = budget; joined = live = 0;
        pthread_mutex_lock(&pool_job); /* Force the concurrent-caller fallback. */
        run_pool(count_work, &w, 8, 1000);
        pthread_mutex_unlock(&pool_job);
        assert(atomic_load(&w.completed) == 1000);
        assert(joined == budget && !live);
    }
    create_budget = 0;
    work w; atomic_init(&w.next, 0); atomic_init(&w.completed, 0);
    run_pool(count_work, &w, 8, 1000); /* Persistent pool also falls back to caller. */
    assert(atomic_load(&w.completed) == 1000 && pool_n == 0);
    create_budget = -1;
}
static void frame_crc(uint8_t *blob) {
    uint32_t c = kolmo_crc32c(0, blob, 28);
    c = kolmo_crc32c(c, blob + HDR, (size_t)get32(blob + 20) * IDX);
    put32(blob + 28, c);
}
static void formula_thread_failure(void) {
    size_t cap = 256 << 10, n = 0; uint8_t *text = malloc(cap); assert(text);
    n += snprintf((char *)text, cap, "entity,a,b,derived\n");
    for (int i = 0; i < 4000; i++) {
        int a = 100 + i * 37 % 10000, b = 7 + i * 13 % 97;
        n += snprintf((char *)text + n, cap - n, "G%d,%d,%d,%.9f\n", i / 20, a, b, (double)a / b * 1000);
    }
    assert(n < cap);
    fset_t serial = {0}, failed = {0};
    fset_discover(text, n, ',', 4, 0, 1, &serial, 1);
    assert(serial.n > 0);
    create_budget = 0;
    fset_discover(text, n, ',', 4, 0, 1, &failed, 4);
    create_budget = -1;
    assert(serial.n == failed.n && serial.gcol == failed.gcol);
    for (int i = 0; i < serial.n; i++) {
        const tformula *a = &serial.f[i], *b = &failed.f[i];
        assert(a->y == b->y && a->a == b->a && a->b == b->b && a->form == b->form && a->k == b->k);
    }
    free(text); zs_trim();
}
static void legacy_frames(void) {
    const uint8_t *frames[] = {legacy_v1, legacy_v2, legacy_v3, legacy_v4, legacy_v5};
    size_t sizes[] = {sizeof legacy_v1, sizeof legacy_v2, sizeof legacy_v3, sizeof legacy_v4, sizeof legacy_v5};
    size_t cap = 1 << 20; uint8_t *expected = malloc(cap), *back = malloc(cap); assert(expected && back);
    for (int v = 1; v <= 5; v++) {
        size_t n;
        if (v <= 3) { n = strlen("legacy fixture\n"); memcpy(expected, "legacy fixture\n", n); }
        else {
            n = snprintf((char *)expected, cap, "entity,index,signal\n");
            for (int i = 0; i < 20000; i++) {
                int x = i % 100, y = v == 4 ? 200 + 5 * x : x * x * x;
                n += snprintf((char *)expected + n, cap - n, "A%d,%d,%d\n", i / 100, x, y);
            }
        }
        for (int threads = 1; threads <= 4; threads += 3) {
            assert(kolmo_decompress(frames[v - 1], sizes[v - 1], back, cap, threads) == (int64_t)n);
            assert(!memcmp(back, expected, n));
        }
    }
    free(expected); free(back);
}
static void seekable_files(void) {
    uint8_t data[12000], restored[sizeof data];
    memset(data, 'A', sizeof data);
    kolmo_params p = {KOLMO_FAST, 0, 1, 4096, 1};
    size_t cap = kolmo_bound(sizeof data, &p);
    uint8_t *frame = malloc(cap), *reference = malloc(cap);
    FILE *input = tmpfile(), *output = tmpfile();
    assert(frame && reference && input && output);
    assert(fwrite(data, 1, sizeof data, input) == sizeof data && !fflush(input));
    int in = fileno(input), out = fileno(output);
    for (size_t n = 0; n <= sizeof data; n += sizeof data) {
        int64_t size = kolmo_compress_from_fd(in, n, frame, cap, &p, NULL, 0);
        assert(size > 0 && kolmo_compress(data, n, reference, cap, &p) == size);
        assert(!memcmp(frame, reference, size));
        assert(kolmo_decompress_to_fd(frame, size, out, 1, NULL, 0) == (int64_t)n);
        assert(pread(out, restored, n, 0) == (ssize_t)n && !memcmp(data, restored, n));
    }
    assert(lseek(in, 0, SEEK_CUR) == sizeof data && lseek(out, 0, SEEK_CUR) == 0);
    assert(kolmo_compress_from_fd(in, sizeof data + 1, frame, cap, &p, NULL, 0) == KOLMO_ERR_IO);
    assert(kolmo_decompress_to_fd(reference, cap, -1, 1, NULL, 0) == KOLMO_ERR_IO);
    fclose(input); fclose(output); free(frame); free(reference);
}

/* Both implementations must emit the same legacy transpose for every byte
 * alignment, including quoted punctuation, empty fields and ragged suffixes. */
static void wide_transpose(void) {
    enum { CAP = 16384 };
    uint8_t *raw = malloc(CAP), *a = malloc(CAP), *b = malloc(CAP);
    uint32_t *pos = malloc((CAP + 1) * sizeof(*pos));
    uint32_t *rows = malloc((CAP + 8) * sizeof(*rows));
    assert(raw && a && b && pos && rows);
    for (int j = 0; j < CAP; j++) raw[j] = j % 1257 == 0 ? '\n' : j % 3 == 0 ? ';' : '7';
    for (size_t n = CAP - 64; n <= CAP; n++) {
        int64_t original[256], wide[256];
        tpq_fwd(raw, n, ';', 256, a, original, pos);
        tpq_wide_fwd(raw, n, ';', b, wide);
        assert(!memcmp(original, wide, sizeof(wide)) && !memcmp(a, b, n));
        int ret = tpq_inv_cols(b, n, ';', 256, wide, a, pos, rows);
        if (ret == 1) ret = tpq_inv(b, n, ';', 256, wide, a, pos);
        assert(ret == 0 && !memcmp(a, raw, n));
    }
    free(rows); free(pos); free(b); free(a); free(raw);
}
int main(void) {
    pool_failures();
    formula_thread_failure();
    legacy_frames();
    seekable_files();
    wide_transpose();
    uint8_t input[8192], output[8192], sentinel[64]; memset(input, 'A', sizeof input);
    kolmo_params p = {KOLMO_FAST, 0, 1, 4096, 0};
    size_t cap = kolmo_bound(sizeof input, &p); uint8_t *blob = malloc(cap), *bad = malloc(cap);
    assert(blob && bad);
    int failures = 0; int64_t cn = 0;
    for (long i = 0; i < 64; i++) {
        kolmo_alloc_fail_after = i;
        cn = kolmo_compress(input, sizeof input, blob, cap, &p);
        kolmo_alloc_fail_after = -1;
        if (cn >= 0) break;
        assert(cn == KOLMO_ERR_ALLOC); failures++;
    }
    assert(failures >= 8 && cn > 0);
    for (long i = 0; i < 2; i++) {
        kolmo_alloc_fail_after = i;
        assert(kolmo_decompress(blob, cn, output, sizeof output, 1) == KOLMO_ERR_ALLOC);
        kolmo_alloc_fail_after = -1;
    }
    assert(kolmo_decompress(blob, cn, output, sizeof output, 1) == sizeof input);
    assert(!memcmp(input, output, sizeof input));
    kolmo_decode_limits lim = {sizeof input, 4096, 2};
    assert(kolmo_decompress_limited(blob, cn, output, sizeof output, 1, &lim) == sizeof input);
    lim.max_output_bytes--; assert(kolmo_decompress_limited(blob, cn, output, sizeof output, 1, &lim) == KOLMO_ERR_LIMIT);
    lim.max_output_bytes++; lim.max_blocks = 1;
    assert(kolmo_decompress_limited(blob, cn, output, sizeof output, 1, &lim) == KOLMO_ERR_LIMIT);
    lim.max_blocks = 2; lim.max_block_bytes--;
    assert(kolmo_decompress_limited(blob, cn, output, sizeof output, 1, &lim) == KOLMO_ERR_LIMIT);
    memset(&lim, 0, sizeof lim);
    assert(kolmo_decompress_limited(blob, cn, output, sizeof output, 1, &lim) == sizeof input);
    memcpy(bad, blob, cn); put32(bad + HDR + 4, (uint32_t)cn + 1); frame_crc(bad);
    assert(kolmo_decompress(bad, cn, output, sizeof output, 1) == KOLMO_ERR_CORRUPT);
    memcpy(bad, blob, cn); put32(bad + HDR, UINT32_MAX); frame_crc(bad);
    assert(kolmo_decompress(bad, cn, output, sizeof output, 1) == KOLMO_ERR_CORRUPT);
    memset(sentinel, 0xA5, sizeof sentinel);
    for (size_t n = 0; n < HDR + IDX; n++) {
        assert(kolmo_compress(input, sizeof input, sentinel, n, &p) == KOLMO_ERR_DSTSIZE);
        for (size_t j = 0; j < sizeof sentinel; j++) assert(sentinel[j] == 0xA5);
    }
    assert(kolmo_read_range(blob, cn, sizeof input - 16, SIZE_MAX, output, 1) == 16);
    assert(!memcmp(output, input + sizeof input - 16, 16));
    assert(!array_alloc(SIZE_MAX, 8, 0));
    if (TLS) {
        if (TLS->retained) pthread_setspecific(tls_key, NULL);
        tls_destroy(TLS); TLS = NULL;
    }
    fail_setspecific = 1;
    assert(kolmo_decompress(blob, cn, output, sizeof output, 1) == sizeof input);
    assert(!TLS); /* Failed destructor registration must retain no scratch. */
    fail_setspecific = 0;
    int key_valid = tls_key_valid; tls_key_valid = 0;
    assert(kolmo_decompress(blob, cn, output, sizeof output, 1) == sizeof input && !TLS);
    tls_key_valid = key_valid;
    free(blob); free(bad);
    zs_trim();
    printf("RESOURCE TESTS PASSED (%d injected compression allocation failures)\n", failures);
    return 0;
}
