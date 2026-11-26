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

/* Streamed tar: segments end at member boundaries. A fixed cut splits a member between two frames and starts the next
 * segment in the middle of one, so the planner of that segment sees anonymous bytes instead of a tar (plan/meta.c
 * needs a ustar header at its start): silesia.tar in 64 MiB segments cost +3.9% to +6.3% at levels 1, 3 and 6 against
 * one frame, and member cuts recover all of it. The walker follows the headers across segments (a member may be longer
 * than a segment); a segment ends at the last header in it (before a pax or GNU long-name header that belongs to the
 * member after it) that leaves the segment at least a sixteenth full, else at the segment size as before. A segment
 * that starts inside a member is planned as that member's end, then a tar from its first header on (meta_tar_at).
 * Any header that does not check (checksum, magic, size) or the end-of-archive block stops the walker: the rest of
 * the input is cut at fixed sizes. Only where segments end and how they are planned change; every segment is an
 * ordinary frame (CZNS version 1). */
typedef struct { int on; uint64_t next, grp; uint64_t *hs; size_t nh, cap; } st_tar;   /* next: offset of the next header; grp: first
                                                                                         * header of its group; hs: the group starts walked, not yet coded */
static int st_tar_hdr(const uint8_t *h, uint64_t *size) {   /* 1: a ustar header, 0: anything else (end block included) */
    if (memcmp(h + 257, "ustar", 5)) return 0;
    unsigned sum = 0, want = 0; int digits = 0;
    for (int i = 0; i < 512; i++) sum += i >= 148 && i < 156 ? ' ' : h[i];
    for (int i = 148; i < 156; i++) { uint8_t c = h[i]; if (c == 0 || c == ' ') { if (digits) break; continue; } if (c < '0' || c > '7') return 0; want = want * 8 + (c - '0'); digits++; }
    if (!digits || sum != want) return 0;
    uint64_t z = 0;
    if (h[124] & 0x80) {                                    /* GNU base-256 size (members of 8 GiB or more) */
        if (h[124] != 0x80 || h[125] | h[126] | h[127]) return 0;
        for (int i = 128; i < 136; i++) z = z << 8 | h[i];
    } else {
        for (int i = 124; i < 136; i++) { uint8_t c = h[i]; if (c == 0 || c == ' ') continue; if (c < '0' || c > '7') return 0; z = z * 8 + (c - '0'); }
    }
    if (z > ((uint64_t)1 << 60)) return 0;
    *size = z; return 1;
}
/* buf[0, have) is the input from offset base; returns where the segment should end (have when no header fits) */
static size_t st_tar_cut(st_tar *T, const uint8_t *buf, uint64_t base, size_t have) {
    size_t cut = 0, least = have / 16;
    while (T->on && T->next >= base && T->next + 512 <= base + have) {
        const uint8_t *h = buf + (T->next - base); uint64_t size;
        if (!st_tar_hdr(h, &size)) { T->on = 0; break; }
        const uint8_t ty = h[156];
        const int ext = ty == 'x' || ty == 'g' || ty == 'L' || ty == 'K';   /* belongs to the member after it */
        if (T->grp == UINT64_MAX) T->grp = T->next;
        if (!ext) {
            if (T->grp > base && T->grp - base >= least) cut = (size_t)(T->grp - base);
            if (T->nh == T->cap) { size_t c = T->cap ? 2 * T->cap : 64; uint64_t *h2 = realloc(T->hs, c * sizeof *h2); if (!h2) { T->on = 0; break; } T->hs = h2; T->cap = c; }
            T->hs[T->nh++] = T->grp; T->grp = UINT64_MAX;
        }
        T->next += 512 + (size + 511) / 512 * 512;
    }
    if (T->on && T->grp != UINT64_MAX && T->grp > base && T->grp - base >= least && T->grp - base < have) cut = (size_t)(T->grp - base);
    return cut ? cut : have;
}

/* the first member header in [base, base + len) as an offset from base (0: none, or one at base itself); forgets the
 * headers before base + len */
static uint64_t st_tar_first(st_tar *T, uint64_t base, size_t len) {
    if (!T->nh) return 0;                               /* no header walked (not a tar): hs may be NULL */
    uint64_t k = 0; size_t i = 0;
    while (i < T->nh && T->hs[i] < base + len) { if (!k && T->hs[i] > base) k = T->hs[i] - base; if (T->hs[i] == base) k = UINT64_MAX; i++; }
    memmove(T->hs, T->hs + i, (T->nh - i) * sizeof *T->hs); T->nh -= i;
    return k == UINT64_MAX ? 0 : k;
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
    st_tar T = {-1, 0, UINT64_MAX, NULL, 0, 0};                     /* -1: not known yet (the first segment's start decides) */
    uint64_t base = 0; size_t have = 0; int eof = 0;
    for (;;) {
        size_t got = 0;
        if (!eof) {
            if (io_read_full(in_fd, in + have, (size_t)seg - have, &got)) { ret = KOLMO_ERR_IO; break; }
            have += got; eof = have < seg;               /* short read = end of input */
        }
        if (have == 0) break;                            /* clean EOF */
        if (T.on < 0) { uint64_t z; T.on = have >= 1024 && st_tar_hdr(in, &z); }
        size_t len = T.on > 0 && !eof ? st_tar_cut(&T, in, base, have) : have;
        if (eof && T.on > 0) st_tar_cut(&T, in, base, have);   /* the headers of the last segment */
        uint64_t k = st_tar_first(&T, base, len);               /* a segment inside a member: its tar starts at k */
        size_t bound = kolmo_bound(len, p);
        if (bound > outcap) { free(out); out = malloc(bound); if (!out) { ret = KOLMO_ERR_ALLOC; break; } outcap = bound; }
        int64_t cl = compress_cuts(in, -1, len, out, -1, outcap, p, NULL, 0, k);
        if (cl < 0) { ret = cl; break; }
        uint8_t lb[8]; put64(lb, (uint64_t)cl);
        if (io_write_full(out_fd, lb, 8) || io_write_full(out_fd, out, (size_t)cl)) { ret = KOLMO_ERR_IO; break; }
        total += 8 + cl;
        ret = total;
        memmove(in, in + len, have - len); have -= len; base += len;
        if (eof && have == 0) break;
    }
    free(in); free(out); free(T.hs);
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
/* decode one whole frame (and its extensions) held in frame[0, len) to the output */
static int sdec_frame(sdec_t *D, const uint8_t *frame, size_t len) {
    const kolmo_decode_limits *limits = D->limits;
    /* Rule 1 (docs/MEMORY-MODEL.md): the frame is checked whole (header and index CRC, block sizes that add up to the
     * claimed size, payloads inside the frame, the declared decode figure against the limit) before any output memory
     * is reserved, and its output is then decoded in windows of whole blocks, written as they finish. A forged size
     * claims nothing it cannot back. */
    int64_t osz = whole_frame(frame, len);                       /* exactly one frame: nothing after its payloads */
    if (osz < 0) return (int)osz;
    if (limits && limits->max_output_bytes && (uint64_t)osz > limits->max_output_bytes - (uint64_t)D->total) return KOLMO_ERR_LIMIT;
    parsed_t P; int pr = parse(frame, len, &P, limits);
    if (pr) return pr;
    uint64_t maxraw = 0; for (uint32_t i = 0; i < P.nb; i++) if (P.roff[i + 1] - P.roff[i] > maxraw) maxraw = P.roff[i + 1] - P.roff[i];
    uint64_t memcap = kmem_limit_of(limits ? limits->max_memory : 0);
    int adm = frame_admit(&P, maxraw, memcap);
    if (adm) { free(P.poff); free(P.roff); return adm; }
    /* Rule 2: the output window and the threads share the limit. Each thread keeps one block's buffers (`per`) once it
     * has decoded a block, whether or not it is decoding now (0.0.2 used every thread and a window of one block per
     * thread outside the limit: 1.3 GiB under -M 256M, review R23 #114 B2); a codec's own tables are reserved per
     * block by the governor inside run_decode. So: at most limit / per threads, and a window of one block per thread
     * (or 64 MiB) within what they leave, down to one thread and a window of one block (the output of that one block
     * is the only memory beyond the limit, as the one-shot decoder's output buffer is). */
    uint64_t per = blk_dec_floor(maxraw);
    int nt = D->threads > 0 ? D->threads : default_threads();
    if (memcap != KMEM_UNLIMITED) {
        uint64_t fit = memcap / per;
        if ((uint64_t)nt > fit) nt = fit ? (int)fit : 1;
    }
    uint64_t win = maxraw * (uint64_t)nt; if (win < KOLMO_STREAM_STEP) win = KOLMO_STREAM_STEP;
    if (memcap != KMEM_UNLIMITED) {
        uint64_t used = per * (uint64_t)nt, room = memcap > used ? memcap - used : 0;
        if (win > room) win = room;
    }
    if (win < maxraw) win = maxraw;
    if (win > (uint64_t)osz) win = (uint64_t)osz;
    if (win > D->ocap || (D->ocap > win && D->ocap - win > KOLMO_STREAM_STEP)) {   /* never hold more than this frame needs */
        free(D->out); D->ocap = 0; D->out = malloc(win ? (size_t)win : 1);
        if (!D->out) { free(P.poff); free(P.roff); return KOLMO_ERR_ALLOC; }
        D->ocap = (size_t)win;
    }
    int64_t dl = 0; uint32_t b = 0;
    while (b < P.nb && dl >= 0) {
        uint32_t e = b; uint64_t lo = P.roff[b];
        while (e < P.nb && (e == b || P.roff[e + 1] - lo <= win)) e++;
        uint64_t hi = P.roff[e];
        int64_t r = hi > lo ? run_decode(frame, -1, len, D->out, -1, nt, lo, hi, 1, limits) : 0;
        if (r < 0) { dl = r; break; }
        if (r > 0 && io_write_full(D->out_fd, D->out, (size_t)r)) { dl = KOLMO_ERR_IO; break; }
        dl += r; b = e;
    }
    free(P.poff); free(P.roff);
    if (dl < 0) return (int)dl;
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
        int r = sdec_frame(D, *frame, have);
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
    uint64_t o, e;                                             /* the end of the payloads, from the header and index */
    if ((t = frame_head(*frame, have, &o, &e))) return t;
    if (e < have) return KOLMO_ERR_CORRUPT;
    if ((t = sin_take(R, frame, fcap, &have, e - have))) return t;
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
    if (have > e) { uint64_t dl; int de; if (kx_find(*frame, have, e, "DDP1", &dl, &de)) return KOLMO_ERR_CORRUPT; }
    return sdec_frame(D, *frame, have);                         /* whole_frame checks the extensions and their trailer */
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
