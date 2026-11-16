/* Byte histogram over four tables: one table stalls on runs of the same byte (each count waits for the last). */
static void kg_hist(const uint8_t *p, size_t n, uint32_t hc[256]) {
    uint32_t h1[256] = {0}, h2[256] = {0}, h3[256] = {0}; size_t i = 0;
    memset(hc, 0, 256 * sizeof(uint32_t));
    for (; i + 4 <= n; i += 4) { hc[p[i]]++; h1[p[i + 1]]++; h2[p[i + 2]]++; h3[p[i + 3]]++; }
    for (; i < n; i++) hc[p[i]]++;
    for (int c = 0; c < 256; c++) hc[c] += h1[c] + h2[c] + h3[c];
}
