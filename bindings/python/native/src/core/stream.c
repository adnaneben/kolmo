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

#define KOLMO_STREAM_HDR 12
#define KOLMO_STREAM_SEG_DEFAULT (64u << 20)

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
 * Returns total bytes written, or a negative KOLMO_ERR_*. */
int64_t kolmo_stream_compress(int in_fd, int out_fd, const kolmo_params *p, uint32_t seg_mib) {
    kolmo_params d = {KOLMO_FAST, 0, 0, 0, 1}; if (!p) p = &d;
    uint64_t seg = seg_mib ? (uint64_t)seg_mib << 20 : KOLMO_STREAM_SEG_DEFAULT;
    if (seg < (1u << 20)) seg = 1u << 20;

    uint8_t hdr[KOLMO_STREAM_HDR] = {'C','Z','N','S', 1, 0, 0, 0};
    put32(hdr + 8, (uint32_t)(seg > 0xFFFFFFFF ? 0xFFFFFFFF : seg));
    if (io_write_full(out_fd, hdr, KOLMO_STREAM_HDR)) return KOLMO_ERR_ALLOC;
    int64_t total = KOLMO_STREAM_HDR;

    uint8_t *in = malloc(seg);
    if (!in) return KOLMO_ERR_ALLOC;
    uint8_t *out = NULL; size_t outcap = 0;
    int64_t ret = total;
    for (;;) {
        size_t got = 0;
        if (io_read_full(in_fd, in, seg, &got)) { ret = KOLMO_ERR_ALLOC; break; }
        if (got == 0) break;                           /* clean EOF */
        size_t bound = kolmo_bound(got, p);
        if (bound > outcap) { free(out); out = malloc(bound); if (!out) { ret = KOLMO_ERR_ALLOC; break; } outcap = bound; }
        int64_t cl = kolmo_compress(in, got, out, outcap, p);
        if (cl < 0) { ret = cl; break; }
        uint8_t lb[8]; put64(lb, (uint64_t)cl);
        if (io_write_full(out_fd, lb, 8) || io_write_full(out_fd, out, (size_t)cl)) { ret = KOLMO_ERR_ALLOC; break; }
        total += 8 + cl;
        ret = total;
        if (got < seg) break;                          /* short read = last segment */
    }
    free(in); free(out);
    if (ret < 0) return ret;
    uint8_t z[8]; put64(z, 0);                          /* end-of-stream terminator */
    if (io_write_full(out_fd, z, 8)) return KOLMO_ERR_ALLOC;
    return total + 8;
}

/* Decompress a CZNS stream from in_fd to out_fd. Returns total bytes written, or a negative KOLMO_ERR_*.
 * A frame's buffer grows only as its bytes arrive (at most KOLMO_STREAM_STEP ahead of them), so a forged frame
 * length cannot make the decoder reserve more memory than the input holds. `limits` (may be NULL) bounds each
 * frame's decoded size, block size and block count, and the stream's total output (max_output_bytes). */
#define KOLMO_STREAM_STEP (64u << 20)
int64_t kolmo_stream_decompress_limited(int in_fd, int out_fd, int threads, const kolmo_decode_limits *limits) {
    uint8_t hdr[KOLMO_STREAM_HDR]; size_t got = 0;
    if (io_read_full(in_fd, hdr, KOLMO_STREAM_HDR, &got)) return KOLMO_ERR_ALLOC;
    if (got < KOLMO_STREAM_HDR || memcmp(hdr, "CZNS", 4) || hdr[4] != 1) return KOLMO_ERR_CORRUPT;

    uint8_t *frame = NULL, *out = NULL; size_t fcap = 0, ocap = 0;
    int64_t total = 0, ret = 0;
    for (;;) {
        uint8_t lb[8]; size_t g2 = 0;
        if (io_read_full(in_fd, lb, 8, &g2)) { ret = KOLMO_ERR_ALLOC; break; }
        if (g2 == 0) { ret = KOLMO_ERR_CORRUPT; break; }  /* missing terminator */
        if (g2 < 8) { ret = KOLMO_ERR_CORRUPT; break; }
        uint64_t len = get64(lb);
        if (len == 0) { ret = total; break; }           /* clean end of stream */
        if (len > (1ull << 34)) { ret = KOLMO_ERR_CORRUPT; break; }
        size_t have = 0;
        while (have < len) {
            size_t want = len - have < KOLMO_STREAM_STEP ? (size_t)(len - have) : KOLMO_STREAM_STEP;
            if (have + want > fcap) { uint8_t *t = realloc(frame, have + want); if (!t) { ret = KOLMO_ERR_ALLOC; goto done; } frame = t; fcap = have + want; }
            size_t fg = 0;
            if (io_read_full(in_fd, frame + have, want, &fg)) { ret = KOLMO_ERR_ALLOC; goto done; }
            have += fg;
            if (fg < want) { ret = KOLMO_ERR_CORRUPT; goto done; }
        }
        /* Rule 1 (docs/MEMORY-MODEL.md): the frame is checked whole (header and index CRC, block sizes that add up
         * to the claimed size, payloads inside the frame, the declared decode figure against the limit) before any
         * output memory is reserved, and its output is then decoded in windows of whole blocks (at most one block per
         * thread, at least 64 MiB), written as they finish. A forged size claims nothing it cannot back. */
        int64_t osz = kolmo_original_size(frame, (size_t)len);
        if (osz < 0) { ret = osz; break; }
        if (limits && limits->max_output_bytes && (uint64_t)osz > limits->max_output_bytes - (uint64_t)total) { ret = KOLMO_ERR_LIMIT; break; }
        parsed_t P; int pr = parse(frame, (size_t)len, &P, limits);
        if (pr) { ret = pr; break; }
        uint64_t maxraw = 0; for (uint32_t i = 0; i < P.nb; i++) if (P.roff[i + 1] - P.roff[i] > maxraw) maxraw = P.roff[i + 1] - P.roff[i];
        int adm = frame_admit(&P, maxraw, kmem_limit_of(limits ? limits->max_memory : 0));
        if (adm) { free(P.poff); free(P.roff); ret = adm; break; }
        int nt = threads > 0 ? threads : default_threads();
        uint64_t win = maxraw * (uint64_t)nt; if (win < KOLMO_STREAM_STEP) win = KOLMO_STREAM_STEP;
        uint64_t want = maxraw > win ? maxraw : win; if (want > (uint64_t)osz) want = (uint64_t)osz;
        if (want > ocap) { free(out); ocap = 0; out = malloc(want ? (size_t)want : 1); if (!out) { free(P.poff); free(P.roff); ret = KOLMO_ERR_ALLOC; break; } ocap = (size_t)want; }
        int64_t dl = 0; uint32_t b = 0;
        while (b < P.nb && dl >= 0) {
            uint32_t e = b; uint64_t lo = P.roff[b];
            while (e < P.nb && (e == b || P.roff[e + 1] - lo <= ocap)) e++;
            uint64_t hi = P.roff[e];
            int64_t r = hi > lo ? run_decode(frame, -1, (size_t)len, out, -1, threads, lo, hi, 1, limits) : 0;
            if (r < 0) { dl = r; break; }
            if (r > 0 && io_write_full(out_fd, out, (size_t)r)) { dl = KOLMO_ERR_IO; break; }
            dl += r; b = e;
        }
        free(P.poff); free(P.roff);
        if (dl < 0) { ret = dl; break; }
        total += dl;
    }
done:
    free(frame); free(out);
    return ret;
}
int64_t kolmo_stream_decompress(int in_fd, int out_fd, int threads) { return kolmo_stream_decompress_limited(in_fd, out_fd, threads, NULL); }
