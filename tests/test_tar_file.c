/* Interpose only this translation unit's mappings to exercise member fallback
 * and verify that both normal and FASTA early-return paths release their views. */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int watched_fd = -1, reject_members;
static size_t archive_size;
static atomic_uint attempts, opened, closed;

static void *watched_mmap(void *addr, size_t size, int prot, int flags, int fd, off_t offset) {
    if (watched_fd >= 0 && fd == watched_fd && size != archive_size) {
        atomic_fetch_add(&attempts, 1);
        if (reject_members) { errno = ENOMEM; return MAP_FAILED; }
    }
    void *result = mmap(addr, size, prot, flags, fd, offset);
    if (watched_fd >= 0 && result != MAP_FAILED) atomic_fetch_add(&opened, 1);
    return result;
}
static int watched_munmap(void *addr, size_t size) {
    int result = munmap(addr, size);
    if (watched_fd >= 0 && !result) atomic_fetch_add(&closed, 1);
    return result;
}
#define mmap watched_mmap
#define munmap watched_munmap
#include "../src/kolmo.c"
#undef munmap
#undef mmap

static size_t member_size(size_t n) { return 512 + (n + 511) / 512 * 512; }

static uint8_t *member(uint8_t *header, const char *name, size_t n) {
    snprintf((char *)header, 100, "%s", name);
    snprintf((char *)header + 124, 12, "%011o", (unsigned)n);
    header[156] = '0';
    memcpy(header + 257, "ustar", 5);
    memcpy(header + 263, "00", 2);
    memset(header + 148, ' ', 8);
    unsigned sum = 0;
    for (size_t i = 0; i < 512; ++i) sum += header[i];
    snprintf((char *)header + 148, 8, "%06o", sum);
    header[155] = ' ';
    return header + 512;
}

static uint8_t *make_archive(size_t *total) {
    const size_t a = (1u << 20) + 37, b = (1u << 20) + 143, c = (1u << 20) + 301;
    *total = member_size(a) + member_size(b) + member_size(c) + 1024;
    uint8_t *tar = calloc(1, *total);
    assert(tar);
    uint8_t *dna = member(tar, "sequence.fa", a);
    memcpy(dna, ">ref\n", 5);
    uint32_t state = 81791;
    for (size_t i = 5; i < a; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        dna[i] = (i - 5) % 81 == 80 ? '\n' : "ACGT"[state & 3];
    }
    uint8_t *table = member(tar + member_size(a), "meter.csv", b);
    const char row[] = "2023-06-06,12023,742,43.000123\n";
    for (size_t i = 0; i < b; ++i) table[i] = row[i % (sizeof row - 1)];
    uint8_t *binary = member(tar + member_size(a) + member_size(b), "records.bin", c);
    for (size_t i = 0; i < c; ++i) binary[i] = (uint8_t)((i / 11) ^ (i >> 9));
    return tar;
}

static void check_files(const uint8_t *src, size_t n, int input, int level) {
    kolmo_params p = {KOLMO_FAST, level, 1, 0, 1};
    size_t cap = kolmo_bound(n, &p);
    uint8_t *reference = malloc(cap), *frame = malloc(cap), *restored = malloc(n);
    assert(reference && frame && restored);
    int64_t encoded = kolmo_compress(src, n, reference, cap, &p);
    assert(encoded > 0 && !memcmp(reference, "CZN1", 4));
    uint32_t blocks = get32(reference + 20);
    unsigned dna = 0;
    for (uint32_t i = 0; i < blocks; ++i) {
        unsigned codec = reference[HDR + i * IDX + 12];
        dna += codec == C_DNA || codec == C_DNA_PREFIX || codec == C_DNA_RUNS;
    }
    assert((dna != 0) == (level == 0));
    for (int threads = 1; threads <= 4; threads += 3) {
        p.threads = threads;
        assert(kolmo_compress(src, n, frame, cap, &p) == encoded && !memcmp(reference, frame, encoded));
        for (int fail = 0; fail <= 1; ++fail) {
            FILE *output = tmpfile();
            assert(output);
            int out = fileno(output);
            assert(lseek(out, 11, SEEK_SET) == 11);
            watched_fd = input; archive_size = n; reject_members = fail;
            atomic_store(&opened, 0); atomic_store(&closed, 0); atomic_store(&attempts, 0);
            int64_t size = kolmo_compress_file(input, out, n, &p, NULL, 0);
            watched_fd = -1;
            assert(size == encoded && atomic_load(&attempts) == 3);
            assert(atomic_load(&opened) == (fail ? 1 : 4) && atomic_load(&closed) == atomic_load(&opened));
            assert(lseek(input, 0, SEEK_CUR) == 19 && lseek(out, 0, SEEK_CUR) == 11);
            assert(pread(out, frame, size, 0) == size && !memcmp(reference, frame, size));
            assert(kolmo_decompress(frame, size, restored, n, threads) == (int64_t)n && !memcmp(restored, src, n));
            fclose(output);
        }
    }
    free(reference); free(frame); free(restored);
}

int main(void) {
    size_t n;
    uint8_t *archive = make_archive(&n);
    FILE *file = tmpfile();
    assert(file && fwrite(archive, 1, n, file) == n);
    assert(fputc(42, file) != EOF && !fflush(file)); /* Compress only the requested prefix. */
    int input = fileno(file);
    assert(lseek(input, 19, SEEK_SET) == 19);
    check_files(archive, n, input, 0);
    check_files(archive, n, input, 3);
    fclose(file); free(archive);
    puts("Tar file/memory archives, member mapping fallback and cleanup passed");
    return 0;
}
