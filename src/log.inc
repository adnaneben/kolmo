/* ---------------------------------------------------------------- log template transform (block flag 5)
 * Lines are split on single spaces. A token holding a digit (or byte 0x01) is variable; the rest of the line is its
 * template. Templates seen at least LOG_MIN times are kept; other lines go verbatim to a "rare" stream. The variable
 * tokens of each template position form a column: "skeleton" columns (same punctuation, digit runs that differ)
 * store each digit run as an integer column (raw or delta, zigzag, byte planes); other columns store the text.
 * Layout: ver u8 | L u32 | T u32 | tail u32 | dict u32 | rare u32 | lid w u8 mode u8 | lid planes | dict | rare | tail
 *         | columns (template order, placeholder order): type u8 + body
 *   text column:     size u32 | values, each followed by '\n'
 *   skeleton column: g u8 | g+1 pieces (len u8 + bytes) | per run: pad u8, mode u8, w u8 | planes per run */
#ifndef LOG_MIN
#define LOG_MIN 4
#endif
#define LOG_MAXRUN 12

typedef struct { const uint8_t *p; uint32_t len; } tok_t;

static inline int is_var_tok(const uint8_t *p, uint32_t len) {     /* holds a digit or byte 0x01 */
    uint32_t i = 0;
    for (; i + 8 <= len; i += 8) {                                    /* SWAR: bytes in '0'..'9' (ASCII only) or == 1 */
        uint64_t x; memcpy(&x, p + i, 8);
        const uint64_t L7 = 0x7F7F7F7F7F7F7F7Full, H = 0x8080808080808080ull;
        uint64_t lo = x & L7;
        uint64_t dig = ((0x0101010101010101ull * (127 + '9' + 1) - lo) & ~x & (lo + 0x0101010101010101ull * (127 - ('0' - 1)))) & H;
        uint64_t y = x ^ 0x0101010101010101ull, one = ~(((y & L7) + L7) | y | L7);
        if (dig | one) return 1;
    }
    for (; i < len; i++) if ((uint8_t)(p[i] - '0') < 10 || p[i] == 1) return 1;
    return 0;
}
static uint64_t fnv1a(const uint8_t *p, size_t n) { uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; } return h; }

/* integer column: mode 0 raw / 1 delta (by cost), zigzag, byte planes */
static void put_ints(Buf *out, const int64_t *v, uint32_t m) {
    uint64_t c0 = 0, c1 = 0, mx0 = 0, mx1 = 0; int64_t prev = 0;
    for (uint32_t i = 0; i < m; i++) {
        uint64_t a = zzg(v[i]), b = zzg((int64_t)((uint64_t)v[i] - (uint64_t)prev)); prev = v[i];
        c0 += bitlen(a); c1 += bitlen(b); if (a > mx0) mx0 = a; if (b > mx1) mx1 = b;
    }
    int mode = c1 < c0; uint64_t mx = mode ? mx1 : mx0;
    int w = 1; while (w < 8 && (mx >> (8 * w))) w++;
    uint8_t hd[2] = {(uint8_t)mode, (uint8_t)w}; bput(out, hd, 2);
    if (out->n + (size_t)m * w > out->cap) { out->cap = (out->n + (size_t)m * w) * 2 + 64; out->p = realloc(out->p, out->cap); }
    uint8_t *o = out->p + out->n; prev = 0;
    for (uint32_t i = 0; i < m; i++) {
        uint64_t z = mode ? zzg((int64_t)((uint64_t)v[i] - (uint64_t)prev)) : zzg(v[i]); prev = v[i];
        for (int b = 0; b < w; b++) o[(size_t)b * m + i] = (uint8_t)(z >> (8 * b));
    }
    out->n += (size_t)m * w;
}
static int get_ints(const uint8_t **pp, const uint8_t *end, uint32_t m, int64_t *v) {
    const uint8_t *p = *pp;
    if (end - p < 2) return -1;
    int mode = p[0], w = p[1]; p += 2;
    if (mode > 1 || w < 1 || w > 8 || (uint64_t)(end - p) < (uint64_t)m * w) return -1;
    const uint8_t *p0 = p, *p1 = p + m, *p2 = p1 + m, *p3 = p2 + m;
    switch (w) {                                          /* byte planes -> values in one pass (vectorizes) */
        case 1: for (uint32_t i = 0; i < m; i++) v[i] = p0[i]; break;
        case 2: for (uint32_t i = 0; i < m; i++) v[i] = (int64_t)(p0[i] | (uint64_t)p1[i] << 8); break;
        case 3: for (uint32_t i = 0; i < m; i++) v[i] = (int64_t)(p0[i] | (uint64_t)p1[i] << 8 | (uint64_t)p2[i] << 16); break;
        case 4: for (uint32_t i = 0; i < m; i++) v[i] = (int64_t)(p0[i] | (uint64_t)p1[i] << 8 | (uint64_t)p2[i] << 16 | (uint64_t)p3[i] << 24); break;
        default: for (uint32_t i = 0; i < m; i++) { uint64_t z = 0; for (int b = 0; b < w; b++) z |= (uint64_t)p[(size_t)b * m + i] << (8 * b); v[i] = (int64_t)z; }
    }
    if (mode) { uint64_t prev = 0; for (uint32_t i = 0; i < m; i++) { prev += (uint64_t)unzz((uint64_t)v[i]); v[i] = (int64_t)prev; } }
    else for (uint32_t i = 0; i < m; i++) v[i] = unzz((uint64_t)v[i]);
    *pp = p + (size_t)m * w; return 0;
}

/* skeleton parse of one token: pieces (non-digit runs) and digit runs; returns run count or -1 */
static int skel_parse(const uint8_t *p, uint32_t len, uint32_t *ps, uint32_t *pl, uint32_t *rs, uint32_t *rl) {
    int g = 0; uint32_t i = 0;
    for (;;) {
        uint32_t s = i; while (i < len && (uint8_t)(p[i] - '0') >= 10) i++;
        ps[g] = s; pl[g] = i - s;
        if (i == len) return g;
        if (g == LOG_MAXRUN) return -1;
        s = i; while (i < len && (uint8_t)(p[i] - '0') < 10) i++;
        rs[g] = s; rl[g] = i - s; g++;
    }
}

static void put_column(Buf *out, const tok_t *tk, uint32_t m, int64_t *scratch) {
    uint32_t ps0[LOG_MAXRUN + 1], pl0[LOG_MAXRUN + 1], rs0[LOG_MAXRUN], rl0[LOG_MAXRUN];
    uint32_t ps[LOG_MAXRUN + 1], pl[LOG_MAXRUN + 1], rs[LOG_MAXRUN], rl[LOG_MAXRUN];
    int g = skel_parse(tk[0].p, tk[0].len, ps0, pl0, rs0, rl0), ok = g >= 1;
    for (int j = 0; ok && j <= g; j++) ok = pl0[j] <= 255;
    uint8_t fixed[LOG_MAXRUN], lz[LOG_MAXRUN];
    for (int r = 0; ok && r < g; r++) { fixed[r] = 1; lz[r] = 0; ok = rl0[r] <= 18; }
    uint32_t *RS = ok ? malloc(sizeof(uint32_t) * 2 * (size_t)m * (g > 0 ? g : 1)) : NULL;    /* run starts and lengths per value */
    if (ok && !RS) ok = 0;
    for (uint32_t i = 0; ok && i < m; i++) {
        const uint8_t *p = tk[i].p;
        if (skel_parse(p, tk[i].len, ps, pl, rs, rl) != g) { ok = 0; break; }
        for (int r = 0; r < g; r++) { RS[2 * ((size_t)i * g + r)] = rs[r]; RS[2 * ((size_t)i * g + r) + 1] = rl[r]; }
        for (int j = 0; j <= g && ok; j++) ok = pl[j] == pl0[j] && !memcmp(p + ps[j], tk[0].p + ps0[j], pl[j]);
        for (int r = 0; r < g && ok; r++) {
            if (rl[r] > 18) { ok = 0; break; }
            if (rl[r] != rl0[r]) fixed[r] = 0;
            if (rl[r] > 1 && p[rs[r]] == '0') lz[r] = 1;
        }
    }
    for (int r = 0; ok && r < g; r++) if (!fixed[r] && lz[r]) ok = 0;   /* varying width with leading zeros: text */
    if (!ok) {
        free(RS);
        uint8_t t = 0; uint32_t size = 0; for (uint32_t i = 0; i < m; i++) size += tk[i].len + 1;
        bput(out, &t, 1); bput(out, &size, 4);
        for (uint32_t i = 0; i < m; i++) { bput(out, tk[i].p, tk[i].len); uint8_t nl = '\n'; bput(out, &nl, 1); }
        return;
    }
    uint8_t t = 1, gg = (uint8_t)g; bput(out, &t, 1); bput(out, &gg, 1);
    for (int j = 0; j <= g; j++) { uint8_t L = (uint8_t)pl0[j]; bput(out, &L, 1); bput(out, tk[0].p + ps0[j], L); }
    for (int r = 0; r < g; r++) {
        uint8_t pad = fixed[r] ? (uint8_t)rl0[r] : 0; bput(out, &pad, 1);
        for (uint32_t i = 0; i < m; i++) {
            uint32_t a = RS[2 * ((size_t)i * g + r)], b = RS[2 * ((size_t)i * g + r) + 1];
            int64_t v = 0; for (uint32_t q = 0; q < b; q++) v = v * 10 + (tk[i].p[a + q] - '0');
            scratch[i] = v;
        }
        put_ints(out, scratch, m);
    }
    free(RS);
}

typedef struct { uint64_t h; uint32_t koff, klen, cnt, id; } tent_t;

/* JSON lines: the next scalar value (string, number, true/false/null; not an object key) at or after i, as [*vs, *ve).
 * Returns 0 when there is none. Anything that is not well-formed JSON simply stays in the template. */
static int json_next_value(const uint8_t *p, uint32_t i, uint32_t len, uint32_t *vs, uint32_t *ve) {
    while (i < len) {
        uint8_t c = p[i];
        if (c == '"') {
            uint32_t q = i + 1;
            for (;;) {
                const uint8_t *e = memchr(p + q, '"', len - q); if (!e) return 0;
                q = (uint32_t)(e - p); uint32_t b = q; while (b > i + 1 && p[b - 1] == '\\') b--;
                if (((q - b) & 1) == 0) break;                                   /* not escaped */
                q++;
            }
            uint32_t k = q + 1; while (k < len && (p[k] == ' ' || p[k] == '\t')) k++;
            if (k < len && p[k] == ':') { i = k + 1; continue; }                  /* object key: template */
            *vs = i; *ve = q + 1; return 1;
        }
        uint8_t pc = i ? p[i - 1] : ',';
        int after = pc == ':' || pc == ',' || pc == '[' || pc == ' ' || pc == '\t';
        if (after && (c == '-' || (uint8_t)(c - '0') < 10)) {
            uint32_t k = i + 1;
            while (k < len && ((uint8_t)(p[k] - '0') < 10 || p[k] == '.' || p[k] == 'e' || p[k] == 'E' || p[k] == '+' || p[k] == '-')) k++;
            *vs = i; *ve = k; return 1;
        }
        if (after && (c == 't' || c == 'f' || c == 'n')) {
            uint32_t w = c == 'f' ? 5 : 4;
            if (i + w <= len && (!memcmp(p + i, "true", 4) || !memcmp(p + i, "null", 4) || (w == 5 && !memcmp(p + i, "false", 5)))
                && (i + w == len || !(((p[i + w] | 32) >= 'a' && (p[i + w] | 32) <= 'z') || (uint8_t)(p[i + w] - '0') < 10))) {
                *vs = i; *ve = i + w; return 1;
            }
        }
        i++;
    }
    return 0;
}

/* jm = 0: tokens are space-separated words, those with a digit are variable (logs). jm = 1: JSON lines; every scalar
 * value is variable and the keys, punctuation and spacing are the template. Same output format either way. */
static int64_t log_encode_m(const uint8_t *in, size_t n, uint8_t *dst, size_t cap, int jm) {
    if (n < 4096) return -1;
    const uint32_t sep = jm ? 0 : 1;                    /* bytes between tokens */
    size_t nl = n; while (nl > 0 && in[nl - 1] != '\n') nl--;
    if (nl == 0) return -1;
    size_t L = 0; for (const uint8_t *q = in; (q = memchr(q, '\n', nl - (size_t)(q - in))); q++) L++;
    if (L < 64) return -1;
    int64_t ret = -1;
    uint32_t *lstart = malloc(4 * (L + 1)), *tstart = NULL, *lid = malloc(4 * L);
    uint8_t *keys = malloc(nl + L + 16);
    size_t tcap = (L + 1) * 2 + 1; while (tcap & (tcap - 1)) tcap &= tcap - 1; tcap *= 2;
    tent_t *tab = calloc(tcap, sizeof(tent_t)); uint32_t *order = malloc(4 * (L + 1)); uint32_t nt = 0; size_t kp = 0;
    tok_t *tk = NULL, *tkg = NULL; size_t tkg_cap = 0; int64_t *scratch = NULL; uint32_t *rows = NULL, *rstart = NULL, *remap = NULL;
    uint32_t *tokb = malloc(4 * (L + 1)), *toks = malloc(4 * (nl + L + 2));    /* per line: first token; token starts */
    /* constant placeholders are found in this first pass, while each line is in cache: per token, whether it is
     * variable; per template, its first line, the offset of its flags (one per placeholder: identical on every line
     * so far) and how many flags are still set. Same decisions as comparing every row with the first one afterwards. */
    uint8_t *tvar = malloc(nl + L + 2), *tfl = malloc(nl + L + 2); size_t fp = 0;
    uint32_t *tl0 = malloc(4 * (L + 1)), *tfo = malloc(4 * (L + 1)), *tsc = malloc(4 * (L + 1));
    Buf out = {0};
    if (!lstart || !lid || !keys || !tab || !order || !tokb || !toks || !tvar || !tfl || !tl0 || !tfo || !tsc) goto done;
    { size_t i = 0; for (size_t l = 0; l < L; l++) { lstart[l] = (uint32_t)i; const uint8_t *q = memchr(in + i, '\n', nl - i); i = (size_t)(q - in) + 1; } lstart[L] = (uint32_t)nl; }
    if (tokb) tokb[L] = 0;
    /* templates */
    size_t ntok = 0;
    for (size_t l = 0; l < L; l++) {
        const uint8_t *p = in + lstart[l]; uint32_t len = lstart[l + 1] - lstart[l] - 1; size_t k0 = kp;
        uint32_t i = 0; tokb[l] = (uint32_t)ntok;
        if (jm) {                                       /* literal, value, literal, ..., literal */
            int whole = memchr(p, 1, len) != NULL;      /* a 0x01 byte cannot sit in a template: the line is one value */
            for (;;) {
                uint32_t vs = len, ve = len; int hv = whole ? (i == 0) : json_next_value(p, i, len, &vs, &ve);
                if (whole && hv) { vs = 0; ve = len; }
                toks[ntok++] = i; tvar[ntok - 1] = 0; memcpy(keys + kp, p + i, vs - i); kp += vs - i;
                if (!hv) break;
                toks[ntok++] = vs; tvar[ntok - 1] = 1; keys[kp++] = 1; i = ve;
            }
        } else for (;;) {
            uint32_t s = i; toks[ntok++] = s;
            int var = 0;                                /* one pass: the next space, and any digit or 0x01 before it */
            for (;;) {
                if (i + 8 <= len) {
                    uint64_t x; memcpy(&x, p + i, 8);
                    const uint64_t L7 = 0x7F7F7F7F7F7F7F7Full, H = 0x8080808080808080ull, O = 0x0101010101010101ull;
                    uint64_t lo = x & L7, y = x ^ (O * ' '), z = x ^ O;
                    uint64_t sp = ~(((y & L7) + L7) | y | L7);
                    uint64_t vm = (((O * (127 + '9' + 1) - lo) & ~x & (lo + O * (127 - ('0' - 1)))) & H) | ~(((z & L7) + L7) | z | L7);
                    if (sp) { int k = __builtin_ctzll(sp) >> 3; var |= k && (vm << (64 - 8 * k)) != 0; i += (uint32_t)k; break; }
                    var |= vm != 0; i += 8;
                    if (i - s >= 16) {                  /* a long token: the library search is faster from here */
                        const uint8_t *q = memchr(p + i, ' ', len - i); uint32_t e = q ? (uint32_t)(q - p) : len;
                        if (!var) var = is_var_tok(p + i, e - i);
                        i = e; break;
                    }
                } else {
                    while (i < len && p[i] != ' ') { var |= (uint8_t)(p[i] - '0') < 10 || p[i] == 1; i++; }
                    break;
                }
            }
            if (var) { keys[kp++] = 1; tvar[ntok - 1] = 1; }
            else { tvar[ntok - 1] = 0; memcpy(keys + kp, p + s, i - s); kp += i - s; }
            if (i == len) break;
            keys[kp++] = ' '; i++;
        }
        uint32_t klen = (uint32_t)(kp - k0); uint64_t h = czn_xxh3(keys + k0, klen, 0); size_t at = h & (tcap - 1);
        for (;;) {
            tent_t *e = &tab[at];
            if (!e->cnt) {
                e->h = h; e->koff = (uint32_t)k0; e->klen = klen; e->cnt = 1; e->id = nt; order[nt++] = (uint32_t)at; lid[l] = e->id;
                uint32_t id = e->id, c = 0; tl0[id] = (uint32_t)l; tfo[id] = (uint32_t)fp;
                for (size_t j = tokb[l]; j < ntok; j++) {
                    if (!tvar[j]) continue;
                    uint32_t s0 = toks[j], e0 = j + 1 < ntok ? toks[j + 1] - sep : len;
                    uint8_t f = !memchr(p + s0, 1, e0 - s0) && !memchr(p + s0, '\n', e0 - s0);
                    tfl[fp++] = f; c += f;
                }
                tsc[id] = c;
                break;
            }
            if (e->h == h && e->klen == klen && !memcmp(keys + e->koff, keys + k0, klen)) {
                e->cnt++; lid[l] = e->id; kp = k0;
                uint32_t id = e->id;
                if (tsc[id]) {          /* same key: same token count and placeholder positions as the first line */
                    uint32_t l0 = tl0[id], b0 = tokb[l0], n0 = tokb[l0 + 1] - b0, len0 = lstart[l0 + 1] - lstart[l0] - 1;
                    const uint8_t *p0 = in + lstart[l0]; uint8_t *f = tfl + tfo[id]; uint32_t q = 0;
                    for (size_t j = tokb[l]; j < ntok && tsc[id]; j++) {
                        if (!tvar[j]) continue;
                        if (f[q]) {
                            uint32_t k = (uint32_t)(j - tokb[l]);
                            uint32_t s = toks[j], en = j + 1 < ntok ? toks[j + 1] - sep : len;
                            uint32_t s0 = toks[b0 + k], e0 = k + 1 < n0 ? toks[b0 + k + 1] - sep : len0;
                            if (en - s != e0 - s0 || memcmp(p + s, p0 + s0, en - s)) { f[q] = 0; tsc[id]--; }
                        }
                        q++;
                    }
                }
                break;
            }
            at = (at + 1) & (tcap - 1);
        }
    }
    if (nt > L / 4) goto done;
    remap = malloc(4 * (nt + 1)); uint32_t T = 0;
    for (uint32_t t = 0; t < nt; t++) remap[t] = tab[order[t]].cnt >= LOG_MIN ? ++T : 0;
    if (T == 0) goto done;
    for (size_t l = 0; l < L; l++) lid[l] = remap[lid[l]];
    /* lines per kept template, in order */
    rstart = calloc(T + 2, 4); rows = malloc(4 * L);
    for (size_t l = 0; l < L; l++) rstart[lid[l] + 1]++;
    for (uint32_t t = 1; t <= T + 1; t++) rstart[t] += rstart[t - 1];
    { uint32_t *fill = malloc(4 * (T + 2)); memcpy(fill, rstart, 4 * (T + 1)); for (size_t l = 0; l < L; l++) rows[fill[lid[l]]++] = (uint32_t)l; free(fill); }
    /* placeholders whose token is the same on every line of the template are folded into the template text: the
     * decoder then copies them with the literal instead of reading a column (same format, faster decode) */
    tk = malloc(sizeof(tok_t) * (L + 1));
    uint32_t *vpos = malloc(4 * 1024);
    uint8_t *keys2 = malloc((size_t)kp + nl + 16); size_t k2 = 0;
    uint32_t *k2off = malloc(4 * ((size_t)nt + 1)), *k2len = malloc(4 * ((size_t)nt + 1));
    uint8_t *keep = malloc((size_t)nt * 1024 + 1);                    /* per template and placeholder: 1 = column */
    if (!tk || !vpos || !keys2 || !k2off || !k2len || !keep) { free(vpos); free(keys2); free(k2off); free(k2len); free(keep); goto done; }
#define LOG_TOK(i, q) do { uint32_t l_ = rows[rstart[tt] + (i)]; const uint8_t *p_ = in + lstart[l_]; uint32_t len_ = lstart[l_ + 1] - lstart[l_] - 1; \
        uint32_t nt_ = (l_ + 1 < L ? tokb[l_ + 1] : (uint32_t)ntok) - tokb[l_], tix_ = vpos[q] < nt_ ? vpos[q] : nt_ - 1; \
        uint32_t s_ = toks[tokb[l_] + tix_], e_ = tix_ + 1 < nt_ ? toks[tokb[l_] + tix_ + 1] - sep : len_; tk[i].p = p_ + s_; tk[i].len = e_ - s_; } while (0)
    for (uint32_t t = 0; t < nt; t++) {
        if (!remap[t]) continue;
        uint32_t tt = remap[t];
        const uint8_t *k = keys + tab[order[t]].koff; uint32_t kl = tab[order[t]].klen, np = 0, ti = 0;
        if (jm) { for (uint32_t i = 0; i < kl; i++) if (k[i] == 1) { if (np < 1024) vpos[np] = 2 * np + 1; np++; } }
        else for (uint32_t i = 0; i <= kl; i++) {
            if (i == kl || k[i] == ' ') { ti++; continue; }
            if (k[i] == 1) { if (np < 1024) vpos[np] = ti; np++; }
        }
        if (np > 1024) { free(vpos); free(keys2); free(k2off); free(k2len); free(keep); goto done; }
        k2off[t] = (uint32_t)k2; uint32_t q = 0;
        for (uint32_t i = 0; i < kl; i++) {
            if (k[i] != 1) { keys2[k2++] = k[i]; continue; }
            int same = tfl[tfo[t] + q]; LOG_TOK(0, q); tok_t t0 = tk[0];     /* flag from the first pass */
            keep[(size_t)t * 1024 + q] = (uint8_t)!same;
            if (same) { memcpy(keys2 + k2, t0.p, t0.len); k2 += t0.len; } else keys2[k2++] = 1;
            q++;
        }
        k2len[t] = (uint32_t)(k2 - k2off[t]);
    }
    /* header + sections */
    uint32_t rare_len = 0; for (uint32_t i = rstart[0]; i < rstart[1]; i++) { uint32_t l = rows[i]; rare_len += lstart[l + 1] - lstart[l]; }
    uint32_t dict_len = 0; for (uint32_t t = 0; t < nt; t++) if (remap[t]) dict_len += k2len[t] + 1;
    uint8_t ver = 1; uint32_t hd[6] = {(uint32_t)L, T, (uint32_t)(n - nl), dict_len, rare_len, 0};
    bput(&out, &ver, 1); bput(&out, hd, 20);
    scratch = malloc(8 * (L + 1));
    for (size_t l = 0; l < L; l++) scratch[l] = lid[l];
    put_ints(&out, scratch, (uint32_t)L);
    for (uint32_t t = 0; t < nt; t++) if (remap[t]) { bput(&out, keys2 + k2off[t], k2len[t]); uint8_t c = '\n'; bput(&out, &c, 1); }
    for (uint32_t i = rstart[0]; i < rstart[1]; i++) { uint32_t l = rows[i]; bput(&out, in + lstart[l], lstart[l + 1] - lstart[l]); }
    bput(&out, in + nl, n - nl);
    /* variable columns */
    for (uint32_t t = 0; t < nt; t++) {
        if (!remap[t]) continue;
        uint32_t tt = remap[t], m = rstart[tt + 1] - rstart[tt];
        const uint8_t *k = keys + tab[order[t]].koff; uint32_t kl = tab[order[t]].klen, np = 0, ti = 0;
        if (jm) { for (uint32_t i = 0; i < kl; i++) if (k[i] == 1) { if (np < 1024) vpos[np] = 2 * np + 1; np++; } }
        else for (uint32_t i = 0; i <= kl; i++) {
            if (i == kl || k[i] == ' ') { ti++; continue; }
            if (k[i] == 1) { if (np < 1024) vpos[np] = ti; np++; }
        }
        /* the template's kept columns, in placeholder order. Rows are read once per group of columns (row-major, at
         * most 64 MiB of token refs) instead of once per column; the columns and their order are unchanged. */
        uint32_t kq[1024], nk = 0;
        for (uint32_t q = 0; q < np; q++) if (keep[(size_t)t * 1024 + q]) kq[nk++] = q;
        if (!nk) continue;
        uint32_t G = (uint32_t)((64u << 20) / (sizeof(tok_t) * (size_t)m)); if (G < 1) G = 1; if (G > nk) G = nk;
        if ((size_t)G * m > tkg_cap) { free(tkg); tkg_cap = (size_t)G * m; tkg = malloc(sizeof(tok_t) * tkg_cap); if (!tkg) tkg_cap = 0; }
        tok_t *cb = tkg; if (!cb) { cb = tk; G = 1; }                     /* tk holds one column (L + 1 refs) */
        for (uint32_t g0 = 0; g0 < nk; g0 += G) {
            uint32_t gn = nk - g0 < G ? nk - g0 : G;
            for (uint32_t i = 0; i < m; i++) {
                uint32_t l_ = rows[rstart[tt] + i]; const uint8_t *p_ = in + lstart[l_]; uint32_t len_ = lstart[l_ + 1] - lstart[l_] - 1;
                uint32_t b_ = tokb[l_], nt_ = (l_ + 1 < L ? tokb[l_ + 1] : (uint32_t)ntok) - b_;
                for (uint32_t j = 0; j < gn; j++) {
                    uint32_t q = kq[g0 + j], tix_ = vpos[q] < nt_ ? vpos[q] : nt_ - 1;
                    uint32_t s_ = toks[b_ + tix_], e_ = tix_ + 1 < nt_ ? toks[b_ + tix_ + 1] - sep : len_;
                    cb[(size_t)j * m + i].p = p_ + s_; cb[(size_t)j * m + i].len = e_ - s_;
                }
            }
            for (uint32_t j = 0; j < gn; j++) put_column(&out, cb + (size_t)j * m, m, scratch);
        }
    }
#undef LOG_TOK
    free(keys2); free(k2off); free(k2len); free(keep);
    free(vpos);
    if (out.n + 64 > cap || out.n >= n) goto done;
    memcpy(dst, out.p, out.n); ret = (int64_t)out.n;
done:
    free(tokb); free(toks); free(lstart); free(tstart); free(lid); free(keys); free(tab); free(order); free(tk); free(scratch); free(rows); free(rstart); free(remap); free(out.p);
    free(tvar); free(tfl); free(tl0); free(tfo); free(tsc); free(tkg);
    return ret;
}

/* ------------------------------------------------ decoder */
typedef struct { int type, g; const uint8_t *p, *pe; const uint8_t *pc[LOG_MAXRUN + 1]; uint8_t pcl[LOG_MAXRUN + 1], pad[LOG_MAXRUN]; int64_t *v; uint32_t m, i; } lcol_t;
typedef struct { uint32_t nseg; uint32_t s0; } ltpl_t;          /* segments: literal (ptr,len) then var column, alternating */

static int log_decode(const uint8_t *in, size_t tn, uint8_t *out, size_t n) {
    if (tn < 21 || in[0] != 1) return -1;
    uint32_t hd[6]; memcpy(hd, in + 1, 20);
    uint32_t L = hd[0], T = hd[1], tail = hd[2], dict_len = hd[3], rare_len = hd[4];
    if ((uint64_t)L > n || T > L || tail > n) return -1;
    const uint8_t *p = in + 21, *end = in + tn;
    int ret = -1;
    int64_t *lid = malloc(8 * ((size_t)L + 1)), *vals = NULL;
    uint32_t *cnt = calloc((size_t)T + 1, 4), *colbase = NULL, *cur = NULL;
    const uint8_t **lit = NULL; uint32_t *litlen = NULL, *segcol = NULL; ltpl_t *tp = NULL; lcol_t *cols = NULL;
    if (!lid || !cnt) goto done;
    if (get_ints(&p, end, L, lid)) goto done;
    for (uint32_t l = 0; l < L; l++) { if ((uint64_t)lid[l] > T) goto done; cnt[lid[l]]++; }
    if ((uint64_t)(end - p) < (uint64_t)dict_len + rare_len + tail) goto done;
    const uint8_t *dict = p, *rare = p + dict_len, *rare_e = rare + rare_len, *tailp = rare_e; p = tailp + tail;
    /* templates: literal segments and placeholders */
    tp = calloc((size_t)T + 1, sizeof(ltpl_t));
    size_t maxseg = (size_t)dict_len + 2 * (size_t)T + 2;
    lit = malloc(sizeof(uint8_t *) * maxseg); litlen = malloc(4 * maxseg); segcol = malloc(4 * maxseg);
    colbase = malloc(4 * ((size_t)T + 2));
    if (!tp || !lit || !litlen || !segcol || !colbase) goto done;
    uint32_t ns = 0, ncols = 0; const uint8_t *d = dict, *de = dict + dict_len;
    for (uint32_t t = 1; t <= T; t++) {
        const uint8_t *e = memchr(d, '\n', (size_t)(de - d)); if (!e) goto done;
        tp[t].s0 = ns; colbase[t] = ncols;
        const uint8_t *s = d;
        for (const uint8_t *q = d; q <= e; q++) {
            if (q == e || *q == 1) {
                lit[ns] = s; litlen[ns] = (uint32_t)(q - s); segcol[ns] = q == e ? UINT32_MAX : ncols++; ns++;
                s = q + 1;
            }
        }
        tp[t].nseg = ns - tp[t].s0; d = e + 1;
    }
    if (d != de) goto done;
    colbase[T + 1] = ncols;
    /* columns */
    cols = calloc((size_t)ncols + 1, sizeof(lcol_t));
    size_t totv = 0;
    { uint32_t c = 0;
      for (uint32_t t = 1; t <= T; t++) for (uint32_t q = colbase[t]; q < colbase[t + 1]; q++, c++) {
          lcol_t *C = &cols[c]; C->m = cnt[t];
          if (p >= end) goto done;
          C->type = *p++;
          if (C->type == 0) {
              if (end - p < 4) goto done; uint32_t size; memcpy(&size, p, 4); p += 4;
              if (size > (size_t)(end - p)) goto done;
              C->p = p; C->pe = p + size; p += size;
          } else if (C->type == 1) {
              if (p >= end) goto done; C->g = *p++;
              if (C->g < 1 || C->g > LOG_MAXRUN) goto done;
              for (int j = 0; j <= C->g; j++) { if (p >= end) goto done; C->pcl[j] = *p++; if (C->pcl[j] > (size_t)(end - p)) goto done; C->pc[j] = p; p += C->pcl[j]; }
              totv += (size_t)C->g * C->m;
              C->p = p;                                   /* runs decoded below */
              for (int r = 0; r < C->g; r++) {
                  if (p >= end) goto done; C->pad[r] = *p++; if (C->pad[r] > 18) goto done;
                  if (end - p < 2) goto done; int w = p[1]; if (w < 1 || w > 8 || p[0] > 1 || (uint64_t)(end - p - 2) < (uint64_t)C->m * w) goto done;
                  p += 2 + (size_t)C->m * w;
              }
          } else goto done;
      } }
    if (p != end) goto done;
    vals = malloc(8 * (totv + 1)); if (!vals) goto done;
    { int64_t *vp = vals;
      for (uint32_t c = 0; c < ncols; c++) {
          lcol_t *C = &cols[c]; if (C->type != 1) continue;
          const uint8_t *q = C->p; C->v = vp;
          for (int r = 0; r < C->g; r++) {
              q++;                                                     /* pad (read above) */
              if (get_ints(&q, end, C->m, vp + (size_t)r * C->m)) goto done;
              {   /* 0 <= v < 10^18, and < 10^pad for fixed widths: one unsigned max (vectorizes) */
                  uint64_t mx = 0; const uint64_t *u = (const uint64_t *)(vp + (size_t)r * C->m);
                  for (uint32_t i = 0; i < C->m; i++) mx = u[i] > mx ? u[i] : mx;
                  uint64_t lim = (C->pad[r] && C->pad[r] < 18) ? (uint64_t)P10[C->pad[r]] : (uint64_t)P10[18];
                  if (mx >= lim) goto done;
              }
          }
          vp += (size_t)C->g * C->m;
      } }
    /* emit lines */
    { uint8_t *o = out, *oe = out + n; const uint8_t *rp = rare;
      for (uint32_t l = 0; l < L; l++) {
          uint32_t t = (uint32_t)lid[l];
          if (!t) {
              const uint8_t *e = memchr(rp, '\n', (size_t)(rare_e - rp)); if (!e) goto done;
              size_t len = (size_t)(e - rp) + 1; if (len > (size_t)(oe - o)) goto done;
              memcpy(o, rp, len); o += len; rp += len; continue;
          }
          const ltpl_t *TP = &tp[t];
          for (uint32_t s = TP->s0; s < TP->s0 + TP->nseg; s++) {
              uint32_t ll = litlen[s];
              if (ll <= 16 && oe - o >= 64 && end - lit[s] >= 16) memcpy(o, lit[s], 16);     /* short copies overrun into room */
              else { if (ll > (size_t)(oe - o)) goto done; memcpy(o, lit[s], ll); }
              o += ll;
              if (segcol[s] == UINT32_MAX) break;
              lcol_t *C = &cols[segcol[s]]; uint32_t i = C->i++;
              if (i >= C->m) goto done;
              if (C->type == 0) {
                  const uint8_t *e = NULL;
                  if (C->pe - C->p >= 16) {                                   /* newline within 16 bytes: SWAR, else memchr */
                      uint64_t w0, w1; memcpy(&w0, C->p, 8); memcpy(&w1, C->p + 8, 8);
                      uint64_t x0 = w0 ^ (ONES * '\n'), x1 = w1 ^ (ONES * '\n');
                      uint64_t z0 = ~(((x0 & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | x0 | 0x7F7F7F7F7F7F7F7Full);
                      uint64_t z1 = ~(((x1 & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | x1 | 0x7F7F7F7F7F7F7F7Full);
                      if (z0) e = C->p + (__builtin_ctzll(z0) >> 3); else if (z1) e = C->p + 8 + (__builtin_ctzll(z1) >> 3);
                      if (e && oe - o >= 64) { memcpy(o, C->p, 16); o += e - C->p; C->p = e + 1; continue; }
                  }
                  if (!e) e = memchr(C->p, '\n', (size_t)(C->pe - C->p));
                  if (!e) goto done;
                  size_t len = (size_t)(e - C->p); if (len > (size_t)(oe - o)) goto done;
                  memcpy(o, C->p, len); o += len; C->p = e + 1;
              } else {
                  if (oe - o < 24 * (C->g + 1) + 256 * (C->g + 1)) {      /* near the end: careful path */
                      for (int j = 0; j <= C->g; j++) {
                          if (C->pcl[j] > (size_t)(oe - o)) goto done; memcpy(o, C->pc[j], C->pcl[j]); o += C->pcl[j];
                          if (j == C->g) break;
                          uint8_t tb[40]; int64_t v = C->v[(size_t)j * C->m + i]; uint8_t *te = fmt_dec_fast(tb, v, 0); size_t dl = (size_t)(te - tb);
                          size_t pad = C->pad[j] > dl ? C->pad[j] - dl : 0;
                          if (pad + dl > (size_t)(oe - o)) goto done;
                          memset(o, '0', pad); o += pad; memcpy(o, tb, dl); o += dl;
                      }
                  } else {
                      for (int j = 0; j <= C->g; j++) {
                          uint32_t pl = C->pcl[j];
                          if (pl <= 16 && end - C->pc[j] >= 16) memcpy(o, C->pc[j], 16); else memcpy(o, C->pc[j], pl);
                          o += pl;
                          if (j == C->g) break;
                          uint64_t v = (uint64_t)C->v[(size_t)j * C->m + i]; int pad = C->pad[j];   /* 0 <= v < 10^18, v < 10^pad */
                          if (pad && pad <= 8) { st8(o, swar8(v) >> (8 * (8 - pad))); o += pad; }  /* fixed width, leading zeros */
                          else if (pad) { st8(o, swar8(v / 100000000) >> (8 * (16 - pad))); st8(o + pad - 8, swar8(v % 100000000)); o += pad; }
                          else if (v < 100000000) { int D = ndigits(v); st8(o, swar8(v) >> (8 * (8 - D))); o += D; }
                          else o = fmt_dec_swar(o, (int64_t)v, 0);
                      }
                  }
              }
          }
          if (o >= oe) goto done;
          *o++ = '\n';
      }
      if (rp != rare_e) goto done;
      for (uint32_t c = 0; c < ncols; c++) if (cols[c].i != cols[c].m || (cols[c].type == 0 && cols[c].p != cols[c].pe)) goto done;
      if (tail != (size_t)(oe - o)) goto done;
      memcpy(o, tailp, tail);
    }
    ret = 0;
done:
    free(lid); free(vals); free(cnt); free(colbase); free(cur); free(lit); free(litlen); free(segcol); free(tp); free(cols);
    return ret;
}

static int64_t log_encode(const uint8_t *in, size_t n, uint8_t *dst, size_t cap) { return log_encode_m(in, n, dst, cap, 0); }
int64_t log_encode_pub(const uint8_t *in, size_t n, uint8_t *dst, size_t cap) { return log_encode(in, n, dst, cap); }
int64_t log_encode_json_pub(const uint8_t *in, size_t n, uint8_t *dst, size_t cap) { return log_encode_m(in, n, dst, cap, 1); }
int log_decode_pub(const uint8_t *in, size_t tn, uint8_t *out, size_t n) { return log_decode(in, tn, out, n); }
