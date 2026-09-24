#include "dsrrl/sha256.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace dsrrl {
namespace {
constexpr std::array<std::uint32_t,64> K = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
constexpr std::uint32_t rotr(std::uint32_t x, int n){ return (x>>n)|(x<<(32-n)); }
struct ctx {
 std::array<std::uint32_t,8> h{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
 std::array<std::uint8_t,64> buf{}; std::size_t used=0; std::uint64_t total=0;
 void block(const std::uint8_t *p){
  std::uint32_t w[64]{};
  for(int i=0;i<16;++i) w[i]=(std::uint32_t(p[4*i])<<24)|(std::uint32_t(p[4*i+1])<<16)|(std::uint32_t(p[4*i+2])<<8)|p[4*i+3];
  for(int i=16;i<64;++i){ auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3); auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
  for(int i=0;i<64;++i){ auto S1=rotr(e,6)^rotr(e,11)^rotr(e,25); auto ch=(e&f)^((~e)&g); auto t1=hh+S1+ch+K[i]+w[i]; auto S0=rotr(a,2)^rotr(a,13)^rotr(a,22); auto maj=(a&b)^(a&c)^(b&c); auto t2=S0+maj; hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
 }
 void update(const std::uint8_t *p,std::size_t n){ total += n; while(n){ auto take=std::min(n,64-used); std::copy_n(p,take,buf.data()+used); used+=take;p+=take;n-=take; if(used==64){block(buf.data());used=0;} } }
 sha256_digest finish(){
  const std::uint64_t bits=total*8; buf[used++]=0x80; if(used>56){std::fill(buf.begin()+used,buf.end(),0);block(buf.data());used=0;} std::fill(buf.begin()+used,buf.begin()+56,0);
  for(int i=0;i<8;++i) { buf[63-i]=std::uint8_t(bits>>(8*i)); }
  block(buf.data());
  sha256_digest out{}; for(int i=0;i<8;++i){out[4*i]=h[i]>>24;out[4*i+1]=h[i]>>16;out[4*i+2]=h[i]>>8;out[4*i+3]=h[i];} return out;
 }
};
int hexv(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
}
sha256_digest sha256(std::span<const std::byte> data){ctx c;c.update(reinterpret_cast<const std::uint8_t*>(data.data()),data.size());return c.finish();}
sha256_digest sha256_file(const std::filesystem::path &path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot open file");ctx c;std::vector<char> b(1u<<20);while(f){f.read(b.data(),static_cast<std::streamsize>(b.size()));auto n=f.gcount();if(n>0)c.update(reinterpret_cast<const std::uint8_t*>(b.data()),static_cast<std::size_t>(n));}return c.finish();}
std::string to_hex(const sha256_digest &d){static constexpr char H[]="0123456789abcdef";std::string s(64,'0');for(std::size_t i=0;i<32;++i){s[2*i]=H[d[i]>>4];s[2*i+1]=H[d[i]&15];}return s;}
bool parse_sha256(std::string_view s, sha256_digest &out){if(s.size()!=64)return false;for(std::size_t i=0;i<32;++i){int a=hexv(s[2*i]),b=hexv(s[2*i+1]);if(a<0||b<0)return false;out[i]=std::uint8_t((a<<4)|b);}return true;}
}