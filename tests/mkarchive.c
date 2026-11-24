/* Writes a DDP1 archive whose stored names are given on the command line, each holding "evil\n" (what `kolmo archive`
 * itself never stores: absolute names and names with ".." components), for tests/test_archive_safety.sh.
 * Usage: mkarchive out.kol name...   Licence: Apache-2.0. */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: mkarchive out.kol name...\n"); return 2; }
    static const uint8_t body[] = "evil\n";
    int nf = argc - 2; kolmo_archive_file *F = calloc((size_t)nf, sizeof *F);
    if (!F) return 1;
    for (int i = 0; i < nf; i++) F[i] = (kolmo_archive_file){argv[i + 2], body, sizeof body - 1, 0644, 0};
    kolmo_params P; kolmo_params_for_level(1, &P);
    size_t cap = kolmo_archive_bound(F, (size_t)nf, &P); uint8_t *o = malloc(cap);
    int64_t r = o ? kolmo_archive_create(F, (size_t)nf, o, cap, &P, NULL, NULL) : -1;
    FILE *f = r > 0 ? fopen(argv[1], "wb") : NULL;
    int bad = !f || fwrite(o, 1, (size_t)r, f) != (size_t)r; if (f && fclose(f)) bad = 1;
    free(o); free(F);
    if (bad) { fprintf(stderr, "mkarchive: failed (%lld)\n", (long long)r); return 1; }
    return 0;
}
