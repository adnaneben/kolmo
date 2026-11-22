/* Huffman leaf variants (deps lane, rc3) on chunks dumped from Kolmo's own Huffman leaves (libprof LIBPROF_DUMP_HUF).
 * usage: huf_bench CHUNKS.bin REPS
 * Encodes every chunk as huf_compress_all does (HUF_compress4X_repeat, table log 11, no repeat) and reports the coded
 * size and the best decode time of several decoder variants, and the size of encoder variants. */
#define HUF_STATIC_LINKING_ONLY
#include "common/huf.h"
#include "common/cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
typedef struct { uint8_t *raw, *z; size_t n, zn; int mode; } chunk;   /* mode 0 stored, 1 rle, 2 huf */
static chunk *C; static size_t NC, RAW;
static size_t encode_all(unsigned tlog, int flags, int repeat) {
    U64 wk[HUF_WORKSPACE_SIZE_U64]; HUF_CREATE_STATIC_CTABLE(ct, HUF_SYMBOLVALUE_MAX); HUF_repeat rp = HUF_repeat_none; size_t tot = 0;
    for (size_t i = 0; i < NC; i++) {
        chunk *c = &C[i]; if (!repeat) rp = HUF_repeat_none;
        size_t z = HUF_compress4X_repeat(c->z, c->n + 1024, c->raw, c->n, 255, tlog, wk, sizeof wk, ct, &rp, flags);
        if (HUF_isError(z)) { fprintf(stderr, "enc error\n"); exit(1); }
        if (z == 1) { c->mode = 1; c->zn = 1; } else if (z == 0 || z + 4 >= c->n) { c->mode = 0; c->zn = c->n; } else { c->mode = 2; c->zn = z; }
        tot += c->zn + 1 + (c->n >= 128) + (c->n >= 16384) + 1 + (c->mode == 2) * (1 + (c->zn >= 128) + (c->zn >= 16384));
    }
    return tot;
}
/* variant: 0 hufOnly flags, 1 X1 forced (readDTableX1 + 4X_usingDTable), 2 X2 forced */
static double decode_all(int variant, int flags, int reps) {
    HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX); U32 wk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32]; uint8_t *out = malloc(HUF_BLOCKSIZE_MAX); double best = 1e9;
    for (int r = 0; r < reps; r++) {
        double t = now();
        for (size_t i = 0; i < NC; i++) {
            chunk *c = &C[i]; size_t got;
            if (c->mode == 0) { memcpy(out, c->z, c->n); continue; }
            if (c->mode == 1) { memset(out, c->raw[0], c->n); continue; }
            if (variant == 0) got = HUF_decompress4X_hufOnly_wksp(dt, out, c->n, c->z, c->zn, wk, sizeof wk, flags);
            else {
                dt[0] = (U32)(HUF_TABLELOG_MAX) * 0x01000001;
                size_t h = variant == 1 ? HUF_readDTableX1_wksp(dt, c->z, c->zn, wk, sizeof wk, flags) : HUF_readDTableX2_wksp(dt, c->z, c->zn, wk, sizeof wk, flags);
                if (HUF_isError(h)) { fprintf(stderr, "table error\n"); exit(1); }
                got = HUF_decompress4X_usingDTable(out, c->n, (const uint8_t *)c->z + h, c->zn - h, dt, flags);
            }
            if (HUF_isError(got) || got != c->n || memcmp(out, c->raw, c->n)) { fprintf(stderr, "decode mismatch v%d chunk %zu\n", variant, i); exit(1); }
        }
        t = now() - t; if (t < best) best = t;
    }
    free(out); return best;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); int reps = argc > 2 ? atoi(argv[2]) : 5; unsigned len; size_t cap = 0;
    while (fread(&len, 4, 1, f) == 1) {
        if (NC == cap) { cap = cap ? cap * 2 : 1024; C = realloc(C, cap * sizeof *C); }
        C[NC].n = len; C[NC].raw = malloc(len); C[NC].z = malloc(len + 1024); if (fread(C[NC].raw, 1, len, f) != len) return 1; RAW += len; NC++;
    }
    int bmi2 = ZSTD_cpuid_bmi2(ZSTD_cpuid()) ? HUF_flags_bmi2 : 0;
    printf("chunks %zu raw %zu (avg %.0f) cpu bmi2 %d build %s\n", NC, RAW, (double)RAW / NC, !!bmi2,
#ifdef __BMI2__
        "bmi2"
#else
        "baseline"
#endif
    );
    for (unsigned tl = 8; tl <= 12; tl++) printf("enc tlog %2u: %zu\n", tl, encode_all(tl, 0, 0));
    printf("enc tlog 11 optimalDepth: %zu\n", encode_all(11, HUF_flags_optimalDepth, 0));
    printf("enc tlog 11 repeat previous table when smaller: %zu\n", encode_all(11, 0, 1));
    size_t base = encode_all(11, 0, 0); (void)base;
    struct { const char *name; int v, fl; } V[] = {
        {"hufOnly flags=0 (rc2)", 0, 0}, {"hufOnly bmi2", 0, bmi2}, {"hufOnly bmi2 disableAsm", 0, bmi2 | HUF_flags_disableAsm},
        {"hufOnly flags=0 disableFast", 0, HUF_flags_disableFast}, {"X1 forced bmi2", 1, bmi2}, {"X2 forced bmi2", 2, bmi2}};
    for (unsigned k = 0; k < sizeof V / sizeof *V; k++) { double t = decode_all(V[k].v, V[k].fl, reps); printf("dec %-28s %8.2f ms  %7.1f MB/s\n", V[k].name, t * 1e3, RAW / t / 1e6); }
    return 0;
}
