/* Research harness of the twopass lane: codec 7 size against the hash-table size (log2 buckets per model).
 *   cmbits FILE MODEL   prints the size for the default table size and for 1 and 2 bits more and less */
#include "../../../src/kolmo.c"
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(l + 1); if (fread(b, 1, l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
int main(int argc, char **argv) {
    size_t n; uint8_t *s = slurp(argv[1], &n); int model = atoi(argv[2]), v2 = model == 3 ? 3 : 1, b0 = cm_bits_for(n);
    printf("%s model=%d", argv[1], model);
    for (int d = -2; d <= 2; d++) {
        int bb = b0 + d; if (bb < 16 || bb > 22) continue;
        uint8_t *hist = malloc(n + 8); cm_t *m = cm_new_x(hist, bb, v2, v2 == 3 ? (n + 8) * 4 : 0, 1);
        size_t cap = n + n / 8 + 1024, o = 4; uint8_t *out = malloc(cap); cm_enc_run(m, s, n, out, cap, &o, v2);
        printf(" bb%d=%zu", bb, o + 1); cm_free(m); free(hist); free(out);
    }
    printf("\n"); return 0;
}
