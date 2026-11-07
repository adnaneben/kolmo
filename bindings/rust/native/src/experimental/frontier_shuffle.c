#include "frontier_shuffle.h"
#include <string.h>
#if defined(__aarch64__) && defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#include <arm_neon.h>

static uint16x8_t differences(uint16x8_t x, uint16_t *last) {
    uint16x8_t prev=vextq_u16(vdupq_n_u16(*last),x,7);
    *last=vgetq_lane_u16(x,7);
    return vsubq_u16(x,prev);
}
static uint16x8_t integrate(uint16x8_t x,uint16_t *last) {
    uint16x8_t zero=vdupq_n_u16(0);
    x=vaddq_u16(x,vextq_u16(zero,x,7));
    x=vaddq_u16(x,vextq_u16(zero,x,6));
    x=vaddq_u16(x,vextq_u16(zero,x,4));
    x=vaddq_u16(x,vdupq_n_u16(*last));
    *last=vgetq_lane_u16(x,7);
    return x;
}

int frontier_shuffle16(const uint8_t *src,size_t rows,unsigned channels,int delta,uint8_t *dst,int inverse) {
    if ((channels!=1 && channels!=2) || rows<8 ||
        ((uintptr_t)(inverse?dst:src)&1u)) return 0;
    size_t i=0,plane=channels*rows;
    uint16_t last[2]={0};
    for (; rows-i>=8; i+=8) {
        uint16x8x2_t group;
        if (!inverse) {
            const uint16_t *at=(const uint16_t *)(src+2*i*channels);
            if (channels==2) group=vld2q_u16(at);
            else group.val[0]=vld1q_u16(at);
            for (unsigned c=0;c<channels;++c) {
                uint16x8_t v=group.val[c];
                if (delta) v=differences(v,last+c);
                vst1_u8(dst+c*rows+i,vmovn_u16(v));
                vst1_u8(dst+plane+c*rows+i,vshrn_n_u16(v,8));
            }
        } else {
            for (unsigned c=0;c<channels;++c) {
                uint16x8_t lo=vmovl_u8(vld1_u8(src+c*rows+i));
                uint16x8_t hi=vmovl_u8(vld1_u8(src+plane+c*rows+i));
                uint16x8_t v=vsliq_n_u16(lo,hi,8);
                group.val[c]=delta?integrate(v,last+c):v;
            }
            uint16_t *at=(uint16_t *)(dst+2*i*channels);
            if (channels==2) vst2q_u16(at,group);
            else vst1q_u16(at,group.val[0]);
        }
    }
    for (;i<rows;++i) for (unsigned c=0;c<channels;++c) {
        size_t lo=c*rows+i, hi=plane+lo, off=2*(i*channels+c);
        uint16_t v;
        if (inverse) {
            v=(uint16_t)(src[lo]|(unsigned)src[hi]<<8);
            if (delta) v=(uint16_t)(v+last[c]);
            last[c]=v;
            memcpy(dst+off,&v,2);
        } else {
            memcpy(&v,src+off,2);
            uint16_t d=delta?(uint16_t)(v-last[c]):v;
            last[c]=v;dst[lo]=(uint8_t)d;dst[hi]=(uint8_t)(d>>8);
        }
    }
    return 1;
}
#else
int frontier_shuffle16(const uint8_t *src,size_t rows,unsigned channels,int delta,uint8_t *dst,int inverse) {
    (void)src;(void)rows;(void)channels;(void)delta;(void)dst;(void)inverse;
    return 0;
}
#endif
