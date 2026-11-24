/* R03 probe: share of each segment's bytes that repeat (>= 32 bytes) content of an EARLIER, different segment.
   usage: xrep file len1 len2 ... (segment lengths, in order) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static inline uint64_t h32(const uint8_t *p) { uint64_t a, b, c, d; memcpy(&a, p, 8); memcpy(&b, p + 8, 8); memcpy(&c, p + 16, 8); memcpy(&d, p + 24, 8);
    uint64_t h = (a * 0x9E3779B97F4A7C15ull) ^ (b * 0xC2B2AE3D27D4EB4Full); h = (h ^ (h >> 29)) * 0x165667B19E3779F9ull; h ^= (c * 0xFF51AFD7ED558CCDull) ^ (d * 0xC4CEB9FE1A85EC53ull); return h ^ (h >> 31); }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *s = malloc(n + 64); if (fread(s, 1, n, f) != (size_t)n) return 1; fclose(f);
    int ns = argc - 2; long st[64], en[64], p = 0; for (int i = 0; i < ns; i++) { st[i] = p; p += atol(argv[2 + i]); en[i] = p; }
    size_t TS = 1u << 22; uint32_t *tab = calloc(TS, 4);            /* position + 1 of the first anchor with this hash */
    long cross[64] = {0}, within[64] = {0};
    for (int g = 0; g < ns; g++) {
        for (long i = st[g]; i + 32 <= en[g]; i++) {
            uint64_t h = h32(s + i); if (h & 63) continue;           /* about one anchor per 64 bytes */
            size_t at = (h >> 8) & (TS - 1); uint32_t q = tab[at];
            if (q && !memcmp(s + q - 1, s + i, 32)) {
                long src = q - 1, L = 32; while (i + L < en[g] && src + L < i && s[src + L] == s[i + L]) L++;
                if (src < st[g]) cross[g] += L; else within[g] += L;
                i += L - 1;
            } else if (!q) tab[at] = (uint32_t)(i + 1);
        }
    }
    long tc = 0; for (int g = 0; g < ns; g++) { tc += cross[g]; printf("segment %d: %ld bytes, repeats from earlier segments %ld (%.1f%%), from itself %ld (%.1f%%)\n", g, en[g] - st[g], cross[g], 100.0 * cross[g] / (en[g] - st[g]), within[g], 100.0 * within[g] / (en[g] - st[g])); }
    printf("total cross-segment repeat share: %.2f%% of %ld bytes\n", 100.0 * tc / n, n);
    return 0;
}
