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
 * A file may hold several members one after another (CZNS streams and CZN1 frames, FORMAT.md 10.1); the decoder reads
 * them all.
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
    if (!kolmo_params_valid(p)) return KOLMO_ERR_PARAM;
    uint64_t seg = seg_mib ? (uint64_t)seg_mib << 20 : KOLMO_STREAM_SEG_DEFAULT;
    if (seg < (1u << 20)) seg = 1u << 20;

    uint8_t hdr[KOLMO_STREAM_HDR] = {'C','Z','N','S', 1, 0, 0, 0};
    put32(hdr + 8, (uint32_t)(seg > 0xFFFFFFFF ? 0xFFFFFFFF : seg));
    if (io_write_full(out_fd, hdr, KOLMO_STREAM_HDR)) return KOLMO_ERR_IO;
    int64_t total = KOLMO_STREAM_HDR;

    uint8_t *in = malloc(seg);
    if (!in) return KOLMO_ERR_ALLOC;
    uint8_t *out = NULL; size_t outcap = 0;
    int64_t ret = total;
    for (;;) {
        size_t got = 0;
        if (io_read_full(in_fd, in, seg, &got)) { ret = KOLMO_ERR_IO; break; }
        if (got == 0) break;                           /* clean EOF */
        size_t bound = kolmo_bound(got, p);
        if (bound > outcap) { free(out); out = malloc(bound); if (!out) { ret = KOLMO_ERR_ALLOC; break; } outcap = bound; }
        int64_t cl = kolmo_compress(in, got, out, outcap, p);
        if (cl < 0) { ret = cl; break; }
        uint8_t lb[8]; put64(lb, (uint64_t)cl);
        if (io_write_full(out_fd, lb, 8) || io_write_full(out_fd, out, (size_t)cl)) { ret = KOLMO_ERR_IO; break; }
        total += 8 + cl;
        ret = total;
        if (got < seg) break;                          /* short read = last segment */
    }
    free(in); free(out);
    if (ret < 0) return ret;
    uint8_t z[8]; put64(z, 0);                          /* end-of-stream terminator */
    if (io_write_full(out_fd, z, 8)) return KOLMO_ERR_IO;
    return total + 8;
}

/* Decompress from in_fd to out_fd. Returns total bytes written, or a negative KOLMO_ERR_*.
 *
 * The input is a sequence of members (FORMAT.md section 10.1), decoded one after another as the CLI decodes
 * concatenated frames: a CZNS stream (header, length-prefixed frames, zero length) or a CZN1 frame (with its container
 * extensions, if any). The end of input after a whole member ends the call; anything else after a member (bytes that
 * do not start a member, a member cut short) is KOLMO_ERR_CORRUPT, never silently skipped. The bytes after a stream's
 * terminator used to be ignored (0.0.2 and earlier): appended --stream outputs lost their later parts.
 *
 * A frame's buffer grows only as its bytes arrive (at most KOLMO_STREAM_STEP ahead of them), so a forged length
 * cannot make the decoder reserve more memory than the input holds. `limits` (may be NULL) bounds each frame's decoded
 * size, block size and block count, the memory of each frame's decode (max_memory, or the process limit: the output
 * window and the blocks decoding at once share it), and the total output (max_output_bytes). */
#define KOLMO_STREAM_STEP (64u << 20)

/* the input: a descriptor, and the few bytes read ahead to find where a frame member ends */
typedef struct { int fd; uint8_t ahead[4]; size_t na; } sin_t;
static int sin_read(sin_t *r, uint8_t *buf, size_t n, size_t *got) {
    size_t k = r->na < n ? r->na : n, g = 0;
    memcpy(buf, r->ahead, k); memmove(r->ahead, r->ahead + k, r->na - k); r->na -= k;
    if (k < n && io_read_full(r->fd, buf + k, n - k, &g)) return -1;
    *got = k + g; return 0;
}
static void sin_unread(sin_t *r, const uint8_t *b, size_t n) { memcpy(r->ahead, b, n); r->na = n; }   /* r->na is 0 */
/* append len bytes of the input to *buf at *have; 0, KOLMO_ERR_IO, KOLMO_ERR_ALLOC or KOLMO_ERR_CORRUPT (input ends) */
static int sin_take(sin_t *r, uint8_t **buf, size_t *cap, size_t *have, uint64_t len) {
    if (len > SIZE_MAX - *have) return KOLMO_ERR_CORRUPT;
    while (len) {
        size_t want = len < KOLMO_STREAM_STEP ? (size_t)len : KOLMO_STREAM_STEP;
        if (*have + want > *cap) { uint8_t *t = realloc(*buf, *have + want); if (!t) return KOLMO_ERR_ALLOC; *buf = t; *cap = *have + want; }
        size_t g = 0;
        if (sin_read(r, *buf + *have, want, &g)) return KOLMO_ERR_IO;
        *have += g; len -= g;
        if (g < want) return KOLMO_ERR_CORRUPT;
    }
    return 0;
}

typedef struct { int out_fd, threads; const kolmo_decode_limits *limits; uint8_t *out; size_t ocap; int64_t total; } sdec_t;

static int sdec_window(sdec_t *D, uint64_t win) {
    if (win > SIZE_MAX) return KOLMO_ERR_ALLOC;
    if (D->ocap == win) return 0;
    free(D->out);
    D->ocap = 0;
    D->out = win ? malloc((size_t)win) : NULL;
    if (win && !D->out) return KOLMO_ERR_ALLOC;
    D->ocap = (size_t)win;
    return 0;
}

/* Finish validating one frame, build its shared offsets, and decode whole-block windows to the output. */
static int sdec_frame(sdec_t *D, const uint8_t *frame, size_t len, parsed_t *P) {
    const kolmo_decode_limits *limits = D->limits;
    if (!frame_tail_ok(frame, len, P->end)) return KOLMO_ERR_CORRUPT;
    if (limits && limits->max_output_bytes && P->orig > limits->max_output_bytes - (uint64_t)D->total) return KOLMO_ERR_LIMIT;
    P->idx = frame + HDR;          /* reading a CZN1 payload or its extensions may have moved the frame buffer */
    uint64_t memcap = kmem_limit_of(limits ? limits->max_memory : 0);
    uint64_t metadata = frame_offset_bytes(P->nb);
    uint64_t codec_cap = frame_codec_budget(memcap, metadata);
    int r = frame_prepare(P, limits, codec_cap);
    if (r) return r;
    /* The output window and the current decode share this cap. A single whole block can keep its output outside
     * the limit, as when the one-shot API decodes into a caller-owned buffer. */
    uint64_t maxraw = P->maxraw, per = blk_dec_floor(maxraw);
    int nt = D->threads > 0 ? D->threads : default_threads();
    if (codec_cap != KMEM_UNLIMITED) {
        uint64_t fit = (codec_cap > maxraw ? codec_cap - maxraw : 0) / per;
        if ((uint64_t)nt > fit) nt = fit ? (int)fit : 1;
    }
    uint64_t win = maxraw * (uint64_t)nt; if (win < KOLMO_STREAM_STEP) win = KOLMO_STREAM_STEP;
    int single = 0;
    if (codec_cap != KMEM_UNLIMITED) {
        uint64_t used = per * (uint64_t)nt;
        if (used < P->memd) used = P->memd;
        uint64_t room = codec_cap - used;
        if (win > room) win = room;
        if (win < maxraw) { win = maxraw; single = 1; }
    }
    if (win > P->orig) win = P->orig;
    r = sdec_window(D, win);
    if (r) { free(P->poff); free(P->roff); return r; }
    uint64_t window_cap = !single && memcap != KMEM_UNLIMITED ? memcap - D->ocap : memcap;
    int64_t dl = 0; uint32_t b = 0;
    while (b < P->nb) {
        uint32_t e = b + 1; uint64_t lo = P->roff[b];
        while (!single && e < P->nb && P->roff[e + 1] - lo <= win) e++;
        uint64_t hi = P->roff[e];
        if (hi == lo) { b = e; continue; }
        /* Match the range reader's treatment of zero-length blocks at window edges. */
        uint32_t first = b, last = e;
        while (P->roff[first + 1] == lo) first++;
        while (P->roff[last - 1] == hi) last--;
        int64_t d = run_decode_parsed(P, frame, -1, D->out, -1, single ? 1 : nt, first, last, lo, hi, 1,
                                      limits, window_cap, metadata);
        /* For frames without a declaration, a block may need more codec memory than the window left available. */
        if (d == KOLMO_ERR_MEMORY && window_cap < memcap) {
            win = maxraw;
            r = sdec_window(D, win);
            if (r) break;
            window_cap = memcap;
            single = 1;
            continue;
        }
        if (d < 0) { r = (int)d; break; }
        if (io_write_full(D->out_fd, D->out, (size_t)d)) { r = KOLMO_ERR_IO; break; }
        dl += d; b = e;
    }
    free(P->poff); free(P->roff);
    if (r) return r;
    D->total += dl;
    return 0;
}

/* a CZNS member, its 4 magic bytes read: the header, then length-prefixed frames up to the zero length */
static int sdec_stream(sdec_t *D, sin_t *R, uint8_t **frame, size_t *fcap) {
    uint8_t hdr[KOLMO_STREAM_HDR - 4]; size_t got = 0;
    if (sin_read(R, hdr, sizeof hdr, &got)) return KOLMO_ERR_IO;
    /* version 1. The three reserved bytes and segment_bytes are not read (COMPATIBILITY.md: reserved bytes are ignored;
     * the segment size is informational, and the frames carry their own sizes). */
    if (got < sizeof hdr || hdr[0] != 1) return KOLMO_ERR_CORRUPT;
    for (;;) {
        uint8_t lb[8]; size_t g2 = 0;
        if (sin_read(R, lb, 8, &g2)) return KOLMO_ERR_IO;
        if (g2 < 8) return KOLMO_ERR_CORRUPT;                  /* missing terminator */
        uint64_t len = get64(lb);
        if (len == 0) return 0;                                /* the end of this stream (a member may follow) */
        if (len > (1ull << 34)) return KOLMO_ERR_CORRUPT;
        size_t have = 0;
        int t = sin_take(R, frame, fcap, &have, len);
        if (t) return t;
        parsed_t P;
        int r = frame_scan(*frame, have, &P);
        if (r) return r;
        r = sdec_frame(D, *frame, have, &P);
        if (r) return r;
    }
}

/* a CZN1 member, its 4 magic bytes read: header, index, payloads, then extension sections and their KEXT trailer when
 * the next bytes are not the next member or the end (as the CLI's frame walk reads them) */
static int sdec_czn1(sdec_t *D, sin_t *R, uint8_t **frame, size_t *fcap) {
    size_t have = 0; int t;
    if (*fcap < 32) { uint8_t *q = realloc(*frame, 32); if (!q) return KOLMO_ERR_ALLOC; *frame = q; *fcap = 32; }
    memcpy(*frame, "CZN1", 4); have = 4;
    if ((t = sin_take(R, frame, fcap, &have, 28))) return t;
    if ((t = frame_version_check(*frame, have))) return t;      /* KOLMO_ERR_LEGACY before the index is read */
    uint64_t nb = get32(*frame + 20);
    if ((t = sin_take(R, frame, fcap, &have, 16 * nb))) return t;
    parsed_t P;                                               /* compute the payload end from the validated index */
    if ((t = frame_scan(*frame, have, &P))) return t;
    if (P.end < have) return KOLMO_ERR_CORRUPT;
    if ((t = sin_take(R, frame, fcap, &have, P.end - have))) return t;
    for (int k = 0; k < 64; k++) {
        uint8_t m[4]; size_t g = 0;
        if (sin_read(R, m, 4, &g)) return KOLMO_ERR_IO;
        if (g < 4 || !memcmp(m, "CZN1", 4) || !memcmp(m, "CZNS", 4)) { sin_unread(R, m, g); break; }   /* the end, or the next member */
        if (have + 4 > *fcap) { uint8_t *q = realloc(*frame, have + 4); if (!q) return KOLMO_ERR_ALLOC; *frame = q; *fcap = have + 4; }
        memcpy(*frame + have, m, 4); have += 4;
        if (!memcmp(m, "KEXT", 4)) { if ((t = sin_take(R, frame, fcap, &have, 20))) return t; break; }   /* the trailer */
        if ((t = sin_take(R, frame, fcap, &have, 8))) return t;      /* a section: tag, length, body, CRC */
        if ((t = sin_take(R, frame, fcap, &have, get64(*frame + have - 8) + 4))) return t;
    }
    /* a `kolmo archive` frame (a DDP1 section) holds a chunk store, not one file: as -d refuses it, it is no member to
     * decode here (a frame member is new in this decoder, so nothing that decoded before is refused) */
    if (have > P.end) { uint64_t dl; int de; if (kx_find(*frame, have, P.end, "DDP1", &dl, &de)) return KOLMO_ERR_CORRUPT; }
    return sdec_frame(D, *frame, have, &P);                     /* sdec_frame checks the extensions and their trailer */
}

int64_t kolmo_stream_decompress_limited(int in_fd, int out_fd, int threads, const kolmo_decode_limits *limits) {
    sin_t R = {in_fd, {0}, 0};
    sdec_t D = {out_fd, threads, limits, NULL, 0, 0};
    uint8_t *frame = NULL; size_t fcap = 0;
    int64_t ret = 0;
    for (int members = 0;; members++) {
        uint8_t m[4]; size_t g = 0; int r;
        if (sin_read(&R, m, 4, &g)) { ret = KOLMO_ERR_IO; break; }
        if (g == 0 && members) { ret = D.total; break; }        /* the end of the input, after a whole member */
        if (g < 4) { ret = KOLMO_ERR_CORRUPT; break; }
        if (!memcmp(m, "CZNS", 4)) r = sdec_stream(&D, &R, &frame, &fcap);
        else if (!memcmp(m, "CZN1", 4)) r = sdec_czn1(&D, &R, &frame, &fcap);
        else r = KOLMO_ERR_CORRUPT;                              /* bytes after a member that start no member */
        if (r) { ret = r; break; }
    }
    free(frame); free(D.out);
    return ret;
}
int64_t kolmo_stream_decompress(int in_fd, int out_fd, int threads) { return kolmo_stream_decompress_limited(in_fd, out_fd, threads, NULL); }
