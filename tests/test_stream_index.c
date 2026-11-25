/* Valid dense CZN1 and CZNS members on both sides of the offset metadata allowance. */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "line %d: %s\n", __LINE__, #expr); exit(1); } } while (0)

static void put_length(FILE *stream, uint64_t length) {
    for (int i = 0; i < 8; ++i) {
        CHECK(fputc((int)(length & 255), stream) != EOF);
        length >>= 8;
    }
}

static void verify_file(FILE *out, const uint8_t *expected, size_t size, unsigned copies) {
    uint8_t buf[8192];
    rewind(out);
    for (unsigned k = 0; k < copies; ++k) {
        for (size_t pos = 0; pos < size;) {
            size_t take = sizeof(buf) < size - pos ? sizeof(buf) : size - pos;
            CHECK(fread(buf, 1, take, out) == take && !memcmp(buf, expected + pos, take));
            pos += take;
        }
    }
    CHECK(fgetc(out) == EOF && !ferror(out));
}

static void check_dense(unsigned blocks) {
    const size_t n = blocks * (size_t)4096;
    uint8_t *raw = malloc(n);
    CHECK(raw);
    for (size_t i = 0; i < n; ++i) raw[i] = (uint8_t)(i % 251 + (i >> 16));
    kolmo_params p = {0};
    CHECK(kolmo_params_for_level(blocks == 4096 ? 3 : 1, &p) == 0);
    p.threads = 1;
    p.block = 4096;
    p.transforms = 0;
    size_t bound = kolmo_bound(n, &p);
    uint8_t *frame = malloc(bound);
    CHECK(frame);
    int64_t len = kolmo_compress(raw, n, frame, bound, &p);
    CHECK(len >= 32);
    unsigned actual = frame[20] | (unsigned)frame[21] << 8 | (unsigned)frame[22] << 16 | (unsigned)frame[23] << 24;
    CHECK(actual == blocks);
    int64_t declared = kolmo_decode_memory(frame, (size_t)len);
    CHECK(declared > 0);
    const uint64_t metadata = 16 * ((uint64_t)blocks + 1);
    const uint64_t headroom = metadata > 65536 ? metadata - 65536 : 0;
    kolmo_decode_limits limits = {0};
    limits.max_memory = (uint64_t)declared + headroom - 1;
    CHECK(kolmo_check_frame(frame, (size_t)len, &limits) == KOLMO_ERR_MEMORY);
    ++limits.max_memory;
    CHECK(kolmo_check_frame(frame, (size_t)len, &limits) == 0);

    FILE *mixed = tmpfile();
    CHECK(mixed);
    /* A plain member followed by a segmented member exercises both stream entry points. */
    const uint8_t header[] = {'C','Z','N','S',1,0,0,0,0,0,0,2};
    CHECK(fwrite(frame, 1, (size_t)len, mixed) == (size_t)len);
    CHECK(fwrite(header, 1, sizeof(header), mixed) == sizeof(header));
    put_length(mixed, (uint64_t)len);
    CHECK(fwrite(frame, 1, (size_t)len, mixed) == (size_t)len);
    put_length(mixed, 0);
    CHECK(!fflush(mixed));
    for (int threads = 1; threads <= 4; threads += 3) {
        FILE *output = tmpfile();
        CHECK(output && lseek(fileno(mixed), 0, SEEK_SET) == 0);
        CHECK(kolmo_stream_decompress_limited(fileno(mixed), fileno(output), threads, &limits) == 2 * (int64_t)n);
        verify_file(output, raw, n, 2);
        fclose(output);
    }
    fclose(mixed);
    FILE *regular = tmpfile(), *restored = tmpfile();
    CHECK(regular && restored);
    CHECK(fwrite(frame, 1, (size_t)len, regular) == (size_t)len && !fflush(regular));
    uint64_t fd_metadata = metadata + 32 + 16 * (uint64_t)blocks;
    uint64_t fd_cap = (uint64_t)declared + (fd_metadata > 65536 ? fd_metadata - 65536 : 0);
    kolmo_set_memlimit(fd_cap - 1);
    CHECK(kolmo_decompress_file(fileno(regular), fileno(restored), (size_t)len, 4, NULL, 0) == KOLMO_ERR_MEMORY);
    kolmo_set_memlimit(fd_cap);
    CHECK(kolmo_decompress_file(fileno(regular), fileno(restored), (size_t)len, 4, NULL, 0) == (int64_t)n);
    kolmo_set_memlimit(0);
    verify_file(restored, raw, n, 1);
    fclose(regular);
    fclose(restored);
    uint8_t clip[27];
    CHECK(kolmo_read_range(frame, (size_t)len, 4091, sizeof(clip), clip, 4) == sizeof(clip));
    CHECK(!memcmp(clip, raw + 4091, sizeof(clip)));
    if (blocks == 4096) {
        /* FORMAT.md permits a v12 frame to omit its decode working-set declaration. The codec and payload
         * stay identical. In that representation the scheduler must leave the admitted codec space alone. */
        CHECK(frame[32 + 12] == 32); /* KLZH2 */
        frame[11] = 0;
        uint32_t crc = kolmo_crc32c(0, frame, 28);
        crc = kolmo_crc32c(crc, frame + 32, (size_t)16 * blocks);
        for (unsigned b = 0; b < 4; ++b) frame[28 + b] = (uint8_t)(crc >> (8 * b));
        /* KLZH2 needs two MiB beyond the largest block's buffer floor. */
        int64_t floor = kolmo_decode_memory(frame, (size_t)len);
        CHECK(floor > 0);
        limits.max_memory = (uint64_t)floor + (2u << 20) + headroom;
        uint8_t *decoded = malloc(n);
        CHECK(decoded);
        for (int threads = 1; threads <= 4; threads += 3) {
            CHECK(kolmo_decompress_limited(frame, (size_t)len, decoded, n, threads, &limits) == (int64_t)n);
            CHECK(!memcmp(raw, decoded, n));
        }
        free(decoded);
    }
    free(frame);
    free(raw);
}

int main(void) {
    check_dense(2046);
    check_dense(2047);
    check_dense(4095);
    check_dense(4096);
    puts("Valid dense streams, offset admission boundary, and clipped ranges passed");
    return 0;
}
