/* Regression tests for decoder memory-safety and DoS fixes (review r1 of 0.0.1: D1, D2, D4, D5, D7, E1, E2).
 * One translation unit with the library, like kolmoresource.c, to reach internal decoders and helpers.
 * The ASan build compiles the separately built coders (qlfc, khuf, klzma, ...) with the sanitizers too. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#define KOLMO_TESTING
#include "../src/kolmo.c"
#include "../src/algo/qlfc.h"
#include "../src/algo/khuf.h"
#include "../src/algo/klzma.h"

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define HARD_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define HARD_ASAN 1
#endif

static long peak_mb(void) {
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss >> 20;     /* bytes */
#else
    return ru.ru_maxrss >> 10;     /* KiB */
#endif
}

/* E1: the log template encoder kept 1 KiB per template (1.5 M templates: 1.35 GB for 60 MB). A 6 MB log of 150 k
 * distinct words, each on four lines, now costs about 10x the input instead of about 34x. Runs first, so the
 * process peak is this test's. Not under ASan (shadow memory and quarantine distort RSS). */
static void log_encoder_memory(void) {
#ifndef HARD_ASAN
    const size_t W = 150000, LINES = 4 * W; size_t cap = LINES * 12 + 64, n = 0;
    uint8_t *in = malloc(cap); assert(in);
    uint64_t s = 12345;
    char (*words)[8] = malloc(W * 8); assert(words);
    for (size_t w = 0; w < W; w++) for (int k = 0; k < 7; k++) { s = s * 6364136223846793005ull + 1442695040888963407ull; words[w][k] = (char)('a' + (s >> 33) % 26); }
    for (size_t l = 0; l < LINES; l++) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        size_t w = (l * 7919) % W;
        memcpy(in + n, words[w], 7); n += 7; in[n++] = ' '; in[n++] = (uint8_t)('0' + (s >> 40) % 10); in[n++] = '\n';
    }
    free(words);
    uint8_t *dst = malloc(n + TBL_SLACK(n)); assert(dst);
    long before = peak_mb();
    int64_t r = log_encode(in, n, dst, n + TBL_SLACK(n));
    long grew = peak_mb() - before;
    printf("log encoder: %zu bytes -> %lld, peak RSS +%ld MB\n", n, (long long)r, grew);
    assert(r > 0);
    assert(grew < (long)(16 * n >> 20));   /* was about 34x the input */
    free(in); free(dst);
#endif
}

/* E2: 256 identical constant columns made formula discovery run a full-sample check for every (y, a, b) triple. */
static void formula_discovery_cap(void) {
    const int K = 256, R = 1100; size_t cap = (size_t)R * K * 5 + 16, n = 0;
    uint8_t *s = malloc(cap); assert(s);
    for (int r = 0; r < R; r++) for (int j = 0; j < K; j++) { memcpy(s + n, "12.5", 4); n += 4; s[n++] = j + 1 < K ? ',' : '\n'; }
    fset_t *fs = calloc(1, sizeof *fs); assert(fs);
    fs_exact_checks = 0;
    fset_discover_pub(s, n, ',', K, 0, 1, fs);
    printf("formula discovery: %d columns, %ld full-sample checks, %d formulas\n", K, fs_exact_checks, fs->n);
    assert(fs_exact_checks <= (long)K * FS_EXACT_MAX);   /* was 256 * 255 * 254 */
    free(fs); free(s);
}

/* D1: kolmo_query_run with more than 64 threads wrote past pthread_t th[64] (both query paths). */
static int sink(void *c, const uint8_t *p, size_t n) { (void)c; (void)p; (void)n; return 0; }
static void query_threads(void) {
    size_t n = 2 << 20; uint8_t *src = malloc(n); size_t o = 0; int row = 0;
    while (o + 64 < n) o += (size_t)snprintf((char *)src + o, n - o, "line %d some text here\n", row++);
    n = o;
    kolmo_params p = {KOLMO_FAST, 0, 1, 4096, 1, 0};
    size_t cap = kolmo_indexed_bound(n, &p); uint8_t *dst = malloc(cap);
    int64_t c = kolmo_compress_indexed(src, n, dst, cap, &p, NULL);
    assert(c > 0);
    for (int noidx = 0; noidx < 2; noidx++) {
        kolmo_query q = {0}; q.flags = KOLMO_Q_COUNT | (noidx ? KOLMO_Q_NOINDEX : 0); q.threads = 256;
        kolmo_query_stats st; int64_t r = kolmo_query_run(dst, (size_t)c, &q, sink, NULL, &st);
        assert(r == row);
    }
    printf("query: 256 threads, %d rows, both paths\n", row);
    free(src); free(dst);
}

/* D2: the table v1 decoder (block flag 3) allocated 9 * nr bytes for a row count read from the payload. */
static void table_v1_rows(void) {
    uint32_t nr = 200000000u;
    uint8_t tb[29] = {0};                                   /* k=1 | nr | nexc=0 | noNL=0 | column: type 0, size 8, cnt 0 */
    uint32_t h[4] = {1, nr, 0, 0}; memcpy(tb, h, 16); tb[16] = 0; put32(tb + 17, 8);
    size_t dlen = sizeof tb, raw = dlen + 4, cl = 4 + 4 + dlen;
    uint8_t f[HDR + IDX + 64] = {0};
    memcpy(f, "CZN1", 4); f[4] = KOLMO_MAX_VERSION; f[5] = 1; f[11] = 0;
    put64(f + 12, raw); put32(f + 20, 1); put32(f + 24, 1 << 20);
    uint8_t *e = f + HDR; put32(e, (uint32_t)raw); put32(e + 4, (uint32_t)cl); put32(e + 8, 0); e[12] = C_STORE; e[13] = 3;
    uint8_t *pay = f + HDR + IDX; pay[0] = T_TBL; pay[1] = ','; put32(pay + 4, (uint32_t)dlen); memcpy(pay + 8, tb, dlen);
    uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, IDX); put32(f + 28, hc);
    long before = peak_mb();
    uint8_t out[64]; int64_t r = kolmo_decompress(f, HDR + IDX + cl, out, sizeof out, 1);
    long grew = peak_mb() - before;
    printf("table v1: nr=%u -> %lld, peak RSS +%ld MB\n", nr, (long long)r, grew);
    assert(r < 0);
    assert(grew < 64);                                     /* was about 1 GB */
    /* the decoder itself, a row count above column 0's field count: rejected before the row arrays */
    uint32_t h2[4] = {1, 20, 0, 0}; memcpy(tb, h2, 16);
    uint8_t o2[64]; assert(tbl_decode(tb, (int64_t)dlen, ',', o2, 30) < 0);
}

/* D4: a KLZMA match distance of 0xFFFFFFFF wrapped reps[0] + 1 to 0 and the decoder returned the output buffer's
 * prior contents with rc 0 (payload from review r1, work-codecs/poc_klzma_dist0.c). */
static void klzma_dist_wrap(void) {
    static const uint8_t pl[18] = {0x5d, 0x00, 0x20, 0xdf, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xbf, 0xfe, 0xa3, 0xbc, 0x21,
                                   0x91, 0x16};
    const size_t N = 1 + 4 * 273; uint8_t *out = malloc(N); memset(out, 0x5A, N);
    int r = kolmo_klzma_decompress(pl, sizeof pl, out, N);
    printf("klzma: distance 0xFFFFFFFF -> rc %d\n", r);
    assert(r != 0);
    /* a valid stream still decodes, and a truncated one fails */
    size_t n = 1 << 16; uint8_t *src = malloc(n), *z = malloc(n * 2 + 64), *back = malloc(n);
    for (size_t i = 0; i < n; i++) src[i] = (uint8_t)("kolmo decoder hardening "[i % 24] ^ (i >> 12));
    size_t zn = kolmo_klzma_compress(src, n, z, n * 2 + 64, 5);
    assert(zn > 6);
    assert(kolmo_klzma_decompress(z, zn, back, n) == 0 && !memcmp(src, back, n));
    assert(kolmo_klzma_decompress(z, zn / 4, back, n) != 0);
    free(out); free(src); free(z); free(back);
}

/* D5: QLFC read MTF table entries the header never set: with a rank above the last entry the header filled, stack
 * bytes reached the output and the decoder returned success (n). These two payloads (mutations of valid streams,
 * found by a fuzzer that pre-filled the table with 0xEE) decoded with rc = n and 31 and 27 such bytes before. */
static const uint8_t qlfc_bad1[] = {   /* coder 1 (static), n 59 */
    0x01, 0x00, 0x00, 0x00, 0x00, 0x39, 0x00, 0x56, 0x86, 0x4d, 0x4f, 0x3b, 0x11, 0x18, 0x6c, 0x14, 0x6e, 0x91, 0x1f, 0xc1,
    0x0d, 0xca, 0x7c, 0x00, 0x00};
static const uint8_t qlfc_bad2[] = {   /* coder 2 (adaptive), n 180 */
    0x01, 0x00, 0x00, 0x00, 0x00, 0xae, 0x00, 0x4e, 0xbf, 0x58, 0x56, 0x9b, 0xfc, 0xbb, 0x3b, 0x91, 0xe5, 0xee, 0x5c, 0x6f,
    0x62, 0x60, 0xcc, 0x3d, 0xb4, 0x1f, 0xf8, 0x90, 0x89, 0x4a, 0x7d, 0xba, 0x0d, 0x30, 0xd1, 0x20, 0x00, 0xfe, 0x13, 0xa1,
    0x69, 0xb9, 0xab};
static void qlfc_mtf(void) {
    uint8_t a[256], b[256];
    int ra = kolmo_qlfc_decompress(qlfc_bad1, (int)sizeof qlfc_bad1, a, 59, 1, 1);
    int rb = kolmo_qlfc_decompress(qlfc_bad2, (int)sizeof qlfc_bad2, b, 180, 2, 1);
    printf("qlfc: crafted rank past the MTF header -> rc %d, %d\n", ra, rb);
    assert(ra < 0 && rb < 0);
    /* valid data still round-trips through both coders */
    int m = 1 << 16; uint8_t *src = malloc(m), *z = malloc(m + 4096), *back = malloc(m);
    for (int i = 0; i < m; i++) src[i] = (uint8_t)("abracadabra "[i % 12] + (i % 997 == 0));
    for (int coder = 1; coder <= 2; coder++) {
        int zn = kolmo_qlfc_compress(src, z, m, coder, 1);
        assert(zn > 0);
        assert(kolmo_qlfc_decompress(z, zn, back, m, coder, 1) == m && !memcmp(src, back, m));
    }
    free(src); free(z); free(back);
}

/* D7: khuf computed stream starts in int: a stream length of 2^31 put stream 1 2 GiB before the input. The input is
 * a mapping of 2^31 + 64 bytes of which only the first page is touched. */
static void khuf_offsets(void) {
    const size_t L0 = (size_t)1 << 31, cn = L0 + 64;
    uint8_t *in = mmap(NULL, cn, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (in == MAP_FAILED) { printf("khuf: no 2 GiB mapping, skipped\n"); return; }
    size_t i = 0; in[i++] = 1; in[i++] = 'a'; in[i++] = 1; in[i++] = 'b'; in[i++] = 1;   /* two symbols, 1 bit each */
    in[i++] = 0x80; in[i++] = 0x80; in[i++] = 0x80; in[i++] = 0x80; in[i++] = 0x08;      /* L0 = 2^31 (uv) */
    in[i++] = 0; in[i++] = 0;                                                             /* L1 = L2 = 0 */
    uint8_t out[64]; int r = kolmo_khuf_decompress(in, cn, out, 64);
    printf("khuf: stream length 2^31 -> rc %d\n", r);
    assert(r != 0);
    munmap(in, cn);
}

/* ---- review40 (SYNTHESIS.md section 2, bugs 6, 7, 8, 10, 16); the vectors are the reviewers' reproducers */
static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); assert(f);
    fseek(f, 0, SEEK_END); long z = ftell(f); rewind(f); assert(z >= 0);
    uint8_t *b = malloc((size_t)z + 1); assert(b);
    assert(fread(b, 1, (size_t)z, f) == (size_t)z); fclose(f); *n = (size_t)z; return b;
}
static void fix_crc(uint8_t *f) {   /* the header and index CRC after a mutation of either */
    uint32_t nb = get32(f + 20), hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, (size_t)nb * IDX); put32(f + 28, hc);
}
static uint64_t payload_at(const uint8_t *f, uint32_t blk) {   /* the offset of block blk's payload (plan prefix first) */
    uint32_t nb = get32(f + 20); uint64_t po = HDR + (uint64_t)nb * IDX;
    for (uint32_t i = 0; i < blk; i++) po += get32(f + HDR + (size_t)i * IDX + 4);
    return po;
}
static int64_t decode_n(const uint8_t *f, size_t n, uint8_t **out, int threads, uint64_t max_memory) {
    int64_t o = kolmo_original_size(f, n); if (o < 0) return o;
    *out = malloc(o ? (size_t)o : 1); assert(*out);
    kolmo_decode_limits L = {0, 0, 0, max_memory};
    return kolmo_decompress_limited(f, n, *out, (size_t)o, threads, &L);
}

/* Bug 6: KAU1 chunk_log2 30 (every encoder writes 18) made the decoder allocate 16 GiB per chunk and abort() when
 * that failed. Refused now before anything is allocated; and no abort() is left in the audio coder. */
static void audio_chunk_cap(void) {
    size_t n; uint8_t *f = slurp("tests/vectors/review40/aud-chunk30.kolmo", &n), *out = NULL;
    int64_t r = decode_n(f, n, &out, 1, 0);
    printf("audio: forged chunk_log2 30 -> %lld\n", (long long)r);
    assert(r == KOLMO_ERR_CORRUPT); free(out); free(f);
    size_t on, wn; uint8_t *w = slurp("tests/v12/tone.wav", &wn); f = slurp("tests/v12/tone.wav.eco.kolmo", &on);
    assert(f[HDR + 12] == C_AUD);
    uint64_t k = payload_at(f, 0) + 4;                          /* the KAU1 stream */
    assert(!memcmp(f + k, "KAU1", 4) && f[k + 7] == 18);
    r = decode_n(f, on, &out, 1, 0); assert(r == (int64_t)wn && !memcmp(out, w, wn)); free(out);
    for (int c = 19; c <= 30; c += 11) {
        f[k + 7] = (uint8_t)c; r = decode_n(f, on, &out, 1, 0); free(out);
        printf("audio: chunk_log2 %d -> %lld\n", c, (long long)r);
        assert(r == KOLMO_ERR_CORRUPT);
    }
    free(f); free(w);
}

/* Bug 8: a video frame (codec 20) decoded past max_memory (353 MiB peak under 64 MiB). Admitted like every frame now,
 * and the nested frame decodes under what the outer buffers leave of the limit. */
static void video_memory(void) {
    size_t n; uint8_t *f = slurp("tests/vectors/review40/video-400.kolmo", &n), *out = NULL;
    kolmo_decode_limits L = {0, 0, 0, 64u << 20};
    assert(kolmo_check_frame(f, n, &L) == KOLMO_ERR_MEMORY);
    int64_t r = decode_n(f, n, &out, 2, 64u << 20); free(out); free(f);
    printf("video: 184 MB frame under a 64 MiB limit -> %lld\n", (long long)r);
    assert(r == KOLMO_ERR_MEMORY);
    /* a small clip: refused one byte under its declared figure, decoded at it */
    const int W = 64, H = 48, FR = 24; const char *hd = "YUV4MPEG2 W64 H48 F25:1 Ip A1:1 C420jpeg\n";
    size_t fs = (size_t)W * H * 3 / 2, yn = strlen(hd) + (size_t)FR * (6 + fs), o = 0; uint8_t *y = malloc(yn); assert(y);
    memcpy(y, hd, strlen(hd)); o = strlen(hd);
    for (int t = 0; t < FR; t++) {
        memcpy(y + o, "FRAME\n", 6); o += 6;
        for (size_t i = 0; i < fs; i++) y[o + i] = (uint8_t)(i < (size_t)W * H ? ((i % W) * 3 + (i / W) * 2 + t * 5) : 128 + (int)(i % 7));
        o += fs;
    }
    kolmo_params p = {0}; assert(!kolmo_params_for_level(3, &p)); p.threads = 2;
    size_t cap = kolmo_bound(yn, &p); uint8_t *z = malloc(cap); assert(z);
    int64_t zn = kolmo_compress(y, yn, z, cap, &p); if (zn <= 0) printf("video compress -> %lld\n", (long long)zn); assert(zn > 0);
    assert(get32(z + 20) == 1 && z[HDR + 12] == C_VID);
    int64_t d = kolmo_decode_memory(z, (size_t)zn); assert(d > 0);
    r = decode_n(z, (size_t)zn, &out, 2, (uint64_t)d - 1); free(out);
    assert(r == KOLMO_ERR_MEMORY);
    r = decode_n(z, (size_t)zn, &out, 2, (uint64_t)d);
    printf("video: %zu B clip, declared %lld B: one byte under -> refused, at it -> %lld\n", yn, (long long)d, (long long)r);
    assert(r == (int64_t)yn && !memcmp(out, y, yn)); free(out); free(z); free(y);
}

/* Bug 10: a FASTQ family block ran nt/nblk model jobs at once while the frame declares one (702 MB under -M 240M at
 * -T 8). Extra jobs are reserved with the governor now: none under a limit at the declared figure, some without one. */
static void fastq_threads(void) {
    size_t n; uint8_t *f = slurp("tests/v12/r0.0.2/reads.fastq.LU.kolmo", &n), *out = NULL;
    assert(f[HDR + 12] == C_FQ);
    int64_t d = kolmo_decode_memory(f, n), o = kolmo_original_size(f, n); assert(d > 0 && o > 0);
    int peak;
    kmem_stats_reset_peak();
    int64_t r = decode_n(f, n, &out, 8, (uint64_t)d); free(out); kmem_stats(NULL, &peak, NULL);
    printf("fastq: -T 8 at the declared %lld B -> %lld, %d job(s) reserved at once\n", (long long)d, (long long)r, peak);
    assert(r == o && peak == 1);
    kmem_stats_reset_peak();
    r = decode_n(f, n, &out, 8, KOLMO_MEM_MAX); free(out); kmem_stats(NULL, &peak, NULL);
    printf("fastq: -T 8 unlimited -> %lld, %d job(s) reserved at once\n", (long long)r, peak);
    assert(r == o && peak > 1);
    free(f);
}

/* Bug 16: what no encoder writes is refused (stored blocks with a plan or a transform, rANS and khuf as block codecs);
 * plan 7's a, b and c stay accepted, because 0.0.1 and 0.0.2 can write a shuffle's parameters there. */
static void block_strictness(void) {
    uint8_t in[8192], z[16384], *out = NULL; uint64_t s = 7;
    for (size_t i = 0; i < sizeof in; i++) { s = s * 6364136223846793005ull + 1442695040888963407ull; in[i] = (uint8_t)(s >> 56); }
    kolmo_params p = {0}; assert(!kolmo_params_for_level(3, &p)); p.threads = 1;
    int64_t zn = kolmo_compress(in, sizeof in, z, sizeof z, &p); assert(zn > 0 && z[HDR + 12] == C_STORE && !(z[10] & 1));
    uint64_t k = payload_at(z, 0);
    assert(decode_n(z, (size_t)zn, &out, 1, 0) == (int64_t)sizeof in); free(out);
    for (int m = 0; m < 4; m++) {                               /* plan t, a, b, c of a stored block */
        uint8_t g[16384]; memcpy(g, z, (size_t)zn); g[k + m] = (uint8_t)(m ? 5 : 1);
        int64_t r = decode_n(g, (size_t)zn, &out, 1, 0); free(out);
        assert(r == KOLMO_ERR_CORRUPT);
    }
    {                                                          /* a stored block with plan 1 and tflag 1 (a transpose) */
        uint8_t g[16384]; memcpy(g, z, (size_t)zn); g[k] = T_TP; g[k + 1] = 2; g[HDR + 13] = 1; fix_crc(g);
        int64_t r = decode_n(g, (size_t)zn, &out, 1, 0); free(out);
        assert(r == KOLMO_ERR_CORRUPT);
    }
    uint8_t e[64]; int64_t en = kolmo_compress(in, 0, e, sizeof e, &p); assert(en > 0 && e[HDR + 12] == C_STORE);
    for (int c = 14; c <= 27; c += 13) {                        /* rANS and khuf are graph leaves, never block codecs */
        uint8_t g[64]; memcpy(g, e, (size_t)en); g[HDR + 12] = (uint8_t)c; fix_crc(g);
        int64_t r = kolmo_decompress(g, (size_t)en, out = malloc(1), 0, 1); free(out);
        printf("block codec %d -> %lld\n", c, (long long)r);
        assert(r == KOLMO_ERR_CORRUPT);
    }
    size_t xn, on; uint8_t *x = slurp("tests/v12/r0.0.2/code.x86.L3.kolmo", &xn), *orig = slurp("tests/v12/r0.0.2/in/code.x86", &on);
    int found = 0;
    for (uint32_t b = 0; b < get32(x + 20); b++) {
        uint64_t q = payload_at(x, b);
        if (x[q] == T_X86) { x[q + 1] = 0x55; x[q + 2] = 0xAA; x[q + 3] = 0x07; found++; }
    }
    assert(found);
    int64_t r = decode_n(x, xn, &out, 1, 0);
    printf("plan 7 with a, b, c = 55 aa 07 (as 0.0.x may write) -> %lld\n", (long long)r);
    assert(r == (int64_t)on && !memcmp(out, orig, on)); free(out); free(x); free(orig);
}

/* Bug 7: decode work. The header's mode now bounds the cost class (a context-mixing leaf in a non-ultra frame is
 * refused), and kolmo_frame_cost reports the decoded bytes per class without decoding. */
static void decode_cost(void) {
    size_t n; uint8_t *f = slurp("tests/vectors/review40/fast-graph-cm-leaf.kolmo", &n), *out = NULL;
    assert(f[5] == KOLMO_FAST && f[HDR + 12] == C_GRAPH);
    kolmo_decode_cost c;
    assert(kolmo_frame_cost(f, n, &c) == KOLMO_ERR_CORRUPT);
    int64_t r = decode_n(f, n, &out, 1, 0); free(out); free(f);
    printf("fast frame with a context-mixing leaf -> %lld\n", (long long)r);
    assert(r == KOLMO_ERR_CORRUPT);
    f = slurp("tests/vectors/review40/cm-16mib-zeros.kolmo", &n);
    assert(!kolmo_frame_cost(f, n, &c));
    printf("5,978 B codec-7 frame: model %llu B, sort %llu, fast %llu, slowest %d\n", (unsigned long long)c.bytes[KOLMO_COST_MODEL],
           (unsigned long long)c.bytes[KOLMO_COST_SORT], (unsigned long long)c.bytes[KOLMO_COST_FAST], c.slowest);
    assert(c.bytes[KOLMO_COST_MODEL] == 16u << 20 && c.slowest == KOLMO_COST_MODEL); free(f);
    f = slurp("tests/vectors/review40/video-400.kolmo", &n);
    assert(!kolmo_frame_cost(f, n, &c) && c.slowest == KOLMO_COST_FAST && c.bytes[KOLMO_COST_FAST] >= 184322435u); free(f);
    /* ultra level 2 writes context-mixing graph leaves: decoded under its own header, refused under a fast one */
    size_t vn = 1u << 20; uint8_t *v = malloc(vn * 8); assert(v); uint64_t s = 1, t = 1700000000000ull;
    for (size_t i = 0; i < vn; i++) { s = s * 6364136223846793005ull + 1442695040888963407ull; t += 1000 + (s >> 58); put64(v + 8 * i, t); }
    kolmo_params p = {KOLMO_ULTRA, 2, 2, 0, 1, 0};
    size_t cap = kolmo_bound(vn * 8, &p); uint8_t *z = malloc(cap); assert(z);
    int64_t zn = kolmo_compress(v, vn * 8, z, cap, &p); assert(zn > 0);
    assert(!kolmo_frame_cost(z, (size_t)zn, &c));
    printf("ultra level 2 int64 series: model %llu B, fast %llu B\n", (unsigned long long)c.bytes[KOLMO_COST_MODEL], (unsigned long long)c.bytes[KOLMO_COST_FAST]);
    r = decode_n(z, (size_t)zn, &out, 2, 0); assert(r == (int64_t)(vn * 8) && !memcmp(out, v, vn * 8)); free(out);
    int graph_cm = 0;
    for (uint32_t b = 0; b < get32(z + 20); b++) graph_cm |= z[HDR + (size_t)b * IDX + 12] == C_GRAPH;
    if (graph_cm && c.bytes[KOLMO_COST_MODEL]) {
        z[5] = KOLMO_FAST; fix_crc(z);
        r = decode_n(z, (size_t)zn, &out, 2, 0); free(out);
        printf("the same frame under a fast header -> %lld\n", (long long)r);
        assert(r == KOLMO_ERR_CORRUPT && kolmo_frame_cost(z, (size_t)zn, &c) == KOLMO_ERR_CORRUPT);
    } else printf("ultra level 2 wrote no context-mixing graph leaf here: header check covered by the vector only\n");
    free(z); free(v);
}

int main(void) {
    log_encoder_memory();
    formula_discovery_cap();
    query_threads();
    table_v1_rows();
    klzma_dist_wrap();
    qlfc_mtf();
    khuf_offsets();
    audio_chunk_cap();
    video_memory();
    fastq_threads();
    block_strictness();
    decode_cost();
    printf("test_decoder_hardening: ALL PASSED\n");
    return 0;
}
