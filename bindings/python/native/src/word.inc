/* ---------------------------------------------------------------- word dictionary transform for text (block flag 9)
 * Frequent words (maximal runs of 3+ ASCII letters; "The" counts as "the" plus a capital marker) are replaced by
 * two-byte codes whose first byte is a value the block never contains, so no escaping is needed. The dictionary
 * travels in the block. Helps LZ coders on prose and markup: enwik8 + zstd -19 3.71x -> 3.93x.
 * Layout: np u8 | prefixes np bytes | cap u8 | nw u32 | words, each followed by '\n' | transformed text */
#define WD_MAXW 48
#define WD_MAXCODES (64 * 256)

static inline int is_alpha(uint8_t c) { return (uint8_t)((c | 32) - 'a') < 26; }
static inline int cap_norm(const uint8_t *w, uint32_t n) {          /* Upper followed by lower case only */
    if (n < 2 || (uint8_t)(w[0] - 'A') >= 26) return 0;
    for (uint32_t i = 1; i < n; i++) if ((uint8_t)(w[i] - 'a') >= 26) return 0;
    return 1;
}
typedef struct { uint64_t h; uint32_t off, len, cnt, code; } went_t;
static inline uint64_t wd_hash(const uint8_t *w, uint32_t L, int cn) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ L; uint32_t k = 0;
    while (k + 8 <= L) { uint64_t v; memcpy(&v, w + k, 8); if (!k && cn) v |= 32; h = (h ^ v) * 0xFF51AFD7ED558CCDull; h ^= h >> 29; k += 8; }
    if (k < L) { uint64_t v = 0; memcpy(&v, w + k, L - k); if (!k && cn) v |= 32; h = (h ^ v) * 0xC4CEB9FE1A85EC53ull; h ^= h >> 31; }
    return h;
}

static int64_t word_encode(const uint8_t *in, size_t n, uint8_t *dst, size_t cap) {
    if (n < 65536) return -1;
    size_t used[256] = {0};
    for (size_t a = 0; a < n; a += (size_t)1 << 30) {      /* (32-bit counts per GiB) */
        uint32_t hc[256]; size_t m = n - a < ((size_t)1 << 30) ? n - a : (size_t)1 << 30; kg_hist(in + a, m, hc);
        for (int c = 0; c < 256; c++) used[c] += hc[c];
    }
    uint8_t pre[64]; int np = 0, capb = -1;
    for (int pass = 0; pass < 2 && np < 8; pass++) {     /* control bytes first: the decoder finds them with SIMD */
        np = 0; capb = -1;
        for (int b = 1; b < (pass ? 256 : 32) && np < 64; b++) {
            if (used[b] || is_alpha((uint8_t)b)) continue;
            if (capb < 0) capb = b; else pre[np++] = (uint8_t)b;
        }
    }
    if (capb < 0 || np < 1) return -1;
    /* count words (lower-cased when capitalised) */
    size_t tcap = 1 << 20; while (tcap < (n < (4u << 20) ? n : (4u << 20)) / 4) tcap <<= 1;
    went_t *tab = calloc(tcap, sizeof(went_t)); uint8_t *lw = malloc(WD_MAXW + 1); size_t nent = 0;
    if (!tab || !lw) { free(tab); free(lw); return -1; }
    int64_t ret = -1; uint32_t *top = NULL;
    size_t SL = n <= (4u << 20) ? n : (1u << 20), NS = n <= (4u << 20) ? 1 : 4;     /* count on a sample */
    for (size_t sp = 0; sp < NS; sp++) {
    size_t i = NS == 1 ? 0 : (n - SL) / 3 * sp, iend = i + SL;
    while (i > 0 && i < n && is_alpha(in[i])) i++;                                 /* start at a word boundary */
    while (i < iend) {
        if (!is_alpha(in[i])) { i++; continue; }
        size_t s = i; while (i < n && is_alpha(in[i])) i++;
        uint32_t L = (uint32_t)(i - s); if (L < 3 || L > WD_MAXW) continue;
        const uint8_t *w = in + s;
        int cn = cap_norm(w, L); uint64_t h = wd_hash(w, L, cn);
        size_t at = h & (tcap - 1);
        for (;;) {
            went_t *e = &tab[at];
            if (!e->cnt) { if (nent * 2 > tcap) break; e->h = h; e->off = (uint32_t)s; e->len = L; e->cnt = 1; nent++; break; }
            if (e->h == h && e->len == L) { e->cnt++; break; }
            at = (at + 1) & (tcap - 1);
        }
    }
    }
    /* pick the most valuable words: count * (len - 2) */
    size_t ncodes = (size_t)np * 256; if (ncodes > WD_MAXCODES) ncodes = WD_MAXCODES;
    top = malloc(sizeof(uint32_t) * (nent + 1)); size_t nt = 0;
    for (size_t i = 0; i < tcap; i++) if (tab[i].cnt >= 3) top[nt++] = (uint32_t)i;
    /* partial selection by score via sort (nt is at most a few hundred thousand) */
    for (size_t i = 0; i < nt; i++) { went_t *e = &tab[top[i]]; e->code = (uint32_t)(e->cnt * (uint64_t)(e->len - 2) > 0xFFFFFFFFull ? 0xFFFFFFFFu : e->cnt * (e->len - 2)); }
    {   /* sort indices by score descending (simple radix on 32-bit score) */
        uint32_t *tmp = malloc(sizeof(uint32_t) * (nt + 1));
        for (int sh = 0; sh < 32; sh += 8) {
            size_t c[257] = {0};
            for (size_t i = 0; i < nt; i++) c[255 - ((tab[top[i]].code >> sh) & 255) + 1]++;
            for (int b = 0; b < 256; b++) c[b + 1] += c[b];
            for (size_t i = 0; i < nt; i++) tmp[c[255 - ((tab[top[i]].code >> sh) & 255)]++] = top[i];
            uint32_t *x = top; top = tmp; tmp = x;
        }
        free(tmp);
    }
    if (nt > ncodes) nt = ncodes;
    if (nt < 4) goto done;
    for (size_t i = 0; i < tcap; i++) tab[i].code = UINT32_MAX;
    for (size_t i = 0; i < nt; i++) tab[top[i]].code = (uint32_t)i;
    {
        uint8_t *o = dst, *oe = dst + cap;
        if (cap < 8 + (size_t)np) goto done;
        *o++ = (uint8_t)np; memcpy(o, pre, np); o += np; *o++ = (uint8_t)capb; uint32_t nw = (uint32_t)nt; memcpy(o, &nw, 4); o += 4;
        for (size_t i = 0; i < nt; i++) {
            went_t *e = &tab[top[i]];
            if ((size_t)(oe - o) < e->len + 1) goto done;
            const uint8_t *w = in + e->off; int cn = cap_norm(w, e->len);
            for (uint32_t k = 0; k < e->len; k++) o[k] = (k == 0 && cn) ? (uint8_t)(w[0] | 32) : w[k];
            o += e->len; *o++ = '\n';
        }
        for (size_t i = 0; i < n;) {
            if (!is_alpha(in[i])) {
                size_t s = i; while (i < n && !is_alpha(in[i])) i++;
                if ((size_t)(oe - o) < i - s) goto done;
                memcpy(o, in + s, i - s); o += i - s; continue;
            }
            size_t s = i; while (i < n && is_alpha(in[i])) i++;
            uint32_t L = (uint32_t)(i - s); const uint8_t *w = in + s; uint32_t code = UINT32_MAX; int cn = 0;
            if (L >= 3 && L <= WD_MAXW) {
                cn = cap_norm(w, L); uint64_t h = wd_hash(w, L, cn);
                size_t at = h & (tcap - 1);
                while (tab[at].cnt) {
                    if (tab[at].h == h && tab[at].len == L) { code = tab[at].code; break; }
                    at = (at + 1) & (tcap - 1);
                }
                if (code != UINT32_MAX) {   /* confirm bytes (hash collision safety) */
                    const uint8_t *r = in + tab[at].off; int rc = cap_norm(r, L);
                    for (uint32_t k = 0; k < L && code != UINT32_MAX; k++) {
                        uint8_t a = (k == 0 && cn) ? (uint8_t)(w[0] | 32) : w[k], b = (k == 0 && rc) ? (uint8_t)(r[0] | 32) : r[k];
                        if (a != b) code = UINT32_MAX;
                    }
                }
            }
            if (code == UINT32_MAX) { if ((size_t)(oe - o) < L) goto done; memcpy(o, w, L); o += L; }
            else { if ((size_t)(oe - o) < 3) goto done; if (cn) *o++ = (uint8_t)capb; *o++ = pre[code >> 8]; *o++ = (uint8_t)(code & 255); }
        }
        if ((size_t)(o - dst) >= n) goto done;
        ret = (int64_t)(o - dst);
    }
done:
    free(tab); free(lw); free(top);
    return ret;
}

/* skip bytes that are not special when every special byte is < 32: 16 bytes per step, candidates checked in the table */
static inline const uint8_t *wd_scan_low(const uint8_t *p, const uint8_t *end, const uint8_t *special) {
#if defined(__SSE2__)
    const __m128i lim = _mm_set1_epi8(31);
    while (p + 16 <= end) {
        __m128i v = _mm_loadu_si128((const __m128i *)p);
        unsigned m = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_min_epu8(v, lim), v));
        while (m) { int t = __builtin_ctz(m); if (special[p[t]]) return p + t; m &= m - 1; }
        p += 16;
    }
#elif defined(__ARM_NEON)
    const uint8x16_t lim = vdupq_n_u8(31);
    while (p + 16 <= end) {
        uint8x16_t c = vcleq_u8(vld1q_u8(p), lim);
        uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(c), 4)), 0);
        while (m) { int t = __builtin_ctzll(m) >> 2; if (special[p[t]]) return p + t; m &= ~(0xFull << (4 * t)); }
        p += 16;
    }
#endif
    return p;
}

static int word_decode(const uint8_t *in, size_t tn, uint8_t *out, size_t n) {
    if (tn < 6) return -1;
    int np = in[0]; if (np < 1 || np > 64 || tn < (size_t)np + 6) return -1;
    const uint8_t *p = in + 1; int16_t pidx[256]; for (int b = 0; b < 256; b++) pidx[b] = -1;
    for (int k = 0; k < np; k++) { if (pidx[p[k]] >= 0) return -1; pidx[p[k]] = (int16_t)k; }
    p += np; uint8_t capb = *p++; if (pidx[capb] >= 0) return -1;
    uint32_t nw; memcpy(&nw, p, 4); p += 4;
    if (nw > (uint32_t)np * 256 || nw > WD_MAXCODES) return -1;
    const uint8_t *end = in + tn;
    uint8_t *arena = malloc((size_t)nw * 64 + 64), *wl = malloc(nw + 1);         /* words padded to 64-byte slots */
    if (!arena || !wl) { free(arena); free(wl); return -1; }
    int ret = -1;
    for (uint32_t i = 0; i < nw; i++) {
        const uint8_t *e = memchr(p, '\n', (size_t)(end - p)); if (!e || e - p < 3 || e - p > WD_MAXW) goto out;
        wl[i] = (uint8_t)(e - p); memcpy(arena + (size_t)i * 64, p, (size_t)(e - p)); p = e + 1;
    }
    uint8_t special[256] = {0}; special[capb] = 1; for (int k = 0; k < np; k++) special[in[1 + k]] = 2;
    int lowset = capb < 32; for (int k = 0; k < np; k++) lowset &= in[1 + k] < 32;
    uint8_t *o = out, *oe = out + n;
    while (p < end) {
        const uint8_t *s = p;
        if (lowset) p = wd_scan_low(p, end, special);
        else while (p + 8 <= end && !(special[p[0]] | special[p[1]] | special[p[2]] | special[p[3]] | special[p[4]] | special[p[5]] | special[p[6]] | special[p[7]])) p += 8;
        while (p < end && !special[*p]) p++;
        size_t L = (size_t)(p - s); if (L > (size_t)(oe - o)) goto out;
        if (L <= 16 && o + 16 <= oe && s + 16 <= end) memcpy(o, s, 16); else memcpy(o, s, L);
        o += L;
        if (p >= end) break;
        int capf = 0;
        if (*p == capb) { capf = 1; p++; if (p >= end || special[*p] != 2) goto out; }
        if (end - p < 2) goto out;
        uint32_t code = (uint32_t)pidx[p[0]] << 8 | p[1]; p += 2;
        if (code >= nw) goto out;
        uint32_t wlen = wl[code]; const uint8_t *src = arena + (size_t)code * 64;
        if (o + 64 <= oe) { memcpy(o, src, 16); if (wlen > 16) memcpy(o + 16, src + 16, 32); }
        else { if (wlen > (size_t)(oe - o)) goto out; memcpy(o, src, wlen); }
        if (capf) o[0] = (uint8_t)(o[0] & ~32);
        o += wlen;
    }
    if (o == oe) ret = 0;
out:
    free(arena); free(wl);
    return ret;
}

int64_t word_encode_pub(const uint8_t *in, size_t n, uint8_t *dst, size_t cap) { return word_encode(in, n, dst, cap); }
int word_decode_pub(const uint8_t *in, size_t tn, uint8_t *out, size_t n) { return word_decode(in, tn, out, n); }
