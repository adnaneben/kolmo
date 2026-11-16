/* Canonical decimals: parse to (mantissa, scale) and format back, byte for byte. The portable references of the
 * decimal kernels in arch/ (arch/decimal.c dispatches). Used by the table, log and graph transforms. */
static const int64_t P10[19] = {1LL,10LL,100LL,1000LL,10000LL,100000LL,1000000LL,10000000LL,100000000LL,1000000000LL,
    10000000000LL,100000000000LL,1000000000000LL,10000000000000LL,100000000000000LL,1000000000000000LL,
    10000000000000000LL,100000000000000000LL,1000000000000000000LL};

/* canonical decimal -> (mantissa, scale); 0 if not canonical (then stored as text) */
/* parse_dec for fields with at most 8 digits before and after the dot, when 24 bytes can be read at p: SWAR
 * validation and conversion, no per-byte loop. Same result as parse_dec (checked by the tests); others go to it. */
static inline uint64_t swar_digits_ok(uint64_t x, uint64_t m) {   /* all bytes selected by m are '0'..'9' */
    uint64_t hi = x & 0xF0F0F0F0F0F0F0F0ull, lo = x & 0x0F0F0F0F0F0F0F0Full;
    return (((hi ^ 0x3030303030303030ull) | ((lo + 0x0606060606060606ull) & 0xF0F0F0F0F0F0F0F0ull)) & m) == 0;
}
static inline uint64_t swar_parse8(uint64_t x, int L) {           /* value of the first L (1..8) digits of x */
    x <<= 8 * (8 - L);                                           /* leading zero bytes act as leading zeros */
    x &= 0x0F0F0F0F0F0F0F0Full;
    x = (x * 2561) >> 8; x &= 0x00FF00FF00FF00FFull;
    x = (x * 6553601) >> 16; x &= 0x0000FFFF0000FFFFull;
    return (x * 42949672960001ull) >> 32;
}
static inline int parse_dec_fast(const uint8_t *p, uint32_t len, size_t avail, int64_t *m, int *sc);
static inline int parse_dec(const uint8_t *p, uint32_t len, int64_t *m, int *sc) {
    if (!len || len > 20) return 0;
    uint32_t i = 0; int neg = 0;
    if (p[0] == '-') { neg = 1; i = 1; if (len == 1) return 0; }
    if (p[i] < '0' || p[i] > '9') return 0;
    if (p[i] == '0' && i + 1 < len && p[i + 1] != '.') return 0;              /* leading zero */
    int64_t v = 0; int digits = 0, s = 0, dot = 0;
    for (; i < len; i++) {
        uint8_t c = p[i];
        if (c == '.') { if (dot || i + 1 == len) return 0; dot = 1; continue; }
        if (c < '0' || c > '9') return 0;
        if (++digits > 18) return 0;
        v = v * 10 + (c - '0'); s += dot;
    }
    if (neg && v == 0) return 0;                                              /* "-0", "-0.00" */
    *m = neg ? -v : v; *sc = s; return 1;
}
static inline int parse_dec_fast(const uint8_t *p, uint32_t len, size_t avail, int64_t *m, int *sc) {
    if (len - 1u >= 18u || avail < 24) return parse_dec(p, len, m, sc);
    int neg = p[0] == '-'; const uint8_t *q = p + neg; int L = (int)len - neg;
    if (!L) return 0;
    uint64_t w0, w1; memcpy(&w0, q, 8); memcpy(&w1, q + 8, 8);
    uint64_t d0 = w0 ^ 0x2E2E2E2E2E2E2E2Eull, d1 = w1 ^ 0x2E2E2E2E2E2E2E2Eull;            /* '.' -> zero byte */
    uint64_t z0 = ~(((d0 & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | d0 | 0x7F7F7F7F7F7F7F7Full);
    uint64_t z1 = ~(((d1 & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | d1 | 0x7F7F7F7F7F7F7F7Full);
    uint64_t m0 = L >= 8 ? ~0ull : ((1ull << (8 * L)) - 1), m1 = L <= 8 ? 0 : L >= 16 ? ~0ull : ((1ull << (8 * (L - 8))) - 1);
    z0 &= m0 & HIGHS; z1 &= m1 & HIGHS;
    int ndot = __builtin_popcountll(z0) + __builtin_popcountll(z1);
    if (ndot > 1) return 0;
    int ip = ndot ? (z0 ? __builtin_ctzll(z0) >> 3 : 8 + (__builtin_ctzll(z1) >> 3)) : L;
    int fl = ndot ? L - ip - 1 : 0;
    if (ip == 0 || (ndot && fl == 0)) return 0;
    if (ip > 8 || fl > 8) return parse_dec(p, len, m, sc);
    if (q[0] == '0' && ip > 1) return 0;                                      /* leading zero */
    if (!swar_digits_ok(w0, ip >= 8 ? ~0ull : ((1ull << (8 * ip)) - 1))) return 0;
    uint64_t v = swar_parse8(w0, ip);
    if (fl) {
        uint64_t f; memcpy(&f, q + ip + 1, 8);
        if (!swar_digits_ok(f, fl >= 8 ? ~0ull : ((1ull << (8 * fl)) - 1))) return 0;
        v = v * (uint64_t)P10[fl] + swar_parse8(f, fl);
    }
    if (neg && v == 0) return 0;
    *m = neg ? -(int64_t)v : (int64_t)v; *sc = fl; return 1;
}

static const char DIG2[201] = "00010203040506070809101112131415161718192021222324252627282930313233343536373839"
    "404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899";
static uint64_t INV5[19];                         /* modular inverses of 5^k mod 2^64: exact division without divide */
__attribute__((constructor)) static void inv5_init(void) {
    for (int k = 0; k < 19; k++) { uint64_t a = 1; for (int i = 0; i < k; i++) a *= 5; uint64_t x = a; for (int i = 0; i < 6; i++) x *= 2 - a * x; INV5[k] = x; }
}
static inline int64_t exact_div_p10(int64_t v, int k) {    /* v is a multiple of 10^k */
    return (int64_t)((uint64_t)(v >> k) * INV5[k]);
}
/* writes the decimal; o must have 24 bytes of room */
static inline uint8_t *fmt_dec(uint8_t *o, int64_t m, int s) {
    char buf[24]; char *e = buf + 24, *q = e; uint64_t a = m < 0 ? -(uint64_t)m : (uint64_t)m;
    while (a >= 100) { uint64_t r = a % 100; a /= 100; q -= 2; memcpy(q, DIG2 + 2 * r, 2); }
    if (a >= 10) { q -= 2; memcpy(q, DIG2 + 2 * a, 2); } else *--q = (char)('0' + a);
    int nd = (int)(e - q);
    while (nd < s + 1) { *--q = '0'; nd++; }
    if (m < 0) *o++ = '-';
    if (!s) { memcpy(o, q, nd); return o + nd; }
    int ip = nd - s; memcpy(o, q, ip); o += ip; *o++ = '.'; memcpy(o, q + ip, s); return o + s;
}
static inline __attribute__((always_inline)) int ndigits(uint64_t a) {
    if (!a) return 1;
    int d = ((64 - __builtin_clzll(a)) * 1233) >> 12;        /* floor(log10) estimate */
    return d + (a >= (uint64_t)P10[d < 18 ? d : 18] ? 1 : 0) + (d >= 19);
}
/* same output as fmt_dec, written straight to its final place (back to front): no re-read of fresh stores */
static inline uint8_t *fmt_dec_fast(uint8_t *o, int64_t m, int s) {
    uint64_t a = m < 0 ? -(uint64_t)m : (uint64_t)m;
    *o = '-'; o += m < 0;
    int nd = ndigits(a); if (nd < s + 1) nd = s + 1;
    int tot = nd + (s > 0);
    uint8_t *p = o + tot;
    int f = s;
    while (f >= 2) { uint64_t r = a % 100; a /= 100; p -= 2; memcpy(p, DIG2 + 2 * r, 2); f -= 2; }
    if (f) { *--p = (uint8_t)('0' + a % 10); a /= 10; }
    if (s) *--p = '.';
    while (p - o >= 2) { uint64_t r = a % 100; a /= 100; p -= 2; memcpy(p, DIG2 + 2 * r, 2); }
    if (p > o) *--p = (uint8_t)('0' + a % 10);
    return o + tot;
}

/* 8 decimal digits of x < 1e8 as ASCII, most significant first in memory (little-endian SWAR) */
static inline __attribute__((always_inline)) uint64_t swar8(uint64_t x) {
    uint64_t hi = x / 10000, lo = x % 10000;
    uint64_t merged = hi | (lo << 32);
    uint64_t top = ((merged * 10486) >> 20) & ((0x7FULL << 32) | 0x7FULL);
    uint64_t bot = merged - 100 * top;
    uint64_t hundreds = (bot << 16) + top;
    uint64_t tens = (hundreds * 103) >> 10;
    tens &= (0xFULL << 48) | (0xFULL << 32) | (0xFULL << 16) | 0xFULL;
    tens += (hundreds - 10 * tens) << 8;
    return tens + 0x3030303030303030ULL;
}
/* same output as fmt_dec_fast for up to 16 digits: SWAR digit conversion and overlapping 8-byte stores, no loads;
 * needs 40 bytes of room at o (bytes past the result are scratch) */
static inline __attribute__((always_inline)) void st8(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint8_t *fmt_dec_swar(uint8_t *o, int64_t m, int s) {
    uint64_t a = m < 0 ? -(uint64_t)m : (uint64_t)m;
    int D = ndigits(a); if (D < s + 1) D = s + 1;
    if (D > 16) return fmt_dec_fast(o, m, s);
    *o = '-'; o += m < 0;
    if (D <= 8) {
        uint64_t l = swar8(a);
        st8(o, l >> (8 * (8 - D)));
        if (s) { o[D - s] = '.'; st8(o + D - s + 1, l >> (8 * (8 - s))); }
    } else {
        uint64_t h = swar8(a / 100000000), l = swar8(a % 100000000);
        st8(o, h >> (8 * (16 - D))); st8(o + D - 8, l);
        if (s) {
            o[D - s] = '.';
            if (s <= 8) st8(o + D - s + 1, l >> (8 * (8 - s)));
            else { st8(o + D - s + 1, h >> (8 * (16 - s))); st8(o + D - 7, l); }
        }
    }
    return o + D + (s > 0);
}
/* Same values, each written at o + FMT_STRIDE * i (fixed slots: no dependency between values); lengths to lv.
 * Values longer than 16 bytes still fit their slot (at most 22 bytes); needs 40 bytes of room past the last slot. */
#define FMT_STRIDE 24
