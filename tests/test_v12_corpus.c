/* Frozen container-version-12 decode corpus (COMPATIBILITY.md): tests/v12 holds frames in the six modes of 0.0.1 (top
 * level) and the frames of each release from 0.0.2 on (r<version>/, written by tests/make_v12_frames.py), with their
 * originals; tests/v12/MANIFEST lists "frame original" pairs. Every decoder of version 12 must keep decoding them to the
 * same bytes, through the whole-buffer call, the frame check and a range read. Do not regenerate these frames with a
 * newer encoder: they are the compatibility record.
 * Usage: test-v12-corpus [dir] (default tests/v12). */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(s > 0 ? (size_t)s : 1);
    if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)s; return b;
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "tests/v12";
    char path[4096], a[1024], b[1024]; snprintf(path, sizeof path, "%s/MANIFEST", dir);
    FILE *m = fopen(path, "r"); if (!m) { fprintf(stderr, "test-v12-corpus: no %s\n", path); return 1; }
    int count = 0, bad = 0;
    while (fscanf(m, "%1023s %1023s", a, b) == 2) {
        size_t fn, on; uint8_t *f, *o;
        snprintf(path, sizeof path, "%s/%s", dir, a); f = slurp(path, &fn);
        snprintf(path, sizeof path, "%s/%s", dir, b); o = slurp(path, &on);
        if (!f || !o) { fprintf(stderr, "missing %s or %s\n", a, b); bad++; free(f); free(o); continue; }
        uint8_t *d = malloc(on + 1); int ok = 1;
        if (kolmo_original_size(f, fn) != (int64_t)on || kolmo_frame_size(f, fn) != (int64_t)fn || kolmo_check_frame(f, fn, NULL)) ok = 0;
        for (int t = 1; ok && t <= 4; t *= 4) if (kolmo_decompress(f, fn, d, on, t) != (int64_t)on || memcmp(d, o, on)) ok = 0;
        if (ok && on > 2) {
            size_t lo = on / 3, len = on / 3 + 1;
            if (kolmo_read_range(f, fn, lo, len, d, 1) != (int64_t)len || memcmp(d, o + lo, len)) ok = 0;
        }
        if (!ok) { fprintf(stderr, "v12 corpus: %s does not decode to %s\n", a, b); bad++; }
        count++; free(f); free(o); free(d);
    }
    fclose(m);
    if (bad || !count) { fprintf(stderr, "V12 CORPUS FAILED (%d of %d)\n", bad, count); return 1; }
    printf("V12 CORPUS PASSED (%d frozen frames decode to their originals)\n", count);
    return 0;
}
