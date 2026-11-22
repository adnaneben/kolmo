/* Plane-aligned leaves (research R2/R3 and P2-E11/P2-E13 on PR #7): a stream of np equal byte planes (integer
 * residuals) coded plane by plane, so that no Huffman or rANS chunk mixes two planes' statistics. The chunks are
 * concatenated: the unchanged leaf decoders read them as one leaf. Returns 0 when a plane does not code. */
static size_t leaf_compress_planes(int codec, const uint8_t *in, size_t n, int np, uint8_t *out, size_t cap) {
    size_t L = n / (size_t)np, o = 0;
    for (int b = 0; b < np; b++) {
        size_t r = codec == C_HUF ? huf_compress_all(in + (size_t)b * L, L, out + o, cap - o)
                 : codec == C_KHUF ? kolmo_khuf_compress_chunks(in + (size_t)b * L, L, out + o, cap - o)   /* KOLMO_NATIVE=huf */
                                  : rans_compress_all_x(in + (size_t)b * L, L, out + o, cap - o, 1);
        if (!r) return 0;
        o += r;
    }
    return o;
}
