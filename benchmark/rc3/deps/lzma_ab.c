/* LZMA2 raw coder A/B (deps lane, rc3): the same program linked with the system liblzma or with the vendored subset.
 * usage: lzma_ab PRESET(e.g. 9e) REPS FILE...  prints per file: size, compressed, best compress and decompress MB/s.
 * Settings as Kolmo's codec 6: preset, dictionary = next power of two >= n (4 KiB..64 MiB). */
#ifdef VENDORED
#include "kolmo_lzma.h"
#else
#include <lzma.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    uint32_t pr = (uint32_t)atoi(argv[1]) | (strchr(argv[1], 'e') ? LZMA_PRESET_EXTREME : 0); int reps = atoi(argv[2]);
    double ti = 0, tc = 0, td = 0; size_t si = 0, so = 0;
    for (int a = 3; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) return 1; fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); rewind(f);
        uint8_t *in = malloc(n), *out = malloc(n + n / 2 + 4096), *back = malloc(n); if (fread(in, 1, n, f) != n) return 1; fclose(f);
        lzma_options_lzma o; lzma_lzma_preset(&o, pr); uint32_t d = 4096; while (d < n && d < (64u << 20)) d <<= 1; o.dict_size = d;
        lzma_filter fl[2] = {{LZMA_FILTER_LZMA2, &o}, {LZMA_VLI_UNKNOWN, NULL}};
        double bc = 1e9, bd = 1e9; size_t pos = 0;
        for (int r = 0; r < reps; r++) {
            pos = 0; double t = now(); if (lzma_raw_buffer_encode(fl, NULL, in, n, out, &pos, n + n / 2 + 4096) != LZMA_OK) return 2; t = now() - t; if (t < bc) bc = t;
            size_t ip = 0, op = 0; t = now(); if (lzma_raw_buffer_decode(fl, NULL, out, &ip, pos, back, &op, n) != LZMA_OK || op != n || memcmp(in, back, n)) return 3; t = now() - t; if (t < bd) bd = t;
        }
        printf("%-12s %10zu %10zu  c %7.2f MB/s  d %7.1f MB/s\n", strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a], n, pos, n / bc / 1e6, n / bd / 1e6);
        si += n; so += pos; tc += bc; td += bd; ti += n; free(in); free(out); free(back);
    }
    printf("%-12s %10zu %10zu  c %7.2f MB/s  d %7.1f MB/s\n", "TOTAL", si, so, ti / tc / 1e6, ti / td / 1e6);
    return 0;
}
