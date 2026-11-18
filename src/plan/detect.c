/* Content detection used by the planner and by the codecs (text, JSON). */
static int looks_text(const uint8_t *s, size_t n) {          /* >= 95% printable in 4 slices of 16 KB */
    if (n < 4096) return 0;
    size_t tot = 0, pr = 0;
    for (int k = 0; k < 4; k++) {
        size_t st = n / 4 * k, m = n - st < 16384 ? n - st : 16384;
        for (size_t i = st; i < st + m; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 0xC2; }
        tot += m;
    }
    return pr * 100 >= tot * 95;
}

/* text in any ASCII-compatible encoding (UTF-8 with many multi-byte characters, Latin-1), possibly with NUL padding
 * (tar headers): in 4 slices of 16 KB, at most 5% of the bytes are control bytes other than NUL, tab, newline and
 * carriage return, and at most 25% are NUL (eco's block-sorting route) */
static int looks_text8(const uint8_t *s, size_t n) {
    if (n < 4096) return 0;
    size_t tot = 0, ctl = 0, nul = 0;
    for (int k = 0; k < 4; k++) {
        size_t st = n / 4 * k, m = n - st < 16384 ? n - st : 16384;
        for (size_t i = st; i < st + m; i++) { uint8_t c = s[i]; nul += !c; ctl += (c && c < 32 && c != 9 && c != 10 && c != 13) || c == 127; }
        tot += m;
    }
    return ctl * 100 <= tot * 5 && nul * 100 <= tot * 25;
}

/* minified JSON: no newline in the sample, starts with { or [, and many records (",{" after "}") */
static int json_minified(const uint8_t *s, size_t h) {
    if (memchr(s, '\n', h)) return 0;
    size_t k = 0; while (k < h && (s[k] == ' ' || s[k] == '\t' || s[k] == '\r')) k++;
    if (k == h || (s[k] != '{' && s[k] != '[')) return 0;
    size_t rec = 0; for (size_t i = k; i + 2 < h; i++) rec += s[i] == '}' && s[i + 1] == ',' && (s[i + 2] == '{' || s[i + 2] == ' ');
    return rec >= 16;
}

/* JSON text: nearly every non-blank line of the sample starts (after indentation) with { [ " } or ], and some hold a ':'.
 * Returns 2 for JSON lines (records start with { or [), 1 for other (pretty-printed) JSON, 0 otherwise. */
static int json_lines(const uint8_t *s, size_t h) {
    size_t lines = 0, js = 0, colon = 0, rec = 0, i = 0;
    while (i < h) {
        const uint8_t *e = memchr(s + i, '\n', h - i); size_t end = e ? (size_t)(e - s) : h;
        size_t k = i; while (k < end && (s[k] == ' ' || s[k] == '\t' || s[k] == '\r')) k++;
        if (k < end) { lines++; uint8_t c = s[k]; js += c == '{' || c == '[' || c == '"' || c == '}' || c == ']'; rec += (c == '{' || c == '[') && end - k > 2; colon += memchr(s + k, ':', end - k) != NULL; }
        if (!e) break;
        i = end + 1;
    }
    if (lines < 8 || js * 100 < lines * 95 || colon * 4 < lines) return 0;
    return rec * 100 >= lines * 95 ? 2 : 1;
}
