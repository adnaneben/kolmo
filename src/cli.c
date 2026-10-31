/* kolmo command line:  kolmo [-m rt|fast|eco|hc|max|ultra] [-l level] [-T threads] [-B MiB] [-n] in out
 *                    kolmo -d [-T threads] in out
 *                    kolmo -r offset:length in out      (random access read) */
#include "czn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(s ? s : 1); if (s && fread(b, 1, s, f) != (size_t)s) { perror("read"); exit(1); }
    fclose(f); *n = s; return b;
}
static void spit(const char *p, const uint8_t *b, size_t n) {
    FILE *f = fopen(p, "wb"); if (!f || fwrite(b, 1, n, f) != n) { perror(p); exit(1); } fclose(f);
}
int main(int argc, char **argv) {
    czn_params P = {CZN_FAST, 0, 0, 0, 1}; int dec = 0, opt; char *range = NULL;
    while ((opt = getopt(argc, argv, "m:l:T:B:ndr:")) != -1) switch (opt) {
        case 'm': P.mode = !strcmp(optarg, "rt") ? CZN_RT : !strcmp(optarg, "fast") ? CZN_FAST : !strcmp(optarg, "eco") ? CZN_ECO :
                         !strcmp(optarg, "max") ? CZN_MAX : !strcmp(optarg, "ultra") ? CZN_ULTRA : CZN_HC; break;
        case 'l': P.level = atoi(optarg); break;
        case 'T': P.threads = atoi(optarg); break;
        case 'B': P.block = (uint32_t)(atof(optarg) * 1048576); break;
        case 'n': P.transforms = 0; break;
        case 'd': dec = 1; break;
        case 'r': range = optarg; break;
        default: fprintf(stderr, "usage: kolmo [-m rt|fast|eco|hc|max|ultra] [-l lvl] [-T thr] [-B MiB] [-n] | -d | -r off:len  in out\n"); return 2;
    }
    if (argc - optind != 2) { fprintf(stderr, "need input and output paths\n"); return 2; }
    size_t n; uint8_t *in = slurp(argv[optind], &n), *out; int64_t r;
    if (range) {
        unsigned long long off = strtoull(range, NULL, 10); char *c = strchr(range, ':'); size_t len = c ? strtoull(c + 1, NULL, 10) : 0;
        out = malloc(len ? len : 1); r = czn_read_range(in, n, off, len, out, P.threads);
    } else if (dec) {
        int64_t o = czn_original_size(in, n); if (o < 0) { fprintf(stderr, "kolmo: %s\n", czn_strerror(o)); return 1; }
        out = malloc(o ? o : 1); r = czn_decompress(in, n, out, o, P.threads);
    } else {
        size_t cap = czn_bound(n, &P); out = malloc(cap); r = czn_compress(in, n, out, cap, &P);
    }
    if (r < 0) { fprintf(stderr, "kolmo: %s\n", czn_strerror(r)); return 1; }
    spit(argv[optind + 1], out, (size_t)r);
    return 0;
}
