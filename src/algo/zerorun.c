/* Zero-run split: a byte stream as its non-zero bytes and the lengths of the zero runs before each of them.
 *
 * Algorithm only; it knows nothing of the container. Used by the zero-run leaf (codec 23, src/codecs/zrun_leaf.c).
 *
 * runs: for each non-zero byte, in order, the number of zero bytes before it (since the previous non-zero byte or
 * the start) as an unsigned LEB128 varint; then the number of zero bytes after the last non-zero byte, also a varint.
 * lits: the non-zero bytes, in order. A stream of n bytes with k non-zero bytes has k + 1 run varints and k literals.
 *
 * zr_split writes both and returns 0, or -1 when a buffer is too small (runs needs at most n + 10 bytes in the worst
 * case of k = n, lits at most n). zr_join rebuilds exactly n bytes and returns 0, or -1 when the streams are not a
 * valid split of n bytes (a varint that overflows, a run past the end, a zero literal, bytes left over). */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static size_t zr_count_nonzero(const uint8_t *p, size_t n) {
    size_t k = 0;
    for (size_t i = 0; i < n; i++) k += p[i] != 0;
    return k;
}

static inline uint8_t *zr_putv(uint8_t *q, uint64_t v) {
    while (v >= 128) { *q++ = (uint8_t)(v | 128); v >>= 7; }
    *q++ = (uint8_t)v; return q;
}

static int zr_split(const uint8_t *p, size_t n, uint8_t *runs, size_t runcap, size_t *runlen, uint8_t *lits, size_t litcap, size_t *litlen) {
    uint8_t *r = runs, *re = runs + runcap, *l = lits; size_t z = 0;
    for (size_t i = 0; i < n; i++) {
        if (!p[i]) { z++; continue; }
        if ((size_t)(re - r) < 10 || (size_t)(l - lits) >= litcap) return -1;
        r = zr_putv(r, z); *l++ = p[i]; z = 0;
    }
    if ((size_t)(re - r) < 10) return -1;
    r = zr_putv(r, z);
    *runlen = (size_t)(r - runs); *litlen = (size_t)(l - lits); return 0;
}

static int zr_join(const uint8_t *runs, size_t rn, const uint8_t *lits, size_t ln, uint8_t *out, size_t n) {
    size_t i = 0, o = 0;
    for (size_t k = 0; k <= ln; k++) {
        uint64_t v = 0; int s = 0;
        for (;;) {
            if (i >= rn || s > 63) return -1;
            uint8_t c = runs[i++]; v |= (uint64_t)(c & 127) << s;
            if (!(c & 128)) break;
            s += 7;
        }
        if (v > n - o) return -1;
        memset(out + o, 0, (size_t)v); o += (size_t)v;
        if (k == ln) break;
        if (o >= n || !lits[k]) return -1;
        out[o++] = lits[k];
    }
    return i == rn && o == n ? 0 : -1;
}
