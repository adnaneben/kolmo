/* Research tool of the twopass lane: is a full-file search of the transform (plan) worth it?
 *   planoracle FILE MODE [THREADS]
 * MODE: speed balanced hc ratio x. Codes FILE with the mode's own analysis (sampled trials), then with every
 * candidate plan forced through the planner hook: the plan each other mode's analysis picks, the same analyses with
 * every trial slice set to the whole file (KOLMO_PLAN_HOOK's kolmo_plan_full), and no transform. Every candidate is
 * coded by the target mode's real coders on the whole file, so the smallest is the full-search oracle over these
 * plans. Prints one line per candidate and a summary; each frame is decoded and compared. */
#define KOLMO_PLAN_HOOK 1
#include "../../../src/kolmo.c"
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
static struct { int active, none, full; kolmo_params src; } cand;
static void hook(const uint8_t *s, size_t n, const kolmo_params *p, plan_t *pl) {
    if (!cand.active) return;
    free(pl->fs); free(pl->rec); free(pl->cmpz);
    if (cand.none) { *pl = (plan_t){0}; return; }
    kolmo_plan_full = cand.full;
    plan_t q = plan_for(s, n, &cand.src, 1, 0);
    kolmo_plan_full = 0;
    if (p->mode == KOLMO_RT && (q.t == T_TP || q.t == T_LOG)) q.t = T_NONE;
    if (p->mode == KOLMO_ULTRA && !ultra_keep_transform(s, n, &q, 1, p->level)) { free(q.fs); q.fs = NULL; q.t = T_NONE; }
    *pl = q;
}
static int sig_plan(const uint8_t *s, size_t n, kolmo_params src, int full, int *sig) {
    kolmo_plan_full = full; plan_t q = plan_for(s, n, &src, 1, 0); kolmo_plan_full = 0;
    sig[0] = q.t; sig[1] = q.a; sig[2] = q.b; sig[3] = q.c; sig[4] = (int)q.kgw;
    free(q.fs); free(q.rec); free(q.cmpz); return q.t;
}
int main(int argc, char **argv) {
    if (argc < 3) return 1;
    size_t n; uint8_t *s = slurp(argv[1], &n); if (!s) return 1;
    const char *ms = argv[2]; int threads = argc > 3 ? atoi(argv[3]) : 1;
    kolmo_mode M = !strcmp(ms, "speed") ? KOLMO_RT : !strcmp(ms, "balanced") ? KOLMO_FAST : !strcmp(ms, "hc") ? KOLMO_HC : !strcmp(ms, "ratio") ? KOLMO_MAX : KOLMO_ULTRA;
    kolmo_params P = {M, 0, threads, 0, 1};
    size_t cap = kolmo_bound(n, &P); uint8_t *o = malloc(cap), *d = malloc(n + 64);
    kolmo_plan_hook = hook;
    double t0 = now(); int64_t z0 = kolmo_compress(s, n, o, cap, &P); double te0 = now() - t0;
    t0 = now(); int64_t dz0 = z0 > 0 ? kolmo_decompress(o, z0, d, n, 1) : -1; double td0 = now() - t0;
    if (dz0 != (int64_t)n || memcmp(d, s, n)) { fprintf(stderr, "default failed\n"); return 2; }
    /* plan-only timing: the target mode's analysis with sampled and with full-file trials */
    int sg[5], sgf[5]; t0 = now(); sig_plan(s, n, P, 0, sg); double tp = now() - t0; t0 = now(); sig_plan(s, n, P, 1, sgf); double tpf = now() - t0;
    printf("%s %s default size=%lld enc=%.3fs dec=%.3fs plan=%.3fs plan_full=%.3fs plan_t=%d full_t=%d\n", argv[1], ms, (long long)z0, te0, td0, tp, tpf, sg[0], sgf[0]);
    static const kolmo_mode SM[] = {KOLMO_RT, KOLMO_FAST, KOLMO_HC, KOLMO_MAX, KOLMO_ULTRA}; static const char *SN[] = {"speed", "balanced", "hc", "ratio", "x"};
    int seen[24][5]; int ns = 0; int64_t best = z0; char bestn[64] = "default";
    for (int c = 0; c < 2 * 5 * 2 + 1; c++) {
        int none = c == 20, full = c >= 10 && c < 20, src = c % 5, quick = (c / 5) & 1;
        kolmo_params sp = {SM[src], 0, 1, 0, quick ? 2 : 1};
        int sig[5] = {0};
        if (!none) { sig_plan(s, n, sp, full, sig); }
        int dup = 0; for (int k = 0; k < ns; k++) if (!memcmp(seen[k], sig, sizeof sig)) dup = 1;
        if (dup) continue;
        memcpy(seen[ns++], sig, sizeof sig);
        cand.active = 1; cand.none = none; cand.full = full; cand.src = sp;
        t0 = now(); int64_t z = kolmo_compress(s, n, o, cap, &P); double te = now() - t0;
        cand.active = 0;
        t0 = now(); int64_t dz = z > 0 ? kolmo_decompress(o, z, d, n, 1) : -1; double td = now() - t0;
        if (dz != (int64_t)n || memcmp(d, s, n)) { printf("  candidate %d failed\n", c); continue; }
        char nm[64]; snprintf(nm, sizeof nm, "%s%s%s", none ? "none" : SN[src], quick ? "-quick" : "", full ? "-full" : "");
        printf("  cand %-18s t=%d a=%d b=%d c=%d size=%lld (%+.3f%%) enc=%.3fs dec=%.3fs\n", nm, sig[0], sig[1], sig[2], sig[3], (long long)z, 100.0 * ((double)z - z0) / z0, te, td);
        if (z < best) { best = z; snprintf(bestn, sizeof bestn, "%s", nm); }
    }
    printf("SUMMARY %s %s default=%lld oracle=%lld gain=%.3f%% best=%s candidates=%d\n", argv[1], ms, (long long)z0, (long long)best, 100.0 * ((double)z0 - best) / z0, bestn, ns);
    return 0;
}
