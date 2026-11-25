/* Valid-input allocation lifecycle tests for a disposable KOLMO_TESTING build. */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define KOLMO_TESTING
#include "src/kolmo.c"
#undef free

enum { N = 8192, CAP = 2 << 20, FILES = 640 };
static uint8_t src_data[N], data2[N], frame[CAP], arc[CAP], stream_data[CAP], indexed[CAP], out[CAP], reference[CAP];
static uint8_t small_data[FILES][32];
static char small_names[FILES][24];
static kolmo_archive_file members[2], small[FILES];
static kolmo_params base = {KOLMO_FAST, 0, 1, 4096, 0};
static const uint64_t mem_cap = 256u << 20;
static FILE *fin, *fout;
static size_t frame_len, arc_len, stream_len, indexed_len, callback_count, side_bytes, returned_size;

static int capture(void *ctx, const char *name, const uint8_t *data, uint64_t n, uint32_t mode, int64_t time) {
    (void)ctx; (void)mode; (void)time;
    assert(callback_count < 2 && !strcmp(name, members[callback_count].name));
    assert(n == members[callback_count].size && !memcmp(data, members[callback_count].data, n));
    callback_count++;
    return 0;
}
static int select_file(void *ctx, const char *name, uint64_t size, uint32_t mode, int64_t time) {
    (void)ctx; (void)name; (void)size; (void)mode; (void)time; return 1;
}
static void reset_fd(FILE *f, const uint8_t *p, size_t n) {
    assert(!fflush(f) && !ftruncate(fileno(f), 0) && !fseek(f, 0, SEEK_SET));
    if (n) assert(fwrite(p, 1, n, f) == n && !fflush(f));
    assert(!fseek(f, 0, SEEK_SET));
}
static size_t file_size(FILE *f) {
    struct stat st;
    assert(!fstat(fileno(f), &st) && st.st_size >= 0);
    return (size_t)st.st_size;
}
enum path { COMP, DEC, TWO, FROM_FD, TO_FD, CFILE, DFILE, RANGE, ARC, SMALL, EXTRACT, ALL, SENC, SDEC, RAW_MEMBER, SELECTED, INDEXED, TWO_DEC };
static const char *paths[] = {"historical-compress","historical-decompress","two-worker-compress",
    "fd-to-memory","memory-to-fd","fd-to-fd-compress","fd-to-fd-decompress","range-read",
    "archive-create","archive-create-many","archive-extract","archive-extract-all",
    "stream-compress","stream-decompress","framed-stream-decompress","archive-select","indexed-stream-decompress","two-worker-decompress"};
static int has_file(enum path p) {
    return p == TO_FD || p == CFILE || p == DFILE || p == SENC || p == SDEC || p == RAW_MEMBER || p == INDEXED;
}
static int64_t invoke(enum path route, long fail_at) {
    memset(out, 0xaf, sizeof out);
    callback_count = side_bytes = returned_size = 0;
    reset_fd(fout, NULL, 0);
    const uint8_t *in = src_data;
    size_t n = N;
    if (route == DFILE || route == RAW_MEMBER) { in = frame; n = frame_len; }
    if (route == SDEC) { in = stream_data; n = stream_len; }
    if (route == INDEXED) { in = indexed; n = indexed_len; }
    reset_fd(fin, in, n);
    int in_fd = fileno(fin), out_fd = fileno(fout);
    kolmo_params p = base;
    if (route != COMP && route != DEC) p.mem_limit = mem_cap;
    if (route == TWO) p.threads = 2;
    kolmo_decode_limits limits = {N, 4096, 2, mem_cap};
    kolmo_archive_params archive_params = {route == SMALL ? 4096u : 256u, 0};
    int64_t rc = -9999;
    assert(!b60.live);
    memset(&b60, 0, sizeof b60);
    b60.active = 1;
    b60.target = fail_at;
    switch (route) {
    case COMP: case TWO: rc = kolmo_compress(src_data, N, out, CAP, &p); break;
    case TWO_DEC: rc = kolmo_decompress(frame,frame_len,out,CAP,2); break;
    case DEC: rc = kolmo_decompress(frame, frame_len, out, CAP, 1); break;
    case FROM_FD: rc = kolmo_compress_from_fd(in_fd, N, out, CAP, &p, NULL, 0); break;
    case TO_FD: rc = kolmo_decompress_to_fd(frame, frame_len, out_fd, 1, NULL, 0); break;
    case CFILE: rc = kolmo_compress_file(in_fd, out_fd, N, &p, NULL, 0); break;
    case DFILE: rc = kolmo_decompress_file(in_fd, out_fd, frame_len, 1, NULL, 0); break;
    case RANGE: rc = kolmo_read_range(frame, frame_len, N - 16, 16, out, 1); break;
    case ARC: rc = kolmo_archive_create(members, 2, out, CAP, &p, &archive_params, NULL); break;
    case SMALL: rc = kolmo_archive_create(small, FILES, out, CAP, &p, &archive_params, NULL); break;
    case EXTRACT: rc = kolmo_archive_extract(arc, arc_len, 0, out, CAP, 1); break;
    case ALL: rc = kolmo_archive_extract_all(arc, arc_len, capture, NULL, 1); break;
    case SENC: rc = kolmo_stream_compress(in_fd, out_fd, &p, 1); break;
    case SELECTED: rc = kolmo__archive_walk(arc, arc_len, select_file, capture, NULL, 1); break;
    case SDEC: case RAW_MEMBER: case INDEXED: rc = kolmo_stream_decompress_limited(in_fd, out_fd, 1, &limits); break;
    }
    b60.active = 0;
    kolmo_alloc_fail_after = -1;
    if (has_file(route)) {
        returned_size = file_size(fout);
        assert(returned_size <= CAP && (!returned_size ||
               pread(out_fd, out, returned_size, 0) == (ssize_t)returned_size));
        side_bytes = returned_size;
    } else {
        if (rc >= 0 && route != ALL && route != SELECTED) returned_size = (size_t)rc;
        for (size_t i = 0; i < CAP; i++) side_bytes += out[i] != 0xaf;
    }
    return rc;
}
static void fixtures(void) {
    memset(src_data, 'A', N);
    for (size_t i = 0; i < N; i++) data2[i] = (uint8_t)(i * 13 + i / 47);
    members[0] = (kolmo_archive_file){"a", src_data, N, 0644, 100};
    members[1] = (kolmo_archive_file){"b", data2, N, 0644, 101};
    for (size_t i = 0; i < FILES; i++) {
        snprintf(small_names[i],sizeof small_names[i],"member-%zu",i);
        memset(small_data[i], (int)i, sizeof small_data[i]);
        small_data[i][0] = (uint8_t)(i >> 8);
        small[i] = (kolmo_archive_file){small_names[i], small_data[i], sizeof small_data[i], 0644, 100};
    }
    uint8_t table[N];
    for (size_t i=0;i<N;i++) table[i] = (uint8_t)"a,b\n"[i % 4];
    kolmo_index_params idx = {0}; idx.kind = KOLMO_IDX_LINES;
    int64_t inum = kolmo_compress_indexed(table,N,indexed,CAP,&base,&idx);
    assert(inum > 0); indexed_len = (size_t)inum;
    fin = tmpfile(); fout = tmpfile();
    assert(fin && fout);
    int64_t v = invoke(COMP,-1);
    assert(v > 0); frame_len = v; memcpy(frame,out,frame_len);
    v = invoke(ARC,-1); assert(v > 0); arc_len = v; memcpy(arc,out,arc_len);
    v = invoke(SENC,-1); assert(v > 0); stream_len = returned_size; memcpy(stream_data,out,stream_len);
}
static int check_path(enum path route) {
    int64_t expected = invoke(route,-1);
    assert(expected >= 0 && b60.live == 0);
    size_t bytes = returned_size, callbacks = callback_count;
    memcpy(reference,out,bytes);
    uint8_t decoded[N];
    switch (route) {
    case COMP: case TWO: case FROM_FD: case CFILE:
        assert(kolmo_decompress(reference,bytes,decoded,N,1)==N && !memcmp(decoded,src_data,N));
        break;
    case DEC: case TWO_DEC: case TO_FD: case DFILE: case RAW_MEMBER: case EXTRACT: case SDEC:
        assert(bytes==N && !memcmp(reference,src_data,N)); break;
    case RANGE: assert(bytes==16 && !memcmp(reference,src_data+N-16,16)); break;
    case ARC: case SMALL: {
        size_t num=route == ARC ? 2 : FILES;
        kolmo_archive_file *files=route == ARC ? members : small;
        assert(kolmo_archive_count(reference,bytes)==(int64_t)num);
        for (size_t i=0;i<num;i++) {
            int64_t n=kolmo_archive_extract(reference,bytes,i,decoded,N,1);
            assert(n==(int64_t)files[i].size && !memcmp(decoded,files[i].data,(size_t)n));
        }
        break;
    }
    case ALL: case SELECTED: assert(callbacks==2); break;
    case SENC: assert(bytes==stream_len && !memcmp(reference,stream_data,bytes)); break;
    case INDEXED:
        assert(bytes==N);
        for (size_t i=0;i<N;i++) assert(reference[i]==(uint8_t)"a,b\n"[i%4]);
        break;
    }
    const char *trace[2048];
    size_t count = b60.ordinal;
    memcpy(trace,b60.trace,count*sizeof(*trace));
    int failures = 0;
    for (size_t i = 0; i < count; i++) {
        /* Exercise every original ordinal in the historical case; otherwise one failure per
           unique call site plus the second-file callback in bulk extraction. */
        size_t repeats = 0;
        for (size_t j=0;j<i;j++) repeats += !strcmp(trace[i],trace[j]);
        if (route != COMP && route != DEC && repeats && !(route == ALL && repeats == 1)) continue;
        int64_t rc = invoke(route,(long)i);
        size_t touched=side_bytes, emitted=callback_count, live=b60.live, hits=(size_t)b60.hits;
        int exact_fallback=rc == expected && returned_size==bytes && !memcmp(reference,out,bytes) && emitted==callbacks;
        int success = hits==1 && live==0 && (rc==KOLMO_ERR_ALLOC || exact_fallback);
        if (live) {
            fprintf(stderr,"allocation balance %zu at %s %s\n",live,paths[route],trace[i]);
            return 1;
        }
        int64_t retry=invoke(route,-1);
        int exact=retry==expected && returned_size==bytes && !memcmp(reference,out,bytes) && callback_count==callbacks && !b60.live;
        printf("%s,%zu,%s,%" PRId64 ",%zu,%s,%zu,%zu,%zu,%d,%d\n",paths[route],i,trace[i],rc,touched,
               has_file(route)?"fd-length":"changed-sentinel-bytes",emitted,live,hits,exact_fallback,exact);
        if (!success || !exact) failures++;
    }
    return failures;
}
int main(void) {
    fixtures();
    puts("path,ordinal,site,return,visible_output,output_measure,callbacks,unreleased_tracked_allocations,hits,exact_fallback,exact_retry");
    int failures=0;
    for (int r=COMP;r<=TWO_DEC;r++) failures+=check_path((enum path)r);
    fclose(fin);fclose(fout);
    if (TLS) { if (TLS->retained) pthread_setspecific(tls_key,NULL); tls_destroy(TLS); TLS=NULL; }
    fprintf(stderr,"valid-input allocation check: %d unexpected outcomes\n", failures);
    return failures != 0;
}
