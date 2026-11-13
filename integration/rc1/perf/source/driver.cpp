#include "openzl_prefix.inc"
#include "custom_parsers/csv/csv_profile.h"
#include "czn.h"
#include "kolmo.h"
#include "zstd.h"
#include "lz4.h"
#include "lz4hc.h"
#include "brotli/encode.h"
#include "brotli/decode.h"
#include <lzma.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include <time.h>
struct B {Bytes v;std::unique_ptr<unsigned char,decltype(&free)> p{nullptr,&free};size_t k=0;
 const unsigned char*data()const{return p?p.get():v.data();}size_t size()const{return p?k:v.size();}};
static uint64_t ns(clockid_t c){timespec t{};if(clock_gettime(c,&t))throw std::runtime_error("clock");return uint64_t(t.tv_sec)*1000000000+t.tv_nsec;}
static Bytes csv_enc(const Bytes&x,int level){
 std::unique_ptr<ZL_Compressor,decltype(&ZL_Compressor_free)>cp(ZL_Compressor_create(),ZL_Compressor_free);
 std::unique_ptr<ZL_CCtx,decltype(&ZL_CCtx_free)>cc(ZL_CCtx_create(),ZL_CCtx_free);
 exact::need(bool(cp)&&bool(cc),"OpenZL context");
 checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_formatVersion,ZL_MAX_FORMAT_VERSION));
 checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_compressionLevel,level));
 checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_compressedChecksum,ZL_TernaryParam_enable));
 checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_contentChecksum,ZL_TernaryParam_enable));
 auto g=openzl::custom_parsers::ZL_createGraph_genericCSVCompressor(cp.get());exact::need(ZL_GraphID_isValid(g),"CSV graph");
 checked(ZL_Compressor_selectStartingGraphID(cp.get(),g));checked(ZL_CCtx_refCompressor(cc.get(),cp.get()));
 Bytes out(ZL_compressBound(x.size())+65536);auto q=ZL_CCtx_compress(cc.get(),out.data(),out.size(),x.data(),x.size());
 if(ZL_isError(q))throw std::runtime_error(ZL_CCtx_getErrorContextString(cc.get(),q));out.resize(checked(q));return out;
}
static B compress(const Bytes&x,const std::string&f,int param){B b;size_t n=x.size(),cap=n+n/2+65536;
 if(f=="release"){czn_params p={(czn_mode)param,0,1,0,1};cap=czn_bound(n,&p);b.v.resize(cap);auto z=czn_compress(x.data(),n,b.v.data(),cap,&p);exact::need(z>0,"release compress");b.v.resize(z);}
 else if(f=="rc"){
    kolmo_params p={(kolmo_mode)(param<6?param:param==8?2:param==9||param==12?5:param==11?4:1),0,1,0,1};
    if(param==6)p.transforms|=KOLMO_T_NATIVE_LEAF;
    if(param==7)p.transforms|=KOLMO_T_TYPED_HUF;
    if(param==8)p.transforms|=KOLMO_T_CSV_ARCHIVE;
    if(param==9)p.level=2;
    if(param==10)p.transforms=2;
    if(param==11||param==12)p.level=1;
    cap=kolmo_bound(n,&p);b.v.resize(cap);
    auto k=kolmo_compress(x.data(),n,b.v.data(),cap,&p);exact::need(k>0,"RC encode");b.v.resize(k);
 }
 else if(f=="ozexact"||f=="ozwhole"||f=="ozserial")b.v=encode(x,param,f=="ozexact"?"exact":f=="ozwhole"?"whole":"serial");
 else if(f=="ozcsv")b.v=csv_enc(x,param);
 else if(f=="zstd"){b.v.resize(ZSTD_compressBound(n));size_t z=ZSTD_compress(b.v.data(),b.v.size(),x.data(),n,param);exact::need(!ZSTD_isError(z),"zstd compress");b.v.resize(z);}
 else if(f=="lz4"){b.v.resize(LZ4_compressBound((int)n));int z=param<0?LZ4_compress_HC((char*)x.data(),(char*)b.v.data(),n,b.v.size(),-param):LZ4_compress_fast((char*)x.data(),(char*)b.v.data(),n,b.v.size(),param);exact::need(z>0,"lz4 compress");b.v.resize(z);}
 else if(f=="brotli"){b.v.resize(BrotliEncoderMaxCompressedSize(n));size_t z=b.v.size();exact::need(BrotliEncoderCompress(param,22,BROTLI_MODE_GENERIC,n,x.data(),&z,b.v.data()),"brotli compress");b.v.resize(z);}
 else if(f=="xz"){b.v.resize(lzma_stream_buffer_bound(n));size_t z=0;uint32_t preset=param<0?uint32_t(-param)|LZMA_PRESET_EXTREME:uint32_t(param);exact::need(lzma_easy_buffer_encode(preset,LZMA_CHECK_CRC64,nullptr,x.data(),n,b.v.data(),&z,b.v.size())==LZMA_OK,"xz compress");b.v.resize(z);}

 else throw std::runtime_error("unknown family "+f);return b;
}
static B expand(const Bytes&x,size_t n,const std::string&f,int param){B b;
 if(f.rfind("oz",0)==0){b.v=decode(x);exact::need(b.v.size()==n,"oz length");return b;}
 b.v.resize(n);
 if(f=="rc")exact::need(kolmo_decompress(x.data(),x.size(),b.v.data(),n,1)==(int64_t)n,"RC decode");
 else if(f=="release")exact::need(czn_decompress(x.data(),x.size(),b.v.data(),n,1)==(int64_t)n,"release decode");
 else if(f=="zstd")exact::need(ZSTD_decompress(b.v.data(),n,x.data(),x.size())==n,"zstd decode");
 else if(f=="lz4")exact::need(LZ4_decompress_safe((char*)x.data(),(char*)b.v.data(),x.size(),n)==(int)n,"lz4 decode");
 else if(f=="brotli"){size_t z=n;exact::need(BrotliDecoderDecompress(x.size(),x.data(),&z,b.v.data())==BROTLI_DECODER_RESULT_SUCCESS&&z==n,"brotli decode");}
 else if(f=="xz"){uint64_t lim=uint64_t(2)<<30;size_t ip=0,op=0;exact::need(lzma_stream_buffer_decode(&lim,0,nullptr,x.data(),&ip,x.size(),b.v.data(),&op,n)==LZMA_OK&&op==n&&ip==x.size(),"xz decode");}
 else throw std::runtime_error("unknown decoder");return b;
}
static void verify(const B&b,const Bytes&x){exact::need(b.size()==x.size()&&(!x.size()||!memcmp(b.data(),x.data(),x.size())),"non-exact/determinism failure");}
int main(int argc,char**argv){try{
 // args: make|run FAMILY PARAM OP ORIGINAL FRAME target_seconds WATCH
 exact::need(argc==9,"args");std::string cmd=argv[1],family=argv[2],op=argv[4];int param=std::stoi(argv[3]);Bytes orig=readfile(argv[5]);
 if(cmd=="make"){auto b=compress(orig,family,param);Bytes frame(b.data(),b.data()+b.size());auto d=expand(frame,orig.size(),family,param);verify(d,orig);writefile(argv[6],frame);printf("{\"stored\":%zu,\"input\":%zu}\n",frame.size(),orig.size());return 0;}
 Bytes gold=readfile(argv[6]);double target=std::stod(argv[7]);bool watch=std::stoi(argv[8]);
 if(watch){FILE*f=fopen("/proc/self/clear_refs","w");if(f){fputs("5",f);fclose(f);}puts("READY");fflush(stdout);exact::need(getchar()!=EOF,"start signal");}
 uint64_t wall=0,cpu=0,begin=ns(CLOCK_MONOTONIC),end=begin;unsigned loops=0;size_t outsize=0;
 do {uint64_t c0=ns(CLOCK_PROCESS_CPUTIME_ID),t0=ns(CLOCK_MONOTONIC);B out=op=="encode"?compress(orig,family,param):expand(gold,orig.size(),family,param);uint64_t t1=ns(CLOCK_MONOTONIC),c1=ns(CLOCK_PROCESS_CPUTIME_ID);outsize=out.size();verify(out,op=="encode"?gold:orig);uint64_t c2=ns(CLOCK_PROCESS_CPUTIME_ID),t2=ns(CLOCK_MONOTONIC);out=B{};uint64_t t3=ns(CLOCK_MONOTONIC),c3=ns(CLOCK_PROCESS_CPUTIME_ID);wall+=t1-t0+t3-t2;cpu+=c1-c0+c3-c2;loops++;end=ns(CLOCK_MONOTONIC);}while(double(wall)*1e-9<target&&loops<20000);
 struct rusage ru{};getrusage(RUSAGE_SELF,&ru); long phase_hwm=0; {std::ifstream st("/proc/self/status"); std::string line; while(std::getline(st,line))if(line.rfind("VmHWM:",0)==0)phase_hwm=std::stol(line.substr(6));}
 printf("{\"loops\":%u,\"wall_ns\":%llu,\"cpu_ns\":%llu,\"begin_ns\":%llu,\"end_ns\":%llu,\"input\":%zu,\"stored\":%zu,\"out\":%zu,\"rusage_hwm_kib\":%ld,\"phase_hwm_kib\":%ld}\n",loops,(unsigned long long)wall,(unsigned long long)cpu,(unsigned long long)begin,(unsigned long long)end,orig.size(),gold.size(),outsize,ru.ru_maxrss,phase_hwm);fflush(stdout);if(watch)getchar();return 0;
}catch(const std::exception&e){fprintf(stderr,"perf: %s\n",e.what());return 2;}}
