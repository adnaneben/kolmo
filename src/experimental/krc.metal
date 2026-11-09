// Independent implementation of the KRC model-cost rule, for Metal compute.
#include <metal_stdlib>
using namespace metal;
struct Plan { uint mode, stride, width, exceptions; };
inline uint load32(device const uchar *p, ulong i) {
    ulong b=i*4; return uint(p[b]) | uint(p[b+1])<<8 | uint(p[b+2])<<16 | uint(p[b+3])<<24;
}
kernel void krc_choose(device const uchar *input [[buffer(0)]],
                       device Plan *plans [[buffer(1)]],
                       constant ulong &word_count [[buffer(2)]],
                       uint3 group [[threadgroup_position_in_grid]],
                       uint lane [[thread_index_in_threadgroup]]) {
    threadgroup uint costs[32], widths[32], exceptions[32];
    ulong start=ulong(group.x)*256;uint words=uint(min(ulong(256),word_count-start));
    uint mode=0,stride=0,best_width=0,best_ex=0,best_cost=words*4;
    if(lane>0 && lane<20) {
        mode=lane==1?1:2+(lane-2)%3;stride=lane==1?1:1u<<((lane-2)/3);
        uint hist[33];for(uint b=0;b<=32;b++)hist[b]=0;
        for(uint i=0;i<words;i++) {
            uint v=load32(input,start+i),prev=i>=stride?load32(input,start+i-stride):0,r=v;
            if(mode==4)r=v^prev;
            else if(mode>=2){uint pred=prev;if(mode==3){uint p2=i>=2*stride?load32(input,start+i-2*stride):0;pred=2*prev-p2;}r=v-pred;r=(r<<1)^(0u-(r>>31));}
            hist[r?32-clz(r):0]++;
        }
        // Use an unreachable initial cost so this candidate keeps its own
        // optimal width; lane zero's raw representation wins global ties.
        best_cost=0xffffffffu;uint ex=words;
        for(uint b=0;b<=32;b++){ex-=hist[b];uint cost=(words*b+7)/8+5*ex;
            if(cost<best_cost){best_cost=cost;best_width=b;best_ex=ex;}}
    }
    costs[lane]=lane<20?best_cost:0xffffffffu;widths[lane]=best_width;exceptions[lane]=best_ex;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if(lane==0) {
        uint winner=0;for(uint candidate=1;candidate<20;candidate++)if(costs[candidate]<costs[winner])winner=candidate;
        Plan p={0,0,0,0};
        if(winner){p.mode=winner==1?1:2+(winner-2)%3;p.stride=winner==1?1:1u<<((winner-2)/3);p.width=widths[winner];p.exceptions=exceptions[winner];}
        plans[group.x]=p;
    }
}
