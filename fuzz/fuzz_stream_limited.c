/* libFuzzer target: kolmo_stream_decompress_limited (src/stream.inc) with tight limits chosen by the last input byte,
 * and with generous limits (16 MiB). No size from the input is pre-filtered: frame lengths are accepted up to the
 * decoder's own 16 GiB bound, and the claimed original size of every frame must be refused by max_output_bytes
 * before allocation. Invariants: a successful tight decode returns at most max_output_bytes, and when both decodes
 * succeed they return the same byte count and write the same bytes. */
#define _GNU_SOURCE
#include "kolmo.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define CAP (16u << 20)
static int in_fd = -1, oa = -1, ob = -1;
static uint8_t *bufa, *bufb;
static int load(int fd, const uint8_t *d, size_t n) {
    if (ftruncate(fd, 0) || lseek(fd, 0, SEEK_SET) != 0) return -1;
    size_t off = 0; while (off < n) { ssize_t w = write(fd, d + off, n - off); if (w <= 0) return -1; off += (size_t)w; }
    return lseek(fd, 0, SEEK_SET) == 0 ? 0 : -1;
}
static size_t slurp(int fd, uint8_t *b, size_t cap) {
    lseek(fd, 0, SEEK_SET); size_t n = 0; ssize_t r;
    while (n < cap && (r = read(fd, b + n, cap - n)) > 0) n += (size_t)r;
    return n;
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > CAP) return 0;
    if (in_fd < 0) { in_fd = memfd_create("sl_in", 0); oa = memfd_create("sl_a", 0); ob = memfd_create("sl_b", 0);
                     bufa = malloc(CAP + 1); bufb = malloc(CAP + 1); }
    if (in_fd < 0 || oa < 0 || ob < 0 || !bufa || !bufb) return 0;
    uint8_t k = data[size - 1];
    static const uint64_t outs[8] = {0 + 1, 7, 4096, 65536, 1u << 20, 4u << 20, 16u << 20, 3};
    static const uint32_t blks[4] = {1, 4096, 65536, 1u << 20};
    static const uint32_t nbl[4] = {1, 2, 4, 4096};
    kolmo_decode_limits tight = {outs[k & 7], blks[(k >> 3) & 3], nbl[(k >> 5) & 3]};
    kolmo_decode_limits big = {CAP, CAP, 4096};
    int T = 1 + ((k >> 7) & 1);
    if (load(in_fd, data, size) || load(oa, NULL, 0) || load(ob, NULL, 0)) return 0;
    int64_t ra = kolmo_stream_decompress_limited(in_fd, oa, T, &big);
    if (ra > (int64_t)CAP) __builtin_trap();
    if (lseek(in_fd, 0, SEEK_SET) != 0) return 0;
    int64_t rb = kolmo_stream_decompress_limited(in_fd, ob, 1, &tight);
    if (rb > (int64_t)tight.max_output_bytes) __builtin_trap();
    if (ra >= 0 && rb >= 0) {
        if (ra != rb) __builtin_trap();
        size_t na = slurp(oa, bufa, CAP + 1), nb = slurp(ob, bufb, CAP + 1);
        if (na != (size_t)ra || nb != na || memcmp(bufa, bufb, na)) __builtin_trap();
    }
    return 0;
}
