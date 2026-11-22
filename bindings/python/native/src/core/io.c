/* POSIX allows a short positional read or write, including after a signal. */
static int read_block(int fd, uint8_t *p, size_t n, off_t off) {
    while (n) {
        ssize_t count = pread(fd, p, n > SSIZE_MAX ? SSIZE_MAX : n, off);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return KOLMO_ERR_IO;
        p += count; n -= count; off += count;
    }
    return 0;
}

static int write_block(int fd, const uint8_t *p, size_t n, off_t off) {
    while (n) {
        ssize_t count = pwrite(fd, p, n > SSIZE_MAX ? SSIZE_MAX : n, off);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return KOLMO_ERR_IO;
        p += count; n -= count; off += count;
    }
    return 0;
}
