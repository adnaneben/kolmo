#include "codecs.h"
#include "czn.h"
int64_t lzbench_kolmo_compress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
    czn_params p = {(czn_mode)o->level, 0, o->threads > 0 ? o->threads : 1, (uint32_t)o->additional_param, 1};
    return czn_compress((const uint8_t *)in, n, (uint8_t *)out, cap, &p);
}
int64_t lzbench_kolmo_decompress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
    return czn_decompress((const uint8_t *)in, n, (uint8_t *)out, cap, o->threads > 0 ? o->threads : 1);
}
