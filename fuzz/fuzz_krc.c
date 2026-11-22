/* libFuzzer target: the standalone KRC1 research decoder (src/experimental/krc.h). The claimed size is checked
 * against a 16 MiB cap before allocating; also decodes into a too-small buffer and with two threads. */
#include "krc.h"
#include <stdlib.h>
#include <string.h>
#define CAP (16u << 20)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > CAP) return 0;
    uint8_t small[4096];
    krc_decompress(data, size, small, sizeof small);
    int64_t o = krc_original_size(data, size);
    if (o < 0 || o > (int64_t)CAP) return 0;
    uint8_t *out = malloc((size_t)o + 1); if (!out) return 0;
    int64_t r = krc_decompress(data, size, out, (size_t)o);
    if (r >= 0 && r != o) __builtin_trap();
    krc_decode_threads(data, size, out, (size_t)o, 2);
    free(out);
    return 0;
}
