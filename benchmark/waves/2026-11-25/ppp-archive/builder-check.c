/* Compile the DDP1 implementation under test with a standalone copy of its symbols.
 * Calls into the rest of the library use the production archive library. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/features.h"

static int inject, empty_buffers;
static void test_uv(kx_buf *b, uint64_t value) {
    /* First reserve of the member reference list, after the recipe header. */
    if (inject && !b->cap && ++empty_buffers == 2) b->oom = 1;
    kx_uv(b, value);
}

#define kx_uv test_uv
#define kolmo_archive_bound probe_bound
#define kolmo_archive_create probe_create
#define kolmo_archive_count probe_count
#define kolmo_archive_stat probe_stat
#define kolmo_archive_extract probe_extract
#define kolmo_archive_extract_all probe_all
#define kolmo__archive_walk probe_walk
#ifndef ARCHIVE_IMPL
#define ARCHIVE_IMPL "core/dedup.c"
#endif
#include ARCHIVE_IMPL

enum { WORKERS = 8, BYTES = 16384 };
static uint8_t input[BYTES];
static const kolmo_archive_file member = {"sample", input, BYTES, 0644, 0};
static kolmo_params params;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int ready, start;

typedef struct { uint8_t *bytes; size_t cap; int64_t size; } result;
static void *create_archive(void *arg) {
    result *r = arg;
    pthread_mutex_lock(&lock);
    ready++;
    pthread_cond_broadcast(&cond);
    while (!start) pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);
    r->size = probe_create(&member, 1, r->bytes, r->cap, &params, NULL, NULL);
    return NULL;
}
int main(int argc, char **argv) {
    (void)argv;
    for (size_t i = 0; i < BYTES; i++) input[i] = (uint8_t)(i ^ (i >> 5));
    params.mode = KOLMO_RT;
    params.threads = 1;
    const size_t cap = probe_bound(&member, 1, &params);
    if (argc > 1) {
        uint8_t *out = malloc(cap);
        if (!out) return 2;
        inject = 1;
        int64_t rc = probe_create(&member, 1, out, cap, &params, NULL, NULL);
        printf("Failed member recipe reserve returned %lld; expected %d\n", (long long)rc, KOLMO_ERR_ALLOC);
        free(out);
        return rc != KOLMO_ERR_ALLOC;
    }
    result results[WORKERS];
    pthread_t threads[WORKERS];
    for (int i = 0; i < WORKERS; i++) {
        results[i] = (result){malloc(cap), cap, 0};
        if (!results[i].bytes || pthread_create(&threads[i], NULL, create_archive, &results[i])) return 2;
    }
    pthread_mutex_lock(&lock);
    while (ready != WORKERS) pthread_cond_wait(&cond, &lock);
    start = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    for (int i = 0; i < WORKERS; i++) pthread_join(threads[i], NULL);
    uint8_t decoded[BYTES];
    for (int i = 0; i < WORKERS; i++) {
        result *r = &results[i];
        if (r->size <= 0 || r->size != results[0].size || memcmp(r->bytes, results[0].bytes, r->size) ||
            probe_extract(r->bytes, r->size, 0, decoded, BYTES, 1) != BYTES || memcmp(decoded, input, BYTES)) return 1;
    }
    for (int i = 0; i < WORKERS; i++) free(results[i].bytes);
    printf("%d concurrent first-use calls produced identical archives and exact restores\n", WORKERS);
    return 0;
}
