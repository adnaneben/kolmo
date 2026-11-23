/* Seed generator for the frame and native-coder fuzz targets (container version 12). Writes
 *   <dir>/v12/<input>.<mode>.kolmo   frames of synthetic inputs in every mode (rt, fast, eco, hc, max, ultra; max level 1
 *                                    and ultra level 1 too), small enough for ultra
 *   <dir>/native/<coder>.<input>     raw coder streams in fuzz_native's layout (selector, size / 16, stream)
 * Build: cc -O2 -Isrc fuzz/make_v12_seeds.c build/libkolmo.a -lpthread -lm -o build/make-v12-seeds
 * Run:   build/make-v12-seeds fuzz/corpus        (the directories must exist) */
#include "kolmo.h"
#include "algo/khuf.h"
#include "algo/klz.h"
#include "algo/klzma.h"
#include "algo/krans.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs = 0x2545F4914F6CDD1Dull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
typedef struct { uint8_t *p; size_t n, cap; } buf;
static void put(buf *b, const void *s, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = realloc(b->p, b->cap); if (!b->p) exit(2); }
    memcpy(b->p + b->n, s, n); b->n += n;
}
static void putf(buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void putf(buf *b, const char *fmt, ...) { char t[512]; va_list a; va_start(a, fmt); int k = vsnprintf(t, sizeof t, fmt, a); va_end(a); put(b, t, (size_t)k); }
static void le16(buf *b, int v) { uint8_t t[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; put(b, t, 2); }
static void le32(buf *b, uint32_t v) { uint8_t t[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; put(b, t, 4); }

static buf gen(int kind) {
    buf b = {0}; static const char *w[] = {"the ", "kolmo ", "frame ", "of ", "and ", "block ", "index ", "\n", "data, ", "2026 "};
    switch (kind) {
    case 0: while (b.n < 20000) putf(&b, "%s", w[rnd() % 10]); break;
    case 1: putf(&b, "id,name,value,ratio\n"); for (int i = 0; i < 1500; i++) putf(&b, "%d,item%u,%d,%.3f\n", i, rnd() % 50, (int)(rnd() % 100000) - 50000, (rnd() % 10000) / 997.0); break;
    case 2: for (int i = 0; i < 400; i++) putf(&b, "{\"id\":%d,\"user\":\"u%u\",\"ok\":%s,\"score\":%u}\n", i, rnd() % 90, rnd() & 1 ? "true" : "false", rnd() % 1000); break;
    case 3: for (int i = 0; i < 500; i++) putf(&b, "2026-11-%02d 12:%02u:%02u INFO [worker-%u] request %u served in %u ms\n", 1 + i % 28, rnd() % 60, rnd() % 60, rnd() % 8, rnd(), rnd() % 900); break;
    case 4: for (int i = 0; i < 4000; i++) le32(&b, (uint32_t)(i * 3 + (int)(rnd() % 7))); break;
    case 5: for (int i = 0; i < 3000; i++) { double v = sin(i * 0.01) * 100.0 + (rnd() % 100) / 1000.0; put(&b, &v, 8); } break;
    case 6: putf(&b, "P6\n96 64\n255\n"); for (int y = 0; y < 64; y++) for (int x = 0; x < 96; x++) { uint8_t px[3] = {(uint8_t)(x * 2 + y), (uint8_t)(x ^ y), (uint8_t)(128 + (int)(40 * sin((x + y) * 0.1)) + (int)(rnd() % 5))}; put(&b, px, 3); } break;
    case 7: { size_t ns = 6000; put(&b, "RIFF", 4); le32(&b, (uint32_t)(36 + 2 * ns)); put(&b, "WAVEfmt ", 8); le32(&b, 16); le16(&b, 1); le16(&b, 1); le32(&b, 16000); le32(&b, 32000); le16(&b, 2); le16(&b, 16);
              put(&b, "data", 4); le32(&b, (uint32_t)(2 * ns)); for (size_t i = 0; i < ns; i++) le16(&b, (int)(9000 * sin(i * 0.07)) + (int)(rnd() % 64) - 32); } break;
    case 8: { int W = 64, H = 48; putf(&b, "YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C420jpeg\n", W, H);
              for (int f = 0; f < 3; f++) { putf(&b, "FRAME\n"); for (int i = 0; i < W * H; i++) { uint8_t v = (uint8_t)(i % W + i / W + f * 2); put(&b, &v, 1); }
                                            for (int i = 0; i < W * H / 2; i++) { uint8_t v = (uint8_t)(128 + (i % 32) / 4); put(&b, &v, 1); } } } break;
    case 9: for (int i = 0; i < 120; i++) { putf(&b, "@SRR%d.%d length=60\n", 835775, i); for (int k = 0; k < 60; k++) putf(&b, "%c", "ACGT"[rnd() & 3]); putf(&b, "\n+\n");
                                            for (int k = 0; k < 60; k++) putf(&b, "%c", (char)('5' + rnd() % 10)); putf(&b, "\n"); } break;
    case 10: for (int i = 0; i < 8192; i++) { uint8_t v = (uint8_t)rnd(); put(&b, &v, 1); } break;
    case 11: { uint8_t z[1024] = {0}; for (int i = 0; i < 16; i++) put(&b, z, sizeof z); } break;
    }
    return b;
}
static const char *KN[] = {"text", "table", "jsonl", "log", "u32", "f64", "image", "audio", "video", "fastq", "random", "zeros"};

static void wr(const char *path, const void *p, size_t n) { FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(1); } fwrite(p, 1, n, f); fclose(f); }
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "fuzz/corpus"; char path[4096]; int frames = 0, streams = 0;
    static const struct { const char *name; kolmo_mode m; int level; } M[] = {
        {"rt", KOLMO_RT, 0}, {"fast", KOLMO_FAST, 0}, {"eco", KOLMO_ECO, 0}, {"hc", KOLMO_HC, 0}, {"max", KOLMO_MAX, 0},
        {"max1", KOLMO_MAX, 1}, {"ultra", KOLMO_ULTRA, 0}, {"ultra1", KOLMO_ULTRA, 1}};
    for (int k = 0; k < 12; k++) {
        buf in = gen(k);
        for (size_t m = 0; m < sizeof M / sizeof *M; m++) {
            kolmo_params p = {M[m].m, M[m].level, 1, 0, 1, 0};
            size_t cap = kolmo_bound(in.n, &p); uint8_t *z = malloc(cap);
            int64_t zn = kolmo_compress(in.p, in.n, z, cap, &p);
            if (zn > 0) { snprintf(path, sizeof path, "%s/v12/%s.%s.kolmo", dir, KN[k], M[m].name); wr(path, z, (size_t)zn); frames++; }
            free(z);
        }
        /* native coder streams; the decoded size is rounded down to a multiple of 16 (fuzz_native's size field) */
        size_t n = in.n / 16 * 16; if (n > 16 * 65535) n = 16 * 65535;
        size_t cap = n * 2 + 4096; uint8_t *z = malloc(cap + 3);
        for (int sel = 0; sel < 7; sel++) {
            size_t zn = 0;
            switch (sel) {
            case 0: zn = kolmo_khuf_compress_chunks(in.p, n, z + 3, cap); break;
            case 1: zn = kolmo_khuf_compress(in.p, n, z + 3, cap); break;
            case 2: zn = kolmo_klz_compress(in.p, n, z + 3, cap, 3); break;
            case 3: zn = kolmo_klzh_compress(in.p, n, z + 3, cap, 5); break;
            case 4: zn = kolmo_klzma_compress(in.p, n, z + 3, cap, 6); break;
            case 5: zn = kolmo_klzh2_compress(in.p, n, z + 3, cap, (k & 1) ? 9 : 3); break;
            case 6: zn = kolmo_krans_compress(in.p, n, z + 3, cap); break;
            }
            if (!zn) continue;
            z[0] = (uint8_t)sel; z[1] = (uint8_t)(n / 16); z[2] = (uint8_t)(n / 16 >> 8);
            snprintf(path, sizeof path, "%s/native/%d.%s", dir, sel, KN[k]); wr(path, z, zn + 3); streams++;
        }
        free(z); free(in.p);
    }
    printf("wrote %d frames and %d coder streams under %s\n", frames, streams, dir);
    return 0;
}
