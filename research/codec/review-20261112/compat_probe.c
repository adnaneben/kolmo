/* Research review R1: existing-leaf framing feasibility, NOT production integration.
 * Calls complete, pinned HUF/rANS decoder functions staged from the core. No timing or release claims.
 * REVIEW_RANS adds a separate encoder with singleton normalization enabled.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include REVIEW_CORE
#include REVIEW_RANS

#define REVIEW_MAX_INPUT (16u << 20)
typedef size_t (*encoder_fn)(const uint8_t *, size_t, uint8_t *, size_t);
typedef int (*decoder_fn)(const uint8_t *, size_t, uint8_t *, size_t);

static size_t split_encode(encoder_fn fn, const uint8_t *in, size_t n,
                           unsigned planes, uint8_t *out, size_t cap) {
    if (!planes || n % planes) return 0;
    size_t off = 0, plane_len = n / planes;
    for (unsigned p = 0; p < planes; ++p) {
        size_t z = fn(in + (size_t)p * plane_len, plane_len, out + off, cap - off);
        if (!z || z > cap - off) return 0;
        off += z;
    }
    return off;
}
static int check_decode(decoder_fn fn, const uint8_t *in, size_t n,
                        const uint8_t *coded, size_t cn) {
    uint8_t *out = malloc(n ? n : 1);
    if (!out) return -1;
    int bad = fn(coded, cn, out, n) || memcmp(in, out, n);
    free(out);
    return bad ? -1 : 0;
}
static int selftest(void) {
    size_t singleton_cases = 0, mixed_cases = 0, plane_cases = 0;
    size_t max_n = 2u * RANS_CHUNK + 4;
    size_t cap = max_n * 2 + 65536;
    uint8_t *in = malloc(max_n), *old = malloc(cap), *candidate = malloc(cap);
    if (!in || !old || !candidate) { free(in); free(old); free(candidate); return 1; }
    int fail = 1;
    /* Every byte symbol, including table edge runs, must work with the old decoder. */
    for (unsigned s = 0; s < 256; ++s) {
        memset(in, s, 257);
        size_t o = rans_compress_all(in, 257, old, cap);
        size_t c = review_rans_compress_all(in, 257, candidate, cap);
        if (o || !c || check_decode(rans_decompress_all, in, 257, candidate, c)) goto done;
        singleton_cases++;
    }
    const size_t sizes[] = {1,2,3,4,15,16,17,127,128,129,131071,131072,131073,
                           RANS_CHUNK-1,RANS_CHUNK,RANS_CHUNK+1};
    for (size_t j = 0; j < sizeof sizes / sizeof sizes[0]; ++j) {
        size_t n = sizes[j]; memset(in, 255, n);
        size_t c = review_rans_compress_all(in, n, candidate, cap);
        if (!c || check_decode(rans_decompress_all, in, n, candidate, c)) goto done;
        singleton_cases++;
    }
    /* Test mixed/constant chunks at beginning, end, and both. */
    for (unsigned layout = 0; layout < 3; ++layout) {
        size_t n = 2u * RANS_CHUNK + 4;
        for (size_t i = 0; i < n; ++i) {
            unsigned chunk = (unsigned)(i / RANS_CHUNK);
            in[i] = (chunk == layout) ? 7 : (uint8_t)((i * 17 + i / 13) & 255);
        }
        size_t c = review_rans_compress_all(in, n, candidate, cap);
        if (!c || check_decode(rans_decompress_all, in, n, candidate, c)) goto done;
        mixed_cases++;
    }
    /* When all chunks contain >1 symbol, the new encoder is byte-identical. */
    for (size_t j = 3; j < sizeof sizes / sizeof sizes[0]; ++j) {
        size_t n = sizes[j];
        if (n % RANS_CHUNK == 1) continue;
        for (size_t i = 0; i < n; ++i) in[i] = (uint8_t)((i * 17 + i / 13) & 255);
        size_t o = rans_compress_all(in, n, old, cap);
        size_t c = review_rans_compress_all(in, n, candidate, cap);
        if (!o || o != c || memcmp(old, candidate, c) ||
            check_decode(rans_decompress_all, in, n, candidate, c)) goto done;
        mixed_cases++;
    }
    /* Different plane distributions, lengths straddling the HUF chunk boundary. */
    const size_t plen[] = {1,3,17,129,8191,131071,131072,131073};
    for (unsigned planes = 1; planes <= 8; planes *= 2) {
        for (size_t j = 0; j < sizeof plen / sizeof plen[0]; ++j) {
            size_t n = planes * plen[j];
            for (unsigned p = 0; p < planes; ++p)
                for (size_t i = 0; i < plen[j]; ++i)
                    in[(size_t)p * plen[j] + i] = p % 2 ? (uint8_t)(i % 7) : (uint8_t)p;
            size_t c = split_encode(huf_compress_all, in, n, planes, candidate, cap);
            if (!c || check_decode(huf_decompress_all, in, n, candidate, c)) goto done;
            c = split_encode(review_rans_compress_all, in, n, planes, candidate, cap);
            if (!c || check_decode(rans_decompress_all, in, n, candidate, c)) goto done;
            plane_cases += 2;
        }
    }
    printf("{\"status\":\"passed\",\"singleton_cases\":%zu,\"mixed_and_identity_cases\":%zu,"
           "\"plane_compatibility_cases\":%zu,\"decoder\":\"pinned_unmodified\"}\n",
           singleton_cases, mixed_cases, plane_cases);
    fail = 0;
done:
    free(in); free(old); free(candidate);
    if (fail) fprintf(stderr, "compatibility selftest failed\n");
    return fail;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "test")) return selftest();
    if (argc == 6 && !strcmp(argv[1], "decode")) {
        char *e; errno=0; unsigned long code=strtoul(argv[2],&e,10);
        if(errno || *e || (code!=0 && code!=13 && code!=14)) return 2;
        errno=0; unsigned long raw=strtoul(argv[4],&e,10);
        if(errno || *e || !raw || raw>REVIEW_MAX_INPUT) return 2;
        FILE *f=fopen(argv[3],"rb"); if(!f) return 2;
        if(fseek(f,0,SEEK_END) || ftell(f)<1 || ftell(f)>2*REVIEW_MAX_INPUT) {fclose(f);return 2;}
        size_t cn=(size_t)ftell(f); rewind(f);
        uint8_t *in=malloc(cn), *out=malloc(raw); int bad=1;
        if(!in||!out||fread(in,1,cn,f)!=cn) {fclose(f);free(in);free(out);return 1;}
        if(fclose(f)) {free(in);free(out);return 1;}
        if(code==0) {bad=cn!=raw;if(!bad)memcpy(out,in,raw);}
        else bad=(code==13?huf_decompress_all:rans_decompress_all)(in,cn,out,raw);
        if(!bad) {f=fopen(argv[5],"wb");if(!f)bad=1;else {bad=fwrite(out,1,raw,f)!=raw;if(fclose(f))bad=1;}}
        free(in);free(out);return bad?1:0;
    }
    if (argc != 3) { fprintf(stderr,"usage: compat_probe test | RESIDUAL_FILE PLANES\n"); return 2; }
    char *end; errno = 0; unsigned long p = strtoul(argv[2], &end, 10);
    if (errno || *end || (p!=1 && p!=2 && p!=4 && p!=8)) return 2;
    FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
    if (fseek(f,0,SEEK_END) || ftell(f)<1 || ftell(f)>REVIEW_MAX_INPUT) { fclose(f); return 2; }
    size_t n = (size_t)ftell(f); rewind(f);
    size_t cap = n*2 + 65536;
    uint8_t *in=malloc(n), *coded=malloc(cap); int fail=1;
    if (!in || !coded || fread(in,1,n,f)!=n) { fclose(f); goto done; }
    if (fclose(f) || n%p) goto done;
    encoder_fn funcs[5] = {huf_compress_all,huf_compress_all,rans_compress_all,
                          review_rans_compress_all,review_rans_compress_all};
    size_t sz[5] = {0};
    for (unsigned k=0;k<5;++k) {
        sz[k] = (k==1 || k==4) ? split_encode(funcs[k],in,n,(unsigned)p,coded,cap)
                                     : funcs[k](in,n,coded,cap);
        if (sz[k] && check_decode(k<2?huf_decompress_all:rans_decompress_all,in,n,coded,sz[k])) goto done;
    }
    printf("{\"input_bytes\":%zu,\"planes\":%lu,\"huf_original_payload\":%zu,"
           "\"huf_plane_aligned_payload\":%zu,\"rans_original_payload\":%zu,"
           "\"rans_singleton_enabled_payload\":%zu,\"rans_plane_aligned_payload\":%zu,"
           "\"all_emitted_payloads_passed_old_decoder\":true}\n", n,p,sz[0],sz[1],sz[2],sz[3],sz[4]);
    fail=0;
done: free(in);free(coded);return fail;
}
