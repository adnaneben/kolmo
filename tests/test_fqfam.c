/* FASTQ family blocks (block codec 16, container version 10): round trips through the container at ultra, the
 * modes and levels that must not write codec 16, the family coder on its own with many small chunks, and decoding
 * of damaged payloads and frames (must fail cleanly, never read or write out of bounds; `make check-fqfam` runs it
 * under AddressSanitizer and UndefinedBehaviorSanitizer). */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
/* reads sampled from a random genome, both strands, some N, variable lengths, Illumina-like names and qualities */
static size_t make_fastq(uint8_t *o, size_t cap, int reads, int plus_names) {
    static char g[20000]; static int ready = 0;
    if (!ready) { for (int i = 0; i < 20000; i++) g[i] = "ACGT"[rnd() % 4]; ready = 1; }
    size_t n = 0;
    for (int r = 0; r < reads; r++) {
        int len = r % 53 == 0 ? 0 : 30 + (int)(rnd() % 120), at = (int)(rnd() % (20000 - 160)), rc = rnd() % 2, q = 30 + (int)(rnd() % 10);
        char name[96]; int nl = snprintf(name, sizeof name, "SIM.%d HWI-ST1:7:C1:%d:%d:%u:%u/1", r + 1, 1 + r / 400, 1101 + r / 1000, 1000 + rnd() % 19000, 1000 + rnd() % 99000);
        if (n + 2 * (size_t)nl + 2 * (size_t)len + 8 > cap) break;
        o[n++] = '@'; memcpy(o + n, name, (size_t)nl); n += (size_t)nl; o[n++] = '\n';
        for (int i = 0; i < len; i++) {
            char b = rc ? "TGCA"[strchr("ACGT", g[at + len - 1 - i]) - "ACGT"] : g[at + i];
            if (rnd() % 300 == 0) b = "ACGT"[rnd() % 4];
            if (r % 7 == 0 && i == len / 2) b = 'N';
            o[n++] = (uint8_t)b;
        }
        o[n++] = '\n'; o[n++] = '+';
        if (plus_names && r % 3 == 0) { memcpy(o + n, name, (size_t)nl); n += (size_t)nl; }
        o[n++] = '\n';
        for (int i = 0; i < len; i++) {
            if (i > len - 15 && r % 5 == 0) { o[n++] = '#'; continue; }
            q += (int)(rnd() % 5) - 2 - (i > 80); if (q < 2) q = 2; if (q > 41) q = 41;
            o[n++] = (uint8_t)(33 + q);
        }
        o[n++] = '\n';
    }
    return n;
}
int main(void) {
    int fails = 0;
    size_t cap = 4u << 20; uint8_t *in = malloc(cap), *z = malloc(2 * cap + (1u << 20)), *b = malloc(cap + 16);
    size_t n = make_fastq(in, cap, 3000, 1);
    /* ultra: the block must be a codec 16 block in a version 10 frame and round trip, whole and by range */
    kolmo_params P = {KOLMO_ULTRA, 0, 4, 0, 1};
    int64_t zn = kolmo_compress(in, n, z, 2 * cap + (1u << 20), &P);
    if (zn <= 0 || z[4] != 10 || z[HDR + 12] != C_FQ) { printf("ULTRA DID NOT WRITE CODEC 16 (zn %lld version %d codec %d)\n", (long long)zn, zn > 0 ? z[4] : -1, zn > 0 ? z[HDR + 12] : -1); fails++; }
    else printf("ultra: %zu -> %lld bytes, codec 16, version 10\n", n, (long long)zn);
    for (int t = 1; zn > 0 && t <= 8; t *= 2) {
        memset(b, 0, n);
        if (kolmo_decompress(z, (size_t)zn, b, n, t) != (int64_t)n || memcmp(in, b, n)) { printf("ROUND TRIP FAIL threads=%d\n", t); fails++; }
    }
    if (zn > 0) {
        uint64_t off = n / 3; size_t len = 1000;
        if (kolmo_read_range(z, (size_t)zn, off, len, b, 2) != (int64_t)len || memcmp(in + off, b, len)) { printf("RANGE READ FAIL\n"); fails++; }
    }
    /* the same bytes whatever the thread count */
    {   kolmo_params P1 = {KOLMO_ULTRA, 0, 1, 0, 1}; uint8_t *z1 = malloc(2 * cap + (1u << 20));
        int64_t z1n = kolmo_compress(in, n, z1, 2 * cap + (1u << 20), &P1);
        if (z1n != zn || memcmp(z, z1, (size_t)zn)) { printf("OUTPUT DEPENDS ON THREADS\n"); fails++; }
        free(z1); }
    /* a version 9 frame may not carry codec 16: patch the version, fix the header checksum, expect a refusal */
    if (zn > 0) {
        uint8_t *d = malloc((size_t)zn); memcpy(d, z, (size_t)zn); d[4] = 9;
        uint32_t nb = get32(d + 20), hc = kolmo_crc32c(0, d, 28); hc = kolmo_crc32c(hc, d + HDR, (size_t)nb * IDX); put32(d + 28, hc);
        if (kolmo_decompress(d, (size_t)zn, b, n, 2) != KOLMO_ERR_CORRUPT) { printf("CODEC 16 ACCEPTED IN VERSION 9\n"); fails++; }
        free(d);
    }
    /* modes and levels that must not write codec 16 */
    {   kolmo_mode modes[] = {KOLMO_RT, KOLMO_FAST, KOLMO_ECO, KOLMO_HC, KOLMO_MAX, KOLMO_ULTRA}; int levels[] = {0, 0, 0, 0, 0, 1};
        for (int k = 0; k < 6; k++) {
            kolmo_params Q = {modes[k], levels[k], 2, 0, 1};
            int64_t qn = kolmo_compress(in, n, z, 2 * cap + (1u << 20), &Q);
            int bad = qn <= 0 || z[4] >= 10;
            for (uint32_t i = 0; qn > 0 && i < get32(z + 20); i++) bad |= z[HDR + (size_t)i * IDX + 12] == C_FQ;
            if (bad || kolmo_decompress(z, (size_t)qn, b, n, 2) != (int64_t)n || memcmp(in, b, n)) { printf("MODE %d LEVEL %d: CODEC 16 OR ROUND TRIP FAIL\n", modes[k], levels[k]); fails++; }
        }
    }
    /* the family coder alone: small chunks (hundreds of jobs), every thread count, then damaged copies */
    int damaged = 0;
    for (int it = 0; it < 6; it++) {
        size_t m = make_fastq(in, cap, 200 + it * 300, it & 1), fz = 0;
        uint8_t *f = kf_fastq_compress(in, m, 1 + it % 4, 257 + it * 500, it % 2 ? 1000 : 0, &fz);
        if (!f) { printf("FAMILY CODER REFUSED FASTQ it=%d\n", it); fails++; continue; }
        if (kf_fastq_decompress(f, fz, b, m, 3) || memcmp(in, b, m)) { printf("FAMILY ROUND TRIP FAIL it=%d\n", it); fails++; }
        if (!kf_fastq_decompress(f, fz, b, m + 1, 1)) { printf("LENGTH NOT CHECKED it=%d\n", it); fails++; }
        for (int k = 0; k < 60; k++) {
            uint8_t *d = malloc(fz); memcpy(d, f, fz);
            size_t at = k < 30 ? rnd() % (fz < 64 ? fz : 64) : rnd() % fz;   /* the header and job table, or anywhere */
            d[at] = k % 3 ? (uint8_t)rnd() : (uint8_t)(d[at] ^ (1u << (rnd() % 8)));
            size_t dn = k % 11 == 0 ? rnd() % (fz + 1) : fz;
            if (!kf_fastq_decompress(d, dn, b, m, 1 + k % 3) && memcmp(in, b, m)) damaged++;   /* a wrong result is caught by the block checksum */
            free(d);
        }
        free(f);
    }
    /* not FASTQ: refused */
    {   static const char *bad[] = {"@r\nACGT\n+\nIII\n", "@r\nACGT\n-\nIIII\n", "@r\nACGT\n+\nIIII", "@r\nAC\n+r2\nII\n", "r\nAC\n+\nII\n", "@r\nAC\n+\nI\x1f\n"};
        for (int k = 0; k < 6; k++) { size_t fz; uint8_t *f = kf_fastq_compress((const uint8_t *)bad[k], strlen(bad[k]), 1, KF_CHUNK, KF_BCHUNK, &fz); if (f) { printf("ACCEPTED NON-FASTQ %d\n", k); fails++; free(f); } }
    }
    printf("damaged copies that decoded to other bytes (caught by the container checksum): %d\n", damaged);
    printf(fails ? "FQFAM TESTS FAILED (%d)\n" : "FQFAM TESTS PASSED\n", fails);
    free(in); free(z); free(b);
    return fails != 0;
}
