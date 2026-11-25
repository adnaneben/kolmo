/* POSIX allows a short positional read or write, including after a signal. At most 1 GiB per call: macOS refuses a
 * read or write of more than INT_MAX bytes with EINVAL (it is not a short count). */
static int read_block(int fd, uint8_t *p, size_t n, off_t off) {
    while (n) {
        ssize_t count = pread(fd, p, n > ((size_t)1 << 30) ? ((size_t)1 << 30) : n, off);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return KOLMO_ERR_IO;
        p += count; n -= count; off += count;
    }
    return 0;
}

static int write_block(int fd, const uint8_t *p, size_t n, off_t off) {
    while (n) {
        ssize_t count = pwrite(fd, p, n > ((size_t)1 << 30) ? ((size_t)1 << 30) : n, off);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return KOLMO_ERR_IO;
        p += count; n -= count; off += count;
    }
    return 0;
}
