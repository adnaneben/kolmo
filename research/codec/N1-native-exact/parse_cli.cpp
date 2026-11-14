#include "header_parser.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**argv){try{exact::need(argc==2,"usage: parse_cli FILE");std::ifstream f(argv[1],std::ios::binary|std::ios::ate);exact::need(bool(f),"open failed");auto n=f.tellg();exact::need(n>=0&&uint64_t(n)<=exact::max_input,"input limit");f.seekg(0);std::vector<uint8_t>b(static_cast<size_t>(n));f.read(reinterpret_cast<char*>(b.data()),n);exact::need(bool(f)||n==0,"read failed");auto x=exact::inspect(b);std::cout<<"{\"kind\":\""<<x.kind<<"\",\"prefix\":"<<x.prefix<<",\"payload\":"<<x.payload<<",\"tail\":"<<x.tail<<",\"width\":"<<x.width<<",\"count\":"<<x.count<<",\"big\":"<<x.big<<"}\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
