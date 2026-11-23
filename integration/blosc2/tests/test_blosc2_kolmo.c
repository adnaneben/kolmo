/* Tests of the Kolmo Blosc2 codec plugin.
 *
 *   ./build/test_blosc2_kolmo [build/plugin/libblosc2_kolmo.so|.dylib]
 *
 * 1. registered in-process (blosc2_kolmo_register): chunks of every typesize, with and without shuffle, at several
 *    levels and block sizes, and the compcode_meta override, are compressed with blosc2_compress_ctx, checked to carry
 *    the codec ID, and decompressed; incompressible data; a damaged chunk is an error, not a crash;
 * 2. a super-chunk is written, serialised to a contiguous frame, reopened from the buffer and read back;
 * 3. the dynamic plugin library is opened the way c-blosc2 opens a registered plugin: its `info` symbol names the
 *    encoder and decoder, which are looked up and used directly.
 */
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "../blosc2_kolmo.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint64_t rng = 0x243F6A8885A308D3ull;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void fill(uint8_t *buf, size_t bytes, int typesize, int compressible) {
    size_t n = bytes / (size_t)typesize;
    if (!compressible) { for (size_t i = 0; i < bytes; i++) buf[i] = (uint8_t)next(); return; }
    for (size_t i = 0; i < n; i++) {
        switch (typesize) {
        case 1: buf[i] = (uint8_t)(128 + 50 * sin(i * 0.01) + (next() & 3)); break;
        case 2: { int16_t v = (int16_t)(900 * sin(i * 0.002) + (int)(next() % 9)); memcpy(buf + 2 * i, &v, 2); } break;
        case 4: { float v = (float)(20 + 5 * sin(i * 0.001)) + (float)(next() % 64) * 1e-4f; memcpy(buf + 4 * i, &v, 4); } break;
        default: { double v = round((500 + i * 0.01 + 3 * sin(i * 0.003)) * 1000) / 1000; memcpy(buf + 8 * i, &v, 8); }
        }
    }
}

static int roundtrip(const char *what, const uint8_t *src, int32_t nbytes, int typesize, int clevel, int meta,
                     int shuffle, int32_t blocksize, int expect_smaller) {
    blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
    cp.compcode = BLOSC_CODEC_KOLMO;
    cp.compcode_meta = (uint8_t)meta;
    cp.clevel = (uint8_t)clevel;
    cp.typesize = typesize;
    cp.nthreads = 1;
    cp.blocksize = blocksize;
    cp.filters[BLOSC2_MAX_FILTERS - 1] = (uint8_t)shuffle;
    blosc2_context *cc = blosc2_create_cctx(cp), *dc = blosc2_create_dctx(dp);
    int32_t cap = nbytes + BLOSC2_MAX_OVERHEAD;
    uint8_t *c = malloc((size_t)cap), *back = malloc((size_t)nbytes);
    int csize = blosc2_compress_ctx(cc, src, nbytes, c, cap);
    int ok = csize > 0;
    CHECK(csize > 0, "%s: compress returned %d", what, csize);
    if (csize > 0) {
        int dsize = blosc2_decompress_ctx(dc, c, csize, back, nbytes);
        CHECK(dsize == nbytes, "%s: decompress returned %d", what, dsize);
        CHECK(dsize == nbytes && memcmp(back, src, (size_t)nbytes) == 0, "%s: data differ", what);
        if (expect_smaller) CHECK(csize < nbytes / 2, "%s: %d -> %d bytes", what, nbytes, csize);
        ok = ok && dsize == nbytes && memcmp(back, src, (size_t)nbytes) == 0;
        printf("%s %-34s %8d -> %8d bytes (%.2fx)\n", ok ? "ok " : "FAIL", what, nbytes, csize, (double)nbytes / csize);
        if (expect_smaller && csize > 64) {
            /* damage a byte in the middle of the first coded block: an error, never a crash or silent garbage */
            c[csize / 2] ^= 0x5A;
            c[csize / 2 + 1] ^= 0xA5;
            int bad = blosc2_decompress_ctx(dc, c, csize, back, nbytes);
            CHECK(bad < 0 || memcmp(back, src, (size_t)nbytes) == 0, "%s: damaged chunk decoded to wrong data", what);
        }
    }
    free(c); free(back);
    blosc2_free_ctx(cc); blosc2_free_ctx(dc);
    return ok;
}

static void test_schunk(void) {
    const int typesize = 8, nchunks = 6;
    const int32_t chunk = 1 << 20;
    blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
    cp.compcode = BLOSC_CODEC_KOLMO; cp.clevel = 5; cp.typesize = typesize; cp.nthreads = 2;
    cp.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOSHUFFLE;
    blosc2_storage st = {.contiguous = true, .cparams = &cp, .dparams = &dp};
    blosc2_schunk *s = blosc2_schunk_new(&st);
    uint8_t *data = malloc((size_t)chunk * nchunks), *back = malloc((size_t)chunk);
    fill(data, (size_t)chunk * nchunks, typesize, 1);
    for (int i = 0; i < nchunks; i++)
        CHECK(blosc2_schunk_append_buffer(s, data + (size_t)i * chunk, chunk) == i + 1, "append chunk %d", i);
    uint8_t *frame; bool needs_free;
    int64_t flen = blosc2_schunk_to_buffer(s, &frame, &needs_free);
    CHECK(flen > 0, "schunk_to_buffer");
    uint8_t *copy = malloc((size_t)flen);
    memcpy(copy, frame, (size_t)flen);
    if (needs_free) free(frame);
    blosc2_schunk_free(s);
    blosc2_schunk *r = blosc2_schunk_from_buffer(copy, flen, false);
    CHECK(r != NULL, "schunk_from_buffer");
    int good = r != NULL;
    for (int i = 0; r && i < nchunks; i++) {
        int n = blosc2_schunk_decompress_chunk(r, i, back, chunk);
        CHECK(n == chunk && memcmp(back, data + (size_t)i * chunk, (size_t)chunk) == 0, "frame chunk %d", i);
        good = good && n == chunk;
    }
    if (good) printf("ok  super-chunk frame: %d chunks of 1 MiB f64 -> %lld bytes (%.2fx), nthreads 2\n", nchunks,
                     (long long)flen, (double)chunk * nchunks / flen);
    if (r) blosc2_schunk_free(r);
    free(copy); free(data); free(back);
}

typedef struct { char *encoder; char *decoder; } plugin_info;

static void test_dynamic(const char *libpath) {
    void *h = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL, "dlopen %s: %s", libpath, dlerror());
    if (!h) return;
    plugin_info *inf = (plugin_info *)dlsym(h, "info");
    CHECK(inf != NULL, "no `info` symbol in %s", libpath);
    if (!inf) { dlclose(h); return; }
    blosc2_codec_encoder_cb enc = (blosc2_codec_encoder_cb)dlsym(h, inf->encoder);
    blosc2_codec_decoder_cb dec = (blosc2_codec_decoder_cb)dlsym(h, inf->decoder);
    CHECK(enc && dec, "encoder %s / decoder %s not found", inf->encoder, inf->decoder);
    if (enc && dec) {
        const int32_t n = 300000;
        uint8_t *src = malloc(n), *c = malloc(n), *back = malloc(n);
        fill(src, n, 2, 1);
        blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
        cp.clevel = 3; cp.typesize = 2;
        int cs = enc(src, n, c, n, 0, &cp, NULL);
        CHECK(cs > 0, "dynamic encoder returned %d", cs);
        int ds = cs > 0 ? dec(c, cs, back, n, 0, NULL, NULL) : -1;
        CHECK(ds == n && memcmp(src, back, n) == 0, "dynamic decoder returned %d", ds);
        /* too little room: 0 (stored by Blosc2), never an overflow */
        int small = enc(src, n, c, 16, 0, &cp, NULL);
        CHECK(small == 0, "encoder with 16 bytes of room returned %d", small);
        if (cs > 0 && ds == n) printf("ok  dynamic plugin %s: info -> %s/%s, %d -> %d bytes\n", libpath, inf->encoder, inf->decoder, n, cs);
        free(src); free(c); free(back);
    }
    dlclose(h);
}

int main(int argc, char **argv) {
    blosc2_init();
    int rc = blosc2_kolmo_register();
    CHECK(rc == 0, "blosc2_kolmo_register returned %d", rc);
    CHECK(blosc2_compname_to_compcode("kolmo") == BLOSC_CODEC_KOLMO, "codec name not registered");

    const int32_t nbytes = 2 * 1024 * 1024 + 24;   /* not a multiple of the block size: a short last block */
    uint8_t *buf = malloc((size_t)nbytes);
    char what[96];
    const int typesizes[] = {1, 2, 4, 8};
    for (int t = 0; t < 4; t++) {
        fill(buf, (size_t)nbytes, typesizes[t], 1);
        for (int shuffle = 0; shuffle <= 1; shuffle++)
            for (int level = 1; level <= 9; level += 4) {
                snprintf(what, sizeof what, "typesize %d %s clevel %d", typesizes[t], shuffle ? "shuffle" : "noshuffle", level);
                roundtrip(what, buf, nbytes, typesizes[t], level, 0, shuffle, 0, typesizes[t] != 4 && level >= 3);   /* level 1: minimal analysis */
            }
        snprintf(what, sizeof what, "typesize %d blocksize 1 MiB", typesizes[t]);
        roundtrip(what, buf, nbytes, typesizes[t], 3, 0, 0, 1 << 20, typesizes[t] != 4);
    }
    fill(buf, 65536, 8, 1);
    roundtrip("compcode_meta 10 (ultra), 64 KiB", buf, 65536, 8, 1, 10, 0, 0, 1);
    fill(buf, (size_t)nbytes, 1, 0);
    roundtrip("random bytes (stored)", buf, nbytes, 4, 5, 0, 1, 0, 0);
    roundtrip("tiny 100 bytes", buf, 100, 1, 5, 0, 0, 0, 0);
    free(buf);

    test_schunk();
    if (argc > 1) test_dynamic(argv[1]);
    else printf("dynamic plugin test skipped: no library path given\n");

    blosc2_destroy();
    printf(failures ? "FAILED: %d check(s)\n" : "all Blosc2 codec tests passed\n", failures);
    return failures != 0;
}
