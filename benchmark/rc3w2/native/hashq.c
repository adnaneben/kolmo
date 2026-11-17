/* hashq: speed and quality of khash64 against XXH3-64 (native lane, rc3 wave 2).
 * usage: hashq [TEXTFILE]    (hashq -s: speed only)
 * Speed: in-cache inputs of 16 B .. 64 KiB (seed of Kolmo's block checksums) and a 64 MiB buffer (memory bound).
 * Quality, for both hashes, on the full 64 bits and on the low 32 bits that Kolmo's block header stores:
 *   - avalanche: flip each input bit of random keys; worst |P(output bit flips) - 1/2| over all (input, output) bits;
 *   - collisions against the birthday expectation on structured key sets: counters in a 4 KiB zero block, all keys
 *     of one and two set bits in 256 zero bytes, every distinct line of TEXTFILE, zero blocks of every length 0..64 KiB;
 *   - corruption: random 64 KiB blocks with 1, 2, 3 or 8 random bits flipped or a byte range zeroed, undetected
 *     changes of the low 32 bits. */
#include "algo/khash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
unsigned long long kolmo_xxh3(const void *p, size_t n, unsigned long long seed);
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
typedef uint64_t (*hfn)(const void *, size_t, uint64_t);
static uint64_t h_x(const void *p, size_t n, uint64_t s) { return kolmo_xxh3(p, n, s); }
static uint64_t h_k(const void *p, size_t n, uint64_t s) { return kolmo_khash64(p, n, s); }
static const struct { const char *name; hfn f; } H[2] = {{"xxh3", h_x}, {"khash", h_k}};
static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static int cmp64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }
static void coll(const char *set, uint64_t *v, size_t n, int hk) {
    size_t c64 = 0, c32 = 0; uint64_t *w = malloc(n * 8);
    qsort(memcpy(w, v, n * 8), n, 8, cmp64); for (size_t i = 1; i < n; i++) c64 += w[i] == w[i - 1];
    for (size_t i = 0; i < n; i++) w[i] = v[i] & 0xFFFFFFFFull;
    qsort(w, n, 8, cmp64); for (size_t i = 1; i < n; i++) c32 += w[i] == w[i - 1];
    printf("  %-6s %-34s keys %9zu  collisions 64-bit %zu  low 32-bit %6zu (expected %.1f)\n", H[hk].name, set, n, c64, c32, (double)n * (n - 1) / 2 / 4294967296.0);
    free(w);
}
int main(int argc, char **argv) {
    const uint64_t seed = 0x637a6e31;   /* Kolmo's block-checksum seed */
    printf("speed (best of 7), seed 0x637a6e31:\n");
    size_t big = 64u << 20; uint8_t *buf = malloc(big); for (size_t i = 0; i < big; i++) buf[i] = (uint8_t)rnd();
    size_t sizes[] = {16, 64, 256, 1024, 4096, 65536, 1u << 20, 64u << 20};
    for (unsigned z = 0; z < sizeof sizes / sizeof *sizes; z++) {
        size_t n = sizes[z], reps = (256u << 20) / n; if (reps > 2000000) reps = 2000000;
        printf("  %8zu B:", n);
        for (int k = 0; k < 2; k++) {
            double best = 1e9; volatile uint64_t sink = 0;
            for (int r = 0; r < 7; r++) { double t = now(); for (size_t i = 0; i < reps; i++) sink += H[k].f(buf + (i & 7), n, seed); t = now() - t; if (t < best) best = t; }
            printf("  %s %8.0f MB/s", H[k].name, (double)n * reps / best / 1e6);
        }
        printf("\n"); fflush(stdout);
    }
    if (argc > 1 && !strcmp(argv[1], "-s")) return 0;
    printf("avalanche (worst bias over input bit x output bit; 4000 keys each; noise ~0.025):\n");
    size_t lens[] = {3, 8, 16, 24, 40, 64, 65, 200, 1024, 4100};
    for (unsigned z = 0; z < sizeof lens / sizeof *lens; z++) {
        size_t n = lens[z]; printf("  len %5zu:", n);
        for (int k = 0; k < 2; k++) {
            int nb = (int)(n * 8 < 512 ? n * 8 : 512); static int cnt[512][64]; memset(cnt, 0, sizeof cnt); const int K = 4000;
            uint8_t key[4200];
            for (int s = 0; s < K; s++) {
                for (size_t i = 0; i < n; i++) key[i] = (uint8_t)rnd();
                uint64_t h0 = H[k].f(key, n, seed);
                for (int b = 0; b < nb; b++) {
                    size_t bit = n * 8 <= 512 ? (size_t)b : (size_t)b * (n * 8 / 512);   /* spread over long keys */
                    key[bit >> 3] ^= (uint8_t)(1 << (bit & 7)); uint64_t d = H[k].f(key, n, seed) ^ h0; key[bit >> 3] ^= (uint8_t)(1 << (bit & 7));
                    for (int o = 0; o < 64; o++) cnt[b][o] += (int)((d >> o) & 1);
                }
            }
            double worst = 0; for (int b = 0; b < nb; b++) for (int o = 0; o < 64; o++) { double e = fabs((double)cnt[b][o] / K - 0.5); if (e > worst) worst = e; }
            printf("  %s %.4f", H[k].name, worst);
        }
        printf("\n");
    }
    printf("collisions:\n");
    for (int k = 0; k < 2; k++) {
        size_t n = 1u << 22; uint64_t *v = malloc(n * 8); uint8_t *blk = calloc(4096, 1);
        for (size_t i = 0; i < n; i++) { memcpy(blk + 2048, &i, 4); v[i] = H[k].f(blk, 4096, seed); }
        coll("counter in a 4 KiB zero block", v, n, k);
        size_t m = 0; uint8_t z[256] = {0}; v = realloc(v, (2048 + 2048 * 2047 / 2) * 8);
        for (int a = 0; a < 2048; a++) { z[a >> 3] ^= (uint8_t)(1 << (a & 7)); v[m++] = H[k].f(z, 256, seed);
            for (int b = a + 1; b < 2048; b++) { z[b >> 3] ^= (uint8_t)(1 << (b & 7)); v[m++] = H[k].f(z, 256, seed); z[b >> 3] ^= (uint8_t)(1 << (b & 7)); }
            z[a >> 3] ^= (uint8_t)(1 << (a & 7)); }
        coll("1 and 2 bits set in 256 zero bytes", v, m, k);
        uint8_t *zb = calloc(65537, 1); m = 0; for (size_t l = 0; l <= 65536; l++) v[m++] = H[k].f(zb, l, seed);
        coll("zero blocks of length 0..65536", v, m, k);
        if (argc > 1) {
            FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t tn = (size_t)ftell(f); rewind(f); char *t = malloc(tn + 1);
            if (fread(t, 1, tn, f) != tn) return 1; fclose(f);
            m = 0; size_t s = 0; v = realloc(v, (tn + 1) * 8);
            for (size_t i = 0; i <= tn; i++) if (i == tn || t[i] == '\n') { v[m++] = H[k].f(t + s, i - s, seed); s = i + 1; }
            qsort(v, m, 8, cmp64);   /* duplicates of equal lines are not collisions: hash each distinct line once */
            /* approximate: count distinct lines by exact compare is costly; equal lines give equal 64-bit hashes,
               so the 64-bit duplicates are removed first (a true 64-bit collision would also be removed, see note) */
            size_t u = 0; for (size_t i = 0; i < m; i++) if (!u || v[i] != v[u - 1]) v[u++] = v[i];
            coll("distinct lines of the text file (*)", v, u, k);
            free(t);
        }
        free(v); free(blk); free(zb);
    }
    printf("corruption of 64 KiB random blocks (low 32 bits unchanged = undetected), 200000 trials each:\n");
    for (int k = 0; k < 2; k++) {
        size_t n = 65536; uint8_t *b = malloc(n); size_t und[5] = {0}; const int T = 200000; const int nf[4] = {1, 2, 3, 8};
        for (int t = 0; t < T; t++) {
            for (size_t i = 0; i < n; i += 8) { uint64_t r = rnd(); memcpy(b + i, &r, 8); }
            uint32_t h0 = (uint32_t)H[k].f(b, n, seed);
            for (int c = 0; c < 4; c++) {
                size_t pos[8]; for (int j = 0; j < nf[c]; j++) { pos[j] = rnd() % (n * 8); b[pos[j] >> 3] ^= (uint8_t)(1 << (pos[j] & 7)); }
                und[c] += (uint32_t)H[k].f(b, n, seed) == h0;
                for (int j = nf[c] - 1; j >= 0; j--) b[pos[j] >> 3] ^= (uint8_t)(1 << (pos[j] & 7));
            }
            size_t a = rnd() % n, l = 1 + rnd() % 64; if (a + l > n) l = n - a; uint8_t sv[64]; memcpy(sv, b + a, l); memset(b + a, 0, l);
            und[4] += memcmp(sv, b + a, l) && (uint32_t)H[k].f(b, n, seed) == h0;
        }
        printf("  %-6s undetected: 1 bit %zu, 2 bits %zu, 3 bits %zu, 8 bits %zu, zeroed range %zu (expected ~%.3f each)\n", H[k].name, und[0], und[1], und[2], und[3], und[4], T / 4294967296.0);
        free(b);
    }
    return 0;
}
