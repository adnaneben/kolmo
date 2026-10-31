/* kolmo command line:  kolmo [-m rt|fast|eco|hc|max|ultra] [-l level] [-T threads] [-B MiB] [-n] in out
 *                    kolmo -s ...                         (streaming: bounded memory, large inputs)
 *                    kolmo -d [-T threads] in out         (auto-detects one-shot vs streamed)
 *                    kolmo -r offset:length in out        (random access read) */
#include "czn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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
    czn_params P = {CZN_FAST, 0, 0, 0, 1}; int dec = 0, stream = 0, opt, seg = 0; char *range = NULL;
    while ((opt = getopt(argc, argv, "m:l:T:B:ndr:sS:")) != -1) switch (opt) {
        case 'm': P.mode = !strcmp(optarg, "rt") ? CZN_RT : !strcmp(optarg, "fast") ? CZN_FAST : !strcmp(optarg, "eco") ? CZN_ECO :
                         !strcmp(optarg, "max") ? CZN_MAX : !strcmp(optarg, "ultra") ? CZN_ULTRA : CZN_HC; break;
        case 'l': P.level = atoi(optarg); break;
        case 'T': P.threads = atoi(optarg); break;
        case 'B': P.block = (uint32_t)(atof(optarg) * 1048576); break;
        case 'n': P.transforms = 0; break;
        case 'd': dec = 1; break;
        case 'r': range = optarg; break;
        case 's': stream = 1; break;
        case 'S': stream = 1; seg = atoi(optarg); break;
        default: fprintf(stderr, "usage: kolmo [-m rt|fast|eco|hc|max|ultra] [-l lvl] [-T thr] [-B MiB] [-n] [-s|-S segMiB] | -d | -r off:len  in out\n"); return 2;
    }
    if (argc - optind != 2) { fprintf(stderr, "need input and output paths\n"); return 2; }
    const char *inp = argv[optind], *outp = argv[optind + 1];

    /* Streaming compress, or decode of a streamed (CZNS) file: bounded memory via file descriptors. */
    int streamed_in = 0;
    if (dec) {
        FILE *pf = fopen(inp, "rb"); if (!pf) { perror(inp); return 1; }
        char mg[4] = {0}; size_t g = fread(mg, 1, 4, pf); fclose(pf);
        streamed_in = (g == 4 && !memcmp(mg, "CZNS", 4));
    }
    if ((stream && !dec && !range) || (dec && streamed_in)) {
        int ifd = open(inp, O_RDONLY); if (ifd < 0) { perror(inp); return 1; }
        int ofd = open(outp, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (ofd < 0) { perror(outp); close(ifd); return 1; }
        int64_t sr = dec ? czn_stream_decompress(ifd, ofd, P.threads)
                         : czn_stream_compress(ifd, ofd, &P, (uint32_t)(seg > 0 ? seg : 0));
        close(ifd); close(ofd);
        if (sr < 0) { fprintf(stderr, "kolmo: %s\n", czn_strerror(sr)); return 1; }
        return 0;
    }

    size_t n; uint8_t *in = slurp(inp, &n), *out; int64_t r;
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
    spit(outp, out, (size_t)r);
    return 0;
}
