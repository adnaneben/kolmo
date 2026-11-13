// Research-only, exact boundary parser. No semantic reserialization.
#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace exact {
constexpr size_t max_input = size_t(256) << 20;
struct Layout {
    std::string kind;
    size_t prefix = 0, payload = 0, tail = 0, width = 0, count = 0;
    bool big = false;
};
inline void need(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
inline uint32_t le(const uint8_t* p, unsigned n) {
    uint32_t v=0; for (unsigned i=0;i<n;++i) v |= uint32_t(p[i])<<(8*i); return v;
}
inline size_t mul(size_t a,size_t b) {
    need(b==0 || a<=max_input/b,"shape exceeds input limit"); return a*b;
}
class Header {
    const std::string s; size_t p=0;
public:
    explicit Header(std::string x):s(std::move(x)){}
    void ws(){while(p<s.size() && (s[p]==' ' || s[p]=='\t' || s[p]=='\r' || s[p]=='\n')) ++p;}
    bool eat(char c){ws(); if(p<s.size()&&s[p]==c){++p;return true;}return false;}
    void req(char c){need(eat(c),"invalid NPY header grammar");}
    std::string text(){ws();need(p<s.size()&&(s[p]=='\''||s[p]=='\"'),"expected quoted token");char q=s[p++];std::string v;
        while(p<s.size()&&s[p]!=q){unsigned char c=s[p++];need(c>=32&&c<127&&c!='\\',"escaped/non-ASCII token unsupported");v+=char(c);}
        need(p<s.size(),"unterminated header token");++p;return v;}
    bool boolean(){ws();if(s.compare(p,4,"True")==0){p+=4;return true;}need(s.compare(p,5,"False")==0,"invalid bool");p+=5;return false;}
    size_t number(){ws();need(p<s.size()&&s[p]>='0'&&s[p]<='9',"invalid dimension");size_t v=0;
        while(p<s.size()&&s[p]>='0'&&s[p]<='9'){unsigned d=unsigned(s[p++]-'0');need(v<=(max_input-d)/10,"dimension limit");v=v*10+d;}return v;}
    size_t shape(){req('(');if(eat(')'))return 1;size_t count=1,dims=0;
        for(;;){need(++dims<=32,"dimension count limit");count=mul(count,number());bool comma=eat(',');
            if(eat(')')){need(dims!=1||comma,"one-dimensional shape needs comma");return count;}need(comma,"missing dimension separator");}}
    void parse(std::string& descr,size_t& count){req('{');unsigned seen=0;bool finished=false;
        for(unsigned fields=0;fields<3;++fields){std::string key=text();req(':');unsigned bit=key=="descr"?1:key=="fortran_order"?2:key=="shape"?4:0;
            need(bit&&!(seen&bit),"unknown/duplicate key");seen|=bit;
            if(bit==1)descr=text();else if(bit==2)(void)boolean();else count=shape();
            if(eat('}')){finished=true;break;}req(',');if(eat('}')){finished=true;break;}}
        need(seen==7&&finished,"incomplete header");ws();need(p==s.size(),"trailing header syntax");}
};
inline Layout npy(const std::vector<uint8_t>& b) {
    const size_t n=b.size();need(n>=10&&std::memcmp(b.data(),"\x93NUMPY",6)==0,"not NPY");
    unsigned ver=b[6];need((ver>=1&&ver<=3)&&b[7]==0,"NPY version unsupported");size_t pre=ver==1?10:12;
    need(n>=pre,"truncated NPY length");size_t hl=le(b.data()+8,ver==1?2:4);
    need(hl>0&&hl<=(size_t(1)<<20)&&hl<=n-pre,"invalid NPY header length");
    size_t start=pre+hl;need(start%16==0,"unaligned NPY header");need(b[start-1]=='\n',"header newline missing");
    std::string descr;size_t count=0;Header(std::string(reinterpret_cast<const char*>(b.data()+pre),hl)).parse(descr,count);
    need(descr.size()==3,"only fixed integer dtype supported");need(descr[1]=='i'||descr[1]=='u',"noninteger dtype unsupported");
    size_t w=unsigned(descr[2]-'0');need(w==1||w==2||w==4||w==8,"invalid integer width");
    need(descr[0]=='<'||descr[0]=='>'||(descr[0]=='|'&&w==1),"ambiguous byte order unsupported");
    size_t bytes=mul(count,w);need(bytes<=n-start,"truncated NPY payload");
    return {"npy",start,bytes,n-start-bytes,w,count,descr[0]=='>'};
}
inline Layout wav(const std::vector<uint8_t>& b) {
    size_t n=b.size();need(n>=12&&std::memcmp(b.data(),"RIFF",4)==0&&std::memcmp(b.data()+8,"WAVE",4)==0,"not RIFF WAVE");
    size_t total=size_t(le(b.data()+4,4))+8;need(total>=12&&total<=n,"invalid RIFF length");
    bool fmt=false,data=false;unsigned channels=0,align=0;Layout out;out.kind="wav";
    for(size_t p=12;p<total;){need(total-p>=8,"truncated RIFF chunk");size_t len=le(b.data()+p+4,4),body=p+8;
        need(len<=total-body,"truncated chunk data");size_t padded=len+(len&1);need(padded<=total-body,"missing RIFF padding");
        if(std::memcmp(b.data()+p,"fmt ",4)==0){need(!fmt&&len>=16,"bad/duplicate format");fmt=true;
            need(le(b.data()+body,2)==1&&le(b.data()+body+14,2)==16,"only PCM16 supported");
            channels=le(b.data()+body+2,2);align=le(b.data()+body+12,2);uint32_t rate=le(b.data()+body+4,4),byte_rate=le(b.data()+body+8,4);
            need(channels>0&&channels<=64&&align==channels*2&&rate>0&&uint64_t(rate)*align==byte_rate,"inconsistent PCM format");
        }else if(std::memcmp(b.data()+p,"data",4)==0){need(fmt&&!data,"data before fmt or multiple data chunks");data=true;need(len%align==0,"incomplete PCM frame");
            out.prefix=body;out.payload=len;out.tail=n-body-len;out.width=2;out.count=len/2;}
        p=body+padded;
    }
    need(fmt&&data,"missing PCM chunks");return out;
}
inline Layout inspect(const std::vector<uint8_t>& b) {
    need(b.size()<=max_input,"file exceeds 256 MiB limit");
    if(b.size()>=6&&std::memcmp(b.data(),"\x93NUMPY",6)==0)return npy(b);
    return wav(b);
}
} // namespace exact
