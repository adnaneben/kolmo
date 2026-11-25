/* Metadata: structure read from the input's own headers (docs/METADATA.md).
 *
 * Kolmo infers structure by sampling bytes. Many formats describe themselves: an image header gives its width and depth,
 * a tensor file its tensors' types and offsets, an archive its members, an executable its code and data sections. This
 * file reads those headers, and only those: the decision depends on the input's bytes alone (never on a file name), so
 * the same bytes give the same frame whatever they are called.
 *
 * Every parser here reads untrusted input: offsets and sizes are checked against n before use, counts are capped, and
 * a header that does not parse leaves the input to the sampling analysis (the result is a hint, never required to
 * decode). Nothing here changes the container format: segments only move block boundaries and give each part its own
 * plan, and a packed segment's blocks are stored, as an incompressible block always was.
 *
 *   meta_segments   the input cut into parts with their own plans: tar members (moved from transforms/tar.c), zip and
 *                   NPZ members, ELF code and data sections, safetensors and GGUF tensors; and packed (already
 *                   compressed) content
 *   meta_image8     8-bit image geometry (row bytes, channels, pixel offset) from NPY, PNM/PAM, BMP and TIFF headers
 *   meta_packed     a magic number of compressed content (gzip, xz, zstd, bzip2, PNG, JPEG, ...)
 *   meta_incompressible  the probe that lets a packed block be stored without running its coder */

enum { MK_DATA = 0, MK_PACKED = 1, MK_CODE = 2, MK_HEADER = 3, MK_A64 = 4, MK_TENSOR = 8 };   /* MK_A64: ARM64 code; bits 8-19 of a kind: its page phase */
#define META_MINSEG ((uint64_t)64 << 10)   /* parts shorter than this join the part before them */

static inline uint16_t mt_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t mt_le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint64_t mt_le64(const uint8_t *p) { return (uint64_t)mt_le32(p) | (uint64_t)mt_le32(p + 4) << 32; }
static inline uint16_t mt_be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline uint32_t mt_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* ---- packed content ---- */

/* 1 when s starts with the magic number of compressed content. Formats whose payload is mostly compressed: a stream
 * compressor's frame, or an image / audio / video / archive format that compresses its payload. */
static int meta_packed(const uint8_t *s, size_t n) {
    if (n < 16) return 0;
    if (s[0] == 0x1F && s[1] == 0x8B && s[2] == 8) return 1;                           /* gzip */
    if (!memcmp(s, "\xFD" "7zXZ\0", 6)) return 1;                                      /* xz */
    if (!memcmp(s, "\x28\xB5\x2F\xFD", 4)) return 1;                                   /* zstd */
    if (!memcmp(s, "BZh", 3) && s[3] >= '1' && s[3] <= '9' && !memcmp(s + 4, "\x31\x41\x59\x26\x53\x59", 6)) return 1;   /* bzip2 */
    if (!memcmp(s, "\x04\x22\x4D\x18", 4)) return 1;                                   /* LZ4 frame */
    if (!memcmp(s, "7z\xBC\xAF\x27\x1C", 6)) return 1;                                 /* 7z */
    if (!memcmp(s, "Rar!\x1A\x07", 6)) return 1;                                       /* RAR */
    if (!memcmp(s, "\x89PNG\r\n\x1A\n", 8)) return 1;                                  /* PNG */
    if (s[0] == 0xFF && s[1] == 0xD8 && s[2] == 0xFF) return 1;                         /* JPEG */
    if (!memcmp(s, "RIFF", 4) && !memcmp(s + 8, "WEBP", 4)) return 1;                   /* WebP */
    if (!memcmp(s, "\0\0\0\x0CjXL \r\n\x87\n", 12) || (s[0] == 0xFF && s[1] == 0x0A)) return 1;   /* JPEG XL */
    if (!memcmp(s + 4, "ftyp", 4) && (!memcmp(s + 8, "avif", 4) || !memcmp(s + 8, "heic", 4) || !memcmp(s + 8, "isom", 4)
                                      || !memcmp(s + 8, "mp4", 3) || !memcmp(s + 8, "M4A ", 4) || !memcmp(s + 8, "qt  ", 4))) return 1;   /* ISO media */
    if (!memcmp(s, "fLaC", 4) || !memcmp(s, "OggS", 4)) return 1;                      /* FLAC, Ogg */
    if (!memcmp(s, "ID3", 3) || (s[0] == 0xFF && (s[1] & 0xE0) == 0xE0 && (s[1] & 6))) return 1;   /* MP3 */
    if (!memcmp(s, "\x1A\x45\xDF\xA3", 4)) return 1;                                   /* Matroska / WebM */
    if (!memcmp(s, "%PDF", 4)) return 0;                                               /* PDF: streams compressed, text around: left to sampling */
    return 0;
}

/* A block of packed content is stored without running its coder when it looks incompressible: on every 16 KiB slice
 * of up to 8 spread over the block, the collision entropy -log2(sum p^2) of the bytes is at least 7.8 bits (in integers:
 * 222 * sum c^2 <= SL^2, so the decision is exact on every platform), and the size probe (kprobe_size, KLZH2 level 1;
 * LZ4 before version 12) gains less than 1% (it would find a repeated member). Under the strong coders this skips seconds
 * of work per MB. */
static int meta_incompressible(const uint8_t *s, size_t n) {
    if (n < 4096) return 0;
    const size_t SL = n < 16384 ? n : 16384; const int k = n >= 8 * SL ? 8 : (int)(n / SL);
    for (int q = 0; q < k; q++) {
        const uint8_t *p = s + (k > 1 ? (n - SL) / (size_t)(k - 1) * (size_t)q : 0); uint32_t c[256] = {0};
        for (size_t i = 0; i < SL; i++) c[p[i]]++;
        uint64_t sq = 0; for (int b = 0; b < 256; b++) sq += (uint64_t)c[b] * c[b];
        if (222 * sq > (uint64_t)SL * SL) return 0;
    }
    size_t z = kprobe_size(s, n);
    return z * 100 >= n * 99;
}

/* ---- audio ---- */

/* AIFF (and AIFF-C with 'NONE' or 'twos' compression), 16-bit PCM, mono or stereo: *ch channels, sample frames in
 * [*off, *end). The audio route (codec 19) codes the samples byte-swapped; the frame records it (plan prefix 0, 1). */
static int meta_aiff(const uint8_t *s, size_t n, unsigned *ch, uint64_t *off, uint64_t *end) {
    if (!s || n < 54 || memcmp(s, "FORM", 4) || (memcmp(s + 8, "AIFF", 4) && memcmp(s + 8, "AIFC", 4))) return 0;
    const int aifc = s[11] == 'C'; uint64_t p = 12, frames = 0, doff = 0, dlen = 0; unsigned c = 0, bits = 0; int comm = 0;
    while (p + 8 <= n) {
        uint64_t l = mt_be32(s + p + 4); if (l > n - p - 8) l = n - p - 8;   /* a chunk may claim more than the file holds */
        if (!memcmp(s + p, "COMM", 4) && l >= 18) {
            c = mt_be16(s + p + 8); frames = mt_be32(s + p + 10); bits = mt_be16(s + p + 14); comm = 1;
            if (aifc && (l < 22 || (memcmp(s + p + 26, "NONE", 4) && memcmp(s + p + 26, "twos", 4)))) return 0;
        } else if (!memcmp(s + p, "SSND", 4) && l >= 8) { doff = p + 16 + mt_be32(s + p + 8); dlen = l - 8; if (doff > p + 8 + l) return 0; dlen = p + 8 + l - doff; }
        p += 8 + l + (l & 1);
    }
    if (!comm || !doff || bits != 16 || (c != 1 && c != 2) || !frames || frames > dlen / (2 * c)) return 0;
    *ch = c; *off = doff; *end = doff + frames * 2 * c;
    return 1;
}

/* ---- 8-bit image geometry ---- */

/* PNM / PAM header token: skips whitespace and # comments, reads a decimal number; 0 on error */
static size_t mt_pnm_num(const uint8_t *s, size_t n, size_t *i, uint64_t *v) {
    while (*i < n) {
        if (s[*i] == '#') { while (*i < n && s[*i] != '\n') (*i)++; }
        else if (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\r' || s[*i] == '\n') (*i)++;
        else break;
    }
    uint64_t x = 0; size_t d = 0;
    while (*i < n && s[*i] >= '0' && s[*i] <= '9' && d < 10) { x = x * 10 + (uint64_t)(s[*i] - '0'); (*i)++; d++; }
    *v = x; return d;
}

/* Geometry of an 8-bit image stored row by row without compression, from its header: *W row bytes (width x channels,
 * with the row padding of BMP), *C bytes per pixel (1, 3 or 4; 2 for grey + alpha), *h the offset of the first pixel.
 * Returns 0 for anything else (16-bit samples go to the IMG16 trial, palettes, compression, planar TIFF). */
static int meta_image8(const uint8_t *s, size_t n, size_t *W, unsigned *C, size_t *h) {
    if (!s || n < 64) return 0;
    uint64_t w = 0, ht = 0, c = 0, off = 0, stride = 0;
    if (!memcmp(s, "\x93NUMPY", 6)) {                         /* NPY uint8, C order, shape (H, W) or (H, W, C) */
        size_t hl = s[6] == 1 ? 10 + (size_t)mt_le16(s + 8) : 12 + (size_t)mt_le32(s + 8);
        if (hl > n) return 0;
        const uint8_t *d = memmem(s, hl, "'descr': '", 10), *sh = memmem(s, hl, "'shape': (", 10);
        if (!d || !sh || d + 13 > s + hl || d[11] != 'u' || d[12] != '1' || memmem(s, hl, "'fortran_order': True", 21)) return 0;
        uint64_t dims[4]; int nd = 0; const uint8_t *q = sh + 10;
        while (q < s + hl && *q != ')' && nd < 4) { while (q < s + hl && (*q == ' ' || *q == ',')) q++; if (q >= s + hl || *q == ')') break; uint64_t v = 0; int dg = 0; while (q < s + hl && *q >= '0' && *q <= '9' && dg < 18) { v = v * 10 + (uint64_t)(*q++ - '0'); dg++; } if (!dg) return 0; dims[nd++] = v; }
        if (nd == 2) { w = dims[1]; c = 1; } else if (nd == 3 && dims[2] >= 1 && dims[2] <= 4) { w = dims[1]; c = dims[2]; } else return 0;
        off = hl; stride = w * c; ht = (n - off) / (stride ? stride : 1);
    } else if (s[0] == 'P' && (s[1] == '5' || s[1] == '6')) {   /* binary PGM / PPM, maxval < 256 */
        size_t i = 2; uint64_t mv = 0;
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n' && s[i] != '#') return 0;
        if (!mt_pnm_num(s, n, &i, &w) || !mt_pnm_num(s, n, &i, &ht) || !mt_pnm_num(s, n, &i, &mv)) return 0;
        if (i >= n || (s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n') || mv == 0 || mv > 255) return 0;
        c = s[1] == '6' ? 3 : 1; off = i + 1; stride = w * c;
    } else if (!memcmp(s, "P7\n", 3)) {                         /* PAM */
        const uint8_t *e = memmem(s, n < 1024 ? n : 1024, "ENDHDR\n", 7); if (!e) return 0;
        size_t hl = (size_t)(e - s) + 7, i; uint64_t mv = 0;
        const uint8_t *k;
        if ((k = memmem(s, hl, "WIDTH ", 6))) { i = (size_t)(k - s) + 6; mt_pnm_num(s, hl, &i, &w); }
        if ((k = memmem(s, hl, "HEIGHT ", 7))) { i = (size_t)(k - s) + 7; mt_pnm_num(s, hl, &i, &ht); }
        if ((k = memmem(s, hl, "DEPTH ", 6))) { i = (size_t)(k - s) + 6; mt_pnm_num(s, hl, &i, &c); }
        if ((k = memmem(s, hl, "MAXVAL ", 7))) { i = (size_t)(k - s) + 7; mt_pnm_num(s, hl, &i, &mv); }
        if (mv == 0 || mv > 255 || c < 1 || c > 4) return 0;
        off = hl; stride = w * c;
    } else if (s[0] == 'B' && s[1] == 'M' && n >= 54) {          /* BMP, BITMAPINFOHEADER or later, BI_RGB / BI_BITFIELDS */
        uint32_t po = mt_le32(s + 10), hs = mt_le32(s + 14);
        if (hs < 40 || hs > 256 || 14 + (uint64_t)hs > n) return 0;
        int32_t bw = (int32_t)mt_le32(s + 18), bh = (int32_t)mt_le32(s + 22); uint16_t planes = mt_le16(s + 26), bpp = mt_le16(s + 28);
        uint32_t comp = mt_le32(s + 30);
        if (planes != 1 || bw <= 0 || bh == 0 || bh == INT32_MIN || (bpp != 8 && bpp != 24 && bpp != 32) || (comp != 0 && !(comp == 3 && bpp == 32))) return 0;
        w = (uint64_t)bw; ht = (uint64_t)(bh < 0 ? -(int64_t)bh : bh); c = bpp / 8; off = po;
        stride = (w * c + 3) / 4 * 4;                            /* rows are padded to 4 bytes */
    } else if ((!memcmp(s, "II*\0", 4) || !memcmp(s, "MM\0*", 4))) {   /* baseline TIFF, one IFD: uncompressed, chunky, 8 bits */
        const int be = s[0] == 'M';
        #define T16(p) (be ? mt_be16(p) : mt_le16(p))
        #define T32(p) (be ? mt_be32(p) : mt_le32(p))
        uint64_t ifd = T32(s + 4); if (ifd + 2 > n) return 0;
        unsigned ne = T16(s + ifd); if (ifd + 2 + 12 * (uint64_t)ne > n || ne > 512) return 0;
        uint64_t comp = 1, planar = 1, bps = 0, nso = 0, so = 0; int sotype = 0;
        for (unsigned k = 0; k < ne; k++) {
            const uint8_t *e = s + ifd + 2 + 12 * (uint64_t)k; unsigned tag = T16(e), ty = T16(e + 2); uint64_t cnt = T32(e + 4);
            if (ty != 3 && ty != 4) continue;
            uint64_t v = ty == 3 ? T16(e + 8) : T32(e + 8);      /* the value, or for SHORTs that do not fit, an offset */
            if (tag == 258 && ty == 3 && cnt > 2) { uint64_t vo = T32(e + 8); if (vo > n - 2) return 0; v = T16(s + vo); }
            switch (tag) {
                case 256: w = v; break; case 257: ht = v; break; case 258: bps = v; break; case 259: comp = v; break;
                case 277: c = v; break; case 284: planar = v; break;
                case 273: nso = cnt; sotype = (int)ty; so = cnt == 1 ? v : T32(e + 8); break;
            }
        }
        if (comp != 1 || bps != 8 || planar != 1 || c < 1 || c > 4 || !nso || nso > n) return 0;
        stride = w * c;
        if (nso == 1) off = so;                                   /* one strip: its offset */
        else {                                                    /* several strips: the first offset (they follow each other in */
            const unsigned es = sotype == 3 ? 2 : 4;              /* the files that matter; otherwise the trial scores it worse) */
            if (so > n || nso * es > n - so) return 0;
            off = sotype == 3 ? T16(s + so) : T32(s + so);
        }
        #undef T16
        #undef T32
    } else return 0;
    if (!w || !c || stride < 2 || stride > ((uint64_t)1 << 24) || off >= n || ht < 4 || (n - off) / stride < 4) return 0;
    *W = (size_t)stride; *C = (unsigned)c; *h = (size_t)off;
    return 1;
}

/* ---- segments ---- */

typedef struct { uint64_t *ss, *se; uint32_t *kind; uint32_t k, maxs; size_t exemin; } mt_segs;   /* kind: MK_* | phase << 8; exemin: least executable size cut into parts */
/* append part [a, b) of a kind; a part shorter than META_MINSEG, or one that may join and has the previous part's
 * kind, extends the previous part. Parts must come in order; a gap becomes a data part. */
static void mt_emit(mt_segs *g, uint64_t a, uint64_t b, uint32_t kind, int join) {
    if (b <= a) return;
    if (g->k && a > g->se[g->k - 1]) mt_emit(g, g->se[g->k - 1], a, MK_DATA, 1);
    if (g->k && a < g->se[g->k - 1]) return;                   /* overlap: ignored (the caller keeps order) */
    if (g->k && (b - a < META_MINSEG || (join && g->kind[g->k - 1] == kind) || g->k + 1 >= g->maxs)) { g->se[g->k - 1] = b; return; }
    if (g->k && g->se[g->k - 1] - g->ss[g->k - 1] < META_MINSEG && g->kind[g->k - 1] != MK_HEADER) {   /* a short part before: joins this one */
        g->se[g->k - 1] = b; g->kind[g->k - 1] = kind; return;
    }
    g->ss[g->k] = a; g->se[g->k] = b; g->kind[g->k] = kind; g->k++;
}

/* tar (ustar): regular-file members of at least 1 MiB become their own segments, and so do members of at least
 * 64 KiB whose header Kolmo reads (an image, NPY, WAV, an executable, a tensor file, a zip, packed content: their
 * part then starts at that header). Such a member is cut further by its own format (sections, tensors, members). The
 * rest between them is one segment, as in rc2. */
static int mt_nested(const uint8_t *s, size_t n, mt_segs *g);
static int mt_known(const uint8_t *s, size_t n) {
    size_t w, h; unsigned c;
    return meta_packed(s, n) || meta_image8(s, n, &w, &c, &h) || (n >= 16 && (!memcmp(s, "\x93NUMPY", 6) || !memcmp(s, "\x7F" "ELF", 4)
        || !memcmp(s, "GGUF", 4) || !memcmp(s, "PK\3\4", 4) || mt_le32(s) == 0xFEEDFACFu || mt_be32(s) == 0xCAFEBABEu || (s[0] == 'M' && s[1] == 'Z') || (!memcmp(s, "RIFF", 4) && !memcmp(s + 8, "WAVE", 4)) || (s[8] == '{' && mt_le64(s) < n)));
}
/* one archive member whose data is s[off, off + size): a regular file of at least 1 MiB, or of at least 64 KiB whose
 * header Kolmo reads, becomes its own part (cut further by its own format); everything since *last (headers, smaller
 * members) becomes one part before it */
static void mt_member(const uint8_t *s, uint64_t off, uint64_t size, int reg, mt_segs *g, uint64_t *last) {
    const uint8_t *m = s + off;
    const int known = reg && size >= META_MINSEG && mt_known(m, size);
    if (!reg || !(size >= (1u << 20) || known)) return;
    if (off > *last) { g->ss[g->k] = *last; g->se[g->k] = off; g->kind[g->k] = MK_DATA; g->k++; }
    uint32_t k0 = g->k; mt_segs t = {g->ss + k0, g->se + k0, g->kind + k0, 0, g->maxs - k0 - 1, g->exemin};
    if (known && t.maxs >= 4 && mt_nested(m, size, &t) && t.k) {     /* the member cut by its own format */
        t.se[t.k - 1] = size;
        for (uint32_t i = 0; i < t.k; i++) { t.ss[i] += off; t.se[i] += off; }
        g->k += t.k;
    } else { g->ss[g->k] = off; g->se[g->k] = off + size; g->kind[g->k] = meta_packed(m, size) ? MK_PACKED : MK_DATA; g->k++; }
    *last = off + size;
}
static int mt_tar(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < 1024 || memcmp(s + 257, "ustar", 5)) return 0;
    uint64_t off = 0, last = 0;
    while (off + 512 <= n && g->k + 2 < g->maxs) {
        const uint8_t *h = s + off; int zero = 1; for (int i = 0; i < 512 && zero; i++) zero = !h[i];
        if (zero) break;
        if (h[124] & 0x80) break;
        uint64_t size = 0; int ok = 1;
        for (int i = 124; i < 136; i++) { uint8_t c = h[i]; if (c == 0 || c == ' ') continue; if (c < '0' || c > '7') { ok = 0; break; } size = size * 8 + (c - '0'); }
        if (!ok || size > n || off + 512 + size > n) break;
        mt_member(s, off + 512, size, h[156] == '0' || h[156] == 0, g, &last);
        off += 512 + (size + 511) / 512 * 512;
    }
    if (last < n || g->k == 0) { g->ss[g->k] = last; g->se[g->k] = n; g->kind[g->k] = MK_DATA; g->k++; }
    return 1;
}

/* zip (and NPZ): members from the central directory; stored members are parts with their own plans (an NPY member
 * then starts its part, so its header gives the array's type and shape), compressed members are packed parts */
typedef struct { uint64_t a, b; uint8_t kind; } mt_part;
static int mt_part_cmp(const void *x, const void *y) { const mt_part *p = x, *q = y; return p->a < q->a ? -1 : p->a > q->a; }
static int mt_zip(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < 22 + 30 || memcmp(s, "PK\3\4", 4)) return 0;
    size_t lim = n > 65557 ? n - 65557 : 0, e = n - 22; int found = 0;
    for (;; e--) { if (!memcmp(s + e, "PK\5\6", 4)) { found = 1; break; } if (e == lim) break; }
    if (!found) return 0;
    uint64_t cnt = mt_le16(s + e + 10), cdsz = mt_le32(s + e + 12), cdoff = mt_le32(s + e + 16);
    if (cdoff == 0xFFFFFFFFu || cnt == 0xFFFF) {                 /* zip64 end record, through its locator */
        if (e < 20 || memcmp(s + e - 20, "PK\6\7", 4)) return 0;
        uint64_t z = mt_le64(s + e - 20 + 8); if (z > n - 56 || memcmp(s + z, "PK\6\6", 4)) return 0;
        cnt = mt_le64(s + z + 32); cdsz = mt_le64(s + z + 40); cdoff = mt_le64(s + z + 48);
    }
    if (cdoff > n || cdsz > n - cdoff || cnt == 0 || cnt > 65536) return 0;
    mt_part *P = malloc(cnt * sizeof *P); if (!P) return 0;
    size_t np = 0, p = cdoff;
    for (uint64_t i = 0; i < cnt; i++) {
        if (p + 46 > cdoff + cdsz || memcmp(s + p, "PK\1\2", 4)) break;
        unsigned method = mt_le16(s + p + 10), nl = mt_le16(s + p + 28), xl = mt_le16(s + p + 30), cl = mt_le16(s + p + 32);
        uint64_t csz = mt_le32(s + p + 20), usz = mt_le32(s + p + 24), lo = mt_le32(s + p + 42);
        if (p + 46 + (uint64_t)nl + xl + cl > cdoff + cdsz) break;
        for (size_t x = p + 46 + nl, xe = x + xl; x + 4 <= xe; ) {   /* zip64 extra field: the 0xFFFFFFFF sizes in order */
            unsigned id = mt_le16(s + x), sz = mt_le16(s + x + 2); if (x + 4 + sz > xe) break;
            if (id == 1) { size_t q = x + 4, qe = q + sz;
                if (usz == 0xFFFFFFFFu && q + 8 <= qe) { usz = mt_le64(s + q); q += 8; }
                if (csz == 0xFFFFFFFFu && q + 8 <= qe) { csz = mt_le64(s + q); q += 8; }
                if (lo == 0xFFFFFFFFu && q + 8 <= qe) { lo = mt_le64(s + q); q += 8; } }
            x += 4 + sz;
        }
        p += 46 + (size_t)nl + xl + cl;
        if (lo > n - 30 || memcmp(s + lo, "PK\3\4", 4)) continue;
        uint64_t d = lo + 30 + (uint64_t)mt_le16(s + lo + 26) + mt_le16(s + lo + 28);
        if (d > n || csz > n - d || !csz) continue;
        P[np].a = d; P[np].b = d + csz; P[np].kind = method == 0 ? (meta_packed(s + d, csz) ? MK_PACKED : MK_DATA) : MK_PACKED; np++;
    }
    qsort(P, np, sizeof *P, mt_part_cmp);
    int any = 0;
    for (size_t i = 0; i < np; i++) {
        if (P[i].b - P[i].a < META_MINSEG) continue;             /* small members stay with the headers around them */
        if (g->k && P[i].a < g->se[g->k - 1]) continue;
        mt_emit(g, g->k ? g->se[g->k - 1] : 0, P[i].a, MK_HEADER, 1);
        mt_emit(g, P[i].a, P[i].b, P[i].kind, P[i].kind == MK_PACKED);
        any = 1;
    }
    free(P);
    if (!any) { g->k = 0; return 0; }
    mt_emit(g, g->se[g->k - 1], n, MK_HEADER, 1);
    return 1;
}

/* Executables: code sections and the rest as separate parts, so a branch transform is chosen for the code alone and
 * the data (read-only data, unwind tables, symbols) gets its own plan. x86 code is left to the planner's call/jump
 * trial; ARM64 code (MK_A64) takes the ARM64 branch transform, with its page phase (offset modulo 4096 in the
 * executable, which the loader maps page-aligned) in bits 8-19 of the kind. */
static int mt_code_parts(const uint8_t *s, size_t n, mt_segs *g, mt_part *P, size_t np, uint32_t ck) {
    (void)s; qsort(P, np, sizeof *P, mt_part_cmp);
    uint64_t pos = 0;
    for (size_t i = 0; i < np; i++) {
        if (P[i].a < pos) continue;
        mt_emit(g, pos, P[i].a, MK_DATA, 1); mt_emit(g, P[i].a, P[i].b, ck, 1); pos = P[i].b;
    }
    if (!np) return 0;
    mt_emit(g, pos, n, MK_DATA, 1);
    for (uint32_t i = 0; i < g->k; i++) if ((g->kind[i] & 0xFF) == MK_A64) g->kind[i] = MK_A64 | (uint32_t)(g->ss[i] & 0xFFF) << 8;
    return g->k > 1 || (g->k == 1 && (g->kind[0] & 0xFF) == MK_A64);
}
/* ELF, 32 and 64-bit little-endian: sections with SHF_EXECINSTR */
static int mt_elf(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < g->exemin || memcmp(s, "\x7F" "ELF", 4) || s[5] != 1 || (s[4] != 1 && s[4] != 2)) return 0;
    const int is64 = s[4] == 2; const unsigned mach = mt_le16(s + 18);
    uint64_t shoff = is64 ? mt_le64(s + 0x28) : mt_le32(s + 0x20);
    unsigned shes = mt_le16(s + (is64 ? 0x3A : 0x2E)), shn = mt_le16(s + (is64 ? 0x3C : 0x30));
    if (shes != (is64 ? 64u : 40u) || !shn || shn > 4096 || shoff > n || (uint64_t)shn * shes > n - shoff) return 0;
    mt_part *P = malloc(shn * sizeof *P); if (!P) return 0; size_t np = 0;
    for (unsigned i = 0; i < shn; i++) {
        const uint8_t *h = s + shoff + (uint64_t)i * shes;
        uint32_t ty = mt_le32(h + 4); uint64_t fl = is64 ? mt_le64(h + 8) : mt_le32(h + 8);
        uint64_t off = is64 ? mt_le64(h + 0x18) : mt_le32(h + 0x10), sz = is64 ? mt_le64(h + 0x20) : mt_le32(h + 0x14);
        if (ty == 8 || !sz || off > n || sz > n - off || !(fl & 4)) continue;   /* NOBITS, empty, outside, or not code */
        P[np].a = off; P[np].b = off + sz; np++;
    }
    int r = mt_code_parts(s, n, g, P, np, mach == 183 ? MK_A64 : MK_CODE);   /* EM_AARCH64 */
    free(P); return r;
}
/* Mach-O 64-bit (thin): sections of LC_SEGMENT_64 commands with S_ATTR_PURE_INSTRUCTIONS or S_ATTR_SOME_INSTRUCTIONS */
static int mt_macho(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < g->exemin || mt_le32(s) != 0xFEEDFACFu) return 0;
    const uint32_t cpu = mt_le32(s + 4), ncmd = mt_le32(s + 16), cmdsz = mt_le32(s + 20);
    if (ncmd > 4096 || cmdsz > n - 32) return 0;
    size_t cap = 64, np = 0; mt_part *P = malloc(cap * sizeof *P); if (!P) return 0;
    uint64_t p = 32, e = 32 + (uint64_t)cmdsz;
    for (uint32_t c = 0; c < ncmd && p + 8 <= e; c++) {
        uint32_t cmd = mt_le32(s + p), sz = mt_le32(s + p + 4);
        if (sz < 8 || sz > e - p) break;
        if (cmd == 0x19 && sz >= 72) {                            /* LC_SEGMENT_64: 72 bytes, then 80 per section */
            uint32_t nsec = mt_le32(s + p + 64);
            if (nsec > (sz - 72) / 80) break;
            for (uint32_t k = 0; k < nsec; k++) {
                const uint8_t *q = s + p + 72 + 80 * (uint64_t)k;
                uint64_t ssz = mt_le64(q + 40); uint32_t off = mt_le32(q + 48), fl = mt_le32(q + 64);
                if (!(fl & 0x80000400u) || (fl & 0xFF) == 1 || !ssz || off > n || ssz > n - off) continue;   /* 1: zero-fill */
                if (np == cap) { mt_part *R = cap < 65536 ? realloc(P, 2 * cap * sizeof *P) : NULL; if (!R) break; P = R; cap *= 2; }
                P[np].a = off; P[np].b = off + ssz; np++;
            }
        }
        p += sz;
    }
    int r = mt_code_parts(s, n, g, P, np, cpu == 0x0100000Cu ? MK_A64 : MK_CODE);   /* CPU_TYPE_ARM64 */
    free(P); return r;
}
/* Mach-O universal (fat) file: each architecture's slice is a part, cut by its own sections */
static int mt_fat(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < g->exemin || (mt_be32(s) != 0xCAFEBABEu && mt_be32(s) != 0xCAFEBABFu)) return 0;
    const int f64 = mt_be32(s) == 0xCAFEBABFu; uint32_t na = mt_be32(s + 4);
    if (na == 0 || na > 32 || 8 + (uint64_t)na * (f64 ? 32 : 20) > n) return 0;   /* (Java class files share the magic: they fail below) */
    mt_part P[32]; size_t np = 0;
    for (uint32_t i = 0; i < na; i++) {
        const uint8_t *a = s + 8 + (uint64_t)i * (f64 ? 32 : 20);
        uint64_t off = f64 ? ((uint64_t)mt_be32(a + 8) << 32 | mt_be32(a + 12)) : mt_be32(a + 8), sz = f64 ? ((uint64_t)mt_be32(a + 16) << 32 | mt_be32(a + 20)) : mt_be32(a + 12);
        if (off > n || sz > n - off || sz < 4 || mt_le32(s + off) != 0xFEEDFACFu) continue;
        P[np].a = off; P[np].b = off + sz; np++;
    }
    if (!np) return 0;
    qsort(P, np, sizeof *P, mt_part_cmp);
    for (size_t i = 0; i < np; i++) {
        if (g->k && P[i].a < g->se[g->k - 1]) continue;
        mt_emit(g, g->k ? g->se[g->k - 1] : 0, P[i].a, MK_HEADER, 1);
        uint32_t k0 = g->k; mt_segs t = {g->ss + k0, g->se + k0, g->kind + k0, 0, g->maxs - k0 - 1, g->exemin};
        if (t.maxs >= 4 && mt_macho(s + P[i].a, (size_t)(P[i].b - P[i].a), &t) && t.k) {
            t.se[t.k - 1] = P[i].b - P[i].a;
            for (uint32_t q = 0; q < t.k; q++) { t.ss[q] += P[i].a; t.se[q] += P[i].a; }
            g->k += t.k;
        } else mt_emit(g, P[i].a, P[i].b, MK_DATA, 0);
    }
    mt_emit(g, g->se[g->k - 1], n, MK_HEADER, 1);
    return 1;
}
/* PE / COFF (Windows executables and DLLs): sections with IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE */
static int mt_pe(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < g->exemin || s[0] != 'M' || s[1] != 'Z') return 0;
    uint32_t pe = mt_le32(s + 0x3C); if (pe > n - 24 || memcmp(s + pe, "PE\0\0", 4)) return 0;
    const unsigned mach = mt_le16(s + pe + 4), nsec = mt_le16(s + pe + 6), opt = mt_le16(s + pe + 20);
    uint64_t st = (uint64_t)pe + 24 + opt; if (nsec > 96 || st + 40 * (uint64_t)nsec > n) return 0;
    mt_part P[96]; size_t np = 0;
    for (unsigned i = 0; i < nsec; i++) {
        const uint8_t *h = s + st + 40 * (uint64_t)i; uint32_t rsz = mt_le32(h + 16), roff = mt_le32(h + 20), ch = mt_le32(h + 36);
        if (!(ch & 0x20000020u) || !rsz || roff > n || rsz > n - roff) continue;
        P[np].a = roff; P[np].b = roff + rsz; np++;
    }
    return mt_code_parts(s, n, g, P, np, mach == 0xAA64 ? MK_A64 : MK_CODE);   /* IMAGE_FILE_MACHINE_ARM64 */
}

/* safetensors: an 8-byte header length, a JSON header {"name": {"dtype": "BF16", "shape": [...], "data_offsets": [a, b]},
 * ...}, then the tensors. Neighbouring tensors of one type form a part (one plan per run of a type): a plan per tensor
 * of 1 MiB or more was 0.035% smaller over the four tensor files measured but planned twice as long (RESULTS.md). */
static int mt_safetensors(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < 16) return 0;
    uint64_t hl = mt_le64(s);
    if (hl < 2 || hl > ((uint64_t)100 << 20) || hl > n - 8 || s[8] != '{') return 0;
    const uint8_t *j = s + 8, *je = j + hl; const uint64_t base = 8 + hl;
    size_t cap = 1024, np = 0; mt_part *P = malloc(cap * sizeof *P); if (!P) return 0;
    int depth = 0, key = 0, ok = 1; uint8_t dt = 0; int64_t o0 = -1, o1 = -1; const uint8_t *ks = NULL; size_t kl = 0;
    for (const uint8_t *q = j; q < je && ok; q++) {
        uint8_t c = *q;
        if (c == '"') {                                           /* a string: a key, or the dtype's value */
            const uint8_t *a = ++q; while (q < je && *q != '"') { if (*q == '\\') q++; q++; }
            if (q >= je) { ok = 0; break; }
            size_t l = (size_t)(q - a);
            if (key) { ks = a; kl = l; key = 0; }
            else if (depth == 2 && ks && kl == 5 && !memcmp(ks, "dtype", 5)) { dt = 0; for (size_t t = 0; t < l; t++) dt = (uint8_t)(dt * 31 + a[t]); dt |= 1; }
            continue;
        }
        if (c == '{') { depth++; key = 1; if (depth == 2) { dt = 0; o0 = o1 = -1; } ks = NULL; continue; }
        if (c == ',') { if (depth >= 1) key = 1; continue; }
        if (c == ':') { key = 0; continue; }
        if (c == '[' && depth == 2 && ks && kl == 12 && !memcmp(ks, "data_offsets", 12)) {
            uint64_t v[2]; int k = 0; q++;
            while (q < je && *q != ']' && k < 2) {
                while (q < je && (*q == ' ' || *q == ',')) q++;
                uint64_t x = 0; int d = 0; while (q < je && *q >= '0' && *q <= '9' && d < 19) { x = x * 10 + (uint64_t)(*q++ - '0'); d++; }
                if (!d) { ok = 0; break; }
                v[k++] = x; while (q < je && *q == ' ') q++;
            }
            if (k != 2 || q >= je || *q != ']') { ok = 0; break; }
            o0 = (int64_t)(v[0] & INT64_MAX); o1 = (int64_t)(v[1] & INT64_MAX);
            continue;
        }
        if (c == '}') {
            if (depth == 2 && o0 >= 0 && o1 > o0 && (uint64_t)o1 <= n - base) {
                if (np == cap) { mt_part *R = cap < 65536 ? realloc(P, 2 * cap * sizeof *P) : NULL; if (!R) { ok = 0; break; } P = R; cap *= 2; }
                P[np].a = base + (uint64_t)o0; P[np].b = base + (uint64_t)o1; P[np].kind = dt; np++;
            }
            depth--; if (depth < 0) ok = 0;
            continue;
        }
    }
    if (!ok || !np) { free(P); return 0; }
    qsort(P, np, sizeof *P, mt_part_cmp);
    mt_emit(g, 0, base, base >= META_MINSEG ? MK_HEADER : MK_DATA, 0);   /* a short header joins the first tensors */
    for (size_t i = 0; i < np; i++) {
        if (P[i].a < g->se[g->k - 1]) continue;
        mt_emit(g, P[i].a, P[i].b, (uint8_t)(MK_TENSOR + P[i].kind % 200), 1);
    }
    free(P);
    mt_emit(g, g->se[g->k - 1], n, MK_DATA, 1);
    return 1;
}

/* GGUF (version 2 and 3): key/value metadata, tensor infos (name, dims, type, offset), then the aligned tensor data.
 * Neighbouring tensors of one type form a part (a quantised type is a record array: its block layout is found by the
 * record trial from the part's start). */
static int mt_gguf_skip(const uint8_t *s, size_t n, size_t *p, uint32_t ty, int depth) {
    static const uint8_t SZ[13] = {1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8};   /* by type; 8 string, 9 array */
    if (ty < 13 && SZ[ty]) { if (*p + SZ[ty] > n) return 0; *p += SZ[ty]; return 1; }
    if (ty == 8) { if (*p + 8 > n) return 0; uint64_t l = mt_le64(s + *p); if (l > n - *p - 8) return 0; *p += 8 + l; return 1; }
    if (ty == 9 && depth < 2) {
        if (*p + 12 > n) return 0; uint32_t et = mt_le32(s + *p); uint64_t c = mt_le64(s + *p + 4); *p += 12;
        if (et < 13 && SZ[et]) { if (c > (n - *p) / SZ[et]) return 0; *p += c * SZ[et]; return 1; }
        if (c > n) return 0;
        for (uint64_t i = 0; i < c; i++) if (!mt_gguf_skip(s, n, p, et, depth + 1)) return 0;
        return 1;
    }
    return 0;
}
static int mt_gguf(const uint8_t *s, size_t n, mt_segs *g) {
    if (n < 64 || memcmp(s, "GGUF", 4)) return 0;
    uint32_t ver = mt_le32(s + 4); if (ver != 2 && ver != 3) return 0;
    uint64_t nt = mt_le64(s + 8), nkv = mt_le64(s + 16); size_t p = 24; uint64_t align = 32;
    if (nt == 0 || nt > 65536 || nkv > 65536) return 0;
    for (uint64_t i = 0; i < nkv; i++) {
        if (p + 8 > n) return 0; uint64_t kl = mt_le64(s + p); if (kl > 65536 || kl > n - p - 12) return 0;
        const uint8_t *k = s + p + 8; p += 8 + kl; uint32_t ty = mt_le32(s + p); p += 4;
        if (kl == 17 && !memcmp(k, "general.alignment", 17) && ty == 4 && p + 4 <= n) { align = mt_le32(s + p); if (!align || align > 65536 || (align & (align - 1))) return 0; }
        if (!mt_gguf_skip(s, n, &p, ty, 0)) return 0;
    }
    mt_part *P = malloc(nt * sizeof *P); if (!P) return 0;
    for (uint64_t i = 0; i < nt; i++) {
        if (p + 8 > n) { free(P); return 0; } uint64_t kl = mt_le64(s + p); if (kl > 65536 || kl > n - p - 12) { free(P); return 0; }
        p += 8 + kl; uint32_t nd = mt_le32(s + p); p += 4;
        if (nd > 8 || p + 8 * (uint64_t)nd + 12 > n) { free(P); return 0; }
        p += 8 * (size_t)nd; P[i].kind = (uint8_t)mt_le32(s + p); P[i].a = mt_le64(s + p + 4); p += 12;
    }
    uint64_t base = (p + align - 1) / align * align;
    if (base > n) { free(P); return 0; }
    for (uint64_t i = 0; i < nt; i++) if (P[i].a > n - base) { free(P); return 0; } else P[i].a += base;
    qsort(P, nt, sizeof *P, mt_part_cmp);
    for (uint64_t i = 0; i < nt; i++) P[i].b = i + 1 < nt ? P[i + 1].a : n;
    mt_emit(g, 0, base, base >= META_MINSEG ? MK_HEADER : MK_DATA, 0);   /* a short header joins the first tensors */
    for (uint64_t i = 0; i < nt; i++) {
        if (P[i].b <= P[i].a) continue;
        mt_emit(g, P[i].a, P[i].b, (uint8_t)(MK_TENSOR + P[i].kind % 200), 1);
    }
    free(P);
    return 1;
}

/* a member of an archive cut by its own format (no nested archives) */
static int mt_nested(const uint8_t *s, size_t n, mt_segs *g) {
    g->k = 0; if (mt_zip(s, n, g)) return 1;
    g->k = 0; if (mt_elf(s, n, g)) return 1;
    g->k = 0; if (mt_macho(s, n, g)) return 1;
    g->k = 0; if (mt_fat(s, n, g)) return 1;
    g->k = 0; if (mt_pe(s, n, g)) return 1;
    g->k = 0; if (mt_safetensors(s, n, g)) return 1;
    g->k = 0; if (mt_gguf(s, n, g)) return 1;
    g->k = 0; return 0;
}

/* The input cut into segments (each planned on its own) by the first format whose headers parse; one segment
 * otherwise. kind[i] (may be NULL): MK_PACKED marks compressed content. */
/* exemin: executables are cut into code and data down to this size. 1 MiB until 0.0.3 kept every smaller executable
 * from its branch filter (75 x86-64 and 75 arm64 tools of 32-256 KiB: -9 +4.1% and +4.5% against the filtered files);
 * the header reads are bounds-checked at any size. Level 1 (speed), which applies no branch filter, keeps 1 MiB: the
 * cut alone cost those tools 0.5% there. */
#define MT_EXE_MIN ((size_t)4 << 10)
#define MT_EXE_MIN_SPEED ((size_t)1 << 20)
static uint32_t meta_segments_x(const uint8_t *s, size_t n, uint64_t *ss, uint64_t *se, uint32_t *kind, uint32_t maxs, size_t exemin) {
    uint32_t kk[1]; mt_segs g = {ss, se, kind, 0, maxs, exemin};
    if (!kind) { g.kind = kk; g.maxs = 1; }
    if (s && n && kind && maxs >= 4) {
        if (mt_tar(s, n, &g)) return g.k;
        g.k = 0; if (mt_zip(s, n, &g)) return g.k;
        g.k = 0; if (mt_elf(s, n, &g)) return g.k;
        g.k = 0; if (mt_macho(s, n, &g)) return g.k;
        g.k = 0; if (mt_fat(s, n, &g)) return g.k;
        g.k = 0; if (mt_pe(s, n, &g)) return g.k;
        g.k = 0; if (mt_safetensors(s, n, &g)) return g.k;
        g.k = 0; if (mt_gguf(s, n, &g)) return g.k;
        g.k = 0;
    }
    ss[0] = 0; se[0] = n; g.kind[0] = s && n && meta_packed(s, n) ? MK_PACKED : MK_DATA;
    return 1;
}
/* A buffer of members without headers (the chunk store of kolmo archive): member i starts at cut[i] (increasing, the
 * first at 0) and ends where the next starts. Each member is a part as a tar member would be (mt_member). */
static uint32_t meta_runs(const uint8_t *s, size_t n, const uint64_t *cut, size_t ncut, uint64_t *ss, uint64_t *se, uint32_t *kind, uint32_t maxs, size_t exemin) {
    mt_segs g = {ss, se, kind, 0, maxs, exemin}; uint64_t last = 0;
    for (size_t i = 0; i < ncut && g.k + 2 < g.maxs; i++) {
        uint64_t a = cut[i], b = i + 1 < ncut ? cut[i + 1] : n;
        if (a < last || b > n || b < a) break;
        if (b > a) mt_member(s, a, b - a, 1, &g, &last);
    }
    if (last < n || g.k == 0) { g.ss[g.k] = last; g.se[g.k] = n; g.kind[g.k] = MK_DATA; g.k++; }
    return g.k;
}
static uint32_t meta_segments(const uint8_t *s, size_t n, uint64_t *ss, uint64_t *se, uint32_t *kind, uint32_t maxs) {
    return meta_segments_x(s, n, ss, se, kind, maxs, MT_EXE_MIN);
}
