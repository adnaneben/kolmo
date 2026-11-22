/* libFuzzer target: Kolmo's own coders on raw streams, the ones every default frame uses (KLZ, KLZH, KLZH2, KLZMA,
 * khuf, krans). Input: selector u8 | decoded size u16 (little-endian, x 16) | stream. Each decoder must stay inside
 * its buffers whatever the stream says and either fail or fill exactly the size asked for. fuzz/make_v12_seeds writes
 * valid streams of every coder in this layout (fuzz/corpus/native) for mutation. Link with build/libkolmoarch.a (the
 * coders are compiled on their own; no src/kolmo.c needed). */
#include "algo/khuf.h"
#include "algo/klz.h"
#include "algo/klzma.h"
#include "algo/krans.h"
#include <stdlib.h>
#include <string.h>
typedef int (*dfn)(const uint8_t *, size_t, uint8_t *, size_t);
static const dfn D[] = {kolmo_khuf_decompress_chunks, kolmo_khuf_decompress, kolmo_klz_decompress, kolmo_klzh_decompress,
                        kolmo_klzma_decompress, kolmo_klzh2_decompress, kolmo_krans_decompress};
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3) return 0;
    const int sel = data[0] % (int)(sizeof D / sizeof *D);
    const size_t n = ((size_t)data[1] | (size_t)data[2] << 8) * 16;
    uint8_t *out = malloc(n + 1); if (!out) return 0;
    D[sel](data + 3, size - 3, out, n);
    free(out);
    return 0;
}
