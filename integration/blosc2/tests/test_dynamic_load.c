/* The Kolmo codec loaded by c-blosc2 itself, as a dynamic plugin: this program does not link the plugin. It registers
 * the codec's ID and name with no encoder or decoder, exactly as c-blosc2's registry (plugins/codecs/codecs-registry.c)
 * lists external codecs such as openzl and grok; on first use Blosc2 opens libblosc2_kolmo.so (found through
 * LD_LIBRARY_PATH / DYLD_LIBRARY_PATH, or through `python -c "import blosc2_kolmo; blosc2_kolmo.print_libpath()"`),
 * reads its `info` symbol and loads the encoder and decoder named there.
 *
 *   DYLD_LIBRARY_PATH=build/plugin ./build/test_dynamic_load     (Linux: LD_LIBRARY_PATH)
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"

#define BLOSC_CODEC_KOLMO 244   /* PLACEHOLDER, as in ../blosc2_kolmo.h */

int main(void) {
    blosc2_init();
    blosc2_codec codec = {0};
    codec.compcode = BLOSC_CODEC_KOLMO;
    codec.compname = (char *)"kolmo";
    codec.complib = BLOSC_CODEC_KOLMO;
    codec.version = 1;
    codec.encoder = NULL;   /* loaded from libblosc2_kolmo.so on first use */
    codec.decoder = NULL;
    int rc = blosc2_register_codec(&codec);
    if (rc != 0) { printf("FAIL: register returned %d\n", rc); return 1; }

    const int32_t n = 1 << 20;
    double *src = malloc(n), *back = malloc(n);
    for (int i = 0; i < n / 8; i++) src[i] = round((1000 + i * 0.5 + 20 * sin(i * 0.01)) * 100) / 100;
    uint8_t *c = malloc(n + BLOSC2_MAX_OVERHEAD);
    blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
    cp.compcode = BLOSC_CODEC_KOLMO; cp.clevel = 3; cp.typesize = 8; cp.nthreads = 1;
    cp.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOSHUFFLE;
    blosc2_context *cc = blosc2_create_cctx(cp), *dc = blosc2_create_dctx(dp);
    int cs = blosc2_compress_ctx(cc, src, n, c, n + BLOSC2_MAX_OVERHEAD);
    int ds = cs > 0 ? blosc2_decompress_ctx(dc, c, cs, back, n) : -1;
    int ok = cs > 0 && cs < n / 4 && ds == n && memcmp(src, back, n) == 0;
    printf("%s dynamically loaded codec: %d -> %d bytes, decoded %d\n", ok ? "ok " : "FAIL", n, cs, ds);
    blosc2_free_ctx(cc); blosc2_free_ctx(dc);
    free(src); free(back); free(c);
    blosc2_destroy();
    if (ok) printf("Blosc2 dynamic plugin test passed\n");
    return !ok;
}
