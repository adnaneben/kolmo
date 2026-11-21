/* Command line for the four-line FASTQ prototype (frontier_fastq.c). Apache-2.0, Kolmo contributors.
 *
 * Standalone research tool: its frame is not a Kolmo frame and the `kolmo` binary never writes or reads it.
 *
 *   kolmo-fastq in out        compress
 *   kolmo-fastq -d in out     decompress
 *   kolmo-fastq -b in         in-memory timing on one thread: size, compress and decompress MB/s (fastest of
 *                             at least 3 runs and 1 s), with a byte-exact check of the decoded data
 *
 * Frame: "KFQ1", u64 original size, u32 block size, then per block: u32 raw length, u32 payload length,
 * payload. A payload length of 0xFFFFFFFF marks a block stored as is (the prototype declined it).
 * Blocks are cut at fixed offsets; the prototype keeps the partial records at both ends of a block verbatim. */
#include "frontier_fastq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK (16u << 20)
#define STORED 0xFFFFFFFFu

static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t get64(const uint8_t *p) { return (uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32; }

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); return NULL; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return NULL; }
    long l = ftell(f); rewind(f);
    uint8_t *b = malloc(l > 0 ? (size_t)l : 1);
    if (!b || fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)l; return b;
}

static int spit(const char *path, const uint8_t *b, size_t n) {
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return -1; }
    int bad = fwrite(b, 1, n, f) != n; if (fclose(f)) bad = 1;
    if (bad) perror(path);
    return bad ? -1 : 0;
}

static size_t bound(size_t n) { return 16 + (n / BLOCK + 1) * 8 + n; }

/* Returns the frame size, or 0 on failure. `declined` counts blocks stored as is. */
static size_t encode(const uint8_t *in, size_t n, uint8_t *out, size_t cap, unsigned *declined) {
    if (cap < 16) return 0;
    memcpy(out, "KFQ1", 4); put64(out + 4, n); put32(out + 12, BLOCK);
    size_t o = 16; *declined = 0;
    for (size_t a = 0; a < n; a += BLOCK) {
        size_t len = n - a < BLOCK ? n - a : BLOCK;
        if (cap - o < 8 + len) return 0;
        size_t c = frontier_fastq_encode(in + a, len, out + o + 8, len);
        put32(out + o, (uint32_t)len);
        if (c == 0 || c >= len) { put32(out + o + 4, STORED); memcpy(out + o + 8, in + a, len); c = len; ++*declined; }
        else put32(out + o + 4, (uint32_t)c);
        o += 8 + c;
    }
    return o;
}

/* Returns 0 on success. */
static int decode(const uint8_t *in, size_t n, uint8_t *out, size_t raw) {
    if (n < 16 || memcmp(in, "KFQ1", 4) || get64(in + 4) != raw) return -1;
    size_t i = 16, o = 0;
    while (o < raw) {
        if (n - i < 8) return -1;
        uint32_t len = get32(in + i), c = get32(in + i + 4); i += 8;
        if (len == 0 || len > raw - o) return -1;
        if (c == STORED) { if (n - i < len) return -1; memcpy(out + o, in + i, len); i += len; }
        else { if (n - i < c || frontier_fastq_decode(in + i, c, out + o, len)) return -1; i += c; }
        o += len;
    }
    return i == n ? 0 : -1;
}

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "-d")) {
        size_t n; uint8_t *in = slurp(argv[2], &n); if (!in) return 1;
        if (n < 16 || memcmp(in, "KFQ1", 4)) { fprintf(stderr, "kolmo-fastq: not a KFQ1 frame\n"); return 1; }
        size_t raw = (size_t)get64(in + 4); uint8_t *out = malloc(raw ? raw : 1);
        if (!out || decode(in, n, out, raw)) { fprintf(stderr, "kolmo-fastq: corrupt input\n"); return 1; }
        int r = spit(argv[3], out, raw); free(in); free(out); return r ? 1 : 0;
    }
    if (argc == 3 && !strcmp(argv[1], "-b")) {
        size_t n; uint8_t *in = slurp(argv[2], &n); if (!in) return 1;
        size_t cap = bound(n); uint8_t *c = malloc(cap), *d = malloc(n ? n : 1); unsigned declined = 0;
        if (!c || !d) return 1;
        size_t z = 0; double ce = 1e30, de = 1e30, t0 = now(); int k = 0;
        do { double s = now(); z = encode(in, n, c, cap, &declined); double e = now() - s; if (e < ce) ce = e; k++; } while (z && (k < 3 || now() - t0 < 1.0));
        if (!z) { fprintf(stderr, "kolmo-fastq: encode failed\n"); return 1; }
        t0 = now(); k = 0;
        do { double s = now(); if (decode(c, z, d, n)) { fprintf(stderr, "kolmo-fastq: decode failed\n"); return 1; } double e = now() - s; if (e < de) de = e; k++; } while (k < 3 || now() - t0 < 1.0);
        if (memcmp(in, d, n)) { fprintf(stderr, "kolmo-fastq: round trip differs\n"); return 1; }
        printf("{\"input\": %zu, \"compressed\": %zu, \"blocks_stored\": %u, \"compress_mbs\": %.2f, \"decompress_mbs\": %.2f}\n",
               n, z, declined, n / ce / 1e6, n / de / 1e6);
        return 0;
    }
    if (argc != 3) { fprintf(stderr, "usage: kolmo-fastq in out | -d in out | -b in\n"); return 2; }
    size_t n; uint8_t *in = slurp(argv[1], &n); if (!in) return 1;
    size_t cap = bound(n); uint8_t *c = malloc(cap), *d = malloc(n ? n : 1); unsigned declined = 0;
    if (!c || !d) return 1;
    size_t z = encode(in, n, c, cap, &declined);
    if (!z || decode(c, z, d, n) || memcmp(in, d, n)) { fprintf(stderr, "kolmo-fastq: encode or verification failed\n"); return 1; }
    if (declined) fprintf(stderr, "kolmo-fastq: %u block(s) not FASTQ, stored as is\n", declined);
    return spit(argv[2], c, z) ? 1 : 0;
}
