// Native exact-header comparator using only OpenZL's standard codecs/decoder.
#include "header_parser.hpp"
#include "openzl/zl_compress.h"
#include "openzl/zl_compressor.h"
#include "openzl/zl_decompress.h"
#include "openzl/zl_public_nodes.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
using Bytes=std::vector<uint8_t>;
static size_t checked(ZL_Report r){if(ZL_isError(r))throw std::runtime_error(ZL_ErrorCode_toString(ZL_errorCode(r)));return ZL_validResult(r);}
static Bytes readfile(const char* p){std::ifstream f(p,std::ios::binary|std::ios::ate);exact::need(bool(f),"input open failed");auto n=f.tellg();exact::need(n>=0&&uint64_t(n)<=exact::max_input+(size_t(2)<<20),"input limit");f.seekg(0);Bytes b(static_cast<size_t>(n));f.read(reinterpret_cast<char*>(b.data()),n);exact::need(bool(f)||n==0,"input read failed");return b;}
static void writefile(const char* p,const Bytes&b){int fd=open(p,O_WRONLY|O_CREAT|O_EXCL,0600);exact::need(fd>=0,"output exists/open failed");size_t n=0;while(n<b.size()){auto z=write(fd,b.data()+n,b.size()-n);if(z<=0){close(fd);unlink(p);throw std::runtime_error("output write failed");}n+=size_t(z);}if(close(fd)){unlink(p);throw std::runtime_error("output close failed");}}
static Bytes encode(const Bytes&b,int level,const std::string&mode){
    exact::need(b.size()<=exact::max_input,"original input limit");
    // Typing and graph construction are inside each encode call, not precomputed.
    exact::Layout x; if(mode!="serial")x=exact::inspect(b);
    std::unique_ptr<ZL_Compressor,decltype(&ZL_Compressor_free)> cp(ZL_Compressor_create(),ZL_Compressor_free);
    std::unique_ptr<ZL_CCtx,decltype(&ZL_CCtx_free)> cc(ZL_CCtx_create(),ZL_CCtx_free);
    exact::need(bool(cp)&&bool(cc),"context allocation failed");
    checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_formatVersion,ZL_MAX_FORMAT_VERSION));
    checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_compressionLevel,level));
    checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_compressedChecksum,ZL_TernaryParam_enable));
    checked(ZL_Compressor_setParameter(cp.get(),ZL_CParam_contentChecksum,ZL_TernaryParam_enable));
    // Standard ACE defaults, without a segmenter: nested segmenters are not legal.
    // Every measured file is below 16 MiB; this adapter explicitly caps inputs there.
    exact::need(b.size()<=(16u<<20),"comparator limit 16 MiB");
    ZL_GraphID serial=ZL_Compressor_buildACEGraphWithDefault(cp.get(),ZL_GRAPH_LZ),start=serial;
    exact::need(ZL_GraphID_isValid(serial),"serial graph invalid");
    if(mode!="serial" && x.payload){
        ZL_NodeID cv=x.big?ZL_Node_convertSerialToNumBE(x.width*8):ZL_Node_convertSerialToNumLE(x.width*8);
        ZL_GraphID inner=ZL_Compressor_buildACEGraphWithDefault(cp.get(),ZL_GRAPH_NUMERIC);
        ZL_GraphID numeric=ZL_Compressor_registerStaticGraph_fromNode1o(cp.get(),cv,inner);
        exact::need(ZL_GraphID_isValid(numeric),"numeric graph invalid");
        if(mode=="whole"){
            exact::need(b.size()%x.width==0,"whole-file numeric requires multiple of width");start=numeric;
        }else{
            std::vector<size_t> sizes;std::vector<ZL_GraphID> graphs;
            if(x.prefix){sizes.push_back(x.prefix);graphs.push_back(serial);}
            sizes.push_back(x.payload);graphs.push_back(numeric);
            if(x.tail){sizes.push_back(x.tail);graphs.push_back(serial);}
            start=ZL_Compressor_registerSplitGraph(cp.get(),ZL_Type_serial,sizes.data(),graphs.data(),sizes.size());
        }
    }
    exact::need(ZL_GraphID_isValid(start),"starting graph invalid");
    checked(ZL_Compressor_selectStartingGraphID(cp.get(),start));checked(ZL_CCtx_refCompressor(cc.get(),cp.get()));
    Bytes out(ZL_compressBound(b.size())+65536);auto r=ZL_CCtx_compress(cc.get(),out.data(),out.size(),b.data(),b.size());if(ZL_isError(r))throw std::runtime_error(ZL_CCtx_getErrorContextString(cc.get(),r));size_t n=checked(r);out.resize(n);return out;
}
static Bytes decode(const Bytes&b){size_t n=checked(ZL_getDecompressedSize(b.data(),b.size()));exact::need(n<=exact::max_input,"decode output limit");exact::need(checked(ZL_getCompressedSize(b.data(),b.size()))==b.size(),"trailing compressed bytes");Bytes out(n);exact::need(checked(ZL_decompress(out.data(),out.size(),b.data(),b.size()))==n,"decode length mismatch");return out;}
int main(int argc,char**argv){try{
    exact::need(argc>=2,"usage: adapter c LEVEL exact|whole|serial IN OUT | d IN OUT");std::string cmd=argv[1];
    if(cmd=="c"){exact::need(argc==6,"encode args");std::string lv=argv[2],mode=argv[3];exact::need(lv=="6"||lv=="9","level must be frozen 6 or 9");exact::need(mode=="exact"||mode=="whole"||mode=="serial","invalid mode");auto b=readfile(argv[4]);writefile(argv[5],encode(b,std::stoi(lv),mode));}
    else if(cmd=="d"){exact::need(argc==4,"decode args");auto b=readfile(argv[2]);writefile(argv[3],decode(b));}
    else throw std::runtime_error("unknown command");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
