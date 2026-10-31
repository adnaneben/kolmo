/* OSS-Fuzz target: every mode must round-trip arbitrary input exactly. The first byte picks the mode. */
#include "czn.h"
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    int mode = data[0] % 6; data++; size--;
    if (size > (8u << 20)) size = 8u << 20;
    czn_params p = {(czn_mode)mode, 0, 1, 0, 1};
    size_t cap = czn_bound(size, &p); uint8_t *c = malloc(cap + 1), *r = malloc(size + 1);
    if (!c || !r) { free(c); free(r); return 0; }
    int64_t k = czn_compress(data, size, c, cap, &p);
    if (k > 0) {
        int64_t d = czn_decompress(c, (size_t)k, r, size, 1);
        if (d != (int64_t)size || (size && memcmp(r, data, size))) __builtin_trap();  /* lossy: a real bug */
    }
    free(c); free(r); return 0;
}
