/* krans-rivals: the entropy coders KRANS is measured against, on the same inputs, one thread.
 *
 *   huf      zstd's Huffman: HUF_compress4X_repeat / HUF_decompress4X_hufOnly_wksp on 128 KiB chunks (HUF's maximum)
 *   huf-f0   the same frames decoded with flags 0 (no BMI2 flag), as src/kolmo.c calls it
 *   fse      zstd's FSE (tANS): normalised count + FSE_compress_usingCTable on 128 KiB chunks, FSE_decompress_wksp_bmi2
 *   lz4      LZ4_compress_default / LZ4_decompress_safe, whole stream
 *   zstd1, zstd3   ZSTD_compressCCtx levels 1 and 3, whole stream, one thread
 *
 * Chunked coders store a 3-byte header per chunk (mode and coded length, as a zstd literals header would) and fall
 * back to raw (z = 0 from HUF / FSE) or one repeated byte. Needs a zstd source tree built separately: see the
 * build/krans-rivals rule in experimental.mk (ZSTD_DIR). Output: one JSON line per input and coder, sizes in bytes,
 * speeds in MB/s (10^6 bytes of input per second), the fastest of -r runs (at least 3). */
#define _POSIX_C_SOURCE 200809L
#define ZSTD_STATIC_LINKING_ONLY
#define FSE_STATIC_LINKING_ONLY
#include <lz4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cpu.h"
#include "fse.h"
#include "hist.h"
#include "huf.h"
#include "zstd.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

#define CH (128 * 1024)

static int g_bmi2;

/* chunk header: byte 0 mode (0 raw, 1 rle, 2 coded), bytes 1-2 coded length - 1 for mode 2 (<= 128 KiB + slack: 17 bits,
 * the top bit in mode bit 7) */
static size_t huf_enc(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    static U64 wk[HUF_WORKSPACE_SIZE_U64]; HUF_CREATE_STATIC_CTABLE(ct, HUF_SYMBOLVALUE_MAX);
    size_t o = 0;
    for (size_t i = 0; i < n; i += CH) {
        size_t m = n - i < CH ? n - i : CH;
        HUF_repeat rp = HUF_repeat_none;
        size_t z = HUF_compress4X_repeat(out + o + 3, cap - o - 3, in + i, m, 255, HUF_TABLELOG_DEFAULT, wk, sizeof wk, ct, &rp,
                                         g_bmi2 ? HUF_flags_bmi2 : 0);
        if (HUF_isError(z)) return 0;
        if (z == 1) { out[o] = 1; out[o + 1] = in[i]; o += 2; }
        else if (z == 0 || z >= m) { out[o] = 0; memcpy(out + o + 1, in + i, m); o += 1 + m; }
        else { out[o] = (uint8_t)(2 | ((z - 1) >> 16) << 7); out[o + 1] = (uint8_t)(z - 1); out[o + 2] = (uint8_t)((z - 1) >> 8); o += 3 + z; }
    }
    return o;
}
static int huf_dec(const uint8_t *in, size_t cn, uint8_t *out, size_t n, int flags) {
    static HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX); static U32 wk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32];
    size_t i = 0;
    for (size_t o = 0; o < n; o += CH) {
        size_t m = n - o < CH ? n - o : CH;
        if (i >= cn) return -1;
        int md = in[i] & 3;
        if (md == 1) { memset(out + o, in[i + 1], m); i += 2; }
        else if (md == 0) { memcpy(out + o, in + i + 1, m); i += 1 + m; }
        else {
            size_t z = ((size_t)(in[i] >> 7) << 16 | in[i + 1] | (size_t)in[i + 2] << 8) + 1;
            size_t r = HUF_decompress4X_hufOnly_wksp(dt, out + o, m, in + i + 3, z, wk, sizeof wk, flags);
            if (HUF_isError(r) || r != m) return -1;
            i += 3 + z;
        }
    }
    return i == cn ? 0 : -1;
}

static size_t fse_enc(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    static unsigned cnt[256]; static short norm[256]; static U32 hw[HIST_WKSP_SIZE_U32];
    static FSE_CTable ct[FSE_CTABLE_SIZE_U32(FSE_MAX_TABLELOG, 255)];
    static U32 bw[FSE_BUILD_CTABLE_WORKSPACE_SIZE_U32(255, FSE_MAX_TABLELOG)];
    size_t o = 0;
    for (size_t i = 0; i < n; i += CH) {
        size_t m = n - i < CH ? n - i : CH;
        unsigned maxs = 255;
        size_t mc = HIST_count_wksp(cnt, &maxs, in + i, m, hw, sizeof hw);
        if (HIST_isError(mc)) return 0;
        if (mc == m) { out[o] = 1; out[o + 1] = in[i]; o += 2; continue; }
        size_t z = 0;
        if (mc > (m >> 7) + 1) {   /* as zstd: not worth trying on nearly flat input */
            unsigned tl = FSE_optimalTableLog(FSE_MAX_TABLELOG, m, maxs);
            uint8_t *d = out + o + 3; size_t dc = cap - o - 3, k = 0;
            size_t r = FSE_normalizeCount(norm, tl, cnt, m, maxs, m >= 2048);
            if (!FSE_isError(r)) r = FSE_writeNCount(d, dc, norm, maxs, tl);
            if (!FSE_isError(r)) { k = r; r = FSE_buildCTable_wksp(ct, norm, maxs, tl, bw, sizeof bw); }
            if (!FSE_isError(r)) r = FSE_compress_usingCTable(d + k, dc - k, in + i, m, ct);
            if (!FSE_isError(r) && r) z = k + r;
        }
        if (z == 0 || z >= m) { out[o] = 0; memcpy(out + o + 1, in + i, m); o += 1 + m; }
        else { out[o] = (uint8_t)(2 | ((z - 1) >> 16) << 7); out[o + 1] = (uint8_t)(z - 1); out[o + 2] = (uint8_t)((z - 1) >> 8); o += 3 + z; }
    }
    return o;
}
static int fse_dec(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    static U32 wk[FSE_DECOMPRESS_WKSP_SIZE_U32(FSE_MAX_TABLELOG, 255)];
    size_t i = 0;
    for (size_t o = 0; o < n; o += CH) {
        size_t m = n - o < CH ? n - o : CH;
        if (i >= cn) return -1;
        int md = in[i] & 3;
        if (md == 1) { memset(out + o, in[i + 1], m); i += 2; }
        else if (md == 0) { memcpy(out + o, in + i + 1, m); i += 1 + m; }
        else {
            size_t z = ((size_t)(in[i] >> 7) << 16 | in[i + 1] | (size_t)in[i + 2] << 8) + 1;
            size_t r = FSE_decompress_wksp_bmi2(out + o, m, in + i + 3, z, FSE_MAX_TABLELOG, wk, sizeof wk, g_bmi2);
            if (FSE_isError(r) || r != m) return -1;
            i += 3 + z;
        }
    }
    return i == cn ? 0 : -1;
}

static ZSTD_CCtx *zc; static ZSTD_DCtx *zd;
static size_t zstd_enc(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int lvl) {
    size_t r = ZSTD_compressCCtx(zc, out, cap, in, n, lvl);
    return ZSTD_isError(r) ? 0 : r;
}
static int zstd_dec(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t r = ZSTD_decompressDCtx(zd, out, n, in, cn);
    return ZSTD_isError(r) || r != n ? -1 : 0;
}

int main(int argc, char **argv) {
    int runs = 3, a = 1;
    if (a + 1 < argc && !strcmp(argv[a], "-r")) { runs = atoi(argv[a + 1]); a += 2; }
    if (runs < 3) runs = 3;
    if (a >= argc) { fprintf(stderr, "usage: krans-rivals [-r runs] files...\n"); return 2; }
    g_bmi2 = ZSTD_cpuid_bmi2(ZSTD_cpuid());
    zc = ZSTD_createCCtx(); zd = ZSTD_createDCtx();
    const char *names[] = {"huf", "huf-f0", "fse", "lz4", "zstd1", "zstd3"};
    for (; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); continue; }
        fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *in = malloc(n + 1);
        if (fread(in, 1, n, f) != n) { fclose(f); free(in); continue; }
        fclose(f);
        size_t cap = ZSTD_compressBound(n) + n / 64 + 4096; if ((size_t)LZ4_compressBound((int)n) > cap) cap = (size_t)LZ4_compressBound((int)n);
        uint8_t *c = malloc(cap), *d = malloc(n + 1);
        const char *nm = strrchr(argv[a], '/'); nm = nm ? nm + 1 : argv[a];
        for (int k = 0; k < 6; k++) {
            double te = 1e30, td = 1e30; size_t z = 0; int ok = 1;
            if (k == 1) {   /* huf frames again, timed decode with flags 0 */
                z = huf_enc(in, n, c, cap); te = 0;
            } else for (int r = 0; r < runs; r++) {
                double t = now();
                switch (k) {
                case 0: z = huf_enc(in, n, c, cap); break;
                case 2: z = fse_enc(in, n, c, cap); break;
                case 3: z = (size_t)LZ4_compress_default((const char *)in, (char *)c, (int)n, (int)cap); break;
                case 4: z = zstd_enc(in, n, c, cap, 1); break;
                default: z = zstd_enc(in, n, c, cap, 3); break;
                }
                t = now() - t; if (t < te) te = t;
            }
            if (!z && n) { printf("{\"file\":\"%s\",\"coder\":\"%s\",\"error\":\"compress\"}\n", nm, names[k]); continue; }
            for (int r = 0; r < runs; r++) {
                memset(d, 0, n); int e;
                double t = now();
                switch (k) {
                case 0: e = huf_dec(c, z, d, n, g_bmi2 ? HUF_flags_bmi2 : 0); break;
                case 1: e = huf_dec(c, z, d, n, 0); break;
                case 2: e = fse_dec(c, z, d, n); break;
                case 3: e = LZ4_decompress_safe((const char *)c, (char *)d, (int)z, (int)n) == (int)n ? 0 : -1; break;
                default: e = zstd_dec(c, z, d, n); break;
                }
                t = now() - t;
                if (e || memcmp(in, d, n)) ok = 0;
                if (t < td) td = t;
            }
            double mb = n / 1e6;
            printf("{\"file\":\"%s\",\"coder\":\"%s\",\"size\":%zu,\"csize\":%zu,\"ratio\":%.4f,\"enc_mbs\":%.1f,\"dec_mbs\":%.1f,\"bmi2\":%d,\"verified\":%s}\n",
                   nm, names[k], n, z, n ? (double)z / n : 0, k == 1 || te <= 0 ? 0 : mb / te, td > 0 ? mb / td : 0, g_bmi2, ok ? "true" : "false");
            fflush(stdout);
        }
        free(in); free(c); free(d);
    }
    ZSTD_freeCCtx(zc); ZSTD_freeDCtx(zd);
    return 0;
}
