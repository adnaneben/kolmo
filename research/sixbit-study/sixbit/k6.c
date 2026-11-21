/* k6: experimental 6-bit symbol codec (research, not part of the kolmo format).
 *
 * A table maps up to 61 codes to byte strings (1-8 bytes). Three codes are reserved:
 *   61 SHIFT  next letter code is upper case          (only with case folding)
 *   62 LOCK   toggle caps lock for letter codes       (only with case folding)
 *   63 ESC    the next two codes carry one raw byte (2 + 6 bits)
 *   62 PAGE   (no case folding, optional page 2) the next code indexes a second table of up to 64 bytes
 * Without case folding codes 61 and 62 are ordinary table entries (unless PAGE is in use).
 * Symbols are packed 4 per 3 bytes. Encoding is greedy longest match over the table.
 *
 * usage: k6 table.bin in out.k6       encode (prints sizes and timing)
 *        k6 -d table.bin in.k6 out     decode
 *        k6 -b table.bin in            round-trip benchmark: prints ratio, enc MB/s, dec MB/s
 * table.bin: u8 fold_case, u8 n, then n entries of (u8 len, bytes). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SHIFT 61
#define LOCK 62
#define ESC 63

typedef struct { int n, fold, np; uint8_t len[64]; uint8_t s[64][8];
                 int cand[256][64]; int ncand[256]; int16_t pagecode[256]; uint8_t page[64]; } table;
#define PAGE 62     /* only without case folding, when the table has a page: next code indexes page 2 */

static void load_table(table *t, const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); exit(1); }
    t->fold = fgetc(f); t->n = fgetc(f);
    int max = t->fold ? 61 : 63;
    memset(t->ncand, 0, sizeof t->ncand);
    for (int i = 0; i < t->n; i++) { t->len[i] = fgetc(f); if (fread(t->s[i], 1, t->len[i], f) != t->len[i]) exit(1); }
    /* optional page 2: u8 count, then that many single bytes (up to 64), reached through PAGE + one code */
    int np = fgetc(f); t->np = np == EOF ? 0 : np;
    memset(t->pagecode, -1, sizeof t->pagecode);
    for (int i = 0; i < t->np; i++) { t->page[i] = fgetc(f); t->pagecode[t->page[i]] = i; }
    if (t->np) max = 62;
    if (t->n > max || t->np > 64) { fprintf(stderr, "table too big\n"); exit(1); }
    fclose(f);
    /* candidates per first byte, longest first */
    for (int L = 8; L >= 1; L--) for (int i = 0; i < t->n; i++) if (t->len[i] == L) {
        int b = t->s[i][0]; t->cand[b][t->ncand[b]++] = i;
    }
}

static uint8_t *read_all(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*n + 16); if (fread(b, 1, *n, f) != *n) exit(1); fclose(f); return b;
}

/* emits symbols (one per byte) into sym; returns count */
static size_t encode_syms(const table *t, const uint8_t *in, size_t n, uint8_t *sym) {
    size_t i = 0, k = 0; int lock = 0;
    while (i < n) {
        uint8_t c = in[i];
        if (t->fold && c >= 'A' && c <= 'Z') {
            int lc = c + 32, code = -1;
            for (int j = 0; j < t->ncand[lc]; j++) { int e = t->cand[lc][j]; if (t->len[e] == 1) { code = e; break; } }
            if (code >= 0) {
                if (!lock) {
                    int run = 1; while (i + run < n && run < 3 && in[i + run] >= 'A' && in[i + run] <= 'Z') run++;
                    if (run >= 3) { sym[k++] = LOCK; lock = 1; } else sym[k++] = SHIFT;
                }
                sym[k++] = code; i++; continue;
            }
        }
        if (t->fold && lock && c >= 'a' && c <= 'z') { sym[k++] = LOCK; lock = 0; }
        int best = -1;
        for (int j = 0; j < t->ncand[c]; j++) {
            int e = t->cand[c][j], L = t->len[e];
            if (i + L <= n && memcmp(in + i, t->s[e], L) == 0) { best = e; break; }
        }
        if (best >= 0) { sym[k++] = best; i += t->len[best]; }
        else if (t->np && t->pagecode[c] >= 0) { sym[k++] = PAGE; sym[k++] = t->pagecode[c]; i++; }
        else { sym[k++] = ESC; sym[k++] = c >> 6; sym[k++] = c & 63; i++; }
    }
    return k;
}

static size_t pack(const uint8_t *sym, size_t k, uint8_t *out) {
    size_t o = 0, i = 0;
    for (; i + 4 <= k; i += 4) {
        uint32_t v = sym[i] | sym[i + 1] << 6 | sym[i + 2] << 12 | (uint32_t)sym[i + 3] << 18;
        out[o++] = v; out[o++] = v >> 8; out[o++] = v >> 16;
    }
    uint32_t v = 0; int r = k - i;
    for (int j = 0; j < r; j++) v |= (uint32_t)sym[i + j] << (6 * j);
    for (int j = 0; j < (6 * r + 7) / 8; j++) out[o++] = v >> (8 * j);
    return o;
}

/* decode k symbols packed in p to out; returns bytes written */
static size_t decode(const table *t, const uint8_t *p, size_t k, uint8_t *out) {
    size_t o = 0; int lock = 0, shift = 0;
    uint8_t sym[4]; size_t i = 0;
    #define GET(idx) ((idx) % 4 == 0 ? (p[(idx)/4*3] & 63) : (idx) % 4 == 1 ? ((p[(idx)/4*3] >> 6 | p[(idx)/4*3+1] << 2) & 63) : \
                      (idx) % 4 == 2 ? ((p[(idx)/4*3+1] >> 4 | p[(idx)/4*3+2] << 4) & 63) : (p[(idx)/4*3+2] >> 2))
    (void)sym;
    while (i < k) {
        int s = GET(i); i++;
        if (s == ESC) { int a = GET(i), b = GET(i + 1); i += 2; out[o++] = a << 6 | b; continue; }
        if (t->np && s == PAGE) { out[o++] = t->page[GET(i)]; i++; continue; }
        if (t->fold && s == SHIFT) { shift = 1; continue; }
        if (t->fold && s == LOCK) { lock ^= 1; continue; }
        memcpy(out + o, t->s[s], 8); /* over-copy, out has slack */
        if ((shift || lock) && t->len[s] == 1 && out[o] >= 'a' && out[o] <= 'z') out[o] -= 32;
        shift = 0; o += t->len[s];
    }
    return o;
}

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "-b") == 0) {
        table t; load_table(&t, argv[2]);
        size_t n; uint8_t *in = read_all(argv[3], &n);
        uint8_t *sym = malloc(3 * n + 16), *pk = malloc(3 * n + 16), *dec = malloc(n + 64);
        size_t k = 0, pn = 0, dn = 0; double te = 1e9, td = 1e9;
        for (int r = 0; r < 5; r++) {
            double a = now(); k = encode_syms(&t, in, n, sym); pn = pack(sym, k, pk); double b = now();
            dn = decode(&t, pk, k, dec); double c = now();
            if (b - a < te) te = b - a; if (c - b < td) td = c - b;
        }
        int ok = dn == n && memcmp(dec, in, n) == 0;
        size_t esc = 0; for (size_t i = 0; i < k; i++) esc += sym[i] == ESC;
        printf("%zu %zu %zu %.4f %.1f %.1f %s esc=%zu\n", n, k, pn, (double)n / pn, n / te / 1e6, n / td / 1e6, ok ? "OK" : "FAIL", esc);
        return !ok;
    }
    if (argc == 4) { /* encode to file: u64 symbol count + packed */
        table t; load_table(&t, argv[1]); size_t n; uint8_t *in = read_all(argv[2], &n);
        uint8_t *sym = malloc(3 * n + 16), *pk = malloc(3 * n + 16);
        size_t k = encode_syms(&t, in, n, sym), pn = pack(sym, k, pk);
        FILE *f = fopen(argv[3], "wb"); uint64_t K = k; fwrite(&K, 8, 1, f); fwrite(pk, 1, pn, f); fclose(f);
        /* also the unpacked symbol stream, one symbol per byte */
        char sp[4096]; snprintf(sp, sizeof sp, "%s.sym", argv[3]); f = fopen(sp, "wb"); fwrite(sym, 1, k, f); fclose(f);
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "-d") == 0) {
        table t; load_table(&t, argv[2]); size_t n; uint8_t *in = read_all(argv[3], &n);
        uint64_t K; memcpy(&K, in, 8); uint8_t *out = malloc(K * 8 + 64);
        size_t o = decode(&t, in + 8, K, out); FILE *f = fopen(argv[4], "wb"); fwrite(out, 1, o, f); fclose(f); return 0;
    }
    fprintf(stderr, "usage: see source\n"); return 2;
}
