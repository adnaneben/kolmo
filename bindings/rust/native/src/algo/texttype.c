/* Text kinds: a deterministic, dependency-free recogniser of the kind of a text (markup, JSON, delimited table, log,
 * C, Python, JavaScript, prose, SQL dump), from hand-coded scanners over a sample (the patterns a regular expression
 * would state, written as small state machines: dates and times, IPv4 addresses, tags, "key": pairs, keywords).
 * Used by the context-mixing coder to choose a per-kind prior (codecs/cm.c, docs/PRIORS.md); the kind it returns
 * only picks a starting point, so a wrong guess costs size, never correctness. The result depends on the bytes only. */
enum { TT_NONE = 0, TT_MARKUP, TT_JSON, TT_CSV, TT_LOG, TT_C, TT_PY, TT_JS, TT_TEXT, TT_SQL, TT_COUNT };
static const char *const tt_name[TT_COUNT] = {"none", "markup", "json", "csv", "log", "c", "py", "js", "text", "sql"};

static inline int tt_dig(uint8_t c) { return c >= '0' && c <= '9'; }
static inline int tt_alpha(uint8_t c) { return (uint8_t)((c | 32) - 'a') < 26; }
/* n occurrences of the word w (ASCII, compared without case when nocase) in s, at a word start */
static size_t tt_count(const uint8_t *s, size_t n, const char *w, int nocase) {
    size_t k = strlen(w), c = 0;
    if (n < k) return 0;
    for (size_t i = 0; i + k <= n; i++) {
        if (i && (tt_alpha(s[i - 1]) || s[i - 1] == '_') && tt_alpha((uint8_t)w[0])) continue;
        size_t j = 0;
        if (nocase) while (j < k && (s[i + j] | 32) == ((uint8_t)w[j] | 32)) j++;
        else while (j < k && s[i + j] == (uint8_t)w[j]) j++;
        if (j == k) { c++; i += k - 1; }
    }
    return c;
}
/* a time or date near the start of a line: hh:mm:ss, yyyy-mm-dd or yyyy/mm/dd, dd/Mon/yyyy, a month name and a day,
 * yymmdd hhmmss (HDFS), a run of 9+ digits (epoch times), or an IPv4 address in the first field (access logs) */
static int tt_stamp(const uint8_t *s, size_t n) {
    static const char *const mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (n > 48) n = 48;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *p = s + i; size_t r = n - i;
        if (r >= 8 && tt_dig(p[0]) && tt_dig(p[1]) && p[2] == ':' && tt_dig(p[3]) && tt_dig(p[4]) && p[5] == ':' && tt_dig(p[6]) && tt_dig(p[7])) return 1;
        if (r >= 10 && tt_dig(p[0]) && tt_dig(p[1]) && tt_dig(p[2]) && tt_dig(p[3]) && (p[4] == '-' || p[4] == '/' || p[4] == '.') && tt_dig(p[5]) && tt_dig(p[6]) && p[7] == p[4] && tt_dig(p[8]) && tt_dig(p[9])) return 1;
        if (r >= 13 && tt_dig(p[0]) && tt_dig(p[1]) && tt_dig(p[2]) && tt_dig(p[3]) && tt_dig(p[4]) && tt_dig(p[5]) && p[6] == ' ' && tt_dig(p[7]) && tt_dig(p[8]) && tt_dig(p[9]) && tt_dig(p[10]) && tt_dig(p[11]) && tt_dig(p[12])) return 1;
        if (r >= 6 && (i == 0 || !tt_alpha(p[-1])))
            for (int k = 0; k < 12; k++) if (p[0] == (uint8_t)mon[k][0] && p[1] == (uint8_t)mon[k][1] && p[2] == (uint8_t)mon[k][2] && (p[3] == ' ' || p[3] == '/') && (tt_dig(p[4]) || (p[4] == ' ' && tt_dig(p[5])))) return 1;
        if (i == 0 || !tt_dig(p[-1])) { size_t d = 0; while (d < r && tt_dig(p[d])) d++; if (d >= 9) return 1; }
    }
    /* IPv4 at the line start */
    size_t i = 0; int parts = 0;
    while (parts < 4) { size_t d = 0; while (i < n && tt_dig(s[i]) && d < 4) { i++; d++; } if (!d || d > 3) break; parts++; if (parts < 4) { if (i < n && s[i] == '.') i++; else break; } }
    return parts == 4 && i < n && s[i] == ' ';
}
/* the kind of a text from its first 64 KiB (TT_NONE when it is not text or matches no kind clearly) */
static int text_kind(const uint8_t *s0, size_t n0) {
    const size_t n = n0 < (64u << 10) ? n0 : 64u << 10; const uint8_t *s = s0;
    if (n < 64) return TT_NONE;
    size_t pr = 0, al = 0, sp = 0, sym = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = s[i];
        pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 128;
        al += tt_alpha(c) || c >= 128; sp += c == ' ' || c == '\n';
        sym += c == '{' || c == '}' || c == '<' || c == '>' || c == ';' || c == '=' || c == '[' || c == ']' || c == '(' || c == ')';
    }
    if (pr * 100 < n * 95) return TT_NONE;
    /* lines: count, lines with a time stamp near the start, field counts of the four usual delimiters */
    static const uint8_t D[4] = {',', ';', '\t', '|'};
    uint32_t cnt[4][65]; memset(cnt, 0, sizeof cnt);
    size_t lines = 0, stamped = 0, colon_end = 0, semi_end = 0, brace_end = 0, def_start = 0;
    for (size_t i = 0; i < n;) {
        const uint8_t *e = memchr(s + i, '\n', n - i); size_t l = e ? (size_t)(e - s) - i : n - i;
        if (!e && lines) break;                              /* a cut last line is not counted */
        const uint8_t *p = s + i; uint32_t f[4] = {1, 1, 1, 1};
        for (size_t k = 0; k < l; k++) for (int d = 0; d < 4; d++) f[d] += p[k] == D[d];
        for (int d = 0; d < 4; d++) cnt[d][f[d] < 64 ? f[d] : 64]++;
        stamped += tt_stamp(p, l);
        size_t q = l; while (q && (p[q - 1] == ' ' || p[q - 1] == '\r' || p[q - 1] == '\t')) q--;
        if (q) { colon_end += p[q - 1] == ':'; semi_end += p[q - 1] == ';'; brace_end += p[q - 1] == '{' || p[q - 1] == '}'; }
        size_t b = 0; while (b < l && (p[b] == ' ' || p[b] == '\t')) b++;
        if (l - b >= 4 && (!memcmp(p + b, "def ", 4) || (l - b >= 6 && !memcmp(p + b, "class ", 6)) || (l - b >= 7 && !memcmp(p + b, "import ", 7)) || (l - b >= 5 && !memcmp(p + b, "from ", 5)))) def_start++;
        lines++; i += l + 1;
    }
    if (!lines) return TT_NONE;
    /* markup: tags */
    size_t tags = 0, kq = 0;
    for (size_t i = 0; i + 1 < n; i++) {
        if (s[i] == '<' && (tt_alpha(s[i + 1]) || s[i + 1] == '/' || s[i + 1] == '!' || s[i + 1] == '?')) tags++;
        if (s[i] == '"') { size_t j = i + 1; while (j < n && (s[j] == ' ' || s[j] == '\t')) j++; if (j < n && s[j] == ':' && i && s[i - 1] != '\\') kq++; }
    }
    size_t fw = 0; while (fw < n && (s[fw] == ' ' || s[fw] == '\t' || s[fw] == '\r' || s[fw] == '\n' || s[fw] == 0xEF || s[fw] == 0xBB || s[fw] == 0xBF)) fw++;
    size_t jp = 0; for (size_t i = 0; i < n; i++) jp += s[i] == '{' || s[i] == '}' || s[i] == '[' || s[i] == ']' || s[i] == ',' || s[i] == ':';
    if (fw < n && (s[fw] == '{' || s[fw] == '[') && (kq * 400 >= n || (kq && jp * 20 >= n))) return TT_JSON;
    if (fw < n && s[fw] == '<' && tags * 200 >= n) return TT_MARKUP;
    if (tags * 100 >= n) return TT_MARKUP;
    /* SQL dumps: statements and tuples */
    size_t sqlk = tt_count(s, n, "INSERT INTO", 1) + tt_count(s, n, "CREATE TABLE", 1) + tt_count(s, n, "VALUES", 1) + tt_count(s, n, "DROP TABLE", 1);
    size_t tup = 0; for (size_t i = 0; i + 2 < n; i++) tup += s[i] == ')' && s[i + 1] == ',' && (s[i + 2] == '(' || (s[i + 2] == '\n' && i + 3 < n && s[i + 3] == '('));
    size_t sqls = tt_count(s, n, "INSERT INTO", 1) + tt_count(s, n, "CREATE TABLE", 1);
    if (sqls >= 2 && sqlk >= 3 && (tup * 400 >= n || sqlk * 5 >= lines)) return TT_SQL;
    /* delimited table: most lines have the same number of fields (3 or more) */
    if (lines >= 8) for (int d = 0; d < 4; d++) { int b = 0; for (int k = 3; k < 64; k++) if (cnt[d][k] > cnt[d][b]) b = k; if (b >= 3 && cnt[d][b] * 10 >= lines * 8) return TT_CSV; }
    if (lines >= 8 && stamped * 10 >= lines * 6) return TT_LOG;
    /* source code: keyword and line-shape scores per line */
    double L = (double)lines;
    double c = (tt_count(s, n, "#include", 0) * 4 + tt_count(s, n, "#define", 0) * 2 + tt_count(s, n, "#if", 0) + tt_count(s, n, "static", 0) + tt_count(s, n, "->", 0)
                + tt_count(s, n, "struct", 0) + tt_count(s, n, "int", 0) + tt_count(s, n, "/*", 0) + semi_end + brace_end) / L;
    double py = (def_start * 3 + tt_count(s, n, "self.", 0) + tt_count(s, n, "self,", 0) + tt_count(s, n, "elif", 0) * 2 + tt_count(s, n, "None", 0) + tt_count(s, n, "True", 0)
                 + tt_count(s, n, "\"\"\"", 0) + colon_end * 2) / L - (double)semi_end * 2 / L;
    double js = (tt_count(s, n, "function", 0) * 2 + tt_count(s, n, "const ", 0) + tt_count(s, n, "var ", 0) + tt_count(s, n, "let ", 0) + tt_count(s, n, "=>", 0) * 2
                 + tt_count(s, n, "===", 0) * 2 + tt_count(s, n, "!==", 0) * 2 + tt_count(s, n, "this.", 0) + tt_count(s, n, "return", 0) + tt_count(s, n, "undefined", 0) * 2
                 + tt_count(s, n, "require(", 0) + tt_count(s, n, "export ", 0) + semi_end + brace_end) / L - tt_count(s, n, "#include", 0) * 4 / L - tt_count(s, n, "->", 0) / L;
    double best = c; int k = TT_C;
    if (py > best) { best = py; k = TT_PY; }
    if (js > best) { best = js; k = TT_JS; }
    if (best >= 0.35) return k;
    if ((al + sp) * 100 >= n * 85 && sym * 100 <= n) return TT_TEXT;
    return TT_NONE;
}
