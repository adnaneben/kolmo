/* OSS-Fuzz target: every mode must round-trip arbitrary input exactly. The first byte picks the mode. */
#include "kolmo.h"
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
#ifdef KOLMO_FUZZ_NO_CM
    /* fast build (campaign 2): no ultra/context-mixing mode (its decoder is stubbed out); the other bits of the first
     * byte pick the analysis (quick / full) and the native-leaf / typed-Huffman opt-ins */
    int mode = data[0] % 5, tr = (data[0] & 8 ? 2 : 1) | (data[0] & 16 ? KOLMO_T_NATIVE_LEAF : 0) | (data[0] & 32 ? KOLMO_T_TYPED_HUF : 0);
#else
    int mode = data[0] % 6, tr = 1;
#endif
    data++; size--;
    if (size > (8u << 20)) size = 8u << 20;
    kolmo_params p = {(kolmo_mode)mode, 0, 1, 0, tr};
    size_t cap = kolmo_bound(size, &p); uint8_t *c = malloc(cap + 1), *r = malloc(size + 1);
    if (!c || !r) { free(c); free(r); return 0; }
    int64_t k = kolmo_compress(data, size, c, cap, &p);
    if (k > 0) {
        int64_t d = kolmo_decompress(c, (size_t)k, r, size, 1);
        if (d != (int64_t)size || (size && memcmp(r, data, size))) __builtin_trap();  /* lossy: a real bug */
    }
    free(c); free(r); return 0;
}
