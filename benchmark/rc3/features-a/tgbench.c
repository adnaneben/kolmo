/* tgbench: how well --target meets its targets.
 *
 *   tgbench [-r rounds] [-x maxbytes] [-B chunkMiB] [-c calibration] [-F] FILE TARGET...
 * (-c: a `kolmo calibrate` table instead of the built-in one; -F: skip the fixed settings)
 *
 * For the file: every fixed setting (the nine --target chooses from; -x and -x -l 1 only for files up to maxbytes,
 * default 3 MB, as they run at 0.1-0.3 MB/s here) is measured on one thread, in memory, round trip checked: size,
 * compress and decompress seconds (fastest of `rounds`, one run for the slow settings). Then, for each target, the
 * whole kolmo_compress_target call (trials included) and its decode are measured the same way. Targets understood
 * here: decode>=R, encode>=R (MB/s), cost:cloud[=months], cost:link=<Mbit>, size. Output: one JSON object per line:
 *   {"file":..., "kind":"fixed", "setting":..., "size":..., "enc_s":..., "dec_s":...}
 *   {"file":..., "kind":"target", "target":..., "size":..., "enc_s":..., "dec_s":..., "predicted_size":...,
 *    "predicted_enc_s":..., "predicted_dec_s":..., "chosen":"..."}
 * The comparison with the best fixed setting (the oracle) is made by summarize_target.py. */
#define _GNU_SOURCE
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static const struct { const char *name; kolmo_mode mode; int level, tr; } SET[9] = {
    {"speed", KOLMO_RT, 0, 3}, {"balanced-n", KOLMO_FAST, 0, 0}, {"balanced", KOLMO_FAST, 0, 1}, {"eco", KOLMO_ECO, 0, 1},
    {"hc", KOLMO_HC, 0, 1}, {"ratio-l1", KOLMO_MAX, 1, 1}, {"ratio", KOLMO_MAX, 0, 1}, {"x-l1", KOLMO_ULTRA, 1, 1},
    {"x", KOLMO_ULTRA, 0, 1}};

int main(int argc, char **argv) {
    int rounds = 3, opt, nofixed = 0; double maxx = 3e6; uint32_t chunk = 0; char *calib = NULL; size_t calibn = 0; const char *calname = "built-in";
    while ((opt = getopt(argc, argv, "r:x:B:c:F")) != -1) switch (opt) {
        case 'r': rounds = atoi(optarg); break; case 'x': maxx = atof(optarg); break; case 'B': chunk = (uint32_t)(atof(optarg) * 1048576); break;
        case 'F': nofixed = 1; break;
        case 'c': { FILE *cf = fopen(optarg, "rb"); if (!cf) { perror(optarg); return 1; } calib = malloc(1 << 16);
                    calibn = fread(calib, 1, 1 << 16, cf); fclose(cf); calname = optarg; break; }
        default: return 2; }
    if (argc - optind < 1) { fprintf(stderr, "usage: tgbench [-r rounds] [-x maxbytes] FILE TARGET...\n"); return 2; }
    const char *path = argv[optind];
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *in = malloc(n ? n : 1); if (fread(in, 1, n, f) != (size_t)n) return 1; fclose(f);
    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    kolmo_params big = {KOLMO_MAX, 0, 1, 0, 1};
    size_t cap = kolmo_bound(n, &big) + (1u << 20); uint8_t *o = malloc(cap), *d = malloc(n ? n : 1);
    for (int s = 0; s < 9 && !nofixed; s++) {
        if (s >= 7 && n > maxx) continue;
        kolmo_params p = {SET[s].mode, SET[s].level, 1, 0, SET[s].tr};
        double be = 1e30, bd = 1e30; int64_t z = 0; int rr = s >= 5 ? 1 : rounds;
        for (int r = 0; r < rr; r++) {
            double t0 = now(); z = kolmo_compress(in, n, o, cap, &p); double t1 = now();
            int64_t u = kolmo_decompress(o, z, d, n, 1); double t2 = now();
            if (z < 0 || u != n || memcmp(d, in, n)) { fprintf(stderr, "%s: %s failed\n", base, SET[s].name); return 1; }
            if (t1 - t0 < be) be = t1 - t0;
            if (t2 - t1 < bd) bd = t2 - t1;
        }
        printf("{\"file\":\"%s\",\"bytes\":%ld,\"kind\":\"fixed\",\"setting\":\"%s\",\"size\":%lld,\"enc_s\":%.6f,\"dec_s\":%.6f}\n", base, n, SET[s].name, (long long)z, be, bd);
        fflush(stdout);
    }
    for (int a = optind + 1; a < argc; a++) {
        kolmo_target *t = NULL;
        if (kolmo_target_parse(argv[a], &t)) { fprintf(stderr, "bad target %s\n", argv[a]); return 2; }
        if (calib && kolmo_target_set_calibration(t, calib, calibn)) { fprintf(stderr, "bad calibration table\n"); return 2; }
        const char *cb = strrchr(calname, '/'); cb = cb ? cb + 1 : calname;
        kolmo_params p = {KOLMO_FAST, 0, 1, chunk, 1}; kolmo_target_report rep;
        double be = 1e30, bd = 1e30; int64_t z = 0;
        for (int r = 0; r < rounds; r++) {
            double t0 = now(); z = kolmo_compress_target(in, n, o, cap, &p, t, NULL, &rep); double t1 = now();
            int64_t u = kolmo_decompress(o, z, d, n, 1); double t2 = now();
            if (z < 0 || u != n || memcmp(d, in, n)) { fprintf(stderr, "%s: target %s failed (%lld)\n", base, argv[a], (long long)z); return 1; }
            if (t1 - t0 < be) be = t1 - t0;
            if (t2 - t1 < bd) bd = t2 - t1;
            if (t1 - t0 > 20) break;                          /* slow choices: one run */
        }
        char chosen[256] = ""; size_t w = 0;
        for (int s = 0; s < 9; s++) if (rep.per_setting[s]) w += (size_t)snprintf(chosen + w, sizeof chosen - w, "%s%s x%u", w ? ", " : "", kolmo_target_setting_name(s), rep.per_setting[s]);
        printf("{\"file\":\"%s\",\"bytes\":%ld,\"kind\":\"target\",\"target\":\"%s\",\"size\":%lld,\"enc_s\":%.6f,\"dec_s\":%.6f,"
               "\"predicted_size\":%llu,\"predicted_enc_s\":%.6f,\"predicted_dec_s\":%.6f,\"chunks\":%u,\"chunk_bytes\":%u,\"chosen\":\"%s\",\"calibration\":\"%s\"}\n",
               base, n, argv[a], (long long)z, be, bd, (unsigned long long)rep.predicted_size, rep.predicted_enc_s, rep.predicted_dec_s, rep.chunks, chunk, chosen, cb);
        fflush(stdout);
        kolmo_target_free(t);
    }
    return 0;
}
