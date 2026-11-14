// S2 research-only policies. All inverses and KST1 semantics stay frozen at S1.
#define main unused_s1_main
#include "baseline.inc"
#undef main
#include <cmath>

struct Policy { const char* name; int original; int plan; };
// Plans 0..3 are byte-identical copy-reduced H, Z, HZ and HL controls.
static const Policy POLICIES[] = {
 {"zstd3",0,0}, {"zstd3_duplicate",0,0},
 {"s1_H",13,0}, {"opt_H",-1,0}, {"s1_Z",12,0}, {"opt_Z",-1,1},
 {"s1_HZ",18,0}, {"opt_HZ",-1,2}, {"s1_HL",27,0}, {"opt_HL",-1,3},
 {"low_HZ",-1,4}, {"high_HZ",-1,5}, {"low_HL",-1,6}, {"high_HL",-1,7},
 {"gate_HZ",-1,8}, {"gate_menu",-1,9}
};
constexpr size_t NP = sizeof(POLICIES)/sizeof(POLICIES[0]);
struct Decision {
 size_t group=0, input=0, hbytes=0, sampled=0, zsample=0, lsample=0;
 std::string proposed="H", selected="H";
 bool tried=false, rejected=false;
};
using Owner = std::unique_ptr<unsigned char,decltype(&free)>;

// Compress a borrowed span; avoid S1's input-vector and codec-output-vector copies.
static V pack_span(const unsigned char* p, size_t n, char codec) {
 const int c=n?codec:0;
 SBuf b=c?p_compress(c,p,n):SBuf{nullptr,0}; Owner owner(b.p,&free);
 need(!c||b.p,"codec failed"); V result; result.reserve(17+b.n);
 put(result,c,1); put(result,n,8); put(result,b.n,8);
 if(b.n) result.insert(result.end(),b.p,b.p+b.n);
 return result;
}
static V sample_three(const V& v) {
 const size_t total=std::min<size_t>(4096,v.size()); V s; s.reserve(total);
 size_t a=total/3, b=total/3, c=total-a-b;
 s.insert(s.end(),v.begin(),v.begin()+a);
 size_t middle=(v.size()-b)/2;
 s.insert(s.end(),v.begin()+middle,v.begin()+middle+b);
 s.insert(s.end(),v.end()-c,v.end()); return s;
}
static size_t probe_bytes(char c,const V& sample) {
 SBuf b=p_compress(c,sample.data(),sample.size()); Owner own(b.p,&free);
 need(bool(b.p),"sample codec failed"); return b.n;
}
static V group_code(const unsigned char* p,size_t n,unsigned g,unsigned ng,
                    int plan,unsigned& depth,Decision& d) {
 d.group=g; d.input=n; depth=1;
 if(plan==1){d.proposed=d.selected="Z";return pack_span(p,n,'Z');}
 V first=pack_span(p,n,'H'); d.hbytes=first.size(); char second=0;
 if(plan==2) second='Z';
 if(plan==3) second='L';
 if((plan==4&&g<(ng+1)/2)||(plan==5&&g>=ng/2)) second='Z';
 if((plan==6&&g<(ng+1)/2)||(plan==7&&g>=ng/2)) second='L';
 if(plan<8){
  if(!second) return first;
  d.proposed=d.selected=std::string("H")+second; d.tried=true; depth=2;
  return pack_span(first.data(),first.size(),second);
 }
 // In the menu arm, an ineffective full H trial permits one full raw Z trial.
 // The discarded H work remains charged. No additional layer is tried here.
 if(plan==9&&n&&first.size()-17>=n){
  d.proposed="Z"; d.tried=true; V alt=pack_span(p,n,'Z');
  if(alt.size()<first.size()){d.selected="Z";return alt;}
  d.rejected=true; return first;
 }
 if(first.size()<8192) return first;
 V sample=sample_three(first); d.sampled=sample.size();
 d.zsample=probe_bytes('Z',sample); second='Z';
 double estimate=17.0+double(d.zsample)*double(first.size())/double(sample.size());
 if(plan==9){
  d.lsample=probe_bytes('L',sample);
  double le=17.0+double(d.lsample)*double(first.size())/double(sample.size());
  if(le<estimate){estimate=le;second='L';}
 }
 if(estimate+64.0>=double(first.size())) return first;
 d.proposed=std::string("H")+second; d.tried=true;
 V alt=pack_span(first.data(),first.size(),second);
 if(alt.size()<first.size()){depth=2;d.selected=d.proposed;return alt;}
 d.rejected=true;return first;
}
static V optimized(const V& x,int plan,std::vector<Decision>* decisions=nullptr) {
 need(x.size()<=LIMIT,"input limit"); int w=0;size_t h=0;
 int typed=p_type(x.data(),x.size(),&w,&h); SProg prog{};
 bool valid=typed==0&&p_forward(x.data(),x.size(),w,h,&prog)==0;
 Owner par(prog.par.p,&free),res(prog.res.p,&free),rest(prog.rest.p,&free);
 unsigned mode=valid?3:0,ow=valid?prog.ow:0,pln=valid?prog.pln:0;
 const unsigned char* data=valid?prog.res.p:x.data();size_t rn=valid?prog.res.n:x.size();
 V transposed;
 if(valid&&!pln&&ow>1){
  need(rn%ow==0,"transpose shape");transposed.resize(rn);size_t count=rn/ow;
  for(size_t i=0;i<count;i++)for(size_t b=0;b<ow;b++)transposed[b*count+i]=data[i*ow+b];
  data=transposed.data();
 }
 size_t pn=valid?prog.par.n:0,tn=valid?prog.rest.n:0;unsigned ng=valid?ow:1;
 need(ng>=1&&ng<=8&&rn%ng==0,"group shape");
 V out;out.reserve(x.size()+1024);out.insert(out.end(),{'K','S','T','1'});
 put(out,mode,1);put(out,x.size(),8);put(out,p_crc(x.data(),x.size()),4);
 put(out,pn,8);put(out,tn,8);put(out,rn,8);put(out,ow,1);put(out,pln,1);put(out,ng,4);
 if(pn)out.insert(out.end(),prog.par.p,prog.par.p+pn);
 if(tn)out.insert(out.end(),prog.rest.p,prog.rest.p+tn);
 size_t len=rn/ng;
 for(unsigned g=0;g<ng;g++){
  Decision d;unsigned depth=1;
  const unsigned char* group=len?data+g*len:nullptr;
  V coded=group_code(group,len,g,ng,plan,depth,d);
  put(out,depth,1);put(out,coded.size(),8);out.insert(out.end(),coded.begin(),coded.end());
  if(decisions) decisions->push_back(d);
 }
 return out;
}
static V enc_s2(const V&x,size_t i,std::vector<Decision>*d=nullptr){
 const auto&p=POLICIES[i];return p.original>=0?encode(x,ARMS[p.original]):optimized(x,p.plan,d);
}
static V dec_s2(const V&f,size_t n,size_t i){return i<2?take(s_decompress('Z',f.data(),f.size(),n)):decode(f,n);}
static void identities(const std::vector<V>&v){
 need(v[0]==v[1],"zstd duplicates");
 for(size_t i:{2U,4U,6U,8U})need(v[i]==v[i+1],"copy optimization changed output");
}
static void self_s2(){
 std::mt19937 rng(7113);size_t rt=0,tr=0,mut=0;
 for(size_t n:{0U,1U,2U,3U,7U,17U,64U,257U,4096U}){
  std::vector<uint32_t> values(n);for(size_t k=0;k<n;k++)values[k]=k%3?uint32_t(k):rng();
  V x=npy(values);std::vector<V> all;
  for(size_t a=0;a<NP;a++){
   V f=enc_s2(x,a);need(f==enc_s2(x,a)&&dec_s2(f,x.size(),a)==x,"selftest");all.push_back(f);rt++;
   if(a<2)continue;
   for(size_t k:{size_t(0),size_t(1),size_t(4),size_t(8),size_t(16),f.size()/2,f.size()-1}){
    bool failed=false;try{decode(V(f.begin(),f.begin()+k),x.size());}catch(...){failed=true;}
    need(failed,"truncated frame accepted");tr++;
   }
   for(unsigned j=0;j<8;j++){
    V f2=f;f2[rng()%f2.size()]^=1u<<(rng()%8);
    try{V z=decode(f2,x.size());need(z==x,"changed accepted output");}
    catch(const std::exception&e){if(std::string(e.what())=="changed accepted output")throw;}
    mut++;
   }
  }identities(all);
 }
 for(size_t n:{0U,3U,127U}){
  V x(n);for(auto&b:x)b=rng();std::vector<V> all;
  for(size_t a=0;a<NP;a++){all.push_back(enc_s2(x,a));need(dec_s2(all.back(),n,a)==x,"opaque");rt++;}identities(all);
 }
 // Sample spans, accepted/rejected policy paths and guard thresholds are deterministic.
 for(size_t n:{8191U,8192U,12288U}){V v(n,42);auto s=sample_three(v);need(s.size()==4096&&std::all_of(s.begin(),s.end(),[](unsigned char x){return x==42;}),"sample spans");}
 std::cout<<"{\"roundtrips\":"<<rt<<",\"truncations_rejected\":"<<tr<<",\"mutation_probes\":"<<mut<<",\"arms\":"<<NP<<"}\n";
}
int main(int argc,char**argv){try{
 if(argc==2&&std::string(argv[1])=="arms"){for(auto&p:POLICIES)std::cout<<p.name<<'\n';return 0;}
 if(argc==2&&std::string(argv[1])=="selftest"){self_s2();return 0;}
 if(argc==5&&std::string(argv[1])=="decode"){V f=read(argv[2]);write(argv[4],decode(f,std::stoull(argv[3])));return 0;}
 if(argc==4&&std::string(argv[1])=="profile"){
  V x=read(argv[2]);unsigned reps=std::stoul(argv[3]);need(reps>0&&reps<=20,"profile reps");
  std::vector<size_t> order(NP);std::iota(order.begin(),order.end(),0);std::mt19937 rng(82713);
  for(unsigned r=0;r<reps;r++){std::shuffle(order.begin(),order.end(),rng);for(auto a:order){
   pc={};profiling=true;uint64_t t=now(CLOCK_MONOTONIC);V f=enc_s2(x,a);uint64_t total=now(CLOCK_MONOTONIC)-t;profiling=false;
   need(dec_s2(f,x.size(),a)==x,"profile roundtrip");
   std::cout<<"{\"arm\":\""<<POLICIES[a].name<<"\",\"round\":"<<r<<",\"total_ns\":"<<total<<",\"typing_ns\":"<<pc.type<<",\"forward_ns\":"<<pc.forward<<",\"codec_ns\":"<<pc.codec<<",\"crc_ns\":"<<pc.crc<<",\"codec_calls\":"<<pc.calls<<"}\n";
  }}return 0;
 }
 need(argc==5,"usage: s2 INPUT OUT ROUNDS SEED | arms | selftest | profile INPUT REPS");
 V x=read(argv[1]);need(x.size()<=LIMIT,"input limit");int rounds=std::stoi(argv[3]);need(rounds>=0&&rounds<=9,"rounds");std::mt19937_64 rng(std::stoull(argv[4]));
 std::vector<V> gold;
 for(size_t a=0;a<NP;a++){
  std::vector<Decision> ds;gold.push_back(enc_s2(x,a,&ds));need(gold.back()==enc_s2(x,a)&&dec_s2(gold.back(),x.size(),a)==x,"gold identity");
  write(std::string(argv[2])+"/"+POLICIES[a].name+".frame",gold.back());
  std::cout<<"{\"kind\":\"frame\",\"arm\":\""<<POLICIES[a].name<<"\",\"bytes\":"<<gold.back().size()<<",\"decisions\":[";
  for(size_t k=0;k<ds.size();k++){auto&d=ds[k];if(k)std::cout<<',';
   std::cout<<"{\"group\":"<<d.group<<",\"input\":"<<d.input<<",\"hbytes\":"<<d.hbytes<<",\"sampled\":"<<d.sampled<<",\"sample_z\":"<<d.zsample<<",\"sample_l\":"<<d.lsample<<",\"proposed\":\""<<d.proposed<<"\",\"selected\":\""<<d.selected<<"\",\"tried\":"<<d.tried<<",\"rejected\":"<<d.rejected<<'}';}
  std::cout<<"]}\n";
 }identities(gold);
 size_t batch=std::max<size_t>(1,std::min<size_t>(8,(2U<<20)/std::max<size_t>(1,x.size())));std::vector<size_t> order(NP);std::iota(order.begin(),order.end(),0);
 for(int op=0;op<2;op++)for(int r=0;r<rounds;r++){std::shuffle(order.begin(),order.end(),rng);for(auto a:order){
  std::vector<V> pending;pending.reserve(batch);uint64_t c0=now(CLOCK_PROCESS_CPUTIME_ID),w0=now(CLOCK_MONOTONIC);
  for(size_t j=0;j<batch;j++)pending.push_back(op?dec_s2(gold[a],x.size(),a):enc_s2(x,a));
  uint64_t w1=now(CLOCK_MONOTONIC),c1=now(CLOCK_PROCESS_CPUTIME_ID);for(auto&v:pending)need(v==(op?x:gold[a]),"timed bytes");
  uint64_t c2=now(CLOCK_PROCESS_CPUTIME_ID),w2=now(CLOCK_MONOTONIC);pending.clear();uint64_t w3=now(CLOCK_MONOTONIC),c3=now(CLOCK_PROCESS_CPUTIME_ID);
  std::cout<<"{\"kind\":\"sample\",\"arm\":\""<<POLICIES[a].name<<"\",\"op\":"<<op<<",\"round\":"<<r<<",\"batch\":"<<batch<<",\"input\":"<<x.size()<<",\"bytes\":"<<gold[a].size()<<",\"wall_ns\":"<<w1-w0+w3-w2<<",\"cpu_ns\":"<<c1-c0+c3-c2<<"}\n";
 }}return 0;
}catch(const std::exception&e){std::cerr<<"S2: "<<e.what()<<'\n';return 2;}}
