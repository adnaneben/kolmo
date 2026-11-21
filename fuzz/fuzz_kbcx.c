/* libFuzzer target: the standalone KBC1 and KBX3 research decoders (src/experimental/kbc.h, kbx.h). Every input is
 * given to both decoders (the KBX decoder also reads KBC1). Claimed sizes are capped at 16 MiB before allocation. */
#include "kbc.h"
#include "kbx.h"
#include <stdlib.h>
#include <string.h>
#define CAP (16u << 20)
static void one(int64_t (*osz)(const uint8_t *, size_t), int64_t (*dec)(const uint8_t *, size_t, uint8_t *, size_t, int),
                const uint8_t *data, size_t size) {
    uint8_t small[4096];
    dec(data, size, small, sizeof small, 1);
    int64_t o = osz(data, size);
    if (o < 0 || o > (int64_t)CAP) return;
    uint8_t *out = malloc((size_t)o + 1); if (!out) return;
    int64_t r = dec(data, size, out, (size_t)o, 1 + (int)(size & 1));
    if (r >= 0 && r != o) __builtin_trap();
    free(out);
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > CAP) return 0;
    one(kbc_original_size, kbc_decompress, data, size);
    one(kbx_original_size, kbx_decompress, data, size);
    return 0;
}
