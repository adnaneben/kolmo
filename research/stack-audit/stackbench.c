/* stackbench: research tool (research/STACK-AUDIT.md section 6), not a Kolmo format. Measures "Kolmo stage -> kfam coder" stacks without any format
 * change: a file is compressed by the Kolmo library (rc2 + this branch, unchanged codec code), then decoded once with
 * two reporting hooks (a patched copy of the source in this scratch directory): every graph leaf (raw bytes, leaf
 * codec, coded size, the node that produced it) and every non-graph block after its transform (tbl, tbl2, lines,
 * log templates, word, ...) but before the inverse transform, with the block codec's coded size. Each captured stream
 * is then coded by kfam's byte model (orders 0-3 + one side context) and binary arithmetic coder, decoded back and
 * compared. For integer residual leaves with byte planes the side context is also tried as the plane index and as the
 * byte at the same position in the previous plane.
 *
 * Output per file: Kolmo frame size; per stream class, Kolmo coded bytes vs kfam bytes; the stacked estimate
 * (frame - replaced leaf payloads + kfam payloads, choosing per stream the smaller, i.e. what a new leaf id would
 * allow); kfam encode+decode time for all streams on 1 thread and on T threads (streams in parallel).
 *
 *   stackbench -m fast|eco|hc|max|ultra [-l level] [-T threads] [--whole] [--typed-huf|--native-leaf] file...
 *   (-T: kfam codes the captured streams on that many threads; kfam_v0_s_1t is the sum of per-stream times; --whole:
 *   also kfam alone on the whole input). Build: research/stack-audit/build.sh OUT. */
#include "kolmo.c"
#include <pthread.h>
size_t kfapi_enc(const uint8_t *d, size_t n, int mode, size_t m, uint8_t **out);
int kfapi_dec(const uint8_t *in, size_t inn, size_t rawn, int mode, size_t m, uint8_t *dst);

enum { NCLS = 24 };
static const char *cls_name[NCLS] = {"leaf.INT.res", "leaf.INT.rest", "leaf.DEC64", "leaf.FLOAT", "leaf.TABLE", "leaf.IMG8", "leaf.other",
    "blk.plain", "blk.tbl", "blk.tbl2", "blk.log", "blk.lines", "blk.word", "blk.json", "blk.rec", "blk.img", "blk.x86", "blk.other"};
typedef struct { int cls; uint8_t *d; size_t n, zl, m; int planes, cc; size_t kz[3]; int ok; double t; } item;
static item *IT; static size_t NIT, CIT;
static void push(int cls, const uint8_t *d, size_t n, size_t zl, size_t m, int planes, int cc) {
    if (NIT == CIT) { CIT = CIT ? 2 * CIT : 64; IT = realloc(IT, CIT * sizeof *IT); }
    item *t = &IT[NIT++]; memset(t, 0, sizeof *t); t->cls = cls; t->n = n; t->zl = zl; t->m = m; t->planes = planes; t->cc = cc;
    t->d = malloc(n ? n : 1); memcpy(t->d, d, n);
}
static void kstk_leaf(int op, int oi, int no, const uint8_t *par, size_t plen, int cc, const uint8_t *d, size_t rl, size_t zl) {
    (void)no; int cls = 6; size_t m = 0; int planes = 0;
    if (op == KG_INT) {
        cls = oi == 0 ? 0 : 1;
        if (oi == 0 && plen >= 3 && (par[1] >> 3 & 1) && par[2] > 1 && rl % par[2] == 0) { planes = par[2]; m = rl / par[2]; }
    } else if (op == KG_DEC64) cls = 2; else if (op == KG_FLOAT) cls = 3;
    else if (op == KG_FIELDS || op == KG_DECIMAL || op == KG_CONCAT) cls = 4; else if (op == KG_IMG8) cls = 5;
    push(cls, d, rl, zl, m, planes, cc);
}
static void kstk_block(int codec, int tflag, int pt, const uint8_t *d, size_t n, size_t zl) {
    (void)pt; int cls = tflag == 0 ? 7 : tflag == 3 ? 8 : tflag == 4 ? 9 : tflag == 5 ? 10 : tflag == 12 ? 11 : tflag == 9 ? 12
              : tflag == 10 ? 13 : tflag == 7 ? 14 : tflag == 6 ? 15 : tflag == 8 ? 16 : 17;
    push(cls, d, n, zl, 0, 0, codec);
}
static void code_item(item *t) {
    struct timespec a, b;
    t->ok = 1; uint8_t *chk = malloc(t->n ? t->n : 1);
    for (int v = 0; v < 3; v++) {                    /* t: encode + decode time of variant 0 (no side context) */
        if (v && !t->planes) { t->kz[v] = (size_t)-1; continue; }
        if (!v) clock_gettime(CLOCK_MONOTONIC, &a);
        uint8_t *o = NULL; size_t z = kfapi_enc(t->d, t->n, v, t->m, &o);
        if (!z || kfapi_dec(o, z, t->n, v, t->m, chk) || memcmp(chk, t->d, t->n)) t->ok = 0;
        if (!v) { clock_gettime(CLOCK_MONOTONIC, &b); t->t = (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9; }
        t->kz[v] = z; free(o);
    }
    free(chk);
}
static atomic_size_t NEXT;
static void *worker(void *u) { (void)u; for (;;) { size_t i = atomic_fetch_add(&NEXT, 1); if (i >= NIT) return NULL; code_item(&IT[i]); } }
static double run_all(int threads) {
    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); atomic_store(&NEXT, 0);
    pthread_t th[256]; int k = threads < 1 ? 1 : threads > 256 ? 256 : threads;
    for (int i = 1; i < k; i++) pthread_create(&th[i], NULL, worker, NULL);
    worker(NULL); for (int i = 1; i < k; i++) pthread_join(th[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &b); return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9;
}
static uint8_t *slurpf(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l ? (size_t)l : 1); if (fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
static double secs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1}; int threads = 0, a = 1, whole = 0; const char *mname = "fast";
    for (; a < argc && argv[a][0] == '-'; a++) {
        if (!strcmp(argv[a], "-m") && a + 1 < argc) { mname = argv[++a];
            p.mode = !strcmp(mname, "fast") ? KOLMO_FAST : !strcmp(mname, "eco") ? KOLMO_ECO : !strcmp(mname, "hc") ? KOLMO_HC
                   : !strcmp(mname, "max") ? KOLMO_MAX : !strcmp(mname, "ultra") ? KOLMO_ULTRA : KOLMO_RT; }
        else if (!strcmp(argv[a], "-T") && a + 1 < argc) threads = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-l") && a + 1 < argc) p.level = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--whole")) whole = 1;          /* also: kfam alone on the whole input (one stream) */
        else if (!strcmp(argv[a], "--typed-huf")) p.transforms |= KOLMO_T_TYPED_HUF;
        else if (!strcmp(argv[a], "--native-leaf")) p.transforms |= KOLMO_T_NATIVE_LEAF;
        else { fprintf(stderr, "unknown option %s\n", argv[a]); return 2; }
    }
    if (threads <= 0) threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    for (; a < argc; a++) {
        size_t n; uint8_t *s = slurpf(argv[a], &n); if (!s) { fprintf(stderr, "read %s\n", argv[a]); continue; }
        size_t cap = kolmo_bound(n, &p); uint8_t *f = malloc(cap), *o = malloc(n + 1);
        double t0 = secs(); int64_t z = kolmo_compress(s, n, f, cap, &p); double t1 = secs();
        NIT = 0; int64_t r = kolmo_decompress(f, (size_t)z, o, n, 1); double t2 = secs();
        if (z <= 0 || r != (int64_t)n || memcmp(o, s, n)) { fprintf(stderr, "kolmo round trip failed %s\n", argv[a]); return 1; }
        double tT = run_all(threads), t1t = 0; for (size_t i = 0; i < NIT; i++) t1t += IT[i].t;   /* sum of per-stream times = one thread */
        size_t kz[NCLS] = {0}, kb[NCLS] = {0}, raw[NCLS] = {0}, cnt[NCLS] = {0}, ok = 1, capt = 0, best = 0, always = 0;
        size_t v0sum = 0, v1sum = 0, v2sum = 0, plz = 0;
        for (size_t i = 0; i < NIT; i++) {
            item *t = &IT[i]; size_t b = t->kz[0]; if (t->kz[1] < b) b = t->kz[1]; if (t->kz[2] < b) b = t->kz[2];
            ok &= (size_t)t->ok; capt += t->zl; best += b < t->zl ? b : t->zl; always += b;
            kz[t->cls] += t->zl; kb[t->cls] += b; raw[t->cls] += t->n; cnt[t->cls]++;
            if (t->planes) { v0sum += t->kz[0]; v1sum += t->kz[1]; v2sum += t->kz[2]; plz += t->zl; }
        }
        printf("{\"file\":\"%s\",\"mode\":\"%s\",\"level\":%d,\"orig\":%zu,\"kolmo\":%lld,\"kolmo_enc_s\":%.3f,\"kolmo_dec_s\":%.3f,\"streams\":%zu,"
               "\"captured_coded\":%zu,\"stack_best\":%lld,\"stack_kfam_always\":%lld,\"kfam_v0_s_1t\":%.3f,\"wall_all_variants_%dt\":%.3f,\"verified\":%zu",
               argv[a], mname, p.level, n, (long long)z, t1 - t0, t2 - t1, NIT, capt, (long long)(z - (int64_t)capt + (int64_t)best),
               (long long)(z - (int64_t)capt + (int64_t)always), t1t, threads, tT, ok);
        if (whole) { uint8_t *wo = NULL; double w0 = secs(); size_t wz = kfapi_enc(s, n, 0, 0, &wo); double w1 = secs();
                     int wok = wz && !kfapi_dec(wo, wz, n, 0, 0, o) && !memcmp(o, s, n); double w2 = secs(); free(wo);
                     printf(",\"kfam_whole\":%zu,\"kfam_whole_enc_s\":%.3f,\"kfam_whole_dec_s\":%.3f,\"kfam_whole_ok\":%d", wz, w1 - w0, w2 - w1, wok); }
        if (plz) printf(",\"int_planes\":{\"kolmo\":%zu,\"kfam_none\":%zu,\"kfam_plane_index\":%zu,\"kfam_cross_plane\":%zu}", plz, v0sum, v1sum, v2sum);
        printf(",\"classes\":{"); int first = 1;
        for (int c = 0; c < NCLS; c++) if (cnt[c]) { printf("%s\"%s\":[%zu,%zu,%zu,%zu]", first ? "" : ",", cls_name[c], cnt[c], raw[c], kz[c], kb[c]); first = 0; }
        printf("}}\n"); fflush(stdout);
        for (size_t i = 0; i < NIT; i++) free(IT[i].d);
        free(s); free(f); free(o);
    }
    return 0;
}
