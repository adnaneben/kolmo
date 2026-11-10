/* Independent callers must retain their own plans, version flags and scratch. */
#include "kolmo.h"
#undef NDEBUG
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    const uint8_t *input, *reference; size_t size, reference_size;
    int threads, transforms; atomic_int *start;
} call;
static void *exercise(void *arg) {
    const call *c = arg;
    kolmo_params p = {KOLMO_FAST, 0, c->threads, 65536, c->transforms};
    size_t cap = kolmo_bound(c->size, &p);
    uint8_t *blob = malloc(cap), *back = malloc(c->size + 1); assert(blob && back);
    while (!atomic_load(c->start)) sched_yield();
    for (int i = 0; i < 24; i++) {
        int64_t cn = kolmo_compress(c->input, c->size, blob, cap, &p);
        assert(cn == (int64_t)c->reference_size && !memcmp(blob, c->reference, cn));
        assert(kolmo_decompress(blob, cn, back, c->size, c->threads) == (int64_t)c->size);
        assert(!memcmp(back, c->input, c->size));
    }
    free(blob); free(back); return NULL;
}
int main(void) {
    size_t cap = 1 << 20, n = 0; uint8_t *input = malloc(cap); assert(input);
    n += snprintf((char *)input + n, cap - n, "entity,index,signal\n");
    for (int i = 0; i < 20000; i++) {
        int x = i % 100;
        n += snprintf((char *)input + n, cap - n, "A%d,%d,%d\n", i / 100, x, x * x * x);
    }
    assert(n < cap);
    uint8_t noise[4096]; memset(noise, 'x', sizeof noise);
    kolmo_params p = {KOLMO_FAST, 0, 1, 65536, 1};
    size_t bound = kolmo_bound(n, &p); uint8_t *ref = malloc(bound), *other = malloc(bound); assert(ref && other);
    int64_t cn = kolmo_compress(input, n, ref, bound, &p); assert(cn > 0 && ref[4] == 5);
    p.threads = 4;
    assert(kolmo_compress(input, n, other, bound, &p) == cn && !memcmp(ref, other, cn));
    p.threads = 1; p.transforms = 0;
    int64_t nn = kolmo_compress(noise, sizeof noise, other, bound, &p); assert(nn > 0 && other[4] == 3);
    atomic_int start; atomic_init(&start, 0);
    call calls[4] = {
        {input, ref, n, cn, 1, 1, &start}, {input, ref, n, cn, 2, 1, &start},
        {input, ref, n, cn, 4, 1, &start}, {noise, other, sizeof noise, nn, 3, 0, &start}
    };
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, exercise, &calls[i]));
    atomic_store(&start, 1);
    for (int i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
    free(input); free(ref); free(other);
    /* Two independently planned archive members exercise binary record scoring,
     * including concurrent callers with different worker counts. */
    size_t member = 1 << 20, total = 2 * (512 + member) + 1024;
    uint8_t *archive = calloc(total, 1); assert(archive);
    for (int m = 0; m < 2; m++) {
        uint8_t *header = archive + m * (512 + member), *body = header + 512;
        snprintf((char *)header, 100, "member%d.bin", m);
        snprintf((char *)header + 124, 12, "%011o", (unsigned)member);
        memcpy(header + 257, "ustar", 5); memcpy(header + 263, "00", 2); header[156] = '0';
        memset(header + 148, ' ', 8);
        unsigned sum = 0; for (int i = 0; i < 512; i++) sum += header[i];
        snprintf((char *)header + 148, 8, "%06o", sum); header[155] = ' ';
        for (size_t i = 0; i < member; i += 4) {
            uint32_t v = (uint32_t)((i / 16) * (m ? 7919 : 97) + i % 16 * 13);
            memcpy(body + i, &v, 4);
        }
    }
    p = (kolmo_params){KOLMO_FAST, 0, 1, 65536, 1};
    bound = kolmo_bound(total, &p); ref = malloc(bound); assert(ref);
    cn = kolmo_compress(archive, total, ref, bound, &p); assert(cn > 0);
    atomic_store(&start, 0);
    call archive_calls[2] = {{archive, ref, total, cn, 2, 1, &start}, {archive, ref, total, cn, 4, 1, &start}};
    for (int i = 0; i < 2; i++) assert(!pthread_create(&threads[i], NULL, exercise, &archive_calls[i]));
    atomic_store(&start, 1);
    for (int i = 0; i < 2; i++) assert(!pthread_join(threads[i], NULL));
    free(archive); free(ref);
    puts("PARALLEL TESTS PASSED (144 concurrent table, archive and plain round trips)");
    return 0;
}
