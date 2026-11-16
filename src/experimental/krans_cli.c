/* krans: command-line tool for the KRANS research coder (see krans.h, KRANS.md).
 *
 *   krans [options] in out         compress
 *   krans -d in out                decompress
 *   krans -b [options] files...    benchmark: one JSON line per file (sizes, MB/s per decoder, fastest of -r runs)
 *   krans -P file prefix           write prefix.lo / prefix.hi: byte planes of the 16-bit LE word deltas
 *
 * options: -o 0|1|a (order; a = choose per block), -w 8|16|32 (ways), -p 0|10..14 (precision, 0 = choose),
 *          -B KiB (block), -T n (threads; in -b mode adds multithreaded encode/decode), -D auto|scalar|avx2|neon,
 *          -r runs (default 3, minimum 3 in -b mode) */
#define _POSIX_C_SOURCE 200809L
#include "krans.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static uint8_t *readf(const char *fn, size_t *n) {
    FILE *f = fopen(fn, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l > 0 ? (size_t)l : 1);
    if (!b || (l > 0 && fread(b, 1, (size_t)l, f) != (size_t)l)) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)l; return b;
}
static int writef(const char *fn, const void *b, size_t n) {
    FILE *f = fopen(fn, "wb"); if (!f) return -1;
    int r = fwrite(b, 1, n, f) == n ? 0 : -1;
    return fclose(f) || r ? -1 : 0;
}
static void usage(void) {
    fprintf(stderr, "usage: krans [-o 0|1|a] [-w 8|16|32] [-p 0|10..14] [-B KiB] [-T n] [-D auto|scalar|avx2|neon] in out\n"
                    "       krans -d [-T n] [-D ...] in out\n       krans -b [options] [-r runs] files...\n       krans -P file prefix\n");
    exit(2);
}
static int dec_of(const char *s) {
    if (!strcmp(s, "auto")) return KRANS_DEC_AUTO;
    if (!strcmp(s, "scalar")) return KRANS_DEC_SCALAR;
    if (!strcmp(s, "avx2")) return KRANS_DEC_AVX2;
    if (!strcmp(s, "neon")) return KRANS_DEC_NEON;
    return -1;
}

static int planes(const char *fn, const char *prefix) {
    size_t n; uint8_t *b = readf(fn, &n); if (!b) { perror(fn); return 1; }
    size_t m = n / 2; uint8_t *lo = malloc(m ? m : 1), *hi = malloc(m ? m : 1); uint16_t prev = 0;
    for (size_t i = 0; i < m; i++) {
        uint16_t v = (uint16_t)(b[2 * i] | b[2 * i + 1] << 8), d = (uint16_t)(v - prev); prev = v;
        lo[i] = (uint8_t)d; hi[i] = (uint8_t)(d >> 8);
    }
    char p1[4096], p2[4096];
    snprintf(p1, sizeof p1, "%s.lo", prefix); snprintf(p2, sizeof p2, "%s.hi", prefix);
    int r = writef(p1, lo, m) || writef(p2, hi, m);
    free(b); free(lo); free(hi);
    return r;
}

static void bench(int argc, char **argv, int first, krans_params *p, int runs, int mt) {
    for (int a = first; a < argc; a++) {
        size_t n; uint8_t *in = readf(argv[a], &n);
        if (!in) { perror(argv[a]); continue; }
        size_t cap = krans_bound(n, p->block); uint8_t *c = malloc(cap), *d = malloc(n ? n : 1);
        krans_stats st; krans_params q = *p; q.stats = &st; q.threads = 1;
        size_t z = 0; double te = 1e30, td[4] = {1e30, 1e30, 1e30, 1e30}, tem = 1e30, tdm = 1e30;
        for (int r = 0; r < runs; r++) { double t = now(); z = krans_compress(in, n, c, cap, &q); t = now() - t; if (t < te) te = t; }
        if (!z) { fprintf(stderr, "%s: compress failed\n", argv[a]); free(in); free(c); free(d); continue; }
        int ok = 1;
        for (int k = KRANS_DEC_SCALAR; k <= KRANS_DEC_NEON; k++) {
            if (!krans_decoder_available(k)) continue;
            krans_set_decoder(k);
            for (int r = 0; r < runs; r++) {
                memset(d, 0, n);
                double t = now(); int e = krans_decompress(c, z, d, n, 1); t = now() - t;
                if (e || memcmp(in, d, n)) { ok = 0; break; }
                if (t < td[k]) td[k] = t;
            }
        }
        krans_set_decoder(KRANS_DEC_AUTO);
        if (mt > 1) {
            q.threads = mt; q.stats = NULL;
            for (int r = 0; r < runs; r++) { double t = now(); size_t z2 = krans_compress(in, n, c, cap, &q); t = now() - t; if (z2 != z) ok = 0; if (t < tem) tem = t; }
            for (int r = 0; r < runs; r++) {
                double t = now(); int e = krans_decompress(c, z, d, n, mt); t = now() - t;
                if (e || memcmp(in, d, n)) ok = 0;
                if (t < tdm) tdm = t;
            }
        }
        const char *nm = strrchr(argv[a], '/'); nm = nm ? nm + 1 : argv[a];
        double mb = n / 1e6;
        printf("{\"file\":\"%s\",\"size\":%zu,\"csize\":%zu,\"ratio\":%.4f,\"bits_per_byte\":%.4f,"
               "\"order\":\"%s\",\"ways\":%d,\"prec\":%d,\"block\":%u,\"enc_mbs\":%.1f,\"dec_mbs\":{",
               nm, n, z, n ? (double)z / n : 0, n ? 8.0 * z / n : 0, p->order == 2 ? "auto" : p->order ? "1" : "0",
               p->ways, p->prec, p->block, te > 0 ? mb / te : 0);
        int comma = 0;
        for (int k = KRANS_DEC_SCALAR; k <= KRANS_DEC_NEON; k++)
            if (td[k] < 1e29) { printf("%s\"%s\":%.1f", comma ? "," : "", krans_decoder_name(k), td[k] > 0 ? mb / td[k] : 0); comma = 1; }
        printf("},\"blocks\":{\"stored\":%llu,\"rle\":%llu,\"o0\":%llu,\"o1\":%llu},\"table_bytes\":%llu,\"o1_tables\":%llu,\"prec_blocks\":{",
               (unsigned long long)st.stored, (unsigned long long)st.rle, (unsigned long long)st.o0, (unsigned long long)st.o1,
               (unsigned long long)st.table_bytes, (unsigned long long)st.o1_tables);
        comma = 0;
        for (int k = 10; k <= 14; k++) if (st.prec_hist[k]) { printf("%s\"%d\":%llu", comma ? "," : "", k, (unsigned long long)st.prec_hist[k]); comma = 1; }
        printf("}");
        if (mt > 1) printf(",\"threads\":%d,\"enc_mbs_mt\":%.1f,\"dec_mbs_mt\":%.1f", mt, mb / tem, mb / tdm);
        printf(",\"verified\":%s}\n", ok ? "true" : "false");
        fflush(stdout);
        free(in); free(c); free(d);
    }
}

int main(int argc, char **argv) {
    krans_params p; krans_default_params(&p);
    int dmode = 0, bmode = 0, runs = 3, threads = 1, ai;
    for (ai = 1; ai < argc && argv[ai][0] == '-' && argv[ai][1]; ai++) {
        const char *o = argv[ai];
        const char *v = ai + 1 < argc ? argv[ai + 1] : NULL;
        if (!strcmp(o, "-d")) dmode = 1;
        else if (!strcmp(o, "-b")) bmode = 1;
        else if (!strcmp(o, "-P")) { if (ai + 2 >= argc) usage(); return planes(argv[ai + 1], argv[ai + 2]); }
        else if (!v) usage();
        else if (!strcmp(o, "-o")) { p.order = !strcmp(v, "a") ? 2 : atoi(v); ai++; }
        else if (!strcmp(o, "-w")) { p.ways = atoi(v); ai++; }
        else if (!strcmp(o, "-p")) { p.prec = atoi(v); ai++; }
        else if (!strcmp(o, "-B")) { p.block = (uint32_t)atoi(v) << 10; ai++; }
        else if (!strcmp(o, "-T")) { threads = atoi(v); ai++; }
        else if (!strcmp(o, "-r")) { runs = atoi(v); ai++; }
        else if (!strcmp(o, "-D")) { int k = dec_of(v); if (k < 0 || krans_set_decoder(k)) { fprintf(stderr, "decoder %s not available\n", v); return 2; } ai++; }
        else usage();
    }
    if (bmode) {
        if (ai >= argc) usage();
        bench(argc, argv, ai, &p, runs < 3 ? 3 : runs, threads);
        return 0;
    }
    if (argc - ai != 2) usage();
    size_t n; uint8_t *in = readf(argv[ai], &n);
    if (!in) { perror(argv[ai]); return 1; }
    if (dmode) {
        uint64_t m;
        if (krans_frame_size(in, n, &m) || m > ((uint64_t)1 << 40)) { fprintf(stderr, "not a krans frame\n"); return 1; }
        uint8_t *out = malloc(m ? (size_t)m : 1);
        if (!out || krans_decompress(in, n, out, (size_t)m, threads)) { fprintf(stderr, "decode failed\n"); return 1; }
        if (writef(argv[ai + 1], out, (size_t)m)) { perror(argv[ai + 1]); return 1; }
        free(out);
    } else {
        p.threads = threads;
        size_t cap = krans_bound(n, p.block); uint8_t *out = malloc(cap);
        size_t z = out ? krans_compress(in, n, out, cap, &p) : 0;
        if (!z) { fprintf(stderr, "compress failed (check -w/-p/-B/-o)\n"); return 1; }
        uint8_t *chk = malloc(n ? n : 1);                      /* verify before writing */
        if (!chk || krans_decompress(out, z, chk, n, threads) || memcmp(chk, in, n)) { fprintf(stderr, "verify failed\n"); return 1; }
        if (writef(argv[ai + 1], out, z)) { perror(argv[ai + 1]); return 1; }
        fprintf(stderr, "%zu -> %zu (%.3f bits/byte)\n", n, z, n ? 8.0 * z / n : 0);
        free(chk); free(out);
    }
    free(in);
    return 0;
}
