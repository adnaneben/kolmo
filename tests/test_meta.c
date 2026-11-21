/* Metadata (src/plan/meta.c) and the ARM64 branch transform (src/transforms/arm64.c).
 *   - the ARM64 mapping is a bijection: random words, every BL and ADRP form, random phases; whole buffers round trip;
 *   - every header parser on a well-formed synthetic file returns the expected geometry or parts;
 *   - every parser on 3000 mutations of each file (bytes flipped in the header, lengths and offsets set to extremes,
 *     truncation): no crash or out-of-bounds read (run under the sanitizers), and the segments are ordered, contiguous
 *     and cover the input;
 *   - frames of each file round trip in fast, hc and max with 1 and 3 threads, and are the same at 1 and 3 threads;
 *     the ARM64 and AIFF frames are version 11 and are refused when their header says version 10. */
#include "../src/kolmo.c"
#include <stdio.h>

static uint64_t st = 0x2545F4914F6CDD1DULL;
static uint64_t rv(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }
#define FAIL(...) do { fprintf(stderr, "test_meta: " __VA_ARGS__); fprintf(stderr, " (line %d)\n", __LINE__); return 1; } while (0)

typedef struct { uint8_t *p; size_t n, cap; } buf;
static void put(buf *b, const void *s, size_t n) { if (b->n + n > b->cap) { b->cap = (b->n + n) * 2; b->p = realloc(b->p, b->cap); } memcpy(b->p + b->n, s, n); b->n += n; }
static void put8(buf *b, unsigned v) { uint8_t x = (uint8_t)v; put(b, &x, 1); }
static void put16(buf *b, unsigned v) { put8(b, v & 255); put8(b, v >> 8); }
static void bput32(buf *b, uint32_t v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }
static void bput64(buf *b, uint64_t v) { bput32(b, (uint32_t)v); bput32(b, (uint32_t)(v >> 32)); }
static void putbe16(buf *b, unsigned v) { put8(b, v >> 8); put8(b, v & 255); }
static void putbe32(buf *b, uint32_t v) { putbe16(b, v >> 16); putbe16(b, v & 0xFFFF); }
static void pad(buf *b, size_t to) { while (b->n < to) put8(b, 0); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* a smooth picture with some texture: rows of W x C bytes */
static void pixels(buf *b, size_t W, size_t H, unsigned C, size_t stride) {
    for (size_t y = 0; y < H; y++) { size_t s0 = b->n; for (size_t x = 0; x < W; x++) for (unsigned c = 0; c < C; c++) put8(b, (unsigned)((x * 3 + y * 2 + c * 40 + (rv() & 7)) & 255)); pad(b, s0 + stride); }
}
/* code-like words: BL and ADRP to a few targets, other words random from a small set */
static void a64_code(buf *b, size_t words) {
    static const uint32_t ops[6] = {0xD10043FFu, 0xA9BF7BFDu, 0x910003FDu, 0xF9400000u, 0xB9400020u, 0xD65F03C0u};
    for (size_t i = 0; i < words; i++) {
        uint32_t r = (uint32_t)rv(), w; size_t pc = b->n;
        if (r % 5 == 0) { uint32_t tgt = 0x10000u + (r >> 8) % 64 * 0x400u; w = 0x94000000u | ((tgt - (uint32_t)pc / 4) & 0x03FFFFFFu); }
        else if (r % 7 == 0) { uint32_t pg = 0x400u + (r >> 9) % 16; uint32_t d = pg - (uint32_t)(pc >> 12); w = 0x90000000u | (r & 31) | (d & 3) << 29 | ((d >> 2) & 0x7FFFF) << 5; }
        else w = ops[r % 6];
        bput32(b, w);
    }
}
static void x86_code(buf *b, size_t n) {
    size_t e = b->n + n;
    while (b->n + 5 < e) { uint32_t r = (uint32_t)rv(); if (r % 4 == 0) { put8(b, 0xE8); bput32(b, (uint32_t)((r >> 8) % 4096 * 16) - (uint32_t)b->n); } else put8(b, (uint8_t)(0x48 + r % 8)); }
    pad(b, e);
}
static void textish(buf *b, size_t n) { size_t e = b->n + n; while (b->n < e) { char l[64]; int k = snprintf(l, sizeof l, "key_%u = %u\n", (unsigned)(rv() % 300), (unsigned)(rv() % 1000)); put(b, l, b->n + (size_t)k <= e ? (size_t)k : e - b->n); } }

static buf mk_ppm(void) { buf b = {0}; const char *h = "P6\n# c\n509 300\n255\n"; put(&b, h, strlen(h)); pixels(&b, 509, 300, 3, 509 * 3); return b; }
static buf mk_pgm(void) { buf b = {0}; const char *h = "P5 777 400 255\n"; put(&b, h, strlen(h)); pixels(&b, 777, 400, 1, 777); return b; }
static buf mk_pam(void) { buf b = {0}; const char *h = "P7\nWIDTH 301\nHEIGHT 300\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n"; put(&b, h, strlen(h)); pixels(&b, 301, 300, 4, 1204); return b; }
static buf mk_bmp(void) {
    buf b = {0}; const unsigned W = 509, H = 300, row = (W * 3 + 3) / 4 * 4;
    put(&b, "BM", 2); bput32(&b, 54 + row * H); bput32(&b, 0); bput32(&b, 54);
    bput32(&b, 40); bput32(&b, W); bput32(&b, H); put16(&b, 1); put16(&b, 24); bput32(&b, 0); bput32(&b, row * H); bput32(&b, 2835); bput32(&b, 2835); bput32(&b, 0); bput32(&b, 0);
    pixels(&b, W, H, 3, row); return b;
}
static buf mk_tiff(int be) {
    buf b = {0}; const unsigned W = 403, H = 300, C = 3; const uint32_t px = W * H * C;
    if (be) { put(&b, "MM\0*", 4); putbe32(&b, 8 + px); } else { put(&b, "II*\0", 4); bput32(&b, 8 + px); }
    pixels(&b, W, H, C, W * C);
    const unsigned T[][4] = {{256, 4, 1, W}, {257, 4, 1, H}, {258, 3, 1, 8}, {259, 3, 1, 1}, {262, 3, 1, 2}, {273, 4, 1, 8}, {277, 3, 1, C}, {278, 4, 1, H}, {279, 4, 1, px}, {284, 3, 1, 1}};
    if (be) putbe16(&b, 10); else put16(&b, 10);
    for (int i = 0; i < 10; i++) {
        if (be) { putbe16(&b, T[i][0]); putbe16(&b, T[i][1]); putbe32(&b, T[i][2]); if (T[i][1] == 3) { putbe16(&b, T[i][3]); putbe16(&b, 0); } else putbe32(&b, T[i][3]); }
        else { put16(&b, T[i][0]); put16(&b, T[i][1]); bput32(&b, T[i][2]); if (T[i][1] == 3) { put16(&b, T[i][3]); put16(&b, 0); } else bput32(&b, T[i][3]); }
    }
    bput32(&b, 0); return b;
}
static buf mk_safetensors(void) {
    buf b = {0}; char h[512]; const size_t A = 300000, B = 400000;
    int k = snprintf(h, sizeof h, "{\"__metadata__\":{\"format\":\"pt\"},\"w.a\":{\"dtype\":\"BF16\",\"shape\":[%zu],\"data_offsets\":[0,%zu]},"
                     "\"m\\\"x\":{\"dtype\":\"U8\",\"shape\":[%zu],\"data_offsets\":[%zu,%zu]}}", A / 2, A, B, A, A + B);
    while (k % 8) h[k++] = ' ';
    bput64(&b, (uint64_t)k); put(&b, h, (size_t)k);
    for (size_t i = 0; i < A / 2; i++) put16(&b, 0x3C00 + (unsigned)(rv() % 512));
    for (size_t i = 0; i < B; i++) put8(&b, (rv() % 9) == 0);
    return b;
}
static void gstr(buf *b, const char *s) { bput64(b, strlen(s)); put(b, s, strlen(s)); }
static buf mk_gguf(void) {
    buf b = {0}; put(&b, "GGUF", 4); bput32(&b, 3); bput64(&b, 2); bput64(&b, 3);
    gstr(&b, "general.name"); bput32(&b, 8); gstr(&b, "t");
    gstr(&b, "general.alignment"); bput32(&b, 4); bput32(&b, 64);
    gstr(&b, "tok"); bput32(&b, 9); bput32(&b, 8); bput64(&b, 3); gstr(&b, "a"); gstr(&b, "bb"); gstr(&b, "ccc");
    gstr(&b, "f32"); bput32(&b, 1); bput64(&b, 100000); bput32(&b, 0); bput64(&b, 0);
    gstr(&b, "q8"); bput32(&b, 2); bput64(&b, 32); bput64(&b, 4000); bput32(&b, 8); bput64(&b, 400000);
    pad(&b, (b.n + 63) / 64 * 64); size_t base = b.n;
    for (size_t i = 0; i < 100000; i++) { float f = (float)((int)(rv() % 2001) - 1000) / 997.0f; uint32_t u; memcpy(&u, &f, 4); bput32(&b, u); }
    pad(&b, base + 400000);
    for (size_t i = 0; i < 4000; i++) { put16(&b, 0x2E66); for (int k = 0; k < 32; k++) put8(&b, (uint8_t)(int8_t)((int)(rv() % 61) - 30)); }
    return b;
}
static void npy_hdr(buf *b, const char *descr, const char *shape) {
    char d[128]; int k = snprintf(d, sizeof d, "{'descr': '%s', 'fortran_order': False, 'shape': %s, }", descr, shape);
    while ((10 + k + 1) % 64) d[k++] = ' ';
    d[k++] = '\n'; put(b, "\x93NUMPY\x01\x00", 8); put16(b, (unsigned)k); put(b, d, (size_t)k);
}
static buf mk_zip(int deflated) {   /* NPZ-like: an image array, an int16 series and (optionally) a member marked deflated */
    buf b = {0}; buf m[3] = {{0}, {0}, {0}}; const char *nm[3] = {"img.npy", "walk.npy", "z.bin"};
    npy_hdr(&m[0], "|u1", "(300, 509, 3)"); pixels(&m[0], 509, 300, 3, 509 * 3);
    npy_hdr(&m[1], "<i2", "(200000,)"); int v = 0; for (int i = 0; i < 200000; i++) { v += (int)(rv() % 601) - 300; if (v > 30000 || v < -30000) v = 0; put16(&m[1], (unsigned)(uint16_t)(int16_t)v); }
    for (int i = 0; i < 100000; i++) put8(&m[2], (uint8_t)rv());
    uint32_t lo[3];
    for (int i = 0; i < 3; i++) {
        lo[i] = (uint32_t)b.n; bput32(&b, 0x04034B50); put16(&b, 20); put16(&b, 0); put16(&b, i == 2 && deflated ? 8 : 0); bput32(&b, 0); bput32(&b, 0);
        bput32(&b, (uint32_t)m[i].n); bput32(&b, (uint32_t)m[i].n); put16(&b, (unsigned)strlen(nm[i])); put16(&b, 0); put(&b, nm[i], strlen(nm[i])); put(&b, m[i].p, m[i].n);
    }
    uint32_t cd = (uint32_t)b.n;
    for (int i = 0; i < 3; i++) {
        bput32(&b, 0x02014B50); put16(&b, 20); put16(&b, 20); put16(&b, 0); put16(&b, i == 2 && deflated ? 8 : 0); bput32(&b, 0); bput32(&b, 0);
        bput32(&b, (uint32_t)m[i].n); bput32(&b, (uint32_t)m[i].n); put16(&b, (unsigned)strlen(nm[i])); put16(&b, 0); put16(&b, 0); put16(&b, 0); put16(&b, 0); bput32(&b, 0); bput32(&b, lo[i]);
        put(&b, nm[i], strlen(nm[i]));
    }
    uint32_t cs = (uint32_t)b.n - cd;
    bput32(&b, 0x06054B50); put16(&b, 0); put16(&b, 0); put16(&b, 3); put16(&b, 3); bput32(&b, cs); bput32(&b, cd); put16(&b, 0);
    for (int i = 0; i < 3; i++) free(m[i].p);
    return b;
}
/* ELF64: header, data, code (.text), data, section table */
static buf mk_elf(unsigned mach) {
    buf b = {0}; const size_t D0 = 0x30000, CODE = 0x100000, D1 = 0x60000; const size_t co = 0x1000 + D0, d1 = co + CODE, sh = d1 + D1;
    put(&b, "\x7F" "ELF\x02\x01\x01", 7); pad(&b, 16); put16(&b, 3); put16(&b, mach); bput32(&b, 1); bput64(&b, 0); bput64(&b, 0); bput64(&b, sh);
    bput32(&b, 0); put16(&b, 64); put16(&b, 56); put16(&b, 0); put16(&b, 64); put16(&b, 4); put16(&b, 0);
    pad(&b, 0x1000); textish(&b, D0);
    if (mach == 183) a64_code(&b, CODE / 4); else x86_code(&b, CODE);
    textish(&b, D1);
    const uint64_t S[4][4] = {{0, 0, 0, 0}, {1, 2, 0x1000, D0}, {1, 6, co, CODE}, {8, 3, d1, D1}};   /* type, flags, offset, size; a NOBITS one */
    for (int i = 0; i < 4; i++) { bput32(&b, 0); bput32(&b, (uint32_t)S[i][0]); bput64(&b, S[i][1]); bput64(&b, 0); bput64(&b, S[i][2]); bput64(&b, S[i][3]); bput32(&b, 0); bput32(&b, 0); bput64(&b, 16); bput64(&b, 0); }
    return b;
}
static buf mk_macho(void) {
    buf b = {0}; const uint32_t CODE = 0x100000, co = 0x4000;
    bput32(&b, 0xFEEDFACF); bput32(&b, 0x0100000C); bput32(&b, 0); bput32(&b, 2); bput32(&b, 1); bput32(&b, 72 + 80); bput32(&b, 0); bput32(&b, 0);
    bput32(&b, 0x19); bput32(&b, 72 + 80); put(&b, "__TEXT\0\0\0\0\0\0\0\0\0\0", 16); bput64(&b, 0); bput64(&b, co + CODE); bput64(&b, 0); bput64(&b, co + CODE); bput32(&b, 5); bput32(&b, 5); bput32(&b, 1); bput32(&b, 0);
    put(&b, "__text\0\0\0\0\0\0\0\0\0\0", 16); put(&b, "__TEXT\0\0\0\0\0\0\0\0\0\0", 16); bput64(&b, co); bput64(&b, CODE); bput32(&b, co); bput32(&b, 2); bput32(&b, 0); bput32(&b, 0); bput32(&b, 0x80000400u); bput32(&b, 0); bput32(&b, 0); bput32(&b, 0);
    textish(&b, co - b.n); a64_code(&b, CODE / 4); textish(&b, 0x50000);
    return b;
}
static buf mk_pe(void) {
    buf b = {0}; const uint32_t CODE = 0x100000, co = 0x400;
    put(&b, "MZ", 2); pad(&b, 0x3C); bput32(&b, 0x80); pad(&b, 0x80); put(&b, "PE\0\0", 4); put16(&b, 0x8664); put16(&b, 2); bput32(&b, 0); bput32(&b, 0); bput32(&b, 0); put16(&b, 0); put16(&b, 0);
    put(&b, ".text\0\0\0", 8); bput32(&b, CODE); bput32(&b, 0x1000); bput32(&b, CODE); bput32(&b, co); bput32(&b, 0); bput32(&b, 0); put16(&b, 0); put16(&b, 0); bput32(&b, 0x60000020u);
    put(&b, ".rdata\0\0", 8); bput32(&b, 0x60000); bput32(&b, 0x200000); bput32(&b, 0x60000); bput32(&b, co + CODE); bput32(&b, 0); bput32(&b, 0); put16(&b, 0); put16(&b, 0); bput32(&b, 0x40000040u);
    pad(&b, co); x86_code(&b, CODE); textish(&b, 0x60000);
    return b;
}
static void tar_member(buf *b, const char *name, const uint8_t *p, size_t n) {
    uint8_t h[512] = {0}; snprintf((char *)h, 100, "%s", name); memcpy(h + 100, "0000644", 7); snprintf((char *)h + 124, 12, "%011llo", (unsigned long long)n);
    h[156] = '0'; memcpy(h + 257, "ustar\0" "00", 8); memset(h + 148, ' ', 8); unsigned s = 0; for (int i = 0; i < 512; i++) s += h[i]; snprintf((char *)h + 148, 8, "%06o", s);
    put(b, h, 512); put(b, p, n); pad(b, (b->n + 511) / 512 * 512);
}
static buf mk_tar(void) {
    buf b = {0}, t = {0}, img = mk_pgm(), elf = mk_elf(183);
    textish(&t, 5000); tar_member(&b, "a.txt", t.p, t.n); tar_member(&b, "p.pgm", img.p, img.n); tar_member(&b, "x", elf.p, elf.n);
    tar_member(&b, "b.txt", t.p, t.n); pad(&b, b.n + 1024);
    free(t.p); free(img.p); free(elf.p); return b;
}
static buf mk_aiff(int ch) {
    buf b = {0}, s = {0}; const uint32_t fr = 60000; int v[2] = {0, 0};
    for (uint32_t i = 0; i < fr; i++) for (int c = 0; c < ch; c++) { v[c] += (int)(rv() % 801) - 400; v[c] = v[c] * 31 / 32; putbe16(&s, (unsigned)(uint16_t)(int16_t)v[c]); }
    put(&b, "FORM", 4); putbe32(&b, (uint32_t)(4 + 8 + 18 + 8 + 8 + s.n)); put(&b, "AIFF", 4);
    put(&b, "COMM", 4); putbe32(&b, 18); putbe16(&b, (unsigned)ch); putbe32(&b, fr); putbe16(&b, 16); put(&b, "\x40\x0E\xAC\x44\0\0\0\0\0\0", 10);
    put(&b, "SSND", 4); putbe32(&b, (uint32_t)(8 + s.n)); putbe32(&b, 0); putbe32(&b, 0); put(&b, s.p, s.n);
    free(s.p); return b;
}
static buf mk_gzip(void) { buf b = {0}; put(&b, "\x1F\x8B\x08\0\0\0\0\0\0\x03", 10); for (int i = 0; i < 300000; i++) put8(&b, (uint8_t)rv()); return b; }

static int check_segs(const uint8_t *s, size_t n, const char *what) {
    enum { M = 64 }; uint64_t ss[M], se[M]; uint32_t kd[M];
    uint32_t k = meta_segments(s, n, ss, se, kd, M);
    if (!k || k > M || ss[0] != 0 || se[k - 1] != n) FAIL("%s: segments do not cover the input (%u)", what, k);
    for (uint32_t i = 0; i < k; i++) if ((n && ss[i] >= se[i]) || (i && ss[i] != se[i - 1])) FAIL("%s: segment %u out of order", what, i);
    size_t w, h; unsigned c; uint64_t a, e; meta_image8(s, n, &w, &c, &h); meta_aiff(s, n, &c, &a, &e); (void)meta_packed(s, n);
    return 0;
}
static int mutate(const buf *b0, const char *what) {
    uint8_t *m = malloc(b0->n);
    for (int it = 0; it < 3000; it++) {
        memcpy(m, b0->p, b0->n); size_t n = b0->n, hl = n < 4096 ? n : 4096;
        int k = 1 + (int)(rv() % 6);
        for (int q = 0; q < k; q++) {
            size_t at = (size_t)(rv() % hl); uint32_t r = (uint32_t)rv();
            switch (r % 5) {
                case 0: m[at] ^= (uint8_t)(1u << (r >> 8) % 8); break;
                case 1: m[at] = (uint8_t)(r >> 8); break;
                case 2: if (at + 4 <= n) wr32(m + at, 0xFFFFFFFFu - (r >> 8) % 4); break;
                case 3: if (at + 4 <= n) wr32(m + at, (uint32_t)(n + (r >> 8) % 64 - 32)); break;
                default: if (at + 8 <= n) { wr32(m + at, (uint32_t)(r >> 3)); wr32(m + at + 4, r % 3 ? 0 : 0x7FFFFFFFu); } break;
            }
        }
        if (rv() % 4 == 0) n = (size_t)(rv() % n) + 1;
        uint8_t *t = malloc(n); memcpy(t, m, n);        /* an exact-size copy, so the sanitizers see any read past the end */
        int r = check_segs(t, n, what); free(t);
        if (r) { free(m); return 1; }
    }
    free(m); return 0;
}
static int roundtrip(const buf *b, const char *what, int expect_v11) {
    static const kolmo_mode modes[3] = {KOLMO_FAST, KOLMO_HC, KOLMO_MAX};
    for (int mi = 0; mi < 3; mi++) {
        kolmo_params p1 = {modes[mi], 0, 1, 0, 1}, p3 = {modes[mi], 0, 3, 0, 1};
        size_t cap = kolmo_bound(b->n, &p1); uint8_t *f = malloc(cap), *g = malloc(cap), *o = malloc(b->n + 1);
        int64_t z = kolmo_compress(b->p, b->n, f, cap, &p1), z3 = kolmo_compress(b->p, b->n, g, cap, &p3);
        if (z <= 0 || z3 != z || memcmp(f, g, (size_t)z)) FAIL("%s mode %d: threads change the frame", what, (int)modes[mi]);
        if (kolmo_decompress(f, (size_t)z, o, b->n, 3) != (int64_t)b->n || memcmp(o, b->p, b->n)) FAIL("%s mode %d: round trip", what, (int)modes[mi]);
        (void)expect_v11;                                      /* every frame is version 12 now */
        if (f[4] != 12) FAIL("%s mode %d: version %d, expected 12", what, (int)modes[mi], f[4]);
        { f[4] = 10; uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, (size_t)get32(f + 20) * IDX); put32(f + 28, hc);
          if (kolmo_decompress(f, (size_t)z, o, b->n, 1) != KOLMO_ERR_LEGACY) FAIL("%s mode %d: a frame labelled version 10 was not refused as legacy", what, (int)modes[mi]); }
        free(f); free(g); free(o);
    }
    return 0;
}

int main(void) {
    /* ARM64 mapping: a bijection on random and on every branch-like word, at random phases */
    for (int i = 0; i < 2000000; i++) {
        uint32_t w = (uint32_t)rv(), pc = (uint32_t)rv() & ~3u;
        if (i % 3 == 1) w = 0x94000000u | (w & 0x03FFFFFFu);
        if (i % 3 == 2) w = (w & ~0x9F000000u) | 0x90000000u;
        uint32_t e = a64_map(w, pc, 1), d = a64_map(e, pc, 0);
        if (d != w) FAIL("a64 word %08x pc %08x: %08x -> %08x", w, pc, e, d);
    }
    {   buf c = {0}; a64_code(&c, 100000); put8(&c, 7); put8(&c, 9); uint8_t *x = malloc(c.n); memcpy(x, c.p, c.n);
        a64_fwd(x, c.n, 0x340); if (!memcmp(x, c.p, c.n)) FAIL("a64: no word changed");
        a64_inv(x, c.n, 0x340); if (memcmp(x, c.p, c.n)) FAIL("a64: buffer round trip");
        free(x); free(c.p); }

    struct { const char *name; buf b; int v11; } F[] = {
        {"ppm", mk_ppm(), 0}, {"pgm", mk_pgm(), 0}, {"pam", mk_pam(), 0}, {"bmp", mk_bmp(), 0}, {"tiff-le", mk_tiff(0), 0}, {"tiff-be", mk_tiff(1), 0},
        {"safetensors", mk_safetensors(), 0}, {"gguf", mk_gguf(), 0}, {"zip", mk_zip(0), 0}, {"zip-deflated", mk_zip(1), 0},
        {"elf-x86", mk_elf(62), 0}, {"elf-arm64", mk_elf(183), 1}, {"macho-arm64", mk_macho(), 1}, {"pe-x86", mk_pe(), 0}, {"tar", mk_tar(), 1},
        {"aiff-mono", mk_aiff(1), 1}, {"aiff-stereo", mk_aiff(2), 1}, {"gzip", mk_gzip(), 0},
    };
    const int NF = (int)(sizeof F / sizeof F[0]);
    /* the parsers on the well-formed files */
    size_t w, h; unsigned c;
    static const struct { int i; size_t w; unsigned c; } IMG[] = {{0, 509 * 3, 3}, {1, 777, 1}, {2, 1204, 4}, {3, 1528, 3}, {4, 1209, 3}, {5, 1209, 3}};
    for (int k = 0; k < 6; k++) { const buf *b = &F[IMG[k].i].b;
        if (!meta_image8(b->p, b->n, &w, &c, &h) || w != IMG[k].w || c != IMG[k].c || (h + w * 300 > b->n)) FAIL("%s: geometry %zu %u %zu", F[IMG[k].i].name, w, c, h); }
    { uint64_t a, e; unsigned ch; if (!meta_aiff(F[15].b.p, F[15].b.n, &ch, &a, &e) || ch != 1 || e - a != 120000) FAIL("aiff mono");
      if (!meta_aiff(F[16].b.p, F[16].b.n, &ch, &a, &e) || ch != 2 || e - a != 240000) FAIL("aiff stereo"); }
    if (!meta_packed(F[17].b.p, F[17].b.n) || meta_packed(F[0].b.p, F[0].b.n)) FAIL("packed magic");
    {   enum { M = 64 }; uint64_t ss[M], se[M]; uint32_t kd[M]; uint32_t k;
        static const struct { int i; uint32_t nseg; int kind; } SEG[] = {{6, 2, -1}, {7, 2, -1}, {8, 4, -1}, {9, 4, MK_PACKED}, {10, 3, MK_CODE}, {11, 3, MK_A64}, {12, 2, MK_A64}, {13, 2, MK_CODE}};
        for (int q = 0; q < (int)(sizeof SEG / sizeof SEG[0]); q++) {
            const buf *b = &F[SEG[q].i].b; k = meta_segments(b->p, b->n, ss, se, kd, M);
            if (k != SEG[q].nseg) FAIL("%s: %u segments, expected %u", F[SEG[q].i].name, k, SEG[q].nseg);
            int found = SEG[q].kind < 0; for (uint32_t i = 0; i < k; i++) found |= (int)(kd[i] & 0xFF) == SEG[q].kind;
            if (!found) FAIL("%s: no segment of kind %d", F[SEG[q].i].name, SEG[q].kind);
            for (uint32_t i = 0; i < k; i++) if ((kd[i] & 0xFF) == MK_A64 && (kd[i] >> 8) != (ss[i] & 0xFFF)) FAIL("%s: phase", F[SEG[q].i].name);
        }
        k = meta_segments(F[14].b.p, F[14].b.n, ss, se, kd, M);   /* tar: the image and the executable's parts on their own */
        if (k < 5) FAIL("tar: %u segments", k);
        k = meta_segments(F[17].b.p, F[17].b.n, ss, se, kd, M); if (k != 1 || kd[0] != MK_PACKED) FAIL("gzip: not packed");
    }
    for (int i = 0; i < NF; i++) if (check_segs(F[i].b.p, F[i].b.n, F[i].name) || mutate(&F[i].b, F[i].name)) return 1;
    for (int i = 0; i < NF; i++) if (roundtrip(&F[i].b, F[i].name, F[i].v11)) return 1;
    for (int i = 0; i < NF; i++) free(F[i].b.p);
    printf("test_meta: ok\n");
    return 0;
}
