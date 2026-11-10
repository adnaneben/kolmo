/* Attribute CPU time inside the verified wide block recipe. Single worker,
 * in memory. Keep these observations separate from end-to-end scoring. */
#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "../../src/kolmo.c"

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f || fseek(f, 0, SEEK_END)) return 1;
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET)) return 1;
    uint8_t *frame = malloc((size_t)size);
    if (!frame || fread(frame, 1, (size_t)size, f) != (size_t)size) return 1;
    fclose(f);
    parsed_t p;
    if (parse(frame, size, &p, NULL)) return 1;
    size_t max = 0;
    for (uint32_t i = 0; i < p.nb; i++) {
        size_t n = p.roff[i + 1] - p.roff[i];
        if (n > max) max = n;
    }
    uint8_t *tmp = malloc(max + TSLACK + 64), *out = malloc(max + 64);
    uint32_t *pos = malloc((max + 1) * sizeof(*pos)), *rows = malloc((max + 8) * sizeof(*rows));
    ZSTD_DCtx *z = ZSTD_createDCtx();
    if (!tmp || !out || !pos || !rows || !z) return 1;
    puts("round,zstd_seconds,restore_rows_seconds,checksum_seconds,suffix_bytes");
    for (int rep = 0; rep < 6; rep++) {
        double zsec = 0, restore = 0, check = 0;
        uint64_t suffix = 0;
        for (uint32_t i = 0; i < p.nb; i++) {
            const uint8_t *entry = p.idx + i * IDX;
            const uint8_t *block = frame + p.poff[i];
            size_t n = get32(entry), stored = get32(entry + 4);
            if (stored < 4 + 1024 || entry[12] != C_ZSTD || entry[13] != 2 ||
                block[0] != T_TP || block[3] != 255) return 1;
            int64_t lanes[256];
            for (int j = 0; j < 256; j++) lanes[j] = get32(block + 4 + 4 * j);
            suffix += (uint64_t)lanes[255];
            double t0 = seconds();
            if (ZSTD_decompressDCtx(z, tmp, n, block + 1028, stored - 1028) != n) return 1;
            double t1 = seconds();
            int r = tpq_inv_cols(tmp, n, block[1], 256, lanes, out, pos, rows);
            if (r > 0) r = tpq_inv(tmp, n, block[1], 256, lanes, out, pos);
            if (r) return 1;
            double t2 = seconds();
            if (blk_sum(p.ver, out, n) != get32(entry + 8)) return 1;
            zsec += t1 - t0; restore += t2 - t1; check += seconds() - t2;
        }
        printf("%d,%.9f,%.9f,%.9f,%" PRIu64 "\n", rep, zsec, restore, check, suffix);
    }
    ZSTD_freeDCtx(z);
    free(out); free(tmp); free(rows); free(pos); free(p.poff); free(p.roff); free(frame);
    zs_trim();
    return 0;
}
