/* GR6+ bit census: where GR6+ spends its bits, per decision kind (study tool, rc3w3 phrase lane) */
#include "../../../src/kolmo.c"
#include "g6x.c"
int main(int argc, char **argv) {
    static const char *kn[] = {"nf", "op", "rank", "rank2", "cdict", "gid", "num", "numnew", "char", "pfx", "row"};
    extern int g6x_rowmode; extern long g6x_rows_flagged, g6x_rows_total, g6x_mismatch; if (getenv("ROW") && *getenv("ROW")) g6x_rowmode = 1;
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *d = malloc(n); if (fread(d, 1, n, f) != n) return 1; fclose(f);
        memset(g6x_bits, 0, sizeof g6x_bits); memset(g6x_units, 0, sizeof g6x_units);
        uint8_t *o = malloc(n * 2 + 65536); size_t c = gr6x_compress(d, n, o, n * 2 + 65536);
        double tot = 0; for (int k = 0; k < G6X_NKIND; k++) tot += g6x_bits[k];
        printf("%s n %zu gr6 %zu applies %d:", argv[a], n, c, gr6x_applies(d, n));
        for (int k = 0; k < G6X_NKIND; k++) if (g6x_units[k]) printf(" %s %.1f%% (%.0f B, %ld u)", kn[k], 100 * g6x_bits[k] / tot, g6x_bits[k] / 8, g6x_units[k]);
        printf(" | rows %ld flagged %ld mismatch %ld\n", g6x_rows_total, g6x_rows_flagged, g6x_mismatch); g6x_rows_total = g6x_rows_flagged = g6x_mismatch = 0; free(d); free(o);
    }
    return 0;
}
