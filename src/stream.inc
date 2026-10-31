/* Streaming / large-input compression.
 *
 * A stream is a tiny header ("CZNS") followed by independent length-prefixed frames, each a
 * normal CZN1 container for one segment of the input, then a zero-length terminator. Encode and
 * decode hold at most one segment (plus its frame) in memory, so arbitrarily large inputs run in
 * bounded memory and the API works on pipes (no seek). Each segment still compresses and decodes
 * across all cores via the one-shot core. Segments are independent, so very small segments cost
 * ratio at their boundaries — the default (64 MiB) makes that negligible.
 *
 * Format:  "CZNS" ver(1) rsvd(3) seg_bytes(u32 le)   then  [ len(u64 le) frame[len] ]*  0(u64 le)
 */
#include <unistd.h>

#define CZNS_HDR 12
#define CZNS_SEG_DEFAULT (64u << 20)

static int io_read_full(int fd, uint8_t *buf, size_t n, size_t *got) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, buf + off, n - off);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break;                 /* EOF */
        off += (size_t)r;
    }
    *got = off;
    return 0;
}

static int io_write_full(int fd, const uint8_t *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, buf + off, n - off);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)w;
    }
    return 0;
}

/* Compress everything readable on in_fd to out_fd. seg_mib: segment size in MiB (0 = default 64).
 * Returns total bytes written, or a negative CZN_ERR_*. */
int64_t czn_stream_compress(int in_fd, int out_fd, const czn_params *p, uint32_t seg_mib) {
    czn_params d = {CZN_FAST, 0, 0, 0, 1}; if (!p) p = &d;
    uint64_t seg = seg_mib ? (uint64_t)seg_mib << 20 : CZNS_SEG_DEFAULT;
    if (seg < (1u << 20)) seg = 1u << 20;

    uint8_t hdr[CZNS_HDR] = {'C','Z','N','S', 1, 0, 0, 0};
    put32(hdr + 8, (uint32_t)(seg > 0xFFFFFFFF ? 0xFFFFFFFF : seg));
    if (io_write_full(out_fd, hdr, CZNS_HDR)) return CZN_ERR_ALLOC;
    int64_t total = CZNS_HDR;

    uint8_t *in = malloc(seg);
    if (!in) return CZN_ERR_ALLOC;
    uint8_t *out = NULL; size_t outcap = 0;
    int64_t ret = total;
    for (;;) {
        size_t got = 0;
        if (io_read_full(in_fd, in, seg, &got)) { ret = CZN_ERR_ALLOC; break; }
        if (got == 0) break;                           /* clean EOF */
        size_t bound = czn_bound(got, p);
        if (bound > outcap) { free(out); out = malloc(bound); if (!out) { ret = CZN_ERR_ALLOC; break; } outcap = bound; }
        int64_t cl = czn_compress(in, got, out, outcap, p);
        if (cl < 0) { ret = cl; break; }
        uint8_t lb[8]; put64(lb, (uint64_t)cl);
        if (io_write_full(out_fd, lb, 8) || io_write_full(out_fd, out, (size_t)cl)) { ret = CZN_ERR_ALLOC; break; }
        total += 8 + cl;
        ret = total;
        if (got < seg) break;                          /* short read = last segment */
    }
    free(in); free(out);
    if (ret < 0) return ret;
    uint8_t z[8]; put64(z, 0);                          /* end-of-stream terminator */
    if (io_write_full(out_fd, z, 8)) return CZN_ERR_ALLOC;
    return total + 8;
}

/* Decompress a CZNS stream from in_fd to out_fd. Returns total bytes written, or a negative CZN_ERR_*. */
int64_t czn_stream_decompress(int in_fd, int out_fd, int threads) {
    uint8_t hdr[CZNS_HDR]; size_t got = 0;
    if (io_read_full(in_fd, hdr, CZNS_HDR, &got)) return CZN_ERR_ALLOC;
    if (got < CZNS_HDR || memcmp(hdr, "CZNS", 4) || hdr[4] != 1) return CZN_ERR_CORRUPT;

    uint8_t *frame = NULL, *out = NULL; size_t fcap = 0, ocap = 0;
    int64_t total = 0, ret = 0;
    for (;;) {
        uint8_t lb[8]; size_t g2 = 0;
        if (io_read_full(in_fd, lb, 8, &g2)) { ret = CZN_ERR_ALLOC; break; }
        if (g2 == 0) { ret = CZN_ERR_CORRUPT; break; }  /* missing terminator */
        if (g2 < 8) { ret = CZN_ERR_CORRUPT; break; }
        uint64_t len = get64(lb);
        if (len == 0) { ret = total; break; }           /* clean end of stream */
        if (len > (1ull << 34)) { ret = CZN_ERR_CORRUPT; break; }
        if (len > fcap) { free(frame); frame = malloc(len); if (!frame) { ret = CZN_ERR_ALLOC; break; } fcap = len; }
        size_t fg = 0;
        if (io_read_full(in_fd, frame, len, &fg)) { ret = CZN_ERR_ALLOC; break; }
        if (fg < len) { ret = CZN_ERR_CORRUPT; break; }
        int64_t osz = czn_original_size(frame, len);
        if (osz < 0) { ret = osz; break; }
        if ((size_t)osz > ocap) { free(out); out = malloc(osz ? (size_t)osz : 1); if (!out) { ret = CZN_ERR_ALLOC; break; } ocap = (size_t)osz; }
        int64_t dl = czn_decompress(frame, len, out, (size_t)osz, threads);
        if (dl < 0) { ret = dl; break; }
        if (dl > 0 && io_write_full(out_fd, out, (size_t)dl)) { ret = CZN_ERR_ALLOC; break; }
        total += dl;
    }
    free(frame); free(out);
    return ret;
}
