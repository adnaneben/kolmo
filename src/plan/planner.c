#ifndef KOLMO_F2_PLAN
#define KOLMO_F2_PLAN 0   /* 1: the planner's graph scores also try codecs 21-23 (measured: no decision changes) */
#endif
/* The planner: codec per mode, the candidate trials, and the file-level analysis (make_plan). */
#ifndef KOLMO_BWT_REACH_PCT
#define KOLMO_BWT_REACH_PCT 100  /* max: BWT is tried on a binary block only where LZMA2 reaches this % or less. Was 35;
                                    * on raw video residual planes (holdout foreman/mobile_cif.y4m, LZMA2 about 45%) BWT is
                                    * 5-6% smaller and, with the slice skip, about 8x faster to code. No other file of either
                                    * corpus, nor four outside binaries, changed by a byte (evidence/NIGHT4_SUMMARY.md). */
#endif
#ifndef KOLMO_BWT_GAIN_PCT
#define KOLMO_BWT_GAIN_PCT 3      /* max: least size gain (%) for BWT to replace LZMA2 on a binary block */
#endif
#ifndef KOLMO_BWT_SKIP_LZMA_PCT
#define KOLMO_BWT_SKIP_LZMA_PCT 98 /* max: a slice with BWT at most this % of LZMA2 (preset 1) codes the block with BWT only.
                                    * 90 until 0.0.2: akiyo_cif.y4m (45 MB) has slice 0.976 and BWT 9.6% smaller on the block,
                                    * and ran the full LZMA2 pass first (65 s of its 68; 62% of max's time on the screening
                                    * corpus) for an output it then discarded. The block ratio sits 6-7 points below the slice
                                    * ratio (osdb 0.84 -> 0.78, akiyo 0.976 -> 0.904), so 98 still means a clear BWT win. */
#endif
static void mode_codec(const kolmo_params *p, int *codec, int *level) {
    switch (p->mode) {
        case KOLMO_RT:  *codec = R_SPEED;    *level = p->level ? p->level : 1; break;
        case KOLMO_FAST:*codec = R_LZH;   *level = p->level ? p->level : CZ_FAST_LEVEL; break;
        case KOLMO_ECO: *codec = R_ECO; *level = p->level ? p->level : CZ_ECO_LEVEL; break;
        case KOLMO_HC:  *codec = R_LZH;   *level = p->level ? p->level : 19; break;
        case KOLMO_MAX: *codec = R_LZMA;   *level = p->level ? p->level : 9; break;     /* max: BWT for text, the better of BWT and LZMA2 for binary (level 1: BWT only) */
        default:      *codec = C_CM;     *level = p->level == 1 || p->level == 2 ? p->level : 0; break;   /* ultra: context mixing (level 1: model 1, CM only; level 2: maximum ratio, graph leaves may use CM) */
    }
}

/* file-level analysis: one plan for every block */
/* light: the cheap analysis of the fastest settings: no JSON, log or word trials (whose transforms are also the slow
 * ones), and binary candidates scored by zstd -1 on a 256 KiB slice instead of zstd -3 on 1 MiB */
/* integer-array candidate for the planner: width from a .npy header (head = the header), else 2, 4 and 8 tried; each
 * coded as an INT graph with zstd at the scoring level on the slice [st, st+L). Returns the best cost per byte (or a
 * huge value) and sets the width and head. */
/* float-array candidate: float32 or a 16-bit float (or a .npy float dtype) whose rotated top byte (the exponent) is far more predictable
 * than its mantissa bytes; the byte alignment (head) with the most predictable exponent wins. Scored like the INT
 * candidate; returns the cost per byte (or a huge value), sets width and head. */
static double kg_plane_h(const uint8_t *p, size_t cnt, int w, int b, int rot) {   /* order-0 entropy of plane b, bits */
    uint32_t c[256] = {0}; size_t m = cnt < 8192 ? cnt : 8192, step = cnt / m;
    for (size_t q = 0; q < m; q++) {
        const uint8_t *e = p + q * step * (size_t)w; unsigned x;
        if (!rot) x = e[b];                                   /* plane b is byte b of the little-endian word */
        else x = (((unsigned)e[b] << 1) | (e[b ? b - 1 : w - 1] >> 7)) & 255;   /* rotated left one bit: the top bit of the byte below */
        c[x]++;
    }
    double hs = 0; for (int k = 0; k < 256; k++) if (c[k]) { double q = (double)c[k] / m; hs -= q * log2(q); }
    return hs;
}
/* 8-bit image candidate (IMG8 node): row length and channel step from an image header (NPY uint8 of shape (H, W) or
 * (H, W, C), binary PGM/PPM, PAM, BMP, uncompressed TIFF: meta_image8),
 * else from common image widths x 1, 3 or 4 channels, keeping the layout whose median-predictor residuals are much
 * smaller than a left-neighbour predictor's on a sample. Scored like the other graph candidates. */
#define KG_SCREEN ((size_t)32 << 10)   /* graph trials: bytes coded first as a screen */
#define KG_SCREEN_SLACK 1.25            /* screen passes when within this factor of plain coding (a short run codes worse) */
static double kg_img8_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, double plain, size_t *bw, unsigned *bc, size_t *bh) {
    double best = 1e300; size_t h = 0, W = 0; unsigned C = 0;
    if (meta_image8(s, n, &W, &C, &h)) {
        /* the geometry from the header (NPY, PNM/PAM, BMP, TIFF: plan/meta.c) */
    } else {
        /* row length: the lag (width x channels, common widths) at which bytes repeat best, on 4096 sampled positions;
         * it must stand out against the other lags, and the median predictor must not lose to the left neighbour */
        static const uint16_t WS[] = {128, 160, 192, 200, 240, 256, 300, 320, 352, 384, 400, 416, 480, 500, 512, 576, 600, 640, 720,
                                      768, 800, 960, 1000, 1024, 1080, 1200, 1280, 1366, 1440, 1536, 1600, 1920, 2048, 2560, 3840, 4096};
        enum { NS = 4096 }; size_t maxlag = 4096 * 4; if (n < 4 * maxlag) maxlag = n / 4;
        size_t span = n - maxlag, stp = span / NS ? span / NS : 1; uint32_t bestc = UINT32_MAX, sum = 0, cnt = 0; size_t bl = 0;
        /* the candidate lags in order, then their costs with the sample loop outside: for each sampled position the
         * lagged bytes lie within maxlag before it, so the reads stay in cache (same sums, so the same choice) */
        enum { NL = sizeof WS / sizeof WS[0] * 3 }; size_t lags[NL]; uint32_t costs[NL]; int nl = 0;
        for (size_t k = 0; k < sizeof WS / sizeof WS[0]; k++) for (unsigned c = 1; c <= 4; c++) {
            size_t lag = (size_t)WS[k] * c; if (c == 2 || lag > maxlag) continue;
            lags[nl] = lag; costs[nl] = 0; nl++;
        }
        if (kolmo_force_c)
            for (int k = 0; k < nl; k++) { uint32_t cost = 0; size_t lag = lags[k]; for (size_t q = 0, i = maxlag; q < NS && i < n; q++, i += stp) { int d = s[i] - s[i - lag]; cost += (uint32_t)(d < 0 ? -d : d); } costs[k] = cost; }
        else
            for (size_t q = 0, i = maxlag; q < NS && i < n; q++, i += stp) {
                const int x = s[i];
                for (int k = 0; k < nl; k++) { int d = x - s[i - lags[k]]; costs[k] += (uint32_t)(d < 0 ? -d : d); }
            }
        for (int k = 0; k < nl; k++) { uint32_t cost = costs[k]; sum += cost / 64; cnt++; if (cost < bestc) { bestc = cost; bl = lags[k]; } }
        if (!cnt || !bl || (uint64_t)(bestc / 64) * cnt * 2 > sum) return best;     /* rows repeat: one lag at half the average cost or less */
        uint32_t lc[5] = {0};
        for (size_t q = 0, i = maxlag; q < NS && i < n; q++, i += stp) for (unsigned c = 1; c <= 4; c++) { int d = s[i] - s[i - c]; lc[c] += (uint32_t)(d < 0 ? -d : d); }
        W = bl; C = 1; for (unsigned c = 3; c <= 4; c++) if (bl % c == 0 && lc[c] < lc[C]) C = c;
        size_t a0 = (n / 2 / W) * W, m = W * 16 < 65536 ? W * 16 : (65536 / W ? 65536 / W * W : W);
        if (a0 < W || a0 + m > n) return best;
        double pc = 0, left = 0;
        for (size_t i = a0; i < a0 + m; i++) { int x = s[i], d = x - kg_med(s[i - C], s[i - W], s[i - W - C]), dl = x - s[i - C]; pc += d < 0 ? -d : d; left += dl < 0 ? -dl : dl; }
        if (pc > left) return best;                             /* the 2-D predictor must not lose to the left neighbour */
    }
    if (W < 2 || n < h + 4 * W) return best;
    if (st < h) st = h;
    size_t a = h + (st - h) / W * W, l = (n - a < L ? n - a : L) / W * W; if (l < 2 * W) l = (n - a) / W * W < 2 * W ? 0 : 2 * W;
    codec_ctx cx = {0}; cx.no_f2 = !KOLMO_F2_PLAN;   /* planner score: rc2 leaf rule unless KOLMO_F2_PLAN */ uint8_t *o = malloc(l + 64);
    if (o && plain < 1e299 && l > KG_SCREEN * 2 && W * 8 <= KG_SCREEN) {   /* screen on a few rows, as for integers */
        size_t ls = KG_SCREEN / W * W, zs = kg_encode_img8(&cx, R_LZH, zl, s + a, ls, W, C, 0, o, l + 64);
        if (!zs || (double)zs / ls > plain * KG_SCREEN_SLACK) { free(o); codec_ctx_free(&cx); return best; }
    }
    size_t z = o && l ? kg_encode_img8(&cx, R_LZH, zl, s + a, l, W, C, 0, o, l + 64) : 0;
    if (z) { best = (double)z / l; *bw = W; *bc = C; *bh = h; }
    free(o); codec_ctx_free(&cx);
    return best;
}

static double kg_float_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, double plain, int *bw, size_t *bh) {
    double best = 1e300; int w = 4; size_t h = 0; int fixed = 0;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL;
        if (!d || d + 13 > s + hl || d[10] == '>' || d[11] != 'f' || (d[12] != '2' && d[12] != '4' && d[12] != '8')) return best;
        h = hl; w = d[12] - '0'; fixed = 1;
        if (n < h + 4096) return best;                       /* same shape test as raw data: predictable exponent, random mantissa */
        if (kg_plane_h(s + h, (n - h) / w, w, w - 1, 1) > 6.0 || kg_plane_h(s + h, (n - h) / w, w, w - 2, 1) < 7.0) return best;
    }
    if (n < h + 4096) return best;
    int ws[2] = {w, 0}; size_t hs[2] = {h, 0}; int nw = 1;
    if (!fixed) {   /* float32 and 16-bit floats (bf16, fp16), each at the byte alignment with the most predictable exponent */
        nw = 0;
        for (int fw = 4; fw >= 2; fw -= 2) {
            double he = 1e9; size_t ph = 0;
            for (size_t k = 0; k < (size_t)fw; k++) { double e = kg_plane_h(s + k, (n - k) / fw, fw, fw - 1, 1); if (e < he) { he = e; ph = k; } }
            double hm = kg_plane_h(s + ph, (n - ph) / fw, fw, fw - 2, 1);
            if (he <= 6.0 && hm >= 7.0) { ws[nw] = fw; hs[nw] = ph; nw++; }   /* exponent predictable, mantissa near random */
        }
        if (!nw) return best;
    }
    codec_ctx cx = {0}; cx.no_f2 = !KOLMO_F2_PLAN;   /* planner score: rc2 leaf rule unless KOLMO_F2_PLAN */ uint8_t *o = malloc(L + 64);
    for (int q = 0; o && q < nw; q++) {
        int fw = ws[q]; size_t fh = hs[q], s0 = st < fh ? fh : st, L0 = L > n - s0 ? n - s0 : L;
        size_t a = fh + (s0 - fh) / fw * fw, l = (n - a < L0 ? n - a : L0) / fw * fw;
        if (l >= 4096) {                                      /* screen: the planes' order-0 entropy must already beat plain coding */
            double e = 0; for (int b = 0; b < fw; b++) e += kg_plane_h(s + a, l / fw, fw, b, 1);
            if (e / (8.0 * fw) >= plain) continue;
        }
        size_t z = l ? kg_encode_float(&cx, R_LZH, zl, s + a, l, fw, 0, o, L + 64) : 0;
        if (z && (double)z / l < best) { best = (double)z / l; *bw = fw; *bh = fh; }
    }
    free(o); codec_ctx_free(&cx);
    return best;
}

/* Binary64 values that were written in decimal (DEC64). A row-major .npy '<f8' table gives its row length; any other
 * data is tried as a plain array of doubles when a spread sample of 8-byte words is nearly all decimal or one repeated
 * pattern (missing values). *bcols: values per row, or 0 for a plain array. */
/* plan byte and row bytes for a DEC64 plan from the trial's column count: 64 a binary64 table, 65 segments, 66 a binary32 table */
#define DEC_B(dc) (!(dc) ? 65 : ((dc) & KG_DEC_F32) ? 66 : 64)
#define DEC_KGW(dc) (!(dc) ? 8u : ((dc) & KG_DEC_F32) ? (uint32_t)((dc) & ~KG_DEC_F32) * 4u : (uint32_t)(dc) * 8u)
static double kg_dec64_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, uint32_t *bcols, size_t *bh) {
    double best = 1e300; size_t h = 0, cols = 0, tw = 8; int f32 = 0;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL;
        if (!d || d + 13 > s + hl || d[10] != '<' || d[11] != 'f' || (d[12] != '8' && d[12] != '4')) return best;
        h = hl; f32 = d[12] == '4'; tw = f32 ? 4 : 8;
        if (n >= h + 4096) {                                 /* 64 values across the array: a quarter must be decimal or NaN */
            size_t words = (n - h) / tw, ok = 0;
            for (size_t q = 0; q < 64; q++) {
                const uint8_t *wp = s + h + (size_t)((uint64_t)q * words / 64) * tw; int64_t m; int e = 0;
                if (f32) { uint32_t bits = get32(wp); while (e < 8 && !kg_dec_fit32(bits, KG_P10[e], &m)) e++; ok += e < 8 || (bits & 0x7f800000u) == 0x7f800000u; }
                else { uint64_t bits = get64(wp); while (e < KG_DEC_ESEARCH && !kg_dec_fit(bits, KG_P10[e], &m)) e++;
                       ok += e < KG_DEC_ESEARCH || (bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull; }
            }
            if (ok < 16) return best;
        }
        const uint8_t *sh = memmem(s, hl, "'shape': (", 10), *fo = memmem(s, hl, "'fortran_order': False", 22);
        if (sh && fo) {                                      /* row-major: a row is the product of the dimensions after the first */
            const uint8_t *q = sh + 10, *e = s + hl; uint64_t dim = 0, prod = 1; int nd = 0, ok = 1, dig = 0;
            for (; q < e && *q != ')'; q++) {
                if (*q >= '0' && *q <= '9') { dim = dim * 10 + (uint64_t)(*q - '0'); dig = 1; if (dim > (1u << 30)) ok = 0; }
                else if (*q == ',') { if (nd++ && dig) { prod *= dim; if (prod > 65536) ok = 0; } dim = 0; dig = 0; }
            }
            if (dig && nd) { prod *= dim; if (prod > 65536) ok = 0; }
            if (ok && prod >= 2 && (n - h) / tw / prod >= 16) cols = (size_t)prod;
        }
    } else {
        /* arrays inside other data (a raw dump, pickle, Arrow, HDF5: headers, then columns at any byte alignment):
         * 64 places across the input are probed, and a quarter of them must hold decimal doubles */
        if (n < 4096) return best;
        size_t hits = 0;
        for (size_t q = 0; q < 64; q++) hits += kg_dec_probe(s + (size_t)((uint64_t)q * (n - 136) / 64)) != 0;
        if (hits < 16) return best;
    }
    if (n < h + 512) return best;
    const size_t rb = cols ? cols * tw : 8;
    if (st < h) st = h;
    size_t a = h + (st - h) / rb * rb, l = (n - a < L ? n - a : L) / rb * rb;
    if (l < 16 * rb || l < 512) { a = h; l = (n - h < L ? n - h : L) / rb * rb; if (l < 512) return best; }
    codec_ctx cx = {0}; cx.no_f2 = !KOLMO_F2_PLAN;   /* planner score: rc2 leaf rule unless KOLMO_F2_PLAN */ uint8_t *o = malloc(l + 64);
    const size_t ca = cols && f32 ? cols | KG_DEC_F32 : cols;      /* a binary32 table carries the flag in its column count */
    size_t z = o ? kg_encode_dec64(&cx, R_LZH, zl, s + a, l, ca, 0, o, l + 64) : 0;
    if (z) { best = (double)z / l; *bcols = (uint32_t)ca; *bh = h; }
    free(o); codec_ctx_free(&cx);
    return best;
}

static double kg_num_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, double plain, int *bw, size_t *bh, int native) {
    size_t h = 0; int ws[3] = {2, 4, 8}, nw = 3; double best = 1e300;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL;
        if (!d || d + 13 > s + hl || d[10] == '>' || (d[11] != 'i' && d[11] != 'u' && d[11] != 'f') || (d[12] != '2' && d[12] != '4' && d[12] != '8')) return best;
        h = hl; ws[0] = d[12] - '0'; nw = 1;
    }
    if (n - h < 4096) return best;
    if (st < h) st = h;
    if (L > n - st) L = n - st;
    {   /* score only the width with the fewest estimated residual bytes per input byte, and only when that estimate
         * is near the plain coding cost (the bit-length estimate ignores repetition, so clearly worse means no gain) */
        double eb = 1e300; int bw0 = 0;
        for (int q = 0; q < nw; q++) {
            int w = ws[q], o0, s0; size_t a = h + (st - h) / w * w, c = (n - a < L ? n - a : L) / w;
            if (c < 16) continue;
            /* four of the sixteen sample runs first: an estimate a quarter above plain coding ends this width */
            if (c > 16384 && plain < 1e299 && kg_num_choose_n(s + a, c, w, &o0, &s0, 4) / (8.0 * w) > plain * 1.25) continue;
            double e = kg_num_choose(s + a, c, w, &o0, &s0) / (8.0 * w); if (e < eb) { eb = e; bw0 = w; }
        }
        if (!bw0 || (eb > plain && !native)) return best;   /* estimate above plain coding: no gain (the bit-length estimate
                                                         * overstates the native leaf's entropy coding, so it is not used to skip) */
        ws[0] = bw0; nw = 1;
    }
    codec_ctx cx = {0}; cx.no_f2 = !KOLMO_F2_PLAN;   /* planner score: rc2 leaf rule unless KOLMO_F2_PLAN */ cx.native_leaf = native; uint8_t *o = malloc(L + 64);
    for (int q = 0; o && q < nw; q++) {
        int w = ws[q]; size_t a = h + (st - h) / w * w, l = (n - a < L ? n - a : L) / w * w;
        if (plain < 1e299 && l > KG_SCREEN * 2) {            /* screen on a short run: far above plain coding means no full trial */
            size_t zs = kg_encode_num(&cx, R_LZH, zl, s + a, KG_SCREEN / w * w, w, 0, o, L + 64);
            if (!zs || (double)zs / (KG_SCREEN / w * w) > plain * KG_SCREEN_SLACK) continue;
        }
        size_t z = l ? kg_encode_num(&cx, R_LZH, zl, s + a, l, w, 0, o, L + 64) : 0;
        if (z && (double)z / l < best) { best = (double)z / l; *bw = w; *bh = h; }
    }
    free(o); codec_ctx_free(&cx);
    return best;
}

#ifndef SHUF_SCREEN
#define SHUF_SCREEN 0.97
#endif
#ifdef KOLMO_PLAN_HOOK
static int kolmo_plan_full;   /* research builds only: every trial slice is the whole input (benchmark/rc3w2/twopass) */
#define PLAN_SLICE(x) (kolmo_plan_full ? ((size_t)1 << 40) : (size_t)(x))
#else
#define PLAN_SLICE(x) (x)
#endif
static plan_t make_plan(const uint8_t *s, size_t n, int effort, int logs, int structured, int threads, int strong, int light, int native) {
    const int numtrial = light == 4;                      /* balanced: quick analysis of binary data, with the integer trial kept */
    if (light && light != 4) logs = 0;
    plan_t pl = {T_NONE, 0, 0, 0};
    if (n < 4096) return pl;
    const size_t TRIAL = PLAN_SLICE(effort ? (1u << 20) : (256u << 10));   /* text-transform trial slice (fast: 256 KiB) */
    size_t h = n < 65536 ? n : 65536, pr = 0;
    for (size_t i = 0; i < h; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13; }
    size_t ihw; unsigned ihc; size_t ihh;
    const int imghdr = meta_image8(s, n, &ihw, &ihc, &ihh);   /* an image header: pixels, even where they look like text */
    const int textlike = pr * 100 > h * 95;
    if (textlike && !imghdr) {                          /* text: field transposition if lines have a fixed field count */
        if (light == 2 || light == 3) return pl;                 /* minimal analysis: text is coded as it is */
        if (logs && n >= 65536) {                                /* wide delimited table (over 256 columns): kolmo graph */
            static const uint8_t WD[4] = {';', ',', '\t', '|'};
            for (int di = 0; di < 4 && pl.t != T_KG; di++) {
                size_t lines = 0, same = 0, i = 0; uint32_t k0 = 0;
                while (i < h) {
                    const uint8_t *e = memchr(s + i, '\n', h - i); if (!e) break;
                    uint32_t f = 1; for (const uint8_t *q = s + i; q < e; q++) f += *q == WD[di];
                    if (lines == 1) k0 = f;                      /* the first line may be a header */
                    if (lines >= 1) same += f == k0;
                    lines++; i = (size_t)(e - s) + 1;
                }
                if (lines < 4 || k0 <= 256 || same * 100 < (lines - 1) * 95) continue;
                uint8_t sep = '.';
                if (WD[di] != ',') { size_t dc = 0, cc = 0; for (size_t q = 1; q + 1 < h; q++) if ((uint8_t)(s[q - 1] - '0') < 10 && (uint8_t)(s[q + 1] - '0') < 10) { dc += s[q] == '.'; cc += s[q] == ','; } if (cc > dc) sep = ','; }
                const size_t TW = PLAN_SLICE(effort ? (512u << 10) : TRIAL);      /* wide rows repeat their structure every line: half a MiB decides */
                size_t mm = n < TW ? n : TW; while (mm > 0 && s[mm - 1] != '\n') mm--;
                if (mm < 16384) continue;
                const uint8_t *ts = s;                           /* large inputs: trial from the middle (the start may be atypical) */
                if (n > 4 * (size_t)TRIAL) {
                    const uint8_t *m0 = memchr(s + n / 2, '\n', n / 2); size_t st = m0 ? (size_t)(m0 - s) + 1 : 0;
                    if (st && st + mm <= n) { size_t e = mm; while (e > 0 && s[st + e - 1] != '\n') e--; if (e >= 16384) { ts = s + st; mm = e; } }
                }
                codec_ctx cx = {0}; cx.no_f2 = !KOLMO_F2_PLAN;   /* planner score: rc2 leaf rule unless KOLMO_F2_PLAN */ uint8_t *o = malloc(mm + 64); size_t zcap = kolmo_klzh_bound(mm) + 4096; uint8_t *z = malloc(zcap);
                if (o && z) {
                    size_t a = kg_encode_table(&cx, R_LZH, 1, ts, mm, WD[di], sep, o, mm + 64), b = kprobe_size(ts, mm);
                    int ok = a && a * 100 < CZ_LOG_GAIN_PCT * b;
                    if (ok && effort) {                          /* slower settings: the graph must also win with a strong codec (max, */
                        int cc = strong ? C_BWT : R_LZH, lv = 9;     /* ultra: BWT; hc: zstd -9 standing in for zstd -19) */
                        size_t ms = mm; if (ms > (384u << 10)) { ms = 384u << 10; while (ms > 0 && ts[ms - 1] != '\n') ms--; }   /* a shorter run decides the same and costs a third */
                        size_t a2 = ms ? kg_encode_table(&cx, cc, lv, ts, ms, WD[di], sep, o, mm + 64) : 0, b2 = ms ? codec_compress(&cx, cc, lv, ts, ms, z, zcap) : 0;
                        ok = a2 && b2 && a2 * 100 < 97 * b2;
                    }
                    if (ok) { pl.t = T_KG; pl.a = WD[di]; pl.b = sep; }
                }
                free(o); free(z); codec_ctx_free(&cx);
            }
            if (pl.t == T_KG) return pl;
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        double tx0 = prof_now(); fprintf(stderr, "text: minified=%d\n", (int)(logs && n >= 65536 && json_minified(s, h)));
#endif
        if (logs && n >= 65536 && json_minified(s, h)) {   /* minified JSON: split into records, then as JSON lines */
            size_t mm = n < TRIAL ? n : TRIAL;             /* (shorter slices were tried: the gain does not show below 256 KiB) */
            uint8_t *sp = malloc(2 * mm + 64), *o = malloc(2 * mm + 64);
            int shaped = 1;
            if (sp && mm >= (64u << 10)) {
                /* records must share a few key layouts for the template transform to pay: split the first 64 KiB and
                 * count distinct key sequences over up to 256 records (a hash of each record's key names) */
                size_t sn0 = json_split(s, 64u << 10, sp), i = 0; uint32_t seen[64]; int ns = 0, lines = 0;
                while (i < sn0 && lines < 256) {
                    const uint8_t *e = memchr(sp + i, '\n', sn0 - i); size_t l = e ? (size_t)(e - (sp + i)) : sn0 - i; uint32_t hk = 2166136261u; int inq = 0; size_t qs = 0;
                    for (size_t q = 0; q < l; q++) {
                        uint8_t c = sp[i + q];
                        if (c == '\\' && inq) { q++; continue; }
                        if (c == '"') { if (!inq) { inq = 1; qs = q + 1; } else { inq = 0; if (q + 1 < l && sp[i + q + 1] == ':') { for (size_t r = qs; r < q; r++) hk = (hk ^ sp[i + r]) * 16777619u; hk = (hk ^ 0xFF) * 16777619u; } } }
                    }
                    int f = 0; for (int q = 0; q < ns; q++) if (seen[q] == hk) { f = 1; break; }
                    if (!f) { if (ns < 64) seen[ns] = hk; ns++; }
                    lines++; i += l + 1;
                }
                shaped = lines >= 8 && (ns <= 8 || ns * 16 <= lines);   /* a handful of layouts, or at most one per 16 records */
            }
            if (sp && o && shaped) {
                size_t sn = json_split(s, mm, sp); int64_t tn = log_encode_m(sp, sn, o, 2 * mm + 64, 1);
                if (tn > 0) {
                    size_t a = kprobe_size(o, (size_t)tn), b = kprobe_size(s, mm);
                    if (a * 100 < CZ_LOG_GAIN_PCT * b) { pl.t = T_LOG; pl.a = 2; }
                }
            }
            free(sp); free(o);
            if (pl.t == T_LOG) return pl;
        }
        int js = logs && n >= 65536 ? json_lines(s, h) : 0;
        int logc = 0; size_t loga = 0, logb = 0;  /* the JSON trial's log-token result, reused by the log trial below */
        if (js == 1 && !strong && n > (64u << 10)) {  /* pretty JSON: a 64 KiB look first; no gain there, no full trial */
            size_t m0 = 64u << 10; while (m0 > 0 && s[m0 - 1] != '\n') m0--;
            uint8_t *o = malloc(m0 + 64);
            int gain = 1;
            if (o && m0 >= 16384) {
                size_t b = kprobe_size(s, m0); gain = 0;
                for (int jm = 1; jm >= 0 && !gain; jm--) {
                    int64_t tn = log_encode_m(s, m0, o, m0 + 64, jm); if (tn <= 0) continue;
                    size_t a = kprobe_size(o, (size_t)tn); gain = a * 100 < b * 100;
                }
            }
            free(o);
            if (!gain) js = 0;
        }
        if (js == 2 || (js == 1 && !strong)) {   /* JSON: values become columns (max/ultra model pretty JSON well raw) */
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            if (mm >= 16384) {                   /* JSON tokens, or log tokens when values hold free text: the smaller */
                uint8_t *o = malloc(mm + 64);
                if (o) {
                    size_t b = kprobe_size(s, mm), best = b * CZ_LOG_GAIN_PCT / 100;
                    for (int jm = 1; jm >= 0; jm--) {
                        int64_t tn = log_encode_m(s, mm, o, mm + 64, jm);
                        if (jm == 0) { logc = 1; loga = (size_t)-1; logb = b; }
                        if (tn <= 0) continue;
                        size_t a = kprobe_size(o, (size_t)tn);
                        if (jm == 0) { loga = a; logc = 2; }
                        if (a < best) { best = a; pl.t = T_LOG; pl.a = (uint8_t)jm; }
                    }
                }
                free(o);
                if (pl.t == T_LOG) return pl;
            }
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        fprintf(stderr, "text: json_ms=%.3f js=%d\n", (prof_now() - tx0) * 1000, js); tx0 = prof_now();
#endif
        static const struct { uint8_t d; int q; } D[] = {{',', 1}, {'\t', 0}, {';', 1}, {'|', 0}, {' ', 1}};
        int *cnt = malloc(4001 * sizeof(int)); size_t head = n < (1 << 16) ? n : (1 << 16);
        if (!cnt) return pl;
        for (int di = 0; di < 5 && pl.t == T_NONE; di++) {
            int L = tp_fieldcounts(s, (int64_t)head, D[di].d, D[di].q, cnt, 4000);
            /* Version 3 can store 255 individual fields and the complete row
             * suffix in its final lane. A literal delimiter split is reversible
             * even if later blocks contain quotes or change width. */
            if (logs && !effort && L >= 2 && L < 9 && cnt[1] > 256 && n > head) {
                size_t probe = n < (256u << 10) ? n : (256u << 10);
                L = tp_fieldcounts(s, probe, D[di].d, D[di].q, cnt, 4000);
            }
            if (logs && !effort && L >= 9 && cnt[1] > 256) {
                int r = 2; while (r < L && cnt[r] == cnt[1]) r++;
                if (r == L) {
                    free(cnt);
                    return (plan_t){.t=T_TP, .a=D[di].d, .c=255, .wide=1};
                }
            }
            if (L < 51) continue;
            int best = 0, bc = 0;
            for (int i = 1; i < L; i++) { int k = cnt[i], c = 0; if (k == best) continue; for (int j = 1; j < L; j++) c += cnt[j] == k; if (c > bc) { bc = c; best = k; } }
            if (best >= 2 && best <= 256 && bc * 100 > (L - 1) * 95) { pl.t = T_TP; pl.a = D[di].d; pl.b = D[di].q; pl.c = best - 1; }
        }
        free(cnt);
        size_t logcost = 0;                                /* the template transform's coded size on the trial slice */
        if (logs && pl.t == T_TP && pl.a == ' ' && n >= 65536) {
            /* a log whose head splits into space-separated fields looks like a table, and the table plan used to end the
             * search before the template transform was tried: Loghub BGL coded 2.5x larger at -3. For space-separated
             * fields (the only delimiter where a log reads as a table) both transforms are
             * scored on the same slice, and the template transform replaces the table only when it is at least 10%
             * smaller (the block codes a table with its field set, which the probe here does not see) */
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            uint8_t *o = mm >= 16384 ? malloc(mm + TSLACK) : NULL, *o2 = o ? malloc(mm + TSLACK) : NULL;
            if (o && o2) {
                int64_t tl = log_encode(s, mm, o, mm + 64);
                int64_t tt = tbl_encode(s, mm, (uint8_t)pl.a, pl.c + 1, pl.b, o2);
                size_t a = tl > 0 ? kprobe_size(o, (size_t)tl) : (size_t)-1, b = tt > 0 ? kprobe_size(o2, (size_t)tt) : kprobe_size(s, mm);
                if (tl > 0 && a * 100 < CZ_LOG_GAIN_PCT * kprobe_size(s, mm) && a * 100 < 90 * (uint64_t)b) {
                    pl.t = T_LOG; pl.a = pl.b = pl.c = 0; pl.wide = 0; logcost = a;
                }
            }
            free(o); free(o2);
        }
        if (pl.t == T_TP) {                       /* typed table transform, kept only if it helps on a sample */
            size_t mm = head; while (mm > 0 && s[mm - 1] != '\n') mm--; if (!mm) mm = head;
            uint8_t *o = malloc(mm + TSLACK);
            if (!o) return pl;
            int64_t tn = tbl_encode(s, mm, (uint8_t)pl.a, pl.c + 1, pl.b, o);
            size_t a = tn > 0 ? kprobe_size(o, (size_t)tn) : (size_t)-1, b = kprobe_size(s, mm);
            if (tn > 0 && a * 100 < CZ_TBL_GAIN_PCT * b) {
                pl.t = T_TBL; pl.fs = malloc(sizeof(fset_t));
                if (pl.fs) fset_discover(s, n, (uint8_t)pl.a, pl.c + 1, pl.b, effort, pl.fs, threads);
            }
            free(o);
        }
        if (logs && pl.t == T_NONE && n >= 65536) {        /* logs: template transform, kept if it helps on a sample */
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            if (mm >= 16384 && logc) {           /* same slice already coded by the JSON trial */
                if (logc == 2 && loga * 100 < CZ_LOG_GAIN_PCT * logb) { pl.t = T_LOG; logcost = loga; }
            } else if (mm >= 16384) {
                uint8_t *o = malloc(mm + 64);
                if (!o) return pl;
                int64_t tn = log_encode(s, mm, o, mm + 64);
                if (tn > 0) {
                    size_t a = kprobe_size(o, (size_t)tn), b = kprobe_size(s, mm);
                    if (a * 100 < CZ_LOG_GAIN_PCT * b) { pl.t = T_LOG; logcost = a; }
                }
                free(o);
            }
        }
        /* balanced: lines of a few fixed lengths, stored column by column. Taken when it codes the slice at least as
         * small as the template transform (or, where that did not apply, clears the same bar): it encodes and decodes
         * several times faster. */
        if (logs && numtrial && n >= 65536 && (pl.t == T_NONE || (pl.t == T_LOG && pl.a == 0 && logcost))) {
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            uint8_t *o = mm >= 16384 ? malloc(mm + TSLACK) : NULL;
            int64_t tn = o ? lines_encode(s, mm, o, mm + TSLACK) : -1;
            if (tn > 0) {
                size_t a = kprobe_size(o, (size_t)tn), b = pl.t == T_LOG ? 0 : kprobe_size(s, mm);
                if (pl.t == T_LOG ? a <= logcost : a * 100 < CZ_LOG_GAIN_PCT * b) { pl.t = T_LINES; pl.a = pl.b = pl.c = 0; }
            }
            free(o);
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        fprintf(stderr, "text: table_log_ms=%.3f t=%d\n", (prof_now() - tx0) * 1000, pl.t);
#endif
        if (logs && effort && pl.t == T_NONE && n >= 65536) {   /* prose and markup (eco and above): word dictionary */
            size_t mm = n < (2u << 20) ? n : (2u << 20);
            uint8_t *o = malloc(mm + 64);
            if (!o) return pl;
            int64_t tn = word_encode(s, mm, o, mm + 64);
            if (tn > 0) {
                size_t a = kprobe_size_l(o, (size_t)tn, 3), b = kprobe_size_l(s, mm, 3);
                if (a * 100 < CZ_WORD_GAIN_PCT * b) pl.t = T_WORD;
            }
            free(o);
        }
        return pl;
    }
    /* binary: numeric byte-plane shuffle, chosen by a zstd -1 trial on a 32 KB slice */
    if (light == 4) light = 1;
    size_t mid = PLAN_SLICE(32768) > 32768 ? 0 : n / 2, sl = n - mid < PLAN_SLICE(32768) ? n - mid : 32768;
    const uint8_t *smp = s + mid;
    int wonly = 0;                                        /* minimal analysis: at most one word width goes to the trials */
    if (light >= 2 && sl >= 4096) {
        /* a shuffle can only pay where some byte plane is more predictable than the bytes as a whole: compare the
         * mean plane entropy per width with the plain byte entropy on the sample (microseconds; no coder runs) */
        double h1 = kg_plane_h(smp, sl, 1, 0, 0), hb = 1e9;
        for (int w = 2; w <= 8; w *= 2) { double e = 0; for (int b = 0; b < w; b++) e += kg_plane_h(smp, sl / w, w, b, 0); e /= w; if (e < hb) { hb = e; wonly = w; } }
        if (hb > (light == 3 ? 0.75 : 0.93) * h1) return pl;  /* (speed mode asks for a plane a quarter below the byte entropy) */                     /* no plane stands out: plain coding, and no coder was run */
        if (light == 3) {                                   /* speed mode: KLZ passes over incompressible input at several GB/s;
                                                             * a shuffle that makes such data compressible costs most of that speed.
                                                             * The test is the size probe (LZ4 before version 12) */
            if (kprobe_size(smp, sl) * 100 >= sl * 98) return pl;
        }
    }
    int noshuf = 0;
    if (numtrial && sl >= 4096) {                       /* balanced: the 28 shuffle trials only where some byte plane is
                                                         * more predictable than the bytes as a whole (see above) */
        double h1 = kg_plane_h(smp, sl, 1, 0, 0), hb = 1e9;
        for (int w = 2; w <= 8; w *= 2) { double e = 0; for (int b = 0; b < w; b++) e += kg_plane_h(smp, sl / w, w, b, 0); e /= w; if (e < hb) { hb = e; wonly = w; } }   /* and only at the width whose planes are most predictable */
        noshuf = hb > SHUF_SCREEN * h1;
    }
    uint8_t *t = malloc(sl ? sl : 1);
    if (!t) return pl;
    size_t base = noshuf ? 0 : kprobe_size(smp, sl);
    if (!noshuf && base * 4 >= sl && wonly >= 0) {
        double best = base * 0.95;
        for (int w = 2; w <= 8; w *= 2) for (int c = 1; c <= 8; c++) {
            if (wonly && w != wonly) continue;
            if (c == 5 || c == 6 || c == 7 || w * c > 32) continue;
            for (int dl = 1; dl >= 0; dl--) {
                size_t rows = sl / (w * c); if (!rows) continue;
                shuffle_fwd(smp, rows, w, c, dl, t);
                size_t z = kprobe_size(t, rows * w * c);
                if (z < best) { best = z; pl.t = T_SHUF; pl.a = w; pl.b = c; pl.c = dl; }
            }
        }
    }
    free(t);
    /* structure-aware candidates (16-bit image, fixed records) against the shuffle and plain coding, scored by zstd -3
     * on a slice of up to 1 MiB from the middle of the input; kept only when at least 3% smaller */
    if (structured && n >= (1u << 18)) {
        size_t LM = PLAN_SLICE(light && !numtrial ? (1u << 18) : (1u << 20)), L = PLAN_SLICE(n / 4) < LM ? n / 4 : LM > n ? n : LM, st = (n / 2 - L / 2) & ~(size_t)7;
        const int zl = light ? 1 : 3;                      /* scoring level */
        /* balanced keeps the 1 MiB slice of the full analysis: on a 47 MB weights file the 256 KiB slice picked a layout
         * that coded 0.8% larger, and the slice costs under a millisecond at level 1 */
        size_t bc = L * 2 + 1024; uint8_t *buf = malloc(bc);
        if (!buf) return pl;
        double plain = (double)zcost(s + st, L, zl) / L, best = plain * CZ_STRUCT_GAIN_PCT / 100.0; plan_t keep = pl; int chosen = 0;
        /* Ownership survives replacement of the tentative winning plan (same repair as fix/planner-record-ownership). */
        rec_plan_t *owned_rec = NULL;
        double d64c = 1e300; uint32_t d64cols = 0; size_t d64h = 0;   /* the DEC64 trial's result, kept for a marked plan */
        if (pl.t == T_SHUF) {
            size_t u = (size_t)pl.a * pl.b, rows = L / u; shuffle_fwd(s + st, rows, pl.a, pl.b, pl.c, buf);
            double c = (double)zcost(buf, rows * u, zl) / (rows * u);
            if (c < best) { best = c; chosen = 1; }
        }
        img_plan_t im;
        int code = 0;                                      /* balanced: machine code where the call/jump transform wins is
                                                            * not an image, a record file or a number array */
        if (numtrial && pl.t != T_SHUF) {
            size_t ej = 0; for (size_t i = 0; i + 4 < L; i++) ej += ((s[st + i] & 0xFE) == 0xE8) & ((uint8_t)(s[st + i + 4] + 1) < 2);
            if (ej * 1000 >= L) {
                memcpy(buf, s + st, L); e8e9_fwd(buf, L);
                double c = (double)zcost(buf, L, zl) / L;
                if (c < best) { best = c; keep = pl; keep.t = T_X86; chosen = 4; code = 1; }
            }
        }
        if (code) goto structured_done;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        double image_start = prof_now();
#endif
        if (img16_detect(s, n, &im)) {
            int64_t e = img16_encode(s + st, L, st, &im, buf);
            if (e > 0) { double c = (double)zcost(buf, (size_t)e, zl) / L; if (c < best) { best = c; keep = pl; keep.t = T_IMG; keep.img = im; chosen = 2; } }
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        double image_ms = (prof_now() - image_start) * 1000;
        double record_start = prof_now();
#endif
        /* balanced: a layout that already saves a fifth is the answer; the remaining trials are alternatives to it */
        if (numtrial && chosen == 2 && best < plain * 0.80 && !native) goto structured_done;   /* native leaf: the integer graph may still win */
        /* balanced types the data first: the float, integer, decimal and image trials run before the record search,
         * and a typed plan that saves a fifth is the answer (the search is the largest fixed cost of planning) */
#define REC_TRIAL(BAR) do { \
        rec_plan_t *R = malloc(sizeof(rec_plan_t)); \
        if (R && rec_detect(s, n, effort, light ? 6 : 4, R)) {   /* layouts are searched only for a clear record period (4x the median lag; quick analysis 6x) */ \
            size_t rs = (size_t)R->h + (st > (size_t)R->h ? (st - R->h) / R->P * R->P : 0); if (!rs) rs = R->P; \
            size_t rl = L / R->P * R->P; \
            if (rs + rl <= n) { \
                int64_t e = rec_encode(s + rs, rl, rs, R, buf, bc); \
                if (e > 0) { double c = (double)zcost(buf, (size_t)e, zl) / rl; if (c < (BAR)) { best = c; keep = pl; keep.t = T_REC; keep.rec = R; owned_rec = R; R = NULL; chosen = 3; } } \
            } \
        } \
        free(R); \
    } while (0)
        if (!numtrial) REC_TRIAL(best);
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        fprintf(stderr, "structure bytes=%zu image_ms=%.3f record_ms=%.3f\n", n, image_ms, (prof_now() - record_start) * 1000);
#endif
        {   /* executables: x86 call/jump transform, tried only where call/jump opcodes with a plausible 32-bit
             * displacement (top byte 00 or FF) are at least 1 in 1000 bytes; other data has about 1 in 16000 */
            size_t ej = 0; for (size_t i = 0; i + 4 < L; i++) ej += ((s[st + i] & 0xFE) == 0xE8) & ((uint8_t)(s[st + i + 4] + 1) < 2);
            if (ej * 1000 >= L) {
            memcpy(buf, s + st, L); e8e9_fwd(buf, L);
            double c = (double)zcost(buf, L, zl) / L;
            if (c < best && chosen < 2 && pl.t != T_SHUF) { best = c; keep = pl; keep.t = T_X86; chosen = 4; }
            else if (c < best && chosen == 0) { best = c; keep = pl; keep.t = T_X86; chosen = 4; }
            }
        }
        {   /* float arrays first (FLOAT node); integer arrays (INT node) only when the data does not look like floats */
            const size_t Lg = effort || L <= PLAN_SLICE(256u << 10) ? L : (256u << 10);     /* fast: graph trials on 256 KiB */
            double plg = -1;                                    /* a shorter slice is compared by its gain over plain coding of that slice */
#define KG_ADJ(C) ((C) > 1e299 || Lg == L ? (C) : (C) * plain / (plg > 0 ? plg : (plg = (double)zcost(s + st, Lg, zl) / Lg)))
            /* the float trial keeps the full slice: its pre-check is cheap, and zstd's plain cost on 256 KiB misleads on float data */
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double kgt0 = prof_now();
#endif
            const int npy = (n > 16 && !memcmp(s, "\x93NUMPY", 6)) || imghdr;   /* quick analysis: integer and image trials only for a .npy or image header */
            size_t iw = 0, ih = 0; unsigned ic = 0; double ci = 1e300; int imgdone = 0;
            const size_t Li = light && !npy && Lg > (64u << 10) ? (64u << 10) : Lg;   /* quick analysis: a 64 KiB image trial */
#define IMG8_TRIAL() do { ci = kg_img8_trial(s, n, st, Li, zl, plain, &iw, &ic, &ih); \
                if (ci < 1e299 && Li != Lg) ci = ci * plain / ((double)zcost(s + st, Li, zl) / Li); else ci = KG_ADJ(ci); imgdone = 1; } while (0)
            /* balanced: the image trial first (its pre-check is the cheapest); an image layout that saves a fifth is the
             * answer and the float, integer and decimal trials are not run */
            if (numtrial) IMG8_TRIAL();
            if (imgdone && ci < best && ci < plain * 0.80 && !native) {   /* native leaf: the integer graph may still win */
                best = ci; keep = pl; keep.t = T_KG; keep.a = 0; keep.b = 32 | (int)ic; keep.c = (int)ih; keep.kgw = (uint32_t)iw; chosen = 7;
            } else {
            int w = 0; size_t hh = 0; double c = kg_float_trial(s, n, st, L, zl, plain, &w, &hh), cf = c; int fw = w; size_t fh = hh;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double kgt1 = prof_now();
#endif
            if (cf > 1e299 && (!light || npy || numtrial)) {
                w = 0; hh = 0; c = kg_num_trial(s, n, st, Lg, zl, plain, &w, &hh, native); c = KG_ADJ(c);
                if (c < best) { best = c; keep = pl; keep.t = T_KG; keep.a = 0; keep.b = w; keep.c = (int)hh; chosen = 5; }
            }
            c = cf; w = fw; hh = fh;
            if (c < best) { best = c; keep = pl; keep.t = T_KG; keep.a = 0; keep.b = w | 16; keep.c = (int)hh; chosen = 6; }
            { uint32_t dc = 0; size_t dh = 0; c = kg_dec64_trial(s, n, st, L, zl, &dc, &dh);     /* decimal doubles: decode is slower than the other graphs, so it must win by 7% */
              d64c = c; d64cols = dc; d64h = dh;
              if (c < best * 0.93) { best = c; keep = pl; keep.t = T_KG; keep.a = 0; keep.b = DEC_B(dc); keep.c = (int)dh; keep.kgw = DEC_KGW(dc); chosen = 8; } }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double kgt2 = prof_now();
#endif
            if (!imgdone) IMG8_TRIAL();                          /* 8-bit images: IMG8 */
            c = ci;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            fprintf(stderr, "kg float_ms=%.3f num_ms=%.3f img8_ms=%.3f\n", (kgt1 - kgt0) * 1000, (kgt2 - kgt1) * 1000, (prof_now() - kgt2) * 1000);
#endif
            if (c < best) { best = c; keep = pl; keep.t = T_KG; keep.a = 0; keep.b = 32 | (int)ic; keep.c = (int)ih; keep.kgw = (uint32_t)iw; chosen = 7; }
            }
#undef IMG8_TRIAL
        }
#undef KG_ADJ
        if (numtrial && !(chosen >= 5 && best < plain * 0.80)) {
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double rt0 = prof_now();
#endif
            REC_TRIAL(chosen == 8 ? best / 0.93 : best);          /* a decimal plan keeps its 7% margin over a record layout */
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            fprintf(stderr, "record late_ms=%.3f\n", (prof_now() - rt0) * 1000);
#endif
        }
#undef REC_TRIAL
structured_done:
        if (chosen >= 5 && n >= (1u << 20)) {
            /* a numeric graph was chosen on one slice from the middle. Four short probes elsewhere check that the
             * rest of the input is the same kind of data: where the graph codes a probe clearly larger than plain
             * coding (a pickle whose integer columns are followed by strings), the plan is marked, and every block is
             * then coded both ways and the smaller kept. Inputs that are one kind of data throughout pay only the probes. */
            static const int at16[4] = {1, 5, 11, 15}; const size_t E = 32u << 10;
            for (int q = 0; q < 4 && !keep.verify; q++) {
                size_t pos = (n / 16 * (size_t)at16[q]) & ~(size_t)7; if (pos + E > n) continue;
                double pe = (double)zcost(s + pos, E, zl) / E, c = 1e300; int w3 = 0; size_t h3 = 0, iw3 = 0; unsigned ic3 = 0; uint32_t dc3 = 0;
                if (chosen == 5) c = kg_num_trial(s, n, pos, E, zl, 1e300, &w3, &h3, native);
                else if (chosen == 6) c = kg_float_trial(s, n, pos, E, zl, 1e300, &w3, &h3);
                else if (chosen == 7) c = kg_img8_trial(s, n, pos, E, zl, 1e300, &iw3, &ic3, &h3);
                else c = kg_dec64_trial(s, n, pos, E, zl, &dc3, &h3);
                if (c > pe * 1.10 && pe < 0.98) keep.verify = 1;
            }
            /* a marked whole-file plan (one word size and alignment for everything) gives way to the segment plan when
             * that also beat plain coding on the slice: segments follow the arrays wherever they sit */
            if (keep.verify && chosen != 8 && d64c < plain * CZ_STRUCT_GAIN_PCT / 100.0) {
                keep = pl; keep.t = T_KG; keep.a = 0; keep.b = DEC_B(d64cols); keep.c = (int)d64h; keep.kgw = DEC_KGW(d64cols); keep.verify = 1; chosen = 8;
            }
        }
        if (owned_rec && chosen != 3) { free(owned_rec); if (keep.rec == owned_rec) keep.rec = NULL; }
        if (chosen >= 2) pl = keep;
        else if (pl.t == T_SHUF && n >= (1u << 20)) {      /* the same probes for a byte shuffle */
            static const int at16[4] = {1, 5, 11, 15}; const size_t u = (size_t)pl.a * pl.b, E = (32u << 10) / u * u;
            for (int q = 0; q < 4 && !pl.verify; q++) {
                size_t pos = n / 16 * (size_t)at16[q] / u * u; if (pos + E > n || E > bc) continue;
                double pe = (double)zcost(s + pos, E, zl) / E;
                shuffle_fwd(s + pos, E / u, pl.a, pl.b, pl.c, buf);
                if ((double)zcost(buf, E, zl) / E > pe * 1.10 && pe < 0.98) pl.verify = 1;
            }
        }
        if (0) {}
        else if (!chosen && pl.t == T_SHUF && plain * 0.97 <= best) { /* shuffle did not beat plain coding at zstd -3 */ }
        free(buf);
    }
    if (structured && n < (1u << 18)) {                    /* small arrays: the integer graph against plain and shuffle */
        const int zl = light && !numtrial ? 1 : 3; uint8_t *buf = malloc(n ? n : 1);
        if (buf) {
            double plain = (double)zcost(s, n, zl) / n, best = plain * CZ_STRUCT_GAIN_PCT / 100.0;
            if (pl.t == T_SHUF) {
                size_t u = (size_t)pl.a * pl.b, rows = n / u; shuffle_fwd(s, rows, pl.a, pl.b, pl.c, buf);
                double c = (double)zcost(buf, rows * u, zl) / (rows * u); if (c < best) best = c;
            }
            int w = 0; size_t hh = 0; double c = kg_num_trial(s, n, 0, n, zl, 1e300, &w, &hh, native);   /* small input: no pre-check */
            if (c < best) { best = c; pl.t = T_KG; pl.a = 0; pl.b = w; pl.c = (int)hh; }
            w = 0; hh = 0; c = kg_float_trial(s, n, 0, n, zl, plain, &w, &hh);
            if (c < best) { best = c; pl.t = T_KG; pl.a = 0; pl.b = w | 16; pl.c = (int)hh; }
            { uint32_t dc = 0; size_t dh = 0; c = kg_dec64_trial(s, n, 0, n, zl, &dc, &dh);
              if (c < best * 0.93) { best = c; pl.t = T_KG; pl.a = 0; pl.b = DEC_B(dc); pl.c = (int)dh; pl.kgw = DEC_KGW(dc); } }
            size_t iw = 0, ih = 0; unsigned ic = 0; c = kg_img8_trial(s, n, 0, n, zl, 1e300, &iw, &ic, &ih);
            if (c < best) { pl.t = T_KG; pl.a = 0; pl.b = 32 | (int)ic; pl.c = (int)ih; pl.kgw = (uint32_t)iw; }
        }
        free(buf);
    }
    /* an image (by its header) whose bytes look like text: the plain coding the text route would have given can be smaller
     * under the strong coders (one photograph: 1.7% at hc), so every block is coded both ways and the smaller kept */
    if (imghdr && textlike && pl.t == T_KG) pl.verify = 1;
    return pl;
}
