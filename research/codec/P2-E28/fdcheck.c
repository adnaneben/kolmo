/* P2-E28 fdcheck: in-process decode of a CZN1 frame with the library under test (linked against the research
 * fast-inverse kolmo.o in the sanitizer gate), compare with the original, free everything: leak detection stays ON
 * (the production CLI's own exit-time buffer leak, reported in wire-review-20261113, is thereby not in the way).
 * usage: fdcheck FRAME ORIGINAL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kolmo.h"
static unsigned char *rd(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(l ? (size_t)l : 1); if (!b || fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    size_t fn, on; unsigned char *f = rd(argv[1], &fn), *o = rd(argv[2], &on); if (!f || !o) return 2;
    int64_t sz = kolmo_original_size(f, fn); if (sz < 0 || (size_t)sz != on) { free(f); free(o); return 1; }
    unsigned char *out = malloc(on ? on : 1); int64_t r = kolmo_decompress(f, fn, out, on, 1);
    int ok = r == (int64_t)on && !memcmp(out, o, on); free(out); free(f); free(o); return ok ? 0 : 1;
}
