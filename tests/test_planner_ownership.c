/* Regression for record plans displaced by later graph candidates.
 * Includes the REAL core: no detector or compression-cost stubs.
 * Only malloc/free are observed; allocation requests and results are unchanged.
 * This is deliberately single-threaded and is run under ASan/UBSan/LSan.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *ownership_malloc(size_t n, const char *caller);
static void ownership_free(void *p);
#define malloc(n) ownership_malloc((n), __func__)
#define free(p) ownership_free(p)
#include "../src/kolmo.c"
#undef malloc
#undef free

static void *records[1024];
static size_t nrecords, record_allocations;
static int observe;
static void *ownership_malloc(size_t n, const char *caller) {
    void *p = malloc(n);
    if (observe && p && n == sizeof(rec_plan_t) && !strcmp(caller, "make_plan")) {
        if (nrecords == sizeof(records) / sizeof(records[0])) abort();
        records[nrecords++] = p;
        record_allocations++;
    }
    return p;
}
static void ownership_free(void *p) {
    if (p) for (size_t i = 0; i < nrecords; i++) if (records[i] == p) {
        records[i] = records[--nrecords]; records[nrecords] = NULL; break;
    }
    free(p);
}
static uint64_t state;
static uint64_t next_random(void) {
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return state;
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static size_t fill(uint8_t *a, int kind, uint64_t seed) {
    state = seed;
    if (kind == 0) {
        const size_t count = 300000, head = 3;
        memcpy(a, "ABC", head);
        for (size_t i = 0; i < count; i++) {
            double g = 0;
            for (int k = 0; k < 4; k++) g += (double)(next_random() % 20001) / 10000.0 - 1.0;
            float f = i % 5000 == 7 ? 0.0f : (float)(g * 0.02);
            uint32_t u; memcpy(&u, &f, 4); put32(a + head + 4 * i, u);
        }
        a[head + 4 * count] = 1; a[head + 4 * count + 1] = 2;
        return head + 4 * count + 2;
    }
    if (kind == 1) {
        const size_t count = 300000, head = 128;
        static const char header[] = "\x93NUMPY\x01\x00\x76\x00{'descr': '<i4', 'fortran_order': False, 'shape': (300000,), }";
        memset(a, ' ', head); memcpy(a, header, sizeof(header) - 1); a[head - 1] = '\n';
        uint32_t v = 0;
        for (size_t i = 0; i < count; i++) {
            v += (uint32_t)((int32_t)(next_random() % 21) - 10);
            put32(a + head + 4 * i, v);
        }
        return head + 4 * count;
    }
    const size_t width = 640, height = 300, channels = 3;
    for (size_t y = 0; y < height; y++) for (size_t x = 0; x < width; x++) for (size_t c = 0; c < channels; c++) {
        int v = (int)((x * 37 + c * 11) % 199) + (int)(y / 4) + (int)(next_random() % 5) - 2;
        a[(y * width + x) * channels + c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    for (size_t i = 0; i < 7; i++) a[width * height * channels + i] = (uint8_t)i;
    return width * height * channels + 7;
}
int main(void) {
    uint8_t *a = malloc(2u << 20);
    if (!a) return 2;
    const uint64_t seeds[] = {88172645463325252ull, 731094ull, 1700000001ull};
    size_t cases = 0, graph_cases = 0, leaked_cases = 0;
    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) for (int kind = 0; kind < 3; kind++) {
        size_t n = fill(a, kind, seeds[s]);
        for (int effort = 0; effort <= 1; effort++) for (int quick = 0; quick <= 1; quick++) {
            size_t before = nrecords;
            observe = 1;
            plan_t p = make_plan(a, n, effort, 1, 1, 1, 0, quick ? 4 : 0);
            observe = 0;
            graph_cases += p.t == T_KG;
            ownership_free(p.fs); ownership_free(p.rec);
            leaked_cases += nrecords != before;
            printf("seed=%llu kind=%d effort=%d light=%d plan=%d live_records=%zu\n",
                   (unsigned long long)seeds[s], kind, effort, quick ? 4 : 0, p.t, nrecords);
            cases++;
        }
    }
    free(a);
    printf("PLANNER_OWNERSHIP cases=%zu record_allocations=%zu graph_cases=%zu leaked_cases=%zu outstanding=%zu\n",
           cases, record_allocations, graph_cases, leaked_cases, nrecords);
    size_t outstanding = nrecords;
    /* Drop observer references so they cannot hide baseline leaks from LSan. */
    memset(records, 0, sizeof(records)); nrecords = 0;
    fflush(stdout);
    if (!record_allocations || !graph_cases) {
        fprintf(stderr, "FAIL insufficient planner-path coverage\n"); return 2;
    }
    if (outstanding) { fprintf(stderr, "FAIL planner record ownership\n"); return 1; }
    puts("PLANNER OWNERSHIP PASSED");
    return 0;
}
