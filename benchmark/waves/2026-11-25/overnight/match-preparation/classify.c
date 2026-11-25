/* Time only computation of KLZH2 worker bucket classes on the original file bytes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

#define CLASSIFY_BODY(CLASS)                                 \
    uint64_t sum = 0;                                        \
    for (size_t i = 0; i + 8 <= n; ++i) {                    \
        uint32_t word; memcpy(&word, s + i, sizeof(word));   \
        uint32_t h = (word * UINT32_C(2654435761)) >> 14;    \
        sum += (CLASS);                                      \
    }                                                        \
    return sum

__attribute__((noinline))
static uint64_t direct(const uint8_t *s, size_t n, unsigned k) {
    CLASSIFY_BODY(h % k);
}

__attribute__((noinline))
static uint64_t reciprocal(const uint8_t *s, size_t n, unsigned k) {
    uint64_t m = ((UINT64_C(1) << 32) + k - 1) / k;
    CLASSIFY_BODY(h - (uint32_t)((h * m) >> 32) * k);
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    unsigned k = (unsigned)atoi(argv[2]);
    if (k < 1 || k > 16) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f || fseek(f, 0, SEEK_END)) return 1;
    long length = ftell(f);
    if (length < 8 || fseek(f, 0, SEEK_SET)) return 1;
    uint8_t *data = malloc((size_t)length);
    if (!data || fread(data, 1, length, f) != (size_t)length) return 1;
    fclose(f);
    uint64_t expected = direct(data, length, k);
    for (int rep = 0; rep < 11; ++rep) {
        for (int arm = 0; arm < 3; ++arm) {
            int current = (arm + rep) % 3;
            double start = seconds();
            __asm__ volatile("" : : "r"(data) : "memory");
            uint64_t sum = current == 2 ? reciprocal(data, length, k) : direct(data, length, k);
            double elapsed = seconds() - start;
            if (sum != expected) return 1;
            printf("%d,%d,%ld,%u,%llu,%.9f\n", rep, current, length, k,
                   (unsigned long long)sum, elapsed);
        }
    }
    free(data);
}
