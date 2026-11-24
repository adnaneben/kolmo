/* R13 prototype (measurement only, not Kolmo code yet): how many bytes does it take to rebuild a deflate stream
 * bit-exactly from its inflated content?  Written from RFC 1951/1952; no zlib code used or linked.
 *   reflate_cost file.gz|file.zlib|raw-deflate [maxwalk]
 * Method: inflate with an own decoder that records the token sequence; then replay the tokens against a
 * deterministic predictor that sees only the INFLATED bytes (a 3-byte hash chain, nearest first, the list of
 * "record setters" = candidates strictly longer than every nearer one) and sum -log2 p of adaptive binary models for
 * every deviation.  Dynamic block headers are counted verbatim (their bits in the stream) plus 3 bytes per block.
 * The figure is a model cost (an arithmetic coder lands within a fraction of a percent of it). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

typedef struct { const uint8_t *p; size_t n, pos; uint64_t bb; int bc; } BR;
static int getbits(BR *b, int k) { while (b->bc < k) { if (b->pos >= b->n) { fprintf(stderr, "eof\n"); exit(2); } b->bb |= (uint64_t)b->p[b->pos++] << b->bc; b->bc += 8; }
    int v = (int)(b->bb & ((1u << k) - 1)); b->bb >>= k; b->bc -= k; return v; }
typedef struct { uint16_t cnt[16], sym[288]; } HT;
static void build(HT *h, const uint8_t *len, int n) { uint16_t offs[16]; memset(h->cnt, 0, sizeof h->cnt);
    for (int i = 0; i < n; i++) h->cnt[len[i]]++; h->cnt[0] = 0; offs[1] = 0; for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->cnt[i];
    for (int i = 0; i < n; i++) if (len[i]) h->sym[offs[len[i]]++] = (uint16_t)i; }
static int decode(BR *b, const HT *h) { int code = 0, first = 0, index = 0;
    for (int l = 1; l <= 15; l++) { code |= getbits(b, 1); int c = h->cnt[l]; if (code - c < first) return h->sym[index + (code - first)]; index += c; first += c; first <<= 1; code <<= 1; }
    fprintf(stderr, "bad code\n"); exit(2); }
static const uint16_t LB[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t LX[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t DB[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t DX[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

typedef struct { uint32_t pos; uint16_t len, dist; } Tok;       /* len 0: literal */
static uint8_t *out; static size_t on, ocap; static Tok *tk; static size_t nt, tcap;
static double hdrbits; static size_t nblocks, nstored, nfixed, ndyn;
static void emit(uint32_t pos, int len, int dist) { if (nt == tcap) { tcap = tcap ? tcap * 2 : 1 << 20; tk = realloc(tk, tcap * sizeof *tk); } tk[nt].pos = pos; tk[nt].len = (uint16_t)len; tk[nt].dist = (uint16_t)dist; nt++; }
static void putb(int c) { if (on == ocap) { ocap = ocap ? ocap * 2 : 1 << 22; out = realloc(out, ocap); } out[on++] = (uint8_t)c; }

static void inflate_all(BR *b) {
    int last;
    do { last = getbits(b, 1); int ty = getbits(b, 2); nblocks++;
        if (ty == 0) { nstored++; b->bb = 0; b->bc = 0; int l = b->p[b->pos] | b->p[b->pos + 1] << 8; b->pos += 4; hdrbits += 40; for (int i = 0; i < l; i++) { emit((uint32_t)on, 0, 0); putb(b->p[b->pos++]); } continue; }
        HT hl, hd; uint8_t len[320];
        if (ty == 1) { nfixed++; for (int i = 0; i < 288; i++) len[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8; build(&hl, len, 288); for (int i = 0; i < 30; i++) len[i] = 5; build(&hd, len, 30); }
        else { ndyn++; size_t p0 = b->pos * 8 - (size_t)b->bc;
            int nl = getbits(b, 5) + 257, nd = getbits(b, 5) + 1, nc = getbits(b, 4) + 4; static const uint8_t ord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            uint8_t cl[19] = {0}; for (int i = 0; i < nc; i++) cl[ord[i]] = (uint8_t)getbits(b, 3); HT hc; build(&hc, cl, 19);
            for (int i = 0; i < nl + nd; ) { int s = decode(b, &hc); if (s < 16) len[i++] = (uint8_t)s; else { int r, v = 0; if (s == 16) { v = len[i - 1]; r = 3 + getbits(b, 2); } else if (s == 17) r = 3 + getbits(b, 3); else r = 11 + getbits(b, 7); while (r--) len[i++] = (uint8_t)v; } }
            build(&hl, len, nl); build(&hd, len + nl, nd); hdrbits += (double)(b->pos * 8 - (size_t)b->bc - p0); }
        for (;;) { int s = decode(b, &hl); if (s == 256) break;
            if (s < 256) { emit((uint32_t)on, 0, 0); putb(s); }
            else { s -= 257; int l = LB[s] + getbits(b, LX[s]); int ds = decode(b, &hd); int d = DB[ds] + getbits(b, DX[ds]); emit((uint32_t)on, l, d); for (int i = 0; i < l; i++) putb(out[on - (size_t)d]); } }
    } while (!last);
}

/* adaptive binary model cost */
typedef struct { uint16_t p; } M; static double cost;
static void code(M *m, int bit) { if (!m->p) m->p = 32768; double p1 = m->p / 65536.0; cost += -log2(bit ? p1 : 1 - p1); if (bit) m->p += (65536 - m->p) >> 4; else m->p -= m->p >> 4; if (m->p < 64) m->p = 64; if (m->p > 65472) m->p = 65472; }
static void unary(M *m, int n, int v) { for (int i = 0; i < n; i++) { code(&m[i], v > i); if (v <= i) return; } }

#define HB 18
static int32_t *head, *prv; static int maxwalk;
static inline uint32_t h3(const uint8_t *p) { return ((uint32_t)(p[0] | p[1] << 8 | p[2] << 16) * 2654435761u) >> (32 - HB); }
typedef struct { int len, dist; } RS;
static int setters(size_t i, RS *r, int cap) {   /* record setters at i among inserted positions < i */
    int k = 0, best = 2, steps = 0; size_t rem = on - i; if (rem < 3) return 0; int lim = rem < 258 ? (int)rem : 258;
    for (int32_t c = head[h3(out + i)]; c >= 0 && i - (size_t)c <= 32768 && steps < maxwalk; c = prv[c], steps++) {
        const uint8_t *a = out + c, *b = out + i; if (a[best] != b[best] || a[0] != b[0] || a[1] != b[1] || a[2] != b[2]) continue;
        int l = 3; while (l < lim && a[l] == b[l]) l++;
        if (l > best) { best = l; if (k < cap) { r[k].len = l; r[k].dist = (int)(i - (size_t)c); k++; } if (l >= lim) break; }
    }
    return k;
}
static inline void ins(size_t i) { if (i + 3 <= on) { uint32_t h = h3(out + i); prv[i] = head[h]; head[h] = (int32_t)i; } }
static int lb(int l) { return l == 0 ? 0 : l < 4 ? 1 : l < 5 ? 2 : l < 7 ? 3 : l < 11 ? 4 : l < 19 ? 5 : l < 35 ? 6 : 7; }

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); fseek(f, 0, SEEK_SET); uint8_t *z = malloc(n); if (fread(z, 1, n, f) != n) return 1; fclose(f);
    maxwalk = argc > 2 ? atoi(argv[2]) : 256; int maxins = argc > 3 ? atoi(argv[3]) : 0;   /* maxins > 0: positions inside a match longer than maxins are not inserted (the "fast" policy of low levels) */
    BR b = {z, n, 0, 0, 0};
    if (z[0] == 0x1F && z[1] == 0x8B) { int fl = z[3]; size_t p = 10; if (fl & 4) p += 2 + (z[p] | z[p + 1] << 8); if (fl & 8) { while (z[p]) p++; p++; } if (fl & 16) { while (z[p]) p++; p++; } if (fl & 2) p += 2; b.pos = p; }
    else if ((z[0] & 15) == 8 && ((z[0] << 8 | z[1]) % 31) == 0) b.pos = 2;
    size_t start = b.pos; inflate_all(&b); size_t dbytes = b.pos - start;
    head = malloc(sizeof(int32_t) << HB); memset(head, 0xFF, sizeof(int32_t) << HB); prv = malloc(sizeof(int32_t) * (on + 1));
    static M mflag[8][8][3][2], midx[8][12], mlen[8][2], mlend[16], mesc[2];
    size_t nlit = 0, nmatch = 0, nesc = 0, nforced = 0, ntop = 0, nfull = 0; size_t ipos = 0; int prevlit_lazy = 0;
    double c_flag = 0, c_idx = 0, c_len = 0, c_esc = 0;
    for (size_t t = 0; t < nt; t++) {
        size_t i = tk[t].pos;
        if (maxins && t && tk[t - 1].len > maxins) { if (ipos <= tk[t - 1].pos) ins(tk[t - 1].pos); ipos = i; }
        while (ipos < i) ins(ipos++);
        if (maxins && tk[t].len > maxins) { /* after this token: only its start is in the chain */ }
        RS r[64], r2[64]; int k = setters(i, r, 64); int best = k ? r[k - 1].len : 0;
        int best2 = 0; if (k && i + 1 < on) { ins(ipos++); int k2 = setters(i + 1, r2, 64); best2 = k2 ? r2[k2 - 1].len : 0; }
        double c0 = cost;
        if (!k) { if (tk[t].len) { /* match the predictor cannot see (beyond maxwalk): escape */ } else { nlit++; nforced++; prevlit_lazy = 0; continue; } }
        int isM = tk[t].len != 0;
        if (k) code(&mflag[lb(best)][lb(best2)][best2 > best ? 2 : best2 == best ? 1 : 0][prevlit_lazy], isM);
        c_flag += cost - c0; c0 = cost;
        if (!isM) { nlit++; prevlit_lazy = 1; continue; }
        prevlit_lazy = 0; nmatch++;
        int j = -1; for (int q = k - 1; q >= 0; q--) if (r[q].dist == tk[t].dist) { j = q; break; }
        if (k) code(&mesc[0], j < 0);
        if (j < 0 || r[j].len < tk[t].len) { if (j >= 0) { /* cannot happen: dist found but shorter */ } cost += 23; nesc++; c_esc += cost - c0; continue; }
        c_esc += cost - c0; c0 = cost;
        int fromend = k - 1 - j; if (fromend == 0) ntop++;
        unary(midx[lb(best)], 12, fromend > 11 ? 11 : fromend); if (fromend >= 11) cost += 6;
        c_idx += cost - c0; c0 = cost;
        int cut = r[j].len - tk[t].len; if (!cut) nfull++;
        code(&mlen[lb(r[j].len)][0], cut != 0); if (cut) { unary(mlend, 16, cut > 15 ? 15 : cut - 1); if (cut > 15) cost += 8; }
        c_len += cost - c0;
    }
    double hb = hdrbits / 8 + 3.0 * (double)nblocks;
    printf("%s: deflate %zu B -> inflated %zu B; blocks %zu (stored %zu fixed %zu dynamic %zu); tokens %zu (lit %zu, match %zu)\n", argv[1], dbytes, on, nblocks, nstored, nfixed, ndyn, nt, nlit, nmatch);
    printf("  maxwalk %d maxins %d: literals forced (no candidate) %zu; matches at longest setter %zu, full length %zu, escapes %zu\n", maxwalk, maxins, nforced, ntop, nfull, nesc);
    printf("  model cost: flag %.0f B, setter index %.0f B, length cut %.0f B, escapes %.0f B = %.0f B; block headers verbatim %.0f B\n", c_flag / 8, c_idx / 8, c_len / 8, c_esc / 8, cost / 8, hb);
    printf("  RECONSTRUCTION INFO %.0f B = %.2f%% of the deflate stream, %.3f%% of the inflated bytes\n", cost / 8 + hb, 100 * (cost / 8 + hb) / (double)dbytes, 100 * (cost / 8 + hb) / (double)on);
    return 0;
}
