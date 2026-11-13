// Research benchmark: frozen codec algorithms; complete inputs, no file I/O in timers.
#include "openzl_prefix.inc"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <time.h>
extern "C" {
struct KBuf { unsigned char* data; size_t size; };
KBuf n2_kenc(const unsigned char*, size_t, int);
KBuf n2_kdec(const unsigned char*, size_t, size_t, int);
}
struct Buffer {
    Bytes oz;
    std::unique_ptr<unsigned char, decltype(&free)> k{nullptr, &free};
    size_t kn=0;
    const unsigned char* data() const { return k ? k.get() : oz.data(); }
    size_t size() const { return k ? kn : oz.size(); }
};
struct Arm { const char* name; int kolmo; int level; const char* mode; };
static const Arm arms[] = {
 {"zstd3",0,0,""}, {"kolmo_fast",1,0,""}, {"tp_prod3",2,0,""},
 {"tp_phuf_pg16",4,0,""}, {"tp_pent",10,0,""}, {"tp_arch19",11,0,""},
 {"oz_exact6",-1,6,"exact"}, {"oz_exact9",-1,9,"exact"},
 {"oz_whole9",-1,9,"whole"}, {"oz_serial9",-1,9,"serial"},
 {"zstd3_duplicate",0,0,""}, {"oz_exact9_duplicate",-1,9,"exact"}
};
static uint64_t stamp(clockid_t id) {
    timespec t{}; if(clock_gettime(id,&t)) throw std::runtime_error("clock_gettime");
    return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);
}
static Buffer do_encode(const Bytes& x, const Arm& a) {
    Buffer b;
    if(a.kolmo>=0){KBuf k=n2_kenc(x.data(),x.size(),a.kolmo);if(!k.data||!k.size)throw std::runtime_error("Kolmo encode");b.k.reset(k.data);b.kn=k.size;}
    else b.oz=encode(x,a.level,a.mode);
    return b;
}
static Buffer do_decode(const Buffer& f,size_t n,const Arm& a) {
    Buffer b;
    if(a.kolmo>=0){KBuf k=n2_kdec(f.data(),f.size(),n,a.kolmo);if(!k.data||k.size!=n)throw std::runtime_error("Kolmo decode");b.k.reset(k.data);b.kn=k.size;}
    else {
      size_t z=checked(ZL_getDecompressedSize(f.data(),f.size()));
      exact::need(z==n,"OpenZL output size");
      exact::need(checked(ZL_getCompressedSize(f.data(),f.size()))==f.size(),"OpenZL trailing bytes");
      b.oz.resize(z);exact::need(checked(ZL_decompress(b.oz.data(),b.oz.size(),f.data(),f.size()))==n,"OpenZL decode");
    }
    return b;
}
static void same(const unsigned char* a,size_t an,const unsigned char* b,size_t bn) {
    if(an!=bn||(an&&memcmp(a,b,an)))throw std::runtime_error("byte mismatch");
}
int main(int argc,char**argv) {try {
    if(argc!=5)throw std::runtime_error("usage: native INPUT NEW_FRAME_DIR ROUNDS SEED");
    Bytes input=readfile(argv[1]);
    exact::need(!input.empty()&&input.size()<=(16U<<20),"input size");
    int rounds=std::stoi(argv[3]);exact::need(rounds>=1&&rounds<=9,"round count");
    uint64_t seed=std::stoull(argv[4]);std::mt19937_64 rng(seed);
    const size_t count=sizeof(arms)/sizeof(arms[0]);
    const size_t batch=std::max<size_t>(1,std::min<size_t>(16,(4U<<20)/input.size()));
    std::vector<Buffer> gold;gold.reserve(count);
    for(size_t i=0;i<count;i++) {
      gold.push_back(do_encode(input,arms[i]));
      auto again=do_encode(input,arms[i]);same(gold[i].data(),gold[i].size(),again.data(),again.size());
      auto restored=do_decode(gold[i],input.size(),arms[i]);same(restored.data(),restored.size(),input.data(),input.size());
      std::string path=std::string(argv[2])+"/"+arms[i].name+".frame";
      Bytes copy(gold[i].data(),gold[i].data()+gold[i].size());writefile(path.c_str(),copy);
    }
    std::vector<size_t> order(count);std::iota(order.begin(),order.end(),0);
    // Encode and decode are separate rounds. Every timed result is checked outside
    // the timer; deallocation is separately timed and INCLUDED in the reported cost.
    for(int operation=0;operation<2;operation++) for(int round=0;round<rounds;round++) {
      std::shuffle(order.begin(),order.end(),rng);
      for(size_t i:order) {
        std::vector<Buffer> pending;pending.reserve(batch);
        uint64_t c0=stamp(CLOCK_PROCESS_CPUTIME_ID),w0=stamp(CLOCK_MONOTONIC);
        for(size_t j=0;j<batch;j++)pending.push_back(operation?do_decode(gold[i],input.size(),arms[i]):do_encode(input,arms[i]));
        uint64_t w1=stamp(CLOCK_MONOTONIC),c1=stamp(CLOCK_PROCESS_CPUTIME_ID);
        for(auto&b:pending) {
          if(operation)same(b.data(),b.size(),input.data(),input.size());
          else same(b.data(),b.size(),gold[i].data(),gold[i].size());
        }
        uint64_t c2=stamp(CLOCK_PROCESS_CPUTIME_ID),w2=stamp(CLOCK_MONOTONIC);
        pending.clear();
        uint64_t w3=stamp(CLOCK_MONOTONIC),c3=stamp(CLOCK_PROCESS_CPUTIME_ID);
        printf("{\"arm\":\"%s\",\"operation\":\"%s\",\"round\":%d,\"batch\":%zu,\"input_bytes\":%zu,\"stored_bytes\":%zu,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n",arms[i].name,operation?"decode":"encode",round,batch,input.size(),gold[i].size(),(unsigned long long)((w1-w0)+(w3-w2)),(unsigned long long)((c1-c0)+(c3-c2)));
        fflush(stdout);
      }
    }
    return 0;
 }catch(const std::exception&e){fprintf(stderr,"N2 failure: %s\n",e.what());return 2;}}
