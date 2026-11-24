/* R07 experiment: resident memory a long-lived caller of the library keeps AFTER kolmo_compress returns
 * (pool threads keep per-thread buffers and codec contexts). usage: idle MODE LEVEL THREADS file */
#include "kolmo.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long rss_kb(void) { FILE *f = fopen("/proc/self/status", "r"); char l[256]; long v = 0; while (fgets(l, sizeof l, f)) if (!strncmp(l, "VmRSS:", 6)) v = atol(l + 6); fclose(f); return v; }
int main(int c, char **v) {
    if (c < 5) return 2;
    kolmo_params p; memset(&p, 0, sizeof p); p.mode = atoi(v[1]); p.level = atoi(v[2]); p.threads = atoi(v[3]); p.transforms = 1;
    FILE *f = fopen(v[4], "rb"); fseek(f, 0, SEEK_END); size_t n = ftell(f); rewind(f);
    printf("start: %ld KiB\n", rss_kb());
    uint8_t *in = malloc(n); if (fread(in, 1, n, f) != n) return 1; fclose(f);
    size_t cap = kolmo_bound(n, &p); uint8_t *out = malloc(cap);
    long long r = kolmo_compress(in, n, out, cap, &p);
    printf("compressed %zu -> %lld; RSS at return (input+output still held): %ld KiB\n", n, r, rss_kb());
    free(in); free(out);
    printf("after freeing input and output: %ld KiB\n", rss_kb());
    malloc_trim(0);
    printf("after malloc_trim(0) (what the library still holds): %ld KiB\n", rss_kb());
    return 0;
}
