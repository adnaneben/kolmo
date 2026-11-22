/* Column table transform of v0.3 (tflag 3). */
/* ---------------------------------------------------------------- table transform (v0.3)
 * Rows are split into fields (quote-aware, same rules as kolmo). Each column becomes one section:
 *   text    : field lengths (u8, 255 = long, then u32) + the bytes, no delimiters
 *   numeric : canonical decimals (-?(0|[1-9]d*)(.d+)?, <= 18 digits) as int64 at the column's largest scale,
 *             delta-coded down the column, zigzag, byte planes; per value a kind (0 empty, 1 number, 2 other text)
 *             and its scale; "other" values stored as text. Decoding needs no parsing or scanning.
 * Layout: k | nrows | nexc | noNL (u32) | exceptions (row,cnt) u32 | per column: type u8, size u32 | sections */
static inline size_t field_end_q(const uint8_t *in, size_t i, size_t n, uint8_t delim, int split) {
    int inq = 0, inbr = 0;
    for (; i < n; i++) {
        uint8_t c = in[i];
        if (c == '\n') return i;
        if (c == '"' && !inbr) inq ^= 1; else if (c == '[' && !inq) inbr = 1; else if (c == ']' && !inq) inbr = 0;
        else if (c == delim && split && !inq && !inbr) return i;
    }
    return n;
}

/* encode one column given its fields (offsets/lengths into in); appends type+size+section to out */
static void col_encode(const uint8_t *in, const uint32_t *off, const uint32_t *len, uint32_t cnt, Buf *out) {
    /* try numeric */
    int64_t *v = malloc(8 * (cnt + 1)); uint8_t *kind = malloc(cnt + 1), *sc = malloc(cnt + 1);
    uint32_t nnum = 0, nexc = 0, nempty = 0; int S = 0;
    for (uint32_t i = 0; i < cnt; i++) {
        int64_t m; int s;
        if (len[i] == 0) { kind[i] = 0; nempty++; }
        else if (parse_dec(in + off[i], len[i], &m, &s)) { kind[i] = 1; v[nnum] = m; sc[nnum] = (uint8_t)s; nnum++; if (s > S) S = s; }
        else { kind[i] = 2; nexc++; }
    }
    int numeric = nnum > 0 && nexc * 20 <= cnt && nnum * 4 >= cnt - nempty;
    if (numeric) {                                          /* common scale; overflow -> text */
        for (uint32_t i = 0; i < nnum && numeric; i++) if (__builtin_mul_overflow(v[i], P10[S - sc[i]], &v[i]) || v[i] > (1LL << 62) || v[i] < -(1LL << 62)) numeric = 0;
    }
    if (!numeric) {
        uint32_t nlong = 0; for (uint32_t i = 0; i < cnt; i++) nlong += len[i] >= 255;
        size_t data = 0; for (uint32_t i = 0; i < cnt; i++) data += len[i];
        uint8_t t = 0; uint32_t size = 8 + cnt + 4 * nlong + data; bput(out, &t, 1); bput(out, &size, 4);
        bput(out, &cnt, 4); bput(out, &nlong, 4);
        size_t at = out->n; bput(out, kind, 0);
        if (out->n + cnt > out->cap) { out->cap = (out->n + cnt) * 2; out->p = zs_realloc(out->p, out->cap); }
        for (uint32_t i = 0; i < cnt; i++) out->p[at + i] = len[i] >= 255 ? 255 : (uint8_t)len[i];
        out->n += cnt;
        for (uint32_t i = 0; i < cnt; i++) if (len[i] >= 255) bput(out, &len[i], 4);
        for (uint32_t i = 0; i < cnt; i++) bput(out, in + off[i], len[i]);
    } else {
        uint64_t mx = 0; int64_t prev = 0;
        uint64_t *z = malloc(8 * (nnum + 1));
        for (uint32_t i = 0; i < nnum; i++) { uint64_t d = (uint64_t)v[i] - (uint64_t)prev; prev = v[i]; int64_t sd = (int64_t)d; z[i] = ((uint64_t)sd << 1) ^ (uint64_t)(sd >> 63); if (z[i] > mx) mx = z[i]; }
        int w = 1; while (w < 8 && (mx >> (8 * w))) w++;
        uint32_t elen = 0; for (uint32_t i = 0; i < cnt; i++) if (kind[i] == 2) elen += len[i];
        uint8_t t = 1; uint32_t size = 4 * 3 + 2 + cnt + nnum + (size_t)nnum * w + 4 * nexc + elen;
        bput(out, &t, 1); bput(out, &size, 4); bput(out, &cnt, 4); bput(out, &nnum, 4); bput(out, &nexc, 4);
        uint8_t Sw[2] = {(uint8_t)S, (uint8_t)w}; bput(out, Sw, 2);
        bput(out, kind, cnt);
        for (uint32_t i = 0; i < nnum; i++) sc[i] = (uint8_t)(S - sc[i]);
        bput(out, sc, nnum);
        size_t at = out->n; if (out->n + (size_t)nnum * w > out->cap) { out->cap = (out->n + (size_t)nnum * w) * 2 + 16; out->p = zs_realloc(out->p, out->cap); }
        for (int b = 0; b < w; b++) for (uint32_t i = 0; i < nnum; i++) out->p[at + (size_t)b * nnum + i] = (uint8_t)(z[i] >> (8 * b));
        out->n += (size_t)nnum * w;
        for (uint32_t i = 0; i < cnt; i++) if (kind[i] == 2) bput(out, &len[i], 4);
        for (uint32_t i = 0; i < cnt; i++) if (kind[i] == 2) bput(out, in + off[i], len[i]);
        free(z);
    }
    free(v); free(kind); free(sc);
}

static int64_t tbl_encode(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, uint8_t *dst) {
    if (quotes && !has_quotes(in, n)) quotes = 0;
    uint32_t *rec = malloc(4 * (n + 2)), *exc = malloc(8 * (n + 2));
    uint32_t colcnt[256] = {0}; int64_t nf = 0, nr = 0, nexc = 0; int noNL = 0;
    int64_t i = 0;
    while (i < n) {
        int j = 0;
        for (;;) {
            int split = j < k - 1; size_t e;
            if (quotes) e = field_end_q(in, i, n, delim, split);
            else e = i + (split ? scan2(in + i, n - i, delim, '\n') : scan2(in + i, n - i, '\n', '\n'));
            rec[nf++] = (uint32_t)(e - i); colcnt[j]++;
            if (e >= (size_t)n) { noNL = 1; nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } i = n; break; }
            if (in[e] == '\n') { nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } i = e + 1; break; }
            j++; i = e + 1;
            if (i == n) { rec[nf++] = 0; colcnt[j]++; noNL = 1; nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } break; }
        }
    }
    /* column-major (offset, length) lists */
    uint32_t *coff = malloc(4 * (nf + 1)), *clen = malloc(4 * (nf + 1)); int64_t lpos[256], pos = 0, base[256];
    for (int j = 0; j < k; j++) { base[j] = lpos[j] = pos; pos += colcnt[j]; }
    int64_t f = 0, ei = 0, src = 0;
    for (int64_t r = 0; r < nr; r++) {
        int cnt = (ei < nexc && exc[2 * ei] == r) ? (int)exc[2 * ei++ + 1] : k;
        for (int j = 0; j < cnt; j++) { uint32_t L = rec[f++]; coff[lpos[j]] = (uint32_t)src; clen[lpos[j]++] = L; src += L + 1; }
    }
    Buf out = {0}; uint32_t h[4] = {(uint32_t)k, (uint32_t)nr, (uint32_t)nexc, (uint32_t)noNL};
    bput(&out, h, 16); bput(&out, exc, 8 * nexc);
    for (int j = 0; j < k; j++) col_encode(in, coff + base[j], clen + base[j], colcnt[j], &out);
    free(rec); free(exc); free(coff); free(clen);
    int64_t total = out.n;
    if (total > n + TSLACK - 64) { zs_free(out.p); return -1; }
    memcpy(dst, out.p, total); zs_free(out.p);
    return total;
}

typedef struct { int type; uint32_t cnt, nnum, nexc, nlong; int S, w;
                 const uint8_t *lens, *longs, *data, *dend, *kind, *sd, *pl, *elen, *edata; uint32_t ki, ni, li, xi; int64_t prev; int64_t *vals; } Col;

static int tbl_decode(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n) {
    if (tn < 16) return -1;
    uint32_t h[4]; memcpy(h, in, 16); int k = h[0]; int64_t nr = h[1], nexc = h[2], noNL = h[3];
    /* every row but the last ends in a newline, so nr <= n + 1; checked before the 9 * nr bytes of row arrays */
    if (k < 1 || k > 256 || 16 + 8 * nexc > tn || nr > n + 1 || nexc > nr) return -1;
    const uint8_t *exc = in + 16, *p = in + 16 + 8 * nexc, *end = in + tn;
    Col C[256];
    for (int j = 0; j < k; j++) {
        if (p + 5 > end) return -1;
        Col *c = &C[j]; memset(c, 0, sizeof *c); c->type = p[0]; uint32_t size; memcpy(&size, p + 1, 4); p += 5;
        if (c->type > 1) return -1;
        const uint8_t *q = p, *qe = p + size; if (qe > end) return -1;
        if (c->type == 0) {
            if (size < 8) return -1;
            memcpy(&c->cnt, q, 4); memcpy(&c->nlong, q + 4, 4); q += 8;
            c->lens = q; q += c->cnt; c->longs = q; q += 4ull * c->nlong; c->data = q; c->dend = qe;
            if (q > qe) return -1;
        } else {
            if (size < 14) return -1;
            memcpy(&c->cnt, q, 4); memcpy(&c->nnum, q + 4, 4); memcpy(&c->nexc, q + 8, 4); c->S = q[12]; c->w = q[13]; q += 14;
            if (c->S > 18 || c->w < 1 || c->w > 8) return -1;
            c->kind = q; q += c->cnt; c->sd = q; q += c->nnum; c->pl = q; q += (size_t)c->nnum * c->w;
            c->elen = q; q += 4ull * c->nexc; c->edata = q; c->dend = qe;
            if (q > qe) return -1;
        }
        p = qe;
    }
    /* each row takes a field of column 0 (a row has 1..k fields), so a row count above it cannot decode */
    if (nr > (int64_t)C[0].cnt) return -1;
    /* numeric columns decoded column-at-a-time (sequential plane reads), mantissas ready for formatting */
    size_t totnum = 0; for (int j = 0; j < k; j++) if (C[j].type == 1) totnum += C[j].nnum;
    int64_t *vals = malloc(8 * (totnum + 1)), *vp = vals;
    if (!vals) return -1;
    for (int j = 0; j < k; j++) {
        Col *c = &C[j]; if (c->type != 1) continue;
        c->vals = vp; uint32_t m = c->nnum;
        memset(vp, 0, 8 * (size_t)m);
        for (int b = 0; b < c->w; b++) { const uint8_t *pl = c->pl + (size_t)b * m; for (uint32_t i = 0; i < m; i++) vp[i] |= (int64_t)((uint64_t)pl[i] << (8 * b)); }
        int64_t prev = 0;
        for (uint32_t i = 0; i < m; i++) {
            uint64_t zz = (uint64_t)vp[i]; int64_t d = (int64_t)(zz >> 1) ^ -(int64_t)(zz & 1);
            prev = (int64_t)((uint64_t)prev + (uint64_t)d);
            int ds = c->sd[i]; if (ds > c->S) goto fail;
            vp[i] = ds ? exact_div_p10(prev, ds) : prev;
        }
        vp += m;
    }
    /* phase A: every column as contiguous bytes + per-field lengths (numeric columns formatted here, column by column) */
    uint32_t *L = NULL, *rowoff = NULL, *cur = NULL; uint8_t *arena = NULL, *rcnt = NULL;
    const uint8_t *cb[256]; uint32_t *cl[256]; size_t nf = 0, ar = 0;
    for (int j = 0; j < k; j++) {
        nf += C[j].cnt;
        if (C[j].type == 1) ar += (size_t)C[j].nnum * 21 + (size_t)(C[j].dend - C[j].edata);
    }
    L = malloc(4 * (nf + 1)); arena = malloc(ar + 128);
    rowoff = malloc(4 * ((size_t)nr + 1)); cur = malloc(4 * ((size_t)nr + 1)); rcnt = malloc((size_t)nr + 1);
    if (!L || !arena || !rowoff || !cur || !rcnt) goto fail2;
    {
        uint32_t *lp = L; uint8_t *ap = arena;
        for (int j = 0; j < k; j++) {
            Col *c = &C[j]; cl[j] = lp;
            if (c->type == 0) {
                cb[j] = c->data; size_t tot = 0;
                for (uint32_t i2 = 0; i2 < c->cnt; i2++) {
                    uint32_t len = c->lens[i2];
                    if (len == 255) { if (c->li >= c->nlong) goto fail2; memcpy(&len, c->longs + 4ull * c->li++, 4); }
                    lp[i2] = len; tot += len;
                }
                if (c->data + tot > c->dend) goto fail2;
            } else {
                cb[j] = ap; uint32_t ni = 0, xi = 0; const uint8_t *ed = c->edata;
                for (uint32_t i2 = 0; i2 < c->cnt; i2++) {
                    uint8_t kd = c->kind[i2]; uint8_t *st = ap;
                    if (kd == 1) { if (ni >= c->nnum) goto fail2; int ds = c->sd[ni]; ap = fmt_dec_fast(ap, c->vals[ni], c->S - ds); ni++; }
                    else if (kd == 2) {
                        if (xi >= c->nexc) goto fail2;
                        uint32_t len; memcpy(&len, c->elen + 4ull * xi++, 4);
                        if (ed + len > c->dend || (size_t)(ap - arena) + len > ar) goto fail2;
                        memcpy(ap, ed, len); ap += len; ed += len;
                    } else if (kd != 0) goto fail2;
                    lp[i2] = (uint32_t)(ap - st);
                }
            }
            lp += c->cnt;
        }
    }
    /* phase B: fields per row, row lengths, row offsets */
    {
        memset(rcnt, (uint8_t)0, (size_t)nr); int64_t ei = 0;
        for (int64_t r = 0; r < nr; r++) rcnt[r] = 0;
        for (int64_t r = 0, e = 0; r < nr; r++) {
            int cnt = k;
            if (e < nexc) { uint32_t er; memcpy(&er, exc + 8 * e, 4); if (er == r) { uint32_t cc; memcpy(&cc, exc + 8 * e + 4, 4); cnt = (int)cc; e++; if (cnt < 1 || cnt > k) goto fail2; } }
            rcnt[r] = (uint8_t)(cnt - 1);                 /* k <= 256 */
            cur[r] = (uint32_t)(cnt - 1) + ((noNL && r == nr - 1) ? 0 : 1);
        }
        (void)ei;
        for (int j = 0; j < k; j++) {
            const uint32_t *lp = cl[j]; uint32_t i2 = 0, cntj = C[j].cnt;
            for (int64_t r = 0; r < nr; r++) {
                if (rcnt[r] < j) continue;
                if (i2 >= cntj) goto fail2;
                cur[r] += lp[i2++];
            }
            if (i2 != cntj) goto fail2;
        }
        uint64_t acc = 0;
        for (int64_t r = 0; r < nr; r++) { rowoff[r] = (uint32_t)acc; acc += cur[r]; cur[r] = 0; }
        rowoff[nr] = (uint32_t)acc;
        if (acc != (uint64_t)n) goto fail2;
    }
    /* phase C: column-major copy in row chunks that stay in cache; rows are independent inside a pass */
    {
        const uint8_t *sp[256], *se[256]; uint32_t ip[256];
        for (int j = 0; j < k; j++) { sp[j] = cb[j]; se[j] = C[j].type == 0 ? C[j].dend : arena + ar; ip[j] = 0; }
        const int64_t CH = 1024;
        for (int64_t r0 = 0; r0 < nr; r0 += CH) {
            int64_t r1 = r0 + CH < nr ? r0 + CH : nr;
            for (int j = 0; j < k; j++) {
                const uint32_t *lp = cl[j]; const uint8_t *src = sp[j], *send = se[j]; uint32_t i2 = ip[j];
                for (int64_t r = r0; r < r1; r++) {
                    int last = rcnt[r];
                    if (last < j) continue;
                    uint32_t len = lp[i2++]; uint8_t *d = out + rowoff[r] + cur[r], *rend = out + rowoff[r + 1];
                    if (len <= 16 && d + 16 <= rend && src + 16 <= send) memcpy(d, src, 16); else memcpy(d, src, len);
                    src += len; d += len;
                    if (j < last) *d = delim; else if (d < rend) *d = '\n';
                    cur[r] += len + 1;
                }
                sp[j] = src; ip[j] = i2;
            }
        }
    }
    free(L); free(arena); free(rowoff); free(cur); free(rcnt);
    free(vals); return 0;
fail2:
    free(L); free(arena); free(rowoff); free(cur); free(rcnt);
fail:
    free(vals); return -1;
}

/* exported for tests */
int64_t tbl_encode_pub(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, uint8_t *dst) { return tbl_encode(in, n, delim, k, quotes, dst); }
int tbl_decode_pub(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n) { return tbl_decode(in, tn, delim, out, n); }
