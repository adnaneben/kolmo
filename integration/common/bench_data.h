/* Inputs and timing shared by integration/hdf5/bench and integration/blosc2/bench.
 *
 * An input is "path:type[:skip]" (type u8 i16 i32 i64 f32 f64; skip = header bytes to drop, e.g. 128 for a .npy
 * file, 44 for a WAV file), or "synth:NAME" for a generated array (deterministic, the same on every machine):
 *   synth:f32_grid   16 MiB float32 gridded field (smooth in both directions, two decimals, like reanalysis output)
 *   synth:i64_ticks   8 MiB int64 irregular millisecond timestamps
 *   synth:i16_wave    4 MiB int16 sampled signal (a few tones plus noise, like a sensor or audio channel)
 * With no inputs the benchmarks run the three synth arrays.
 */
#ifndef KOLMO_BENCH_DATA_H
#define KOLMO_BENCH_DATA_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { char name[256]; char type[8]; int esize; uint8_t *data; size_t bytes; } bench_input;

static double bench_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static int bench_esize(const char *t) {
    if (!strcmp(t, "u8")) return 1;
    if (!strcmp(t, "i16")) return 2;
    if (!strcmp(t, "i32") || !strcmp(t, "f32")) return 4;
    if (!strcmp(t, "i64") || !strcmp(t, "f64")) return 8;
    return 0;
}

static uint64_t bench_rng = 0x2545F4914F6CDD1Dull;
static uint64_t bench_next(void) { bench_rng ^= bench_rng << 13; bench_rng ^= bench_rng >> 7; bench_rng ^= bench_rng << 17; return bench_rng; }
static double bench_unit(void) { return (bench_next() >> 11) * (1.0 / 9007199254740992.0); }

static int bench_synth(const char *what, bench_input *in) {
    bench_rng = 0x2545F4914F6CDD1Dull;
    snprintf(in->name, sizeof in->name, "synth:%s", what);
    if (!strcmp(what, "f32_grid")) {
        const size_t ny = 1024, nx = 4096; float *a = malloc(ny * nx * 4);
        for (size_t y = 0; y < ny; y++) for (size_t x = 0; x < nx; x++) {
            double v = 15 + 12 * cos(y * 3.14159 / ny) + 4 * sin(x * 6.2832 * 3 / nx + y * 0.01) + 1.5 * sin(x * 0.05) * cos(y * 0.07)
                       + 0.2 * (bench_unit() - 0.5);
            a[y * nx + x] = (float)(round(v * 100) / 100);
        }
        strcpy(in->type, "f32"); in->data = (uint8_t *)a; in->bytes = ny * nx * 4;
    } else if (!strcmp(what, "i64_ticks")) {
        const size_t n = 1 << 20; int64_t *a = malloc(n * 8), t = 1732000000000ll;
        for (size_t i = 0; i < n; i++) { t += 200 + (int64_t)(bench_next() % 60) + ((bench_next() & 255) == 0 ? (int64_t)(bench_next() % 50000) : 0); a[i] = t; }
        strcpy(in->type, "i64"); in->data = (uint8_t *)a; in->bytes = n * 8;
    } else if (!strcmp(what, "i16_wave")) {
        const size_t n = 1 << 21; int16_t *a = malloc(n * 2);
        for (size_t i = 0; i < n; i++)
            a[i] = (int16_t)lrint(4000 * sin(i * 0.0123) + 1500 * sin(i * 0.0571 + 1) + 600 * sin(i * 0.31) + 120 * (bench_unit() - 0.5));
        strcpy(in->type, "i16"); in->data = (uint8_t *)a; in->bytes = n * 2;
    } else return -1;
    in->esize = bench_esize(in->type);
    return 0;
}

/* Parses one argument into *in; 0 or -1 (message printed). */
static int bench_load(const char *arg, bench_input *in) {
    memset(in, 0, sizeof *in);
    if (!strncmp(arg, "synth:", 6)) {
        if (bench_synth(arg + 6, in) == 0) return 0;
        fprintf(stderr, "unknown synthetic input %s\n", arg);
        return -1;
    }
    char path[1024];
    snprintf(path, sizeof path, "%s", arg);
    char *c1 = strrchr(path, ':');
    long skip = 0;
    if (c1 && bench_esize(c1 + 1) == 0) { skip = atol(c1 + 1); *c1 = 0; c1 = strrchr(path, ':'); }
    if (!c1 || !bench_esize(c1 + 1)) { fprintf(stderr, "input %s: expected path:type[:skip]\n", arg); return -1; }
    snprintf(in->type, sizeof in->type, "%s", c1 + 1);
    *c1 = 0;
    in->esize = bench_esize(in->type);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f) - skip;
    if (n <= 0) { fclose(f); fprintf(stderr, "%s: empty\n", path); return -1; }
    n -= n % in->esize;
    in->data = malloc((size_t)n);
    fseek(f, skip, SEEK_SET);
    if (fread(in->data, 1, (size_t)n, f) != (size_t)n) { fclose(f); perror(path); return -1; }
    fclose(f);
    in->bytes = (size_t)n;
    const char *base = strrchr(path, '/');
    snprintf(in->name, sizeof in->name, "%s", base ? base + 1 : path);
    return 0;
}

#endif
