/* kolmo command line:  kolmo [-m rt|fast|eco|hc|max|ultra] [-l level] [-T threads] [-B MiB] [-n|-q] [--native-leaf|--typed-huf] in out
 *                    (--x-tables=scaled|full|16..22: -x model table size, scaled by default; see kolmo_set_x_tables)
 *                    (--native-leaf: balanced only; native plane-context leaves for integer series, KOLMO_T_NATIVE_LEAF)
 *                    (--typed-huf: -m fast -T1, integer NPY / PCM16 WAV coded type first, KOLMO_T_TYPED_HUF)
 *                    (ultra/-x: -l 1 = context-mixing model 1 only, faster; -l 2 = maximum ratio, graph leaves may use CM)
 *                    kolmo -s ...                         (streaming: bounded memory, large inputs)
 *                    kolmo -d [-T threads] in out         (auto-detects one-shot vs streamed)
 *                    kolmo -r offset:length in out        (random access read)
 *                    (-D, -t, -P, kolmo train, kolmo profile: dictionaries and trained profiles, not in this build:
 *                     they print KOLMO_ERR_UNSUPPORTED's message and exit 2)
 *                    kolmo --target 'decode>=1GB/s'|size|'cost:...' [--calib table] in out   (per-chunk setting)
 *                    kolmo calibrate [-o table] [FILES]   (speed table for --target)
 *                    kolmo --index ... / cat / grep / a / x / l   (queryable and deduplicating archives: cli/features.c)
 *                    -M SIZE | --memlimit SIZE   memory limit for compressing and decompressing: 512M, 8G, 50%, auto
 *                                         (default, 50% of RAM) or max; also KOLMO_MEMLIMIT (docs/MEMORY-MODEL.md) */
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
/* Returns 0, or -1 after reporting the error, so the caller can release its buffers before it exits. */
static int spit(const char *p, const uint8_t *b, size_t n) {
    FILE *f = fopen(p, "wb"); if (!f) { perror(p); return -1; }
    int failed = fwrite(b, 1, n, f) != n;
    if (fclose(f)) failed = 1;
    if (failed) perror(p);
    return failed ? -1 : 0;
}

/* Stage the complete frame or verified decoded output in a private file. For a
 * new destination, publish the verified inode. When overwriting, write through
 * the original path to preserve symlink, hard-link and permission semantics. */
static int process_file(int input_fd, size_t n, const char *path, const kolmo_params *params, int decode) {
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
    int64_t size = decode ? kolmo_decompress_file(input_fd, fd, n, params->threads, NULL, 0)
                          : kolmo_compress_file(input_fd, fd, n, params, NULL, 0);
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
int kolmo_cli_features(int argc, char **argv);     /* cli/features.c: cat, grep, a, x, l, --index */

/* sizes for messages: 4.7 GiB, 512 MiB */
static const char *mem_str(uint64_t b, char *o, size_t cap) {
    if (b == UINT64_MAX) snprintf(o, cap, "no limit");
    else if (b >= (1ull << 30)) snprintf(o, cap, "%.1f GiB", (double)b / (1ull << 30));
    else snprintf(o, cap, "%.0f MiB", (double)b / (1ull << 20));
    return o;
}
/* the memory error, with the figure the frame declares (decode) or the mode's floor (encode) */
static void mem_error(const uint8_t *hdr, size_t n, int dec, kolmo_mode mode, uint64_t limit) {
    char a[32], b[32];
    uint64_t lim = limit ? limit : kolmo_mem_auto();
    if (dec) {
        int64_t need = hdr ? kolmo_decode_memory(hdr, n) : -1;
        if (need > 0) {
            uint64_t g = ((uint64_t)need + (1ull << 30) - 1) >> 30;
            fprintf(stderr, "kolmo: needs %s to decode (limit %s); run with --memlimit %lluG or --memlimit max\n",
                    mem_str((uint64_t)need, a, sizeof a), mem_str(lim, b, sizeof b), (unsigned long long)(g ? g : 1));
        } else fprintf(stderr, "kolmo: a frame needs more memory to decode than the limit (%s); run with a larger --memlimit or --memlimit max\n", mem_str(lim, b, sizeof b));
    } else {
        static const char *names[] = {"speed", "balanced", "eco", "hc", "ratio", "-x"};
        fprintf(stderr, "kolmo: --memlimit %s is below the floor of %s (%s)\n", mem_str(lim, a, sizeof a),
                (unsigned)mode < 6 ? names[mode] : "this mode", mem_str(kolmo_mem_floor(mode), b, sizeof b));
    }
}
#include "cli/train.c"

/* Release label, kept in step with VERSION (RELEASING.md step 1). */
#ifndef KOLMO_VERSION_STR
#define KOLMO_VERSION_STR "0.0.1"
#endif

int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version"))) {
        printf("kolmo %s (container version 12, Kolmo-native codecs)\n", KOLMO_VERSION_STR); return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "train")) return kolmo_cli_train(argc - 1, argv + 1);
    if (argc >= 2 && !strcmp(argv[1], "calibrate")) return kolmo_cli_calibrate(argc - 1, argv + 1);
    if (argc == 3 && !strcmp(argv[1], "profile")) { fprintf(stderr, "kolmo profile: %s\n", kolmo_strerror(KOLMO_ERR_UNSUPPORTED)); return 2; }
    { int fr = kolmo_cli_features(argc, argv); if (fr >= 0) return fr; }
    kolmo_params P = {KOLMO_FAST, 0, 0, 0, 1}; int speed = 0; int dec = 0, stream = 0, opt, seg = 0; char *range = NULL;
    char *dictpath = NULL, *trainpath = NULL;
    int extreme = 0, native_leaf = 0, typed_huf = 0, verbose = 0, mode_given = 0;
    char *profpath = NULL, *targetspec = NULL, *calibpath = NULL;
    const char *memspec = getenv("KOLMO_MEMLIMIT"); int mem_from_env = memspec != NULL;
    for (int a = 1; a < argc; a++)                           /* options with a value: --target T, --calib FILE, --memlimit SIZE */
        if ((!strcmp(argv[a], "--target") || !strcmp(argv[a], "--calib") || !strcmp(argv[a], "--memlimit")) && a + 1 < argc) {
            if (argv[a][2] == 't') targetspec = argv[a + 1]; else if (argv[a][2] == 'c') calibpath = argv[a + 1]; else { memspec = argv[a + 1]; mem_from_env = 0; }
            for (int b = a; b + 2 <= argc; b++) argv[b] = argv[b + 2];
            argc -= 2; a--;
        } else if (!strncmp(argv[a], "--memlimit=", 11)) {
            memspec = argv[a] + 11; mem_from_env = 0;
            for (int b = a; b < argc; b++) argv[b] = argv[b + 1];
            argc--; a--;
        }
    for (int a = 1; a < argc; a++)                           /* long options, removed before getopt sees the arguments */
        if (!strcmp(argv[a], "--native-leaf") || !strcmp(argv[a], "--typed-huf") || !strcmp(argv[a], "--gpu")) {
            if (!strcmp(argv[a], "--gpu")) { if (!kolmo_set_gpu(1)) fprintf(stderr, "kolmo: --gpu: this build or machine has no GPU path; the CPU does the work\n"); }
            else if (!strcmp(argv[a], "--native-leaf")) native_leaf = 1; else typed_huf = 1;
            for (int b = a; b < argc; b++) argv[b] = argv[b + 1];
            argc--; a--;
        } else if (!strncmp(argv[a], "--x-tables=", 11)) {   /* experimental: -x table sizing (kolmo_set_x_tables) */
            const char *v = argv[a] + 11; int mode = !strcmp(v, "scaled") || !strcmp(v, "default") ? -1 : !strcmp(v, "full") ? 0 : atoi(v);
            if (kolmo_set_x_tables(mode)) { fprintf(stderr, "--x-tables: scaled (the default), full or 16..22\n"); return 1; }
            for (int b = a; b < argc; b++) argv[b] = argv[b + 1];
            argc--; a--;
        }
    while ((opt = getopt(argc, argv, "m:l:T:B:nqQxdr:sS:D:t:P:vM:")) != -1) switch (opt) {
        case 'M': memspec = optarg; mem_from_env = 0; break;
        /* three modes: speed, balanced (default), ratio. The older names stay as advanced settings. */
        case 'P': profpath = optarg; break;
        case 'v': verbose = 1; break;
        case 'm': speed = !strcmp(optarg, "speed"); mode_given = 1;
                  P.mode = !strcmp(optarg, "speed") || !strcmp(optarg, "rt") ? KOLMO_RT :
                           !strcmp(optarg, "balanced") || !strcmp(optarg, "fast") ? KOLMO_FAST :
                           !strcmp(optarg, "ratio") || !strcmp(optarg, "max") ? KOLMO_MAX :
                           !strcmp(optarg, "eco") ? KOLMO_ECO : !strcmp(optarg, "hc") ? KOLMO_HC : !strcmp(optarg, "ultra") ? KOLMO_ULTRA : (kolmo_mode)-1;
                  if ((int)P.mode < 0) { fprintf(stderr, "kolmo: unknown mode '%s' (speed, balanced or ratio)\n", optarg); return 2; }
                  break;
        case 'x': extreme = 1; break;
        case 'l': P.level = atoi(optarg); mode_given = 1; break;
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
            "  --typed-huf  with -m fast -T1: integer NPY arrays and PCM16 WAV files smaller than 8 MiB are coded type\n"
            "            first (integer prediction, Huffman planes). A speed option: several times faster to\n"
            "            compress. On single-channel 16-bit signals (ECG, mono audio) the output is often smaller\n"
            "            too; on other arrays and on multi-channel data (channels are not split) it is often larger\n"
            "            than balanced, so compare sizes before relying on it\n"
            "  -x -l 2   maximum ratio: as -x, and numeric graph leaves may also use context mixing\n"
            "  -M SIZE, --memlimit SIZE  memory limit to compress and decompress: 512M, 8G, 50%% of RAM, auto (default:\n"
            "            50%% of RAM; only how much runs at once) or max. An explicit size also sizes ratio and -x blocks and\n"
            "            the -x tables so the output decodes within it. Env: KOLMO_MEMLIMIT\n"
            "  --x-tables=full  with -x: model tables of up to 5.4 GiB (rc2's sizing) instead of one bucket per 4 bytes,\n"
            "            at most 2.5 GiB (scaled, the default); slightly smaller output, about twice the memory\n"
            "            (smaller integer series; those blocks decode much more slowly)\n"
            "advanced: -m rt|fast|eco|hc|max|ultra  -l level  -q quick analysis  -Q minimal analysis  -n no transforms\n"
            "          -B blockMiB\n"
            "          -s|-S segMiB stream  -r off:len read a range\n"
            "  (dictionaries -D/-t and trained profiles -P, kolmo train: not available in this build)\n"
            "targets:  --target size | 'decode>=1GB/s' | 'encode>=50MB/s' | 'cost:<formula of size,n,enc,dec>' |\n"
            "          cost:cloud[=months] | cost:link=1Gbit   (floors join with ','; -v reports the choice)\n"
            "          kolmo calibrate [-o table] [FILES]   speed table (default ~/.config/kolmo/calibration; --calib FILE)\n"
            "          --gpu  macOS builds made with GPU=1: one GPU worker beside the threads in ratio and -x (same output)\n"); return 2;

    }
    if (extreme) { P.mode = KOLMO_ULTRA; mode_given = 1; }
    uint64_t memlimit = 0;                                     /* 0 = auto */
    if (memspec && kolmo_mem_parse(memspec, &memlimit)) {
        fprintf(stderr, "kolmo: %s '%s': a size (512M, 8G), a share of RAM (50%%), auto or max\n", mem_from_env ? "KOLMO_MEMLIMIT" : "--memlimit", memspec); return 2;
    }
    P.mem_limit = memlimit;                                    /* explicit: a format decision as well (blocks, -x tables) */
    if (memlimit) kolmo_set_memlimit(memlimit);                /* decoders and the governor */
    if (memlimit && memlimit != KOLMO_MEM_MAX && !dec && memlimit < kolmo_mem_floor(P.mode)) { mem_error(NULL, 0, 0, P.mode, memlimit); return 2; }
    if (native_leaf && typed_huf) { fprintf(stderr, "kolmo: choose --native-leaf or --typed-huf, not both\n"); return 2; }
    if (speed && P.transforms == 1) P.transforms = 3;      /* speed: nothing that costs encode time (set before the
                                                            * opt-in bits, so that -m speed with one of them is still speed) */
    if (native_leaf && P.transforms) P.transforms |= KOLMO_T_NATIVE_LEAF;   /* applied by the library at balanced only */
    if (typed_huf && P.transforms) P.transforms |= KOLMO_T_TYPED_HUF;       /* library: fast, default level, one thread */

    /* dictionaries (-D, -t) and trained profiles (-P) need library codecs: not in this build */
    if (dictpath || trainpath || profpath) {
        fprintf(stderr, "kolmo: %s: %s\n", profpath ? "-P" : dictpath ? "-D" : "-t", kolmo_strerror(KOLMO_ERR_UNSUPPORTED)); return 2;
    }

    if (argc - optind != 2) { fprintf(stderr, "need input and output paths\n"); return 2; }
    const char *inp = argv[optind], *outp = argv[optind + 1];

    if (targetspec && !dec && (stream || range)) {
        fprintf(stderr, "kolmo: --target works on whole files in memory; not with -s, -S or -r\n"); return 2;
    }
    if (targetspec && !dec) {                                /* targets: whole file in memory */
        kolmo_target *tg = NULL; int ret = 1;
        int e = kolmo_target_parse(targetspec, &tg);
        if (e) { fprintf(stderr, "kolmo: --target '%s': %s\n", targetspec, kolmo_strerror(e)); return 2; }
        if (kc_apply_calibration(tg, calibpath)) { kolmo_target_free(tg); return 2; }
        size_t n; uint8_t *in = slurp(inp, &n), *out = NULL; int64_t r;
        kolmo_params q = P;
        size_t cap = kolmo_bound(n, &q); out = malloc(cap); kolmo_target_report rep;
        if (!out) r = KOLMO_ERR_ALLOC;
        else r = kolmo_compress_target(in, n, out, cap, &q, tg, NULL, &rep);
        if (r >= 0 && verbose) {
            fprintf(stderr, "kolmo: target '%s': %u chunk%s:", targetspec, rep.chunks, rep.chunks == 1 ? "" : "s");
            for (int s = 0; kolmo_target_setting_name(s); s++) if (rep.per_setting[s]) fprintf(stderr, " %s x%u", kolmo_target_setting_name(s), rep.per_setting[s]);
            fprintf(stderr, "; predicted %llu bytes, encode %.3f s, decode %.3f s (one thread); actual %lld bytes\n",
                    (unsigned long long)rep.predicted_size, rep.predicted_enc_s, rep.predicted_dec_s, (long long)r);
        }
        if (r == KOLMO_ERR_MEMORY) mem_error(NULL, n, 0, P.mode, memlimit);
        else if (r < 0) fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r));
        else ret = spit(outp, out, (size_t)r) != 0;
        free(in); free(out); kolmo_target_free(tg);
        return ret;
    }

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
        int64_t sr = dec ? kolmo_stream_decompress(ifd, ofd, P.threads)
                         : kolmo_stream_compress(ifd, ofd, &P, (uint32_t)(seg > 0 ? seg : 0));
        close(ifd); close(ofd);
        if (sr == KOLMO_ERR_MEMORY) { mem_error(NULL, 0, dec, P.mode, memlimit); return 1; }
        if (sr < 0) { fprintf(stderr, "kolmo: %s\n", kolmo_strerror(sr)); return 1; }
        return 0;
    }

    /* Small files fit in a few blocks. Keep their inexpensive in-memory path. */
#ifndef KOLMO_FILE_THRESHOLD
#define KOLMO_FILE_THRESHOLD (8u << 20)
#endif
    const size_t file_threshold = KOLMO_FILE_THRESHOLD;   /* the file path (reused block buffers, staged output) wins from 8 MiB; below
                                               * that its extra file operations cost more than they save on macOS */
    FILE *input = fopen(inp, "rb"); if (!input) { perror(inp); return 1; }
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
                int err = process_file(fd, st.st_size, outp, &P, dec);
                fclose(input);
                if (err == KOLMO_ERR_MEMORY) {
                    uint8_t h[32]; int ok = dec && pread(fd, h, sizeof h, 0) == (ssize_t)sizeof h;
                    mem_error(ok ? h : NULL, sizeof h, dec, P.mode, memlimit);
                } else if (err) fprintf(stderr, "kolmo: %s\n", kolmo_strerror(err));
                return err != 0;
            }
        }
    }
    size_t n; uint8_t *in = read_input(input, &n), *out; int64_t r;
    if (range) {
        unsigned long long off = strtoull(range, NULL, 10); char *c = strchr(range, ':'); size_t len = c ? strtoull(c + 1, NULL, 10) : 0;
        out = malloc(len ? len : 1); r = kolmo_read_range(in, n, off, len, out, P.threads);
    } else if (dec) {
        int64_t o = kolmo_original_size(in, n);
        if (o >= 0) { int c = kolmo_check_frame(in, n, NULL); if (c) o = c; }   /* checked before the output is allocated */
        if (o < 0) {
            if (o == KOLMO_ERR_MEMORY) mem_error(in, n, 1, P.mode, memlimit); else fprintf(stderr, "kolmo: %s\n", kolmo_strerror(o));
            free(in); return 1;
        }
        out = malloc(o ? o : 1);
        r = kolmo_decompress(in, n, out, o, P.threads);
    } else {
        size_t cap = kolmo_bound(n, &P); out = malloc(cap);
        r = kolmo_compress(in, n, out, cap, &P);
    }
    if (r < 0) {
        if (r == KOLMO_ERR_MEMORY) mem_error(in, n, dec || range, P.mode, memlimit); else fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r));
        free(in); free(out); return 1;
    }
    int failed = spit(outp, out, (size_t)r);
    free(in); free(out);
    return failed != 0;
}
