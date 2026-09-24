#include "dsrrl/dxbc_checksum.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl { namespace {
constexpr std::array<std::uint32_t,64> K = {
0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u};
constexpr std::array<int,64> S = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
constexpr std::uint32_t rol(std::uint32_t x,int n) noexcept { return (x<<n)|(x>>(32-n)); }
std::uint32_t rd(const std::uint8_t *p) noexcept { std::uint32_t v=0; std::memcpy(&v,p,4); return v; }
void wr(std::uint8_t *p,std::uint32_t v) noexcept { std::memcpy(p,&v,4); }
void transform(std::array<std::uint32_t,4> &st,const std::uint8_t *block) noexcept {
    std::uint32_t M[16]{}; for(int i=0;i<16;++i) M[i]=rd(block+4*i);
    auto a=st[0],b=st[1],c=st[2],d=st[3]; const auto A=a,B=b,C=c,D=d;
    for(int i=0;i<64;++i){ std::uint32_t f=0; int g=0;
        if(i<16){f=(b&c)|((~b)&d);g=i;} else if(i<32){f=(d&b)|((~d)&c);g=(5*i+1)%16;}
        else if(i<48){f=b^c^d;g=(3*i+5)%16;} else {f=c^(b|(~d));g=(7*i)%16;}
        const auto oldd=d; d=c; c=b; b=b+rol(a+f+K[static_cast<std::size_t>(i)]+M[g],S[static_cast<std::size_t>(i)]); a=oldd;
    }
    st[0]=A+a; st[1]=B+b; st[2]=C+c; st[3]=D+d;
}
}
bool fix_dxbc_checksum(std::vector<std::uint8_t> &v) noexcept {
    if(v.size()<0x20 || std::memcmp(v.data(),"DXBC",4)!=0) return false;
    const auto *p=v.data()+0x14; const std::size_t n=v.size()-0x14; const std::uint64_t bit=static_cast<std::uint64_t>(n)*8u;
    std::array<std::uint32_t,4> st{0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u};
    const std::size_t full=n&~std::size_t(63); for(std::size_t o=0;o<full;o+=64) transform(st,p+o);
    const std::size_t tail=n-full; std::array<std::uint8_t,64> b{};
    if(tail>=56){ std::memcpy(b.data(),p+full,tail); b[tail]=0x80; transform(st,b.data()); b.fill(0); wr(b.data(),static_cast<std::uint32_t>(bit)); wr(b.data()+60,static_cast<std::uint32_t>((bit>>2)|1u)); transform(st,b.data()); }
    else { wr(b.data(),static_cast<std::uint32_t>(bit)); std::memcpy(b.data()+4,p+full,tail); b[4+tail]=0x80; wr(b.data()+60,static_cast<std::uint32_t>((bit>>2)|1u)); transform(st,b.data()); }
    for(int i=0;i<4;++i) wr(v.data()+4+4*i,st[static_cast<std::size_t>(i)]);
    return true;
}
}
