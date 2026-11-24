/* kolmo command line (0.0.2; docs/LEVELS.md):
 *
 *   kolmo [OPTIONS] [FILE...]
 *
 * gzip/xz/zstd conventions: FILE is compressed to FILE.kolmo and kept (--rm removes it after success); -d decompresses
 * FILE.kolmo to FILE; no FILE, or FILE '-', reads standard input and writes standard output. One scale of levels,
 * -1 (fastest) to -9 (max), -3 (light) the default, -U ultra outside the scale; the table is the library's
 * (kolmo_params_for_level), so the CLI and the bindings give the same bytes for the same level. Combined short options
 * (-dc, -9T4), --opt=value and --opt value, and `--` work; so do the xz/gzip spellings of docs/XZ-COMPAT.md (-0, -e, --fast,
 * --best, --uncompress, --to-stdout, --memory, --no-name, -dcf pass-through). A pipe larger than KOLMO_PIPE_ONESHOT is
 * compressed as a stream, so tar -I kolmo runs in bounded memory. Outputs are staged and published atomically, never over an
 * existing file without -f, with the input's permissions and mtime. Exit status: 0 success, 1 error (I/O, corrupt
 * data, memory), 2 usage error; with several files processing continues after an error and the status is 1.
 *
 * The options of 0.0.1 (-m, -x, -l LEVEL, -B MiB, -Q, -n, -s, -S, -r, -D, -t DICT, -P, kolmo train, kolmo profile and
 * the two-positional `kolmo IN OUT`) were removed in 0.0.2: each fails with exit status 2 and a one-line hint.
 * Subcommands: kolmo calibrate (cli/train.c), and kolmo --index / cat / grep / archive / extract / contents (cli/features.c); subcommands are recognised only as the first argument. */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

/* Release label, kept in step with VERSION (RELEASING.md step 1). */
#ifndef KOLMO_VERSION_STR
#define KOLMO_VERSION_STR "0.0.2"
#endif
#define SUFFIX ".kolmo"

static int quiet, verbose;

/* ---- reading and writing ------------------------------------------------------------------------------------- */
static int write_all(int fd, const uint8_t *p, size_t n) {
    while (n) {
        ssize_t w = write(fd, p, n > (1u << 30) ? (1u << 30) : n);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return -1;
        p += w; n -= (size_t)w;
    }
    return 0;
}
/* the whole of fd from its current position (size_hint: bytes expected, 0 = unknown); NULL on error, errno set */
static uint8_t *read_fd(int fd, size_t size_hint, size_t *n) {
    size_t cap = size_hint ? size_hint + 1 : 1 << 16, len = 0;
    uint8_t *b = malloc(cap);
    if (!b) return NULL;
    for (;;) {
        if (len == cap) {
            size_t c = cap * 2; uint8_t *q = c > cap ? realloc(b, c) : NULL;
            if (!q) { free(b); errno = ENOMEM; return NULL; }
            b = q; cap = c;
        }
        ssize_t r = read(fd, b + len, cap - len);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) { int e = errno; free(b); errno = e; return NULL; }
        if (!r) break;
        len += (size_t)r;
    }
    *n = len; return b;
}
/* used by cli/train.c (kolmo calibrate, --calib) */
static uint8_t *read_input(FILE *f, size_t *n) {
    uint8_t *b = read_fd(fileno(f), 0, n);
    if (!b) { perror("read"); exit(1); }
    fclose(f); return b;
}
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
    return read_input(f, n);
}
/* Returns 0, or -1 after reporting the error. */
static int spit(const char *p, const uint8_t *b, size_t n) {
    FILE *f = fopen(p, "wb"); if (!f) { perror(p); return -1; }
    int failed = fwrite(b, 1, n, f) != n;
    if (fclose(f)) failed = 1;
    if (failed) perror(p);
    return failed ? -1 : 0;
}

/* ---- staged output --------------------------------------------------------------------------------------------
 * The complete frame, or the verified decoded output, is written to a private file in the output's directory first.
 * A new destination is published by linking the staged inode (it never replaces a file that appeared meanwhile);
 * with -f, an existing plain file is replaced atomically by rename; symlinks, hard links and special files are
 * written through the original path, which keeps their identity and mode. New and renamed outputs get the input's
 * permissions and mtime (or 0666 & ~umask and the current time for standard input). */
typedef struct { char *name; int fd; FILE *anon; } stage_t;
/* the private file being written, removed if SIGINT, SIGTERM or SIGHUP ends the run (as gzip and xz remove theirs) */
static char *volatile stage_live;
static void on_signal(int sig) {
    char *n = stage_live;
    if (n) unlink(n);
    signal(sig, SIG_DFL);
    raise(sig);
}
static void catch_signals(void) {
    static const int sigs[] = {SIGINT, SIGTERM, SIGHUP};
    for (size_t i = 0; i < sizeof sigs / sizeof *sigs; i++) {
        struct sigaction old, sa; memset(&sa, 0, sizeof sa);
        sa.sa_handler = on_signal; sigemptyset(&sa.sa_mask);
        if (!sigaction(sigs[i], NULL, &old) && old.sa_handler != SIG_IGN) sigaction(sigs[i], &sa, NULL);   /* nohup keeps SIG_IGN */
    }
}
static int stage_open(stage_t *s, const char *path) {
    const char *slash = strrchr(path, '/');
    size_t dirlen = slash ? (size_t)(slash - path + 1) : 0;
    static const char suffix[] = ".kolmo-XXXXXX";
    s->anon = NULL;
    s->name = malloc(dirlen + sizeof(suffix));
    if (!s->name) return -1;
    memcpy(s->name, path, dirlen); memcpy(s->name + dirlen, suffix, sizeof(suffix));
    s->fd = mkstemp(s->name);
    if (s->fd >= 0) stage_live = s->name;
    if (s->fd < 0) {            /* an existing output can be writable in a read-only directory: stage anonymously */
        free(s->name); s->name = NULL;
        s->anon = tmpfile();
        if (!s->anon) return -1;
        s->fd = fileno(s->anon);
    }
    return 0;
}
static void stage_discard(stage_t *s) {
    stage_live = NULL;
    if (s->anon) fclose(s->anon);
    else if (s->fd >= 0) { close(s->fd); unlink(s->name); }
    free(s->name); s->name = NULL; s->fd = -1; s->anon = NULL;
}
static void set_meta(int fd, const struct stat *src) {
    if (src) {
        struct timespec t[2];
#ifdef __APPLE__
        t[0] = src->st_atimespec; t[1] = src->st_mtimespec;
#else
        t[0] = src->st_atim; t[1] = src->st_mtim;
#endif
        (void)fchmod(fd, src->st_mode & 0777);
        (void)futimens(fd, t);
    } else {
        mode_t mask = umask(0); umask(mask);
        (void)fchmod(fd, 0666 & ~mask);
    }
}
static void exists_error(const char *path) { fprintf(stderr, "kolmo: %s: already exists; use -f to overwrite\n", path); }
/* 0, or -1 after reporting the error (the stage is gone either way) */
static int stage_publish(stage_t *s, const char *path, const struct stat *src, int force) {
    int ret = 0;
    if (!s->anon) {
        struct stat st;
        if (lstat(path, &st)) {
            if (errno != ENOENT) { perror(path); stage_discard(s); return -1; }
            set_meta(s->fd, src);
            stage_live = NULL;  /* published under its final name from here: a signal must not remove it */
            if (!link(s->name, path)) goto done;
            if (errno == EEXIST) { exists_error(path); stage_discard(s); return -1; }
            if (!rename(s->name, path)) goto renamed;               /* a file system without hard links */
            perror(path); stage_discard(s); return -1;
        }
        if (!force) { exists_error(path); stage_discard(s); return -1; }
        if (S_ISREG(st.st_mode) && st.st_nlink == 1) {
            set_meta(s->fd, src);
            stage_live = NULL;
            if (!rename(s->name, path)) goto renamed;
        }
    } else if (!force && !access(path, F_OK)) {
        exists_error(path); stage_discard(s); return -1;
    }
    {   /* write through the existing path (symlink, hard link, special file, or a read-only directory) */
        FILE *out = fopen(path, "wb");
        if (!out || lseek(s->fd, 0, SEEK_SET) < 0) { perror(path); if (out) fclose(out); stage_discard(s); return -1; }
        uint8_t buffer[64 * 1024];
        for (;;) {
            ssize_t got = read(s->fd, buffer, sizeof(buffer));
            if (got < 0 && errno == EINTR) continue;
            if (!got) break;
            if (got < 0 || fwrite(buffer, 1, (size_t)got, out) != (size_t)got) { ret = -1; break; }
        }
        if (fclose(out)) ret = -1;
        if (ret) perror(path);
    }
done:
    stage_live = NULL;
    if (s->anon) { if (fclose(s->anon)) ret = -1; }
    else { if (close(s->fd)) ret = -1; unlink(s->name); }
    free(s->name); s->name = NULL; s->fd = -1; s->anon = NULL;
    return ret;
renamed:
    if (close(s->fd)) { perror(path); ret = -1; }
    free(s->name); s->name = NULL; s->fd = -1;
    return ret;
}

int kolmo_cli_features(int argc, char **argv);     /* cli/features.c: cat, grep, archive, extract, contents, --index */
/* for cli/features.c (kolmo archive, kolmo --index): n bytes staged and published at path as the main command line
 * publishes its outputs; an existing path is refused unless force. 0, or -1 after reporting the error. */
int kolmo_cli_publish(const char *path, const uint8_t *p, size_t n, int force) {
    catch_signals();
    stage_t s = {NULL, -1, NULL};
    if (stage_open(&s, path)) { fprintf(stderr, "kolmo: %s: cannot stage the output: %s\n", path, strerror(errno)); return -1; }
    if (write_all(s.fd, p, n)) { fprintf(stderr, "kolmo: %s: %s\n", path, strerror(errno)); stage_discard(&s); return -1; }
    return stage_publish(&s, path, NULL, force);
}

/* ---- messages ------------------------------------------------------------------------------------------------- */
static const char *const LEVEL_NAME[KOLMO_LEVEL_ULTRA + 1] = {
    NULL, "fastest", NULL, "light", NULL, "balanced", NULL, "compact", NULL, "max", "ultra"};
static const char *level_label(int level, char *o, size_t cap) {
    if (level == KOLMO_LEVEL_ULTRA) snprintf(o, cap, "ultra (-U)");
    else if (level >= 1 && level <= 9 && LEVEL_NAME[level]) snprintf(o, cap, "level %d (%s)", level, LEVEL_NAME[level]);
    else snprintf(o, cap, "level %d", level);
    return o;
}
/* sizes for messages: 4.7 GiB, 512 MiB */
static const char *mem_str(uint64_t b, char *o, size_t cap) {
    if (b == UINT64_MAX) snprintf(o, cap, "no limit");
    else if (b >= (1ull << 30)) snprintf(o, cap, "%.1f GiB", (double)b / (1ull << 30));
    else snprintf(o, cap, "%.0f MiB", (double)b / (1ull << 20));
    return o;
}
/* the memory error, with the figure the frame declares (decode) or the level's floor (encode) */
static void mem_error(const uint8_t *hdr, size_t n, int dec, kolmo_mode mode, int level, uint64_t limit) {
    char a[32], b[32], l[32];
    uint64_t lim = limit ? limit : kolmo_mem_auto();
    if (dec) {
        int64_t need = hdr ? kolmo_decode_memory(hdr, n) : -1;
        if (need > 0) {
            uint64_t g = ((uint64_t)need + (1ull << 30) - 1) >> 30;
            fprintf(stderr, "kolmo: needs %s to decode (limit %s); run with --memlimit %lluG or --memlimit max\n",
                    mem_str((uint64_t)need, a, sizeof a), mem_str(lim, b, sizeof b), (unsigned long long)(g ? g : 1));
        } else fprintf(stderr, "kolmo: a frame needs more memory to decode than the limit (%s); run with a larger --memlimit or --memlimit max\n", mem_str(lim, b, sizeof b));
    } else {
        fprintf(stderr, "kolmo: --memlimit %s is below the floor of %s (%s)\n", mem_str(lim, a, sizeof a),
                level_label(level, l, sizeof l), mem_str(kolmo_mem_floor(mode), b, sizeof b));
    }
}
#include "cli/train.c"

static void usage_hint(void) { fprintf(stderr, "Try 'kolmo -h' for help.\n"); }
static int usage_error(const char *fmt, const char *arg) {
    fprintf(stderr, "kolmo: "); fprintf(stderr, fmt, arg); fprintf(stderr, "\n"); usage_hint(); return 2;
}
/* a form removed in 0.0.2: one line naming the replacement, exit status 2 */
static int removed(const char *what, const char *instead) {
    fprintf(stderr, "kolmo: %s was removed in 0.0.2; %s\n", what, instead); return 2;
}
#define HINT_LEVEL "use a level: -1 (fastest) .. -9 (max), -3 light is the default, -U ultra"

static void print_version(void) { printf("kolmo %s (container version 12, Kolmo-native codecs)\n", KOLMO_VERSION_STR); }
static void help(void) {
    printf(
"usage: kolmo [OPTIONS] [FILE...]\n"
"Compress each FILE to FILE.kolmo (inputs are kept), or decompress FILE.kolmo to FILE with -d.\n"
"With no FILE, or FILE '-', read standard input and write standard output.\n"
"\n"
"Levels (each smaller and slower than the one before; 1-7 decompress at LZ speed, 8-9 about 10x more\n"
"slowly; the last level option given wins):\n"
"  -1  -F  --fastest    the fastest Kolmo compresses\n"
"  -2\n"
"  -3  -L  --light      the default: zstd -3 class speed, smaller output\n"
"  -4\n"
"  -5  -B  --balanced   markedly smaller than light, much slower to compress\n"
"  -6\n"
"  -7  -C  --compact    the smallest output that still decompresses at LZ speed\n"
"  -8                   block sorting: smaller, decompresses about 10x more slowly\n"
"  -9  -X  --max        the smallest output that still decompresses at practical speed\n"
"  -U      --ultra      extreme: decompresses about as slowly as it compresses, ~1 GB+ of memory\n"
"          --level N    N = 1..9, or ultra\n"
"  xz and gzip spellings: -0 and --fast = -1, --best = -9, -e/--extreme accepted (no effect:\n"
"  -9e = -9), --memory = --memlimit, --no-name accepted (frames store no name); docs/XZ-COMPAT.md\n"
"\n"
"Operation:\n"
"  -z, --compress       compress (the default)\n"
"  -d, --decompress     decompress FILE.kolmo to FILE (another name needs -o or -c); also --uncompress\n"
"  -t, --test           decode and verify, write nothing\n"
"  -l, --list           compressed size, original size, ratio, engine, checksum\n"
"Output:\n"
"  -c, --stdout         write to standard output, keep inputs; also --to-stdout\n"
"  -o, --output FILE    output path (one input only; '-' is standard output)\n"
"  -k, --keep           keep inputs (the default)\n"
"      --rm             remove each input after its output is written\n"
"  -f, --force          overwrite existing outputs; write compressed data to a terminal; with -dc, copy\n"
"                       input that is not Kolmo's through unchanged (as xz -dcf, zcat -f)\n"
"Resources:\n"
"  -T, --threads N      worker threads (0 = all cores, the default)\n"
"  -M, --memlimit SIZE  memory limit: 512M, 8G, 50%% of RAM, auto (the default: 50%%; only how much runs at once)\n"
"                       or max. An explicit size also sizes the blocks of levels 8, 9 and ultra and the ultra\n"
"                       tables, so the output decodes within it. Environment: KOLMO_MEMLIMIT\n"
"Messages:\n"
"  -q, --quiet          no warnings          -v, --verbose   sizes, ratio and speed per file\n"
"  -h, --help           this help            -V, --version   version\n"
"\n"
"Expert options:\n"
"      --block-size MiB          block size\n"
"      --analysis none|minimal|quick|full   content analysis (the level chooses one)\n"
"      --stream[=SEGMiB]         bounded memory for large inputs (segments of SEGMiB, default 64); a pipe over\n"
"                                256 MiB is streamed in 256 MiB segments without it\n"
"      --range OFF:LEN           decode only bytes [OFF, OFF+LEN) of FILE.kolmo (to -o FILE or standard output)\n"
"      --codec-level N           the engine's own level for the chosen tier (ultra: 1 = model 1 only, faster;\n"
"                                2 = maximum ratio, numeric graph leaves may use context mixing)\n"
"      --target SPEC             per-chunk setting: size | 'decode>=1GB/s' | 'encode>=50MB/s' |\n"
"                                'cost:<formula of size,n,enc,dec>' | cost:cloud[=months] | cost:link=1Gbit\n"
"      --calib FILE              speed table for --target (default ~/.config/kolmo/calibration)\n"
"      --native-leaf             level 3: 5-12%% smaller integer series (sensor, ECG, numeric columns) with Kolmo's\n"
"                                plane-context leaves; such files decode ~1.5-2.6x slower\n"
"      --typed-huf               level 3 with -T1: integer NPY arrays and PCM16 WAV files up to 16 MiB coded type\n"
"                                first; several times faster to compress, sometimes larger: compare sizes\n"
"      --gpu                     macOS builds made with GPU=1: a GPU worker in levels 8, 9 and ultra (same output)\n"
"      --x-tables=scaled|full|16..22   ultra model table size (full: up to 5.4 GiB, slightly smaller output)\n"
"  --                            end of options\n"
"\n"
"Commands:\n"
"  kolmo calibrate [-o table] [FILES]   measure the speed table for --target\n"
"  kolmo --index [LEVEL] in out.kol     queryable archive; kolmo cat | grep FILE.kol to read it\n"
"  kolmo archive | extract | contents   deduplicating archives (create, extract, list their files)\n"
"  Subcommands are recognised only as the first argument; a file of that name: kolmo -- NAME or kolmo ./NAME\n"
"\n"
"With tar: tar -I 'kolmo -6' -cf a.tar.kolmo DIR (GNU tar); bsdtar: --use-compress-program, and 'kolmo -d' to read.\n"
"Exit status: 0 success, 1 error, 2 usage error.\n"
"The 0.0.1 options (-m MODE, -x, kolmo IN OUT, ...) were removed in 0.0.2; CHANGELOG.md has the mapping.\n");
}

/* ---- options -------------------------------------------------------------------------------------------------- */
enum { OP_COMPRESS, OP_DECOMPRESS, OP_TEST, OP_LIST };
typedef struct {
    int op, level, to_stdout, force, rm, stream, seg, threads, analysis, codec_level, native_leaf, typed_huf;
    double block_mib;
    const char *outpath, *range, *target, *calib, *memspec;
    int mem_from_env;
    /* derived */
    kolmo_params P;
    uint64_t memlimit;
    kolmo_target *tg;
} opts_t;

static int is_number(const char *s, int allow_dot) {
    if (!*s) return 0;
    for (; *s; s++) if (!((*s >= '0' && *s <= '9') || (allow_dot && *s == '.'))) return 0;
    return 1;
}
static int exists(const char *p) { struct stat st; return !stat(p, &st); }
static int parse_int(const char *s, int lo, int hi, int *out) {
    char *e; long v = strtol(s, &e, 10);
    if (!*s || *e || v < lo || v > hi) return -1;
    *out = (int)v; return 0;
}
static int set_level_str(opts_t *o, const char *v) {
    if (!strcmp(v, "ultra")) { o->level = KOLMO_LEVEL_ULTRA; return 0; }
    for (int l = 1; l <= KOLMO_LEVEL_MAX; l++) if (LEVEL_NAME[l] && !strcmp(v, LEVEL_NAME[l])) { o->level = l; return 0; }
    int n;
    if (parse_int(v, KOLMO_LEVEL_MIN, KOLMO_LEVEL_MAX, &n)) {
        fprintf(stderr, "kolmo: level '%s': levels are 1 (fastest) to 9 (max); -U / --level ultra is the extreme tier\n", v);
        return 2;
    }
    o->level = n; return 0;
}

/* Long options. Returns 0, or an exit status. *a is advanced past a separate value. */
static int long_option(opts_t *o, int argc, char **argv, int *a) {
    char *s = argv[*a] + 2, *eq = strchr(s, '=');
    size_t nl = eq ? (size_t)(eq - s) : strlen(s);
    char name[32];
    if (nl >= sizeof name) return usage_error("unknown option '%s'", argv[*a]);
    memcpy(name, s, nl); name[nl] = 0;
    const char *val = eq ? eq + 1 : NULL;
    static const char *const flag[] = {"fastest", "light", "balanced", "compact", "max", "ultra", "compress", "decompress",
        "stdout", "keep", "rm", "force", "test", "list", "quiet", "verbose", "help", "version", "native-leaf", "typed-huf",
        "gpu", "stream", "fast", "best", "extreme", "uncompress", "to-stdout", "no-name", NULL};
    static const char *const valued[] = {"level", "output", "threads", "memlimit", "block-size", "analysis", "range",
        "codec-level", "target", "calib", "x-tables", "memory", NULL};
    int f = -1, v = -1;
    for (int i = 0; flag[i]; i++) if (!strcmp(name, flag[i])) f = i;
    for (int i = 0; valued[i]; i++) if (!strcmp(name, valued[i])) v = i;
    if (f < 0 && v < 0) return usage_error("unknown option '%s'", argv[*a]);
    if (f >= 0) {
        if (val && strcmp(name, "stream")) return usage_error("option '--%s' takes no value", name);
        switch (f) {
            case 0: o->level = 1; break;  case 1: o->level = 3; break;  case 2: o->level = 5; break;
            case 3: o->level = 7; break;  case 4: o->level = 9; break;  case 5: o->level = KOLMO_LEVEL_ULTRA; break;
            case 6: o->op = OP_COMPRESS; break;  case 7: o->op = OP_DECOMPRESS; break;
            case 8: o->to_stdout = 1; break;  case 9: o->rm = 0; break;  case 10: o->rm = 1; break;
            case 11: o->force = 1; break;  case 12: o->op = OP_TEST; break;  case 13: o->op = OP_LIST; break;
            case 14: quiet = 1; verbose = 0; break;  case 15: verbose++; quiet = 0; break;
            case 16: help(); exit(0);
            case 17: print_version(); exit(0);
            case 18: o->native_leaf = 1; break;  case 19: o->typed_huf = 1; break;
            case 20: if (!kolmo_set_gpu(1) && !quiet) fprintf(stderr, "kolmo: --gpu: this build or machine has no GPU path; the CPU does the work\n"); break;
            case 21: o->stream = 1;
                     if (val && parse_int(val, 1, 1 << 20, &o->seg)) return usage_error("--stream=SEGMiB: a segment size in MiB, got '%s'", val);
                     break;
            /* xz and gzip spellings (docs/XZ-COMPAT.md) */
            case 22: o->level = 1; break;                       /* gzip --fast */
            case 23: o->level = 9; break;                       /* gzip --best */
            case 24: break;                                     /* xz --extreme: no Kolmo equivalent, accepted */
            case 25: o->op = OP_DECOMPRESS; break;              /* gzip --uncompress */
            case 26: o->to_stdout = 1; break;                   /* gzip --to-stdout */
            case 27: break;                                     /* gzip --no-name: frames store no name or time */
        }
        return 0;
    }
    if (!val) {
        if (*a + 1 >= argc) return usage_error("option '--%s' needs a value", name);
        val = argv[++*a];
    }
    switch (v) {
        case 0: { int r = set_level_str(o, val); if (r) usage_hint(); return r; }
        case 1: o->outpath = val; break;
        case 2: if (parse_int(val, 0, 4096, &o->threads)) return usage_error("--threads: a number of threads (0 = all cores), got '%s'", val); break;
        case 3: case 11: o->memspec = val; o->mem_from_env = 0; break;    /* --memory: xz's alias */
        case 4: { char *e; o->block_mib = strtod(val, &e);
                  if (*e || !(o->block_mib > 0) || o->block_mib > 4095) return usage_error("--block-size: a size in MiB, got '%s'", val); break; }
        case 5: o->analysis = !strcmp(val, "none") ? 0 : !strcmp(val, "minimal") ? 3 : !strcmp(val, "quick") ? 2 : !strcmp(val, "full") ? 1 : -2;
                if (o->analysis == -2) return usage_error("--analysis: none, minimal, quick or full, got '%s'", val);
                break;
        case 6: o->range = val; break;
        case 7: if (parse_int(val, 1, 1000, &o->codec_level)) return usage_error("--codec-level: a positive number, got '%s'", val); break;
        case 8: o->target = val; break;
        case 9: o->calib = val; break;
        case 10: { int m = !strcmp(val, "scaled") || !strcmp(val, "default") ? -1 : !strcmp(val, "full") ? 0 : atoi(val);
                   if (kolmo_set_x_tables(m)) return usage_error("--x-tables: scaled (the default), full or 16..22, got '%s'", val);
                   break; }
    }
    return 0;
}

/* One cluster of short options (argv[*a] = "-dc", "-9T4", "-o" FILE ...). Returns 0 or an exit status. */
static int short_options(opts_t *o, int argc, char **argv, int *a) {
    const char *s = argv[*a] + 1;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9') {                       /* -1 .. -9; several digits form one number (-10 is refused) */
            const char *st = s - 1; while (*s >= '0' && *s <= '9') s++;
            char num[16]; size_t l = (size_t)(s - st); if (l >= sizeof num) l = sizeof num - 1;
            memcpy(num, st, l); num[l] = 0;
            if (!strcmp(num, "0")) { o->level = 1; continue; }   /* xz -0 (its fastest preset): level 1 */
            int r = set_level_str(o, num); if (r) { usage_hint(); return r; }
            continue;
        }
        const char *val = NULL;
        if (c == 'o' || c == 'T' || c == 'M') {            /* options with a value: the rest of the cluster, or the next argument */
            if (*s) val = s;
            else if (*a + 1 < argc) val = argv[++*a];
            else { char opt[3] = {'-', c, 0}; return usage_error("option '%s' needs a value", opt); }
            s = "";
        }
        switch (c) {
            case 'F': o->level = 1; break;  case 'L': o->level = 3; break;  case 'C': o->level = 7; break;
            case 'X': o->level = 9; break;  case 'U': o->level = KOLMO_LEVEL_ULTRA; break;
            case 'B':                                       /* 0.0.1: -B MiB (block size); now --balanced */
                if ((*s >= '0' && *s <= '9') || *s == '.' ||
                    (!*s && *a + 1 < argc && is_number(argv[*a + 1], 1) && !exists(argv[*a + 1])))
                    return removed("-B MiB", "-B is now --balanced (level 5); use --block-size MiB");
                o->level = 5; break;
            case 'l':                                       /* 0.0.1: -l LEVEL (codec level); now --list */
                if ((*s >= '0' && *s <= '9') || (!*s && *a + 1 < argc && is_number(argv[*a + 1], 0) && !exists(argv[*a + 1])))
                    return removed("-l LEVEL", "-l now lists; " HINT_LEVEL " (expert: --codec-level N)");
                o->op = OP_LIST; break;
            case 't':                                       /* 0.0.1: -t DICT (train a dictionary); now --test */
                if (!*s && *a + 1 < argc) { size_t L = strlen(argv[*a + 1]);
                    if (L > 5 && !strcmp(argv[*a + 1] + L - 5, ".dict")) return removed("-t DICT", "-t now tests; dictionaries are not available (Kolmo-native codecs only)"); }
                o->op = OP_TEST; break;
            case 'z': o->op = OP_COMPRESS; break;
            case 'e': break;                                /* xz -e (extreme): no Kolmo equivalent, accepted (-9e = -9) */
            case 'd': o->op = OP_DECOMPRESS; break;
            case 'c': o->to_stdout = 1; break;
            case 'k': o->rm = 0; break;
            case 'f': o->force = 1; break;
            case 'q': quiet = 1; verbose = 0; break;
            case 'v': verbose++; quiet = 0; break;
            case 'h': help(); exit(0);
            case 'V': print_version(); exit(0);
            case 'o': o->outpath = val; break;
            case 'T': if (parse_int(val, 0, 4096, &o->threads)) return usage_error("-T: a number of threads (0 = all cores), got '%s'", val); break;
            case 'M': o->memspec = val; o->mem_from_env = 0; break;
            /* removed in 0.0.2 */
            case 'm': return removed("-m", HINT_LEVEL);
            case 'x': return removed("-x", "use -U (--ultra)");
            case 'Q': return removed("-Q", "use --analysis minimal");
            case 'n': return removed("-n", "use --analysis none");
            case 's': return removed("-s", "use --stream");
            case 'S': return removed("-S", "use --stream=SEGMiB");
            case 'r': return removed("-r", "use --range OFF:LEN");
            case 'D': return removed("-D", "dictionaries are not available (Kolmo-native codecs only)");
            case 'P': return removed("-P", "trained profiles are not available (Kolmo-native codecs only)");
            default: { char opt[3] = {'-', c, 0}; return usage_error("unknown option '%s'", opt); }
        }
    }
    return 0;
}

/* ---- the work, one input at a time ---------------------------------------------------------------------------- */
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int has_suffix(const char *p) { size_t l = strlen(p), s = sizeof(SUFFIX) - 1; return l > s && !strcmp(p + l - s, SUFFIX); }

/* Concatenated frames (as zstd): `kolmo -d` decodes the frames of a file one after another; the library's decoders
 * take exactly one frame. kolmo_frame_size gives a frame's length from its header and index (and, given the whole
 * buffer, the container extensions that end it). Reads go through a buffer or a descriptor. */
typedef struct { const uint8_t *buf; int fd; uint64_t n; } src_t;
static int src_read(const src_t *s, uint64_t off, void *out, size_t len) {
    if (off > s->n || len > s->n - off) return -1;
    if (s->buf) { memcpy(out, s->buf + off, len); return 0; }
    return pread(s->fd, out, len, (off_t)off) == (ssize_t)len ? 0 : -1;
}
static uint64_t le64(const uint8_t *p) { uint64_t v = 0; for (int k = 7; k >= 0; k--) v = v << 8 | p[k]; return v; }
/* the header and the whole index of the frame at off (what kolmo_original_size and kolmo_frame_size need):
 * malloc'ed into *hi, or NULL when no CZN1 frame with a complete index starts there */
static uint8_t *read_head(const src_t *s, uint64_t off, size_t *hn) {
    uint8_t h[32];
    if (src_read(s, off, h, 32) || memcmp(h, "CZN1", 4)) return NULL;
    uint64_t nb = (uint32_t)h[20] | (uint32_t)h[21] << 8 | (uint32_t)h[22] << 16 | (uint32_t)h[23] << 24;
    uint64_t len = 32 + 16 * nb;
    if (len > s->n - off || len > SIZE_MAX) return NULL;
    uint8_t *b = malloc((size_t)len);
    if (!b || src_read(s, off, b, (size_t)len)) { free(b); return NULL; }
    *hn = (size_t)len; return b;
}
/* the claimed decoded size of the frame at off (header and index checked), or a negative error */
static int64_t head_orig(const src_t *s, uint64_t off) {
    size_t hn; uint8_t *hi = read_head(s, off, &hn);
    if (!hi) { uint8_t h[32]; return src_read(s, off, h, 32) ? KOLMO_ERR_CORRUPT : kolmo_original_size(h, 32); }
    int64_t o = kolmo_original_size(hi, hn); free(hi); return o;
}
static uint64_t ext_extent(const src_t *s, uint64_t off, uint64_t end, int *ddp1);
/* the length of the frame at off, or 0 when no whole frame starts there (trailing data) */
static uint64_t frame_extent(const src_t *s, uint64_t off) {
    if (off >= s->n) return 0;
    int64_t e;
    if (s->buf) e = kolmo_frame_size(s->buf + off, (size_t)(s->n - off));   /* extensions ending the buffer included */
    else {
        size_t hn; uint8_t *hi = read_head(s, off, &hn);
        if (!hi) return 0;
        e = kolmo_frame_size(hi, hn); free(hi);
    }
    if (e <= 0 || (uint64_t)e > s->n - off) return 0;
    return ext_extent(s, off, (uint64_t)e, NULL);
}
/* extension sections from off + end (the end of the frame's block payloads), then their KEXT trailer: the frame's
 * extent with them, or end when no trailer closes them. *ddp1 (when given): whether a DDP1 section is among them. */
static uint64_t ext_extent(const src_t *s, uint64_t off, uint64_t end, int *ddp1) {
    uint64_t q = off + end; uint8_t t[24]; int dd = 0;
    if (ddp1) *ddp1 = 0;
    for (int k = 0; k < 64 && q + 4 <= s->n; k++) {
        if (src_read(s, q, t, 4) || !memcmp(t, "CZN1", 4) || !memcmp(t, "CZNS", 4)) break;
        if (!memcmp(t, "KEXT", 4)) {
            if (!src_read(s, q, t, 24) && le64(t + 12) == q - (off + end)) { if (ddp1) *ddp1 = dd; return q + 24 - off; }
            break;
        }
        if (src_read(s, q, t, 12)) break;
        dd |= !memcmp(t, "DDP1", 4);
        uint64_t len = le64(t + 4);
        if (len > s->n - q - 16) break;
        q += 16 + len;
    }
    return end;
}
/* 1 when a frame of s carries a DDP1 section: a `kolmo archive` file, whose frame holds the internal chunk store (the
 * distinct chunks of all its files, without their order), never the files themselves. -d, -t, -l and --range refuse
 * it, pointing to kolmo extract and kolmo contents. */
static int is_archive(const src_t *s) {
    for (uint64_t off = 0; off < s->n;) {
        uint8_t m[8];
        if (!src_read(s, off, m, 4) && !memcmp(m, "CZNS", 4)) {   /* a stream member (FORMAT.md 10.1): its frames */
            uint64_t q = off + 12;
            for (;;) {
                if (src_read(s, q, m, 8)) return 0;
                uint64_t len = le64(m);
                if (!len) break;
                if (len > s->n - q - 8) return 0;
                size_t hn; uint8_t *hi = read_head(&(src_t){s->buf, s->fd, q + 8 + len}, q + 8, &hn);
                if (!hi) return 0;
                int64_t e = kolmo_frame_size(hi, hn); free(hi);
                int dd = 0;
                if (e > 0 && (uint64_t)e <= len) ext_extent(&(src_t){s->buf, s->fd, q + 8 + len}, q + 8, (uint64_t)e, &dd);
                if (dd) return 1;
                q += 8 + len;
            }
            off = q + 8; continue;
        }
        size_t hn; uint8_t *hi = read_head(s, off, &hn);
        if (!hi) return 0;
        int64_t e = kolmo_frame_size(hi, hn); free(hi);
        if (e <= 0 || (uint64_t)e > s->n - off) return 0;
        int dd; uint64_t len = ext_extent(s, off, (uint64_t)e, &dd);
        if (dd) return 1;
        off += len;
    }
    return 0;
}
static void archive_error(const char *name, const char *path) {
    if (!path) path = "FILE";
    fprintf(stderr, "kolmo: %s: a 'kolmo archive' file (many files and a chunk store), not one compressed file; "
                    "unpack it with 'kolmo extract %s', list it with 'kolmo contents %s'\n", name, path, path);
}

/* 1 when a CZNS stream follows the CZN1 frames that start the input: the whole input is then read by the stream
 * decoder, which takes every kind of member (FORMAT.md 10.1) */
static int stream_follows(const src_t *s) {
    uint64_t off = 0, len; uint8_t m[4];
    while ((len = frame_extent(s, off))) off += len;
    return off < s->n && !src_read(s, off, m, 4) && !memcmp(m, "CZNS", 4);
}

/* compress or decompress a buffer in memory; *out is malloc'ed. Returns the output size or a negative error. */
static int64_t code_buffer(const opts_t *o, const uint8_t *in, size_t n, uint8_t **out, const char *name) {
    *out = NULL;
    if (o->range) {
        unsigned long long off = strtoull(o->range, NULL, 10); const char *c = strchr(o->range, ':');
        size_t len = c ? strtoull(c + 1, NULL, 10) : 0;
        int64_t orig = kolmo_original_size(in, n);   /* one whole frame: kolmo_read_range refuses anything after it */
        if (orig < 0) return orig;
        if (off > (uint64_t)orig) return KOLMO_ERR_RANGE;
        if (len > (uint64_t)orig - off) len = (size_t)((uint64_t)orig - off);
        *out = malloc(len ? len : 1); if (!*out) return KOLMO_ERR_ALLOC;
        return kolmo_read_range(in, n, off, len, *out, o->P.threads);
    }
    if (o->op != OP_COMPRESS) {        /* every frame is checked before the output is allocated */
        src_t sr = {in, -1, n};
        uint64_t total = 0, off = 0;
        if (!n) return KOLMO_ERR_CORRUPT;
        while (off < n) {
            uint64_t len = frame_extent(&sr, off);
            if (!len) { int64_t v = kolmo_frame_size(in + off, n - off); return v < 0 ? v : KOLMO_ERR_CORRUPT; }   /* trailing data */
            int64_t orig = kolmo_original_size(in + off, (size_t)len);
            if (orig >= 0) { int c = kolmo_check_frame(in + off, (size_t)len, NULL); if (c) orig = c; }
            if (orig < 0) return orig;
            total += (uint64_t)orig; off += len;
        }
        if (total > SIZE_MAX - 1) return KOLMO_ERR_ALLOC;
        *out = malloc(total ? (size_t)total : 1); if (!*out) return KOLMO_ERR_ALLOC;
        uint64_t got = 0;
        for (off = 0; off < n;) {
            uint64_t len = frame_extent(&sr, off);
            int64_t r = kolmo_decompress(in + off, (size_t)len, *out + got, (size_t)(total - got), o->P.threads);
            if (r < 0) return r;
            got += (uint64_t)r; off += len;
        }
        return (int64_t)got;
    }
    kolmo_params q = o->P;
    size_t cap = kolmo_bound(n, &q);
    *out = malloc(cap ? cap : 1); if (!*out) return KOLMO_ERR_ALLOC;
    if (!o->tg) return kolmo_compress(in, n, *out, cap, &q);
    kolmo_target_report rep;
    int64_t r = kolmo_compress_target(in, n, *out, cap, &q, o->tg, NULL, &rep);
    if (r >= 0 && verbose) {
        fprintf(stderr, "kolmo: %s: target '%s': %u chunk%s:", name, o->target, rep.chunks, rep.chunks == 1 ? "" : "s");
        for (int s = 0; kolmo_target_setting_name(s); s++) if (rep.per_setting[s]) fprintf(stderr, " %s x%u", kolmo_target_setting_name(s), rep.per_setting[s]);
        fprintf(stderr, "; predicted %llu bytes, encode %.3f s, decode %.3f s (one thread); actual %lld bytes\n",
                (unsigned long long)rep.predicted_size, rep.predicted_enc_s, rep.predicted_dec_s, (long long)r);
    }
    return r;
}

/* a reader that closed standard output early (kolmo -d | head) when SIGPIPE is ignored, as on some CI runners and
 * under some shells: the run fails (status 1) without a message, as xz and gzip end quietly by the signal */
static void write_error(const char *where) { if (errno != EPIPE) fprintf(stderr, "kolmo: %s: %s\n", where, strerror(errno)); }
static void report_error(const opts_t *o, const char *name, int64_t e, const uint8_t *hdr, size_t hn) {
    int dec = o->op != OP_COMPRESS || o->range;
    if (e == KOLMO_ERR_IO && errno == EPIPE) return;
    if (e == KOLMO_ERR_MEMORY) { fprintf(stderr, "kolmo: %s:\n", name); mem_error(hdr, hn, dec, o->P.mode, o->level, o->memlimit); }
    else fprintf(stderr, "kolmo: %s: %s\n", name, kolmo_strerror(e));
}

/* ---- pipes (tar -I kolmo, cmd | kolmo | kolmo -d) --------------------------------------------------------------
 * The stream coders read a descriptor. When the CLI has already read the start of a pipe (to see whether the input
 * fits in memory, or which kind of frame it is), a feeder thread writes those bytes and then the rest of the pipe into
 * a pipe of its own, which the coder reads. */
#ifndef KOLMO_PIPE_ONESHOT
#define KOLMO_PIPE_ONESHOT (256u << 20)   /* a pipe up to this size is one frame, as the same bytes in a file */
#endif
typedef struct { const uint8_t *pre; size_t pn; int src, wfd; pthread_t th; int rfd; } feeder_t;
static void feeder_close(void *p) { feeder_t *f = p; if (f->wfd >= 0) { close(f->wfd); f->wfd = -1; } }
static void *feeder_run(void *p) {
    feeder_t *f = p;
    sigset_t m; sigemptyset(&m); sigaddset(&m, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &m, NULL);           /* Linux: a reader that stopped early gives EPIPE here; the
                                                       thread's pending SIGPIPE goes with it */
    pthread_cleanup_push(feeder_close, f);
    if (!write_all(f->wfd, f->pre, f->pn)) {
        uint8_t b[1 << 16];
        for (;;) {
            ssize_t r = read(f->src, b, sizeof b);
            if (r < 0 && errno == EINTR) continue;
            if (r <= 0 || write_all(f->wfd, b, (size_t)r)) break;
        }
    }
    pthread_cleanup_pop(1);
    return NULL;
}
/* the read end of a pipe that yields pre[0..pn) and then the rest of src; -1 on error */
static int feeder_start(feeder_t *f, const uint8_t *pre, size_t pn, int src) {
    int fds[2];
    f->rfd = f->wfd = -1;
    if (pipe(fds)) return -1;
#ifdef F_SETNOSIGPIPE
    (void)fcntl(fds[1], F_SETNOSIGPIPE, 1);         /* macOS raises SIGPIPE for the process, not the writing thread */
#endif
    f->pre = pre; f->pn = pn; f->src = src; f->rfd = fds[0]; f->wfd = fds[1];
    if (pthread_create(&f->th, NULL, feeder_run, f)) { close(fds[0]); close(fds[1]); f->rfd = f->wfd = -1; return -1; }
    return f->rfd;
}
static void feeder_stop(feeder_t *f) {
    if (f->rfd < 0) return;
    close(f->rfd); f->rfd = -1;                     /* a feeder blocked in write returns (EPIPE) */
    pthread_cancel(f->th);                          /* one blocked in read of the source is cancelled */
    pthread_join(f->th, NULL);
}
/* up to lim bytes of fd; *more = 1 when the input goes on past them. NULL on error, errno set. */
static uint8_t *read_upto(int fd, size_t lim, size_t *n, int *more) {
    size_t cap = lim < (1u << 16) ? lim : 1u << 16, len = 0;
    uint8_t *b = malloc(cap);
    *more = 0;
    if (!b) return NULL;
    for (;;) {
        if (len == cap) {
            if (len >= lim) {                       /* full: one more byte says whether the input ends here */
                uint8_t one; ssize_t r;
                do r = read(fd, &one, 1); while (r < 0 && errno == EINTR);
                if (r < 0) { int e = errno; free(b); errno = e; return NULL; }
                if (r == 1) {
                    uint8_t *q = realloc(b, len + 1);
                    if (!q) { free(b); errno = ENOMEM; return NULL; }
                    b = q; b[len++] = one; *more = 1;
                }
                break;
            }
            size_t c = cap * 2 < lim ? cap * 2 : lim; uint8_t *q = realloc(b, c);
            if (!q) { free(b); errno = ENOMEM; return NULL; }
            b = q; cap = c;
        }
        ssize_t r = read(fd, b + len, cap - len);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) { int e = errno; free(b); errno = e; return NULL; }
        if (!r) break;
        len += (size_t)r;
    }
    *n = len; return b;
}
static int is_kolmo_magic(const uint8_t *h, size_t n) { return n >= 4 && (!memcmp(h, "CZN1", 4) || !memcmp(h, "CZNS", 4)); }

#ifndef KOLMO_FILE_THRESHOLD
#define KOLMO_FILE_THRESHOLD (8u << 20)
#endif

/* in: a path, or NULL for standard input. Returns 0 or 1. */
static int process(const opts_t *o, const char *in) {
    const char *name = in ? in : "(stdin)";
    int ifd = 0; struct stat ist; int ireg = 0;
    if (in) {
        ifd = open(in, O_RDONLY);
        if (ifd < 0) { fprintf(stderr, "kolmo: %s: %s\n", in, strerror(errno)); return 1; }
    }
    if (fstat(ifd, &ist)) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); if (in) close(ifd); return 1; }
    ireg = S_ISREG(ist.st_mode);
    if (in && S_ISDIR(ist.st_mode)) { fprintf(stderr, "kolmo: %s: is a directory -- ignored\n", in); close(ifd); return 1; }

    /* where the output goes */
    int to_stdout = 0; const char *outp = NULL; char *derived = NULL;
    if (o->op == OP_COMPRESS || o->op == OP_DECOMPRESS) {
        if (o->outpath) { if (!strcmp(o->outpath, "-")) to_stdout = 1; else outp = o->outpath; }
        else if (o->to_stdout || !in || o->range) to_stdout = 1;
        else if (o->op == OP_COMPRESS) {
            if (has_suffix(in) && !o->force) { fprintf(stderr, "kolmo: %s: already has the %s suffix -- ignored (use -f to compress it again)\n", in, SUFFIX); close(ifd); return 1; }
            size_t l = strlen(in); derived = malloc(l + sizeof(SUFFIX));
            if (!derived) { close(ifd); return 1; }
            memcpy(derived, in, l); memcpy(derived + l, SUFFIX, sizeof(SUFFIX)); outp = derived;
        } else {
            if (!has_suffix(in)) { fprintf(stderr, "kolmo: %s: unknown suffix (not %s) -- use -o FILE or -c\n", in, SUFFIX); close(ifd); return 1; }
            derived = strdup(in); if (!derived) { close(ifd); return 1; }
            derived[strlen(derived) - (sizeof(SUFFIX) - 1)] = 0; outp = derived;
        }
        if (in && !ireg && !to_stdout && !o->force) {
            fprintf(stderr, "kolmo: %s: not a regular file -- ignored (use -c or -f)\n", in); close(ifd); free(derived); return 1;
        }
        struct stat os;
        if (outp ? !stat(outp, &os) : (in && !fstat(1, &os) && S_ISREG(os.st_mode))) {
            if (in && os.st_dev == ist.st_dev && os.st_ino == ist.st_ino) {   /* never, in any path, even with -f */
                fprintf(stderr, "kolmo: %s: the output is the input file -- refused\n", in); close(ifd); free(derived); return 1;
            }
            if (outp && !o->force) { exists_error(outp); close(ifd); free(derived); return 1; }
        }
    }

    int dec = o->op != OP_COMPRESS;
    double t0 = now_s();
    uint8_t hdr[32]; size_t hn = 0;
    int ret = 0; int64_t insize = -1, outsize = -1;
    uint8_t *buf = NULL, *obuf = NULL; size_t bn = 0;
    feeder_t fd_feed = {NULL, 0, -1, -1, 0, -1}; /* a pipe whose start was read already, replayed to a stream coder */
    int sfd = ifd;                           /* the descriptor streamed decoders read */
    int streamed = 0, buffered = 0, filepath = 0;
    stage_t st = {NULL, -1, NULL};
    int ofd = 1;
    int64_t r = 0;

    /* a peek at the first bytes: CZNS streams decode through file descriptors */
    if (dec) {
        if (ireg) { ssize_t g = pread(ifd, hdr, sizeof hdr, 0); hn = g > 0 ? (size_t)g : 0; }
        else {                               /* a pipe: its first 4 bytes say how to read the rest */
            int more = 0;
            buf = read_upto(ifd, 4, &bn, &more);
            if (!buf) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; }
            buffered = 1; hn = bn < 4 ? bn : 4; memcpy(hdr, buf, hn);
        }
        streamed = hn >= 4 && !memcmp(hdr, "CZNS", 4);
        if (streamed && o->range) { fprintf(stderr, "kolmo: %s: --range needs a one-shot frame, not a --stream file\n", name); ret = 1; goto out; }
        if (buffered && !streamed && !is_kolmo_magic(hdr, hn) && o->op == OP_DECOMPRESS && o->force && to_stdout) {
            /* -dcf on data that is not Kolmo's: copied through unchanged, as xz -dcf and gzip -dcf do */
        } else if (buffered && !streamed) {  /* a one-shot frame: the rest of the pipe in memory */
            size_t rn; uint8_t *rest = read_fd(ifd, 0, &rn), *q;
            if (!rest || !(q = realloc(buf, bn + rn + 1))) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(rest ? ENOMEM : errno)); free(rest); ret = 1; goto out; }
            buf = q; memcpy(buf + bn, rest, rn); bn += rn; free(rest);
            hn = bn < sizeof hdr ? bn : sizeof hdr; memcpy(hdr, buf, hn);
        }
        /* frames followed by a stream (FORMAT.md 10.1): all of it through the stream decoder, which reads both */
        if (!streamed && !o->range && is_kolmo_magic(hdr, hn))
            streamed = buffered ? stream_follows(&(src_t){buf, -1, bn}) : ireg && stream_follows(&(src_t){NULL, ifd, (uint64_t)ist.st_size});
        if (streamed && buffered) {
            sfd = feeder_start(&fd_feed, buf, bn, ifd);
            if (sfd < 0) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; }
        }
        if (!streamed && !is_kolmo_magic(hdr, hn) && o->op == OP_DECOMPRESS && o->force && to_stdout && !o->range) {
            if (buffered) {
                if (write_all(1, buf, bn)) { write_error("(stdout)"); ret = 1; goto out; }
            } else if (lseek(ifd, 0, SEEK_SET) < 0) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; }
            uint8_t b[1 << 16];
            for (;;) {
                ssize_t g = read(ifd, b, sizeof b);
                if (g < 0 && errno == EINTR) continue;
                if (g < 0) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; break; }
                if (!g) break;
                if (write_all(1, b, (size_t)g)) { write_error("(stdout)"); ret = 1; break; }
            }
            goto out;
        }
    }

    if (dec && is_kolmo_magic(hdr, hn) &&                 /* any member, frames followed by a stream included */
        is_archive(buffered ? &(src_t){buf, -1, bn} : &(src_t){NULL, ifd, ireg ? (uint64_t)ist.st_size : 0})) {
        archive_error(name, in); ret = 1; goto out;
    }

    if (o->op == OP_TEST) {
        if (streamed) {
            int nul = open("/dev/null", O_WRONLY);
            r = nul < 0 ? KOLMO_ERR_IO : kolmo_stream_decompress(sfd, nul, o->P.threads);
            if (nul >= 0) close(nul);
        } else {
            if (!buffered) { buf = read_fd(ifd, ireg ? (size_t)ist.st_size : 0, &bn); if (!buf) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; } }
            r = code_buffer(o, buf, bn, &obuf, name);
        }
        if (r < 0) { report_error(o, name, r, hdr, hn); ret = 1; }
        else if (verbose) fprintf(stderr, "kolmo: %s: OK (%lld bytes)\n", name, (long long)r);
        goto out;
    }

    /* the sink: standard output, or a staged file */
    if (!to_stdout) {
        if (stage_open(&st, outp)) { fprintf(stderr, "kolmo: %s: cannot stage the output: %s\n", outp, strerror(errno)); ret = 1; goto out; }
        ofd = st.fd;
    }
    int pipe_stream = 0;
    if (!dec && !ireg && !o->stream && !o->tg) {   /* a pipe: one frame if it fits KOLMO_PIPE_ONESHOT, else a stream */
        int more = 0;
        buf = read_upto(ifd, KOLMO_PIPE_ONESHOT, &bn, &more);
        if (!buf) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; }
        buffered = 1;
        if (more) {
            pipe_stream = 1;
            sfd = feeder_start(&fd_feed, buf, bn, ifd);
            if (sfd < 0) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; }
        }
    }
    if (!dec && (o->stream || pipe_stream) && !o->tg) {
        r = kolmo_stream_compress(sfd, ofd, &o->P, o->stream ? (uint32_t)o->seg : (uint32_t)(KOLMO_PIPE_ONESHOT >> 20));
        if (r >= 0) { outsize = r; insize = ireg ? (int64_t)ist.st_size : -1; }
    } else if (streamed) {
        r = kolmo_stream_decompress(sfd, ofd, o->P.threads);
        if (r >= 0) outsize = r;
        insize = ireg ? (int64_t)ist.st_size : -1;
    } else if (!to_stdout && ireg && !buffered && !o->range && !o->tg &&
               (dec || o->P.mode == KOLMO_FAST || o->P.mode == KOLMO_RT) && (uintmax_t)ist.st_size <= SIZE_MAX &&
               (dec ? (hn == sizeof hdr && head_orig(&(src_t){NULL, ifd, (uint64_t)ist.st_size}, 0) >= (int64_t)KOLMO_FILE_THRESHOLD &&
                       frame_extent(&(src_t){NULL, ifd, (uint64_t)ist.st_size}, 0) == (uint64_t)ist.st_size)
                    : ist.st_size >= (off_t)KOLMO_FILE_THRESHOLD)) {
        /* large files: reused block buffers, output written by offset into the stage (verified before publishing) */
        filepath = 1;
        r = dec ? kolmo_decompress_file(ifd, ofd, (size_t)ist.st_size, o->P.threads, NULL, 0)
                : kolmo_compress_file(ifd, ofd, (size_t)ist.st_size, &o->P, NULL, 0);
        if (r >= 0) { outsize = r; insize = ist.st_size; }
    } else {
        if (!buffered) {
            buf = read_fd(ifd, ireg ? (size_t)ist.st_size : 0, &bn);
            if (!buf) { fprintf(stderr, "kolmo: %s: %s\n", name, strerror(errno)); ret = 1; goto out; }
        }
        r = code_buffer(o, buf, bn, &obuf, name);
        if (r >= 0) {
            insize = (int64_t)bn; outsize = r;
            if (write_all(ofd, obuf, (size_t)r)) { write_error(to_stdout ? "(stdout)" : outp); ret = 1; goto out; }
        }
    }
    if (r < 0) { report_error(o, name, r, dec ? hdr : NULL, dec ? hn : 0); ret = 1; goto out; }
    if (!to_stdout) {
        int e = stage_publish(&st, outp, in && ireg ? &ist : NULL, o->force);
        if (e) { ret = 1; goto out; }
    }
    if (verbose) {
        double t = now_s() - t0;
        if (insize >= 0 && outsize >= 0) {
            int64_t raw = dec ? outsize : insize, packed = dec ? insize : outsize;
            fprintf(stderr, "kolmo: %s: %lld -> %lld bytes (%.2f%%), %.1f MB/s%s%s%s\n", name, (long long)(dec ? packed : raw),
                    (long long)(dec ? raw : packed), raw ? 100.0 * (double)packed / (double)raw : 0.0,
                    t > 0 ? (double)raw / t / 1e6 : 0.0, outp ? " -> " : "", outp ? outp : "",
                    verbose > 1 ? (filepath ? " [file path]" : streamed || o->stream ? " [stream]" : " [in memory]") : "");
        } else fprintf(stderr, "kolmo: %s: done%s%s\n", name, outp ? " -> " : "", outp ? outp : "");
    }
    /* --rm: the input goes once its output is published (never with -c, standard input, or the output itself) */
    if (o->rm && in && outp && unlink(in)) { fprintf(stderr, "kolmo: %s: cannot remove: %s\n", in, strerror(errno)); ret = 1; }
out:
    feeder_stop(&fd_feed);
    if (st.fd >= 0) stage_discard(&st);
    free(buf); free(obuf); free(derived);
    if (in) close(ifd);
    return ret;
}

/* -l: compressed size, original size, ratio, engine, checksum of one file */
static const char *codec_name(int c) {
    switch (c) {
        case 0: return "store"; case 5: return "bsc"; case 7: return "cm"; case 12: return "graph"; case 16: return "fqfam";
        case 17: return "cm-image"; case 18: return "bwt"; case 19: return "audio"; case 20: return "video";
        case 24: return "klzma"; case 26: return "gr6"; case 28: return "klz"; case 29: return "klzh"; case 31: return "kimg";
        case 32: return "klzh2";
    }
    return "other";
}
/* header byte 5: the kolmo_mode that wrote the frame (informational) */
static const char *mode_name(int m) {
    static const char *const n[] = {"rt", "fast", "eco", "hc", "max", "ultra"};
    return m >= 0 && m < 6 ? n[m] : "?";
}
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int list_one(const char *in, int *header_done) {
    int fd = open(in, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "kolmo: %s: %s\n", in, strerror(errno)); return 1; }
    struct stat st; uint8_t h[32];
    if (fstat(fd, &st) || !S_ISREG(st.st_mode)) { fprintf(stderr, "kolmo: %s: not a regular file\n", in); close(fd); return 1; }
    ssize_t g = pread(fd, h, sizeof h, 0);
    char engine[200] = ""; uint64_t orig = 0; uint32_t crc = 0; int ok = 1, have_crc = 0;
    /* the members one after another (FORMAT.md 10.1): CZN1 frames and CZNS streams, whose segments are frames too */
    src_t sr = {NULL, fd, (uint64_t)st.st_size};
    uint64_t off = 0, frames = 0, streams = 0, segs = 0; uint32_t used[64] = {0};
    if (g < 4) ok = 0;
    else if (is_archive(&sr)) { archive_error(in, in); close(fd); return 1; }
    while (ok && off < (uint64_t)st.st_size) {
        uint8_t m[4]; uint64_t end = 0, foff = off, flen = 0;
        if (pread(fd, m, 4, (off_t)off) != 4) { ok = 0; break; }
        int in_stream = !memcmp(m, "CZNS", 4);
        if (in_stream) { if ((uint64_t)st.st_size - off < 12) { ok = 0; break; } foff = off + 12; streams++; }
        for (;;) {                                         /* one frame, or each segment of the stream */
            if (in_stream) {
                uint8_t lb[8];
                if (pread(fd, lb, 8, (off_t)foff) != 8) { ok = 0; break; }
                flen = le64(lb);
                if (!flen) { end = foff + 8; break; }
                if (flen < 32 || flen > (uint64_t)st.st_size - foff - 8) { ok = 0; break; }
                foff += 8;
            } else if (!(flen = frame_extent(&sr, foff))) { ok = 0; break; }
            uint8_t fh[32]; src_t fs = {NULL, fd, foff + flen};
            if (pread(fd, fh, 32, (off_t)foff) != 32) { ok = 0; break; }
            int64_t o = head_orig(&fs, foff);
            if (o < 0) { if (in_stream) { ok = 0; break; } fprintf(stderr, "kolmo: %s: %s\n", in, kolmo_strerror(o)); close(fd); return 1; }
            orig += (uint64_t)o;
            if (!frames && !segs) { crc = le32(fh + 28); have_crc = 1; snprintf(engine, sizeof engine, "%s:", mode_name(fh[5])); }
            uint32_t nb = le32(fh + 20);
            for (uint32_t i = 0; i < nb; i++) {
                uint8_t e[16];
                if (pread(fd, e, 16, (off_t)(foff + 32 + 16 * (uint64_t)i)) != 16) { ok = 0; break; }
                used[e[12] & 63]++;
            }
            if (!ok) break;
            if (!in_stream) { frames++; end = foff + flen; break; }
            segs++; foff += flen;
        }
        off = end;
    }
    if (ok) {
        for (int c = 0; c < 64; c++) if (used[c]) { size_t l = strlen(engine); snprintf(engine + l, sizeof engine - l, " %s", codec_name(c)); }
        size_t l = strlen(engine);
        if (frames > 1 || (frames && streams)) l += (size_t)snprintf(engine + l, sizeof engine - l, ", %llu frames", (unsigned long long)frames);
        if (streams == 1) snprintf(engine + l, sizeof engine - l, ", stream of %llu segment%s", (unsigned long long)segs, segs == 1 ? "" : "s");
        else if (streams > 1) snprintf(engine + l, sizeof engine - l, ", %llu streams of %llu segments", (unsigned long long)streams, (unsigned long long)segs);
        if (streams) have_crc = 0;                         /* a stream has no one frame's checksum to show */
    }
    close(fd);
    if (!ok) { fprintf(stderr, "kolmo: %s: %s\n", in, kolmo_strerror(KOLMO_ERR_CORRUPT)); return 1; }
    if (!*header_done) { printf("%14s %14s %8s  %-8s  %s\n", "compressed", "original", "ratio", "crc32c", "engine  file"); *header_done = 1; }
    char cs[16]; if (have_crc) snprintf(cs, sizeof cs, "%08x", crc); else snprintf(cs, sizeof cs, "-");
    printf("%14llu %14llu %7.3fx  %-8s  %s  %s\n", (unsigned long long)st.st_size, (unsigned long long)orig,
           st.st_size ? (double)orig / (double)st.st_size : 0.0, cs, engine, in);
    return 0;
}

int main(int argc, char **argv) {
    /* subcommands, and the removed ones */
    if (argc >= 2 && !strcmp(argv[1], "calibrate")) return kolmo_cli_calibrate(argc - 1, argv + 1);
    if (argc >= 2 && !strcmp(argv[1], "train")) return removed("'kolmo train'", "trained profiles and dictionaries are not available (Kolmo-native codecs only)");
    if (argc >= 2 && !strcmp(argv[1], "profile")) return removed("'kolmo profile'", "trained profiles are not available (Kolmo-native codecs only)");
    { int fr = kolmo_cli_features(argc, argv); if (fr >= 0) return fr; }
    catch_signals();

    opts_t o; memset(&o, 0, sizeof o);
    o.op = OP_COMPRESS; o.level = KOLMO_LEVEL_DEFAULT; o.analysis = -1;
    o.memspec = getenv("KOLMO_MEMLIMIT"); o.mem_from_env = o.memspec != NULL;
    char **files = malloc(sizeof(char *) * (size_t)(argc + 1)); int nf = 0, dashdash = 0, ret = 2;
    if (!files) { perror("kolmo"); return 1; }
    for (int a = 1; a < argc; a++) {
        char *s = argv[a]; int r = 0;
        if (dashdash || s[0] != '-' || !s[1]) files[nf++] = s;
        else if (!strcmp(s, "--")) dashdash = 1;
        else if (s[1] == '-') r = long_option(&o, argc, argv, &a);
        else r = short_options(&o, argc, argv, &a);
        if (r) { free(files); return r; }
    }

    /* the two-positional form of 0.0.1: `kolmo IN OUT` and `kolmo -d IN OUT` (a second name that does not exist yet,
     * or that has the wrong suffix to be a second input) */
    if (nf == 2 && (o.op == OP_COMPRESS || o.op == OP_DECOMPRESS) && !o.outpath && !o.to_stdout && !o.range &&
        strcmp(files[0], "-") && strcmp(files[1], "-") &&
        (!exists(files[1]) || (o.op == OP_COMPRESS ? has_suffix(files[1]) : !has_suffix(files[1])))) {
        free(files);
        return o.op == OP_COMPRESS ? removed("'kolmo IN OUT'", "use 'kolmo IN -o OUT'") : removed("'kolmo -d IN OUT'", "use 'kolmo -d IN -o OUT'");
    }

    /* the level, then the expert overrides */
    kolmo_params_for_level(o.level, &o.P);
    o.P.threads = o.threads;
    if (o.analysis >= 0) o.P.transforms = o.analysis;
    if (o.codec_level) o.P.level = o.codec_level;
    if (o.block_mib > 0) o.P.block = (uint32_t)(o.block_mib * 1048576);
    if (o.native_leaf && o.typed_huf) { usage_error("choose --native-leaf or --typed-huf, not both%s", ""); goto done; }
    if (o.native_leaf && o.P.transforms) o.P.transforms |= KOLMO_T_NATIVE_LEAF;   /* applied by the library at level 3 only */
    if (o.typed_huf && o.P.transforms) o.P.transforms |= KOLMO_T_TYPED_HUF;       /* library: level 3, default codec level, one thread */
    if (o.memspec && kolmo_mem_parse(o.memspec, &o.memlimit)) {
        fprintf(stderr, "kolmo: %s '%s': a size (512M, 8G), a share of RAM (50%%), auto or max\n", o.mem_from_env ? "KOLMO_MEMLIMIT" : "--memlimit", o.memspec);
        goto done;
    }
    o.P.mem_limit = o.memlimit;                                /* explicit: a format decision as well (blocks, ultra tables) */
    if (o.memlimit) kolmo_set_memlimit(o.memlimit);            /* decoders and the governor */
    if (o.memlimit && o.memlimit != KOLMO_MEM_MAX && o.op == OP_COMPRESS && !o.range && o.memlimit < kolmo_mem_floor(o.P.mode)) {
        mem_error(NULL, 0, 0, o.P.mode, o.level, o.memlimit); goto done;
    }

    /* combinations */
    if (o.range) {
        const char *c = strchr(o.range, ':'); char *e = NULL;
        if (c) strtoull(o.range, &e, 10);
        if (!c || e != c || c == o.range || !is_number(c + 1, 0)) { usage_error("--range OFF:LEN, got '%s'", o.range); goto done; }
        if (o.op == OP_TEST || o.op == OP_LIST) { usage_error("--range cannot be combined with %s", o.op == OP_TEST ? "-t" : "-l"); goto done; }
        o.op = OP_DECOMPRESS;                                 /* a range is always decoded */
    }
    if (o.outpath && nf > 1) { usage_error("-o takes one input; %s", "with several, each FILE gets FILE.kolmo (or use -c)"); goto done; }
    if (o.outpath && o.to_stdout) { usage_error("choose -c or -o, not both%s", ""); goto done; }
    if (o.outpath && (o.op == OP_TEST || o.op == OP_LIST)) { usage_error("-o is not used with %s", o.op == OP_TEST ? "-t" : "-l"); goto done; }
    if (o.op == OP_LIST && !nf) { usage_error("-l needs FILE arguments (standard input is not listed)%s", ""); goto done; }
    if (o.target && o.op == OP_COMPRESS) {
        if (o.stream) { usage_error("--target works on whole inputs in memory; not with --stream%s", ""); goto done; }
        int e = kolmo_target_parse(o.target, &o.tg);
        if (e) { fprintf(stderr, "kolmo: --target '%s': %s\n", o.target, kolmo_strerror(e)); goto done; }
        if (kc_apply_calibration(o.tg, o.calib)) goto done;
    }
    if (o.rm && !quiet && (o.to_stdout || (o.outpath && !strcmp(o.outpath, "-")) || o.op == OP_TEST || o.op == OP_LIST))
        fprintf(stderr, "kolmo: warning: --rm is ignored with %s\n", o.op == OP_TEST ? "-t" : o.op == OP_LIST ? "-l" : "standard output");

    if (!nf) files[nf++] = "-";
    /* terminals: compressed data is not written to one, nor read from one, without -f (as zstd) */
    int any_stdin = 0, any_stdout = 0;
    for (int i = 0; i < nf; i++) any_stdin |= !strcmp(files[i], "-");
    if (o.op == OP_COMPRESS || o.op == OP_DECOMPRESS)
        any_stdout = o.to_stdout || (o.outpath && !strcmp(o.outpath, "-")) || (!o.outpath && (any_stdin || o.range));
    ret = 1;
    if (o.op == OP_COMPRESS && any_stdout && isatty(1) && !o.force) {
        fprintf(stderr, "kolmo: refusing to write compressed data to a terminal; use -f, -o FILE or a redirection\n");
        usage_hint(); goto done;
    }
    if (o.op != OP_COMPRESS && o.op != OP_LIST && any_stdin && isatty(0) && !o.force) {
        fprintf(stderr, "kolmo: refusing to read compressed data from a terminal; use -f or a redirection\n");
        usage_hint(); goto done;
    }
    if (o.op == OP_COMPRESS && any_stdin && isatty(0) && !quiet)
        fprintf(stderr, "kolmo: reading standard input from the terminal (end with Ctrl-D)\n");

    ret = 0;
    int header = 0;
    for (int i = 0; i < nf; i++) {
        int stdin_in = !strcmp(files[i], "-");
        if (o.op == OP_LIST) { if (stdin_in) { fprintf(stderr, "kolmo: -l does not read standard input\n"); ret = 1; } else ret |= list_one(files[i], &header); }
        else ret |= process(&o, stdin_in ? NULL : files[i]);
    }
done:
    kolmo_target_free(o.tg); free(files);
    return ret;
}
