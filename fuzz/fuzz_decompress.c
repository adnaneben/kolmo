/* OSS-Fuzz target: kolmo_decompress must never crash or read out of bounds on arbitrary input,
 * and must never return success with wrong-length output. Build in OSS-Fuzz with build.sh; run
 * locally with fuzz/run_local.c under ASan.
 * Mutated frames almost never keep a valid header CRC (CRC-32C over the header and the block index), which stops
 * them before any block decoder. So each input is decoded twice: as given, and with the header CRC recomputed
 * (fuzz/fuzz_fixup.h), which lets mutations of the index and the payloads reach the block decoders. With the CRC
 * fixed, a successful decode must still be exactly the claimed size, and a range read must agree with it. */
#include "kolmo.h"
#include "fuzz_fixup.h"
#include <stdlib.h>
#include <string.h>
#define CAP (16u << 20)
static void one(const uint8_t *data, size_t size) {
    uint8_t out_stack[1 << 16];
    kolmo_decompress(data, size, out_stack, sizeof out_stack, 1);   /* a bounded output buffer */
    int64_t orig = kolmo_original_size(data, size);
    if (orig < 0 || orig > (int64_t)CAP) return;
    if (kolmo_check_frame(data, size, NULL)) return;
    uint8_t *o = malloc((size_t)orig + 1); if (!o) return;
    for (int T = 1; T <= 2; T++) {                                   /* the exact buffer, one and two threads */
        int64_t r = kolmo_decompress(data, size, o, (size_t)orig, T);
        if (r >= 0 && r != orig) __builtin_trap();
        if (r == orig && orig > 2) {
            uint64_t off = (uint64_t)orig / 3; size_t len = (size_t)(orig - (int64_t)off) / 2 + 1;
            uint8_t *rr = malloc(len);
            if (rr) {
                int64_t k = kolmo_read_range(data, size, off, len, rr, T);
                if (k >= 0 && ((size_t)k != len || memcmp(rr, o + off, len))) __builtin_trap();
                free(rr);
            }
            break;
        }
    }
    free(o);
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4 || size > CAP) return 0;
    one(data, size);
    if (size >= 32 && !memcmp(data, "CZN1", 4)) {
        uint8_t *c = malloc(size); if (!c) return 0;
        memcpy(c, data, size); fz_fix_frame(c, size); fz_fix_kext(c, size);
        if (memcmp(c, data, size)) one(c, size);
        free(c);
    }
    return 0;
}
