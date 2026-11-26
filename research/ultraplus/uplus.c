/* Ultra+ research harness: runs the engine of src/codecs/upx.c on one file, outside the container.
 *   uplus c IN OUT   write the codec-7 stream (model nibble 11); settings UP_<name> from the environment
 *   uplus d IN OUT   decode it (settings come from the stream)
 * Build: make build/libkolmoarch.a; clang -O3 -mcpu=native -ffp-contract=off -Isrc research/ultraplus/uplus.c \
 *        build/libkolmoarch.a -o build/uplus -lpthread -lm
 * stderr: one JSON line (sizes, seconds). */
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "plan/detect.c"
#include "codecs/cm.c"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int envi(const char *k, int d) { const char *v = getenv(k); return v && *v ? atoi(v) : d; }
static uint8_t *readf(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*n + 16); if (*n && fread(b, 1, *n, f) != *n) exit(2); fclose(f); return b;
}
static void writef(const char *p, const uint8_t *b, size_t n) { FILE *f = fopen(p, "wb"); if (!f || fwrite(b, 1, n, f) != n) exit(2); fclose(f); }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: uplus c|d IN OUT\n"); return 2; }
    int enc = argv[1][0] == 'c'; size_t n; uint8_t *in = readf(argv[2], &n); double t0 = now();
    if (enc) {
        uint16_t par[UPX_NPAR]; upx_default_par(par);
        for (int i = 0; i < UPX_NPAR; i++) if (upx_par_name[i]) par[i] = (uint16_t)envi(upx_par_name[i], par[i]);
        uint32_t flags = upx_default_flags(looks_text(in, n), n);
        if (envi("UP_M4", 1) == 0) flags &= ~1u;
        if (envi("UP_FULL", 0)) { flags = (flags & ~0xFF0004u) | 3u | (uint32_t)cm4_shift_for(n, CM_MASK3) << 16; }
        if (envi("UP_BIN", -1) >= 0) { uint32_t b = (uint32_t)envi("UP_BIN", 0); uint32_t mk = b ? CM4_MASK_BIN : CM4_MASK_TEXT; flags = (flags & ~0xFF0004u) | (b ? 4u : 0) | (uint32_t)cm4_shift_for(n, mk) << 16; }
        flags |= (uint32_t)envi("UP_H", 0) << 8;
        size_t cap = n + n / 4 + 1024; uint8_t *out = malloc(cap);
        size_t z = upx_compress(in, n, out, cap, flags, par); if (!z) return 1;
        writef(argv[3], out, z);
        fprintf(stderr, "{\"mode\":\"c\",\"n\":%zu,\"z\":%zu,\"flags\":%u,\"code_s\":%.3f,\"need\":%llu}\n", n, z, flags, now() - t0, (unsigned long long)upx_need(n, flags));
    } else {
        if (n < 4) return 1; uint32_t n32; memcpy(&n32, in, 4); size_t N = n32 & 0x0FFFFFFFu;
        uint8_t *out = malloc(N + 8); if (upx_decompress(in, n, out, N)) return 1;
        writef(argv[3], out, N);
        fprintf(stderr, "{\"mode\":\"d\",\"n\":%zu,\"z\":%zu,\"code_s\":%.3f}\n", N, n, now() - t0);
    }
    return 0;
}
