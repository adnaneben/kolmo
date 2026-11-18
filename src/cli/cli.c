/* kolmo command line:  kolmo [-m rt|fast|eco|hc|max|ultra] [-l level] [-T threads] [-B MiB] [-n|-q] [-D dict] [--native-leaf|--typed-huf] in out
 *                    (--x-tables=scaled|full|16..22: -x model table size, scaled by default; see kolmo_set_x_tables)
 *                    (--native-leaf: balanced only; native plane-context leaves for integer series, KOLMO_T_NATIVE_LEAF)
 *                    (--typed-huf: -m fast -T1, integer NPY / PCM16 WAV coded type first, KOLMO_T_TYPED_HUF)
 *                    (ultra/-x: -l 1 = context-mixing model 1 only, faster; -l 2 = maximum ratio, graph leaves may use CM)
 *                    kolmo -s ...                         (streaming: bounded memory, large inputs)
 *                    kolmo -d [-T threads] [-D dict] in out  (auto-detects one-shot vs streamed)
 *                    kolmo -r offset:length in out        (random access read)
 *                    kolmo -t dict.bin sample1 sample2 ...   (train a dictionary from sample files)
 *                    kolmo train [-m mode] [--target T] -o profile.kpf FILES|DIRS   (learn a reusable profile)
 *                    kolmo -P profile.kpf in out  /  kolmo -d -P profile.kpf in out   (compress, decode with it)
 *                    kolmo --target 'decode>=1GB/s'|size|'cost:...' [--calib table] in out   (per-chunk setting)
 *                    kolmo calibrate [-o table] [FILES]   (speed table for --target)   kolmo profile p.kpf (describe)
 *                    kolmo --index ... / cat / grep / a / x / l   (queryable and deduplicating archives: cli/features.c)
 *                    -M SIZE | --memlimit SIZE   memory limit for compressing and decompressing: 512M, 8G, 50%, auto
 *                                         (default, 50% of RAM) or max; also KOLMO_MEMLIMIT (docs/MEMORY-MODEL.md)
 *                    --jobs-shared=on|off|system   machine-wide pool of worker tokens and memory (docs/CONCURRENCY.md);
 *                                         also KOLMO_JOBS_SHARED, KOLMO_JOBS (tokens), KOLMO_JOBS_DIR
 *                    kolmo --jobs-status            machine-wide use: tokens busy, processes, memory reserved */
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

/* Output is staged in a private file next to the destination (unique name from mkstemp) and published only once it
 * is complete, so a reader never sees a partial file and two instances writing the same path never mix their bytes.
 * A new destination gets the verified inode (link); an existing plain file is replaced atomically (rename) and keeps
 * its mode; symlinks, hard links and special files are written through, to keep their semantics. SIGINT, SIGTERM and
 * SIGHUP remove the staged file before the process ends (the machine pool's tokens are flocks: the kernel releases
 * them with the process, however it ends; docs/CONCURRENCY.md). After kill -9 a staged file `.kolmo-XXXXXX` may stay
 * behind; the destination is never partial. */
#include <signal.h>
static char *volatile cli_stage;                       /* the staged file being written, for the signal handler */
static void cli_on_signal(int sig) {
    char *p = cli_stage; if (p) unlink(p);              /* async-signal-safe */
    signal(sig, SIG_DFL); raise(sig);
}
static void cli_signals(void) {
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = cli_on_signal; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
}
/* opens a staged file for path: its descriptor, *stage its name (free it), or -1 (*stage NULL) when the directory is
 * not writable */
static int stage_open(const char *path, char **stage) {
    const char *slash = strrchr(path, '/');
    size_t dirlen = slash ? (size_t)(slash - path + 1) : 0;
    static const char suffix[] = ".kolmo-XXXXXX";
    *stage = malloc(dirlen + sizeof(suffix));
    if (!*stage) return -1;
    memcpy(*stage, path, dirlen); memcpy(*stage + dirlen, suffix, sizeof(suffix));
    sigset_t all, old; sigfillset(&all); sigprocmask(SIG_BLOCK, &all, &old);    /* name and file appear together */
    int fd = mkstemp(*stage);
    if (fd >= 0) cli_stage = *stage;
    sigprocmask(SIG_SETMASK, &old, NULL);
    if (fd < 0) { free(*stage); *stage = NULL; }
    return fd;
}
static void stage_forget(char *stage) {                 /* the staged name is gone (published or removed) */
    sigset_t all, old; sigfillset(&all); sigprocmask(SIG_BLOCK, &all, &old);
    if (cli_stage == stage) cli_stage = NULL;
    sigprocmask(SIG_SETMASK, &old, NULL);
}
/* publishes the staged output (ret 0) or drops it (ret != 0); closes fd and frees stage; returns ret or an I/O error */
static int stage_publish(int fd, char *stage, FILE *anonymous, const char *path, int ret) {
    if (!ret && !anonymous) {
        struct stat st;
        if (lstat(path, &st) && errno == ENOENT) {
            mode_t mask = umask(0); umask(mask);
            if (!fchmod(fd, 0666 & ~mask) && !link(stage, path)) goto done;
        } else if (!ret && S_ISREG(st.st_mode) && st.st_nlink == 1) {
            /* An existing plain file: replace it atomically and keep its mode, instead of copying the staged output
             * over it (a second pass over the whole output). Symlinks, hard links and special files take the copy. */
            if (!fchmod(fd, st.st_mode & 07777) && !rename(stage, path)) {
                stage_forget(stage);
                if (close(fd)) ret = KOLMO_ERR_IO;
                free(stage); return ret;
            }
        }
    }
    if (!ret) {
        FILE *out = fopen(path, "wb");
        if (!out || lseek(fd, 0, SEEK_SET) < 0) { ret = KOLMO_ERR_IO; if (out) fclose(out); }
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
    else { if (close(fd)) ret = KOLMO_ERR_IO; unlink(stage); stage_forget(stage); }
    free(stage);
    return ret;
}
/* a staged descriptor for path; *anon set when the directory is read-only (an existing output can still be written) */
static int stage_any(const char *path, char **stage, FILE **anon) {
    *anon = NULL;
    int fd = stage_open(path, stage);
    if (fd < 0) { *anon = tmpfile(); if (!*anon) return -1; fd = fileno(*anon); }
    return fd;
}
static int process_file(int input_fd, size_t n, const char *path, const kolmo_params *params, int decode,
                       const void *dict, size_t dictlen) {
    char *stage; FILE *anonymous;
    int fd = stage_any(path, &stage, &anonymous);
    if (fd < 0) return KOLMO_ERR_IO;
    int64_t size = decode ? kolmo_decompress_file(input_fd, fd, n, params->threads, dict, dictlen)
                          : kolmo_compress_file(input_fd, fd, n, params, dict, dictlen);
    return stage_publish(fd, stage, anonymous, path, size < 0 ? (int)size : 0);
}
/* Returns 0, or -1 after reporting the error, so the caller can release its buffers before it exits. */
static int spit(const char *p, const uint8_t *b, size_t n) {
    char *stage; FILE *anonymous;
    int fd = stage_any(p, &stage, &anonymous);
    if (fd < 0) { perror(p); return -1; }
    int ret = 0;
    for (size_t o = 0; o < n && !ret; ) {
        ssize_t w = write(fd, b + o, n - o);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) ret = KOLMO_ERR_IO; else o += (size_t)w;
    }
    if (stage_publish(fd, stage, anonymous, p, ret)) { perror(p); return -1; }
    return 0;
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

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "train")) return kolmo_cli_train(argc - 1, argv + 1);
    if (argc >= 2 && !strcmp(argv[1], "calibrate")) return kolmo_cli_calibrate(argc - 1, argv + 1);
    if (argc == 3 && !strcmp(argv[1], "profile")) {
        kolmo_profile *pp = kc_load_profile(argv[2]); if (!pp) return 1;
        char d[65536]; kolmo_profile_describe(pp, d, sizeof d); fputs(d, stdout); kolmo_profile_free(pp); return 0;
    }
    cli_signals();
    for (int a = 1; a < argc; a++)                           /* the machine-wide pool (docs/CONCURRENCY.md) */
        if (!strncmp(argv[a], "--jobs-shared=", 14)) {
            const char *v = argv[a] + 14;
            int m = !strcmp(v, "off") ? 0 : !strcmp(v, "on") || !strcmp(v, "user") ? 1 : !strcmp(v, "system") ? 2 : -9;
            if (m == -9 || kolmo_jobs_shared(m)) { fprintf(stderr, "kolmo: --jobs-shared=on|off|system\n"); return 2; }
            for (int b = a; b < argc; b++) argv[b] = argv[b + 1];
            argc--; a--;
        }
    if (argc == 2 && (!strcmp(argv[1], "--jobs-status") || !strcmp(argv[1], "--status"))) {
        kolmo_jobs_info in; kolmo_jobs_proc pr[256]; char a[32], b[32];
        int k = kolmo_jobs_status(&in, pr, 256);
        if (k < 0) { printf("machine-wide pool: off (%s)\n", in.state); return 0; }
        printf("machine-wide pool: on (%s), %s\n", in.mode == 2 ? "every user" : "this user", in.dir);
        printf("worker tokens: %d busy of %d\n", in.busy, in.tokens);
        printf("memory reserved by Kolmo processes: %s (shared limit %s)\n", mem_str(in.reserved, a, sizeof a), mem_str(in.limit, b, sizeof b));
        printf("processes: %d\n", k);
        for (int i = 0; i < k && i < 256; i++)
            printf("  pid %d: %d token%s, %d job%s, %s reserved\n", pr[i].pid, pr[i].tokens, pr[i].tokens == 1 ? "" : "s",
                   pr[i].jobs, pr[i].jobs == 1 ? "" : "s", mem_str(pr[i].reserved, a, sizeof a));
        return 0;
    }
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
            "            and need a kolmo that reads container version 9\n"
            "  --typed-huf  with -m fast -T1: integer NPY arrays and PCM16 WAV files smaller than 8 MiB are coded type\n"
            "            first (integer prediction, Huffman planes). A speed option: several times faster to\n"
            "            compress. On single-channel 16-bit signals (ECG, mono audio) the output is often smaller\n"
            "            too; on other arrays and on multi-channel data (channels are not split) it is often larger\n"
            "            than balanced and sometimes larger than plain zstd, so compare sizes before relying on it\n"
            "  -x -l 2   maximum ratio: as -x, and numeric graph leaves may also use context mixing\n"
            "  -M SIZE, --memlimit SIZE  memory limit to compress and decompress: 512M, 8G, 50%% of RAM, auto (default:\n"
            "            50%% of RAM; only how much runs at once) or max. An explicit size also sizes ratio and -x blocks and\n"
            "            the -x tables so the output decodes within it. Env: KOLMO_MEMLIMIT\n"
            "  --x-tables=full  with -x: model tables of up to 5.4 GiB (rc2's sizing) instead of one bucket per 4 bytes,\n"
            "            at most 2.5 GiB (scaled, the default); slightly smaller output, about twice the memory\n"
            "            (smaller integer series; those blocks decode much more slowly)\n"
            "advanced: -m rt|fast|eco|hc|max|ultra  -l level  -q quick analysis  -Q minimal analysis  -n no transforms\n"
            "          -B blockMiB\n"
            "          -D dict  -t dict samples...  -s|-S segMiB stream  -r off:len read a range\n"
            "profiles: kolmo train [-m mode] [--target T] -o p.kpf FILES|DIRS   then  kolmo -P p.kpf in out,\n"
            "          kolmo -d -P p.kpf in out   (the decoder needs the profile; kolmo profile p.kpf describes it)\n"
            "targets:  --target size | 'decode>=1GB/s' | 'encode>=50MB/s' | 'cost:<formula of size,n,enc,dec>' |\n"
            "          cost:cloud[=months] | cost:link=1Gbit   (floors join with ','; -v reports the choice)\n"
            "          kolmo calibrate [-o table] [FILES]   speed table (default ~/.config/kolmo/calibration; --calib FILE)\n"
            "          --gpu  macOS builds made with GPU=1: one GPU worker beside the threads in ratio and -x (same output)\n"
            "pool:     --jobs-shared=on|off|system  share worker tokens (default: the physical cores; KOLMO_JOBS=N) and\n"
            "          memory with the other kolmo processes of this user (on, the default) or of every user (system);\n"
            "          env KOLMO_JOBS_SHARED.  kolmo --jobs-status  shows machine-wide use. Never changes output.\n"); return 2;

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

    /* Training mode: concatenate the sample files given as positional args and write a dictionary. */
    if (trainpath) {
        int nf = argc - optind; if (nf < 1) { fprintf(stderr, "kolmo -t: need at least one sample file\n"); return 2; }
        size_t *sz = malloc(nf * sizeof(size_t)), tot = 0; uint8_t **buf = malloc(nf * sizeof(uint8_t *));
        for (int i = 0; i < nf; i++) { buf[i] = slurp(argv[optind + i], &sz[i]); tot += sz[i]; }
        uint8_t *samp = malloc(tot ? tot : 1); size_t off = 0;
        for (int i = 0; i < nf; i++) { memcpy(samp + off, buf[i], sz[i]); off += sz[i]; free(buf[i]); }
        size_t dcap = 112 * 1024; uint8_t *dict = malloc(dcap);
        int64_t dl = kolmo_train_dict(samp, sz, nf, dict, dcap);
        if (dl < 0) { fprintf(stderr, "kolmo: train: %s\n", kolmo_strerror(dl)); free(samp); free(sz); free(buf); free(dict); return 1; }
        int write_failed = spit(trainpath, dict, (size_t)dl);
        if (!write_failed) fprintf(stderr, "kolmo: wrote %lld-byte dictionary from %d samples to %s\n", (long long)dl, nf, trainpath);
        free(samp); free(sz); free(buf); free(dict);
        return write_failed != 0;
    }

    if (argc - optind != 2) { fprintf(stderr, "need input and output paths\n"); return 2; }
    const char *inp = argv[optind], *outp = argv[optind + 1];

    size_t dictlen = 0; uint8_t *dict = NULL;
    if ((profpath || targetspec) && (dictpath || stream || range || trainpath)) {
        fprintf(stderr, "kolmo: -P and --target work on whole files in memory; not with -D, -s, -S or -r\n"); return 2;
    }
    if (profpath || targetspec) {                            /* profiles and targets: whole file in memory */
        kolmo_profile *prof = NULL; kolmo_target *tg = NULL; int ret = 1;
        if (profpath && !(prof = kc_load_profile(profpath))) return 1;
        if (targetspec && !dec) {
            int e = kolmo_target_parse(targetspec, &tg);
            if (e) { fprintf(stderr, "kolmo: --target '%s': %s\n", targetspec, kolmo_strerror(e)); kolmo_profile_free(prof); return 2; }
            if (kc_apply_calibration(tg, calibpath)) { kolmo_target_free(tg); kolmo_profile_free(prof); return 2; }
        }
        size_t n; uint8_t *in = slurp(inp, &n), *out = NULL; int64_t r;
        if (dec) {
            int64_t o = kolmo_original_size(in, n);
            uint32_t id = 0; int needs = kolmo_frame_profile_id(in, n, &id);
            if (o < 0) r = o;
            else if (needs == 1 && (!prof || kolmo_profile_id(prof) != id)) {
                fprintf(stderr, "kolmo: %s needs profile %08x\n", inp, id); r = KOLMO_ERR_PROFILE;
            } else if ((r = kolmo_check_frame(in, n, NULL)) != 0) {
            } else { out = malloc(o ? (size_t)o : 1); r = out ? kolmo_decompress_profile(in, n, out, (size_t)o, P.threads, prof) : KOLMO_ERR_ALLOC; }
        } else {
            kolmo_params q = P;
            if (prof && !mode_given) { kolmo_params pp; kolmo_profile_params(prof, &pp); q.mode = pp.mode; q.level = pp.level; if (P.transforms == 1) q.transforms = pp.transforms; if (!P.block) q.block = pp.block; }
            size_t cap = kolmo_bound(n, &q); out = malloc(cap); kolmo_target_report rep;
            if (!out) r = KOLMO_ERR_ALLOC;
            else if (tg) r = kolmo_compress_target(in, n, out, cap, &q, tg, prof, &rep);
            else r = kolmo_compress_profile(in, n, out, cap, &q, prof);
            if (tg && r >= 0 && verbose) {
                fprintf(stderr, "kolmo: target '%s': %u chunk%s:", targetspec, rep.chunks, rep.chunks == 1 ? "" : "s");
                for (int s = 0; kolmo_target_setting_name(s); s++) if (rep.per_setting[s]) fprintf(stderr, " %s x%u", kolmo_target_setting_name(s), rep.per_setting[s]);
                fprintf(stderr, "; predicted %llu bytes, encode %.3f s, decode %.3f s (one thread); actual %lld bytes\n",
                        (unsigned long long)rep.predicted_size, rep.predicted_enc_s, rep.predicted_dec_s, (long long)r);
            }
        }
        if (r == KOLMO_ERR_MEMORY) mem_error(dec ? in : NULL, n, dec, P.mode, memlimit);
        else if (r < 0) fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r));
        else ret = spit(outp, out, (size_t)r) != 0;
        free(in); free(out); kolmo_target_free(tg); kolmo_profile_free(prof);
        return ret;
    }
    if (dictpath) dict = slurp(dictpath, &dictlen);

    /* Streaming compress, or decode of a streamed (CZNS) file: bounded memory via file descriptors. */
    int streamed_in = 0;
    if (dec) {
        FILE *pf = fopen(inp, "rb"); if (!pf) { perror(inp); free(dict); return 1; }
        char mg[4] = {0}; size_t g = fread(mg, 1, 4, pf); fclose(pf);
        streamed_in = (g == 4 && !memcmp(mg, "CZNS", 4));
    }
    if ((stream && !dec && !range) || (dec && streamed_in)) {
        int ifd = open(inp, O_RDONLY); if (ifd < 0) { perror(inp); free(dict); return 1; }
        char *stage; FILE *anon;
        int ofd = stage_any(outp, &stage, &anon); if (ofd < 0) { perror(outp); close(ifd); free(dict); return 1; }
        int64_t sr = dec ? kolmo_stream_decompress(ifd, ofd, P.threads)
                         : kolmo_stream_compress(ifd, ofd, &P, (uint32_t)(seg > 0 ? seg : 0));
        close(ifd); free(dict);
        int pr = stage_publish(ofd, stage, anon, outp, sr < 0 ? (int)sr : 0);
        if (sr >= 0 && pr) { perror(outp); return 1; }
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
    FILE *input = fopen(inp, "rb"); if (!input) { perror(inp); free(dict); return 1; }
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
                fclose(input); free(dict);
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
            free(in); free(dict); return 1;
        }
        out = malloc(o ? o : 1);
        r = dict ? kolmo_decompress_dict(in, n, out, o, P.threads, dict, dictlen) : kolmo_decompress(in, n, out, o, P.threads);
    } else {
        size_t cap = kolmo_bound(n, &P); out = malloc(cap);
        r = dict ? kolmo_compress_dict(in, n, out, cap, &P, dict, dictlen) : kolmo_compress(in, n, out, cap, &P);
    }
    if (r < 0) {
        if (r == KOLMO_ERR_MEMORY) mem_error(in, n, dec || range, P.mode, memlimit); else fprintf(stderr, "kolmo: %s\n", kolmo_strerror(r));
        free(in); free(out); free(dict); return 1;
    }
    int failed = spit(outp, out, (size_t)r);
    free(in); free(out); free(dict);
    return failed != 0;
}
