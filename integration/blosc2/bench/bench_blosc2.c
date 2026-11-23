/* Single-thread benchmark of Blosc2 codecs: Kolmo (this plugin, registered in-process) against Blosc2's own
 * blosclz, LZ4 and Zstd with the byte shuffle (or bitshuffle) filter in front.
 *
 *   ./build/bench_blosc2 [-r runs] [-c chunk_bytes] [input ...]
 *
 * Inputs as in ../../common/bench_data.h ("path:type[:skip]" or "synth:NAME"); the input's element size is the Blosc2
 * typesize. The data is cut into chunks (4 MiB by default) compressed with blosc2_compress_ctx and decompressed with
 * blosc2_decompress_ctx, nthreads = 1, Blosc2's automatic block size unless stated. Times are the best of `runs`
 * passes over all chunks; speeds are uncompressed MB/s (10^6 bytes); sizes include Blosc2's chunk headers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "../blosc2_kolmo.h"
#include "../../common/bench_data.h"

typedef struct { const char *label; int codec; int clevel; int filter; int32_t blocksize; } config;

#define WHOLE (-1)   /* blocksize = the chunk */

static const config CONFIGS[] = {
    {"blosclz 5 + shuffle",       BLOSC_BLOSCLZ,     5, BLOSC_SHUFFLE,    0},
    {"lz4 5 + shuffle",           BLOSC_LZ4,         5, BLOSC_SHUFFLE,    0},
    {"lz4 5 + bitshuffle",        BLOSC_LZ4,         5, BLOSC_BITSHUFFLE, 0},
    {"zstd 1 + shuffle",          BLOSC_ZSTD,        1, BLOSC_SHUFFLE,    0},
    {"zstd 5 + shuffle",          BLOSC_ZSTD,        5, BLOSC_SHUFFLE,    0},
    {"zstd 9 + shuffle",          BLOSC_ZSTD,        9, BLOSC_SHUFFLE,    0},
    {"zstd 5 + bitshuffle",       BLOSC_ZSTD,        5, BLOSC_BITSHUFFLE, 0},
    {"kolmo 1",                   BLOSC_CODEC_KOLMO, 1, BLOSC_NOSHUFFLE,  0},
    {"kolmo 3",                   BLOSC_CODEC_KOLMO, 3, BLOSC_NOSHUFFLE,  0},
    {"kolmo 5",                   BLOSC_CODEC_KOLMO, 5, BLOSC_NOSHUFFLE,  0},
    {"kolmo 3 + shuffle",         BLOSC_CODEC_KOLMO, 3, BLOSC_SHUFFLE,    0},
    {"kolmo 3, block = chunk",    BLOSC_CODEC_KOLMO, 3, BLOSC_NOSHUFFLE,  WHOLE},
    {"kolmo 5, block = chunk",    BLOSC_CODEC_KOLMO, 5, BLOSC_NOSHUFFLE,  WHOLE},
};

int main(int argc, char **argv) {
    int runs = 3;
    int32_t chunk_bytes = 4 << 20;
    const char *defaults[] = {"synth:f32_grid", "synth:i64_ticks", "synth:i16_wave"};
    const char **inputs = defaults;
    int ninputs = 3, a = 1;
    for (; a < argc && argv[a][0] == '-'; a++) {
        if (!strcmp(argv[a], "-r") && a + 1 < argc) runs = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-c") && a + 1 < argc) chunk_bytes = (int32_t)atol(argv[++a]);
        else { fprintf(stderr, "usage: %s [-r runs] [-c chunk_bytes] [path:type[:skip] | synth:NAME ...]\n", argv[0]); return 2; }
    }
    if (a < argc) { inputs = (const char **)(argv + a); ninputs = argc - a; }

    blosc2_init();
    if (blosc2_kolmo_register() != 0) { fprintf(stderr, "cannot register the Kolmo codec\n"); return 1; }
    printf("c-blosc2 %s, one thread, %d-byte chunks, best of %d; MB/s of uncompressed data\n\n",
           BLOSC2_VERSION_STRING, chunk_bytes, runs);
    for (int k = 0; k < ninputs; k++) {
        bench_input in;
        if (bench_load(inputs[k], &in) != 0) continue;
        size_t nchunks = (in.bytes + chunk_bytes - 1) / chunk_bytes;
        uint8_t **c = calloc(nchunks, sizeof *c), *back = malloc(in.bytes);
        int *csz = calloc(nchunks, sizeof *csz);
        for (size_t i = 0; i < nchunks; i++) c[i] = malloc((size_t)chunk_bytes + BLOSC2_MAX_OVERHEAD);
        printf("### %s (%s, typesize %d, %.2f MB)\n\n| codec | ratio | compress MB/s | decompress MB/s |\n|---|---:|---:|---:|\n",
               in.name, in.type, in.esize, in.bytes / 1e6);
        for (size_t f = 0; f < sizeof CONFIGS / sizeof CONFIGS[0]; f++) {
            const config *cf = &CONFIGS[f];
            blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
            blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
            cp.compcode = (uint8_t)cf->codec; cp.clevel = (uint8_t)cf->clevel; cp.typesize = in.esize; cp.nthreads = 1;
            cp.blocksize = cf->blocksize == WHOLE ? chunk_bytes : cf->blocksize;
            cp.filters[BLOSC2_MAX_FILTERS - 1] = (uint8_t)cf->filter;
            dp.nthreads = 1;
            blosc2_context *cc = blosc2_create_cctx(cp), *dc = blosc2_create_dctx(dp);
            double best_c = 1e30, best_d = 1e30;
            size_t total = 0;
            int ok = 1;
            for (int r = 0; r < runs && ok; r++) {
                total = 0;
                double t0 = bench_now();
                for (size_t i = 0; i < nchunks && ok; i++) {
                    size_t off = i * (size_t)chunk_bytes, len = in.bytes - off < (size_t)chunk_bytes ? in.bytes - off : (size_t)chunk_bytes;
                    csz[i] = blosc2_compress_ctx(cc, in.data + off, (int32_t)len, c[i], chunk_bytes + BLOSC2_MAX_OVERHEAD);
                    if (csz[i] <= 0) ok = 0;
                    total += (size_t)csz[i];
                }
                double t1 = bench_now();
                for (size_t i = 0; i < nchunks && ok; i++) {
                    size_t off = i * (size_t)chunk_bytes, len = in.bytes - off < (size_t)chunk_bytes ? in.bytes - off : (size_t)chunk_bytes;
                    if (blosc2_decompress_ctx(dc, c[i], csz[i], back + off, (int32_t)len) != (int)len) ok = 0;
                }
                double t2 = bench_now();
                if (ok && memcmp(back, in.data, in.bytes) != 0) ok = 0;
                if (t1 - t0 < best_c) best_c = t1 - t0;
                if (t2 - t1 < best_d) best_d = t2 - t1;
            }
            blosc2_free_ctx(cc); blosc2_free_ctx(dc);
            if (!ok) { printf("| %s | (failed) | | |\n", cf->label); continue; }
            printf("| %s | %.2f | %.0f | %.0f |\n", cf->label, (double)in.bytes / total, in.bytes / best_c / 1e6, in.bytes / best_d / 1e6);
            fflush(stdout);
        }
        printf("\n");
        for (size_t i = 0; i < nchunks; i++) free(c[i]);
        free(c); free(csz); free(back); free(in.data);
    }
    blosc2_destroy();
    return 0;
}
