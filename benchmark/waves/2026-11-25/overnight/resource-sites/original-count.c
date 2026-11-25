/* Replay only the valid-input counter portion of the original resource suite. */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define KOLMO_TESTING
#include "src/kolmo.c"

int main(void) {
    uint8_t raw[8192], decoded[8192];
    memset(raw,'A',sizeof(raw));
    kolmo_params params = {KOLMO_FAST, 0, 1, 4096, 0};
    size_t cap=kolmo_bound(sizeof(raw),&params);
    uint8_t *frame=malloc(cap);
    assert(frame);
    long failed=0;
    int64_t result;
    for (long i=0;i<64;i++) {
        kolmo_alloc_fail_after=i;
        result=kolmo_compress(raw,sizeof(raw),frame,cap,&params);
        kolmo_alloc_fail_after=-1;
        printf("compress %ld %lld\n",i,(long long)result);
        if (result>0) break;
        assert(result==KOLMO_ERR_ALLOC);
        failed++;
    }
    assert(failed==13 && result>0);
    for (long i=0;i<2;i++) {
        kolmo_alloc_fail_after=i;
        int64_t got=kolmo_decompress(frame,(size_t)result,decoded,sizeof(decoded),1);
        kolmo_alloc_fail_after=-1;
        printf("decompress %ld %lld\n",i,(long long)got);
        assert(got==KOLMO_ERR_ALLOC);
    }
    assert(kolmo_decompress(frame,(size_t)result,decoded,sizeof(decoded),1)==sizeof(raw));
    assert(!memcmp(raw,decoded,sizeof(raw)));
    free(frame);
    puts("13 compression failure budgets, 2 decoder failure budgets, exact retry");
}
