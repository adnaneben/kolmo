/* OSS-Fuzz target for the native plane-context graph leaf (codec 15). The first byte picks the mode:
 * even: decode the rest as a leaf payload (must never crash or read out of bounds; any result is allowed);
 * odd:  encode the rest as 1-8 byte planes and require an exact round trip; then corrupt that valid leaf (bytes taken
 *       from the input) and decode it, so the table, lane and word-stream checks see near-valid streams. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
size_t np_encode_pub(const uint8_t *s, size_t n, int np, uint8_t *out, size_t cap);
int np_decode_pub(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
size_t np_bound_pub(size_t n);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;
    uint8_t sel = data[0]; data++; size--;
    if (size > (1u << 20)) size = 1u << 20;
    if (!(sel & 1)) {                                           /* raw length from the selector: 1..8 planes of up to 64 KiB */
        size_t np = 1 + (sel >> 1) % 8, L = 8 + (size_t)data[0] * 256; size_t n = np * L;
        uint8_t *o = malloc(n); if (!o) return 0;
        np_decode_pub(data, size, o, n); free(o); return 0;
    }
    int np = 1 + (sel >> 1) % 8; size_t n = size / (size_t)np * (size_t)np; if (!n) return 0;
    size_t cap = np_bound_pub(n); uint8_t *e = malloc(cap), *o = malloc(n);
    if (e && o) {
        size_t z = np_encode_pub(data, n, np, e, cap);
        if (z && (np_decode_pub(e, z, o, n) || memcmp(o, data, n))) __builtin_trap();   /* lossy: a real bug */
        for (size_t k = 0; z && k + 1 < size && k < 64; k += 2) {
            e[data[k] * 131u % z] ^= (uint8_t)(data[k + 1] | 1);
            np_decode_pub(e, (sel & 2) ? z : data[k] * 257u % z, o, n);
        }
    }
    free(e); free(o); return 0;
}
