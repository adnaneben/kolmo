/* Research harness of the w4 CM lane: codes one file with codec 7 (model, preset) and times encode and decode
 * separately, checking the round trip.   cmx FILE [MODEL=3] [PRESET=0, -1 = race] [BB=auto] [MASK (model 4, hex)]
 * Built by benchmark/w4/cm/build.sh against the library objects of `make`. */
#include "../../../src/kolmo.c"
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    size_t n; uint8_t *s = slurp(argv[1], &n); if (!s) return 1;
    int model = argc > 2 ? atoi(argv[2]) : 3, pr = argc > 3 ? atoi(argv[3]) : 0, bb = argc > 4 ? atoi(argv[4]) : 0;
    uint32_t mask = argc > 5 ? (uint32_t)strtoul(argv[5], NULL, 16) : CM_MASK3;
    size_t cap = n + n / 8 + 1024; uint8_t *z = malloc(cap), *d = malloc(n + 8);
    cm_tune_t tu = pr >= 0 ? cm_presets[pr] : cm_presets[0];
    if (getenv("CMX_B2")) tu.b2 = (uint8_t)atoi(getenv("CMX_B2"));   /* initial weights: 0 = 1/4 each, 1 = the prior */
    if (getenv("CMX_S")) cm4_s_default = atoi(getenv("CMX_S"));     /* model 4's table size shift */
    if (getenv("CMX_TUNE")) { unsigned a, b; if (sscanf(getenv("CMX_TUNE"), "%x,%x", &a, &b) == 2) { tu.b0 = (uint8_t)a; tu.b1 = (uint8_t)b; } }   /* learning settings b0,b1 (hex) */
    double t0 = now();
    size_t zn = pr < 0 ? cm_compress_raced(s, n, z, cap, model, bb, 1, mask) : cm_compress_tm(s, n, z, cap, model, bb, tu, mask);
    double t1 = now();
    const int dec = !getenv("CMX_NODEC");   /* CMX_NODEC=1: encode only (size studies) */
    int r = dec ? cm_decompress(z, zn, d, n) : 0;
    double t2 = now();
    int ok = !dec || (!r && !memcmp(s, d, n));
    printf("%s model=%d mask=%08x preset=%d size=%zu enc=%.3fs (%.3f MB/s) dec=%.3fs (%.3f MB/s) %s\n", argv[1], model, mask, pr, zn,
           t1 - t0, n / 1e6 / (t1 - t0), t2 - t1, n / 1e6 / (t2 - t1), ok ? "ok" : "MISMATCH");
    return !ok;
}
