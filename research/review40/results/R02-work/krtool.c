/* R02 experiment tool (not part of Kolmo): runs src/algo/krans.c on a raw file or on the leaves of a leaf dump
 * (.lv: records of 6 x uint64 {magic, chosen codec, level<<32|..., raw n, coded z, planes} + n bytes).
 * usage: krtool FILE [lv=MINLEAF] [stride=N] [step=N] [effort=N] [chunk=N] [maxt=N] [force=ls,P,mb,eq,bq,mk,pf,lag...] [reps=N] */
#include "algo/krans.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int run(const uint8_t *d, size_t n, const kolmo_krans_params *pp, int reps, const char *tag, uint64_t chosen) {
    size_t cap = kolmo_krans_bound(n); uint8_t *z = malloc(cap), *r = malloc(n + 1);
    double te = 1e9, td = 1e9; size_t zn = 0;
    for (int q = 0; q < reps; q++) { double t0 = now(); zn = kolmo_krans_compress(d, n, z, cap, pp); double t1 = now(); if (t1 - t0 < te) te = t1 - t0; }
    if (!zn) { printf("%s: compress failed\n", tag); return 1; }
    int rc = 0;
    for (int q = 0; q < reps; q++) { double t0 = now(); rc = kolmo_krans_decompress(z, zn, r, n); double t1 = now(); if (t1 - t0 < td) td = t1 - t0; }
    int ok = !rc && !memcmp(d, r, n);
    printf("%s n %zu krans %zu", tag, n, zn);
    if (chosen) printf(" chosen %llu (%+.2f%%)", (unsigned long long)chosen, 100.0 * ((double)zn / (double)chosen - 1));
    printf(" enc %.1f MB/s dec %.1f MB/s %s\n", n / te / 1e6, n / td / 1e6, ok ? "RT_OK" : "RT_FAIL");
    free(z); free(r); return !ok;
}
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    kolmo_krans_params pp = {0}; size_t lv = 0; int reps = 1, islv = 0;
    for (int a = 2; a < argc; a++) {
        if (!strncmp(argv[a], "lv=", 3)) { lv = strtoull(argv[a] + 3, 0, 10); islv = 1; }
        else if (!strncmp(argv[a], "stride=", 7)) pp.stride = (uint32_t)atoi(argv[a] + 7);
        else if (!strncmp(argv[a], "step=", 5)) pp.step = (uint8_t)atoi(argv[a] + 5);
        else if (!strncmp(argv[a], "effort=", 7)) pp.effort = (uint8_t)atoi(argv[a] + 7);
        else if (!strncmp(argv[a], "chunk=", 6)) pp.chunk = (uint32_t)atoi(argv[a] + 6);
        else if (!strncmp(argv[a], "maxt=", 5)) pp.max_tables = (uint8_t)atoi(argv[a] + 5);
        else if (!strncmp(argv[a], "reps=", 5)) reps = atoi(argv[a] + 5);
        else if (!strncmp(argv[a], "force=", 6)) {
            unsigned v[12] = {0}; int k = 0; for (char *s = argv[a] + 6; *s && k < 12;) { v[k++] = (unsigned)strtoul(s, &s, 10); if (*s == ',') s++; }
            pp.force = 1; pp.ls = v[0]; pp.P = v[1]; pp.mb = v[2]; pp.eq = v[3]; pp.bq = v[4]; pp.mk = v[5]; pp.pf = v[6];
            pp.nlag = (uint8_t)(k > 7 ? k - 7 : 0); for (int q = 0; q < pp.nlag; q++) pp.lag[q] = v[7 + q];
        }
    }
    FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)sz + 1); if (fread(d, 1, (size_t)sz, f) != (size_t)sz) return 2;
    fclose(f);
    int bad = 0;
    if (argc > 2 && !strcmp(argv[2], "t6")) {      /* a T_IMG transformed block: header 12, skip, lo plane, hi plane, tail -> high/low byte pairs */
        uint32_t W, N; memcpy(&W, d + 4, 4); memcpy(&N, d + 8, 4); const uint8_t *lo = d + 12 + d[1], *hi = lo + N;
        uint8_t *w = malloc(2 * (size_t)N + 1); for (uint32_t i = 0; i < N; i++) { w[2 * i] = hi[i]; w[2 * i + 1] = lo[i]; }
        if (pp.force && pp.nlag == 0 && pp.eq) { pp.nlag = 4; pp.lag[0] = 2; pp.lag[1] = 2 * W; pp.lag[2] = 2 * W + 2; pp.lag[3] = 2 * W - 2; }
        if (!pp.force) { pp.stride = 2 * W; pp.step = 2; }
        if (!pp.chunk) pp.chunk = 2 * N;
        bad = run(w, 2 * (size_t)N, &pp, reps, "t6", 0); free(w); free(d); return bad;
    }
    if (!islv) { bad = run(d, (size_t)sz, &pp, reps, argv[1], 0); free(d); return bad; }
    for (size_t i = 0; i + 48 <= (size_t)sz;) {
        uint64_t h[6]; memcpy(h, d + i, 48); i += 48;
        if (h[3] >= lv && (h[2] >> 32) == 3) { char tag[64]; snprintf(tag, sizeof tag, "leaf cc=%llu", (unsigned long long)h[1]); bad |= run(d + i, (size_t)h[3], &pp, reps, tag, h[4]); }
        i += h[3];
    }
    free(d); return bad;
}
