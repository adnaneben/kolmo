#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "codecs/bwt_stream.h"
#include "algo/qlfc.h"
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc, char **argv) {
    /* variants: name hash min coder */
    int th = atoi(argv[1]); const char *vs = argv[2];
    kolmo_bwt_params P; char name[64];
    for (const char *v = vs; *v; ) {
        int h, m, c, k = 0; sscanf(v, "%d/%d/%d%n", &h, &m, &c, &k); v += k; if (*v == ',') v++;
        P.lzp_hash = h; P.lzp_min = m; P.coder = c; P.index_log2 = 0; P.min_block = getenv("MINB") ? atoi(getenv("MINB")) : 0;
        snprintf(name, sizeof name, "lzp%d/%d %s", h, m, c == 2 ? "adaptive" : "static");
        long long tin = 0, tout = 0; double tc = 0, td = 0;
        for (int f = 3; f < argc; f++) {
            FILE *fp = fopen(argv[f], "rb"); fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
            uint8_t *in = malloc(n), *out = malloc(n + 4096), *d = malloc(n + 64); if (fread(in, 1, n, fp) != (size_t)n) return 2; fclose(fp);
            double t0 = now(); int r = kolmo_bwt_compress(in, out, n + 4096, (int)n, &P, th); double t1 = now();
            int ok = r > 0 && kolmo_bwt_decompress(out, r, d, (int)n, th) == 0 && !memcmp(d, in, n); double t2 = now();
            if (!ok) { printf("FAIL %s %s r=%d\n", argv[f], name, r); return 1; }
            if (getenv("PERFILE")) printf("  %s %s %ld -> %d\n", name, argv[f], n, r);
            tin += n; tout += r; tc += t1 - t0; td += t2 - t1; free(in); free(out); free(d);
        }
        printf("%-22s threads=%d %lld -> %lld (%.3f%%) comp %.1f MB/s dec %.1f MB/s\n", name, th, tin, tout, 100.0 * tout / tin, tin / 1e6 / tc, tin / 1e6 / td);
        fflush(stdout);
    }
}
