#include "src/kolmo.c"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 5 || (strcmp(argv[1], "encode") && strcmp(argv[1], "decode"))) {
        fprintf(stderr, "usage: %s encode|decode INPUT OUTPUT RAW_BYTES\n", argv[0]);
        return 2;
    }
    int encode = !strcmp(argv[1], "encode");
    char *end;
    unsigned long long expected = strtoull(argv[4], &end, 10);
    if (*end || expected < 64 || expected >= (1u << 28)) return 2;
    FILE *f = fopen(argv[2], "rb");
    if (!f || fseek(f, 0, SEEK_END)) return 3;
    long bytes = ftell(f);
    if (bytes < 5 || fseek(f, 0, SEEK_SET)) return 3;
    size_t n = (size_t)bytes;
    size_t raw = (size_t)expected;
    if (encode && n != raw) return 3;
    size_t cap = encode ? raw + raw / 8 + 1024 : raw;
    uint8_t *in = malloc(n), *out = malloc(cap);
    if (!in || !out || fread(in, 1, n, f) != n || fclose(f)) return 3;
    const char *wanted = getenv("KOLMO_CPU");
    const char *have = kolmo_isa_name(kolmo_cpu_level());
    if (wanted && strcmp(wanted, have)) {
        fprintf(stderr, "requested dispatch %s, got %s\n", wanted, have);
        return 5;
    }
    size_t written = raw;
    if (encode) {
        written = cm_compress_raced(in, n, out, cap, 4, 0, 1, cm4_mask_for(in, n));
        if (written < 5 || out[3] >> 4 != 8 || out[4] >> 4 != 9) return 4;
    } else if (in[3] >> 4 != 8 || in[4] >> 4 != 9 || cm_decompress(in, n, out, raw)) {
        return 4;
    }
    f = fopen(argv[3], "wb");
    if (!f || fwrite(out, 1, written, f) != written || fclose(f)) return 3;
    free(in);
    free(out);
    return 0;
}
