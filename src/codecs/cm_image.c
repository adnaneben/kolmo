/* Image model (block codec 17): 8-bit samples in rows of iw bytes, ich interleaved channels, after ih bytes that are
 * not pixels. Stream: iw u32 | ih u32 | ich u8 | size word as above (always model 2 tables) | coded bytes. */
static size_t cm_compress_img(const uint8_t *in, size_t n, uint32_t iw, uint32_t ich, uint32_t ih, uint8_t *out, size_t cap) {
    if (cap < 20 || n >= ((size_t)1 << 28) || !iw || !ich || ich > 4 || iw < ich || ih > n) return 0;
    int bb = cm_bits_for(n);
    uint8_t *hist = malloc(n + 8); cm_t *m = hist ? cm_new_x(hist, bb, 1, 0, 1) : NULL;
    if (!m) { free(hist); return 0; }
    m->iw = iw; m->ich = ich; m->ih = ih;
    memcpy(out, &iw, 4); memcpy(out + 4, &ih, 4); out[8] = (uint8_t)(ich | 0x80);   /* bit 7: exact logistic tables */
    uint32_t n32 = (uint32_t)n | (uint32_t)((bb - 15) | 8) << 28; memcpy(out + 9, &n32, 4);
    size_t o = 4;
    int r = cm_enc_loop(m, in, n, out + 9, cap - 9, &o, 2);
    cm_free(m); free(hist);
    return r ? 0 : o + 9;
}
static int cm_decompress_img(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 17 || n >= ((size_t)1 << 28)) return -1;
    uint32_t iw, ih, n32; memcpy(&iw, in, 4); memcpy(&ih, in + 4, 4); uint32_t ich = in[8] & 0x7f; int lx = in[8] >> 7; memcpy(&n32, in + 9, 4);
    uint32_t f = n32 >> 28; n32 &= 0x0FFFFFFFu; int bb = (int)(f & 7) + 15;
    if (n32 != n || !(f & 8) || bb < 16 || bb > 22 || !iw || !ich || ich > 4 || iw < ich || ih > n) return -1;
    cm_t *m = cm_new_x(out, bb, 1, 0, lx); if (!m) return -1;
    m->iw = iw; m->ich = ich; m->ih = ih;
    cm_dec_loop(m, in + 9, cn - 9, out, n, 2);
    cm_free(m);
    return 0;
}
#ifdef KOLMO_CODEC_REGISTRY
static int cm_image_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return cm_decompress_img(in, cn, out, n); }
static const kolmo_codec codec_cm_image = { C_CMI, "cm_image", NULL, NULL, cm_image_decompress, NULL };   /* written at block level */
#endif
