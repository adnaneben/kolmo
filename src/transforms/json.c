/* Minified JSON (no newline at all): a newline is inserted after every ',' that follows '}' or ']' outside strings, so
 * each record becomes a line for the JSON template transform. The decoder removes every newline again, so the split
 * is lossless whatever the input (a block that starts inside a string only splits worse). Returns the new length. */
static size_t json_split(const uint8_t *s, size_t n, uint8_t *d) {
    size_t o = 0, i = 0;
    if (n && s[0] != '[' && s[0] != '{') {             /* a block may start inside a string: sync at the first "},{" */
        while (i + 2 < n && !(s[i] == '}' && s[i + 1] == ',' && s[i + 2] == '{')) i++;
        if (i + 2 >= n) i = n;
        memcpy(d, s, i); o = i;
    }
    const size_t i0 = i;
    while (i < n) {                                     /* runs between quotes and commas are copied whole */
        size_t j = i; while (j < n && s[j] != '"' && s[j] != ',') j++;
        memcpy(d + o, s + i, j - i); o += j - i; i = j;
        if (i >= n) break;
        if (s[i] == ',') {                              /* a newline after a ',' whose last non-blank byte is } or ] */
            d[o++] = ','; uint8_t lst = 0;
            for (size_t b2 = i; b2 > i0;) { uint8_t c = s[--b2]; if (c != ' ' && c != '\t' && c != '\r') { lst = c; break; } }
            if (lst == '}' || lst == ']') d[o++] = '\n';
            i++; continue;
        }
        size_t st = ++i;                                /* a string: its closing quote has an even run of backslashes before it */
        for (;;) {
            const uint8_t *q = i < n ? memchr(s + i, '"', n - i) : NULL;
            if (!q) { i = n; break; }
            size_t qi = (size_t)(q - s), bs = 0; while (qi - bs > st && s[qi - 1 - bs] == '\\') bs++;
            i = qi + 1; if (!(bs & 1)) break;
        }
        d[o++] = '"'; memcpy(d + o, s + st, i - st); o += i - st;
    }
    return o;
}
