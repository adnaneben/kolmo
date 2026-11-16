/* kolmo train | calibrate | profile: the command-line side of trained profiles and --target (src/api/features.c).
 * Included by src/cli.c until the structure lane moves the CLI to src/cli/. */
#include <dirent.h>

typedef struct { char **path; size_t *size; size_t n, cap; } kc_files;
static int kc_add(kc_files *F, const char *path, size_t size) {
    if (F->n == F->cap) {
        size_t c = F->cap ? F->cap * 2 : 64;
        char **p = realloc(F->path, c * sizeof *p); if (!p) return -1; F->path = p;
        size_t *s = realloc(F->size, c * sizeof *s); if (!s) return -1; F->size = s;
        F->cap = c;
    }
    F->path[F->n] = strdup(path); F->size[F->n] = size;
    return F->path[F->n++] ? 0 : -1;
}
static int kc_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
/* regular files under path (recursively for a directory), in sorted order so a profile does not depend on readdir */
static int kc_collect(kc_files *F, const char *path, int depth) {
    struct stat st;
    if (stat(path, &st)) { perror(path); return -1; }
    if (S_ISREG(st.st_mode)) return kc_add(F, path, (size_t)st.st_size);
    if (!S_ISDIR(st.st_mode) || depth > 32) return 0;
    DIR *d = opendir(path); if (!d) { perror(path); return -1; }
    char **names = NULL; size_t nn = 0, nc = 0; struct dirent *e; int ret = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (nn == nc) { nc = nc ? nc * 2 : 64; char **q = realloc(names, nc * sizeof *q); if (!q) { ret = -1; break; } names = q; }
        if (!(names[nn] = strdup(e->d_name))) { ret = -1; break; }
        nn++;
    }
    closedir(d);
    if (nn) qsort(names, nn, sizeof *names, kc_cmp);
    for (size_t i = 0; i < nn && !ret; i++) {
        size_t l = strlen(path) + strlen(names[i]) + 2; char *q = malloc(l);
        if (!q) { ret = -1; break; }
        snprintf(q, l, "%s/%s", path, names[i]);
        ret = kc_collect(F, q, depth + 1); free(q);
    }
    for (size_t i = 0; i < nn; i++) free(names[i]);
    free(names);
    return ret;
}
static void kc_free(kc_files *F) { for (size_t i = 0; i < F->n; i++) free(F->path[i]); free(F->path); free(F->size); }

static double kc_size_arg(const char *s) {
    char *e; double v = strtod(s, &e);
    if (*e == 'k' || *e == 'K') v *= 1024; else if (*e == 'm' || *e == 'M') v *= 1048576; else if (*e == 'g' || *e == 'G') v *= 1073741824.0;
    return v;
}

static kolmo_profile *kc_load_profile(const char *path) {
    size_t n; uint8_t *b = slurp(path, &n); kolmo_profile *P = NULL;
    int r = kolmo_profile_load(b, n, &P); free(b);
    if (r) { fprintf(stderr, "kolmo: %s: %s\n", path, kolmo_strerror(r)); return NULL; }
    return P;
}

/* kolmo train [-m mode] [-l level] [-n|-q|-Q] [--dict-size N] [--no-dict] [--max-bytes N] [--target T] -o out.kpf FILES|DIRS... */
static int kolmo_cli_train(int argc, char **argv) {
    kolmo_train_params tp = {{KOLMO_FAST, 0, 0, 0, 1}, 0, 0, NULL};
    const char *outp = NULL; double maxb = 256.0 * 1048576; int speed = 0; kc_files F = {0};
    for (int a = 1; a < argc; a++) {
        const char *s = argv[a];
        if (!strcmp(s, "-o") && a + 1 < argc) outp = argv[++a];
        else if (!strcmp(s, "-m") && a + 1 < argc) {
            const char *m = argv[++a]; speed = !strcmp(m, "speed");
            tp.params.mode = !strcmp(m, "speed") || !strcmp(m, "rt") ? KOLMO_RT : !strcmp(m, "balanced") || !strcmp(m, "fast") ? KOLMO_FAST :
                             !strcmp(m, "ratio") || !strcmp(m, "max") ? KOLMO_MAX : !strcmp(m, "eco") ? KOLMO_ECO : !strcmp(m, "hc") ? KOLMO_HC :
                             !strcmp(m, "ultra") ? KOLMO_ULTRA : (kolmo_mode)-1;
            if ((int)tp.params.mode < 0) { fprintf(stderr, "kolmo train: unknown mode '%s'\n", m); kc_free(&F); return 2; }
        }
        else if (!strcmp(s, "-x")) tp.params.mode = KOLMO_ULTRA;
        else if (!strcmp(s, "-l") && a + 1 < argc) tp.params.level = atoi(argv[++a]);
        else if (!strcmp(s, "-n")) tp.params.transforms = 0;
        else if (!strcmp(s, "-q")) tp.params.transforms = 2;
        else if (!strcmp(s, "-Q")) tp.params.transforms = 3;
        else if (!strcmp(s, "--dict-size") && a + 1 < argc) tp.dict_size = (size_t)kc_size_arg(argv[++a]);
        else if (!strcmp(s, "--no-dict")) tp.no_dict = 1;
        else if (!strcmp(s, "--max-bytes") && a + 1 < argc) maxb = kc_size_arg(argv[++a]);
        else if (!strcmp(s, "--target") && a + 1 < argc) tp.target = argv[++a];
        else if (s[0] == '-' && s[1]) { fprintf(stderr, "kolmo train: unknown option '%s'\n", s); kc_free(&F); return 2; }
        else if (kc_collect(&F, s, 0)) { kc_free(&F); return 1; }
    }
    if (speed && tp.params.transforms == 1) tp.params.transforms = 3;
    if (!outp || !F.n) {
        fprintf(stderr, "usage: kolmo train [-m speed|balanced|ratio|...] [-l level] [--target T] [--dict-size N] [--no-dict]\n"
                        "                   [--max-bytes N] -o profile.kpf FILES|DIRS...\n");
        kc_free(&F); return 2;
    }
    /* more than --max-bytes of samples: every k-th file, so the sample spans the whole set */
    size_t tot = 0; for (size_t i = 0; i < F.n; i++) tot += F.size[i];
    size_t step = maxb > 0 && tot > maxb ? (size_t)(tot / maxb) + 1 : 1, use = 0, bytes = 0;
    for (size_t i = 0; i < F.n; i += step) { use++; bytes += F.size[i]; }
    uint8_t *buf = malloc(bytes ? bytes : 1); size_t *sz = malloc(use * sizeof(size_t)), k = 0, off = 0;
    if (!buf || !sz) { fprintf(stderr, "kolmo train: out of memory\n"); free(buf); free(sz); kc_free(&F); return 1; }
    for (size_t i = 0; i < F.n; i += step) {
        FILE *f = fopen(F.path[i], "rb"); if (!f) { perror(F.path[i]); free(buf); free(sz); kc_free(&F); return 1; }
        size_t got = fread(buf + off, 1, F.size[i], f); fclose(f);
        sz[k++] = got; off += got;
    }
    size_t cap = kolmo_profile_bound(&tp); uint8_t *out = malloc(cap);
    int64_t r = out ? kolmo_profile_train(buf, sz, k, &tp, out, cap) : KOLMO_ERR_ALLOC;
    free(buf); free(sz);
    if (r < 0) { fprintf(stderr, "kolmo train: %s\n", kolmo_strerror(r)); free(out); kc_free(&F); return 1; }
    int failed = spit(outp, out, (size_t)r);
    if (!failed) {
        kolmo_profile *P = NULL;
        if (!kolmo_profile_load(out, (size_t)r, &P)) {
            char d[8192]; kolmo_profile_describe(P, d, sizeof d);
            fprintf(stderr, "kolmo: wrote %lld-byte profile from %zu file%s (%zu bytes) to %s\n%s", (long long)r, k, k == 1 ? "" : "s", off, outp, d);
            kolmo_profile_free(P);
        }
    }
    free(out); kc_free(&F);
    return failed != 0;
}

static char *kc_default_calibration_path(void) {
    const char *home = getenv("HOME"); if (!home || !*home) return NULL;
    size_t l = strlen(home) + 40; char *p = malloc(l);
    if (p) snprintf(p, l, "%s/.config/kolmo/calibration", home);
    return p;
}

/* kolmo calibrate [-o FILE] [-s seconds] [FILES|DIRS...] */
static int kolmo_cli_calibrate(int argc, char **argv) {
    const char *outp = NULL; double secs = 0.2; kc_files F = {0};
    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "-o") && a + 1 < argc) outp = argv[++a];
        else if (!strcmp(argv[a], "-s") && a + 1 < argc) secs = atof(argv[++a]);
        else if (argv[a][0] == '-') { fprintf(stderr, "usage: kolmo calibrate [-o FILE] [-s seconds] [FILES|DIRS...]\n"); kc_free(&F); return 2; }
        else if (kc_collect(&F, argv[a], 0)) { kc_free(&F); return 1; }
    }
    uint8_t **bufs = calloc(F.n ? F.n : 1, sizeof *bufs); size_t *sz = calloc(F.n ? F.n : 1, sizeof *sz);
    if (!bufs || !sz) { free(bufs); free(sz); kc_free(&F); return 1; }
    for (size_t i = 0; i < F.n; i++) bufs[i] = slurp(F.path[i], &sz[i]);
    size_t cap = 1 << 16; char *out = malloc(cap);
    fprintf(stderr, "kolmo: calibrating 9 settings x 6 classes on one thread (about a minute)...\n");
    int64_t r = out ? kolmo_calibrate((const uint8_t *const *)bufs, sz, F.n, secs, out, cap) : KOLMO_ERR_ALLOC;
    for (size_t i = 0; i < F.n; i++) free(bufs[i]);
    free(bufs); free(sz); kc_free(&F);
    if (r < 0) { fprintf(stderr, "kolmo calibrate: %s\n", kolmo_strerror(r)); free(out); return 1; }
    char *def = NULL;
    if (!outp) {
        def = kc_default_calibration_path();
        if (!def) { fwrite(out, 1, (size_t)r, stdout); free(out); return 0; }
        char dir[4096]; snprintf(dir, sizeof dir, "%s", def);
        for (char *q = dir + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(dir, 0755); *q = '/'; }
        outp = def;
    }
    int failed = spit(outp, (const uint8_t *)out, (size_t)r);
    if (!failed) fprintf(stderr, "kolmo: wrote calibration table to %s\n%.*s", outp, (int)r, out);
    free(out); free(def);
    return failed != 0;
}

/* the table --target uses: --calib FILE, else $KOLMO_CALIBRATION, else ~/.config/kolmo/calibration if present */
static int kc_apply_calibration(kolmo_target *t, const char *path) {
    char *def = NULL; int explicit = path != NULL;
    if (!path) { path = getenv("KOLMO_CALIBRATION"); explicit = path != NULL; }
    if (!path) { def = kc_default_calibration_path(); path = def; }
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) { free(def); if (explicit) { perror(path); return -1; } return 0; }
    size_t n; uint8_t *b = read_input(f, &n);
    int r = kolmo_target_set_calibration(t, (const char *)b, n);
    free(b);
    if (r) fprintf(stderr, "kolmo: %s: %s\n", path, kolmo_strerror(r));
    free(def);
    return r ? -1 : 0;
}
