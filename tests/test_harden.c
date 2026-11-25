/* Hostile-input and API-contract regressions (public API only):
 *  - kolmo_original_size checks the header and index (checksum, block sizes adding up, sizes that fit int64_t)
 *    before a caller allocates what the header claims; kolmo_check_frame applies max_output_bytes;
 *  - the decoders take exactly one frame (trailing bytes are refused unless they are container extensions), and
 *    kolmo_frame_size walks concatenated frames;
 *  - kolmo_params out of range are refused with KOLMO_ERR_PARAM; kolmo_read_range past the end is KOLMO_ERR_RANGE;
 *  - a context-mixing frame whose block size was forged upwards fails fast instead of decoding padding to the claim;
 *  - kolmo_release_buffers leaves the library usable;
 *  - kolmo_stream_decompress reads every member of a file (CZNS streams and CZN1 frames, FORMAT.md 10.1) and refuses
 *    any other bytes after one: 0.0.2 returned success at the first stream terminator and dropped the rest. */
#include "kolmo.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void p32(uint8_t *p, uint32_t v) { for (int k = 0; k < 4; k++) p[k] = (uint8_t)(v >> 8 * k); }
static void p64(uint8_t *p, uint64_t v) { p32(p, (uint32_t)v); p32(p + 4, (uint32_t)(v >> 32)); }
static uint32_t g32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void fix_crc(uint8_t *s) {          /* header CRC over bytes 0..27 and the index */
    uint32_t nb = g32(s + 20), hc = kolmo_crc32c(0, s, 28); hc = kolmo_crc32c(hc, s + 32, (size_t)nb * 16); p32(s + 28, hc);
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *make_text(size_t n) {
    static const char *w[] = {"the ", "quick ", "brown ", "fox ", "jumps ", "over ", "lazy ", "dog ", "and ", "runs\n"};
    uint8_t *p = malloc(n); uint64_t s = 88172645463325252ull; size_t i = 0;
    while (i < n) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; const char *q = w[s % 10]; while (*q && i < n) p[i++] = (uint8_t)*q++; }
    return p;
}
static int64_t pack(const uint8_t *in, size_t n, kolmo_mode m, uint8_t **out) {
    kolmo_params p = {m, 0, 1, 0, 1, 0};
    size_t cap = kolmo_bound(n, &p); *out = malloc(cap); assert(*out);
    int64_t z = kolmo_compress(in, n, *out, cap, &p); assert(z > 0); return z;
}

static void forged_headers(void) {
    size_t n = 50000; uint8_t *in = make_text(n), *z; int64_t zn = pack(in, n, KOLMO_FAST, &z);
    uint8_t *b = malloc((size_t)zn + 64), *o = malloc(n);
    assert(kolmo_original_size(z, (size_t)zn) == (int64_t)n);
    /* header and index only: still the size (callers read that much first) */
    size_t hi = 32 + 16 * (size_t)g32(z + 20);
    assert(kolmo_original_size(z, hi) == (int64_t)n);
    assert(kolmo_original_size(z, 32) == KOLMO_ERR_CORRUPT || g32(z + 20) == 0);   /* the index is not there */
    /* a claim of 40 GiB without fixing the CRC: refused by the checksum */
    memcpy(b, z, (size_t)zn); p64(b + 12, 40ull << 30);
    assert(kolmo_original_size(b, (size_t)zn) == KOLMO_ERR_CHECKSUM);
    assert(kolmo_check_frame(b, (size_t)zn, NULL) == KOLMO_ERR_CHECKSUM);
    /* the same with the CRC fixed: block sizes no longer add up to the claim */
    fix_crc(b);
    assert(kolmo_original_size(b, (size_t)zn) == KOLMO_ERR_CORRUPT);
    assert(kolmo_check_frame(b, (size_t)zn, NULL) == KOLMO_ERR_CORRUPT);
    /* the 32-byte blob of the review: header only, claiming 40 GiB in zero blocks... */
    uint8_t h[48] = {'C', 'Z', 'N', '1', 12, 1}; p64(h + 12, 40ull << 30); p32(h + 20, 0); fix_crc(h);
    assert(kolmo_original_size(h, 32) == KOLMO_ERR_CORRUPT);
    /* ...or in one block whose index entry is missing (the CRC covers the 16 bytes after the header) */
    p32(h + 20, 1); fix_crc(h); assert(kolmo_original_size(h, 32) == KOLMO_ERR_CORRUPT);
    /* a claim of 2^63 or more is not an error code in disguise */
    memcpy(b, z, (size_t)zn); p64(b + 12, ~0ull - 12); fix_crc(b);
    assert(kolmo_original_size(b, (size_t)zn) == KOLMO_ERR_CORRUPT);
    /* a consistent huge claim (1024 x 256 MiB blocks in a 21 KB frame, the third review's bomb) passes the
     * structural checks (such ratios are valid: long runs) and is refused by max_output_bytes */
    {
        uint32_t nb = 1024, raw = 256u << 20; size_t fn = 32 + 16 * (size_t)nb + 5 * (size_t)nb;
        uint8_t *f = calloc(fn, 1); memcpy(f, "CZN1", 4); f[4] = 12; f[5] = 1; p64(f + 12, (uint64_t)nb * raw); p32(f + 20, nb); p32(f + 24, raw);
        for (uint32_t i = 0; i < nb; i++) { uint8_t *e = f + 32 + 16 * (size_t)i; p32(e, raw); p32(e + 4, 5); e[12] = 28; f[32 + 16 * (size_t)nb + 5 * (size_t)i + 4] = 7; }
        fix_crc(f);
        assert(kolmo_original_size(f, fn) == (int64_t)nb * raw);
        kolmo_decode_limits L = {1u << 30, 0, 0, 0};
        assert(kolmo_check_frame(f, fn, &L) == KOLMO_ERR_LIMIT);
        uint8_t small[64]; assert(kolmo_decompress(f, fn, small, sizeof small, 1) == KOLMO_ERR_DSTSIZE);
        free(f);
    }
    free(in); free(z); free(b); free(o);
}

static void trailing_and_concatenation(void) {
    size_t n1 = 30000, n2 = 7000; uint8_t *a = make_text(n1), *c = make_text(n2), *za, *zc;
    int64_t na = pack(a, n1, KOLMO_FAST, &za), nc = pack(c, n2, KOLMO_HC, &zc);
    uint8_t *cat = malloc((size_t)(na + nc) + 16), *o = malloc(n1 + n2);
    memcpy(cat, za, (size_t)na); memcpy(cat + na, zc, (size_t)nc);
    assert(kolmo_frame_size(za, (size_t)na) == na);
    assert(kolmo_frame_size(cat, (size_t)(na + nc)) == na);
    assert(kolmo_frame_size(cat + na, (size_t)nc) == nc);
    assert(kolmo_frame_size(za, 32 + 16 * (size_t)g32(za + 20)) == na);   /* from the header and index alone */
    /* one frame per call: the concatenation and trailing garbage are refused, not silently truncated */
    assert(kolmo_decompress(cat, (size_t)(na + nc), o, n1 + n2, 1) == KOLMO_ERR_CORRUPT);
    assert(kolmo_check_frame(cat, (size_t)(na + nc), NULL) == KOLMO_ERR_CORRUPT);
    assert(kolmo_read_range(cat, (size_t)(na + nc), 0, 10, o, 1) == KOLMO_ERR_CORRUPT);
    memcpy(cat + na, "GARBAGE", 7);
    assert(kolmo_decompress(cat, (size_t)na + 7, o, n1, 1) == KOLMO_ERR_CORRUPT);
    /* walking the concatenation frame by frame decodes both */
    memcpy(cat + na, zc, (size_t)nc);
    size_t at = 0, out = 0;
    while (at < (size_t)(na + nc)) {
        int64_t fs = kolmo_frame_size(cat + at, (size_t)(na + nc) - at); assert(fs > 0);
        int64_t r = kolmo_decompress(cat + at, (size_t)fs, o + out, n1 + n2 - out, 1); assert(r > 0);
        at += (size_t)fs; out += (size_t)r;
    }
    assert(out == n1 + n2 && !memcmp(o, a, n1) && !memcmp(o + n1, c, n2));
    /* a frame with container extensions (an indexed table) is one frame, extensions included */
    {
        size_t tn = 0, cap = 1 << 20; char *t = malloc(cap);
        tn += (size_t)snprintf(t, cap, "id,name,score\n");
        for (int i = 0; i < 5000; i++) tn += (size_t)snprintf(t + tn, cap - tn, "%d,item%d,%d\n", i, i % 37, i * 7 % 101);
        kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1, 0}; size_t zc2 = kolmo_indexed_bound(tn, &p); uint8_t *zi = malloc(zc2);
        int64_t zn = kolmo_compress_indexed((uint8_t *)t, tn, zi, zc2, &p, NULL); assert(zn > 0);
        if (zn > 0) {
            uint8_t *back = malloc(tn);
            assert(kolmo_frame_size(zi, (size_t)zn) == zn);
            assert(kolmo_check_frame(zi, (size_t)zn, NULL) == 0);
            assert(kolmo_decompress(zi, (size_t)zn, back, tn, 1) == (int64_t)tn && !memcmp(back, t, tn));
            free(back);
        }
        free(zi); free(t);
    }
    free(a); free(c); free(za); free(zc); free(cat); free(o);
}

static void params(void) {
    uint8_t in[4096]; for (int i = 0; i < 4096; i++) in[i] = (uint8_t)(i * 7 % 13);
    uint8_t *z = malloc(1 << 16);
    struct { int mode, level, transforms; int ok; } T[] = {
        {0, 0, 1, 1}, {1, 0, 1, 1}, {2, 0, 1, 1}, {3, 19, 1, 1}, {4, 10, 1, 1}, {5, 2, 1, 1}, {1, 22, 0, 1}, {0, 0, 3, 1},
        {1, 0, 2 | KOLMO_T_NATIVE_LEAF, 1}, {1, 0, 1 | KOLMO_T_TYPED_HUF | KOLMO_T_COLUMNS, 1}, {4, 308, 1, 1},
        {-1, 0, 1, 0}, {6, 0, 1, 0}, {1000, 0, 1, 0}, {1, -1, 1, 1}, {1, -2, 1, 0}, {0, -1, 1, 0}, {1, 23, 1, 0}, {1, 99, 1, 0}, {3, 308, 1, 0},
        {1, 0, -1, 0}, {1, 0, 4, 0}, {1, 0, 7, 0}, {1, 0, 255, 0}, {1, 0, 8, 0}};
    for (size_t i = 0; i < sizeof T / sizeof *T; i++) {
        kolmo_params p = {(kolmo_mode)T[i].mode, T[i].level, 1, 0, T[i].transforms, 0};
        int64_t r = kolmo_compress(in, sizeof in, z, 1 << 16, &p);
        if (T[i].ok ? r <= 0 : r != KOLMO_ERR_PARAM) { fprintf(stderr, "params %zu: mode %d level %d transforms %d -> %lld\n", i, T[i].mode, T[i].level, T[i].transforms, (long long)r); assert(0); }
    }
    kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1, 0}; int64_t zn = kolmo_compress(in, sizeof in, z, 1 << 16, &p); assert(zn > 0);
    uint8_t o[16];
    assert(kolmo_read_range(z, (size_t)zn, 5000, 4, o, 1) == KOLMO_ERR_RANGE);
    assert(kolmo_read_range(z, (size_t)zn, 4096, 4, o, 1) == 0);
    assert(kolmo_read_range(z, (size_t)zn, 4094, 4, o, 1) == 2 && !memcmp(o, in + 4094, 2));
    free(z);
}

/* ultra (context mixing) frame of a small text whose block size and stream length are forged to 8 MiB: before the
 * input-exhaustion check this decoded padding for seconds before failing its checksum */
static void cm_amplification(void) {
    size_t n = 3000; uint8_t *in = make_text(n), *z; int64_t zn = pack(in, n, KOLMO_ULTRA, &z);
    assert(g32(z + 20) == 1);
    uint8_t *e = z + 32; int codec = e[12];
    if (codec == 7) {                                  /* codec 7: size word at payload + 4 (after the plan bytes) */
        uint32_t N = 8u << 20; uint8_t *pay = z + 48;
        p64(z + 12, N); p32(e, N);
        uint32_t w = g32(pay + 4); p32(pay + 4, (w & 0xF0000000u) | N);
        z[11] = 100;                                   /* a declared figure that admits the larger model */
        fix_crc(z);
        uint8_t *o = malloc(N); double t0 = now();
        int64_t r = kolmo_decompress(z, (size_t)zn, o, N, 1);
        double dt = now() - t0;
        printf("  forged 8 MiB context-mixing block (%lld-byte frame): %lld after %.3f s\n", (long long)zn, (long long)r, dt);
        assert(r < 0 && dt < 2.0);
        free(o);
    } else printf("  (ultra chose codec %d for the sample; context-mixing amplification case skipped)\n", codec);
    free(in); free(z);
}

static void release(void) {
    size_t n = 3u << 20; uint8_t *in = make_text(n), *o = malloc(n);
    kolmo_params p = {KOLMO_HC, 0, 4, 1u << 18, 1, 0}; size_t cap = kolmo_bound(n, &p); uint8_t *z = malloc(cap);
    for (int k = 0; k < 2; k++) {
        int64_t zn = kolmo_compress(in, n, z, cap, &p); assert(zn > 0);
        kolmo_release_buffers();
        assert(kolmo_decompress(z, (size_t)zn, o, n, 4) == (int64_t)n && !memcmp(o, in, n));
        kolmo_release_buffers();
    }
    free(in); free(o); free(z);
}

/* a file of bytes on a descriptor, read from the start */
static int fd_of(const uint8_t *b, size_t n) {
    FILE *f = tmpfile(); assert(f); int fd = dup(fileno(f)); fclose(f); assert(fd >= 0);
    assert(write(fd, b, n) == (ssize_t)n); assert(lseek(fd, 0, SEEK_SET) == 0); return fd;
}
static uint8_t *slurp(int fd, size_t *n) {
    off_t e = lseek(fd, 0, SEEK_END); uint8_t *b = malloc(e ? (size_t)e : 1); assert(b && lseek(fd, 0, SEEK_SET) == 0);
    assert(read(fd, b, (size_t)e) == (ssize_t)e); *n = (size_t)e; return b;
}
/* decode a file through kolmo_stream_decompress; *out (malloc'ed) holds the output */
static int64_t sdecode(const uint8_t *b, size_t n, uint8_t **out, size_t *on) {
    int in = fd_of(b, n), o = fd_of((const uint8_t *)"", 0);
    int64_t r = kolmo_stream_decompress(in, o, 2);
    *out = slurp(o, on); close(in); close(o); return r;
}
static void stream_members(void) {
    size_t n1 = 3 << 20, n2 = 70000; uint8_t *a = make_text(n1), *c = make_text(n2), *zf;
    int64_t nf = pack(c, n2, KOLMO_FAST, &zf);
    int ia = fd_of(a, n1), sa = fd_of((const uint8_t *)"", 0);
    kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1, 0};
    assert(kolmo_stream_compress(ia, sa, &p, 1) > 0); close(ia);       /* 1 MiB segments: a stream of 3 frames */
    size_t ns; uint8_t *zs = slurp(sa, &ns); close(sa);
    uint8_t *f = malloc(3 * ns + 2 * (size_t)nf + 64), *o; size_t on;
    /* stream, frame, stream, frame: every member, in order */
    size_t k = 0;
    memcpy(f + k, zs, ns); k += ns; memcpy(f + k, zf, (size_t)nf); k += (size_t)nf;
    memcpy(f + k, zs, ns); k += ns; memcpy(f + k, zf, (size_t)nf); k += (size_t)nf;
    assert(sdecode(f, k, &o, &on) == (int64_t)(2 * (n1 + n2)) && on == 2 * (n1 + n2));
    assert(!memcmp(o, a, n1) && !memcmp(o + n1, c, n2) && !memcmp(o + n1 + n2, a, n1) && !memcmp(o + 2 * n1 + n2, c, n2));
    free(o);
    /* a frame first, then a stream */
    memcpy(f, zf, (size_t)nf); memcpy(f + nf, zs, ns);
    assert(sdecode(f, (size_t)nf + ns, &o, &on) == (int64_t)(n1 + n2) && !memcmp(o, c, n2) && !memcmp(o + n2, a, n1)); free(o);
    /* anything after a member that starts no member is refused, never dropped */
    static const char *const tails[] = {"\0", "GARBAGE", "CZNS", "CZN1", "\0\0\0\0\0\0\0\0"};
    static const size_t tl[] = {1, 7, 4, 4, 8};
    for (int t = 0; t < 5; t++) {
        memcpy(f, zs, ns); memcpy(f + ns, tails[t], tl[t]);
        assert(sdecode(f, ns + tl[t], &o, &on) == KOLMO_ERR_CORRUPT); free(o);
        memcpy(f, zf, (size_t)nf); memcpy(f + nf, tails[t], tl[t]);
        assert(sdecode(f, (size_t)nf + tl[t], &o, &on) == KOLMO_ERR_CORRUPT); free(o);
    }
    memcpy(f, zs, ns); memcpy(f + ns, zs, ns - 1);                        /* a second stream cut short */
    assert(sdecode(f, 2 * ns - 1, &o, &on) == KOLMO_ERR_CORRUPT); free(o);
    assert(sdecode(zs, ns - 8, &o, &on) == KOLMO_ERR_CORRUPT); free(o);   /* no terminator */
    assert(sdecode(zs, 0, &o, &on) == KOLMO_ERR_CORRUPT); free(o);        /* no member at all */
    /* the header's reserved bytes and segment_bytes are ignored (COMPATIBILITY.md); its version is not */
    memcpy(f, zs, ns); f[5] = 7; f[6] = 1; f[7] = 0xFF; memset(f + 8, 0, 4);
    assert(sdecode(f, ns, &o, &on) == (int64_t)n1 && !memcmp(o, a, n1)); free(o);
    f[4] = 2; assert(sdecode(f, ns, &o, &on) == KOLMO_ERR_CORRUPT); free(o);
    /* a frame with container extensions (an indexed table), then a stream */
    {
        size_t tn = 0, cap = 1 << 20; char *t = malloc(cap);
        tn += (size_t)snprintf(t, cap, "id,name,score\n");
        for (int i = 0; i < 5000; i++) tn += (size_t)snprintf(t + tn, cap - tn, "%d,item%d,%d\n", i, i % 37, i * 7 % 101);
        size_t zc2 = kolmo_indexed_bound(tn, &p); uint8_t *zi = malloc(zc2 + ns);
        int64_t zn = kolmo_compress_indexed((uint8_t *)t, tn, zi, zc2, &p, NULL); assert(zn > 0);
        memcpy(zi + zn, zs, ns);
        assert(sdecode(zi, (size_t)zn + ns, &o, &on) == (int64_t)(tn + n1) && !memcmp(o, t, tn) && !memcmp(o + tn, a, n1)); free(o);
        free(zi); free(t);
    }
    free(a); free(c); free(zf); free(zs); free(f);
}

/* a streamed tar ends its segments at member headers (a pax header stays with its member), at the segment size inside
 * a member longer than a segment, and decodes to the input */
static size_t tar_hdr(uint8_t *h, const char *name, size_t size, char type) {
    memset(h, 0, 512); snprintf((char *)h, 100, "%s", name); memcpy(h + 100, "0000644", 7);
    snprintf((char *)h + 124, 12, "%011zo", size); h[156] = (uint8_t)type; memcpy(h + 257, "ustar\0" "00", 8);
    memset(h + 148, ' ', 8); unsigned sum = 0; for (int i = 0; i < 512; i++) sum += h[i];
    snprintf((char *)h + 148, 8, "%06o", sum); h[155] = ' ';
    return 512 + (size + 511) / 512 * 512;
}
static void stream_tar_cuts(void) {
    static const size_t sz[] = {300000, 900000, 2621440 + 1000, 200000, 600000, 400000, 700000};
    size_t cap = 8 << 20, n = 0, nh = 0, hdr[16]; uint8_t *t = calloc(cap, 1), *txt = make_text(3 << 20);
    for (int i = 0; i < 7; i++) {
        char nm[32]; hdr[nh++] = n;
        if (i == 3) n += tar_hdr(t + n, "PaxHeader", 0, 'x');      /* the cut goes before the pax header */
        snprintf(nm, sizeof nm, "member%d", i); size_t at = n + 512; n += tar_hdr(t + n, nm, sz[i], '0');
        memcpy(t + at, txt + 4096 * i, sz[i]);
    }
    n += 1024;                                                        /* end-of-archive blocks */
    int in = fd_of(t, n), so = fd_of((const uint8_t *)"", 0);
    kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1, 0};
    assert(kolmo_stream_compress(in, so, &p, 1) > 0); close(in);
    size_t zn; uint8_t *z = slurp(so, &zn); close(so);
    size_t pos = 12, at = 0, prev = 0, oncut = 0; int frames = 0;
    for (;;) {
        uint64_t len = 0; for (int k = 7; k >= 0; k--) len = len << 8 | z[pos + k];
        pos += 8; if (!len) break;
        int64_t o = kolmo_original_size(z + pos, (size_t)len); assert(o > 0 && o <= (1 << 20));
        pos += (size_t)len; at += (size_t)o; frames++;
        if (at < n) {                                                 /* every cut: a member's first header, or a full segment */
            int hit = 0; for (size_t i = 0; i < nh; i++) hit |= hdr[i] == at;
            assert(hit || at - prev == (1u << 20)); oncut += hit;
        }
        prev = at;
    }
    assert(at == n && pos == zn && oncut >= 4 && frames >= 6);
    uint8_t *o; size_t on;
    assert(sdecode(z, zn, &o, &on) == (int64_t)n && on == n && !memcmp(o, t, n));
    free(o); free(z); free(t); free(txt);
}

int main(void) {
    forged_headers();
    trailing_and_concatenation();
    params();
    cm_amplification();
    release();
    stream_members();
    stream_tar_cuts();
    puts("HARDENING TESTS PASSED (forged headers, trailing bytes, frame walking, parameters, CM amplification, buffer release, stream members, stream tar cuts)");
    return 0;
}
