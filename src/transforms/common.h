/* Definitions shared by the structure transforms: transform ids, room a transform may need past the block, and
 * the byte scanners. */
#define TSLACK 16384            /* table transform may exceed the block by this much (headers, exceptions) */
/* table transform v2 (flag 4), container version 11: an eighth of the block more. Its typed columns carry a kind and a
 * scale byte per value and a length byte per text field, so a table of short numbers grows by a few percent before coding
 * (5.5 MB of daily counts: +4.4%) and a fixed 16 KiB slack turned the transform off on every block of 400 KiB or more. */
#define TBL_SLACK(n) ((size_t)(n) / 8 + TSLACK)
#define ONES 0x0101010101010101ull
#define HIGHS 0x8080808080808080ull
/* first index of byte a or b in p[0..lim), or lim (SWAR, 8 bytes per step; lowest flagged byte is always exact) */
static inline size_t scan2(const uint8_t *p, size_t lim, uint8_t a, uint8_t b) {
    uint64_t ma = ONES * a, mb = ONES * b; size_t i = 0;
    for (; i + 8 <= lim; i += 8) {
        uint64_t v; memcpy(&v, p + i, 8); uint64_t x = v ^ ma, y = v ^ mb;
        uint64_t t = (((x - ONES) & ~x) | ((y - ONES) & ~y)) & HIGHS;
        if (t) return i + (__builtin_ctzll(t) >> 3);
    }
    for (; i < lim; i++) if (p[i] == a || p[i] == b) return i;
    return lim;
}
static inline size_t scan1(const uint8_t *p, size_t lim, uint8_t a) { const uint8_t *q = memchr(p, a, lim); return q ? (size_t)(q - p) : lim; }
static int has_quotes(const uint8_t *p, size_t n) { return memchr(p, '"', n) || memchr(p, '[', n) || memchr(p, ']', n); }

enum { T_NONE = 0, T_TP = 1, T_SHUF = 2, T_TBL = 3, T_LOG = 4, T_IMG = 5, T_REC = 6, T_X86 = 7, T_WORD = 8, T_KG = 9, T_LINES = 10 };   /* 9: kolmo graph (a=delim b=decimal separator) */
