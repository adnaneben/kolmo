/* KRANS round-trip and malformed-input tests. Build with sanitizers (make krans-check) or for another target. */
#include "krans.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }

static int fails = 0, cases = 0;

static void gen(int kind, uint8_t *b, size_t n) {
    switch (kind) {
    case 0: for (size_t i = 0; i < n; i++) b[i] = (uint8_t)rnd(); break;                       /* random */
    case 1: for (size_t i = 0; i < n; i++) { uint32_t r = rnd(); int k = 0; while (k < 40 && (r & 1)) { k++; r >>= 1; } b[i] = (uint8_t)(k * 7); } break; /* skewed */
    case 2: memset(b, 'z', n); break;                                                          /* one symbol */
    case 3: { uint8_t c = 0; for (size_t i = 0; i < n; i++) { uint32_t r = rnd(); c = (r & 7) ? (uint8_t)(c * 5 + 1 + (r >> 29)) : (uint8_t)(r >> 8); b[i] = c; } break; } /* order-1 structure */
    case 4: for (size_t i = 0; i < n; i++) b[i] = (rnd() % 1000) ? 'a' : (uint8_t)rnd(); break; /* one dominant, rare others */
    default: for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(i * 2654435761u >> 24) & (uint8_t)(rnd() % 2 ? 0x0f : 0xff); break;
    }
}

static void check(const uint8_t *in, size_t n, const krans_params *p, const char *what) {
    size_t cap = krans_bound(n, p->block);
    uint8_t *c = malloc(cap), *d = malloc(n + 1);
    cases++;
    size_t z = krans_compress(in, n, c, cap, p);
    if (!z) { printf("FAIL compress %s n=%zu o=%d w=%d p=%d B=%u T=%d\n", what, n, p->order, p->ways, p->prec, p->block, p->threads); fails++; goto out; }
    uint64_t m;
    if (krans_frame_size(c, z, &m) || m != n) { printf("FAIL size %s\n", what); fails++; goto out; }
    for (int dec = KRANS_DEC_SCALAR; dec <= KRANS_DEC_NEON; dec++) {
        if (!krans_decoder_available(dec)) continue;
        krans_set_decoder(dec);
        for (int t = 1; t <= 3; t += 2) {
            memset(d, 0xa5, n + 1);
            if (krans_decompress(c, z, d, n, t) || memcmp(in, d, n) || d[n] != 0xa5) {
                printf("FAIL roundtrip %s n=%zu o=%d w=%d p=%d B=%u dec=%s T=%d\n", what, n, p->order, p->ways, p->prec, p->block,
                       krans_decoder_name(dec), t);
                fails++; krans_set_decoder(KRANS_DEC_AUTO); goto out;
            }
        }
    }
    krans_set_decoder(KRANS_DEC_AUTO);
    if (z > 18 + (n / p->block + 1) * 5 + n) { printf("FAIL expansion %s n=%zu z=%zu\n", what, n, z); fails++; }
out:
    free(c); free(d);
}

/* corrupt, truncate and extend frames: decoding must fail cleanly or succeed, never touch memory out of bounds */
static void mangle(const uint8_t *in, size_t n, const krans_params *p) {
    size_t cap = krans_bound(n, p->block);
    uint8_t *c = malloc(cap + 16), *d = malloc(n ? n : 1), *e = malloc(cap + 16);
    size_t z = krans_compress(in, n, c, cap, p);
    if (!z) { fails++; goto out; }
    for (int it = 0; it < 60; it++) {
        memcpy(e, c, z); size_t zz = z;
        int how = it % 4;
        if (how == 0) e[rnd() % z] ^= (uint8_t)(1 + rnd() % 255);
        else if (how == 1) { for (int k = 0; k < 8; k++) e[rnd() % z] = (uint8_t)rnd(); }
        else if (how == 2) zz = rnd() % z;
        else { e[zz++] = (uint8_t)rnd(); }
        for (int dec = KRANS_DEC_SCALAR; dec <= KRANS_DEC_NEON; dec++) {
            if (!krans_decoder_available(dec)) continue;
            krans_set_decoder(dec);
            (void)krans_decompress(e, zz, d, n, 1 + (it & 1));
        }
        krans_set_decoder(KRANS_DEC_AUTO);
        cases++;
    }
out:
    free(c); free(d); free(e);
}

int main(int argc, char **argv) {
    int quick = argc > 1 && !strcmp(argv[1], "-q");
    printf("decoders:");
    for (int k = KRANS_DEC_SCALAR; k <= KRANS_DEC_NEON; k++) if (krans_decoder_available(k)) printf(" %s", krans_decoder_name(k));
    printf("\n");
    const size_t sizes[] = {0, 1, 2, 7, 8, 9, 31, 32, 33, 255, 1023, 1024, 1025, 4097, 70000, 300001};
    const int ways[] = {8, 16, 32}, precs[] = {0, 10, 12, 14}, orders[] = {0, 1, 2};
    const uint32_t blocks[] = {1024, 65536, 1 << 20};
    size_t maxn = 1 << 23; uint8_t *buf = malloc(maxn);
    for (int kind = 0; kind < 6; kind++)
        for (size_t si = 0; si < sizeof sizes / sizeof *sizes; si++) {
            size_t n = sizes[si];
            gen(kind, buf, n);
            for (int oi = 0; oi < 3; oi++) for (int wi = 0; wi < 3; wi++) for (int pi = 0; pi < 4; pi++) {
                if (quick && (pi & 1)) continue;
                for (int bi = 0; bi < 3; bi++) {
                    if (n > 70000 && bi == 0 && (wi != 2 || pi)) continue;   /* keep the 1 KiB-block runs on big inputs small */
                    krans_params p; krans_default_params(&p);
                    p.order = orders[oi]; p.ways = ways[wi]; p.prec = precs[pi]; p.block = blocks[bi];
                    p.threads = (int)(si % 3) + 1;
                    char what[32]; snprintf(what, sizeof what, "kind%d", kind);
                    check(buf, n, &p, what);
                }
            }
        }
    /* large multi-block inputs: 8 MiB skewed and order-1 data, default and forced modes, 4 threads */
    for (int kind = 1; kind <= 3; kind += 2) {
        gen(kind, buf, maxn);
        krans_params p; krans_default_params(&p);
        for (int o = 0; o < 3; o++) { p.order = o; p.threads = 4; check(buf, maxn, &p, "large"); }
        p.order = 2; p.block = 1 << 20; p.ways = 16; check(buf, maxn - 12345, &p, "large-1M");
    }
    /* every symbol present once, rest one symbol: extreme frequency tables */
    for (size_t n = 256; n <= 200000; n *= 7) {
        for (size_t i = 0; i < n; i++) buf[i] = i < 256 ? (uint8_t)i : 'q';
        for (int pr = 10; pr <= 14; pr += 2) for (int o = 0; o < 3; o++) {
            krans_params p; krans_default_params(&p); p.prec = pr; p.order = o; p.block = 65536;
            check(buf, n, &p, "allsyms");
        }
    }
    /* invalid parameters are refused */
    {
        krans_params p; krans_default_params(&p); uint8_t o[4096];
        p.ways = 4; if (krans_compress(buf, 10, o, sizeof o, &p)) { printf("FAIL accepted ways 4\n"); fails++; }
        krans_default_params(&p); p.prec = 9; if (krans_compress(buf, 10, o, sizeof o, &p)) { printf("FAIL accepted prec 9\n"); fails++; }
        krans_default_params(&p); p.block = 100; if (krans_compress(buf, 10, o, sizeof o, &p)) { printf("FAIL accepted block 100\n"); fails++; }
        krans_default_params(&p); if (krans_compress(buf, 1000, o, 100, &p)) { printf("FAIL accepted small capacity\n"); fails++; }
        cases += 4;
    }
    /* malformed frames */
    for (int kind = 0; kind < 6; kind++) {
        size_t n = kind == 2 ? 5000 : 40000 + 977 * kind;
        gen(kind, buf, n);
        for (int o = 0; o < 3; o++) for (int w = 0; w < 3; w++) {
            krans_params p; krans_default_params(&p); p.order = o; p.ways = ways[w]; p.block = 16384; p.prec = precs[(kind + w) % 4];
            mangle(buf, n, &p);
        }
    }
    free(buf);
    printf("%d cases, %d failures\n", cases, fails);
    if (!fails) printf("KRANS TESTS PASSED\n");
    return fails ? 1 : 0;
}
