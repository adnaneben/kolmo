/* Scanner helpers of the table and log transforms against their references: the inline population counts against
 * the compiler's builtin, and the SWAR decimal parser (parse_dec_fast) against the per-byte one (parse_dec) on random
 * and edge-case fields. make check-scan (part of check-rc), with AddressSanitizer. */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
int main(void) {
    long checks = 0, fails = 0;
    for (int it = 0; it < 2000000; it++) {
        uint64_t x = it < 64 ? 1ull << it : it < 128 ? ~0ull >> (it - 64) : rnd() & (it % 3 ? rnd() : ~0ull);
        checks++; if (kolmo_popcnt64(x) != __builtin_popcountll(x)) { if (fails++ < 10) printf("popcnt64 %llx\n", (unsigned long long)x); }
        uint64_t h = x & 0x8080808080808080ull;
        checks++; if (kolmo_popcnt_highs(h) != __builtin_popcountll(h)) { if (fails++ < 10) printf("popcnt_highs %llx\n", (unsigned long long)h); }
    }
    static const char *edge[] = {"0", "-0", "0.0", "-0.00", "00", "01", "0.", ".5", "-", "-.", "1.2.3", "123456789", "12345678.12345678",
                                 "123456789012345678", "1234567890123456789", "99999999.99999999", "-99999999.9", "1e5", "+1", " 1", "1 ", "12a"};
    uint8_t buf[64];
    for (int it = 0; it < 3000000; it++) {
        uint32_t len; memset(buf, (int)(rnd() & 0xff), sizeof buf);
        if (it < (int)(sizeof edge / sizeof edge[0])) { len = (uint32_t)strlen(edge[it]); memcpy(buf, edge[it], len); }
        else {
            static const char al[] = "0123456789012345678901234567890123456789.-.x ";
            len = 1 + (uint32_t)(rnd() % 22); int mode = (int)(rnd() % 4);
            for (uint32_t i = 0; i < len; i++) buf[i] = mode == 0 ? (uint8_t)al[rnd() % (sizeof al - 1)] : (uint8_t)('0' + rnd() % 10);
            if (mode == 1 && len > 2) buf[1 + rnd() % (len - 2)] = '.';
            if (mode == 2) buf[0] = '-';
        }
        int64_t m1 = 0, m2 = 0; int s1 = 0, s2 = 0;
        int a = parse_dec(buf, len, &m1, &s1), b = parse_dec_fast(buf, len, 64, &m2, &s2);
        checks++;
        if (a != b || (a && (m1 != m2 || s1 != s2))) { if (fails++ < 10) printf("parse_dec_fast \"%.*s\": %d %lld %d vs %d %lld %d\n", (int)len, buf, a, (long long)m1, s1, b, (long long)m2, s2); }
    }
    printf("%s: %ld checks, %ld failures\n", fails ? "FAILED" : "PASSED", checks, fails);
    return fails != 0;
}
