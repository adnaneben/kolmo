/* Local driver so the OSS-Fuzz targets can be exercised here under gcc's ASan/UBSan: seeds the targets
 * with real .czn files (decompress target) and random inputs, plus bit-flipped mutations. Not a
 * replacement for libFuzzer coverage-guided fuzzing -- it just proves the harness and the paths are clean. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void){ rs ^= rs<<13; rs ^= rs>>7; rs ^= rs<<17; return rs; }
int main(int argc, char **argv) {
    long iters = argc > 1 ? atol(argv[1]) : 200000;
    uint8_t *buf = malloc(4<<20);  /* scratch region at +2MiB */
    /* seed corpus: any files passed after the count */
    for (int a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) continue;
        size_t n = fread(buf, 1, 1<<20, f); fclose(f);
        LLVMFuzzerTestOneInput(buf, n);
        for (int it = 0; it < 500; it++) { size_t m = n ? n : 1; uint8_t *mut = buf + (2<<20); memcpy(mut, buf, m);
            int nf = 1 + rnd()%6; for (int q=0;q<nf;q++) mut[rnd()%m] ^= (uint8_t)(1+rnd()%255);
            LLVMFuzzerTestOneInput(mut, m); }
    }
    for (long it = 0; it < iters; it++) {
        size_t n = rnd() % 4096;
        for (size_t i = 0; i < n; i += 8) { uint64_t r = rnd(); memcpy(buf+i, &r, n-i<8?n-i:8); }
        if (n >= 4 && rnd()%2) memcpy(buf, "CZN1", 4);          /* bias toward plausible headers */
        LLVMFuzzerTestOneInput(buf, n);
    }
    puts("local fuzz driver done");
    free(buf); return 0;
}
