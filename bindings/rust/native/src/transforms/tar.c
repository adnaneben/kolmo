/* tar archives: regular-file members of at least 1 MiB become their own segments (own plan); else one segment */
static uint32_t segments(const uint8_t *s, size_t n, uint64_t *ss, uint64_t *se, uint32_t maxs) {
    uint32_t k = 0; size_t off = 0, last = 0;
    if (n >= 1024 && !memcmp(s + 257, "ustar", 5)) {
        while (off + 512 <= n && k + 2 < maxs) {
            const uint8_t *h = s + off; int zero = 1; for (int i = 0; i < 512 && zero; i++) zero = !h[i];
            if (zero) break;
            if (h[124] & 0x80) break;
            uint64_t size = 0; int ok = 1;
            for (int i = 124; i < 136; i++) { uint8_t c = h[i]; if (c == 0 || c == ' ') continue; if (c < '0' || c > '7') { ok = 0; break; } size = size * 8 + (c - '0'); }
            if (!ok || off + 512 + size > n) break;
            if ((h[156] == '0' || h[156] == 0) && size >= (1u << 20)) {
                if (off + 512 > last) { ss[k] = last; se[k] = off + 512; k++; }
                ss[k] = off + 512; se[k] = off + 512 + size; k++; last = off + 512 + size;
            }
            off += 512 + (size + 511) / 512 * 512;
        }
    }
    if (last < n || k == 0) { ss[k] = last; se[k] = n; k++; }
    return k;
}
