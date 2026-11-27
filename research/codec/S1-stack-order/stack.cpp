// S1 research-only explicit nested framing; never emitted as a production Kolmo frame.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <time.h>
extern "C" {
struct SBuf{unsigned char*p;size_t n;};struct SProg{SBuf par,res,rest;int ow,pln;};
int s_type(const unsigned char*,size_t,int*,size_t*);
int s_forward(const unsigned char*,size_t,int,size_t,SProg*);
SBuf s_inverse(const unsigned char*,size_t,const unsigned char*,size_t,const unsigned char*,size_t,size_t);
uint32_t s_crc(const unsigned char*,size_t);
SBuf s_compress(int,const unsigned char*,size_t);
SBuf s_decompress(int,const unsigned char*,size_t,size_t);
}
using V=std::vector<unsigned char>;constexpr size_t LIMIT=16U<<20, STAGE_LIMIT=64U<<20;
static void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
static V take(SBuf b){std::unique_ptr<unsigned char,decltype(&free)> p(b.p,&free);need(b.p!=nullptr,"codec failed");return V(b.p,b.p+b.n);}
static V read(const char*p){std::ifstream f(p,std::ios::binary);need(bool(f),"read open");V x((std::istreambuf_iterator<char>(f)),{});need(x.size()<=STAGE_LIMIT,"read limit");return x;}
static void write(const std::string&p,const V&v){std::ofstream f(p,std::ios::binary);need(bool(f),"write open");f.write((const char*)v.data(),v.size());need(bool(f),"write");}
static void put(V&o,uint64_t x,unsigned bytes){for(unsigned k=0;k<bytes;k++)o.push_back((x>>(8*k))&255);}
struct Cur{const V&v;size_t i=0;uint64_t get(unsigned n){need(n<=8&&i<=v.size()&&n<=v.size()-i,"truncated integer");uint64_t x=0;for(unsigned k=0;k<n;k++)x|=uint64_t(v[i++])<<(8*k);return x;}V bytes(size_t n){need(i<=v.size()&&n<=v.size()-i,"truncated data");V x(v.begin()+i,v.begin()+i+n);i+=n;return x;}};
struct Stage{char codec;size_t in,out;std::array<size_t,3> hbytes{};};struct Stats{std::vector<Stage> s;size_t residual=0,rest=0,par=0;int groups=0,ow=0;};
struct Arm{const char*name;int place;const char*seq;};
static const Arm ARMS[]={
 {"zstd3",-1,"Z"},{"raw_Z",0,"Z"},{"raw_H",0,"H"},{"raw_R",0,"R"},{"raw_L",0,"L"},
 {"inter_Z",1,"Z"},{"inter_H",1,"H"},{"inter_R",1,"R"},
 {"plane_Z",2,"Z"},{"plane_H",2,"H"},{"plane_R",2,"R"},{"plane_L",2,"L"},
 {"split_Z",3,"Z"},{"split_H",3,"H"},{"split_R",3,"R"},{"split_L",3,"L"},
 {"plane_HZ",2,"HZ"},{"plane_ZH",2,"ZH"},{"split_HZ",3,"HZ"},{"split_ZH",3,"ZH"},
 {"plane_RZ",2,"RZ"},{"plane_ZR",2,"ZR"},{"split_RZ",3,"RZ"},{"split_ZR",3,"ZR"},
 {"plane_LH",2,"LH"},{"plane_HL",2,"HL"},{"split_LH",3,"LH"},{"split_HL",3,"HL"},
 {"plane_LZ",2,"LZ"},{"plane_ZL",2,"ZL"},{"split_LZ",3,"LZ"},
 {"outer_split_H_Z",4,"H"},{"outer_split_R_Z",4,"R"},
 {"split_H_duplicate",3,"H"},{"zstd3_duplicate",-1,"Z"}};
constexpr size_t NA=sizeof(ARMS)/sizeof(ARMS[0]);
static V pack(const V&x,const char*seq,Stats*st){
 V cur=x;for(size_t k=0;k<strlen(seq);k++){
  char c=cur.empty()?0:seq[k];V coded=c?take(s_compress(c,cur.data(),cur.size())):cur;
  if(st){
   Stage record{c,cur.size(),coded.size(),{0,0,0}};
   if(c=='H') { Cur hr{coded}; auto uv=[&](){uint64_t v=0;unsigned shift=0;for(;;){need(shift<64,"HUF varint");unsigned z=hr.get(1);v|=uint64_t(z&127)<<shift;if(!(z&128))return v;shift+=7;}};
     while(hr.i<coded.size()){unsigned m=hr.get(1);size_t len=uv();need(m<3,"HUF mode");record.hbytes[m]+=len;size_t skip=m==0?len:m==1?1:uv();need(skip<=coded.size()-hr.i,"HUF span");hr.i+=skip;}
   } st->s.push_back(record);
  }V next;next.reserve(coded.size()+17);
  put(next,(unsigned char)c,1);put(next,cur.size(),8);put(next,coded.size(),8);next.insert(next.end(),coded.begin(),coded.end());cur=std::move(next);
 }return cur;
}
static V unpack(V cur,unsigned depth,size_t expected){
 need(depth>=1&&depth<=2,"depth");for(unsigned k=0;k<depth;k++){
  Cur r{cur};int c=int(r.get(1));size_t n=r.get(8),z=r.get(8);need(n<=STAGE_LIMIT&&z==cur.size()-r.i,"stage lengths");
  need(c==0||c=='H'||c=='R'||c=='Z'||c=='L',"codec id");V b=r.bytes(z);
  if(!c){need(n==0&&z==0,"invalid empty storage");cur.clear();}else cur=take(s_decompress(c,b.data(),b.size(),n));
 }need(cur.size()==expected,"decoded group size");return cur;
}
static V layout(const V&x,size_t w,bool fromplane,bool toplane){
 if(w==1||fromplane==toplane)return x;need(w>0&&x.size()%w==0,"layout length");size_t m=x.size()/w;V out(x.size());
 for(size_t i=0;i<m;i++)for(size_t b=0;b<w;b++)out[toplane?b*m+i:i*w+b]=x[fromplane?b*m+i:i*w+b];return out;
}
static V encode(const V&x,const Arm&a,Stats*st=nullptr){
 need(x.size()<=LIMIT,"input limit");if(a.place<0)return take(s_compress('Z',x.data(),x.size()));
 int w=0;size_t h=0;int typ=s_type(x.data(),x.size(),&w,&h);int mode=a.place;
 V par,rest,res;unsigned ow=0,pln=0;std::vector<V> groups;
 if(mode==4){Arm inner{"inner",3,a.seq};V base=encode(x,inner,st);V coded=pack(base,"Z",st);V o={'K','S','T','1'};put(o,4,1);put(o,x.size(),8);put(o,s_crc(x.data(),x.size()),4);put(o,base.size(),8);put(o,coded.size(),8);o.insert(o.end(),coded.begin(),coded.end());return o;}
 if(mode&&typ==0){SProg p={};if(s_forward(x.data(),x.size(),w,h,&p)==0){
  std::unique_ptr<unsigned char,decltype(&free)> p1(p.par.p,&free),p2(p.res.p,&free),p3(p.rest.p,&free);
  if(p.par.n)par.assign(p.par.p,p.par.p+p.par.n);if(p.res.n)res.assign(p.res.p,p.res.p+p.res.n);if(p.rest.n)rest.assign(p.rest.p,p.rest.p+p.rest.n);
  ow=p.ow;pln=p.pln;res=layout(res,ow,pln,mode!=1);
 }else mode=0;}else mode=0;
 if(!mode){res=x;ow=pln=0;}
 unsigned ng=mode==3?ow:1;need(ng>=1&&ng<=8&&res.size()%ng==0,"group count");
 if(st){st->residual=res.size();st->rest=rest.size();st->par=par.size();st->groups=ng;st->ow=ow;}
 V o={'K','S','T','1'};put(o,mode,1);put(o,x.size(),8);put(o,s_crc(x.data(),x.size()),4);
 put(o,par.size(),8);put(o,rest.size(),8);put(o,res.size(),8);put(o,ow,1);put(o,pln,1);put(o,ng,4);
 o.insert(o.end(),par.begin(),par.end());o.insert(o.end(),rest.begin(),rest.end());
 size_t len=res.size()/ng;for(unsigned g=0;g<ng;g++){V data(res.begin()+g*len,res.begin()+(g+1)*len);V coded=pack(data,a.seq,st);put(o,strlen(a.seq),1);put(o,coded.size(),8);o.insert(o.end(),coded.begin(),coded.end());}
 return o;
}
static V decode(const V&f,size_t expected,unsigned recursion=0){
 need(expected<=LIMIT&&recursion<=1&&f.size()>=17&&f.size()<=STAGE_LIMIT,"frame bounds");need(!memcmp(f.data(),"KST1",4),"frame magic");Cur r{f,4};unsigned mode=r.get(1);size_t n=r.get(8);uint32_t crc=r.get(4);need(n==expected&&mode<=4,"frame type/length");V out;
 if(mode==4){need(recursion==0,"nested wrapper");size_t ns=r.get(8),cn=r.get(8);need(ns<=STAGE_LIMIT&&cn==f.size()-r.i,"outer length");V inner=unpack(r.bytes(cn),1,ns);out=decode(inner,expected,recursion+1);}
 else{
  size_t pn=r.get(8),tn=r.get(8),rn=r.get(8);unsigned ow=r.get(1),pln=r.get(1),ng=r.get(4);
  need(pn<=(1U<<20)&&tn<=n&&rn<=LIMIT&&ng>=1&&ng<=8&&pln<=1,"header bounds");
  if(mode==0)need(!pn&&!tn&&rn==n&&!ow&&!pln&&ng==1,"raw header");
  else{need(pn>=3&&(ow==1||ow==2||ow==4||ow==8)&&rn%ow==0,"typed header");need(ng==(mode==3?ow:1),"typed groups");}
  V par=r.bytes(pn),rest=r.bytes(tn),res;res.reserve(rn);
  if(mode)need(par[2]==ow&&((par[1]>>3)&1)==pln,"parameter/header disagreement");
  for(unsigned g=0;g<ng;g++){unsigned depth=r.get(1);size_t cn=r.get(8);need(cn<=f.size()-r.i,"group length");V part=unpack(r.bytes(cn),depth,rn/ng);res.insert(res.end(),part.begin(),part.end());}
  need(r.i==f.size(),"trailing frame bytes");
  if(!mode)out=std::move(res);else{res=layout(res,ow,mode!=1,pln);out=take(s_inverse(par.data(),par.size(),res.data(),res.size(),rest.data(),rest.size(),n));}
 }
 need(out.size()==n&&s_crc(out.data(),out.size())==crc,"output checksum/length");return out;
}
static V decoded(const V&f,size_t n,const Arm&a){return a.place<0?take(s_decompress('Z',f.data(),f.size(),n)):decode(f,n);}
static uint64_t now(clockid_t c){timespec t{};need(!clock_gettime(c,&t),"clock");return uint64_t(t.tv_sec)*1000000000ULL+t.tv_nsec;}
static V npy(const std::vector<uint32_t>&v){
 std::string h="{'descr': '<u4', 'fortran_order': False, 'shape': ("+std::to_string(v.size())+",), }";while((10+h.size()+1)%64)h+=' ';h+='\n';V o={0x93,'N','U','M','P','Y',1,0};put(o,h.size(),2);o.insert(o.end(),h.begin(),h.end());for(auto x:v)put(o,x,4);return o;
}
static void selftest(){
 std::mt19937 rng(4113);size_t rt=0,tr=0,mut=0;std::vector<size_t> ns={0,1,2,3,7,17,64,257,4096};
 for(auto n:ns){std::vector<uint32_t> v(n);for(size_t i=0;i<n;i++)v[i]=(i%3)?uint32_t(i):rng();V x=npy(v);
 for(const auto&a:ARMS){V f=encode(x,a),f2=encode(x,a);need(f==f2&&decoded(f,x.size(),a)==x,"selftest roundtrip");rt++;
  if(a.place<0)continue;
  std::vector<size_t> cuts={0,1,4,8,16,f.size()/2,f.size()-1};for(auto k:cuts){bool bad=false;try{decode(V(f.begin(),f.begin()+k),x.size());}catch(...){bad=true;}need(bad,"truncation accepted");tr++;}
  for(int j=0;j<8;j++){V b=f;size_t pos=rng()%b.size();b[pos]^=1u<<(rng()%8);try{V z=decode(b,x.size());need(z==x,"changed accepted output");}catch(const std::exception&e){if(std::string(e.what())=="changed accepted output")throw;}mut++;}
 }}
 // Non-NPY opaque controls use a reversible raw fallback, including empty input.
 for(size_t n:{size_t(0),size_t(3),size_t(127)}){V x(n);for(auto&b:x)b=rng();for(const auto&a:ARMS){V f=encode(x,a);need(decoded(f,n,a)==x,"opaque roundtrip");rt++;}}
 std::cout<<"{\"roundtrips\":"<<rt<<",\"truncations_rejected\":"<<tr<<",\"mutation_probes\":"<<mut<<",\"arms\":"<<NA<<"}\n";
}
int main(int argc,char**argv){try{
 if(argc==2&&std::string(argv[1])=="arms"){for(auto&a:ARMS)std::cout<<a.name<<' '<<a.place<<' '<<a.seq<<'\n';return 0;}
 if(argc==2&&std::string(argv[1])=="selftest"){selftest();return 0;}
 if(argc==5&&std::string(argv[1])=="decode"){V f=read(argv[2]);V x=decode(f,std::stoull(argv[3]));write(argv[4],x);return 0;}
 need(argc==5,"usage: stack INPUT NEW_FRAME_DIR ROUNDS SEED | selftest | arms | decode FRAME ORIGINAL_LEN OUTPUT");
 V x=read(argv[1]);need(x.size()<=LIMIT,"input limit");int rounds=std::stoi(argv[3]);need(rounds>=0&&rounds<=9,"rounds");std::mt19937_64 rng(std::stoull(argv[4]));
 std::vector<V> gold;gold.reserve(NA);
 for(auto&a:ARMS){Stats st;gold.push_back(encode(x,a,&st));need(gold.back()==encode(x,a)&&decoded(gold.back(),x.size(),a)==x,"gold identity");write(std::string(argv[2])+"/"+a.name+".frame",gold.back());
  std::cout<<"{\"kind\":\"frame\",\"arm\":\""<<a.name<<"\",\"bytes\":"<<gold.back().size()<<",\"residual\":"<<st.residual<<",\"rest\":"<<st.rest<<",\"par\":"<<st.par<<",\"groups\":"<<st.groups<<",\"ow\":"<<st.ow<<",\"stages\":[";
  for(size_t i=0;i<st.s.size();i++){auto&s=st.s[i];if(i)std::cout<<',';std::cout<<"{\"codec\":"<<int(s.codec)<<",\"in\":"<<s.in<<",\"out\":"<<s.out<<",\"huf_source_bytes_by_mode\":["<<s.hbytes[0]<<','<<s.hbytes[1]<<','<<s.hbytes[2]<<"]}";}std::cout<<"]}\n";
 }
 need(gold[0]==gold[NA-1]&&gold[13]==gold[NA-2],"duplicate bytes");
 size_t batch=std::max<size_t>(1,std::min<size_t>(8,(2U<<20)/std::max<size_t>(1,x.size())));std::vector<size_t> order(NA);std::iota(order.begin(),order.end(),0);
 for(int op=0;op<2;op++)for(int r=0;r<rounds;r++){std::shuffle(order.begin(),order.end(),rng);for(auto i:order){std::vector<V> pending;pending.reserve(batch);uint64_t c0=now(CLOCK_PROCESS_CPUTIME_ID),w0=now(CLOCK_MONOTONIC);
  for(size_t j=0;j<batch;j++)pending.push_back(op?decoded(gold[i],x.size(),ARMS[i]):encode(x,ARMS[i]));
  uint64_t w1=now(CLOCK_MONOTONIC),c1=now(CLOCK_PROCESS_CPUTIME_ID);for(auto&v:pending)need(v==(op?x:gold[i]),"timed bytes mismatch");
  uint64_t c2=now(CLOCK_PROCESS_CPUTIME_ID),w2=now(CLOCK_MONOTONIC);pending.clear();uint64_t w3=now(CLOCK_MONOTONIC),c3=now(CLOCK_PROCESS_CPUTIME_ID);
  std::cout<<"{\"kind\":\"sample\",\"arm\":\""<<ARMS[i].name<<"\",\"op\":"<<op<<",\"round\":"<<r<<",\"batch\":"<<batch<<",\"input\":"<<x.size()<<",\"bytes\":"<<gold[i].size()<<",\"wall_ns\":"<<w1-w0+w3-w2<<",\"cpu_ns\":"<<c1-c0+c3-c2<<"}\n";
 }}return 0;
}catch(const std::exception&e){std::cerr<<"S1: "<<e.what()<<'\n';return 2;}}
