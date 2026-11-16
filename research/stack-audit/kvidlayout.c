/* writes kvid's residual layout (src/experimental/kvid.c transform(), unchanged) to a file, so the stacking tool can
 * code it; checks kvid's inverse first. usage: kvidlayout in.y4m out.layout */
#define main kvid_main
#include "kvid.c"
#undef main
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    size_t n, tn; uint8_t *s = slurp(argv[1], &n); if (!s) return 1;
    uint8_t *t = transform(s, n, &tn); if (!t) return 1;
    uint8_t *b = malloc(n); if (!b || inverse(t, tn, b, n) || memcmp(b, s, n)) return 3;
    return spit(argv[2], t, tn) ? 1 : 0;
}
