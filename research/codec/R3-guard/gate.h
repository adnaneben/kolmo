/* Frozen bounded heuristic. NOT a conservative compressed-size bound. */
#ifndef R3_GATE_H
#define R3_GATE_H
#include <math.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { unsigned aligned, sampled; double advantage_bits, predicted_bytes, margin_bytes; } gate_result;
static double sample_entropy(const uint32_t *h, size_t n) {
    double sum=0;
    for(unsigned i=0;i<256;i++) if(h[i]) sum+=(double)h[i]*log2((double)n/(double)h[i]);
    return n ? sum/(double)n : 0;
}
static gate_result choose_schedule(const uint8_t *p,size_t n,unsigned planes) {
    gate_result r={0};
    if(!p || !planes || planes>8 || (planes&(planes-1)) || n%planes) return r;
    size_t values=n/planes;
    if(planes==1 || values<1024 || n<16384) return r;
    size_t q=values<1024?values:1024;
    uint32_t hist[8][256]={{0}}, pooled[256]={0};
    for(unsigned k=0;k<planes;k++) for(size_t i=0;i<q;i++) {
        size_t at=i*(values-1)/(q-1);
        uint8_t x=p[(size_t)k*values+at];++hist[k][x];++pooled[x];
    }
    double separate=0;
    for(unsigned k=0;k<planes;k++) separate+=sample_entropy(hist[k],q)/(double)planes;
    r.sampled=(unsigned)(planes*q);
    r.advantage_bits=sample_entropy(pooled,q*planes)-separate;
    if(r.advantage_bits<0) r.advantage_bits=0;
    r.predicted_bytes=(double)n*r.advantage_bits/8.0;
    r.margin_bytes=256.0+1024.0*(planes-1);
    if(r.margin_bytes<(double)n/32.0) r.margin_bytes=(double)n/32.0;
    r.aligned=r.predicted_bytes>=r.margin_bytes;
    return r;
}
#endif
