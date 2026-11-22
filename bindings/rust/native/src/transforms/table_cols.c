/* Column-split table blocks (plan type TBL, block flag 13) and the column reader of queryable archives.
 *
 * A table-v2 stream (tbl2.inc) is a header (k, rows, exceptions, group column, formulas, exception rows) followed by
 * one section per column. Flag 4 codes the whole stream with the block codec. Flag 13, written only on request
 * (KOLMO_T_COLUMNS, src/core/index.c), codes the header and every column section as separate segments, so that a
 * query reads one column by decoding the header, that column's segment and the segments it depends on (the group
 * column for grouped predictors, the formula inputs for mode 3), and nothing else. A full decode reassembles the
 * stream and runs the ordinary table-v2 decoder. Layout after the 4-byte plan prefix (SPEC.md, "Column-split tables"):
 *
 *   tn u32 (stream length) | nseg u16 (k + 1) | nseg * (codec u8 | raw u32 | coded u32 | check u32) | coded bytes
 *
 * codec is 0 (stored) or the block's codec; check is the low 32 bits of XXH3-64 (seed 0x74626c63) of the raw segment,
 * verified on every decode, full or partial, because a partial decode cannot verify the block checksum. */

#define TBLC_SEED 0x74626c63u
#define TBLC_ENTRY 13

/* segment boundaries of a table-v2 stream: off[0] = 0, off[1] = end of header, off[j + 2] = end of column j */
static int tblc_split(const uint8_t *t, size_t tn, size_t *off, int *pk) {
    if (tn < 24) return -1;
    uint32_t k = rd32(t), nexc = rd32(t + 8), nform = rd32(t + 20);
    if (k < 1 || k > 256 || nform > 256 || nexc > tn) return -1;
    uint64_t p = 24 + 16ull * nform + 8ull * nexc; if (p > tn) return -1;
    off[0] = 0; off[1] = (size_t)p;
    for (uint32_t j = 0; j < k; j++) {
        if (p + 5 > tn) return -1;
        p += 5 + (uint64_t)rd32(t + p + 1); if (p > tn) return -1;
        off[j + 2] = (size_t)p;
    }
    if (p != tn) return -1;
    *pk = (int)k; return 0;
}

/* Returns the payload length (after the plan prefix), or 0 when the stream does not split or does not fit in cap. */
static size_t tblc_compress(codec_ctx *x, int codec, int level, const uint8_t *t, size_t tn, uint8_t *dst, size_t cap) {
    size_t off[258]; int k;
    if (tblc_split(t, tn, off, &k)) return 0;
    int nseg = k + 1; size_t dir = 6 + (size_t)nseg * TBLC_ENTRY, pos = dir;
    if (cap < dir) return 0;
    put32(dst, (uint32_t)tn); dst[4] = (uint8_t)nseg; dst[5] = (uint8_t)(nseg >> 8);
    for (int s = 0; s < nseg; s++) {
        const uint8_t *seg = t + off[s]; size_t n = off[s + 1] - off[s];
        uint8_t *e = dst + 6 + (size_t)s * TBLC_ENTRY;
        size_t z = n >= 32 && cap - pos > 64 ? codec_compress(x, codec, level, seg, n, dst + pos, cap - pos) : 0;
        int cc = codec;
        if (!z || z >= n) { if (cap - pos < n) return 0; memcpy(dst + pos, seg, n); z = n; cc = C_STORE; }
        e[0] = codec_wire(x, cc); put32(e + 1, (uint32_t)n); put32(e + 5, (uint32_t)z); put32(e + 9, (uint32_t)kolmo_xxh3(seg, n, TBLC_SEED));
        pos += z;
    }
    return pos;
}

typedef struct { int nseg; size_t tn; const uint8_t *dir, *data; size_t dn; size_t at[258], raw[258], z[258]; } tblc_dir;
static int tblc_parse(int codec, const uint8_t *pay, size_t cl, size_t maxtn, tblc_dir *D) {
    if (cl < 6) return -1;
    D->tn = get32(pay); D->nseg = pay[4] | pay[5] << 8;
    if (D->tn > maxtn || D->nseg < 2 || D->nseg > 257) return -1;
    size_t dir = 6 + (size_t)D->nseg * TBLC_ENTRY; if (dir > cl) return -1;
    D->dir = pay + 6; D->data = pay + dir; D->dn = cl - dir;
    size_t at = 0, tot = 0;
    for (int s = 0; s < D->nseg; s++) {
        const uint8_t *e = D->dir + (size_t)s * TBLC_ENTRY;
        if (e[0] != C_STORE && e[0] != codec) return -1;
        D->raw[s] = get32(e + 1); D->z[s] = get32(e + 5); D->at[s] = at;
        if (D->z[s] > D->dn - at || (e[0] == C_STORE && D->z[s] != D->raw[s])) return -1;
        at += D->z[s]; tot += D->raw[s];
        if (tot > D->tn) return -1;
    }
    return at == D->dn && tot == D->tn ? 0 : -1;
}
static int tblc_segment(codec_ctx *x, const tblc_dir *D, int s, uint8_t *out) {
    const uint8_t *e = D->dir + (size_t)s * TBLC_ENTRY;
    if (e[0] == C_STORE) memcpy(out, D->data + D->at[s], D->raw[s]);
    else if (codec_decompress(x, e[0], D->data + D->at[s], D->z[s], out, D->raw[s])) return -1;
    return (uint32_t)kolmo_xxh3(out, D->raw[s], TBLC_SEED) == get32(e + 9) ? 0 : -1;
}
/* full decode: the table-v2 stream into t (capacity maxtn); returns its length or -1 */
static int64_t tblc_decompress(codec_ctx *x, int codec, const uint8_t *pay, size_t cl, uint8_t *t, size_t maxtn) {
    tblc_dir D; if (tblc_parse(codec, pay, cl, maxtn, &D)) return -1;
    size_t o = 0;
    for (int s = 0; s < D.nseg; s++) { if (tblc_segment(x, &D, s, t + o)) return -1; o += D.raw[s]; }
    size_t off[258]; int k;                                      /* the segments must be the header and k columns */
    if (tblc_split(t, D.tn, off, &k) || k + 1 != D.nseg) return -1;
    for (int s = 0; s < D.nseg; s++) if (off[s + 1] - off[s] != D.raw[s]) return -1;
    return (int64_t)D.tn;
}

/* One column section of a table-v2 stream into c (the checks of tbl2_decode). want = the rows that hold this field. */
static int tblc_col(const uint8_t *p, size_t n, uint32_t want, Col2 *c) {
    memset(c, 0, sizeof *c);
    if (n < 5) return -1;
    c->type = p[0]; uint32_t size = rd32(p + 1); if (size != n - 5) return -1;
    const uint8_t *q = p + 5, *qe = p + n;
    if (c->type == 0) {
        if (size < 8) return -1;
        c->cnt = rd32(q); c->nlong = rd32(q + 4); q += 8;
        if (c->cnt != want || c->cnt > size || c->nlong > size) return -1;
        c->lens = q; q += c->cnt; c->longs = q; q += 4ull * c->nlong; c->data = q; c->dend = qe;
        if (q > qe) return -1;
        uint64_t tot = 0; uint32_t nl = 0;
        for (uint32_t i = 0; i < c->cnt; i++) { uint32_t L8 = c->lens[i]; nl += L8 == 255; tot += L8; }
        if (nl != c->nlong) return -1;
        tot -= 255ull * nl; for (uint32_t i = 0; i < nl; i++) tot += rd32(c->longs + 4ull * i);
        return tot > (uint64_t)(c->dend - c->data) ? -1 : 0;
    }
    if (c->type != 2 || size < 16) return -1;
    c->cnt = rd32(q); c->nnum = rd32(q + 4); c->nexc = rd32(q + 8);
    c->S = q[12]; c->w = q[13]; c->mode = q[14]; c->fid = q[15]; q += 16;
    if (c->S > 18 || c->w < 1 || c->w > 8 || c->mode > 5 || c->cnt != want || c->cnt > size || c->nnum > c->cnt || c->nexc > c->cnt) return -1;
    c->kind = q; q += c->cnt; c->sd = q; q += c->nnum; c->pl = q; q += (size_t)c->nnum * c->w;
    c->elen = q; q += 4ull * c->nexc; c->edata = q; c->dend = qe;
    if (q > qe) return -1;
    uint32_t n1 = 0, n2 = 0, bad = 0;
    for (uint32_t i = 0; i < c->cnt; i++) { uint8_t kd = c->kind[i]; n1 += kd == 1; n2 += kd == 2; bad |= kd > 2; }
    if (bad || n1 != c->nnum || n2 != c->nexc) return -1;
    uint8_t mx = 0; for (uint32_t i = 0; i < c->nnum; i++) mx = c->sd[i] > mx ? c->sd[i] : mx;
    if (mx > c->S) return -1;
    uint64_t tot = 0; for (uint32_t i = 0; i < c->nexc; i++) tot += rd32(c->elen + 4ull * i);
    return tot > (uint64_t)(c->dend - c->edata) ? -1 : 0;
}

/* Column col of a flag-13 block: every row's field as text followed by '\n' (an empty line for a row without that
 * field), appended to out. *seg_bytes and *seg_coded receive the decoded and stored bytes read. Returns 0, or -1 when the
 * payload is corrupt (a checksum or a structure check failed); out is then undefined. */
static int tblc_column(codec_ctx *x, int codec, const uint8_t *pay, size_t cl, size_t len, int col, kbuf *out, uint64_t *seg_bytes, uint64_t *seg_coded) {
    tblc_dir D; if (tblc_parse(codec, pay, cl, len + TSLACK, &D)) return -1;
    int k = D.nseg - 1; if (col < 0 || col >= k) return -1;
    uint8_t *seg[257] = {0}; Col2 C[3]; int ci[3] = {-1, -1, -1};       /* C[0] the column, C[1], C[2] formula inputs */
    uint32_t *gid = NULL, *ridx = NULL; int64_t *vals[3] = {0}; double *fa = NULL, *fb = NULL, *la = NULL;
    Col2 G; int ret = -1; uint64_t used = 0, coded = 6 + (uint64_t)D.nseg * TBLC_ENTRY;
#define TBLC_LOAD(s) (seg[s] ? 0 : (seg[s] = malloc(D.raw[s] + 1)) == NULL || tblc_segment(x, &D, (s), seg[s]) ? -1 : (used += D.raw[s], coded += D.z[s], 0))
    if (TBLC_LOAD(0)) goto out;
    const uint8_t *h = seg[0]; size_t hn = D.raw[0];
    if (hn < 24 || rd32(h) != (uint32_t)k) goto out;
    int64_t nr = rd32(h + 4), nexc = rd32(h + 8), noNL = rd32(h + 12); int32_t gcol; memcpy(&gcol, h + 16, 4); uint32_t nform = rd32(h + 20);
    (void)noNL;
    if (nform > 256 || gcol >= k || gcol < -1 || 24 + 16ull * nform + 8ull * nexc != hn || (uint64_t)nr > len + 1) goto out;
    tformula FB[256];
    for (uint32_t q = 0; q < nform; q++) {
        const uint8_t *e = h + 24 + 16 * q;
        memcpy(&FB[q].y, e, 2); memcpy(&FB[q].a, e + 2, 2); memcpy(&FB[q].b, e + 4, 2); FB[q].form = e[6]; memcpy(&FB[q].k, e + 8, 8);
        if (FB[q].form < FM_RATIO || FB[q].form > FM_PCT || FB[q].y >= k || FB[q].a >= k || (FB[q].form == FM_RATIO && FB[q].b >= k)) goto out;
    }
    const uint8_t *exc = h + 24 + 16 * nform;
    uint32_t want[256]; for (int j = 0; j < k; j++) want[j] = (uint32_t)nr;
    for (int64_t e = 0; e < nexc; e++) {
        uint32_t er = rd32(exc + 8 * e), cc = rd32(exc + 8 * e + 4);
        if (cc < 1 || cc > (uint32_t)k || er >= (uint64_t)nr || (e && er <= rd32(exc + 8 * (e - 1)))) goto out;
        for (uint32_t j = cc; j < (uint32_t)k; j++) want[j]--;
    }
    ci[0] = col; if (TBLC_LOAD(col + 1) || tblc_col(seg[col + 1], D.raw[col + 1], want[col], &C[0])) goto out;
    int needg = 0; const tformula *F = NULL;
    if (C[0].type == 2) {
        if (C[0].mode == 3) {
            if ((uint32_t)C[0].fid >= nform) goto out;
            F = &FB[C[0].fid]; if (F->y != col) goto out;
            ci[1] = F->a; if (F->form == FM_RATIO) ci[2] = F->b;
            if (F->form != FM_RATIO) needg = 1;
        } else needg = C[0].mode == 2 || C[0].mode >= 4;
    }
    for (int r = 1; r < 3; r++) if (ci[r] >= 0) {
        if (TBLC_LOAD(ci[r] + 1) || tblc_col(seg[ci[r] + 1], D.raw[ci[r] + 1], want[ci[r]], &C[r])) goto out;
        if (C[r].type != 2 || C[r].mode == 3) goto out;
        needg |= C[r].mode == 2 || C[r].mode >= 4;
    }
    gid = malloc(4 * ((size_t)nr + 1)); ridx = malloc(4 * ((size_t)nr + 2));
    if (!gid || !ridx) goto out;
    if (needg) {                                                 /* group ids from the group column (aligned tables only) */
        if (gcol < 0 || nexc) goto out;
        if (TBLC_LOAD(gcol + 1) || tblc_col(seg[gcol + 1], D.raw[gcol + 1], want[gcol], &G) || G.type != 0) goto out;
        const uint8_t *d = G.data, *pd = NULL; uint32_t pl = 0, li = 0, gg = 0;
        for (int64_t r = 0; r < nr; r++) {
            uint32_t L = G.lens[r];
            if (L == 255) { if (li >= G.nlong) goto out; L = rd32(G.longs + 4ull * li++); }
            if (L > (size_t)(G.dend - d)) goto out;
            if (r && (L != pl || memcmp(d, pd, L))) gg++;
            gid[r] = gg; pd = d; pl = L; d += L;
        }
    } else for (int64_t r = 0; r < nr; r++) gid[r] = 0;
    for (int r = 2; r >= 0; r--) if (ci[r] >= 0 && C[r].type == 2) {     /* inputs first, then the column */
        C[r].ridx = ridx; vals[r] = malloc(8 * ((size_t)C[r].nnum + 1)); if (!vals[r]) goto out;
        C[r].vals = vals[r];
        if (r == 0 && F) {
            if (C[0].cnt != (uint32_t)nr || C[1].cnt != (uint32_t)nr || (ci[2] >= 0 && C[2].cnt != (uint32_t)nr)) goto out;
            fa = col2_fvals(&C[1], (uint32_t)nr); if (!fa) goto out;
            if (ci[2] >= 0 && !(fb = col2_fvals(&C[2], (uint32_t)nr))) goto out;
            if (F->form != FM_RATIO) { la = malloc(sizeof(double) * ((size_t)nr + 1)); if (!la) goto out;
                                       la[0] = NAN; for (int64_t t = 1; t < nr; t++) la[t] = gid[t] == gid[t - 1] ? fa[t - 1] : NAN; }
            if (num_decode(&C[0], gid, F, fa, fb, la, vals[0])) goto out;
        } else if (num_decode(&C[r], gid, NULL, NULL, NULL, NULL, vals[r])) goto out;
    }
    /* emit: one line per row */
    {   Col2 *c = &C[0]; uint32_t i = 0, li = 0, ni = 0, xi = 0; const uint8_t *d = c->type == 0 ? c->data : c->edata; int64_t e = 0;
        for (int64_t r = 0; r < nr; r++) {
            uint32_t cnt = (uint32_t)k;
            if (e < nexc && rd32(exc + 8 * e) == (uint64_t)r) { cnt = rd32(exc + 8 * e + 4); e++; }
            if ((uint32_t)col < cnt) {
                if (i >= c->cnt) goto out;
                if (c->type == 0) {
                    uint32_t L = c->lens[i];
                    if (L == 255) { if (li >= c->nlong) goto out; L = rd32(c->longs + 4ull * li++); }
                    if (L > (size_t)(c->dend - d) || kb_put(out, d, L)) goto out;
                    d += L;
                } else {
                    uint8_t kd = c->kind[i];
                    if (kd == 1) {
                        int ds = c->sd[ni], cut = ds + (ds == c->S && ds); uint8_t tb[48];
                        uint8_t *te = fmt_dec_fast(tb, vals[0][ni++], c->S) - cut;
                        if (kb_put(out, tb, (size_t)(te - tb))) goto out;
                    } else if (kd == 2) {
                        uint32_t L = rd32(c->elen + 4ull * xi++);
                        if (L > (size_t)(c->dend - d) || kb_put(out, d, L)) goto out;
                        d += L;
                    }
                }
                i++;
            }
            if (kb_byte(out, '\n')) goto out;
        }
        if (i != c->cnt) goto out;
    }
    ret = 0;
out:
#undef TBLC_LOAD
    if (seg_bytes) *seg_bytes = used;
    if (seg_coded) *seg_coded = coded;
    for (int s = 0; s < 257; s++) free(seg[s]);
    for (int r = 0; r < 3; r++) free(vals[r]);
    free(gid); free(ridx); free(fa); free(fb); free(la);
    return ret;
}
