/* kolmo graph (KG): a block stores a small program instead of one fixed transform.
 *
 * Stream 0 is the block. Each node consumes one or more streams and appends its outputs to the stream table; the
 * streams no node consumes are the leaves, each coded by one of the ordinary codecs. A decoder decodes the leaves,
 * then runs the nodes' inverses in reverse order. New transforms add node ids, never container versions, and a decoder
 * that meets an unknown node id fails cleanly instead of misreading the block.
 *
 * Payload: 'G' | version u8 (1) | nodes uv | node* | leaves uv | leaf*
 *   node: op u8 | nin uv | in-index uv * nin | nout uv | plen uv | params[plen]   (outputs get the next stream indices)
 *   leaf: stream uv | codec u8 (block codecs 0-7, 13 Huffman only, 14 rANS) | raw length uv | coded length uv | coded bytes
 * Every stream is consumed at most once and stream 0 at least once (when there are nodes).
 *
 * Nodes (version 1):
 *   1 FIELDS  params: delim u8, k uv. In: text. Out: shape, column 0 .. column k-1. Lines with exactly k fields send each
 *             field, '\n'-terminated, to its column; any other line goes to shape verbatim. shape = lines uv, final
 *             newline u8, then per irregular line: line-index gap uv, length uv, bytes.
 *   2 DECIMAL params: separator u8, predictor u8 (0 none, 1 previous value). In: values, '\n'-terminated. Out: tags (one
 *             byte per value: 0 empty, 1 number, 2 text), scales (one byte per number: digits after the separator, or
 *             255 for none), mantissas (zigzag uv of the difference from the predictor), texts ('\n'-terminated).
 *             A number is -?(0|[1-9][0-9]*)(sep[0-9]+)? with at most 18 digits and not a negative zero; anything else
 *             is text, so every input round-trips.
 *   3 CONCAT  params: count uv, length uv * count. In: count streams. Out: their concatenation.
 *   4 INT     params: width u8 (1, 2, 4 or 8), flags u8 (bit 0 signed, bits 1-2 order 0..2, bit 3 byte planes), output
 *             width u8 (1, 2, 4 or 8), head uv, count uv, gcd uv (>= 1), bias uv (zigzag), then order seeds uv (zigzag).
 *             In: bytes. Out: residuals, rest. The input is head bytes, count little-endian integers of the given width
 *             (sign- or zero-extended to 64 bits), then trailing bytes; rest = head bytes then trailing bytes. Order 0
 *             stores (v - bias) / gcd unsigned; orders 1 and 2 store zigzag((v - prediction) / gcd), the prediction being
 *             the previous value or 2*previous - the one before, after the first `order` values (the seeds). Arithmetic
 *             wraps modulo 2^64; each residual takes `output width` little-endian bytes, transposed into byte planes
 *             when bit 3 is set.
 *   5 FLOAT   params: width u8 (2, 4 or 8), head uv, count uv. In: bytes. Out: plane 0 .. plane width-1, rest. The input is
 *             head bytes, count little-endian words of the given width, then trailing bytes (rest = head then trailing).
 *             Each word is rotated left by one bit (the sign moves to bit 0, so the top byte of a float32 is its
 *             exponent) and byte b of every word goes to plane b.
 *   6 IMG8    params: row bytes uv, channel step u8 (1-4), head uv. In: bytes. Out: residuals, rest. After `head` bytes, the
 *             input is rows of `row bytes`; each byte is predicted from its neighbours `step` bytes left (a), one row up
 *             (b) and up-left (c) with the median of a, b and a + b - c (LOCO-I), the first row from a, the first
 *             `step` bytes of a row from b; residual = zigzag((x - prediction) mod 256 as int8). Bytes after the last
 *             whole row and the head go to rest.
 *   7 DEC64   params: layout u8, then for layout 0 (a table): head uv, rows uv, cols uv; for layout 1 (segments):
 *             count uv; then planes u8 (1-8), then per column or segment: for layout 1 skip uv and values uv (>= 1);
 *             exponent u8 (0-22), flags u8 (bit 0 delta, bit 1 has a missing pattern, bit 2 integers, bit 3 32-bit words,
 *             bit 4 second difference, only with bit 0: the planes hold the difference of successive deltas), and the
 *             8-byte missing pattern when bit 1 is set. In: bytes. Out: tags, plane 0 .. plane planes-1, exceptions, rest. The values are
 *             little-endian binary64. Layout 0: after `head` bytes, rows x cols values row by row, then trailing
 *             bytes (rest = head then trailing); every stream is in column order (column 0 top to bottom, then
 *             column 1). Layout 1: each segment is `skip` bytes that are not values, then its values; rest is every
 *             skipped byte and the bytes after the last segment, in order, and the streams are in input order
 *             (arrays at any byte alignment inside other data: pickles, Arrow and HDF5 files). Tag 0: the value is
 *             m / 10^exponent (IEEE-754 division, round to nearest even) for a signed integer m; the planes hold
 *             zigzag(m), or zigzag(m - previous tag-0 m of the column or segment) with delta, byte b in plane b.
 *             With bit 2 the words are int64 and a tag-0 value is m itself (integer columns beside the float ones).
 *             With bit 3 the words are 4 bytes: binary32 values, (float)(m / 10^exponent) with the division in
 *             binary64, or int32 with bit 2; exceptions are then 4 bytes, and a table's columns all share bit 3.
 *             Tag 1: the missing pattern. Tag 2: the next 8 bytes of exceptions, verbatim. Tags 1 and 2 hold 0 in
 *             the planes. Values written in decimal (prices, measurements, counts) become small integers; anything
 *             else (NaN payloads, -0.0, full-precision values) stays exact as an exception.
 */
enum { KG_FIELDS = 1, KG_DECIMAL = 2, KG_CONCAT = 3, KG_INT = 4, KG_FLOAT = 5, KG_IMG8 = 6, KG_DEC64 = 7, KG_MAXOP = 7 };
#define KG_MAXSTREAMS 65536

typedef struct { uint8_t *p; size_t n, cap; } kbuf;
static int kb_res(kbuf *b, size_t add) {
    if (b->n + add <= b->cap) return 0;
    size_t c = b->cap ? b->cap : 256; while (c < b->n + add) c *= 2;
    uint8_t *q = realloc(b->p, c); if (!q) return -1; b->p = q; b->cap = c; return 0;
}
static int kb_put(kbuf *b, const void *s, size_t n) { if (!n) return 0; if (kb_res(b, n)) return -1; memcpy(b->p + b->n, s, n); b->n += n; return 0; }
static int kb_byte(kbuf *b, uint8_t v) { if (kb_res(b, 1)) return -1; b->p[b->n++] = v; return 0; }
static int kb_uv(kbuf *b, uint64_t v) {
    if (kb_res(b, 10)) return -1;
    while (v >= 128) { b->p[b->n++] = (uint8_t)(v | 128); v >>= 7; }
    b->p[b->n++] = (uint8_t)v; return 0;
}
static int kg_getuv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int s = 0;
    while (*i < n && s < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << s; if (!(c & 128)) { *v = r; return 0; } s += 7; }
    return -1;
}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static inline uint64_t kg_le64(uint64_t w) { return __builtin_bswap64(w); }
#else
static inline uint64_t kg_le64(uint64_t w) { return w; }
#endif
/* ---- DECIMAL ---- */
static int kg_num(const uint8_t *v, size_t l, uint8_t sep, int64_t *m, uint8_t *sc) {
    size_t i = 0; int neg = 0; uint64_t x = 0;                /* digits are summed as they are scanned; more than 18 is refused below */
    if (i < l && v[i] == '-') { neg = 1; i++; }
    size_t a = i; for (unsigned c; i < l && (c = (unsigned)v[i] - '0') < 10; i++) x = x * 10 + c;
    size_t id = i - a; if (!id || (id > 1 && v[a] == '0')) return 0;
    size_t fd = 0, b = 0; int hassep = 0;
    if (i < l) { if (v[i] != sep) return 0; hassep = 1; b = ++i; for (unsigned c; i < l && (c = (unsigned)v[i] - '0') < 10; i++) x = x * 10 + c; fd = i - b; if (!fd || i != l) return 0; }
    if (id + fd > 18) return 0;
    if (neg && x == 0) return 0;
    *m = neg ? -(int64_t)x : (int64_t)x; *sc = hassep ? (uint8_t)fd : 255; return 1;
}
static uint64_t kg_zz(int64_t x) { return ((uint64_t)x << 1) ^ (uint64_t)(x >> 63); }
static int64_t kg_unzz(uint64_t u) { return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }
static const char KG_D2[201] =
    "00010203040506070809101112131415161718192021222324252627282930313233343536373839"
    "40414243444546474849505152535455565758596061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";
/* print mantissa m with scale s (255: no separator, else digits after sep, s <= 18); at most 21 bytes */
static inline uint8_t *kg_put_num(uint8_t *w, int64_t m, uint8_t s, uint8_t sep) {
    int f = s == 255 ? 0 : s; char dg[24]; int nd = 0; uint64_t u = m < 0 ? (uint64_t)0 - (uint64_t)m : (uint64_t)m;
    while (u >= 100) { uint64_t qd = u / 100; unsigned r2 = (unsigned)(u - qd * 100) * 2; dg[nd++] = KG_D2[r2 + 1]; dg[nd++] = KG_D2[r2]; u = qd; }
    if (u >= 10) { dg[nd++] = KG_D2[u * 2 + 1]; dg[nd++] = KG_D2[u * 2]; } else dg[nd++] = (char)('0' + u);
    while (nd < f + 1) dg[nd++] = '0';
    if (m < 0) *w++ = '-';
    if (s == 255) { for (int r = nd - 1; r >= 0; r--) *w++ = (uint8_t)dg[r]; }
    else { for (int r = nd - 1; r >= f; r--) *w++ = (uint8_t)dg[r]; *w++ = sep; for (int r = f - 1; r >= 0; r--) *w++ = (uint8_t)dg[r]; }
    return w;
}
static int kg_decimal_inv(uint8_t sep, uint8_t pred, const kbuf *tags, const kbuf *sc, const kbuf *mant, const kbuf *txt, kbuf *out) {
    size_t si = 0, mi = 0, ti = 0; int64_t prev = 0;
    if (kb_res(out, tags->n * 23 + txt->n + 16)) return -1;    /* a number prints in at most 21 bytes plus '\n' */
    uint8_t *w = out->p + out->n;
    for (size_t q = 0; q < tags->n; q++) {
        uint8_t t = tags->p[q];
        if (t == 0) *w++ = '\n';
        else if (t == 1) {
            uint64_t z = 0; int sh = 0;
            if (mi < mant->n && mant->p[mi] < 128) z = mant->p[mi++];         /* one-byte varint: the common case */
            else for (;;) { if (mi >= mant->n || sh > 63) return -1; uint8_t c = mant->p[mi++]; z |= (uint64_t)(c & 127) << sh; if (!(c & 128)) break; sh += 7; }
            int64_t m = pred ? (int64_t)((uint64_t)prev + (uint64_t)kg_unzz(z)) : kg_unzz(z); prev = m;
            if (si >= sc->n) return -1;
            uint8_t s = sc->p[si++]; int f = s == 255 ? 0 : s; if (s != 255 && s > 18) return -1;
            (void)f; w = kg_put_num(w, m, s, sep); *w++ = '\n';
        } else if (t == 2) {
            const uint8_t *e = ti < txt->n ? memchr(txt->p + ti, '\n', txt->n - ti) : NULL; if (!e) return -1;
            size_t l = (size_t)(e - (txt->p + ti)) + 1; memcpy(w, txt->p + ti, l); w += l; ti += l;
        } else return -1;
    }
    out->n = (size_t)(w - out->p);
    return si == sc->n && mi == mant->n && ti == txt->n ? 0 : -1;
}

/* A DECIMAL column read field by field (the decoder's fused DECIMAL -> FIELDS path: numbers print straight into the
 * rows, the column text is never built). Same checks as kg_decimal_inv. */
typedef struct { const uint8_t *tg, *tge, *sc, *sce, *mt, *mte, *tx, *txe; int64_t prev; uint8_t sep, pred; } kgdec;
static inline int kg_dec_field(kgdec *D, kbuf *out) {
    if (D->tg >= D->tge) return -1;
    uint8_t t = *D->tg++;
    if (t == 0) return 0;
    if (t == 1) {
        uint64_t z;
        if (D->mt < D->mte && *D->mt < 128) z = *D->mt++;
        else { int sh = 0; z = 0; for (;;) { if (D->mt >= D->mte || sh > 63) return -1; uint8_t c = *D->mt++; z |= (uint64_t)(c & 127) << sh; if (!(c & 128)) break; sh += 7; } }
        int64_t m = D->pred ? (int64_t)((uint64_t)D->prev + (uint64_t)kg_unzz(z)) : kg_unzz(z); D->prev = m;
        if (D->sc >= D->sce) return -1;
        uint8_t s = *D->sc++; if (s != 255 && s > 18) return -1;
        if (out->n + 24 > out->cap && kb_res(out, 24)) return -1;
        out->n = (size_t)(kg_put_num(out->p + out->n, m, s, D->sep) - out->p); return 0;
    }
    if (t == 2) {
        const uint8_t *e = D->tx < D->txe ? memchr(D->tx, '\n', (size_t)(D->txe - D->tx)) : NULL; if (!e) return -1;
        size_t l = (size_t)(e - D->tx); if (kb_put(out, D->tx, l)) return -1; D->tx = e + 1; return 0;
    }
    return -1;
}
static int kg_dec_done(const kgdec *D) { return D->tg == D->tge && D->sc == D->sce && D->mt == D->mte && D->tx == D->txe; }

/* ---- FIELDS ---- */
/* dec[j] (optional): column j is read through a fused DECIMAL reader instead of its text */
static int kg_fields_inv(uint8_t d, uint32_t k, const kbuf *shape, const kbuf *cols, kgdec **dec, kbuf *out) {
    size_t si = 0; uint64_t lines, gap = 0, len; int have = 0;
    if (kg_getuv(shape->p, shape->n, &si, &lines) || si >= shape->n) return -1;
    int fin = shape->p[si++]; if (fin > 1) return -1;
    size_t *cp = calloc(k, sizeof(size_t)); if (!cp) return -1;
    { size_t tot = shape->n + lines; for (uint32_t j = 0; j < k; j++) tot += cols[j].n; if (kb_res(out, tot + 16)) { free(cp); return -1; } }
    uint64_t next = UINT64_MAX;
    if (si < shape->n) { if (kg_getuv(shape->p, shape->n, &si, &gap)) goto bad; next = gap; have = 1; }
    for (uint64_t l = 0; l < lines; l++) {
        if (have && l == next) {
            if (kg_getuv(shape->p, shape->n, &si, &len) || len > shape->n - si || kb_put(out, shape->p + si, len)) goto bad;
            si += len; have = 0;
            if (si < shape->n) { if (kg_getuv(shape->p, shape->n, &si, &gap) || gap == 0) goto bad; next = l + gap; have = 1; }
        } else {
            for (uint32_t j = 0; j < k; j++) {                  /* fields are short: scan inline, write in place */
                if (dec && dec[j]) { if (kg_dec_field(dec[j], out)) goto bad; if (j + 1 < k) { if (out->n >= out->cap && kb_res(out, 1)) goto bad; out->p[out->n++] = d; } continue; }
                const kbuf *c = &cols[j]; const uint8_t *a = c->p + cp[j], *ce = c->p + c->n, *e = a;
                while (e < ce && *e != '\n') e++;
                if (e == ce) goto bad;
                size_t fl = (size_t)(e - a);
                if (kb_res(out, fl + 1)) goto bad;
                memcpy(out->p + out->n, a, fl); out->n += fl;
                if (j + 1 < k) out->p[out->n++] = d;
                cp[j] += fl + 1;
            }
        }
        if ((l + 1 < lines || fin) && kb_byte(out, '\n')) goto bad;
    }
    for (uint32_t j = 0; j < k; j++) if (dec && dec[j] ? !kg_dec_done(dec[j]) : cp[j] != cols[j].n) goto bad;
    if (have || si != shape->n) goto bad;
    free(cp); return 0;
bad: free(cp); return -1;
}

/* ---- INT ---- */
static inline uint64_t kg_ld(const uint8_t *p, int w, int sg) {      /* little-endian load (as the rest of kolmo) */
    uint64_t v;
    if (w == 8) { memcpy(&v, p, 8); return v; }
    if (w == 4) { uint32_t u; memcpy(&u, p, 4); return sg ? (uint64_t)(int64_t)(int32_t)u : u; }
    if (w == 2) { uint16_t u; memcpy(&u, p, 2); return sg ? (uint64_t)(int64_t)(int16_t)u : u; }
    return sg ? (uint64_t)(int64_t)(int8_t)p[0] : p[0];
}
static int kg_int_inv(const uint8_t *par, size_t plen, const kbuf *res, const kbuf *rest, size_t maxs, kbuf *out) {
    size_t pi = 3; uint64_t h, cnt, g, bz, sd[2] = {0, 0};
    if (plen < 3) return -1;
    int w = par[0], fl = par[1], ow = par[2], ord = (fl >> 1) & 3, sg = fl & 1, pln = (fl >> 3) & 1;
    if ((w != 1 && w != 2 && w != 4 && w != 8) || (ow != 1 && ow != 2 && ow != 4 && ow != 8) || ord > 2 || (fl & ~15)) return -1;
    if (kg_getuv(par, plen, &pi, &h) || kg_getuv(par, plen, &pi, &cnt) || kg_getuv(par, plen, &pi, &g) || kg_getuv(par, plen, &pi, &bz) || g == 0) return -1;
    for (int q = 0; q < ord; q++) if (kg_getuv(par, plen, &pi, &sd[q])) return -1;
    if (pi != plen || h > rest->n || (uint64_t)ord > cnt || cnt > maxs / (unsigned)w || (cnt - (uint64_t)ord) * (unsigned)ow != res->n
        || cnt * (unsigned)w + rest->n > maxs) return -1;
    if (kb_res(out, (size_t)(cnt * w) + rest->n + 16)) return -1;
    if (h) memcpy(out->p, rest->p, (size_t)h);
    uint8_t *wp = out->p + h; uint64_t v1 = 0, v2 = 0, bias = (uint64_t)kg_unzz(bz); size_t m = (size_t)(cnt - (uint64_t)ord);
    for (size_t q = 0; q < (size_t)cnt; q++) {
        uint64_t v;
        if (q < (size_t)ord) v = (uint64_t)kg_unzz(sd[q]);
        else {
            size_t r = q - (size_t)ord; uint64_t z = 0;
            if (pln) for (int b = 0; b < ow; b++) z |= (uint64_t)res->p[(size_t)b * m + r] << (8 * b);
            else for (int b = 0; b < ow; b++) z |= (uint64_t)res->p[r * (size_t)ow + b] << (8 * b);
            if (ord == 0) v = bias + z * g;
            else { uint64_t d = (uint64_t)kg_unzz(z) * g; v = (ord == 1 ? v1 : 2 * v1 - v2) + d; }
        }
        if (sg && w < 8) { int sh = 64 - 8 * w; v = (uint64_t)((int64_t)(v << sh) >> sh); } else if (w < 8) v &= ((uint64_t)1 << (8 * w)) - 1;
        for (int b = 0; b < w; b++) *wp++ = (uint8_t)(v >> (8 * b));
        v2 = v1; v1 = v;
    }
    memcpy(wp, rest->p + h, rest->n - (size_t)h); out->n = (size_t)(cnt * w) + rest->n;
    return 0;
}

/* ---- FLOAT ---- */
static int kg_float_inv(const uint8_t *par, size_t plen, const kbuf *pl, const kbuf *rest, size_t maxs, kbuf *out) {
    size_t pi = 1; uint64_t h, cnt; if (plen < 1) return -1;
    int w = par[0]; if (w != 2 && w != 4 && w != 8) return -1;
    if (kg_getuv(par, plen, &pi, &h) || kg_getuv(par, plen, &pi, &cnt) || pi != plen || h > rest->n || cnt > maxs / (unsigned)w
        || cnt * (unsigned)w + rest->n > maxs) return -1;
    for (int b = 0; b < w; b++) if (pl[b].n != cnt) return -1;
    if (kb_res(out, (size_t)(cnt * w) + rest->n + 16)) return -1;
    if (h) memcpy(out->p, rest->p, (size_t)h);
    uint8_t *o = out->p + h; const int bits = 8 * w;
    if (w == 4) {
        const uint8_t *p0 = pl[0].p, *p1 = pl[1].p, *p2 = pl[2].p, *p3 = pl[3].p;
        for (size_t q = 0; q < (size_t)cnt; q++) {
            uint32_t v = (uint32_t)p0[q] | (uint32_t)p1[q] << 8 | (uint32_t)p2[q] << 16 | (uint32_t)p3[q] << 24;
            v = (v >> 1) | (v << 31); o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); o[2] = (uint8_t)(v >> 16); o[3] = (uint8_t)(v >> 24); o += 4;
        }
    } else if (w == 2) {
        const uint8_t *p0 = pl[0].p, *p1 = pl[1].p;
        for (size_t q = 0; q < (size_t)cnt; q++) { unsigned v = (unsigned)p0[q] | (unsigned)p1[q] << 8; v = ((v >> 1) | (v << 15)) & 0xffff; o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); o += 2; }
    } else for (size_t q = 0; q < (size_t)cnt; q++) {
        uint64_t v = 0; for (int b = 0; b < w; b++) v |= (uint64_t)pl[b].p[q] << (8 * b);
        v = (v >> 1) | ((v & 1) << (bits - 1));                          /* rotate right by one */
        for (int b = 0; b < w; b++) *o++ = (uint8_t)(v >> (8 * b));
    }
    memcpy(o, rest->p + h, rest->n - (size_t)h); out->n = (size_t)(cnt * w) + rest->n;
    return 0;
}

/* ---- DEC64 ---- */
#define KG_DEC_MAXE 22            /* 10^22 is the largest power of ten a binary64 holds exactly */
#define KG_DEC_MAXCOLS 65536
static const double KG_P10[KG_DEC_MAXE + 1] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                               1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
static inline uint64_t kg_ld64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline uint64_t kg_dbits(double d) { uint64_t v; memcpy(&v, &d, 8); return v; }
/* st[0] tags, st[1..planes] planes, st[planes+1] exceptions, st[planes+2] rest */
static int kg_dec64_inv(const uint8_t *par, size_t plen, const kbuf *st, uint32_t nout, size_t maxs, kbuf *out) {
    size_t pi = 1; uint64_t h = 0, rows = 0, cols;
    if (plen < 1 || par[0] > 1) return -1;
    const int segs = par[0];
    if (!segs && (kg_getuv(par, plen, &pi, &h) || kg_getuv(par, plen, &pi, &rows))) return -1;
    if (kg_getuv(par, plen, &pi, &cols) || pi >= plen) return -1;
    int np = par[pi++]; if (np < 1 || np > 8 || nout != (uint32_t)np + 3) return -1;
    const kbuf *tg = &st[0], *pl = &st[1], *ex = &st[np + 1], *rest = &st[np + 2];
    if (!cols || cols > KG_DEC_MAXCOLS || tg->n > maxs / 4 || rest->n > maxs) return -1;
    if (!segs && (!rows || rows > tg->n / cols || rows * cols != tg->n || h > rest->n)) return -1;
    const uint64_t cnt = tg->n;
    for (int b = 0; b < np; b++) if (pl[b].n != cnt) return -1;
    const uint8_t *t = tg->p;
    /* pass 1: the columns' parameters, and the tags checked and counted, so that each column knows where its
     * exceptions start and the output size is known. pass 2 then needs no checks, and a table is rebuilt a band of
     * rows at a time (every column's part of the band), which keeps the writes inside the cache. */
    typedef struct { double P; uint64_t miss, prev, pd, dmask, d2mask, skip, n; const uint8_t *xp; int isint, w; } kgdc;
    kgdc *C = malloc((size_t)cols * sizeof(kgdc)); if (!C) return -1;
    int rc = -1; uint64_t xb = 0, q = 0, skips = 0, vb = 0;
    for (uint64_t c = 0; c < cols; c++) {
        uint64_t skip = 0, rn = rows;
        if (segs && (kg_getuv(par, plen, &pi, &skip) || kg_getuv(par, plen, &pi, &rn) || !rn || rn > cnt - q || skip > rest->n - skips)) goto bad;
        if (plen - pi < 2) goto bad;
        int e = par[pi], fl = par[pi + 1]; pi += 2; if (e > KG_DEC_MAXE || fl > 31 || ((fl & 16) && !(fl & 1))) goto bad;
        const int w = fl & 8 ? 4 : 8;
        if (!segs && c && w != C[0].w) goto bad;                 /* a table has one word size */
        uint64_t miss = 0; if (fl & 2) { if (plen - pi < 8) goto bad; miss = kg_ld64(par + pi); pi += 8; }
        unsigned mx = 0; uint64_t n1 = 0, n2 = 0;
        for (uint64_t r = 0; r < rn; r++) { unsigned g = t[q + r]; mx |= g; n1 += g == 1; n2 += g == 2; }
        if (mx > 3) goto bad;
        if (mx == 3) for (uint64_t r = 0; r < rn; r++) if (t[q + r] == 3) goto bad;   /* 1 | 2 also gives 3 */
        if ((n1 && !(fl & 2)) || n2 * (uint64_t)w > ex->n - xb) goto bad;
        C[c] = (kgdc){KG_P10[e], miss, 0, 0, fl & 1 ? ~(uint64_t)0 : 0, fl & 16 ? ~(uint64_t)0 : 0, skip, rn, ex->p + xb, fl & 4, w};
        xb += n2 * (uint64_t)w; q += rn; skips += skip; vb += rn * (uint64_t)w;
    }
    if (pi != plen || xb != ex->n || q != cnt || vb + rest->n > maxs) goto bad;
    if (kb_res(out, (size_t)vb + rest->n + 16)) goto bad;
/* one value of column K at stream index Q, written at W8 (the column's word size) */
#define KG_DEC_VALUE(K, Q, W8) do { const unsigned tag_ = t[Q]; uint64_t bits_;                         \
        if (tag_ == 0) { uint64_t z_ = 0; for (int b_ = 0; b_ < np; b_++) z_ |= (uint64_t)pl[b_].p[Q] << (8 * b_); \
                         uint64_t v_ = (uint64_t)kg_unzz(z_) + (pd & d2mask); pd = v_;                   \
                         uint64_t m_ = v_ + (prev & dmask); prev = m_;                                   \
                         if (w4) { if (isint) bits_ = (uint32_t)m_; else { float f_ = (float)((double)(int64_t)m_ / P); uint32_t u_; memcpy(&u_, &f_, 4); bits_ = u_; } } \
                         else bits_ = isint ? m_ : kg_dbits((double)(int64_t)m_ / P); }                  \
        else if (tag_ == 1) bits_ = (K)->miss;                                                           \
        else if (w4) { uint32_t u_; memcpy(&u_, xp, 4); bits_ = u_; xp += 4; }                           \
        else { bits_ = kg_ld64(xp); xp += 8; }                                                           \
        if (w4) { uint32_t u_ = (uint32_t)bits_; memcpy(W8, &u_, 4); } else memcpy(W8, &bits_, 8); } while (0)
    if (segs) {
        uint8_t *w = out->p; const uint8_t *rp = rest->p; size_t qq = 0;
        for (uint64_t c = 0; c < cols; c++) {
            kgdc *K = &C[c]; const double P = K->P; uint64_t prev = 0, pd = 0; const uint64_t dmask = K->dmask, d2mask = K->d2mask; const uint8_t *xp = K->xp;
            const int isint = K->isint, w4 = K->w == 4; const size_t ws = (size_t)K->w;
            if (K->skip) { memcpy(w, rp, (size_t)K->skip); w += K->skip; rp += K->skip; }
            for (size_t r = 0; r < (size_t)K->n; r++, qq++, w += ws) KG_DEC_VALUE(K, qq, w);
        }
        memcpy(w, rp, rest->n - (size_t)skips);
    } else {
        uint8_t *o = out->p + h; const size_t band = 128, ws = (size_t)C[0].w, step = (size_t)cols * ws;
        if (h) memcpy(out->p, rest->p, (size_t)h);
        for (uint64_t r0 = 0; r0 < rows; r0 += band) {
            const size_t rn = (size_t)(rows - r0 < band ? rows - r0 : band);
            for (uint64_t c = 0; c < cols; c++) {
                const size_t q0 = (size_t)(c * rows + r0);
                kgdc *K = &C[c]; const double P = K->P; uint64_t prev = K->prev, pd = K->pd; const uint64_t dmask = K->dmask, d2mask = K->d2mask; const uint8_t *xp = K->xp;
                const int isint = K->isint, w4 = K->w == 4;
                uint8_t *w = o + ((size_t)r0 * (size_t)cols + (size_t)c) * ws;
                for (size_t r = 0; r < rn; r++, w += step) KG_DEC_VALUE(K, q0 + r, w);
                K->prev = prev; K->pd = pd; K->xp = xp;
            }
        }
        memcpy(o + (size_t)vb, rest->p + h, rest->n - (size_t)h);
    }
#undef KG_DEC_VALUE
    out->n = (size_t)vb + rest->n;
    rc = 0;
bad:
    free(C); return rc;
}

/* ---- program builder ---- */
typedef struct { kbuf *s; int ns, cap; uint8_t *used; kbuf prog; int nodes; } kgb;
static int kgb_new(kgb *g) {
    int ns = g->ns; if (ns >= KG_MAXSTREAMS) return -1;
    if (ns == g->cap) {
        int c = g->cap ? g->cap * 2 : 64; kbuf *q = realloc(g->s, (size_t)c * sizeof(kbuf)); uint8_t *u = realloc(g->used, (size_t)c);
        if (!q || !u) { if (q) g->s = q; if (u) g->used = u; return -1; }
        g->s = q; g->used = u; g->cap = c;
    }
    memset(&g->s[ns], 0, sizeof(kbuf)); g->used[ns] = 0; g->ns++; return ns;
}
/* record a node with inputs in[0..nin) and nout fresh outputs, returning the first output index */
static int kgb_node(kgb *g, uint8_t op, const int *in, int nin, int nout, const kbuf *par) {
    int first = g->ns;
    for (int q = 0; q < nout; q++) if (kgb_new(g) < 0) return -1;
    if (kb_byte(&g->prog, op) || kb_uv(&g->prog, (uint64_t)nin)) return -1;
    for (int q = 0; q < nin; q++) { if (kb_uv(&g->prog, (uint64_t)in[q])) return -1; g->used[in[q]] = 1; }
    if (kb_uv(&g->prog, (uint64_t)nout) || kb_uv(&g->prog, par ? par->n : 0) || (par && kb_put(&g->prog, par->p, par->n))) return -1;
    g->nodes++; return first;
}
static void kgb_free(kgb *g) { for (int q = 0; q < g->ns; q++) free(g->s[q].p); free(g->s); free(g->used); free(g->prog.p); }

/* concatenate streams idx[0..n) into one, freeing them */
static int kgb_concat(kgb *g, const int *idx, int n) {
    kbuf par = {0}; if (kb_uv(&par, (uint64_t)n)) return -1;
    for (int q = 0; q < n; q++) if (kb_uv(&par, g->s[idx[q]].n)) { free(par.p); return -1; }
    int o = kgb_node(g, KG_CONCAT, idx, n, 1, &par); free(par.p); if (o < 0) return -1;
    for (int q = 0; q < n; q++) { if (kb_put(&g->s[o], g->s[idx[q]].p, g->s[idx[q]].n)) return -1; free(g->s[idx[q]].p); memset(&g->s[idx[q]], 0, sizeof(kbuf)); }
    return o;
}

/* serialize a built program: header, program, leaves (every unconsumed stream, in index order). Returns the payload
 * length, or 0 when it is not smaller than n or does not fit in cap. */
static size_t kg_serialize(codec_ctx *x, int codec, int level, kgb *g, size_t n, uint8_t *dst, size_t cap) {
    kbuf o = {0}; int nleaf = 0; size_t out = 0;
    for (int q = 0; q < g->ns; q++) nleaf += !g->used[q];
    if (kb_byte(&o, 'G') || kb_byte(&o, 1) || kb_uv(&o, (uint64_t)g->nodes) || kb_put(&o, g->prog.p, g->prog.n) || kb_uv(&o, (uint64_t)nleaf)) goto done;
    for (int q = 0; q < g->ns; q++) {
        if (g->used[q]) continue;
        kbuf *s = &g->s[q]; size_t zc = s->n + s->n / 8 + 1024; int cc = codec == C_CM || codec == C_STORE ? C_LZMA : codec;
        if (kb_uv(&o, (uint64_t)q) || kb_res(&o, 1 + 20 + zc)) goto done;
        size_t hp = o.n; o.n += 1;
        /* fast settings, large leaves: three 32 KiB samples decide between stored (no gain either way), Huffman only
         * (it matches or beats zstd -1: skewed bytes without repeats) and the block codec */
        int flat = 0, hufonly = 0, nohuf = 0;
        if (s->n >= (256u << 10) && cc == C_ZSTD && level <= CZ_FAST_LEVEL) {
            size_t zs = 0, hs = 0, sn = 32768; uint8_t *sb = o.p + o.n + 20;
            for (int q = 1; q <= 3; q++) {
                const uint8_t *sp = s->p + ((s->n / 4 * q - sn / 2) & ~(size_t)63);
                size_t a1 = codec_compress(x, C_ZSTD, 1, sp, sn, sb, zc), a2 = codec_compress(x, C_HUF, 0, sp, sn, sb, zc);
                zs += a1 && a1 < sn ? a1 : sn; hs += a2 && a2 < sn ? a2 : sn;
            }
            flat = zs * 100 >= 3 * sn * 99 && hs * 100 >= 3 * sn * 99;
            hufonly = !flat && hs * 100 <= zs * 97;
            nohuf = hs * 100 > zs * 110;                        /* Huffman a tenth behind on the samples: zstd alone */
            if (hufonly) {
                /* the three samples can all land in the busy part of a leaf that is mostly one byte value (a high
                 * byte plane that only a few columns use). Huffman cannot go below one bit per byte there, where
                 * zstd codes the runs for almost nothing: 164 KB against 3 KB on one plane (found by the E02C
                 * research harness). When one value fills half the leaf, both are tried. */
                uint32_t hc[256], top = 0; kg_hist(s->p, s->n, hc);
                for (int c = 0; c < 256; c++) if (hc[c] > top) top = hc[c];
                if ((uint64_t)top * 2 >= s->n) hufonly = 0;
            }
        }
        size_t z = s->n >= 64 && !flat && !hufonly ? codec_compress(x, cc, level, s->p, s->n, o.p + o.n + 20, zc) : 0;
        int tryh = s->n >= 64 && !flat, tryhuf = 1;
        const int fastleaf = codec == C_ZSTD && level <= CZ_FAST_LEVEL;   /* no rANS at these settings */
        if (tryh && z && nohuf && fastleaf) tryh = 0;
        else if (tryh && z) {                                 /* Huffman cannot beat the order-0 entropy: skip when z is below it */
            double bits = 0, hbits = 0;                       /* per 128 KiB chunk, as Huffman codes them */
            for (size_t a = 0; a < s->n; a += 131072) {
                size_t m = s->n - a < 131072 ? s->n - a : 131072; uint32_t hc[256]; double e = 0; int nd = 0;
                kg_hist(s->p + a, m, hc);
                for (int c = 0; c < 256; c++) if (hc[c]) { e -= (double)hc[c] * log2((double)hc[c] / (double)m); nd++; }
                bits += e; hbits += nd > 1 && e < (double)m ? (double)m : e;   /* a prefix code spends a whole bit per byte */
            }
            if ((double)z * 8 <= bits) tryh = 0;
            tryhuf = (double)z * 8 > hbits && !nohuf;         /* (rANS below is bounded by the entropy alone) */
        }
        if (tryh && (tryhuf || !fastleaf)) {   /* Huffman only, when smaller (skewed bytes without repeats) */
            uint8_t *hb = malloc(zc); size_t hz = hb && tryhuf ? codec_compress(x, C_HUF, 0, s->p, s->n, hb, zc) : 0;
            if (hz && (!z || hz < z)) { memcpy(o.p + o.n + 20, hb, hz); z = hz; cc = C_HUF; }
            int slow = !fastleaf;                                                           /* rANS at hc/max/ultra only */
            size_t rz = hb && slow ? codec_compress(x, C_RANS, 0, s->p, s->n, hb, zc) : 0;   /* rANS: whole-bit loss of Huffman */
            /* rANS decodes about four times slower than Huffman or zstd here: with the zstd family (hc: fast reads) it
             * must be at least 3% smaller to be taken; max and ultra take any gain */
            if (rz && (!z || (codec == C_ZSTD ? (uint64_t)rz * 100 <= (uint64_t)z * 97 : rz < z))) { memcpy(o.p + o.n + 20, hb, rz); z = rz; cc = C_RANS; }
            free(hb);
        }
        if (!z || z >= s->n) { cc = C_STORE; z = s->n; if (z) memcpy(o.p + o.n + 20, s->p, z); }
        o.p[hp] = (uint8_t)cc;
        uint8_t hdr[20]; size_t hn = 0;
        for (uint64_t v = s->n;; v >>= 7) { hdr[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (uint64_t v = z;; v >>= 7) { hdr[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        memmove(o.p + o.n + hn, o.p + o.n + 20, z); memcpy(o.p + o.n, hdr, hn); o.n += hn + z;
    }
    if (o.n < cap && o.n < n) { memcpy(dst, o.p, o.n); out = o.n; }
done:
    free(o.p); return out;
}

/* Delimited numeric table: FIELDS, then DECIMAL on each mostly-numeric column, then one CONCAT per stream kind.
 * The encoder fuses the nodes: each field is scanned once and written straight to its column's final streams (the
 * intermediate column texts exist only in the decoder). Writes the payload to dst; returns its length, or 0 when the
 * graph does not apply or does not fit. */
typedef struct { kbuf tag, sc, mt, tx, raw; int64_t prev; uint8_t num, pred; } kgcol;
static inline uint8_t *kg_uvw(uint8_t *q, uint64_t v) { while (v >= 128) { *q++ = (uint8_t)(v | 128); v >>= 7; } *q++ = (uint8_t)v; return q; }
static size_t kg_encode_table(codec_ctx *x, int codec, int level, const uint8_t *src, size_t n, uint8_t d, uint8_t sep,
                              uint8_t *dst, size_t cap) {
    if (n < 64) return 0;
    uint32_t k = 1;                                            /* field count of the second line (the first may be a header) */
    { const uint8_t *e = memchr(src, '\n', n), *a = e && (size_t)(e - src) + 1 < n ? e + 1 : src;
      const uint8_t *b = memchr(a, '\n', n - (size_t)(a - src)); if (!b) b = src + n;
      for (const uint8_t *q = a; q < b; q++) k += *q == d; }
    if (k < 2 || k > 60000) return 0;
    size_t out = 0, lines = 0, last = 0, rows = 0, i = 0; int fin = src[n - 1] == '\n';
    kgcol *C = calloc(k, sizeof(kgcol)); size_t *fs = malloc((k + 1) * sizeof(size_t)); kbuf irr = {0}, shape = {0};
    kgb g = {0};
    if (!C || !fs) goto done;
    /* classify columns and pick each predictor on the first lines */
    { size_t *vals = calloc(k, sizeof(size_t)), *nums = calloc(k, sizeof(size_t)), *c0 = calloc(k, sizeof(size_t)), *c1 = calloc(k, sizeof(size_t));
      int64_t *pv = calloc(k, sizeof(int64_t));
      if (!vals || !nums || !c0 || !c1 || !pv) { free(vals); free(nums); free(c0); free(c1); free(pv); goto done; }
      size_t j0 = 0; int seen = 0;
      /* about 256K fields, as runs of 16 consecutive lines spread over the block (delta costs counted inside a run) */
      size_t lim = 262144 / k < 16 ? 16 : 262144 / k; if (lim > 4096) lim = 4096;
      if (!(codec == C_ZSTD && level <= CZ_FAST_LEVEL)) lim = 4096;           /* slower settings: up to 4096 lines */
      size_t l0 = 0; { const uint8_t *e0 = memchr(src, '\n', n); l0 = e0 ? (size_t)(e0 - src) + 1 : n; }
      size_t est = n / (l0 ? l0 : 1), runs = (lim + 15) / 16, skip = est > lim ? (est - lim) / runs : 0; int rl = 0;
      while (j0 < n && seen < (int)lim) {
          if (rl == 16) { rl = 0; for (size_t q = 0; q < skip && j0 < n; q++) { const uint8_t *e = memchr(src + j0, '\n', n - j0); j0 = e ? (size_t)(e - src) + 1 : n; } if (j0 >= n) break; }
          const uint8_t *e = memchr(src + j0, '\n', n - j0); size_t end = e ? (size_t)(e - src) : n, a = j0; uint32_t j = 0;
          uint32_t f = 1; for (size_t q = j0; q < end; q++) f += src[q] == d;
          if (f == k) for (;; j++) {
              size_t bq = a; while (bq < end && src[bq] != d) bq++;
              int64_t m; uint8_t sc; vals[j]++;
              if (bq == a) nums[j]++;
              else if (kg_num(src + a, bq - a, sep, &m, &sc)) {
                  nums[j]++; uint64_t z0 = kg_zz(m), z1 = kg_zz((int64_t)((uint64_t)m - (uint64_t)pv[j])); pv[j] = m;
                  if (rl) { c0[j] += 1 + (z0 >= 128) + (z0 >= 16384) + (z0 >= 2097152); c1[j] += 1 + (z1 >= 128) + (z1 >= 16384) + (z1 >= 2097152); }
              }
              if (bq >= end) break; a = bq + 1;
          }
          seen++; rl++; j0 = end + 1;
      }
      for (uint32_t j = 0; j < k; j++) { C[j].num = vals[j] && nums[j] * 100 >= vals[j] * 60; C[j].pred = c1[j] <= c0[j]; }
      free(vals); free(nums); free(c0); free(c1); free(pv); }
    {   size_t nl = 1; for (const uint8_t *q = src, *e; (e = memchr(q, '\n', n - (size_t)(q - src))); q = e + 1) nl++;
        for (uint32_t j = 0; j < k; j++) if (C[j].num && (kb_res(&C[j].tag, nl) || kb_res(&C[j].sc, nl) || kb_res(&C[j].mt, 16))) goto done; }
    /* one pass: fields straight to their streams */
    while (i < n) {
        const uint8_t *e = memchr(src + i, '\n', n - i); size_t end = e ? (size_t)(e - src) : n;
        uint32_t f = 0; fs[f++] = i;
        {   const uint8_t *q = src + i, *qe = src + end; const uint64_t dd = 0x0101010101010101ull * d;
            for (; q + 8 <= qe && f <= k; q += 8) {           /* eight bytes at a time: the bytes equal to the delimiter */
                uint64_t w; memcpy(&w, q, 8); w = kg_le64(w) ^ dd;
                uint64_t hit = ~(((w & 0x7f7f7f7f7f7f7f7full) + 0x7f7f7f7f7f7f7f7full) | w | 0x7f7f7f7f7f7f7f7full);
                for (; hit; hit &= hit - 1) { if (f > k) break; fs[f++] = (size_t)(q - src) + ((size_t)__builtin_ctzll(hit) >> 3) + 1; }
            }
            for (; q < qe; q++) if (*q == d) { if (f > k) break; fs[f++] = (size_t)(q - src) + 1; }
        }
        if (f == k) {
            fs[k] = end + 1;
            for (uint32_t j = 0; j < k; j++) {
                kgcol *c = &C[j]; const uint8_t *v = src + fs[j]; size_t l = fs[j + 1] - 1 - fs[j];
                if (!c->num) { if (kb_put(&c->raw, v, l) || kb_byte(&c->raw, '\n')) goto done; continue; }
                int64_t m; uint8_t sc;
                if (c->mt.cap - c->mt.n < 10 && kb_res(&c->mt, 10)) goto done;   /* tags and scales were reserved above */
                if (!l) c->tag.p[c->tag.n++] = 0;
                else if (kg_num(v, l, sep, &m, &sc)) {
                    c->tag.p[c->tag.n++] = 1; c->sc.p[c->sc.n++] = sc;
                    uint8_t *q = kg_uvw(c->mt.p + c->mt.n, kg_zz(c->pred ? (int64_t)((uint64_t)m - (uint64_t)c->prev) : m));
                    c->mt.n = (size_t)(q - c->mt.p); c->prev = m;
                } else { c->tag.p[c->tag.n++] = 2; if (kb_put(&c->tx, v, l) || kb_byte(&c->tx, '\n')) goto done; }
            }
            rows++;
        } else if (kb_uv(&irr, lines - last) || kb_uv(&irr, end - i) || kb_put(&irr, src + i, end - i)) goto done;
        else last = lines;
        lines++; i = end + 1;
    }
    if (!rows || kb_uv(&shape, lines) || kb_byte(&shape, (uint8_t)fin) || kb_put(&shape, irr.p, irr.n)) goto done;
    /* the program: FIELDS, DECIMAL per numeric column, CONCAT per stream kind (column streams stay virtual) */
    if (kgb_new(&g) != 0) goto done;
    { kbuf par = {0}; kb_byte(&par, d); kb_uv(&par, k);
      int first = kgb_node(&g, KG_FIELDS, (int[]){0}, 1, (int)k + 1, &par); free(par.p); if (first != 1) goto done; }
    g.s[1] = shape; memset(&shape, 0, sizeof shape);
    int *lists = malloc(sizeof(int) * k * 5); if (!lists) goto done;
    int *tg = lists, *scs = lists + k, *mts = lists + 2 * k, *txs = lists + 3 * k, *raws = lists + 4 * k, nt = 0, nraw = 0;
    for (uint32_t j = 0; j < k; j++) {
        int c = 2 + (int)j;
        if (!C[j].num) { g.s[c] = C[j].raw; memset(&C[j].raw, 0, sizeof(kbuf)); raws[nraw++] = c; continue; }
        kbuf par = {0}; kb_byte(&par, sep); kb_byte(&par, C[j].pred);
        int o = kgb_node(&g, KG_DECIMAL, &c, 1, 4, &par); free(par.p); if (o < 0) { free(lists); goto done; }
        g.s[o] = C[j].tag; g.s[o + 1] = C[j].sc; g.s[o + 2] = C[j].mt; g.s[o + 3] = C[j].tx;
        memset(&C[j].tag, 0, sizeof(kbuf)); memset(&C[j].sc, 0, sizeof(kbuf)); memset(&C[j].mt, 0, sizeof(kbuf)); memset(&C[j].tx, 0, sizeof(kbuf));
        tg[nt] = o; scs[nt] = o + 1; mts[nt] = o + 2; txs[nt] = o + 3; nt++;
    }
    if ((nt && (kgb_concat(&g, tg, nt) < 0 || kgb_concat(&g, scs, nt) < 0 || kgb_concat(&g, mts, nt) < 0 || kgb_concat(&g, txs, nt) < 0))
        || (nraw && kgb_concat(&g, raws, nraw) < 0)) { free(lists); goto done; }
    free(lists);
    out = kg_serialize(x, codec, level, &g, n, dst, cap);
done:
    if (C) for (uint32_t j = 0; j < k; j++) { free(C[j].tag.p); free(C[j].sc.p); free(C[j].mt.p); free(C[j].tx.p); free(C[j].raw.p); }
    free(C); free(fs); free(irr.p); free(shape.p);
    kgb_free(&g);
    return out;
}

/* Integer array: one INT node. The width w and head length h come from the planner; the order, signedness, gcd and
 * output width are chosen here for the block. Returns the payload length, or 0 when the graph does not apply. */
static uint64_t kg_gcd(uint64_t a, uint64_t b) { while (b) { uint64_t t = a % b; a = b; b = t; } return a; }
static int kg_bw(uint64_t z) { return z < 256 ? 1 : z < 65536 ? 2 : z < 4294967296ull ? 4 : 8; }
/* choose signedness and order on a sample (16 runs of 1024 consecutive values) by the summed bit length of the
 * residuals, the sample's gcd taken into account; returns the estimated bits per value */
static double kg_num_choose_n(const uint8_t *a, size_t cnt, int w, int *bo, int *bs, size_t nruns) {
    size_t runs = cnt > 16384 ? nruns : 1, rl = cnt > 16384 ? 1024 : cnt, tot = 0; double bcost = 1e300;
    for (int sg = (w == 8); sg <= 1; sg++) for (int ord = 0; ord <= 2; ord++) {
        uint64_t g = 0; int64_t mn = INT64_MAX; double cost = 0; tot = 0;
        for (int pass = 0; pass < 2; pass++) for (size_t r = 0; r < runs; r++) {
            const uint8_t *b = a + (cnt - rl) / (runs > 1 ? runs - 1 : 1) * r * (size_t)w; uint64_t v1 = 0, v2 = 0;
            for (size_t q = 0; q < rl; q++) {
                uint64_t v = kg_ld(b + q * w, w, sg), z;
                if (ord == 0) {
                    if (!pass) { if ((int64_t)v < mn) mn = (int64_t)v; continue; }
                    z = v - (uint64_t)mn; if (g > 1) z /= g;           /* (a division only where there is a common factor) */
                } else if ((int)q < ord) { v2 = v1; v1 = v; continue; }
                else {
                    int64_t d = (int64_t)(v - (ord == 1 ? v1 : 2 * v1 - v2));
                    if (!pass) { uint64_t r0 = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; if (g != 1) g = kg_gcd(r0, g); v2 = v1; v1 = v; continue; }
                    z = kg_zz(g > 1 ? d / (int64_t)g : d);
                }
                cost += z ? 65 - __builtin_clzll(z) : 1; tot++;
                v2 = v1; v1 = v;
            }
            if (!pass && ord == 0 && r + 1 == runs) for (size_t r2 = 0; r2 < runs; r2++) {   /* gcd of v - min */
                const uint8_t *b2 = a + (cnt - rl) / (runs > 1 ? runs - 1 : 1) * r2 * (size_t)w;
                for (size_t q = 0; q < rl && g != 1; q++) g = kg_gcd(kg_ld(b2 + q * w, w, sg) - (uint64_t)mn, g);
            }
        }
        if (tot && cost / tot < bcost) { bcost = cost / tot; *bo = ord; *bs = sg; }
    }
    return bcost;
}
static double kg_num_choose(const uint8_t *a, size_t cnt, int w, int *bo, int *bs) { return kg_num_choose_n(a, cnt, w, bo, bs, 16); }
static size_t kg_encode_num(codec_ctx *x, int codec, int level, const uint8_t *src, size_t n, int w, size_t h,
                            uint8_t *dst, size_t cap) {
    if (h > n || (n - h) / (size_t)w < 16 || (w != 2 && w != 4 && w != 8)) return 0;
    size_t cnt = (n - h) / (size_t)w, out = 0; const uint8_t *a = src + h;
    int bo = 0, bs = 1; kg_num_choose(a, cnt, w, &bo, &bs);
    /* the chosen predictor over the whole block: bias (order 0), exact gcd, largest residual */
    uint64_t bg = 0, bbias = 0, bmax = 0;
    {   uint64_t v1 = 0, v2 = 0; int64_t mn = INT64_MAX;
        if (bo == 0) { for (size_t q = 0; q < cnt; q++) { int64_t v = (int64_t)kg_ld(a + q * w, w, bs); if (v < mn) mn = v; } bbias = (uint64_t)mn; }
        for (size_t q = 0; q < cnt; q++) {
            uint64_t v = kg_ld(a + q * w, w, bs), r;
            if (bo == 0) r = v - bbias;
            else if ((int)q < bo) { v2 = v1; v1 = v; continue; }
            else { int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)); r = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; }
            if (bg != 1) bg = kg_gcd(r, bg);
            v2 = v1; v1 = v;
        }
        if (!bg) bg = 1;
        v1 = v2 = 0;
        for (size_t q = 0; q < cnt; q++) {
            uint64_t v = kg_ld(a + q * w, w, bs), z;
            if (bo == 0) z = (v - bbias) / bg;
            else if ((int)q < bo) { v2 = v1; v1 = v; continue; }
            else z = kg_zz((int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)) / (int64_t)bg);
            if (z > bmax) bmax = z;
            v2 = v1; v1 = v;
        }
    }
    int ow = kg_bw(bmax), pln = ow > 1; size_t m = cnt - (size_t)bo;
    kgb g = {0}; kbuf par = {0};
    if (kgb_new(&g) != 0) goto done;
    kb_byte(&par, (uint8_t)w); kb_byte(&par, (uint8_t)(bs | bo << 1 | pln << 3)); kb_byte(&par, (uint8_t)ow);
    kb_uv(&par, h); kb_uv(&par, cnt); kb_uv(&par, bg); kb_uv(&par, kg_zz((int64_t)bbias));
    for (int q = 0; q < bo; q++) kb_uv(&par, kg_zz((int64_t)kg_ld(a + (size_t)q * w, w, bs)));
    if (!par.p) goto done;
    int o = kgb_node(&g, KG_INT, (int[]){0}, 1, 2, &par); if (o != 1) goto done;
    if (kb_res(&g.s[1], m * (size_t)ow + 1) || kb_res(&g.s[2], n - cnt * (size_t)w + 1)) goto done;
    {   uint8_t *r = g.s[1].p; uint64_t v1 = 0, v2 = 0;
        for (size_t q = 0; q < cnt; q++) {
            uint64_t v = kg_ld(a + q * w, w, bs), z;
            if ((int)q < bo) { v2 = v1; v1 = v; continue; }
            z = bo == 0 ? (v - bbias) / bg : kg_zz((int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)) / (int64_t)bg);
            size_t i = q - (size_t)bo;
            if (pln) for (int b = 0; b < ow; b++) r[(size_t)b * m + i] = (uint8_t)(z >> (8 * b));
            else r[i] = (uint8_t)z;
            v2 = v1; v1 = v;
        }
        g.s[1].n = m * (size_t)ow;
        kb_put(&g.s[2], src, h); kb_put(&g.s[2], a + cnt * (size_t)w, n - h - cnt * (size_t)w); }
    out = kg_serialize(x, codec, level, &g, n, dst, cap);
done:
    free(par.p); kgb_free(&g); return out;
}

/* Float array: one FLOAT node, each byte plane its own leaf (mantissa planes usually end up stored). */
static size_t kg_encode_float(codec_ctx *x, int codec, int level, const uint8_t *src, size_t n, int w, size_t h,
                              uint8_t *dst, size_t cap) {
    if (h > n || (n - h) / (size_t)w < 16 || (w != 2 && w != 4 && w != 8)) return 0;
    size_t cnt = (n - h) / (size_t)w, out = 0; const uint8_t *a = src + h; const int bits = 8 * w;
    kgb g = {0}; kbuf par = {0};
    if (kgb_new(&g) != 0) goto done;
    kb_byte(&par, (uint8_t)w); kb_uv(&par, h); kb_uv(&par, cnt); if (!par.p) goto done;
    int o = kgb_node(&g, KG_FLOAT, (int[]){0}, 1, w + 1, &par); if (o != 1) goto done;
    for (int b = 0; b < w; b++) if (kb_res(&g.s[1 + b], cnt + 1)) goto done;
    if (kb_res(&g.s[1 + w], n - cnt * (size_t)w + 1)) goto done;
    if (w == 4) {                                             /* float32: the common case */
        uint8_t *p0 = g.s[1].p, *p1 = g.s[2].p, *p2 = g.s[3].p, *p3 = g.s[4].p;
        for (size_t q = 0; q < cnt; q++) {
            uint32_t v = (uint32_t)a[q * 4] | (uint32_t)a[q * 4 + 1] << 8 | (uint32_t)a[q * 4 + 2] << 16 | (uint32_t)a[q * 4 + 3] << 24;
            v = (v << 1) | (v >> 31); p0[q] = (uint8_t)v; p1[q] = (uint8_t)(v >> 8); p2[q] = (uint8_t)(v >> 16); p3[q] = (uint8_t)(v >> 24);
        }
    } else if (w == 2) {                                       /* bf16 / fp16 */
        uint8_t *p0 = g.s[1].p, *p1 = g.s[2].p;
        for (size_t q = 0; q < cnt; q++) { unsigned v = (unsigned)a[q * 2] | (unsigned)a[q * 2 + 1] << 8; v = ((v << 1) | (v >> 15)) & 0xffff; p0[q] = (uint8_t)v; p1[q] = (uint8_t)(v >> 8); }
    } else for (size_t q = 0; q < cnt; q++) {
        uint64_t v = 0; for (int b = 0; b < w; b++) v |= (uint64_t)a[q * w + b] << (8 * b);
        v = (v << 1) | (v >> (bits - 1)); if (bits < 64) v &= ((uint64_t)1 << bits) - 1;
        for (int b = 0; b < w; b++) g.s[1 + b].p[q] = (uint8_t)(v >> (8 * b));
    }
    for (int b = 0; b < w; b++) g.s[1 + b].n = cnt;
    kb_put(&g.s[1 + w], src, h); kb_put(&g.s[1 + w], a + cnt * (size_t)w, n - h - cnt * (size_t)w);
    out = kg_serialize(x, codec, level, &g, n, dst, cap);
done:
    free(par.p); kgb_free(&g); return out;
}

/* Decimal test: the integer m with m / 10^e bit-equal to the value, when there is one below 2^51 in magnitude. */
static inline uint64_t kg_ldv(const uint8_t *p, int f32) { if (f32) { uint32_t v; memcpy(&v, p, 4); return v; } return kg_ld64(p); }
/* the same for a binary32 value: (float)(m / 10^e), the division in binary64 */
static inline int kg_dec_fit32(uint32_t bits, double P, int64_t *m) {
    float fv; memcpy(&fv, &bits, 4);
    double x = (double)fv * P;
    if (!(fabs(x) < 2147483648.0)) return 0;
    double r = (x + 6755399441055744.0) - 6755399441055744.0;
    int64_t k = (int64_t)r; float back = (float)((double)k / P); uint32_t bb; memcpy(&bb, &back, 4);
    if (bb != bits) return 0;
    *m = k; return 1;
}
static inline int kg_dec_fit(uint64_t bits, double P, int64_t *m);
static inline int kg_dec_fitv(uint64_t bits, double P, int64_t *m, int f32) { return f32 ? kg_dec_fit32((uint32_t)bits, P, m) : kg_dec_fit(bits, P, m); }
static inline int kg_dec_fit(uint64_t bits, double P, int64_t *m) {
    double v; memcpy(&v, &bits, 8);
    double x = v * P;
    if (!(fabs(x) < 2251799813685248.0)) return 0;                         /* also rejects NaN and infinities */
    double r = (x + 6755399441055744.0) - 6755399441055744.0;              /* round to nearest, valid below 2^51 */
    int64_t k = (int64_t)r;
    if (kg_dbits((double)k / P) != bits) return 0;                         /* -0.0 fails here: 0 / P is +0.0 */
    *m = k; return 1;
}
#define KG_DEC_SAMPLE 1024       /* values per column that choose its exponent */
#define KG_DEC_ESEARCH 16        /* exponents tried: 0 .. 15 */
#define KG_DEC_RUN 4096          /* layout 1: values per segment */
#define KG_DEC_MINSPAN 64        /* layout 1: shortest array worth a segment */
#define KG_DEC_XFLOAT 1024       /* exceptions from which they are split into byte planes by a FLOAT node */
/* One column or segment (rn values, `step` bytes apart): the exponent with the lowest estimated cost on a sample and
 * the most common pattern that fails. *ok of the *seen sampled values (the missing pattern left out) are decimal. */
static void kg_dec_sample(const uint8_t *base, size_t step, size_t rn, int f32, uint8_t *pe, uint8_t *pfl, uint64_t *pmiss, uint32_t *pok, uint32_t *pseen) {
    const size_t sn = rn < KG_DEC_SAMPLE ? rn : KG_DEC_SAMPLE;
    uint32_t hist[KG_DEC_ESEARCH + 1] = {0}; double mx[KG_DEC_ESEARCH] = {0}; uint64_t cand = 0, vote = 0;
    for (size_t k = 0; k < sn; k++) {
        uint64_t bits = kg_ldv(base + (size_t)((uint64_t)k * rn / sn) * step, f32); int64_t m; int e = 0;
        while (e < (f32 ? 10 : KG_DEC_ESEARCH) && !kg_dec_fitv(bits, KG_P10[e], &m, f32)) e++;
        if (f32 && e == 10) e = KG_DEC_ESEARCH;
        hist[e]++;
        if (e == KG_DEC_ESEARCH) { if (!vote) { cand = bits; vote = 1; } else if (cand == bits) vote++; else vote--; }
        else { double v; if (f32) { float fv; uint32_t u = (uint32_t)bits; memcpy(&fv, &u, 4); v = fv; } else memcpy(&v, &bits, 8); v = fabs(v); if (v > mx[e]) mx[e] = v; }
    }
    uint32_t nm = 0;                                         /* how often the candidate really occurs */
    if (vote) for (size_t k = 0; k < sn; k++) nm += kg_ldv(base + (size_t)((uint64_t)k * rn / sn) * step, f32) == cand;
    if (nm * 16 < hist[KG_DEC_ESEARCH] || nm < 2) nm = 0;
    *pfl = nm ? 2 : 0; *pmiss = nm ? cand : 0;
    double bc = 1e300, top = 0; uint32_t pass = 0; int be = 0;
    for (int e = 0; e < KG_DEC_ESEARCH; e++) {               /* values that fit at e: those whose smallest exponent is <= e */
        pass += hist[e]; if (mx[e] > top) top = mx[e];
        double mag = top * KG_P10[e], by = mag < 1 ? 1 : ceil((log2(mag) + 2) / 8); if (by > 8) by = 8;
        double cost = (double)(sn - pass - nm) * 9 + (double)pass * by + (double)nm * 0.1;
        if (pass && cost < bc) { bc = cost; be = e; }
    }
    *pe = (uint8_t)be;
    uint32_t ok = 0; for (int e = 0; e <= be; e++) ok += hist[e];
    *pok = ok; *pseen = (uint32_t)sn - nm;
}
/* One column or segment into its tags t[0..rn) and plane bytes at q0. col is scratch for 2 * rn values. Returns the
 * flags to add (1 delta, 16 | 1 second difference), or -1 when memory ran out. */
static inline int kg_int_plaus(uint64_t b) { return (b >> 47) == 0 || (b >> 47) == 0x1ffff; }   /* an int64 below 2^47 in magnitude */
static int kg_dec_code(const uint8_t *p, size_t step, size_t rn, int e, int hasm, uint64_t miss, int isint, int f32, uint8_t *t, uint8_t **pln, size_t q0,
                       uint64_t *col, kbuf *exc, uint64_t *all) {
    const double P = KG_P10[e]; size_t k = 0; uint64_t prev = 0, pd = 0, any = 0; double bp = 0, bd = 0, b2 = 0;
    for (size_t r = 0; r < rn; r++, p += step) {
        uint64_t bits = kg_ldv(p, f32); int64_t m;
        if (isint ? (m = f32 ? (int64_t)(int32_t)(uint32_t)bits : (int64_t)bits, f32 || kg_int_plaus(bits)) : kg_dec_fitv(bits, P, &m, f32)) {
            t[r] = 0; col[k++] = (uint64_t)m;
            const uint64_t dl = (uint64_t)m - prev, zp = kg_zz(m), zd = kg_zz((int64_t)dl), z2 = kg_zz((int64_t)(dl - pd)); prev = (uint64_t)m; pd = dl;
            bp += 64 - __builtin_clzll(zp | 1); bd += 64 - __builtin_clzll(zd | 1); b2 += 64 - __builtin_clzll(z2 | 1);
        } else if (hasm && bits == miss) t[r] = 1;
        else { t[r] = 2; if (kb_put(exc, p, f32 ? 4 : 8)) return -1; }
    }
    /* the second difference where it is 5% shorter again (running totals, steady trends) */
    const int ord2 = b2 < 0.95 * (bd < bp ? bd : bp), delta = ord2 || bd < bp * 0.95; size_t q = q0; k = 0; prev = 0; pd = 0;
    uint64_t *zs = col + rn;                                 /* the residuals, one per row, then plane by plane */
    for (size_t r = 0; r < rn; r++) {
        uint64_t z = 0;
        if (!t[r]) { uint64_t m = col[k++]; const uint64_t dl = m - prev; z = kg_zz((int64_t)(ord2 ? dl - pd : delta ? dl : m)); prev = m; pd = dl; }
        any |= z; zs[r] = z;
    }
    for (int b = 0; b < 8; b++) {                            /* only the planes this column reaches are extracted */
        uint8_t *o = pln[b] + q;
        if (!(any >> (8 * b))) { memset(o, 0, rn); continue; }
        for (size_t r = 0; r < rn; r++) o[r] = (uint8_t)(zs[r] >> (8 * b));
    }
    *all |= any; return delta | (ord2 ? 16 : 0);
}
/* A word that can be an ordinary double: zero, NaN or infinity, or a magnitude between 2^-64 and 2^64. Text, small
 * integers and doubles read at the wrong byte alignment mostly fail it, at the cost of one comparison. */
/* the same for a binary32 word: zero, NaN or infinity, or a magnitude between 2^-40 and 2^40 */
static inline int kg_f32_plaus(uint32_t b) { unsigned ex = (b >> 23) & 0xff; return ex == 0xff || b == 0 || ex - 87u < 80u; }
static inline int kg_dec_plaus(uint64_t b) { unsigned ex = (unsigned)(b >> 52) & 0x7ff; return ex == 0x7ff || b == 0 || ex - 959u < 128u; }
/* 16 words at each of the 8 byte alignments from p (136 readable bytes): the alignment + 1 at which they are decimal
 * doubles (at least 12 decimal or NaN, at least 4 of them not zero), 9 + the alignment at which they are small
 * int64 values, 17 + the alignment (0-3) at which 32 four-byte words are decimal binary32 values, or 0. */
static int kg_dec_probe(const uint8_t *p) {
    int best = 0, bk = -1;
    for (int k = 0; k < 8; k++) {
        int pl = 0; for (int i = 0; i < 16; i++) pl += kg_dec_plaus(kg_ld64(p + k + 8 * i));
        if (pl < 14) continue;
        int fit = 0, nz = 0;
        for (int i = 0; i < 16; i++) {
            uint64_t bits = kg_ld64(p + k + 8 * i); int64_t m; int e = 0;
            if (((bits >> 52) & 0x7ff) == 0x7ff) { fit++; nz++; continue; }
            while (e < KG_DEC_ESEARCH && !kg_dec_fit(bits, KG_P10[e], &m)) e++;
            if (e < KG_DEC_ESEARCH) { fit++; nz += bits != 0; }
        }
        if (fit >= 12 && nz >= 4 && fit > best) { best = fit; bk = k; }
    }
    if (bk >= 0) return bk + 1;
    best = 0;                                                /* not doubles (tried first: 1.0 read two bytes off looks like an integer) */
    for (int k = 0; k < 8; k++) {                            /* small int64 values: 14 of 16, at least 6 not zero and 4 different */
        /* below 2^32 in magnitude at this alignment and not one byte later. Doubles with short mantissas read a
         * few bytes off also have zero high bytes, but they keep them one byte later; an integer's low byte lands
         * in the high byte of the word before it. */
        int pl = 0, nz = 0, p1 = 0;
        for (int i = 0; i < 16; i++) { uint64_t b = kg_ld64(p + k + 8 * i); pl += (b >> 32) == 0 || (b >> 32) == 0xffffffffu; nz += b != 0; }
        if (pl < 14 || nz < 6 || pl <= best) continue;
        for (int i = 0; i < 15; i++) { uint64_t b = kg_ld64(p + k + 1 + 8 * i); p1 += (b >> 32) == 0 || (b >> 32) == 0xffffffffu; }
        if (p1 < 13) { best = pl; bk = k + 8; }
    }
    if (bk >= 0) return bk + 1;                              /* 9 .. 16: integers at alignment bk - 8 */
    best = 0;
    for (int k = 0; k < 4; k++) {                            /* binary32 decimals: 32 words, 28 plausible, 24 decimal or NaN, 8 not zero */
        int pl = 0, fit = 0, nz = 0;
        for (int i = 0; i < 32; i++) pl += kg_f32_plaus((uint32_t)kg_ldv(p + k + 4 * i, 1));
        if (pl < 28) continue;
        for (int i = 0; i < 32; i++) {
            uint32_t bits = (uint32_t)kg_ldv(p + k + 4 * i, 1); int64_t m; int e = 0;
            if (((bits >> 23) & 0xff) == 0xff) { fit++; nz++; continue; }
            while (e < 8 && !kg_dec_fit32(bits, KG_P10[e], &m)) e++;
            if (e < 8) { fit++; nz += bits != 0; }
        }
        if (fit >= 24 && nz >= 8 && fit > best) { best = fit; bk = k + 16; }
    }
    return bk + 1;                                           /* 17 .. 20: binary32 at alignment bk - 16 */
}
typedef struct { size_t off; uint32_t n; uint8_t e, fl; uint64_t miss; } kgseg;   /* fl & 8: 4-byte words */
#define KG_DEC_F32 ((size_t)1 << 30)   /* added to cols: a table of binary32 values */
/* Binary64 values that were written in decimal: one DEC64 node. cols > 0: a table of rows of cols values after h
 * bytes (layout 0). cols == 0: the block is searched for arrays of doubles at any byte alignment (layout 1); each
 * is cut into segments that choose their own exponent, so columns stored one after the other keep their own scale,
 * and everything else stays as plain bytes. Returns 0 when the values are not decimal, or the payload is not smaller. */
static size_t kg_encode_dec64(codec_ctx *x, int codec, int level, const uint8_t *src, size_t n, size_t cols, size_t h,
                              uint8_t *dst, size_t cap) {
    const int tf32 = (cols & KG_DEC_F32) != 0; cols &= ~KG_DEC_F32; const size_t tw = tf32 ? 4 : 8;
    if (h > n || cols > KG_DEC_MAXCOLS || n < 512) return 0;
    const int segs = !cols; size_t out = 0, cnt = 0, rows = 0, nseg = 0, segcap = 0, maxrn = 0, vbytes = 0; int any32 = tf32;
    kgb g = {0}; kbuf par = {0}, exc = {0}; kgseg *S = NULL;
    uint8_t *tags = NULL, *pln[8] = {0}; uint64_t *col = NULL, all = 0;
    if (segs) {
        /* find the arrays: probe for an alignment, walk back to the array's first word, then forward while the words
         * stay plausible doubles. A failed probe doubles the step (up to 4 KiB): the walk back recovers the start. */
        size_t pos = 0, stepb = 64, prevend = 0;
        while (pos + 136 <= n) {
            int k1 = kg_dec_probe(src + pos);
            if (!k1) { pos += stepb; if (stepb < 4096) stepb *= 2; continue; }
            stepb = 64;
            const int f32 = k1 > 16, isint = !f32 && k1 > 8; k1 -= f32 ? 16 : isint ? 8 : 0;
            const size_t W = f32 ? 4 : 8;
#define KG_PL(q_) (f32 ? kg_f32_plaus((uint32_t)kg_ldv(q_, 1)) : isint ? kg_int_plaus(kg_ld64(q_)) : kg_dec_plaus(kg_ld64(q_)))
            size_t s0 = pos + (size_t)k1 - 1, e0;
            while (s0 >= prevend + W && KG_PL(src + s0 - W)) s0 -= W;
            for (e0 = s0;;) {
                size_t left = (n - e0) / W, m = left < 64 ? left : 64, pl = 0; if (!m) break;
                for (size_t i = 0; i < m; i++) pl += (size_t)KG_PL(src + e0 + W * i);
                if (pl * 16 >= m * 15) { e0 += m * W; continue; }
                size_t i = 0; while (i < m && KG_PL(src + e0 + W * i)) i++;
                e0 += i * W; break;
            }
#undef KG_PL
            size_t nv = (e0 - s0) / W;
            if (nv < KG_DEC_MINSPAN || e0 <= pos) { pos += 64; continue; }
            for (size_t v = 0; v < nv;) {                    /* segments of KG_DEC_RUN values; a short remainder joins the last */
                size_t rn = nv - v < KG_DEC_RUN + KG_DEC_MINSPAN ? nv - v : KG_DEC_RUN;
                uint8_t e = 0, fl = 4; uint64_t miss = 0; uint32_t ok = 1, seen = 1;
                if (!isint) kg_dec_sample(src + s0 + v * W, W, rn, f32, &e, &fl, &miss, &ok, &seen);
                if (ok && ok * 2 >= seen && nseg < KG_DEC_MAXCOLS) {   /* mostly decimal; other runs stay plain bytes */
                    if (nseg == segcap) { segcap = segcap ? segcap * 2 : 64; kgseg *q = realloc(S, segcap * sizeof(kgseg)); if (!q) goto done; S = q; }
                    S[nseg++] = (kgseg){s0 + v * W, (uint32_t)rn, e, (uint8_t)(fl | (f32 ? 8 : 0)), miss}; cnt += rn; vbytes += rn * W; any32 |= f32; if (rn > maxrn) maxrn = rn;
                }
                v += rn;
            }
            pos = prevend = e0;
        }
        if (!nseg || vbytes * 8 < n) goto done;               /* under an eighth of the block is decimal values */
    } else {
        rows = (n - h) / tw / cols; cnt = rows * cols; maxrn = rows; nseg = cols; vbytes = cnt * tw;
        if (cnt < 64) goto done;
        S = malloc(cols * sizeof(kgseg)); if (!S) goto done;
        uint64_t good = 0, seen = 0;
        for (size_t c = 0; c < cols; c++) {
            uint32_t ok, sn; S[c].off = h + c * tw; S[c].n = (uint32_t)rows;
            kg_dec_sample(src + S[c].off, cols * tw, rows, tf32, &S[c].e, &S[c].fl, &S[c].miss, &ok, &sn);
            if (tf32) S[c].fl |= 8;
            good += ok; seen += sn;
        }
        if (good * 4 < seen || !good) goto done;             /* under a quarter of the present values are decimal */
    }
    tags = malloc(cnt + 1); col = malloc((maxrn + 1) * 16);
    for (int b = 0; b < 8; b++) pln[b] = malloc(cnt + 1);
    if (!tags || !col || !pln[0] || !pln[1] || !pln[2] || !pln[3] || !pln[4] || !pln[5] || !pln[6] || !pln[7]) goto done;
    for (size_t c = 0, q0 = 0; c < nseg; c++) {
        const size_t W = S[c].fl & 8 ? 4 : 8;
        int d = kg_dec_code(src + S[c].off, segs ? W : cols * W, S[c].n, S[c].e, S[c].fl & 2, S[c].miss, S[c].fl & 4, S[c].fl & 8, tags + q0, pln, q0, col, &exc, &all);
        if (d < 0) goto done;
        S[c].fl |= (uint8_t)d; q0 += S[c].n;
    }
    {   const int np = (71 - __builtin_clzll(all | 1)) / 8;        /* bytes the largest residual needs */
        if (kgb_new(&g) != 0) goto done;
        kb_byte(&par, (uint8_t)segs); if (!segs) { kb_uv(&par, h); kb_uv(&par, rows); }
        kb_uv(&par, nseg); kb_byte(&par, (uint8_t)np);
        for (size_t c = 0; c < nseg; c++) {
            if (segs) { kb_uv(&par, S[c].off - (c ? S[c - 1].off + (size_t)S[c - 1].n * (S[c - 1].fl & 8 ? 4 : 8) : 0)); kb_uv(&par, S[c].n); }
            kb_byte(&par, S[c].e); kb_byte(&par, S[c].fl); if (S[c].fl & 2) kb_put(&par, &S[c].miss, 8);
        }
        if (!par.p || kb_res(&par, 1)) goto done;
        if (kgb_node(&g, KG_DEC64, (int[]){0}, 1, np + 3, &par) != 1) goto done;
        g.s[1].p = tags; g.s[1].n = cnt; g.s[1].cap = cnt + 1; tags = NULL;
        for (int b = 0; b < np; b++) { g.s[2 + b].p = pln[b]; g.s[2 + b].n = cnt; g.s[2 + b].cap = cnt + 1; pln[b] = NULL; }
        g.s[2 + np] = exc; memset(&exc, 0, sizeof exc);
        kbuf *rest = &g.s[3 + np];
        if (kb_res(rest, n - vbytes + 1)) goto done;
        if (segs) {
            size_t at = 0;
            for (size_t c = 0; c < nseg; c++) { kb_put(rest, src + at, S[c].off - at); at = S[c].off + (size_t)S[c].n * (S[c].fl & 8 ? 4 : 8); }
            kb_put(rest, src + at, n - at);
        } else { kb_put(rest, src, h); kb_put(rest, src + h + vbytes, n - h - vbytes); }
        const size_t xn = any32 ? 0 : g.s[2 + np].n / 8;       /* (exceptions of mixed or 4-byte width stay one stream) */
        if (xn >= KG_DEC_XFLOAT) {                                /* many exceptions: a FLOAT node splits them into byte planes */
            kbuf fp = {0}; kb_byte(&fp, 8); kb_uv(&fp, 0); kb_uv(&fp, xn);
            int f = fp.p ? kgb_node(&g, KG_FLOAT, (int[]){2 + np}, 1, 9, &fp) : -1; free(fp.p); if (f < 0) goto done;
            for (int b = 0; b < 8; b++) if (kb_res(&g.s[f + b], xn + 1)) goto done;
            const uint8_t *xs = g.s[2 + np].p;
            for (size_t q = 0; q < xn; q++) {
                uint64_t v = kg_ld64(xs + q * 8); v = (v << 1) | (v >> 63);
                for (int b = 0; b < 8; b++) g.s[f + b].p[q] = (uint8_t)(v >> (8 * b));
            }
            for (int b = 0; b < 8; b++) g.s[f + b].n = xn;
            free(g.s[2 + np].p); memset(&g.s[2 + np], 0, sizeof(kbuf));
        }
        out = kg_serialize(x, codec, level, &g, n, dst, cap);
    }
done:
    free(S); free(tags); free(col); for (int b = 0; b < 8; b++) free(pln[b]);
    free(par.p); free(exc.p); kgb_free(&g); return out;
}

/* ---- IMG8 ---- */
static inline uint8_t kg_med(int a, int b, int c) {       /* median of a, b, a + b - c, branch-free */
    int mx = a > b ? a : b, mn = a < b ? a : b, g = a + b - c; g = g < mn ? mn : g; return (uint8_t)(g > mx ? mx : g);
}
static inline uint8_t kg_zz8(uint8_t d) { int8_t v = (int8_t)d; return (uint8_t)(((unsigned)v << 1) ^ (unsigned)(v >> 7)); }
static inline uint8_t kg_unzz8(uint8_t z) { return (uint8_t)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }
/* rows of W bytes, channel step C: row 0 predicts from the left, later rows from above for the first C bytes and from
 * the median of left, above and left + above - above-left after that */
static void kg_img8_fwd(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r) {
    size_t x = 0;
    for (; x < C && x < m; x++) r[x] = kg_zz8(p[x]);
    for (; x < W && x < m; x++) r[x] = kg_zz8((uint8_t)(p[x] - p[x - C]));
    for (size_t y = W; y + W <= m; y += W) {
        const uint8_t *q = p + y, *u = q - W; uint8_t *o = r + y;
        for (x = 0; x < C; x++) o[x] = kg_zz8((uint8_t)(q[x] - u[x]));
        for (; x < W; x++) o[x] = kg_zz8((uint8_t)(q[x] - kg_med(q[x - C], u[x], u[x - C])));
    }
}
static void kg_img8_back(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p) {
    size_t x = 0;
    for (; x < C && x < m; x++) p[x] = kg_unzz8(r[x]);
    for (; x < W && x < m; x++) p[x] = (uint8_t)(p[x - C] + kg_unzz8(r[x]));
    for (size_t y = W; y + W <= m; y += W) {
        uint8_t *q = p + y; const uint8_t *u = q - W, *o = r + y;
        for (x = 0; x < C; x++) q[x] = (uint8_t)(u[x] + kg_unzz8(o[x]));
        for (; x < W; x++) q[x] = (uint8_t)(kg_med(q[x - C], u[x], u[x - C]) + kg_unzz8(o[x]));
    }
}
static int kg_img8_inv(const uint8_t *par, size_t plen, const kbuf *res, const kbuf *rest, size_t maxs, kbuf *out) {
    size_t pi = 0; uint64_t W, h; if (kg_getuv(par, plen, &pi, &W) || pi >= plen) return -1;
    unsigned C = par[pi++]; if (kg_getuv(par, plen, &pi, &h) || pi != plen || C < 1 || C > 4 || W < C || W > (1u << 26) || h > rest->n) return -1;
    size_t m = res->n; if (m % W || m + rest->n > maxs) return -1;
    if (kb_res(out, m + rest->n + 16)) return -1;
    if (h) memcpy(out->p, rest->p, (size_t)h);
    kg_img8_back(res->p, m, (size_t)W, C, out->p + h);
    memcpy(out->p + h + m, rest->p + h, rest->n - (size_t)h); out->n = m + rest->n;
    return 0;
}
static size_t kg_encode_img8(codec_ctx *x, int codec, int level, const uint8_t *src, size_t n, size_t W, unsigned C, size_t h,
                             uint8_t *dst, size_t cap) {
    if (h > n || W < C || C < 1 || C > 4 || (n - h) / W < 2) return 0;
    size_t m = (n - h) / W * W, out = 0; const uint8_t *a = src + h;
    kgb g = {0}; kbuf par = {0};
    if (kgb_new(&g) != 0) goto done;
    kb_uv(&par, W); kb_byte(&par, (uint8_t)C); kb_uv(&par, h); if (!par.p) goto done;
    if (kgb_node(&g, KG_IMG8, (int[]){0}, 1, 2, &par) != 1) goto done;
    if (kb_res(&g.s[1], m + 1) || kb_res(&g.s[2], n - m + 1)) goto done;
    kg_img8_fwd(a, m, W, C, g.s[1].p);
    g.s[1].n = m;
    kb_put(&g.s[2], src, h); kb_put(&g.s[2], a + m, n - h - m);
    out = kg_serialize(x, codec, level, &g, n, dst, cap);
done:
    free(par.p); kgb_free(&g); return out;
}

/* ---- decoder ---- */
typedef struct { uint8_t op; uint32_t nin, nout, firstout; const uint32_t *in; const uint8_t *par; size_t plen; } kgnode;
static int kg_decode(codec_ctx *x, const uint8_t *p, size_t n, uint8_t *out, size_t len) {
    const uint64_t maxs = (uint64_t)len * 4 + (1u << 20);    /* no stream of a valid program is larger */
    size_t i = 0; uint64_t v, nn;
    if (n < 3 || p[0] != 'G' || p[1] != 1) return -1;
    i = 2; if (kg_getuv(p, n, &i, &nn) || nn > KG_MAXSTREAMS) return -1;
    kgnode *nd = calloc(nn ? nn : 1, sizeof(kgnode)); uint32_t *ins = NULL; size_t nins = 0, insc = 0;
    kbuf *s = NULL; uint8_t *have = NULL, *cons = NULL; int rc = -1; uint64_t ns = 1;
    uint32_t *prod = NULL; kgdec **dec = NULL, *decs = NULL;
    if (!nd) return -1;
    /* pass 1: parse nodes (input indices stored in one array) */
    size_t save = i;
    for (int pass = 0; pass < 2; pass++) {
        i = save; ns = 1; nins = 0;
        for (uint64_t q = 0; q < nn; q++) {
            if (i >= n) goto out;
            uint8_t op = p[i++]; uint64_t nin, nout, pl;
            if (op < 1 || op > KG_MAXOP || kg_getuv(p, n, &i, &nin) || nin == 0 || nin > KG_MAXSTREAMS) goto out;
            if (pass) nd[q].in = ins + nins;
            for (uint64_t r = 0; r < nin; r++) { if (kg_getuv(p, n, &i, &v) || v >= ns) goto out; if (pass) ins[nins] = (uint32_t)v; nins++; }
            if (kg_getuv(p, n, &i, &nout) || nout == 0 || nout > KG_MAXSTREAMS || ns + nout > KG_MAXSTREAMS) goto out;
            if (kg_getuv(p, n, &i, &pl) || pl > n - i) goto out;
            if (pass) { nd[q].op = op; nd[q].nin = (uint32_t)nin; nd[q].nout = (uint32_t)nout; nd[q].firstout = (uint32_t)ns; nd[q].par = p + i; nd[q].plen = (size_t)pl; }
            i += pl; ns += nout;
        }
        if (!pass) { insc = nins; ins = malloc((insc ? insc : 1) * sizeof(uint32_t)); if (!ins) goto out; }
    }
    s = calloc(ns, sizeof(kbuf)); have = calloc(ns, 1); cons = calloc(ns, 1);
    if (!s || !have || !cons) goto out;
    for (size_t q = 0; q < nins; q++) { if (cons[ins[q]]) goto out; cons[ins[q]] = 1; }
    if (nn && !cons[0]) goto out;
    /* leaves */
    uint64_t nl; if (kg_getuv(p, n, &i, &nl) || nl > ns) goto out;
    for (uint64_t q = 0; q < nl; q++) {
        uint64_t id, rl, zl; if (kg_getuv(p, n, &i, &id) || id >= ns || cons[id] || have[id] || i >= n) goto out;
        int cc = p[i++]; if (kg_getuv(p, n, &i, &rl) || kg_getuv(p, n, &i, &zl) || zl > n - i || rl > maxs) goto out;
        if (kb_res(&s[id], (size_t)rl + 1)) goto out;
        if (codec_decompress(x, cc, p + i, (size_t)zl, s[id].p, (size_t)rl)) goto out;
        s[id].n = (size_t)rl; have[id] = 1; i += zl;
    }
    if (i != n) goto out;
    for (uint64_t q = 0; q < ns; q++) if (!cons[q] && !have[q] && (q || nn)) goto out;   /* every leaf present */
    /* inverses, last node first */
    prod = malloc(ns * sizeof(uint32_t)); dec = calloc(ns, sizeof(kgdec *)); decs = calloc(nn ? nn : 1, sizeof(kgdec));
    if (!prod || !dec || !decs) goto out;
    prod[0] = UINT32_MAX;
    for (uint64_t q = 0; q < nn; q++) for (uint32_t r = 0; r < nd[q].nout; r++) prod[nd[q].firstout + r] = (uint32_t)q;
    for (uint64_t qq = nn; qq-- > 0;) {
        kgnode *t = &nd[qq]; kbuf *o = &s[t->firstout];
        for (uint32_t r = 0; r < t->nout; r++) if (!have[t->firstout + r]) goto out;
        size_t pi = 0;
        if (t->op == KG_FIELDS) {
            uint64_t k; pi = 1;
            if (t->nin != 1 || t->plen < 2 || kg_getuv(t->par, t->plen, &pi, &k) || pi != t->plen || k < 1 || t->nout != k + 1) goto out;
            kbuf r = {0}; if (kg_fields_inv(t->par[0], (uint32_t)k, &o[0], &o[1], dec + t->firstout + 1, &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_DECIMAL) {
            if (t->nin != 1 || t->nout != 4 || t->plen != 2 || t->par[1] > 1) goto out;
            uint32_t pq = prod[t->in[0]];
            if (pq != UINT32_MAX && nd[pq].op == KG_FIELDS && t->in[0] != nd[pq].firstout) {   /* a FIELDS column: fuse */
                kgdec *D = &decs[qq]; D->sep = t->par[0]; D->pred = t->par[1];
                D->tg = o[0].p; D->tge = o[0].p + o[0].n; D->sc = o[1].p; D->sce = o[1].p + o[1].n;
                D->mt = o[2].p; D->mte = o[2].p + o[2].n; D->tx = o[3].p; D->txe = o[3].p + o[3].n;
                dec[t->in[0]] = D; have[t->in[0]] = 1; continue;      /* outputs stay alive until the FIELDS inverse */
            }
            kbuf r = {0}; if (kg_decimal_inv(t->par[0], t->par[1], &o[0], &o[1], &o[2], &o[3], &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_CONCAT) {
            uint64_t cnt; if (t->nout != 1 || kg_getuv(t->par, t->plen, &pi, &cnt) || cnt != t->nin) goto out;
            size_t off = 0;
            for (uint32_t r = 0; r < t->nin; r++) {              /* the parts are read-only views into the output (cap 0) */
                uint64_t L; if (kg_getuv(t->par, t->plen, &pi, &L) || L > o->n - off) goto out;
                s[t->in[r]] = (kbuf){o->p + off, (size_t)L, 0}; off += (size_t)L;
            }
            if (off != o->n) goto out;
            for (uint32_t r = 0; r < t->nin; r++) have[t->in[r]] = 1;
            continue;                                               /* the concatenation stays alive for its views */
        } else if (t->op == KG_IMG8) {
            if (t->nin != 1 || t->nout != 2) goto out;
            kbuf r = {0}; if (kg_img8_inv(t->par, t->plen, &o[0], &o[1], maxs, &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_FLOAT) {
            if (t->nin != 1 || t->plen < 1 || (t->par[0] != 2 && t->par[0] != 4 && t->par[0] != 8) || t->nout != (uint32_t)t->par[0] + 1) goto out;
            kbuf r = {0}; if (kg_float_inv(t->par, t->plen, &o[0], &o[t->par[0]], maxs, &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_DEC64) {
            if (t->nin != 1 || t->nout < 4 || t->nout > 11) goto out;
            kbuf r = {0}; if (kg_dec64_inv(t->par, t->plen, &o[0], t->nout, maxs, &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_INT) {
            if (t->nin != 1 || t->nout != 2) goto out;
            kbuf r = {0}; if (kg_int_inv(t->par, t->plen, &o[0], &o[1], maxs, &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else goto out;
        for (uint32_t r = 0; r < t->nin; r++) have[t->in[r]] = 1;
        for (uint32_t r = 0; r < t->nout; r++) { if (s[t->firstout + r].cap) free(s[t->firstout + r].p); memset(&s[t->firstout + r], 0, sizeof(kbuf)); }
    }
    if (!have[0] || s[0].n != len) goto out;
    if (len) memcpy(out, s[0].p, len);
    rc = 0;
out:
    if (s) for (uint64_t q = 0; q < ns; q++) if (s[q].cap) free(s[q].p);   /* cap 0: a view, or empty */
    free(s); free(have); free(cons); free(nd); free(ins); free(prod); free(dec); free(decs);
    return rc;
}
