/* blosc2_kolmo: Kolmo as a Blosc2 codec plugin (see blosc2_kolmo.h for the codec ID and settings).
 *
 * Two ways in:
 *   - linked into an application, which calls blosc2_kolmo_register() after blosc2_init();
 *   - as a dynamically loaded plugin (once the codec has a global ID in c-blosc2's registry): Blosc2 opens
 *     libblosc2_kolmo, reads the `info` symbol below and looks up the encoder and decoder it names.
 *
 * Licence: Apache-2.0.
 */
#include <stdlib.h>
#include <string.h>

#include "kolmo.h"
#include "blosc2_kolmo.h"

#if defined(_WIN32)
#define B2K_EXPORT __declspec(dllexport)
#else
#define B2K_EXPORT __attribute__((visibility("default")))
#endif

/* The layout c-blosc2 reads from a dynamically loaded codec (plugins/codecs-registry.h: codec_info). */
typedef struct { char *encoder; char *decoder; } blosc2_kolmo_codec_info;
B2K_EXPORT blosc2_kolmo_codec_info info = {"blosc2_kolmo_encoder", "blosc2_kolmo_decoder"};

B2K_EXPORT int blosc2_kolmo_encoder(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                                    uint8_t meta, blosc2_cparams *cparams, const void *chunk) {
    kolmo_params p;
    int level = meta ? meta : (cparams && cparams->clevel > 0 ? cparams->clevel : KOLMO_LEVEL_DEFAULT);
    (void)chunk;
    if (input_len < 0 || output_len < 0) return BLOSC2_ERROR_INVALID_PARAM;
    memset(&p, 0, sizeof p);
    if (kolmo_params_for_level(level, &p) != 0) return BLOSC2_ERROR_INVALID_PARAM;
    p.threads = 1;

    size_t bound = kolmo_bound((size_t)input_len, &p);
    if (bound <= (size_t)output_len) {
        int64_t r = kolmo_compress(input, (size_t)input_len, output, (size_t)output_len, &p);
        return r < 0 ? BLOSC2_ERROR_CODEC_SUPPORT : (int)r;
    }
    /* Blosc2 offers less room than Kolmo's worst case: code into scratch and keep it only if it fits. 0 tells
     * Blosc2 the block did not compress (it then stores it as is). */
    uint8_t *tmp = (uint8_t *)malloc(bound);
    if (!tmp) return BLOSC2_ERROR_MEMORY_ALLOC;
    int64_t r = kolmo_compress(input, (size_t)input_len, tmp, bound, &p);
    int ret;
    if (r < 0) ret = BLOSC2_ERROR_CODEC_SUPPORT;
    else if (r > output_len) ret = 0;
    else { memcpy(output, tmp, (size_t)r); ret = (int)r; }
    free(tmp);
    return ret;
}

B2K_EXPORT int blosc2_kolmo_decoder(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                                    uint8_t meta, blosc2_dparams *dparams, const void *chunk) {
    kolmo_decode_limits lim;
    (void)meta; (void)dparams; (void)chunk;
    if (input_len < 0 || output_len < 0) return BLOSC2_ERROR_INVALID_PARAM;
    memset(&lim, 0, sizeof lim);
    lim.max_output_bytes = (uint64_t)output_len;        /* a block never decodes to more than Blosc2 expects */
    int64_t r = kolmo_decompress_limited(input, (size_t)input_len, output, (size_t)output_len, 1, &lim);
    return r < 0 ? BLOSC2_ERROR_CODEC_SUPPORT : (int)r;
}

B2K_EXPORT int blosc2_kolmo_register(void) {
    static blosc2_codec codec;
    codec.compcode = BLOSC_CODEC_KOLMO;
    codec.compname = (char *)"kolmo";
    codec.complib = BLOSC_CODEC_KOLMO;
    codec.version = BLOSC2_KOLMO_VERSION;
    codec.encoder = blosc2_kolmo_encoder;
    codec.decoder = blosc2_kolmo_decoder;
    return blosc2_register_codec(&codec);
}
