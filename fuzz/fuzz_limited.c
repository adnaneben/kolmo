/* libFuzzer target: kolmo_decompress_limited and kolmo_read_range on arbitrary frames. Never trusts a size from the
 * input beyond 16 MiB: the claimed original size is checked against the cap before any allocation. Each input is
 * decoded with (a) generous limits, (b) tight limits derived from the claimed size, (c) a range read of a sub-range. */
#include "kolmo.h"
#include <stdlib.h>
#include <string.h>
#define CAP (16u << 20)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4 || size > CAP) return 0;
    int64_t orig = kolmo_original_size(data, size);
    if (orig < 0 || orig > (int64_t)CAP) {             /* still exercise the limit checks with a small buffer */
        uint8_t small[4096]; kolmo_decode_limits L = {sizeof small, 4096, 4};
        kolmo_decompress_limited(data, size, small, sizeof small, 1, &L);
        return 0;
    }
    size_t o = (size_t)orig;
    uint8_t *out = malloc(o + 1); if (!out) return 0;
    kolmo_decode_limits big = {CAP, CAP, 4096};
    int64_t r = kolmo_decompress_limited(data, size, out, o, 1, &big);
    if (r >= 0 && (uint64_t)r != o) __builtin_trap();   /* success must mean exactly the claimed size */
    kolmo_decode_limits tight = {o / 2 + 1, (uint32_t)(o / 4 + 1), 1 + (uint32_t)(size & 3)};
    kolmo_decompress_limited(data, size, out, o, 2, &tight);
    if (o) {
        uint64_t off = (size * 2654435761u) % o; size_t len = (size_t)((o - off) > 4097 ? 4097 : (o - off));
        uint8_t *rr = malloc(len + 1);
        if (rr) {
            int64_t k = kolmo_read_range(data, size, off, len, rr, 1);
            if (k >= 0 && r == (int64_t)o && ((size_t)k != len || memcmp(rr, out + off, len))) __builtin_trap();
            free(rr);
        }
    }
    free(out);
    return 0;
}
