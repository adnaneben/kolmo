/* kolmo calibrate (and the retired kolmo train): the command-line side of --target (src/api/features.c).
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

/* kolmo train and kolmo profile: trained profiles were built on zstd and Brotli dictionaries and are not available in
 * this build (container version 12, Kolmo's own coders only). */
static int kolmo_cli_train(int argc, char **argv) {
    (void)argc; (void)argv;
    fprintf(stderr, "kolmo train: %s\n", kolmo_strerror(KOLMO_ERR_UNSUPPORTED)); return 2;
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
