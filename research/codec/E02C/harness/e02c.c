/* E02C: residual representation experiment for the KG_INT integer graph.
 *
 * Research-only translation unit. It includes the REAL core (as tests/test_planner_ownership.c does) so the
 * production predictor selection, signedness, bias, GCD, seeds, planner, block splitting, leaf coders (zstd's HUF
 * via huf_compress_all, the static rANS via rans_compress_all, zstd) and the KG_INT inverse are the shipped
 * code paths. Nothing in src/ is changed and nothing here is reachable from the production library or CLI.
 *
 * Residuals are captured by decoding the residual leaf of a payload produced by the unchanged kg_encode_num (regime
 * A, explicitly typed input) or of a block in a frame produced by the unchanged kolmo_compress (regime B, complete
 * planner). The captured byte stream is exactly what kg_int_inv consumes.
 *
 * Experimental leaf framing (NOT CZN1/CZNS, NOT a KG leaf codec id; never written into a production frame):
 *   tag u8 (0xE0 | rep) | rep header | streams
 *   rep 0 EXIST  existing residual byte layout                   streams: residual bytes
 *   rep 1 GBP    global bitpacking            header: b u8      streams: packed
 *   rep 2 LBP    local bitpacking, 128 values                    streams: widths (u8 per segment), packed
 *   rep 3 GPP    global patched packing       header: b u8, exceptions uv
 *                                                                streams: packed lows, position gaps uv, highs uv
 *   rep 4 LPP    local patched, 128 values                       streams: widths u8, exception counts u8, packed
 *                                                                lows, positions u8 (in segment), highs uv
 *   rep 5 PLANES existing residual bytes, one stream per byte plane (ow streams of m bytes when the layout is planar,
 *                else the single stream): the plane-aware control, so each backend builds one table per plane
 *   stream: codec u8 (0 stored, 3 zstd, 13 Huffman only, 14 rANS) | raw length uv | coded length uv | bytes
 * Production-leaf-rule variant (follow-up 2, exist/planes only): tag 0xD0 | rep, then the output of the unchanged
 * kg_serialize for a leaf-only program ('G' 1, 0 nodes, leaves 1..k = the streams), so each stream gets production's
 * own leaf decision (sampling, Huffman/rANS trials and thresholds, stored fallback) at the given zstd level.
 * Count m, output width and plane flag come from the KG_INT parameters, as for a production leaf.
 * Packing is LSB first; padding bits are zero; a coded stream is always smaller than its raw length (else stored).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <malloc.h>
#include "../../../../src/kolmo.c"

#define SEG 128
enum { R_EXIST, R_GBP, R_LBP, R_GPP, R_LPP, R_PLANES, R_N };
#define R_SEL 5                           /* the size selector chooses among the first five representations (as in the first E02C run) */
enum { B_STORE, B_HUF, B_RANS, B_ZSTD, B_N, B_ENT = B_N, B_Z19, B_PROD3, B_PROD19 };   /* B_ENT: per stream, both existing entropy coders tried, smaller kept (both charged) */
static const char *RN[R_N] = {"exist", "gbp", "lbp128", "gpp", "lpp128", "planes"};
static const char *BN[B_N + 4] = {"store", "huf", "rans", "zstd3", "ent", "zstd19", "prod3", "prod19"};
static const int BC[B_N] = {C_STORE, C_HUF, C_RANS, C_ZSTD};
#define ZSTD_CONTROL_LEVEL 3
#define NFIX (R_SEL * B_N)              /* configurations 0-19: fixed; 20-23: charged selector per backend; 24-27: planes (appended, indices kept) */
/* appended configurations 28-36: best-of-entropy (28-30), then level-matched zstd -19 and production's leaf rule
 * at levels 3 and 19 (31-36), for the existing layout and the plane layout */
static const int XREP[9] = {R_EXIST, R_PLANES, R_LPP, R_EXIST, R_PLANES, R_EXIST, R_PLANES, R_EXIST, R_PLANES};
static const int XBE[9] = {B_ENT, B_ENT, B_ENT, B_Z19, B_Z19, B_PROD3, B_PROD3, B_PROD19, B_PROD19};
#define NCONF (NFIX + B_N + B_N + 9)

static int uvlen(uint64_t v) { int l = 1; while (v >= 128) { v >>= 7; l++; } return l; }
static int e2bitlen(uint64_t z) { return z ? 64 - __builtin_clzll(z) : 0; }
static uint64_t lomask(int b) { return b >= 64 ? ~0ull : ((1ull << b) - 1); }
static const char *codec_name(int c) { return c == -1 ? "huf|rans" : c == C_STORE ? "store" : c == C_HUF ? "huf" : c == C_RANS ? "rans" : c == C_ZSTD ? "zstd" : c == C_LZMA ? "lzma" : c == C_BSC ? "bsc" : "other"; }

/* ---------------- residual layout <-> values ---------------- */
static void res_to_z(const uint8_t *r, size_t m, int ow, int pln, uint64_t *z) {
    for (size_t i = 0; i < m; i++) {
        uint64_t v = 0;
        if (pln) for (int b = 0; b < ow; b++) v |= (uint64_t)r[(size_t)b * m + i] << (8 * b);
        else for (int b = 0; b < ow; b++) v |= (uint64_t)r[i * (size_t)ow + b] << (8 * b);
        z[i] = v;
    }
}
static void z_to_res(const uint64_t *z, size_t m, int ow, int pln, uint8_t *r) {
    for (size_t i = 0; i < m; i++) for (int b = 0; b < ow; b++) {
        uint8_t c = (uint8_t)(z[i] >> (8 * b));
        if (pln) r[(size_t)b * m + i] = c; else r[i * (size_t)ow + b] = c;
    }
}

/* ---------------- bit packing ---------------- */
static int pk_put(kbuf *o, const uint64_t *v, size_t cnt, int b) {
    size_t bytes = (cnt * (size_t)b + 7) / 8; if (kb_res(o, bytes + 16)) return -1;
    uint8_t *p = o->p + o->n; unsigned __int128 acc = 0; int nb = 0; size_t w = 0; uint64_t mk = lomask(b);
    if (b) for (size_t i = 0; i < cnt; i++) {
        acc |= (unsigned __int128)(v[i] & mk) << nb; nb += b;
        while (nb >= 64) { uint64_t lo = (uint64_t)acc; memcpy(p + w, &lo, 8); w += 8; acc >>= 64; nb -= 64; }
    }
    while (nb > 0) { p[w++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    if (w != bytes) return -1;
    o->n += bytes; return 0;
}
static int pk_get(const uint8_t *p, size_t avail, size_t cnt, int b, uint64_t *v) {
    size_t bytes = (cnt * (size_t)b + 7) / 8; if (bytes != avail) return -1;
    unsigned __int128 acc = 0; int nb = 0; size_t r = 0; uint64_t mk = lomask(b);
    if (!b) { for (size_t i = 0; i < cnt; i++) v[i] = 0; return 0; }
    for (size_t i = 0; i < cnt; i++) {
        while (nb < b) {
            if (r + 8 <= bytes) { uint64_t x; memcpy(&x, p + r, 8); acc |= (unsigned __int128)x << nb; nb += 64; r += 8; }
            else if (r < bytes) { acc |= (unsigned __int128)p[r] << nb; nb += 8; r++; }
            else return -1;
        }
        v[i] = (uint64_t)acc & mk; acc >>= b; nb -= b;
    }
    if (acc != 0) return -1;                                   /* padding bits must be zero */
    for (; r < bytes; r++) if (p[r]) return -1;
    return 0;
}

/* ---------------- per-stream backend ---------------- */
typedef struct { int requested, actual; size_t raw, coded; } sstat;
typedef struct { int rep, be, ns; sstat s[8]; size_t hdr; int b; uint64_t ne; } estat;
static int enc_stream(codec_ctx *x, int be, const kbuf *s, kbuf *o, sstat *st) {
    size_t cap = s->n + s->n / 8 + 1024, z = 0; int cc = C_STORE;
    if (kb_res(o, 1 + 20 + cap)) return -1;
    uint8_t *dst = o->p + o->n + 21;
    if (s->n && be == B_ENT) {                                 /* existing Huffman-only and rANS, smaller kept */
        size_t h = codec_compress(x, C_HUF, 0, s->p, s->n, dst, cap);
        if (h && h < s->n) { z = h; cc = C_HUF; }
        uint8_t *t = malloc(cap); size_t r = t ? codec_compress(x, C_RANS, 0, s->p, s->n, t, cap) : 0;
        if (r && r < s->n && (!z || r < z)) { memcpy(dst, t, r); z = r; cc = C_RANS; }
        free(t);
    } else if (s->n && be == B_Z19) {
        size_t r = codec_compress(x, C_ZSTD, 19, s->p, s->n, dst, cap);
        if (r && r < s->n) { z = r; cc = C_ZSTD; }
    } else if (s->n && be != B_STORE) {
        size_t r = codec_compress(x, BC[be], be == B_ZSTD ? ZSTD_CONTROL_LEVEL : 0, s->p, s->n, dst, cap);
        if (r && r < s->n) { z = r; cc = BC[be]; }
    }
    if (cc == C_STORE) { z = s->n; if (z) memcpy(dst, s->p, z); }
    uint8_t hdr[21]; size_t hn = 0; hdr[hn++] = (uint8_t)cc;
    hn = (size_t)(kg_uvw(hdr + hn, s->n) - hdr); hn = (size_t)(kg_uvw(hdr + hn, z) - hdr);
    memmove(o->p + o->n + hn, dst, z); memcpy(o->p + o->n, hdr, hn); o->n += hn + z;
    if (st) { st->requested = be == B_ENT ? -1 : be == B_Z19 ? C_ZSTD : BC[be]; st->actual = cc; st->raw = s->n; st->coded = z; }
    return 0;
}
/* exact < 0: any raw length up to maxraw */
static int dec_stream(codec_ctx *x, const uint8_t *p, size_t n, size_t *i, int64_t exact, size_t maxraw, kbuf *out) {
    uint64_t rl, zl; if (*i >= n) return -1;
    int cc = p[(*i)++];
    if (cc != C_STORE && cc != C_HUF && cc != C_RANS && cc != C_ZSTD) return -1;
    if (kg_getuv(p, n, i, &rl) || kg_getuv(p, n, i, &zl) || zl > n - *i || rl > maxraw) return -1;
    if (exact >= 0 && rl != (uint64_t)exact) return -1;
    if (cc == C_STORE ? zl != rl : (zl >= rl || rl == 0)) return -1;  /* canonical: coded streams are smaller */
    out->n = 0; if (kb_res(out, (size_t)rl + 16)) return -1;
    if (rl && codec_decompress(x, cc, p + *i, (size_t)zl, out->p, (size_t)rl)) return -1;
    out->n = (size_t)rl; *i += zl; return 0;
}

/* ---------------- representations ---------------- */
static int seg_count(size_t m) { return (int)((m + SEG - 1) / SEG); }
static int gpp_choose(const uint64_t *z, size_t m, uint64_t *ne_out) {
    size_t hist[65] = {0}; for (size_t i = 0; i < m; i++) hist[e2bitlen(z[i])]++;
    int maxl = 64; while (maxl > 0 && !hist[maxl]) maxl--;
    double best = 1e300; int bb = maxl;
    for (int b = 0; b <= maxl; b++) {
        double e = 0, hi = 0; for (int L = b + 1; L <= maxl; L++) { e += hist[L]; hi += (double)hist[L] * ((L - b + 6) / 7); }
        double gap = e ? (double)m / e : 0; int gl = gap < 128 ? 1 : gap < 16384 ? 2 : gap < 2097152 ? 3 : 4;
        double c = ((double)m * b + 7) / 8 + e * gl + hi;
        if (c < best) { best = c; bb = b; }
    }
    uint64_t ne = 0; for (size_t i = 0; i < m; i++) ne += e2bitlen(z[i]) > bb; *ne_out = ne; return bb;
}
static int lpp_choose(const uint64_t *z, size_t len) {
    size_t hist[65] = {0}; for (size_t i = 0; i < len; i++) hist[e2bitlen(z[i])]++;
    int maxl = 64; while (maxl > 0 && !hist[maxl]) maxl--;
    size_t best = SIZE_MAX; int bb = maxl;
    for (int b = 0; b <= maxl; b++) {
        size_t c = (len * (size_t)b + 7) / 8; for (int L = b + 1; L <= maxl; L++) c += hist[L] * (size_t)(1 + (L - b + 6) / 7);
        if (c < best) { best = c; bb = b; }
    }
    return bb;
}
/* r: production residual stream (m * ow bytes) */
static int e2c_encode(codec_ctx *x, int rep, int be, const uint8_t *r, size_t m, int ow, int pln, kbuf *blob, estat *st) {
    kbuf s[8] = {{0}}; int ns = 0, rc = -1; uint64_t *z = NULL; blob->n = 0;
    estat dummy; if (!st) st = &dummy; memset(st, 0, sizeof *st); st->rep = rep; st->be = be;
    int prodrule = be == B_PROD3 || be == B_PROD19;
    if (prodrule && rep != R_EXIST && rep != R_PLANES) goto done;
    if (kb_byte(blob, (uint8_t)((prodrule ? 0xD0 : 0xE0) | rep))) goto done;
    if (rep == R_EXIST) { ns = 1; if (kb_put(&s[0], r, m * (size_t)ow)) goto done; }
    else if (rep == R_PLANES) {
        ns = pln ? ow : 1;
        for (int q = 0; q < ns; q++) if (kb_put(&s[q], r + (pln ? (size_t)q * m : 0), pln ? m : m * (size_t)ow)) goto done;
    } else {
        z = malloc((m ? m : 1) * sizeof(uint64_t)); if (!z) goto done; res_to_z(r, m, ow, pln, z);
        if (rep == R_GBP) {
            uint64_t mx = 0; for (size_t i = 0; i < m; i++) mx |= z[i];
            int b = e2bitlen(mx); st->b = b; if (kb_byte(blob, (uint8_t)b)) goto done;
            ns = 1; if (pk_put(&s[0], z, m, b)) goto done;
        } else if (rep == R_LBP) {
            ns = 2; int nsg = seg_count(m);
            for (int g = 0; g < nsg; g++) {
                size_t a = (size_t)g * SEG, l = m - a < SEG ? m - a : SEG; uint64_t mx = 0;
                for (size_t i = 0; i < l; i++) mx |= z[a + i];
                int b = e2bitlen(mx); if (kb_byte(&s[0], (uint8_t)b) || pk_put(&s[1], z + a, l, b)) goto done;
            }
        } else if (rep == R_GPP) {
            uint64_t ne; int b = gpp_choose(z, m, &ne); st->b = b; st->ne = ne;
            if (kb_byte(blob, (uint8_t)b) || kb_uv(blob, ne)) goto done;
            ns = 3; if (pk_put(&s[0], z, m, b)) goto done;
            int64_t prev = -1;
            for (size_t i = 0; i < m; i++) if (e2bitlen(z[i]) > b) {
                if (kb_uv(&s[1], (uint64_t)((int64_t)i - prev - 1)) || kb_uv(&s[2], z[i] >> b)) goto done; prev = (int64_t)i;
            }
        } else if (rep == R_LPP) {
            ns = 5; int nsg = seg_count(m);
            for (int g = 0; g < nsg; g++) {
                size_t a = (size_t)g * SEG, l = m - a < SEG ? m - a : SEG;
                int b = lpp_choose(z + a, l); uint8_t ec = 0;
                for (size_t i = 0; i < l; i++) if (e2bitlen(z[a + i]) > b) { ec++; if (kb_byte(&s[3], (uint8_t)i) || kb_uv(&s[4], z[a + i] >> b)) goto done; }
                if (kb_byte(&s[0], (uint8_t)b) || kb_byte(&s[1], ec) || pk_put(&s[2], z + a, l, b)) goto done;
            }
        } else goto done;
    }
    st->hdr = blob->n; st->ns = ns;
    if (prodrule) {                                             /* production's leaf rule, via the unchanged kg_serialize */
        kgb g = {0}; size_t tot = 0; int ok = kgb_new(&g) == 0;
        if (ok) g.used[0] = 1;
        for (int q = 0; ok && q < ns; q++) { int id = kgb_new(&g); ok = id == q + 1 && !kb_put(&g.s[id], s[q].p, s[q].n); tot += s[q].n; }
        size_t cap = tot + tot / 8 + 4096 * (size_t)(ns + 1), w = 0;
        uint8_t *o = ok ? malloc(cap) : NULL;
        if (o) w = kg_serialize(x, C_ZSTD, be == B_PROD3 ? CZ_FAST_LEVEL : 19, &g, SIZE_MAX / 2, o, cap);
        if (w) { size_t i = 2; uint64_t v, nn, nl; kg_getuv(o, w, &i, &nn); kg_getuv(o, w, &i, &nl);   /* record leaf choices */
                 for (int q = 0; q < ns && q < 8; q++) { uint64_t rl, zl; kg_getuv(o, w, &i, &v); int cc = o[i++]; kg_getuv(o, w, &i, &rl); kg_getuv(o, w, &i, &zl);
                     st->s[q] = (sstat){C_ZSTD, cc, (size_t)rl, (size_t)zl}; i += zl; } }
        int bad = !w || kb_put(blob, o, w);
        free(o); kgb_free(&g); if (bad) goto done;
    } else
    for (int q = 0; q < ns; q++) if (enc_stream(x, be, &s[q], blob, &st->s[q])) goto done;
    rc = 0;
done:
    for (int q = 0; q < 8; q++) free(s[q].p); free(z); return rc;
}
/* decode into the production residual stream layout; maxs: resource limit on the residual stream bytes */
static int e2c_decode(codec_ctx *x, const uint8_t *p, size_t n, size_t m, int ow, int pln, size_t maxs, kbuf *out) {
    size_t i = 0; int rc = -1; kbuf s[8] = {{0}}; uint64_t *z = NULL;
    if ((ow != 1 && ow != 2 && ow != 4 && ow != 8) || m > maxs / (size_t)ow) return -1;
    if (n < 1 || ((p[0] & 0xF0) != 0xE0 && (p[0] & 0xF0) != 0xD0) || (p[0] & 15) >= R_N) return -1;
    int rep = p[0] & 15; i = 1; size_t rb = m * (size_t)ow, nsg = (size_t)seg_count(m), lim = 10 * m + 64;
    out->n = 0; if (kb_res(out, rb + 16)) return -1;
    if ((p[0] & 0xF0) == 0xD0) {                               /* production-leaf-rule framing: leaf-only 'G' program */
        if (rep != R_EXIST && rep != R_PLANES) goto done;
        int np = rep == R_PLANES && pln ? ow : 1; size_t pl = np > 1 ? m : rb; uint64_t nn, nl, id, rl, zl;
        if (n - i < 2 || p[i] != 'G' || p[i + 1] != 1) goto done;
        i += 2; if (kg_getuv(p, n, &i, &nn) || nn || kg_getuv(p, n, &i, &nl) || nl != (uint64_t)np) goto done;
        for (int q = 0; q < np; q++) {
            if (kg_getuv(p, n, &i, &id) || id != (uint64_t)(q + 1) || i >= n) goto done;
            int cc = p[i++];
            if ((cc != C_STORE && cc != C_HUF && cc != C_RANS && cc != C_ZSTD) || kg_getuv(p, n, &i, &rl) || rl != pl || kg_getuv(p, n, &i, &zl) || zl > n - i) goto done;
            if (cc == C_STORE && zl != rl) goto done;
            if (pl && codec_decompress(x, cc, p + i, (size_t)zl, out->p + (size_t)q * pl, pl)) goto done;
            i += zl;
        }
    } else if (rep == R_EXIST) {
        if (dec_stream(x, p, n, &i, (int64_t)rb, rb, &s[0])) goto done;
        memcpy(out->p, s[0].p, rb);
    } else if (rep == R_PLANES) {
        int np = pln ? ow : 1; size_t pl = pln ? m : rb;
        for (int q = 0; q < np; q++) { if (dec_stream(x, p, n, &i, (int64_t)pl, pl, &s[0])) goto done; if (pl) memcpy(out->p + (size_t)q * pl, s[0].p, pl); }
    } else {
        z = malloc((m ? m : 1) * sizeof(uint64_t)); if (!z) goto done;
        if (rep == R_GBP) {
            if (i >= n) goto done; int b = p[i++]; if (b > 8 * ow) goto done;
            size_t pb = (m * (size_t)b + 7) / 8;
            if (dec_stream(x, p, n, &i, (int64_t)pb, pb, &s[0]) || pk_get(s[0].p, s[0].n, m, b, z)) goto done;
        } else if (rep == R_LBP) {
            if (dec_stream(x, p, n, &i, (int64_t)nsg, nsg, &s[0])) goto done;
            size_t pb = 0; for (size_t g = 0; g < nsg; g++) { size_t l = m - g * SEG < SEG ? m - g * SEG : SEG; if (s[0].p[g] > 8 * ow) goto done; pb += (l * s[0].p[g] + 7) / 8; }
            if (dec_stream(x, p, n, &i, (int64_t)pb, pb, &s[1])) goto done;
            size_t off = 0;
            for (size_t g = 0; g < nsg; g++) {
                size_t a = g * SEG, l = m - a < SEG ? m - a : SEG, bytes = (l * s[0].p[g] + 7) / 8;
                if (pk_get(s[1].p + off, bytes, l, s[0].p[g], z + a)) goto done; off += bytes;
            }
        } else if (rep == R_GPP) {
            uint64_t ne; if (i >= n) goto done; int b = p[i++];
            if (b > 8 * ow || kg_getuv(p, n, &i, &ne) || ne > m || (b == 8 * ow && ne)) goto done;
            size_t pb = (m * (size_t)b + 7) / 8;
            if (dec_stream(x, p, n, &i, (int64_t)pb, pb, &s[0]) || pk_get(s[0].p, s[0].n, m, b, z)) goto done;
            if (dec_stream(x, p, n, &i, -1, 10 * (size_t)ne, &s[1]) || dec_stream(x, p, n, &i, -1, 10 * (size_t)ne, &s[2])) goto done;
            size_t gi = 0, hi = 0; int64_t prev = -1;
            for (uint64_t e = 0; e < ne; e++) {
                uint64_t g, h; if (kg_getuv(s[1].p, s[1].n, &gi, &g) || kg_getuv(s[2].p, s[2].n, &hi, &h)) goto done;
                if (g >= m || (uint64_t)(prev + 1) + g >= m) goto done;
                size_t pos = (size_t)(prev + 1) + (size_t)g; prev = (int64_t)pos;
                if (!h || b >= 64 || (b && (h >> (64 - b))) || (ow < 8 && ((h << b) >> (8 * ow)))) goto done;
                z[pos] |= h << b;
            }
            if (gi != s[1].n || hi != s[2].n) goto done;
        } else {
            if (dec_stream(x, p, n, &i, (int64_t)nsg, nsg, &s[0]) || dec_stream(x, p, n, &i, (int64_t)nsg, nsg, &s[1])) goto done;
            size_t pb = 0, te = 0;
            for (size_t g = 0; g < nsg; g++) {
                size_t l = m - g * SEG < SEG ? m - g * SEG : SEG;
                if (s[0].p[g] > 8 * ow || s[1].p[g] > l || (s[0].p[g] == 8 * ow && s[1].p[g])) goto done;
                pb += (l * s[0].p[g] + 7) / 8; te += s[1].p[g];
            }
            if (dec_stream(x, p, n, &i, (int64_t)pb, pb, &s[2]) || dec_stream(x, p, n, &i, (int64_t)te, te, &s[3])
                || dec_stream(x, p, n, &i, -1, 10 * te, &s[4])) goto done;
            size_t off = 0, pi = 0, hi = 0;
            for (size_t g = 0; g < nsg; g++) {
                size_t a = g * SEG, l = m - a < SEG ? m - a : SEG; int b = s[0].p[g]; size_t bytes = (l * (size_t)b + 7) / 8;
                if (pk_get(s[2].p + off, bytes, l, b, z + a)) goto done; off += bytes;
                int last = -1;
                for (int e = 0; e < s[1].p[g]; e++) {
                    int pos = s[3].p[pi++]; uint64_t h;
                    if (pos <= last || (size_t)pos >= l || kg_getuv(s[4].p, s[4].n, &hi, &h)) goto done; last = pos;
                    if (!h || b >= 64 || (b && (h >> (64 - b))) || (ow < 8 && ((h << b) >> (8 * ow)))) goto done;
                    z[a + (size_t)pos] |= h << b;
                }
            }
            if (hi != s[4].n) goto done;
        }
        (void)lim;
        z_to_res(z, m, ow, pln, out->p);
    }
    if (i != n) goto done;
    out->n = rb; rc = 0;
done:
    for (int q = 0; q < 8; q++) free(s[q].p); free(z); return rc;
}

/* ---------------- configurations (incl. charged selector) ---------------- */
static int conf_rep(int c) { return c < NFIX ? c / B_N : c < NFIX + B_N ? -1 : c < NFIX + 2 * B_N ? R_PLANES : XREP[c - NFIX - 2 * B_N]; }
static int conf_be(int c) { return c < NFIX ? c % B_N : c < NFIX + B_N ? c - NFIX : c < NFIX + 2 * B_N ? c - NFIX - B_N : XBE[c - NFIX - 2 * B_N]; }
static const char *conf_name(int c, char *buf) {
    if (c < NFIX) snprintf(buf, 32, "%s+%s", RN[c / B_N], BN[c % B_N]);
    else if (c < NFIX + B_N) snprintf(buf, 32, "sel+%s", BN[c - NFIX]);
    else snprintf(buf, 32, "%s+%s", RN[conf_rep(c)], BN[conf_be(c)]);
    return buf;
}
/* selector: every representation is encoded with the backend; the smallest blob wins (all trials are run, so their
 * cost is part of the measured encode time) */
static int conf_encode(codec_ctx *x, int c, const uint8_t *r, size_t m, int ow, int pln, kbuf *blob, estat *st) {
    if (c < NFIX) return e2c_encode(x, c / B_N, c % B_N, r, m, ow, pln, blob, st);
    if (c >= NFIX + B_N) return e2c_encode(x, conf_rep(c), conf_be(c), r, m, ow, pln, blob, st);
    int be = c - NFIX; kbuf t = {0}; estat ts; int ok = 0; blob->n = 0;
    for (int rep = 0; rep < R_SEL; rep++) {
        if (e2c_encode(x, rep, be, r, m, ow, pln, &t, &ts)) { free(t.p); return -1; }
        if (!ok || t.n < blob->n) { blob->n = 0; if (kb_put(blob, t.p, t.n)) { free(t.p); return -1; } if (st) *st = ts; ok = 1; }
    }
    free(t.p); return 0;
}

/* ---------------- INT graph payload capture / experimental block decode ---------------- */
typedef struct {
    const uint8_t *par; size_t plen; int w, ow, pln, ord; uint64_t cnt; size_t m;
    kbuf res, rest; int rcodec, tcodec; size_t rleaf_len, rcoded, tleaf_len, tcoded, payload;
} intg;
static void ig_free(intg *g) { free(g->res.p); free(g->rest.p); memset(g, 0, sizeof *g); }
static int ig_parse(codec_ctx *x, const uint8_t *p, size_t n, size_t len, int decode_res, intg *g) {
    memset(g, 0, sizeof *g); g->payload = n;
    size_t i = 2; uint64_t v, nn, nin, in0, nout, pl, nl; const uint64_t maxs = (uint64_t)len * 4 + (1u << 20);
    if (n < 3 || p[0] != 'G' || p[1] != 1 || kg_getuv(p, n, &i, &nn) || nn != 1 || i >= n || p[i++] != KG_INT) return -1;
    if (kg_getuv(p, n, &i, &nin) || nin != 1 || kg_getuv(p, n, &i, &in0) || in0 != 0 || kg_getuv(p, n, &i, &nout) || nout != 2
        || kg_getuv(p, n, &i, &pl) || pl > n - i || pl < 3) return -1;
    g->par = p + i; g->plen = (size_t)pl; i += pl;
    g->w = g->par[0]; g->ow = g->par[2]; g->ord = (g->par[1] >> 1) & 3; g->pln = (g->par[1] >> 3) & 1;
    { size_t pi = 3; uint64_t h; if (kg_getuv(g->par, g->plen, &pi, &h) || kg_getuv(g->par, g->plen, &pi, &g->cnt) || g->cnt < (uint64_t)g->ord) return -1; }
    g->m = (size_t)(g->cnt - (uint64_t)g->ord);
    if (kg_getuv(p, n, &i, &nl) || nl != 2) return -1;
    for (int q = 0; q < 2; q++) {
        size_t start = i; uint64_t rl, zl;
        if (kg_getuv(p, n, &i, &v) || v != (uint64_t)(q + 1) || i >= n) return -1;
        int cc = p[i++]; if (kg_getuv(p, n, &i, &rl) || kg_getuv(p, n, &i, &zl) || zl > n - i || rl > maxs) return -1;
        kbuf *d = q ? &g->rest : &g->res;
        if (q || decode_res) {
            if (kb_res(d, (size_t)rl + 16) || codec_decompress(x, cc, p + i, (size_t)zl, d->p, (size_t)rl)) return -1;
            d->n = (size_t)rl;
        }
        i += zl;
        if (q) { g->tcodec = cc; g->tleaf_len = i - start; g->tcoded = (size_t)zl; }
        else { g->rcodec = cc; g->rleaf_len = i - start; g->rcoded = (size_t)zl; if (rl != (uint64_t)g->m * (uint64_t)g->ow) return -1; }
    }
    return i == n ? 0 : -1;
}
/* size of the replacement residual leaf: stream id, codec slot, raw length, coded length, blob */
static size_t exp_leaf_bytes(const intg *g, size_t blob) { return (size_t)uvlen(1) + 1 + (size_t)uvlen((uint64_t)g->m * (uint64_t)g->ow) + (size_t)uvlen(blob) + blob; }
static int ig_decode_exp(codec_ctx *x, const uint8_t *p, size_t n, const uint8_t *blob, size_t bn, uint8_t *out, size_t len) {
    intg g; int rc = -1; kbuf res = {0}, r = {0}; const size_t maxs = (size_t)len * 4 + (1u << 20);
    if (ig_parse(x, p, n, len, 0, &g)) goto done;
    if (e2c_decode(x, blob, bn, g.m, g.ow, g.pln, maxs, &res)) goto done;
    if (kg_int_inv(g.par, g.plen, &res, &g.rest, maxs, &r) || r.n != len) goto done;
    memcpy(out, r.p, len); rc = 0;
done:
    ig_free(&g); free(res.p); free(r.p); return rc;
}

/* ---------------- timing / memory ---------------- */
static uint64_t ns_of(clockid_t c) { struct timespec t; clock_gettime(c, &t); return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec; }
static uint64_t xs = 88172645463325252ull;
static uint64_t xr(void) { xs ^= xs << 13; xs ^= xs >> 7; xs ^= xs << 17; return xs; }
static void shuffle(int *a, int n) { for (int i = n - 1; i > 0; i--) { int j = (int)(xr() % (uint64_t)(i + 1)), t = a[i]; a[i] = a[j]; a[j] = t; } }
static long status_kb(const char *key) {
    FILE *f = fopen("/proc/self/status", "r"); char line[256]; long v = -1; size_t kl = strlen(key);
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) if (!strncmp(line, key, kl)) { v = atol(line + kl + 1); break; }
    fclose(f); return v;
}
static int hwm_reset(void) { FILE *f = fopen("/proc/self/clear_refs", "w"); if (!f) return -1; int r = fputs("5", f) < 0; return fclose(f) || r ? -1 : 0; }

static uint64_t fnv(const uint8_t *p, size_t n) { uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; } return h; }
static uint8_t *readfile(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)l + 1); if (!b || fread(b, 1, (size_t)l, f) != (size_t)l) { free(b); fclose(f); return NULL; }
    fclose(f); *n = (size_t)l; return b;
}
#define DIE(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); exit(2); } while (0)

/* ---------------- kernel regime over a set of residual streams ---------------- */
typedef struct { const uint8_t *r; size_t m; int ow, pln; } rstream;
static void print_estat(const estat *st) {
    printf("\"rep_header\":%zu,\"b\":%d,\"exceptions\":%llu,\"streams\":[", st->hdr, st->b, (unsigned long long)st->ne);
    for (int q = 0; q < st->ns; q++) printf("%s{\"requested\":\"%s\",\"actual\":\"%s\",\"raw\":%zu,\"coded\":%zu}", q ? "," : "", codec_name(st->s[q].requested), codec_name(st->s[q].actual), st->s[q].raw, st->s[q].coded);
    printf("]");
}
/* per configuration: blob bytes per stream, actual codecs, determinism; then reps x shuffled timing of encode
 * (residual stream -> blob, including allocation) and decode (blob -> residual stream). */
static void kernel(const char *tag, rstream *S, int ns, int reps, size_t maxs) {
    codec_ctx x = {0}; char nb[32];
    kbuf *blobs = calloc((size_t)ns * NCONF, sizeof(kbuf));
    for (int c = 0; c < NCONF; c++) {
        size_t tot = 0, raw = 0; int det = 1, nstored = 0, ncoded = 0, nreq = 0;
        printf("{\"kind\":\"kernel_size\",\"input\":\"%s\",\"config\":\"%s\",\"per_stream\":[", tag, conf_name(c, nb));
        for (int s = 0; s < ns; s++) {
            estat st; kbuf *b = &blobs[(size_t)s * NCONF + c], b2 = {0}, d = {0};
            if (conf_encode(&x, c, S[s].r, S[s].m, S[s].ow, S[s].pln, b, &st)) DIE("encode failed %s", nb);
            if (conf_encode(&x, c, S[s].r, S[s].m, S[s].ow, S[s].pln, &b2, NULL) || b2.n != b->n || memcmp(b2.p, b->p, b->n)) det = 0;
            if (e2c_decode(&x, b->p, b->n, S[s].m, S[s].ow, S[s].pln, maxs, &d) || d.n != S[s].m * (size_t)S[s].ow || memcmp(d.p, S[s].r, d.n)) DIE("kernel round trip failed %s %s", tag, nb);
            free(b2.p); free(d.p); tot += b->n; raw += S[s].m * (size_t)S[s].ow;
            for (int q = 0; q < st.ns; q++) { if (st.s[q].requested != C_STORE) nreq++; if (st.s[q].requested != C_STORE && st.s[q].actual == C_STORE) nstored++; if (st.s[q].actual != C_STORE) ncoded++; }
            printf("%s{\"m\":%zu,\"ow\":%d,\"pln\":%d,\"blob\":%zu,\"rep\":\"%s\",\"fnv\":\"%016llx\",", s ? "," : "", S[s].m, S[s].ow, S[s].pln, b->n, RN[st.rep], (unsigned long long)fnv(b->p, b->n));
            print_estat(&st); printf("}");
        }
        printf("],\"residual_bytes\":%zu,\"blob_bytes\":%zu,\"deterministic\":%s,\"streams_requested_entropy\":%d,\"requested_but_stored\":%d,\"streams_entropy_coded\":%d}\n",
               raw, tot, det ? "true" : "false", nreq, nstored, ncoded);
    }
    /* calibration: inner repetitions so each timed sample is >= ~3 ms */
    int k[NCONF], ord[NCONF];
    for (int c = 0; c < NCONF; c++) {
        uint64_t t0 = ns_of(CLOCK_MONOTONIC); kbuf b = {0}, d = {0};
        for (int s = 0; s < ns; s++) { conf_encode(&x, c, S[s].r, S[s].m, S[s].ow, S[s].pln, &b, NULL); e2c_decode(&x, b.p, b.n, S[s].m, S[s].ow, S[s].pln, maxs, &d); }
        uint64_t t = ns_of(CLOCK_MONOTONIC) - t0; free(b.p); free(d.p);
        k[c] = t >= 3000000 ? 1 : (int)(3000000 / (t ? t : 1)) + 1; if (k[c] > 2000) k[c] = 2000; ord[c] = c;
    }
    for (int rp = 0; rp < reps; rp++) {
        shuffle(ord, NCONF);
        for (int oi = 0; oi < NCONF; oi++) {
            int c = ord[oi]; uint64_t ew = 0, ec = 0, dw = 0, dc = 0;
            for (int it = 0; it < k[c]; it++) for (int s = 0; s < ns; s++) {
                kbuf b = {0}, d = {0};
                uint64_t w0 = ns_of(CLOCK_MONOTONIC), c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                if (conf_encode(&x, c, S[s].r, S[s].m, S[s].ow, S[s].pln, &b, NULL)) DIE("enc");
                uint64_t w1 = ns_of(CLOCK_MONOTONIC), c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                if (e2c_decode(&x, b.p, b.n, S[s].m, S[s].ow, S[s].pln, maxs, &d)) DIE("dec");
                uint64_t w2 = ns_of(CLOCK_MONOTONIC), c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                if (d.n != S[s].m * (size_t)S[s].ow || memcmp(d.p, S[s].r, d.n)) DIE("timed round trip mismatch");
                free(b.p); free(d.p); ew += w1 - w0; ec += c1 - c0; dw += w2 - w1; dc += c2 - c1;
            }
            printf("{\"kind\":\"kernel_time\",\"input\":\"%s\",\"config\":\"%s\",\"rep\":%d,\"slot\":%d,\"inner\":%d,\"enc_wall_ns\":%.1f,\"enc_cpu_ns\":%.1f,\"dec_wall_ns\":%.1f,\"dec_cpu_ns\":%.1f}\n",
                   tag, conf_name(c, nb), rp, oi, k[c], (double)ew / k[c], (double)ec / k[c], (double)dw / k[c], (double)dc / k[c]);
        }
    }
    for (size_t q = 0; q < (size_t)ns * NCONF; q++) free(blobs[q].p);
    free(blobs); codec_ctx_free(&x);
}

/* ---------------- regime A: explicitly typed numeric input ---------------- */
static int typed(const char *path, const char *tag, int w, size_t h, int level, int reps) {
    size_t n; uint8_t *src = readfile(path, &n); if (!src) DIE("read %s", path);
    codec_ctx x = {0}; size_t cap = n + n / 8 + 4096; uint8_t *pay = malloc(cap);
    uint64_t t0 = ns_of(CLOCK_MONOTONIC), c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
    size_t pn = kg_encode_num(&x, C_ZSTD, level, src, n, w, h, pay, cap);
    uint64_t t1 = ns_of(CLOCK_MONOTONIC), c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
    if (!pn) { printf("{\"kind\":\"typed_na\",\"input\":\"%s\",\"bytes\":%zu,\"w\":%d,\"head\":%zu,\"reason\":\"kg_encode_num returned 0 (graph not smaller or not applicable)\"}\n", tag, n, w, h); free(src); free(pay); return 0; }
    intg g; if (ig_parse(&x, pay, pn, n, 1, &g)) DIE("capture failed %s", tag);
    { kbuf r = {0}; const size_t maxs = n * 4 + (1u << 20);          /* production inverse on the captured streams */
      if (kg_int_inv(g.par, g.plen, &g.res, &g.rest, maxs, &r) || r.n != n || memcmp(r.p, src, n)) DIE("production inverse mismatch %s", tag); free(r.p); }
    { uint8_t *o = malloc(n + 64); if (kg_decode(&x, pay, pn, o, n) || memcmp(o, src, n)) DIE("kg_decode mismatch %s", tag); free(o); }
    size_t zb = ZSTD_compressBound(n); uint8_t *zbuf = malloc(zb); size_t zn = ZSTD_compress(zbuf, zb, src, n, ZSTD_CONTROL_LEVEL); free(zbuf);
    printf("{\"kind\":\"typed_capture\",\"input\":\"%s\",\"bytes\":%zu,\"w\":%d,\"head\":%zu,\"level\":%d,\"order\":%d,\"signed\":%d,\"ow\":%d,\"pln\":%d,\"count\":%llu,\"m\":%zu,"
           "\"payload\":%zu,\"residual_leaf\":%zu,\"residual_codec\":\"%s\",\"residual_coded\":%zu,\"rest_leaf\":%zu,\"rest_codec\":\"%s\",\"rest_bytes\":%zu,\"program_bytes\":%zu,"
           "\"encode_num_wall_ns\":%llu,\"encode_num_cpu_ns\":%llu,\"zstd3_whole_input\":%zu}\n",
           tag, n, w, h, level, g.ord, g.par[1] & 1, g.ow, g.pln, (unsigned long long)g.cnt, g.m, pn, g.rleaf_len, codec_name(g.rcodec), g.rcoded, g.tleaf_len, codec_name(g.tcodec), g.rest.n,
           pn - g.rleaf_len - g.tleaf_len, (unsigned long long)(t1 - t0), (unsigned long long)(c1 - c0), zn);
    /* complete-input reconstruction per configuration through the experimental block decoder + production inverse */
    char nb[32];
    for (int c = 0; c < NCONF; c++) {
        kbuf b = {0}; uint8_t *o = malloc(n + 64);
        if (conf_encode(&x, c, g.res.p, g.m, g.ow, g.pln, &b, NULL)) DIE("enc");
        if (ig_decode_exp(&x, pay, pn, b.p, b.n, o, n) || memcmp(o, src, n)) DIE("complete-input reconstruction failed %s %s", tag, conf_name(c, nb));
        printf("{\"kind\":\"typed_total\",\"input\":\"%s\",\"config\":\"%s\",\"payload\":%zu,\"exact\":true}\n", tag, conf_name(c, nb), pn - g.rleaf_len + exp_leaf_bytes(&g, b.n));
        free(b.p); free(o);
    }
    rstream S = {g.res.p, g.m, g.ow, g.pln};
    kernel(tag, &S, 1, reps, n * 4 + (1u << 20));
    ig_free(&g); codec_ctx_free(&x); free(pay); free(src); return 0;
}

/* ---------------- regime B: complete production planner ---------------- */
typedef struct { size_t poff, cl, roff, len; int isint; intg g; } binfo;
typedef struct { uint8_t *frame; size_t fn; parsed_t P; binfo *B; size_t maxraw; } fparsed;
static int frame_walk(codec_ctx *x, uint8_t *frame, size_t fn, fparsed *F) {
    memset(F, 0, sizeof *F); F->frame = frame; F->fn = fn;
    if (parse(frame, fn, &F->P, NULL)) return -1;
    F->B = calloc(F->P.nb ? F->P.nb : 1, sizeof(binfo));
    for (uint32_t i = 0; i < F->P.nb; i++) {
        const uint8_t *e = F->P.idx + (size_t)i * IDX; binfo *b = &F->B[i];
        b->poff = F->P.poff[i]; b->cl = F->P.poff[i + 1] - F->P.poff[i]; b->roff = F->P.roff[i]; b->len = get32(e);
        if (b->len > F->maxraw) F->maxraw = b->len;
        const uint8_t *pay = frame + b->poff;
        if (F->P.ver >= 2 && b->cl >= 4 && e[12] == C_GRAPH && pay[0] == T_KG && pay[1] == 0 && !(pay[2] & 48)) {
            if (ig_parse(x, pay + 4, b->cl - 4, b->len, 1, &b->g)) return -1;
            b->isint = 1;
        }
    }
    return 0;
}
static void frame_free(fparsed *F) { if (F->B) for (uint32_t i = 0; i < F->P.nb; i++) ig_free(&F->B[i].g); free(F->B); free(F->P.poff); free(F->P.roff); }
/* sequential single-thread decode of the whole frame; blobs: replacement residual leaves for INT blocks (NULL: production) */
static int seq_decode(fparsed *F, kbuf *blobs, uint8_t *out) {
    djob_t J = {0}; J.src = F->frame; J.P = &F->P; J.maxraw = F->maxraw;
    tls_t S = {0}; uint8_t *tbuf = malloc(F->maxraw + TSLACK + 64); int rc = 0;
    if (!tbuf) return -1;
    for (uint32_t i = 0; i < F->P.nb && !rc; i++) {
        binfo *b = &F->B[i]; const uint8_t *e = F->P.idx + (size_t)i * IDX;
        if (blobs && b->isint) {
            if (ig_decode_exp(&S.x, F->frame + b->poff + 4, b->cl - 4, blobs[i].p, blobs[i].n, out + b->roff, b->len)) rc = -1;
            else if (blk_sum(F->P.ver, out + b->roff, b->len) != get32(e + 8)) rc = -2;   /* production block checksum */
        } else rc = decode_block(&J, i, &S.x, &S, F->frame + b->poff, tbuf, out + b->roff);
    }
    free(tbuf); free(S.pos); free(S.rowbuf); free(S.rbuf); free(S.slot); free(S.tbuf); codec_ctx_free(&S.x);
    return rc;
}
static kolmo_params mode_params(const char *mode) {
    kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1};
    if (!strcmp(mode, "hc")) p.mode = KOLMO_HC; else if (strcmp(mode, "fast")) DIE("mode must be fast or hc");
    return p;
}
static int planner(const char *path, const char *tag, const char *mode, int reps) {
    size_t n; uint8_t *src = readfile(path, &n); if (!src) DIE("read %s", path);
    kolmo_params p = mode_params(mode); size_t cap = kolmo_bound(n, &p); uint8_t *frame = malloc(cap), *out = malloc(n + 64);
    int64_t fn = kolmo_compress(src, n, frame, cap, &p); if (fn <= 0) DIE("kolmo_compress failed");
    { int64_t d = kolmo_decompress(frame, (size_t)fn, out, n + 64, 1); if (d != (int64_t)n || memcmp(out, src, n)) DIE("production decode mismatch"); }
    codec_ctx x = {0}; fparsed F;
    if (frame_walk(&x, frame, (size_t)fn, &F)) DIE("frame walk failed");
    if (seq_decode(&F, NULL, out) || memcmp(out, src, n)) DIE("sequential production decode mismatch");
    int nint = 0; size_t intraw = 0, rleaf = 0; char nb[32];
    for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) { nint++; intraw += F.B[i].len; rleaf += F.B[i].g.rleaf_len; }
    size_t zb = ZSTD_compressBound(n); uint8_t *zbuf = malloc(zb); size_t zn = ZSTD_compress(zbuf, zb, src, n, ZSTD_CONTROL_LEVEL);
    printf("{\"kind\":\"planner_frame\",\"input\":\"%s\",\"mode\":\"%s\",\"bytes\":%zu,\"frame\":%lld,\"blocks\":%u,\"int_blocks\":%d,\"int_block_raw\":%zu,\"residual_leaf_bytes\":%zu,\"zstd3_whole_input\":%zu,\"blocks_detail\":[",
           tag, mode, n, (long long)fn, F.P.nb, nint, intraw, rleaf, zn);
    for (uint32_t i = 0; i < F.P.nb; i++) {
        const uint8_t *e = F.P.idx + (size_t)i * IDX; binfo *b = &F.B[i];
        printf("%s{\"raw\":%zu,\"stored\":%zu,\"codec\":%d,\"plan\":%d", i ? "," : "", b->len, b->cl, e[12], F.frame[b->poff]);
        if (b->isint) printf(",\"int\":{\"w\":%d,\"order\":%d,\"signed\":%d,\"ow\":%d,\"pln\":%d,\"m\":%zu,\"residual_leaf\":%zu,\"residual_codec\":\"%s\",\"residual_coded\":%zu,\"rest_leaf\":%zu,\"rest_codec\":\"%s\",\"rest_bytes\":%zu,\"program_bytes\":%zu}",
                             b->g.w, b->g.ord, b->g.par[1] & 1, b->g.ow, b->g.pln, b->g.m, b->g.rleaf_len, codec_name(b->g.rcodec), b->g.rcoded, b->g.tleaf_len, codec_name(b->g.tcodec), b->g.rest.n, b->g.payload - b->g.rleaf_len - b->g.tleaf_len);
        printf("}");
    }
    printf("]}\n");
    kbuf *blobs = calloc((size_t)F.P.nb * NCONF, sizeof(kbuf));
    rstream *S = calloc((size_t)(nint ? nint : 1), sizeof(rstream)); int si = 0;
    for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) S[si++] = (rstream){F.B[i].g.res.p, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln};
    /* complete-input sizes and exact reconstruction per configuration */
    for (int c = 0; c < NCONF && nint; c++) {
        kbuf *bl = calloc(F.P.nb, sizeof(kbuf)); size_t tot = (size_t)fn;
        for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) {
            if (conf_encode(&x, c, F.B[i].g.res.p, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln, &bl[i], NULL)) DIE("enc");
            tot = tot - F.B[i].g.rleaf_len + exp_leaf_bytes(&F.B[i].g, bl[i].n);
        }
        memset(out, 0xA5, n);
        int r = seq_decode(&F, bl, out);
        if (r || memcmp(out, src, n)) DIE("complete-input reconstruction failed %s %s (%d)", tag, conf_name(c, nb), r);
        printf("{\"kind\":\"planner_total\",\"input\":\"%s\",\"mode\":\"%s\",\"config\":\"%s\",\"total\":%zu,\"exact\":true}\n", tag, mode, conf_name(c, nb), tot);
        for (uint32_t i = 0; i < F.P.nb; i++) blobs[(size_t)c * F.P.nb + i] = bl[i];
        free(bl);
    }
    /* end-to-end timings: production compress / API decompress / sequential decode, experimental sequential decode,
     * residual-only work (production leaf re-encode with its chosen codec, experimental encode), zstd -3 control */
    int nord = NCONF + 5, *ordv = malloc(sizeof(int) * (size_t)nord); for (int q = 0; q < nord; q++) ordv[q] = q;
    int lvl = p.mode == KOLMO_HC ? 19 : CZ_FAST_LEVEL;
    for (int rp = 0; rp < reps; rp++) {
        shuffle(ordv, nord);
        for (int oi = 0; oi < nord; oi++) {
            int c = ordv[oi]; uint64_t w0 = ns_of(CLOCK_MONOTONIC), c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID), w1, c1; const char *what; char nm[48] = "";
            if (c == NCONF) { what = "prod_compress"; int64_t r = kolmo_compress(src, n, frame, cap, &p); w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID); if (r != fn) DIE("nondeterministic frame"); }
            else if (c == NCONF + 1) { what = "prod_decompress_api"; int64_t r = kolmo_decompress(frame, (size_t)fn, out, n + 64, 1); w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID); if (r != (int64_t)n || memcmp(out, src, n)) DIE("api decode"); }
            else if (c == NCONF + 2) { what = "prod_decode_seq"; int r = seq_decode(&F, NULL, out); w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID); if (r || memcmp(out, src, n)) DIE("seq decode"); }
            else if (c == NCONF + 3) {
                what = "prod_residual_leaf_reencode"; uint8_t *tb = malloc(F.maxraw * 2 + 4096);
                for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint && F.B[i].g.rcodec != C_STORE)
                    codec_compress(&x, F.B[i].g.rcodec, F.B[i].g.rcodec == C_ZSTD ? lvl : 0, F.B[i].g.res.p, F.B[i].g.res.n, tb, F.maxraw * 2 + 4096);
                w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID); free(tb);
            } else if (c == NCONF + 4) {
                what = "zstd3_whole"; size_t z = ZSTD_compress(zbuf, zb, src, n, ZSTD_CONTROL_LEVEL); w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                uint64_t w2 = ns_of(CLOCK_MONOTONIC), c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID); size_t d = ZSTD_decompress(out, n + 64, zbuf, z); uint64_t w3 = ns_of(CLOCK_MONOTONIC), c3 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                if (d != n || memcmp(out, src, n)) DIE("zstd control");
                printf("{\"kind\":\"e2e_time\",\"input\":\"%s\",\"mode\":\"%s\",\"what\":\"zstd3_whole_decompress\",\"rep\":%d,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n", tag, mode, rp, (unsigned long long)(w3 - w2), (unsigned long long)(c3 - c2));
            } else {
                if (!nint) continue;
                what = "exp_residual_encode"; conf_name(c, nm);
                for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) { kbuf b = {0}; conf_encode(&x, c, F.B[i].g.res.p, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln, &b, NULL); free(b.p); }
                w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                printf("{\"kind\":\"e2e_time\",\"input\":\"%s\",\"mode\":\"%s\",\"what\":\"%s\",\"config\":\"%s\",\"rep\":%d,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n", tag, mode, what, nm, rp, (unsigned long long)(w1 - w0), (unsigned long long)(c1 - c0));
                uint64_t w2 = ns_of(CLOCK_MONOTONIC), c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                int r = seq_decode(&F, blobs + (size_t)c * F.P.nb, out);
                uint64_t w3 = ns_of(CLOCK_MONOTONIC), c3 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
                if (r || memcmp(out, src, n)) DIE("exp e2e decode");
                printf("{\"kind\":\"e2e_time\",\"input\":\"%s\",\"mode\":\"%s\",\"what\":\"exp_decode_seq\",\"config\":\"%s\",\"rep\":%d,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n", tag, mode, nm, rp, (unsigned long long)(w3 - w2), (unsigned long long)(c3 - c2));
                continue;
            }
            printf("{\"kind\":\"e2e_time\",\"input\":\"%s\",\"mode\":\"%s\",\"what\":\"%s\",\"rep\":%d,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n", tag, mode, what, rp, (unsigned long long)(w1 - w0), (unsigned long long)(c1 - c0));
        }
    }
    if (nint) kernel(tag, S, nint, reps, F.maxraw * 4 + (1u << 20));
    for (size_t q = 0; q < (size_t)F.P.nb * NCONF; q++) free(blobs[q].p);
    free(blobs); free(S); free(ordv); free(zbuf); frame_free(&F); codec_ctx_free(&x); free(frame); free(out); free(src); return 0;
}

/* ---------------- peak memory (process RSS high-water above the RSS just before the operation) ---------------- */
static int memmode(const char *path, const char *mode, const char *op, int conf) {
    size_t n; uint8_t *src = readfile(path, &n); if (!src) DIE("read");
    kolmo_params p = mode_params(mode); size_t cap = kolmo_bound(n, &p); uint8_t *frame = malloc(cap), *out = malloc(n + 64);
    memset(frame, 0, cap); memset(out, 0, n + 64);
    int64_t fn = kolmo_compress(src, n, frame, cap, &p); if (fn <= 0) DIE("compress");
    codec_ctx x = {0}; fparsed F; if (frame_walk(&x, frame, (size_t)fn, &F)) DIE("walk");
    kbuf *bl = calloc(F.P.nb, sizeof(kbuf));
    for (uint32_t i = 0; conf >= 0 && i < F.P.nb; i++) if (F.B[i].isint) conf_encode(&x, conf, F.B[i].g.res.p, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln, &bl[i], NULL);
    codec_ctx y = {0};
    malloc_trim(0);                                   /* return setup's freed pages so they cannot hide the operation's */
    if (hwm_reset()) DIE("clear_refs unavailable");
    long before = status_kb("VmRSS:");
    if (!strcmp(op, "prod_compress")) { if (kolmo_compress(src, n, frame, cap, &p) != fn) DIE("c"); }
    else if (!strcmp(op, "prod_decompress_api")) { if (kolmo_decompress(frame, (size_t)fn, out, n + 64, 1) != (int64_t)n) DIE("d"); }
    else if (!strcmp(op, "prod_decode_seq")) { if (seq_decode(&F, NULL, out)) DIE("s"); }
    else if (!strcmp(op, "exp_decode_seq")) { if (seq_decode(&F, bl, out)) DIE("e"); }
    else if (!strcmp(op, "exp_residual_encode")) { for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) { kbuf b = {0}; conf_encode(&y, conf, F.B[i].g.res.p, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln, &b, NULL); free(b.p); } }
    else if (!strcmp(op, "exp_residual_decode")) { for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) { kbuf d = {0}; e2c_decode(&y, bl[i].p, bl[i].n, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln, F.maxraw * 4 + (1u << 20), &d); free(d.p); } }
    else DIE("op");
    long hwm = status_kb("VmHWM:");
    if (strcmp(op, "exp_residual_encode") && strcmp(op, "exp_residual_decode") && memcmp(out, src, n) && strcmp(op, "prod_compress")) DIE("mem op output mismatch");
    char nb[32] = "";
    printf("{\"kind\":\"mem\",\"input\":\"%s\",\"mode\":\"%s\",\"op\":\"%s\",\"config\":\"%s\",\"rss_before_kb\":%ld,\"hwm_kb\":%ld,\"peak_extra_kb\":%ld}\n",
           path, mode, op, conf >= 0 ? conf_name(conf, nb) : "", before, hwm, hwm - before);
    codec_ctx_free(&y); for (uint32_t i = 0; i < F.P.nb; i++) free(bl[i].p); free(bl); frame_free(&F); codec_ctx_free(&x); free(frame); free(out); free(src); return 0;
}

#include "selftest.inc"

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest(argc > 2 && argv[2][0] ? argv[2] : NULL, argc > 3 ? argv[3] : NULL);
    if (argc >= 8 && !strcmp(argv[1], "typed")) { xs ^= (uint64_t)atoll(argv[7]) * 0x9E3779B97F4A7C15ull; return typed(argv[2], argv[3], atoi(argv[4]), (size_t)atoll(argv[5]), atoi(argv[6]), argc > 8 ? atoi(argv[8]) : 7); }
    if (argc >= 6 && !strcmp(argv[1], "planner")) { xs ^= (uint64_t)atoll(argv[5]) * 0x9E3779B97F4A7C15ull; return planner(argv[2], argv[3], argv[4], argc > 6 ? atoi(argv[6]) : 7); }
    if (argc >= 6 && !strcmp(argv[1], "mem")) return memmode(argv[2], argv[3], argv[4], atoi(argv[5]));
    if (argc >= 2 && !strcmp(argv[1], "configs")) { char nb[32]; for (int c = 0; c < NCONF; c++) printf("%d %s\n", c, conf_name(c, nb)); return 0; }
    fprintf(stderr, "usage: e02c selftest [vector-dir] [golden.json] | typed FILE TAG W HEAD LEVEL SEED [REPS] | planner FILE TAG fast|hc SEED [REPS] | mem FILE fast|hc OP CONF | configs\n");
    return 2;
}
