/* libFuzzer target: the CZNS streaming decoder (kolmo_stream_decompress, src/stream.inc) reading from a memfd and
 * writing to /dev/null. The decoder itself allocates the 64-bit frame length and the claimed original size of each
 * frame, so the harness first walks the stream and skips inputs where either exceeds 16 MiB (never trusting a size
 * beyond that cap). */
#define _GNU_SOURCE
#include "kolmo.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define CAP (16u << 20)
static int nullfd = -1, memfd = -1;
static uint64_t g64(const uint8_t *p) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > CAP) return 0;
    if (size >= 12) {
        size_t i = 12;
        while (i + 8 <= size) {
            uint64_t len = g64(data + i); i += 8;
            if (len == 0) break;
            if (len > CAP) return 0;
            if (len <= size - i) { int64_t o = kolmo_original_size(data + i, (size_t)len); if (o > (int64_t)CAP) return 0; }
            else break;                                   /* truncated frame: allocation bounded by len <= CAP */
            i += (size_t)len;
        }
    }
    if (nullfd < 0) nullfd = open("/dev/null", O_WRONLY);
    if (memfd < 0) memfd = memfd_create("fuzz_stream", 0);
    if (nullfd < 0 || memfd < 0) return 0;
    if (ftruncate(memfd, 0) || lseek(memfd, 0, SEEK_SET) != 0) return 0;
    size_t off = 0; while (off < size) { ssize_t w = write(memfd, data + off, size - off); if (w <= 0) return 0; off += (size_t)w; }
    lseek(memfd, 0, SEEK_SET);
    kolmo_stream_decompress(memfd, nullfd, 1 + (int)(size & 1));
    return 0;
}
