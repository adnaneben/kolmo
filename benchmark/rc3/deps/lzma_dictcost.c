/* Upper bound of an LZMA2 decoder that writes straight into Kolmo's output block (deps lane, rc3): liblzma's raw decoder
 * decodes into its own dictionary buffer (dict_size bytes, freshly allocated per call) and copies to the output. This
 * times that overhead alone, a fresh mapping (page faults included) filled over n bytes plus a copy of n bytes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/mman.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    double tot = 0; size_t all = 0;
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); fclose(f);
        uint8_t *out = malloc(n); memset(out, 1, n); double best = 1e9;
        for (int r = 0; r < 5; r++) {
            double t = now(); uint8_t *d = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); memset(d, 2, n); memcpy(out, d, n); munmap(d, n); t = now() - t; if (t < best) best = t;
        }
        printf("%-10s %10zu  dictionary alloc+fill+copy %.2f ms\n", strrchr(argv[a], '/') + 1, n, best * 1e3); tot += best; all += n; free(out);
    }
    printf("TOTAL %zu bytes  %.2f ms\n", all, tot * 1e3);
}
