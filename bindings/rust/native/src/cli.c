/* kolmo command line:  kolmo [-m rt|fast|eco|hc|max|ultra] [-l level] [-T threads] [-B MiB] [-n] [-D dict] in out
 *                    kolmo -s ...                         (streaming: bounded memory, large inputs)
 *                    kolmo -d [-T threads] [-D dict] in out  (auto-detects one-shot vs streamed)
 *                    kolmo -r offset:length in out        (random access read)
 *                    kolmo -t dict.bin sample1 sample2 ...   (train a dictionary from sample files) */
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
    char *dictpath = NULL, *trainpath = NULL;
    while ((opt = getopt(argc, argv, "m:l:T:B:ndr:sS:D:t:")) != -1) switch (opt) {
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
        case 'D': dictpath = optarg; break;
        case 't': trainpath = optarg; break;
        default: fprintf(stderr, "usage: kolmo [-m rt|fast|eco|hc|max|ultra] [-l lvl] [-T thr] [-B MiB] [-n] [-D dict] [-s|-S segMiB] | -d [-D dict] | -r off:len | -t dict samples...  in out\n"); return 2;
    }

    /* Training mode: concatenate the sample files given as positional args and write a dictionary. */
    if (trainpath) {
        int nf = argc - optind; if (nf < 1) { fprintf(stderr, "kolmo -t: need at least one sample file\n"); return 2; }
        size_t *sz = malloc(nf * sizeof(size_t)), tot = 0; uint8_t **buf = malloc(nf * sizeof(uint8_t *));
        for (int i = 0; i < nf; i++) { buf[i] = slurp(argv[optind + i], &sz[i]); tot += sz[i]; }
        uint8_t *samp = malloc(tot ? tot : 1); size_t off = 0;
        for (int i = 0; i < nf; i++) { memcpy(samp + off, buf[i], sz[i]); off += sz[i]; free(buf[i]); }
        size_t dcap = 112 * 1024; uint8_t *dict = malloc(dcap);
        int64_t dl = czn_train_dict(samp, sz, nf, dict, dcap);
        if (dl < 0) { fprintf(stderr, "kolmo: train: %s\n", czn_strerror(dl)); return 1; }
        spit(trainpath, dict, (size_t)dl);
        fprintf(stderr, "kolmo: wrote %lld-byte dictionary from %d samples to %s\n", (long long)dl, nf, trainpath);
        return 0;
    }

    if (argc - optind != 2) { fprintf(stderr, "need input and output paths\n"); return 2; }
    const char *inp = argv[optind], *outp = argv[optind + 1];

    size_t dictlen = 0; uint8_t *dict = NULL;
    if (dictpath) dict = slurp(dictpath, &dictlen);

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
        out = malloc(o ? o : 1);
        r = dict ? czn_decompress_dict(in, n, out, o, P.threads, dict, dictlen) : czn_decompress(in, n, out, o, P.threads);
    } else {
        size_t cap = czn_bound(n, &P); out = malloc(cap);
        r = dict ? czn_compress_dict(in, n, out, cap, &P, dict, dictlen) : czn_compress(in, n, out, cap, &P);
    }
    if (r < 0) { fprintf(stderr, "kolmo: %s\n", czn_strerror(r)); return 1; }
    spit(outp, out, (size_t)r);
    return 0;
}
