/* kolmo graph (KG): a block stores a small program instead of one fixed transform.
 *
 * Stream 0 is the block. Each node consumes one or more streams and appends its outputs to the stream table; the
 * streams no node consumes are the leaves, each coded by one of the ordinary codecs. A decoder decodes the leaves,
 * then runs the nodes' inverses in reverse order. New transforms add node ids, never container versions, and a decoder
 * that meets an unknown node id fails cleanly instead of misreading the block.
 *
 * Payload: 'G' | version u8 (1) | nodes uv | node* | leaves uv | leaf*
 *   node: op u8 | nin uv | in-index uv * nin | nout uv | plen uv | params[plen]   (outputs get the next stream indices)
 *   leaf: stream uv | codec u8 | raw length uv | coded length uv | coded bytes
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
 */
enum { KG_FIELDS = 1, KG_DECIMAL = 2, KG_CONCAT = 3, KG_MAXOP = 3 };
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

/* ---- FIELDS ---- */
static int kg_fields_inv(uint8_t d, uint32_t k, const kbuf *shape, const kbuf *cols, kbuf *out) {
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
            for (uint32_t j = 0; j < k; j++) {
                const kbuf *c = &cols[j]; const uint8_t *e = cp[j] < c->n ? memchr(c->p + cp[j], '\n', c->n - cp[j]) : NULL;
                if (!e) goto bad;
                size_t fl = (size_t)(e - (c->p + cp[j]));
                if (kb_put(out, c->p + cp[j], fl)) goto bad;
                cp[j] += fl + 1;
                if (j + 1 < k && kb_byte(out, d)) goto bad;
            }
        }
        if ((l + 1 < lines || fin) && kb_byte(out, '\n')) goto bad;
    }
    for (uint32_t j = 0; j < k; j++) if (cp[j] != cols[j].n) goto bad;
    if (have || si != shape->n) goto bad;
    free(cp); return 0;
bad: free(cp); return -1;
}

/* ---- DECIMAL ---- */
static int kg_num(const uint8_t *v, size_t l, uint8_t sep, int64_t *m, uint8_t *sc) {
    size_t i = 0; int neg = 0;
    if (i < l && v[i] == '-') { neg = 1; i++; }
    size_t a = i; while (i < l && (uint8_t)(v[i] - '0') < 10) i++;
    size_t id = i - a; if (!id || (id > 1 && v[a] == '0')) return 0;
    size_t fd = 0, b = 0; int hassep = 0;
    if (i < l) { if (v[i] != sep) return 0; hassep = 1; b = ++i; while (i < l && (uint8_t)(v[i] - '0') < 10) i++; fd = i - b; if (!fd || i != l) return 0; }
    if (id + fd > 18) return 0;
    int64_t x = 0; for (size_t q = a; q < a + id; q++) x = x * 10 + (v[q] - '0');
    for (size_t q = b; q < b + fd; q++) x = x * 10 + (v[q] - '0');
    if (neg && x == 0) return 0;
    *m = neg ? -x : x; *sc = hassep ? (uint8_t)fd : 255; return 1;
}
static uint64_t kg_zz(int64_t x) { return ((uint64_t)x << 1) ^ (uint64_t)(x >> 63); }
static int64_t kg_unzz(uint64_t u) { return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }
static int kg_decimal_inv(uint8_t sep, uint8_t pred, const kbuf *tags, const kbuf *sc, const kbuf *mant, const kbuf *txt, kbuf *out) {
    size_t si = 0, mi = 0, ti = 0; int64_t prev = 0;
    if (kb_res(out, tags->n * 23 + txt->n + 16)) return -1;    /* a number prints in at most 21 bytes plus '\n' */
    uint8_t *w = out->p + out->n;
    for (size_t q = 0; q < tags->n; q++) {
        uint8_t t = tags->p[q];
        if (t == 0) *w++ = '\n';
        else if (t == 1) {
            uint64_t z = 0; int sh = 0;
            for (;;) { if (mi >= mant->n || sh > 63) return -1; uint8_t c = mant->p[mi++]; z |= (uint64_t)(c & 127) << sh; if (!(c & 128)) break; sh += 7; }
            int64_t m = pred ? (int64_t)((uint64_t)prev + (uint64_t)kg_unzz(z)) : kg_unzz(z); prev = m;
            if (si >= sc->n) return -1;
            uint8_t s = sc->p[si++]; int f = s == 255 ? 0 : s; if (s != 255 && s > 18) return -1;
            char dg[24]; int nd = 0; uint64_t u = m < 0 ? (uint64_t)0 - (uint64_t)m : (uint64_t)m;
            do { dg[nd++] = (char)('0' + u % 10); u /= 10; } while (u);
            while (nd < f + 1) dg[nd++] = '0';
            if (m < 0) *w++ = '-';
            for (int r = nd - 1; r >= f; r--) *w++ = (uint8_t)dg[r];
            if (s != 255) { *w++ = sep; for (int r = f - 1; r >= 0; r--) *w++ = (uint8_t)dg[r]; }
            *w++ = '\n';
        } else if (t == 2) {
            const uint8_t *e = ti < txt->n ? memchr(txt->p + ti, '\n', txt->n - ti) : NULL; if (!e) return -1;
            size_t l = (size_t)(e - (txt->p + ti)) + 1; memcpy(w, txt->p + ti, l); w += l; ti += l;
        } else return -1;
    }
    out->n = (size_t)(w - out->p);
    return si == sc->n && mi == mant->n && ti == txt->n ? 0 : -1;
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
      while (j0 < n && seen < 4096) {
          const uint8_t *e = memchr(src + j0, '\n', n - j0); size_t end = e ? (size_t)(e - src) : n, a = j0; uint32_t j = 0;
          uint32_t f = 1; for (size_t q = j0; q < end; q++) f += src[q] == d;
          if (f == k) for (;; j++) {
              size_t bq = a; while (bq < end && src[bq] != d) bq++;
              int64_t m; uint8_t sc; vals[j]++;
              if (bq == a) nums[j]++;
              else if (kg_num(src + a, bq - a, sep, &m, &sc)) {
                  nums[j]++; uint64_t z0 = kg_zz(m), z1 = kg_zz((int64_t)((uint64_t)m - (uint64_t)pv[j])); pv[j] = m;
                  c0[j] += 1 + (z0 >= 128) + (z0 >= 16384) + (z0 >= 2097152); c1[j] += 1 + (z1 >= 128) + (z1 >= 16384) + (z1 >= 2097152);
              }
              if (bq >= end) break; a = bq + 1;
          }
          seen++; j0 = end + 1;
      }
      for (uint32_t j = 0; j < k; j++) { C[j].num = vals[j] && nums[j] * 100 >= vals[j] * 60; C[j].pred = c1[j] <= c0[j]; }
      free(vals); free(nums); free(c0); free(c1); free(pv); }
    /* one pass: fields straight to their streams */
    while (i < n) {
        const uint8_t *e = memchr(src + i, '\n', n - i); size_t end = e ? (size_t)(e - src) : n;
        uint32_t f = 0; fs[f++] = i;
        for (const uint8_t *q = src + i, *qe = src + end; q < qe; q++) if (*q == d) { if (f > k) break; fs[f++] = (size_t)(q - src) + 1; }
        if (f == k) {
            fs[k] = end + 1;
            for (uint32_t j = 0; j < k; j++) {
                kgcol *c = &C[j]; const uint8_t *v = src + fs[j]; size_t l = fs[j + 1] - 1 - fs[j];
                if (!c->num) { if (kb_put(&c->raw, v, l) || kb_byte(&c->raw, '\n')) goto done; continue; }
                int64_t m; uint8_t sc;
                if (kb_res(&c->tag, 1) || kb_res(&c->sc, 1) || kb_res(&c->mt, 10)) goto done;
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
    /* serialize: header, program, leaves (every unconsumed stream, in index order) */
    kbuf o = {0}; int nleaf = 0;
    for (int q = 0; q < g.ns; q++) nleaf += !g.used[q];
    if (kb_byte(&o, 'G') || kb_byte(&o, 1) || kb_uv(&o, (uint64_t)g.nodes) || kb_put(&o, g.prog.p, g.prog.n) || kb_uv(&o, (uint64_t)nleaf)) { free(o.p); goto done; }
    for (int q = 0; q < g.ns; q++) {
        if (g.used[q]) continue;
        kbuf *s = &g.s[q]; size_t zc = s->n + s->n / 8 + 1024; int cc = codec == C_CM || codec == C_STORE ? C_LZMA : codec;
        if (kb_uv(&o, (uint64_t)q) || kb_res(&o, 1 + 20 + zc)) { free(o.p); goto done; }
        size_t hp = o.n; o.n += 1;
        size_t z = s->n >= 64 ? codec_compress(x, cc, level, s->p, s->n, o.p + o.n + 20, zc) : 0;
        if (!z || z >= s->n) { cc = C_STORE; z = s->n; if (z) memcpy(o.p + o.n + 20, s->p, z); }
        o.p[hp] = (uint8_t)cc;
        uint8_t hdr[20]; size_t hn = 0;
        for (uint64_t v = s->n;; v >>= 7) { hdr[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (uint64_t v = z;; v >>= 7) { hdr[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        memmove(o.p + o.n + hn, o.p + o.n + 20, z); memcpy(o.p + o.n, hdr, hn); o.n += hn + z;
    }
    if (o.n < cap && o.n < n) { memcpy(dst, o.p, o.n); out = o.n; }
    free(o.p);
done:
    if (C) for (uint32_t j = 0; j < k; j++) { free(C[j].tag.p); free(C[j].sc.p); free(C[j].mt.p); free(C[j].tx.p); free(C[j].raw.p); }
    free(C); free(fs); free(irr.p); free(shape.p);
    kgb_free(&g);
    return out;
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
    for (uint64_t qq = nn; qq-- > 0;) {
        kgnode *t = &nd[qq]; kbuf *o = &s[t->firstout];
        for (uint32_t r = 0; r < t->nout; r++) if (!have[t->firstout + r]) goto out;
        size_t pi = 0;
        if (t->op == KG_FIELDS) {
            uint64_t k; pi = 1;
            if (t->nin != 1 || t->plen < 2 || kg_getuv(t->par, t->plen, &pi, &k) || pi != t->plen || k < 1 || t->nout != k + 1) goto out;
            kbuf r = {0}; if (kg_fields_inv(t->par[0], (uint32_t)k, &o[0], &o[1], &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_DECIMAL) {
            if (t->nin != 1 || t->nout != 4 || t->plen != 2 || t->par[1] > 1) goto out;
            kbuf r = {0}; if (kg_decimal_inv(t->par[0], t->par[1], &o[0], &o[1], &o[2], &o[3], &r)) { free(r.p); goto out; }
            s[t->in[0]] = r;
        } else if (t->op == KG_CONCAT) {
            uint64_t cnt; if (t->nout != 1 || kg_getuv(t->par, t->plen, &pi, &cnt) || cnt != t->nin) goto out;
            size_t off = 0;
            for (uint32_t r = 0; r < t->nin; r++) {
                uint64_t L; if (kg_getuv(t->par, t->plen, &pi, &L) || L > o->n - off) goto out;
                kbuf b = {0}; if (kb_put(&b, o->p + off, (size_t)L)) { free(b.p); goto out; }
                s[t->in[r]] = b; off += (size_t)L;
            }
            if (off != o->n) goto out;
        } else goto out;
        for (uint32_t r = 0; r < t->nin; r++) have[t->in[r]] = 1;
        for (uint32_t r = 0; r < t->nout; r++) { free(s[t->firstout + r].p); memset(&s[t->firstout + r], 0, sizeof(kbuf)); }
    }
    if (!have[0] || s[0].n != len) goto out;
    if (len) memcpy(out, s[0].p, len);
    rc = 0;
out:
    if (s) for (uint64_t q = 0; q < ns; q++) free(s[q].p);
    free(s); free(have); free(cons); free(nd); free(ins);
    return rc;
}
