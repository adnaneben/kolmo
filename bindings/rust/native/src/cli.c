/* kolmo command line:  kolmo [-m rt|fast|eco|hc|max|ultra] [-l level] [-T threads] [-B MiB] [-n|-q] [-D dict] [--native-leaf] in out
 *                    (--native-leaf: balanced only; native plane-context leaves for integer series, KOLMO_T_NATIVE_LEAF)
 *                    (ultra/-x: -l 1 = context-mixing model 1 only, faster; -l 2 = maximum ratio, graph leaves may use CM)
 *                    kolmo -s ...                         (streaming: bounded memory, large inputs)
 *                    kolmo -d [-T threads] [-D dict] in out  (auto-detects one-shot vs streamed)
 *                    kolmo -r offset:length in out        (random access read)
 *                    kolmo -t dict.bin sample1 sample2 ...   (train a dictionary from sample files) */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

static uint8_t *read_input(FILE *f, size_t *n) {
    if (fseek(f, 0, SEEK_END)) { perror("seek"); exit(1); }
    long s = ftell(f);
    if (s < 0 || fseek(f, 0, SEEK_SET)) { perror("seek"); exit(1); }
    uint8_t *b = malloc(s ? (size_t)s : 1);
    if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { perror("read"); exit(1); }
    fclose(f); *n = (size_t)s; return b;
}
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
    return read_input(f, n);
}
static int spit(const char *p, const uint8_t *b, size_t n) {
    FILE *f=fopen(p,"wb");if(!f){perror(p);return -1;}
    int failed=fwrite(b,1,n,f)!=n;
    if(fclose(f))failed=1;
    if(failed)perror(p);
    return failed?-1:0;
}

/* Stage the complete frame or verified decoded output in a private file. For a
 * new destination, publish the verified inode. When overwriting, write through
 * the original path to preserve symlink, hard-link and permission semantics. */
static int process_file(int input_fd, size_t n, const char *path, const kolmo_params *params, int decode,
                       const void *dict, size_t dictlen) {
    const char *slash = strrchr(path, '/');
    size_t dirlen = slash ? (size_t)(slash - path + 1) : 0;
    static const char suffix[] = ".kolmo-XXXXXX";
    char *stage = malloc(dirlen + sizeof(suffix));
    if (!stage) return KOLMO_ERR_ALLOC;
    memcpy(stage, path, dirlen); memcpy(stage + dirlen, suffix, sizeof(suffix));
    int fd = mkstemp(stage);
    FILE *anonymous = NULL;
    if (fd < 0) { /* Existing outputs can be writable even in a read-only directory. */
        anonymous = tmpfile();
        if (!anonymous) { free(stage); return KOLMO_ERR_IO; }
        fd = fileno(anonymous);
    }
    int64_t size = decode ? kolmo_decompress_file(input_fd, fd, n, params->threads, dict, dictlen)
                          : kolmo_compress_file(input_fd, fd, n, params, dict, dictlen);
    int ret = size < 0 ? (int)size : 0;
    if (!ret && !anonymous) {
        struct stat st;
        if (lstat(path, &st) && errno == ENOENT) {
            mode_t mask = umask(0); umask(mask);
            if (!fchmod(fd, 0666 & ~mask) && !link(stage, path)) goto done;
        } else if (S_ISREG(st.st_mode) && st.st_nlink == 1) {
            /* An existing plain file: replace it atomically and keep its mode, instead of copying the staged output
             * over it (a second pass over the whole output). Symlinks, hard links and special files take the copy. */
            if (!fchmod(fd, st.st_mode & 07777) && !rename(stage, path)) {
                if (close(fd)) ret = KOLMO_ERR_IO;
                free(stage); return ret;
            }
        }
    }
    if (!ret) {
        FILE *out = fopen(path, "wb");
        if (!out) ret = KOLMO_ERR_IO;
        else {
            uint8_t buffer[64 * 1024];
            for (;;) {
                ssize_t got = read(fd, buffer, sizeof(buffer));
                if (got < 0 && errno == EINTR) continue;
                if (!got) break;
                if (got < 0 || fwrite(buffer, 1, (size_t)got, out) != (size_t)got) { ret = KOLMO_ERR_IO; break; }
            }
            if (fclose(out)) ret = KOLMO_ERR_IO;
        }
    }
done:
    if (anonymous) { if (fclose(anonymous)) ret = KOLMO_ERR_IO; }
    else { if (close(fd)) ret = KOLMO_ERR_IO; unlink(stage); }
    free(stage);
    return ret;
}
int main(int argc, char **argv) {
    kolmo_params P = {KOLMO_FAST, 0, 0, 0, 1}; int speed = 0; int dec = 0, stream = 0, opt, seg = 0; char *range = NULL;
    char *dictpath = NULL, *trainpath = NULL;
    int extreme = 0, native_leaf = 0, typed_huf = 0, csv_archive = 0;
    for (int a = 1; a < argc; a++)                           /* long option, removed before getopt sees the arguments */
        if (!strcmp(argv[a], "--native-leaf") || !strcmp(argv[a], "--typed-huf") || !strcmp(argv[a], "--csv-archive")) {
            if(!strcmp(argv[a],"--native-leaf"))native_leaf=1;
            else if(!strcmp(argv[a],"--typed-huf"))typed_huf=1;else csv_archive=1;
            for(int b=a;b<argc;b++)argv[b]=argv[b+1];argc--;a--;
        }
    while ((opt = getopt(argc, argv, "m:l:T:B:nqQxdr:sS:D:t:")) != -1) switch (opt) {
        /* three modes: speed, balanced (default), ratio. The older names stay as advanced settings. */
        case 'm': speed = !strcmp(optarg, "speed");
                  P.mode = !strcmp(optarg, "speed") || !strcmp(optarg, "rt") ? KOLMO_RT :
                           !strcmp(optarg, "balanced") || !strcmp(optarg, "fast") ? KOLMO_FAST :
                           !strcmp(optarg, "ratio") || !strcmp(optarg, "max") ? KOLMO_MAX :
                           !strcmp(optarg, "eco") ? KOLMO_ECO : !strcmp(optarg, "hc") ? KOLMO_HC : !strcmp(optarg, "ultra") ? KOLMO_ULTRA : (kolmo_mode)-1;
                  if ((int)P.mode < 0) { fprintf(stderr, "kolmo: unknown mode '%s' (speed, balanced or ratio)\n", optarg); return 2; }
                  break;
        case 'x': extreme = 1; break;
        case 'l': P.level = atoi(optarg); break;
        case 'T': P.threads = atoi(optarg); break;
        case 'B': P.block = (uint32_t)(atof(optarg) * 1048576); break;
        case 'n': P.transforms = 0; break;
        case 'q': P.transforms = 2; break;
        case 'Q': P.transforms = 3; break;
        case 'd': dec = 1; break;
        case 'r': range = optarg; break;
        case 's': stream = 1; break;
        case 'S': stream = 1; seg = atoi(optarg); break;
        case 'D': dictpath = optarg; break;
        case 't': trainpath = optarg; break;
        default: fprintf(stderr,
            "usage: kolmo [-m speed|balanced|ratio] [-x] [-T threads] in out      compress (default: balanced)\n"
            "       kolmo -d in out                                               decompress\n"
            "  speed     fastest, for data that is read or sent right away\n"
            "  balanced  small and fast; the default\n"
            "  ratio     smallest at practical speed;  -x  smallest of all (very slow, large memory)\n"
            "  --native-leaf  with balanced: 5-12%% smaller integer series (sensor, ECG, numeric columns) using\n"
            "            Kolmo's native plane-context leaves; such files decode ~1.5-2.6x slower than balanced\n"
            "            and need a kolmo that reads container version 9\n"
            "  --csv-archive RC option: CSV regularity route, -m eco -T1; may lose to plain eco\n"
            "  --typed-huf  RC option: declared integer NPY / PCM16, -m fast -T1; generic fallback otherwise\n"
            "  -x -l 2   maximum ratio: as -x, and numeric graph leaves may also use context mixing\n"
            "            (smaller integer series; those blocks decode much more slowly)\n"
            "advanced: -m rt|fast|eco|hc|max|ultra  -l level  -q quick analysis  -Q minimal analysis  -n no transforms\n"
            "          -B blockMiB\n"
            "          -D dict  -t dict samples...  -s|-S segMiB stream  -r off:len read a range\n"); return 2;
    }
    if (extreme) P.mode = KOLMO_ULTRA;
    if(native_leaf && typed_huf){fprintf(stderr,"kolmo: choose --native-leaf OR --typed-huf\n");return 2;}
    if(typed_huf && P.transforms)P.transforms |= KOLMO_T_TYPED_HUF;
    if(csv_archive && P.transforms)P.transforms |= KOLMO_T_CSV_ARCHIVE;
    if (native_leaf && P.transforms) P.transforms |= KOLMO_T_NATIVE_LEAF;   /* applied by the library at balanced only */
    if (speed && P.transforms == 1) P.transforms = 3;      /* speed: nothing that costs encode time */

    /* Training mode: concatenate the sample files given as positional args and write a dictionary. */
    if (trainpath) {
        int nf = argc - optind; if (nf < 1) { fprintf(stderr, "kolmo -t: need at least one sample file\n"); return 2; }
        size_t *sz = malloc(nf * sizeof(size_t)), tot = 0; uint8_t **buf = malloc(nf * sizeof(uint8_t *));
        for (int i = 0; i < nf; i++) { buf[i] = slurp(argv[optind + i], &sz[i]); tot += sz[i]; }
        uint8_t *samp = malloc(tot ? tot : 1); size_t off = 0;
        for (int i = 0; i < nf; i++) { memcpy(samp + off, buf[i], sz[i]); off += sz[i]; free(buf[i]); }
        size_t dcap = 112 * 1024; uint8_t *dict = malloc(dcap);
        int64_t dl = kolmo_train_dict(samp, sz, nf, dict, dcap);
        if (dl < 0) { fprintf(stderr, "kolmo: train: %s\n", kolmo_strerror(dl)); free(samp);free(sz);free(buf);free(dict);return 1; }
        int write_failed=spit(trainpath, dict, (size_t)dl);
        if(!write_failed)fprintf(stderr, "kolmo: wrote %lld-byte dictionary from %d samples to %s\n", (long long)dl, nf, trainpath);
        free(samp);free(sz);free(buf);free(dict);return write_failed!=0;
    }

    if (argc - optind != 2) { fprintf(stderr, "need input and output paths\n"); return 2; }
    const char *inp = argv[optind], *outp = argv[optind + 1];

    size_t dictlen = 0; uint8_t *dict = NULL;
    if (dictpath) dict = slurp(dictpath, &dictlen);

    /* Streaming compress, or decode of a streamed (CZNS) file: bounded memory via file descriptors. */
    int streamed_in = 0;
    if (dec) {
        FILE *pf = fopen(inp, "rb"); if (!pf) { perror(inp); free(dict);return 1; }
        char mg[4] = {0}; size_t g = fread(mg, 1, 4, pf); fclose(pf);
        streamed_in = (g == 4 && !memcmp(mg, "CZNS", 4));
    }
    if ((stream && !dec && !range) || (dec && streamed_in)) {
        int ifd = open(inp, O_RDONLY); if (ifd < 0) { perror(inp); free(dict);return 1; }
        int ofd = open(outp, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (ofd < 0) { perror(outp); close(ifd);free(dict);return 1; }
        int64_t sr = dec ? kolmo_stream_decompress(ifd, ofd, P.threads)
                         : kolmo_stream_compress(ifd, ofd, &P, (uint32_t)(seg > 0 ? seg : 0));
        close(ifd); close(ofd);free(dict);
        if (sr < 0) { fprintf(stderr, "kolmo: %s\n", kolmo_strerror(sr)); return 1; }
        return 0;
    }

    /* Small files fit in a few blocks. Keep their inexpensive in-memory path. */
#ifndef KOLMO_FILE_THRESHOLD
#define KOLMO_FILE_THRESHOLD (8u << 20)
#endif
    const size_t file_threshold = KOLMO_FILE_THRESHOLD;   /* the file path (reused block buffers, staged output) wins from 8 MiB; below
                                               * that its extra file operations cost more than they save on macOS */
    FILE *input = fopen(inp, "rb"); if (!input) { perror(inp);free(dict);return 1; }
    if (!range && (dec || P.mode == KOLMO_FAST || P.mode == KOLMO_RT)) {
        int fd = fileno(input); struct stat st;
        if (!fstat(fd, &st) && S_ISREG(st.st_mode) && st.st_size >= 0 && (uintmax_t)st.st_size <= SIZE_MAX) {
            int use_file = st.st_size >= (off_t)file_threshold;
            if (dec) {
                uint8_t header[32];
                use_file = pread(fd, header, sizeof(header), 0) == sizeof(header) &&
                    kolmo_original_size(header, sizeof(header)) >= (int64_t)file_threshold;
            }
            if (use_file) {
                int err = process_file(fd, st.st_size, outp, &P, dec, dict, dictlen);
                fclose(input);free(dict);
                if (err) fprintf(stderr, "kolmo: %s\n", kolmo_strerror(err));
                return err != 0;
            }
        }
    }
    size_t n; uint8_t *in = read_input(input, &n), *out; int64_t r;
    if (range) {
        unsigned long long off = strtoull(range, NULL, 10); char *c = strchr(range, ':'); size_t len = c ? strtoull(c + 1, NULL, 10) : 0;
        out = malloc(len ? len : 1); r = kolmo_read_range(in, n, off, len, out, P.threads);
    } else if (dec) {
        int64_t o = kolmo_original_size(in, n); if (o < 0) { fprintf(stderr, "kolmo: %s\n", kolmo_strerror(o));free(in);free(dict);return 1; }
        out = malloc(o ? o : 1);
        r = dict ? kolmo_decompress_dict(in, n, out, o, P.threads, dict, dictlen) : kolmo_decompress(in, n, out, o, P.threads);
    } else {
        size_t cap = kolmo_bound(n, &P); out = malloc(cap);
        r = dict ? kolmo_compress_dict(in, n, out, cap, &P, dict, dictlen) : kolmo_compress(in, n, out, cap, &P);
    }
    if (r < 0) { fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r));free(in);free(out);free(dict);return 1; }
    int failed=spit(outp,out,(size_t)r);
    free(in);free(out);free(dict);return failed!=0;
}
