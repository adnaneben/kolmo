/* Source-level controls and the linked .S symbol on identical real bytes.
 * Repeatedly scan 4 KiB windows, resetting the carry to a deterministic value
 * and restarting after each match. This measures the scanner, not an archive. */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
typedef size_t (*scan_fn)(const uint8_t *, size_t, const uint64_t *, uint64_t, uint64_t *);
size_t kolmo_gear_aarch64(const uint8_t *, size_t, const uint64_t *, uint64_t, uint64_t *);
static size_t scalar(const uint8_t *p, size_t n, const uint64_t *g, uint64_t mask, uint64_t *state) {
    uint64_t h = *state;
    for (size_t i = 0; i < n; i++) {
        h = (h << 1) + g[p[i]];
        if (!(h & mask)) { *state = h; return i + 1; }
    }
    *state = h;
    return 0;
}
static size_t paired(const uint8_t *p, size_t n, const uint64_t *g, uint64_t mask, uint64_t *state) {
    uint64_t h = *state;
    size_t i = 0;
    for (; n - i >= 2; i += 2) {
        uint64_t a = g[p[i]], b = g[p[i+1]];
        uint64_t h1 = (h << 1) + a, h2 = (h << 2) + (a << 1) + b;
        if (!(h1 & mask)) { *state = h1; return i+1; }
        if (!(h2 & mask)) { *state = h2; return i+2; }
        h = h2;
    }
    if (i < n) {
        h = (h << 1) + g[p[i]];
        if (!(h & mask)) { *state = h; return i+1; }
    }
    *state = h;
    return 0;
}
static uint64_t ns(clockid_t clock) {
    struct timespec t;
    if (clock_gettime(clock, &t)) exit(2);
    return (uint64_t)t.tv_sec*1000000000 + t.tv_nsec;
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    size_t cap = 1u << 24;
    uint8_t *p = malloc(cap);
    if (!p || !f) return 2;
    size_t n = fread(p, 1, cap, f);
    if (!n || ferror(f)) return 2;
    fclose(f);
    uint64_t g[256], seed = UINT64_C(0x6b6f6c6d6f646470), mask = 0;
    for (int i = 0; i < 256; i++) {
        uint64_t z = (seed += UINT64_C(0x9e3779b97f4a7c15));
        z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
        g[i] = z ^ (z >> 31);
    }
    for (unsigned i = 0; i < 12; i++) mask |= UINT64_C(1) << (63 - i*47/12);
    scan_fn scans[] = {scalar, paired, kolmo_gear_aarch64};
    const char *names[] = {"scalar","paired-C","asm"};
    uint64_t expect = 0;
    for (int round = -1; round < 15; round++) for (int k = 0; k < 3; k++) {
        int arm = (k + round + 1) % 3;
        size_t off = 0;
        uint64_t checksum = 0, w = ns(CLOCK_MONOTONIC), c = ns(CLOCK_PROCESS_CPUTIME_ID);
        while (off < n) {
            size_t step = n-off < 4096 ? n-off : 4096;
            uint64_t h = 0;
            size_t cut = scans[arm](p+off,step,g,mask,&h);
            off += cut ? cut : step;
            checksum = checksum * 33 + h + off;
        }
        c = ns(CLOCK_PROCESS_CPUTIME_ID)-c;
        w = ns(CLOCK_MONOTONIC)-w;
        if (round == -1 && k == 0) expect = checksum;
        if (expect != checksum) return 1;
        printf("%zu,%d,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",n,round,names[arm],w,c,checksum);
    }
    free(p);
    return 0;
}
