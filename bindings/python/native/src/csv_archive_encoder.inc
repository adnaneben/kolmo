/* Explicit RC CSV route from P2-E25. This frozen regularity heuristic is NOT
 * the general column-first joint graph planner; retain plain eco as a control.
 * No automatic selection or claim of non-regression. Existing wire format. */
static double kcsv_regularity(const uint8_t *s,size_t n) {
    if(n>65536)n=65536;
    uint32_t cnt[4096]={0},first[4096];size_t lines=0;uint32_t c=1;
    for(size_t i=0;i<n;i++){
        if(s[i]==',')c++;
        else if(s[i]=='\n'){
            unsigned k=c<4096?c:4095;
            if(!cnt[k])first[k]=(uint32_t)lines;cnt[k]++;lines++;c=1;
        }
    }
    if(!lines)return 1.0;
    unsigned best=0;
    for(unsigned k=1;k<4096;k++)
        if(cnt[k]>cnt[best] || (cnt[k]&&cnt[k]==cnt[best]&&first[k]<first[best]))best=k;
    return (double)cnt[best]/(double)lines;
}
static int64_t kcsv_encode(const uint8_t *s,size_t n,uint8_t *dst,size_t cap) {
    if(n<64 || kcsv_regularity(s,n)<=0.9916)return 0;
    if(cap<HDR+IDX+4)return KOLMO_ERR_DSTSIZE;
    codec_ctx x={0};size_t pn=kg_encode_table(&x,C_ZSTD,12,s,n,',','.',dst+HDR+IDX+4,cap-HDR-IDX-4);
    codec_ctx_free(&x);if(!pn)return 0;
    size_t size=ktf_wrap(s,n,0,0,dst+HDR+IDX+4,pn,dst);
    dst[5]=KOLMO_ECO;dst[HDR+IDX+1]=',';dst[HDR+IDX+2]='.';
    uint32_t crc=kolmo_crc32c(0,dst,28);crc=kolmo_crc32c(crc,dst+HDR,IDX);put32(dst+28,crc);
    return (int64_t)size;
}
