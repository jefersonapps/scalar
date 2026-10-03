#include "ZipArchive.h"
#include <algorithm>
#include <stdexcept>
namespace scalar {
namespace {
constexpr std::size_t maxBytes=128*1024*1024;
constexpr char filename[]="project.json";
std::uint32_t crc(std::span<const std::uint8_t> data){
    std::uint32_t value=0xffffffff;
    for(auto c:data){value^=c;for(int i=0;i<8;++i)value=(value>>1)^((value&1)?0xedb88320:0);}
    return value^0xffffffff;
}
void put(Bytes& b,std::uint32_t value,int n){for(int i=0;i<n;++i)b.push_back(std::uint8_t((value>>(i*8))&255));}
std::uint32_t get(std::span<const std::uint8_t> b,std::size_t at,int n){
    if(at>b.size()||std::size_t(n)>b.size()-at)return 0;
    std::uint32_t result=0;for(int i=0;i<n;++i)result|=std::uint32_t(b[at+i])<<(i*8);return result;
}
bool nameAt(std::span<const std::uint8_t> b,std::size_t at){return at+12<=b.size()&&std::equal(filename,filename+12,b.begin()+std::ptrdiff_t(at));}
}
Bytes packBoard(std::span<const std::uint8_t> json){
    if(json.size()>maxBytes-122)throw std::length_error("Archive exceeds 128 MiB");
    Bytes b; b.reserve(json.size()+100);const auto sum=crc(json);const auto size=std::uint32_t(json.size());
    put(b,0x04034b50,4);put(b,20,2);put(b,0,2);put(b,0,2);put(b,0,2);put(b,33,2);
    put(b,sum,4);put(b,size,4);put(b,size,4);put(b,12,2);put(b,0,2);
    b.insert(b.end(),filename,filename+12);b.insert(b.end(),json.begin(),json.end());
    const auto directory=std::uint32_t(b.size());
    put(b,0x02014b50,4);put(b,20,2);put(b,20,2);put(b,0,2);put(b,0,2);put(b,0,2);put(b,33,2);
    put(b,sum,4);put(b,size,4);put(b,size,4);put(b,12,2);put(b,0,2);put(b,0,2);put(b,0,2);put(b,0,2);put(b,0,4);put(b,0,4);
    b.insert(b.end(),filename,filename+12);
    put(b,0x06054b50,4);put(b,0,2);put(b,0,2);put(b,1,2);put(b,1,2);put(b,58,4);put(b,directory,4);put(b,0,2);
    return b;
}
ArchiveResult unpackBoard(std::span<const std::uint8_t> b){
    auto fail=[](){return ArchiveResult{{},"Arquivo .board inválido ou corrompido."};};
    if(b.size()<100||b.size()>maxBytes||get(b,0,4)!=0x04034b50||get(b,6,2)!=0||get(b,8,2)!=0||get(b,26,2)!=12||get(b,28,2)!=0||!nameAt(b,30))return fail();
    const std::size_t size=get(b,18,4),dir=42+size;
    if(dir>b.size()||size!=get(b,22,4)||dir+80!=b.size()||get(b,dir,4)!=0x02014b50||get(b,dir+8,2)!=0||get(b,dir+10,2)!=0||get(b,dir+16,4)!=get(b,14,4)||get(b,dir+20,4)!=size||get(b,dir+24,4)!=size||get(b,dir+28,2)!=12||get(b,dir+30,2)!=0||get(b,dir+32,2)!=0||get(b,dir+42,4)!=0||!nameAt(b,dir+46))return fail();
    const auto end=b.size()-22;
    if(get(b,end,4)!=0x06054b50||get(b,end+4,2)!=0||get(b,end+6,2)!=0||get(b,end+8,2)!=1||get(b,end+10,2)!=1||get(b,end+12,4)!=58||get(b,end+16,4)!=dir||get(b,end+20,2)!=0)return fail();
    const auto json=b.subspan(42,size);if(crc(json)!=get(b,14,4))return fail();return {Bytes(json.begin(),json.end()),{}};
}
}
