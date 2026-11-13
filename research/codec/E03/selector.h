/* E03 research-only bounded sample gate. Uses frozen E02C packing primitives. */
#ifndef E03_SELECTOR_H
#define E03_SELECTOR_H
#include "../E02C/packing.h"
typedef struct {
    size_t values, original_bytes, local_bytes, patched_bytes;
    unsigned method;
} e03_sample;
static int e03_sample_method(const uint8_t *res, size_t count, unsigned ow,
                             unsigned planes, e03_sample *s) {
    memset(s, 0, sizeof *s);
    if (count > EC_MAX_VALUES || (ow != 1 && ow != 2 && ow != 4 && ow != 8) || planes > 1)
        return -1;
    if (count < 1024) return 0;
    if (!res) return -1;
    uint64_t values[1024];
    size_t blocks = (count + 127) / 128, take = blocks < 8 ? blocks : 8;
    for (size_t k = 0; k < take; k++) {
        size_t block = take == 1 ? 0 : k * (blocks - 1) / (take - 1);
        size_t at = block * 128, n = count - at < 128 ? count - at : 128;
        for (size_t i = 0; i < n; i++) {
            uint64_t x = 0;
            for (unsigned j = 0; j < ow; j++)
                x |= (uint64_t)res[planes ? j * count + at + i : (at + i) * ow + j] << (8 * j);
            values[s->values++] = x;
        }
    }
    ec_buf local = {0}, patched = {0};
    int bad = ec_pack(values, s->values, 2, &local) || ec_pack(values, s->values, 3, &patched);
    if (!bad) {
        s->original_bytes = s->values * ow;
        s->local_bytes = local.n; s->patched_bytes = patched.n;
        unsigned method = patched.n * 10 <= local.n * 9 ? 3 : 2;
        size_t estimated = method == 3 ? patched.n : local.n;
        if (estimated * 8 <= s->original_bytes * 7 &&
            (uint64_t)(s->original_bytes - estimated) * count >= (uint64_t)64 * s->values)
            s->method = method;
    }
    ec_free(&local); ec_free(&patched);
    return bad ? -1 : 0;
}
#endif
