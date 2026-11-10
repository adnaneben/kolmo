#include "codecs.h"
#include "kolmo.h"
int64_t lzbench_kolmo_compress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
    kolmo_params p = {(kolmo_mode)o->level, 0, o->threads > 0 ? o->threads : 1, (uint32_t)o->additional_param, 1};
    return kolmo_compress((const uint8_t *)in, n, (uint8_t *)out, cap, &p);
}
int64_t lzbench_kolmo_decompress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
    return kolmo_decompress((const uint8_t *)in, n, (uint8_t *)out, cap, o->threads > 0 ? o->threads : 1);
}
