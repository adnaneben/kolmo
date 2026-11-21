/* libFuzzer target: frontier_fastq_decode (src/experimental/frontier_fastq.h). Inputs are KFQ1 frames as written by
 * build/kolmo-fastq; the frame walk mirrors fastq_cli.c's decode(), with the claimed original size capped at 16 MiB.
 * Inputs without the KFQ1 magic are decoded as one raw payload whose output length comes from the first two bytes. */
#include "frontier_fastq.h"
#include <stdlib.h>
#include <string.h>
#define CAP (16u << 20)
#define STORED 0xFFFFFFFFu
static uint32_t g32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
int LLVMFuzzerTestOneInput(const uint8_t *in, size_t n) {
    if (n > CAP) return 0;
    if (n < 16 || memcmp(in, "KFQ1", 4)) {
        if (n < 3) return 0;
        size_t raw = 1 + ((size_t)in[0] | (size_t)in[1] << 8) * 4;
        uint8_t *o = malloc(raw); if (!o) return 0;
        frontier_fastq_decode(in + 2, n - 2, o, raw); free(o); return 0;
    }
    uint64_t raw = (uint64_t)g32(in + 4) | (uint64_t)g32(in + 8) << 32;
    if (raw > CAP) return 0;
    uint8_t *out = malloc(raw ? (size_t)raw : 1); if (!out) return 0;
    size_t i = 16, o = 0;
    while (o < raw) {
        if (n - i < 8) break;
        uint32_t len = g32(in + i), c = g32(in + i + 4); i += 8;
        if (len == 0 || len > raw - o) break;
        if (c == STORED) { if (n - i < len) break; memcpy(out + o, in + i, len); i += len; }
        else { if (n - i < c || frontier_fastq_decode(in + i, c, out + o, len)) break; i += c; }
        o += len;
    }
    free(out);
    return 0;
}
