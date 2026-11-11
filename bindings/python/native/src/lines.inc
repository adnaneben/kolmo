/* ---------------------------------------------------------------- line-class transform (block flag 12, format version 8)
 * Text whose lines come in a few fixed lengths (fixed-width records: chemistry and geometry files, reports, dumps).
 * Lines of the same length are a table of characters: stored column by column, a column of spaces, signs or decimal
 * points is one long run and a digit column holds digits only. It costs a line scan and a transpose (no tokens, no
 * templates), which is several times cheaper than the template transform on the same data, to encode and to decode.
 * Layout: ver u8 (1) | K u8 | L u32 | tail u32 | other u32 | K x (len u32, count u32)
 *         | class id per line (L bytes: 0 = other, 1..K) | other lines, each with its '\n' | tail (after the last '\n')
 *         | per class: len columns of count bytes */
#define LINES_K 8             /* classes */
#define LINES_MIN 64          /* lines a length needs to be a class */
#define LINES_MAXLEN 4096     /* longest class line */
#define LINES_CHUNK 256       /* rows transposed at a time (the rows stay in cache) */

static int64_t lines_encode(const uint8_t *in, size_t n, uint8_t *dst, size_t cap) {
    if (n < 4096 || n > 0x7FFFFFFFu) return -1;
    size_t nl = n; while (nl > 0 && in[nl - 1] != '\n') nl--;
    if (!nl) return -1;
    size_t L = 0; for (const uint8_t *q = in; (q = memchr(q, '\n', nl - (size_t)(q - in))); q++) L++;
    if (L < LINES_MIN) return -1;
    int64_t ret = -1;
    uint32_t *lstart = malloc(4 * (L + 1)), *cnt = calloc(LINES_MAXLEN + 1, 4), *rows = NULL;
    uint8_t *cls = malloc(L);
    if (!lstart || !cnt || !cls) goto done;
    { size_t i = 0; for (size_t l = 0; l < L; l++) { lstart[l] = (uint32_t)i; const uint8_t *q = memchr(in + i, '\n', nl - i); i = (size_t)(q - in) + 1; } lstart[L] = (uint32_t)nl; }
    for (size_t l = 0; l < L; l++) { uint32_t len = lstart[l + 1] - lstart[l] - 1; if (len && len <= LINES_MAXLEN) cnt[len]++; }
    uint32_t klen[LINES_K], kcnt[LINES_K]; int K = 0;
    for (;;) {                                           /* the K lengths that cover the most bytes */
        uint32_t bl = 0; uint64_t bb = 0;
        for (uint32_t len = 1; len <= LINES_MAXLEN; len++) if (cnt[len] >= LINES_MIN && (uint64_t)cnt[len] * len > bb) { bb = (uint64_t)cnt[len] * len; bl = len; }
        if (!bl || K == LINES_K) break;
        klen[K] = bl; kcnt[K] = cnt[bl]; cnt[bl] = 0; K++;
    }
    if (!K) goto done;
    uint64_t covered = 0; for (int k = 0; k < K; k++) covered += (uint64_t)klen[k] * kcnt[k];
    if (covered * 4 < nl) goto done;                     /* under a quarter of the text is in classes: not this kind of file */
    memset(cnt, 0, 4 * (LINES_MAXLEN + 1));
    for (int k = 0; k < K; k++) cnt[klen[k]] = (uint32_t)k + 1;      /* length -> class id */
    size_t other = 0;
    for (size_t l = 0; l < L; l++) {
        uint32_t len = lstart[l + 1] - lstart[l] - 1; uint8_t c = len && len <= LINES_MAXLEN ? (uint8_t)cnt[len] : 0;
        cls[l] = c; if (!c) other += len + 1;
    }
    size_t hdr = 14 + 8 * (size_t)K, total = hdr + L + other + (n - nl) + (size_t)covered;
    if (total + 64 > cap) goto done;
    uint8_t *o = dst;
    o[0] = 1; o[1] = (uint8_t)K; put32(o + 2, (uint32_t)L); put32(o + 6, (uint32_t)(n - nl)); put32(o + 10, (uint32_t)other); o += 14;
    for (int k = 0; k < K; k++) { put32(o, klen[k]); put32(o + 4, kcnt[k]); o += 8; }
    memcpy(o, cls, L); o += L;
    for (size_t l = 0; l < L; l++) if (!cls[l]) { uint32_t len = lstart[l + 1] - lstart[l]; memcpy(o, in + lstart[l], len); o += len; }
    memcpy(o, in + nl, n - nl); o += n - nl;
    rows = malloc(4 * (L + 1)); if (!rows) goto done;
    for (int k = 0; k < K; k++) {
        uint32_t len = klen[k], m = 0;
        for (size_t l = 0; l < L; l++) if (cls[l] == k + 1) rows[m++] = lstart[l];
        for (uint32_t r0 = 0; r0 < m; r0 += LINES_CHUNK) {
            uint32_t rn = m - r0 < LINES_CHUNK ? m - r0 : LINES_CHUNK;
            for (uint32_t j = 0; j < len; j++) {
                uint8_t *col = o + (size_t)j * m + r0;
                for (uint32_t r = 0; r < rn; r++) col[r] = in[rows[r0 + r] + j];
            }
        }
        o += (size_t)len * m;
    }
    ret = (int64_t)(o - dst);
done:
    free(lstart); free(cnt); free(cls); free(rows);
    return ret;
}

static int lines_decode(const uint8_t *t, size_t tn, uint8_t *out, size_t n) {
    if (tn < 14 || t[0] != 1) return -1;
    int K = t[1]; if (K < 1 || K > LINES_K) return -1;
    uint64_t L = get32(t + 2), tail = get32(t + 6), other = get32(t + 10);
    size_t hdr = 14 + 8 * (size_t)K; if (tn < hdr) return -1;
    uint32_t klen[LINES_K], kcnt[LINES_K]; uint64_t covered = 0, lines = 0;
    for (int k = 0; k < K; k++) {
        klen[k] = get32(t + 14 + 8 * k); kcnt[k] = get32(t + 18 + 8 * k);
        if (!klen[k] || klen[k] > LINES_MAXLEN) return -1;
        covered += (uint64_t)klen[k] * kcnt[k]; lines += kcnt[k];
        if (covered > n) return -1;
    }
    if (L > n || lines > L || hdr + L + other + tail + covered != tn || covered + lines + other + tail != n) return -1;
    const uint8_t *cls = t + hdr, *op = cls + L, *oe = op + other, *tp = oe, *cp = tp + tail;
    /* each class back to rows, then the lines in order */
    uint8_t *rowb = malloc((size_t)covered + 1); if (!rowb) return -1;
    const uint8_t *row[LINES_K]; uint32_t left[LINES_K];
    { uint8_t *rb = rowb;
      for (int k = 0; k < K; k++) {
          uint32_t len = klen[k], m = kcnt[k]; row[k] = rb; left[k] = m;
          for (uint32_t r0 = 0; r0 < m; r0 += LINES_CHUNK) {
              uint32_t rn = m - r0 < LINES_CHUNK ? m - r0 : LINES_CHUNK;
              for (uint32_t j = 0; j < len; j++) {
                  const uint8_t *col = cp + (size_t)j * m + r0; uint8_t *w = rb + (size_t)r0 * len + j;
                  for (uint32_t r = 0; r < rn; r++) w[(size_t)r * len] = col[r];
              }
          }
          rb += (size_t)len * m; cp += (size_t)len * m;
      } }
    int ret = -1; uint8_t *o = out, *oend = out + n;
    for (uint64_t l = 0; l < L; l++) {
        unsigned c = cls[l];
        if (c) {
            if (c > (unsigned)K || !left[c - 1]) goto done;
            uint32_t len = klen[c - 1];
            if ((size_t)(oend - o) < (size_t)len + 1) goto done;
            memcpy(o, row[c - 1], len); o[len] = '\n'; o += len + 1; row[c - 1] += len; left[c - 1]--;
        } else {
            const uint8_t *q = memchr(op, '\n', (size_t)(oe - op)); if (!q) goto done;
            size_t len = (size_t)(q - op) + 1;
            if ((size_t)(oend - o) < len) goto done;
            memcpy(o, op, len); o += len; op += len;
        }
    }
    for (int k = 0; k < K; k++) if (left[k]) goto done;
    if (op != oe || (size_t)(oend - o) != tail) goto done;
    memcpy(o, tp, tail);
    ret = 0;
done:
    free(rowb);
    return ret;
}
