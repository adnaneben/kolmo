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
static int kb_put(kbuf *b, const void *s, size_t n) { if (kb_res(b, n)) return -1; memcpy(b->p + b->n, s, n); b->n += n; return 0; }
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
static int kg_fields_fwd(const uint8_t *s, size_t n, uint8_t d, uint32_t k, kbuf *shape, kbuf *cols) {
    size_t lines = 0, last = 0, i = 0; int fin = n && s[n - 1] == '\n';
    kbuf irr = {0};
    while (i < n) {
        const uint8_t *e = memchr(s + i, '\n', n - i); size_t end = e ? (size_t)(e - s) : n;
        uint32_t f = 1; for (size_t q = i; q < end; q++) f += s[q] == d;
        if (f == k) {
            size_t a = i;
            for (uint32_t j = 0; j < k; j++) {
                size_t b = a; while (b < end && s[b] != d) b++;
                if (kb_put(&cols[j], s + a, b - a) || kb_byte(&cols[j], '\n')) goto fail;
                a = b + 1;
            }
        } else if (kb_uv(&irr, lines - last) || kb_uv(&irr, end - i) || kb_put(&irr, s + i, end - i)) goto fail;
        else last = lines;
        lines++; i = end + 1;
    }
    if (kb_uv(shape, lines) || kb_byte(shape, (uint8_t)fin) || kb_put(shape, irr.p, irr.n)) goto fail;
    free(irr.p); return 0;
fail: free(irr.p); return -1;
}
static int kg_fields_inv(uint8_t d, uint32_t k, const kbuf *shape, const kbuf *cols, kbuf *out) {
    size_t si = 0; uint64_t lines, gap = 0, len; int have = 0;
    if (kg_getuv(shape->p, shape->n, &si, &lines) || si >= shape->n) return -1;
    int fin = shape->p[si++]; if (fin > 1) return -1;
    size_t *cp = calloc(k, sizeof(size_t)); if (!cp) return -1;
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
static int kg_numfmt(int64_t m, uint8_t sc, uint8_t sep, kbuf *o) {
    char dg[24]; int nd = 0; uint64_t u = m < 0 ? (uint64_t)0 - (uint64_t)m : (uint64_t)m;
    do { dg[nd++] = (char)('0' + u % 10); u /= 10; } while (u);
    int f = sc == 255 ? 0 : sc; if (f > 18) return -1;
    while (nd < f + 1) dg[nd++] = '0';
    if (kb_res(o, (size_t)nd + 3)) return -1;
    if (m < 0) o->p[o->n++] = '-';
    for (int q = nd - 1; q >= f; q--) o->p[o->n++] = (uint8_t)dg[q];
    if (sc != 255) { o->p[o->n++] = sep; for (int q = f - 1; q >= 0; q--) o->p[o->n++] = (uint8_t)dg[q]; }
    return 0;
}
static uint64_t kg_zz(int64_t x) { return ((uint64_t)x << 1) ^ (uint64_t)(x >> 63); }
static int64_t kg_unzz(uint64_t u) { return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }
/* returns the share of values that are numbers, in percent, without writing anything when probe is set */
static int kg_decimal_fwd(const kbuf *in, uint8_t sep, uint8_t pred, kbuf *tags, kbuf *sc, kbuf *mant, kbuf *txt) {
    size_t i = 0; int64_t prev = 0;
    while (i < in->n) {
        const uint8_t *e = memchr(in->p + i, '\n', in->n - i); if (!e) return -1;
        size_t l = (size_t)(e - (in->p + i)); int64_t m; uint8_t s;
        if (!l) { if (kb_byte(tags, 0)) return -1; }
        else if (kg_num(in->p + i, l, sep, &m, &s)) {
            if (kb_byte(tags, 1) || kb_byte(sc, s) || kb_uv(mant, kg_zz(pred ? (int64_t)((uint64_t)m - (uint64_t)prev) : m))) return -1;
            prev = m;
        } else if (kb_byte(tags, 2) || kb_put(txt, in->p + i, l + 1)) return -1;
        i += l + 1;
    }
    return 0;
}
static int kg_decimal_inv(uint8_t sep, uint8_t pred, const kbuf *tags, const kbuf *sc, const kbuf *mant, const kbuf *txt, kbuf *out) {
    size_t si = 0, mi = 0, ti = 0; int64_t prev = 0;
    for (size_t q = 0; q < tags->n; q++) {
        uint8_t t = tags->p[q];
        if (t == 0) { if (kb_byte(out, '\n')) return -1; }
        else if (t == 1) {
            uint64_t z; if (si >= sc->n || kg_getuv(mant->p, mant->n, &mi, &z)) return -1;
            int64_t m = pred ? (int64_t)((uint64_t)prev + (uint64_t)kg_unzz(z)) : kg_unzz(z); prev = m;
            uint8_t s = sc->p[si++]; if (s != 255 && s > 18) return -1;
            if (kg_numfmt(m, s, sep, out) || kb_byte(out, '\n')) return -1;
        } else if (t == 2) {
            const uint8_t *e = ti < txt->n ? memchr(txt->p + ti, '\n', txt->n - ti) : NULL; if (!e) return -1;
            size_t l = (size_t)(e - (txt->p + ti)) + 1; if (kb_put(out, txt->p + ti, l)) return -1; ti += l;
        } else return -1;
    }
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
 * Writes the payload to dst; returns its length, or 0 when the graph does not apply or does not fit. */
static size_t kg_encode_table(codec_ctx *x, int codec, int level, const uint8_t *src, size_t n, uint8_t d, uint8_t sep,
                              uint8_t *dst, size_t cap) {
    if (n < 64) return 0;
    uint32_t k = 1;                                            /* field count of the second line (the first may be a header) */
    { const uint8_t *e = memchr(src, '\n', n), *a = e && (size_t)(e - src) + 1 < n ? e + 1 : src;
      const uint8_t *b = memchr(a, '\n', n - (size_t)(a - src)); if (!b) b = src + n;
      for (const uint8_t *q = a; q < b; q++) k += *q == d; }
    if (k < 2 || k > 60000) return 0;
    kgb g = {0}; size_t out = 0;
    if (kgb_new(&g) != 0) goto done;
    { kbuf par = {0}; kb_byte(&par, d); kb_uv(&par, k);
      int first = kgb_node(&g, KG_FIELDS, (int[]){0}, 1, (int)k + 1, &par); free(par.p);
      if (first != 1 || kg_fields_fwd(src, n, d, k, &g.s[1], &g.s[2])) goto done; }
    int *tg = malloc(sizeof(int) * k * 5), nt = 0, *scs = tg + k, nsc = 0, *mts = tg + 2 * k, nmt = 0, *txs = tg + 3 * k, ntx = 0, *raws = tg + 4 * k, nraw = 0;
    if (!tg) goto done;
    for (uint32_t j = 0; j < k; j++) {
        int c = 2 + (int)j; kbuf *col = &g.s[c];
        size_t vals = 0, nums = 0, i = 0; int64_t m; uint8_t s;
        while (i < col->n && vals < 4096) { const uint8_t *q = memchr(col->p + i, '\n', col->n - i); size_t l = (size_t)(q - (col->p + i)); vals++; nums += !l || kg_num(col->p + i, l, sep, &m, &s); i += l + 1; }
        if (!vals || nums * 100 < vals * 60) { raws[nraw++] = c; continue; }
        /* predictor: previous value, unless no prediction is cheaper on a sample */
        uint8_t pred = 1;
        { kbuf t0 = {0}, s0 = {0}, m0 = {0}, x0 = {0}, t1 = {0}, s1 = {0}, m1 = {0}, x1 = {0}; kbuf sm = {col->p, i, i};
          if (!kg_decimal_fwd(&sm, sep, 0, &t0, &s0, &m0, &x0) && !kg_decimal_fwd(&sm, sep, 1, &t1, &s1, &m1, &x1) && m0.n < m1.n) pred = 0;
          free(t0.p); free(s0.p); free(m0.p); free(x0.p); free(t1.p); free(s1.p); free(m1.p); free(x1.p); }
        kbuf par = {0}; kb_byte(&par, sep); kb_byte(&par, pred);
        int o = kgb_node(&g, KG_DECIMAL, &c, 1, 4, &par); free(par.p); if (o < 0) { free(tg); goto done; }
        col = &g.s[c];                                         /* the stream table may have moved */
        if (kg_decimal_fwd(col, sep, pred, &g.s[o], &g.s[o + 1], &g.s[o + 2], &g.s[o + 3])) { free(tg); goto done; }
        free(col->p); memset(col, 0, sizeof(kbuf));
        tg[nt++] = o; scs[nsc++] = o + 1; mts[nmt++] = o + 2; txs[ntx++] = o + 3;
    }
    int lv[6], nl = 0; lv[nl++] = 1;                           /* shape */
    if (nt && (lv[nl++] = kgb_concat(&g, tg, nt)) < 0) { free(tg); goto done; }
    if (nsc && (lv[nl++] = kgb_concat(&g, scs, nsc)) < 0) { free(tg); goto done; }
    if (nmt && (lv[nl++] = kgb_concat(&g, mts, nmt)) < 0) { free(tg); goto done; }
    if (ntx && (lv[nl++] = kgb_concat(&g, txs, ntx)) < 0) { free(tg); goto done; }
    if (nraw && (lv[nl++] = kgb_concat(&g, raws, nraw)) < 0) { free(tg); goto done; }
    free(tg);
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
        if (!z || z >= s->n) { cc = C_STORE; z = s->n; memcpy(o.p + o.n + 20, s->p, s->n); }
        o.p[hp] = (uint8_t)cc;
        uint8_t hdr[20]; size_t hn = 0;
        for (uint64_t v = s->n;; v >>= 7) { hdr[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (uint64_t v = z;; v >>= 7) { hdr[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        memmove(o.p + o.n + hn, o.p + o.n + 20, z); memcpy(o.p + o.n, hdr, hn); o.n += hn + z;
    }
    if (o.n < cap && o.n < n) { memcpy(dst, o.p, o.n); out = o.n; }
    free(o.p);
done:
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
    memcpy(out, s[0].p, len); rc = 0;
out:
    if (s) for (uint64_t q = 0; q < ns; q++) free(s[q].p);
    free(s); free(have); free(cons); free(nd); free(ins);
    return rc;
}
