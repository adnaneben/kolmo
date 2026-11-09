// Same exact KRC cost rule, distributed across SIMD lanes instead of private histograms.
#include <metal_stdlib>
using namespace metal;
struct Plan {uint mode,stride,width,exceptions;};
struct Trial {uint cost,width,exceptions,pad;};
inline uint load32(device const uchar *p,ulong i){ulong b=i*4;return uint(p[b])|uint(p[b+1])<<8|uint(p[b+2])<<16|uint(p[b+3])<<24;}
kernel void krc_trials(device const uchar *input [[buffer(0)]],device Trial *trials [[buffer(1)]],
                       constant ulong &word_count [[buffer(2)]],uint3 group [[threadgroup_position_in_grid]],
                       uint lane [[thread_index_in_simdgroup]]) {
    uint candidate=group.x%20;ulong tile=group.x/20,start=tile*256;
    uint words=uint(min(ulong(256),word_count-start));
    uint best_cost=words*4,best_width=0,best_ex=0;
    if(candidate) {
        uint mode=candidate==1?1:2+(candidate-2)%3;
        uint stride=candidate==1?1:1u<<((candidate-2)/3);
        uint lengths[8];
        for(uint j=0;j<8;j++) {
            uint i=lane+j*32,r=0;
            if(i<words) {
                uint v=load32(input,start+i),prev=i>=stride?load32(input,start+i-stride):0;r=v;
                if(mode==4)r=v^prev;
                else if(mode>=2){uint pred=prev;if(mode==3){uint p2=i>=2*stride?load32(input,start+i-2*stride):0;pred=2*prev-p2;}r=v-pred;r=(r<<1)^(0u-(r>>31));}
            }
            lengths[j]=r?32-clz(r):0;
        }
        best_cost=0xffffffffu;
        for(uint width=0;width<=32;width++) {
            uint local_ex=0;for(uint j=0;j<8;j++)local_ex+=lengths[j]>width;
            uint ex=simd_sum(local_ex),cost=(words*width+7)/8+5*ex;
            if(cost<best_cost){best_cost=cost;best_width=width;best_ex=ex;}
        }
    }
    if(lane==0)trials[group.x]={best_cost,best_width,best_ex,0};
}
kernel void krc_reduce(device const Trial *trials [[buffer(0)]],device Plan *plans [[buffer(1)]],
                       constant ulong &tiles [[buffer(2)]],uint tile [[thread_position_in_grid]]) {
    if(ulong(tile)>=tiles)return;
    ulong base=ulong(tile)*20;uint winner=0;
    for(uint c=1;c<20;c++)if(trials[base+c].cost<trials[base+winner].cost)winner=c;
    Plan p={0,0,0,0};
    if(winner){p.mode=winner==1?1:2+(winner-2)%3;p.stride=winner==1?1:1u<<((winner-2)/3);p.width=trials[base+winner].width;p.exceptions=trials[base+winner].exceptions;}
    plans[tile]=p;
}
