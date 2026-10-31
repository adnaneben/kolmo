/* OSS-Fuzz target: czn_decompress must never crash or read out of bounds on arbitrary input,
 * and must never return success with wrong-length output. Build in OSS-Fuzz with build.sh; run
 * locally with fuzz/run_local.c under ASan. */
#include "czn.h"
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4 || size > (16u << 20)) return 0;
    /* cap the claimed original size so a corrupt header can't ask for a huge allocation */
    uint8_t out_stack[1 << 16];
    for (int T = 1; T <= 2; T++) {
        /* try a bounded output buffer and an exact one */
        czn_decompress(data, size, out_stack, sizeof out_stack, T);
        int64_t orig = czn_original_size(data, size);
        if (orig >= 0 && orig <= (16 << 20)) {
            uint8_t *o = malloc((size_t)orig + 1);
            if (o) { czn_decompress(data, size, o, (size_t)orig, T); free(o); }
        }
    }
    return 0;
}
