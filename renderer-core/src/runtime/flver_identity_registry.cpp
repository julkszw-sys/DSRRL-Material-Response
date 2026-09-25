#include "dsrrl/runtime/flver_identity_registry.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime {
namespace {

constexpr std::size_t k_container_offset = 0x88u;
constexpr std::uint64_t k_max_flver_bytes = 0x40000000ull;

struct sha256_context {
    std::array<std::uint32_t,8> h{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    std::array<std::uint8_t,64> buffer{};
    std::size_t used=0;
    std::uint64_t total=0;
};

constexpr std::array<std::uint32_t,64> k_round = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

constexpr std::uint32_t rotr(std::uint32_t x,int n) noexcept { return (x>>n)|(x<<(32-n)); }
void block(sha256_context &c,const std::uint8_t *p) noexcept {
    std::uint32_t w[64]{};
    for(int i=0;i<16;++i)w[i]=(std::uint32_t(p[4*i])<<24)|(std::uint32_t(p[4*i+1])<<16)|(std::uint32_t(p[4*i+2])<<8)|p[4*i+3];
    for(int i=16;i<64;++i){const auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);const auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
    auto a=c.h[0],b=c.h[1],cc=c.h[2],d=c.h[3],e=c.h[4],f=c.h[5],g=c.h[6],hh=c.h[7];
    for(int i=0;i<64;++i){const auto s1=rotr(e,6)^rotr(e,11)^rotr(e,25);const auto ch=(e&f)^((~e)&g);const auto t1=hh+s1+ch+k_round[i]+w[i];const auto s0=rotr(a,2)^rotr(a,13)^rotr(a,22);const auto maj=(a&b)^(a&cc)^(b&cc);const auto t2=s0+maj;hh=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;}
    c.h[0]+=a;c.h[1]+=b;c.h[2]+=cc;c.h[3]+=d;c.h[4]+=e;c.h[5]+=f;c.h[6]+=g;c.h[7]+=hh;
}
void update(sha256_context &c,const std::uint8_t *p,std::size_t n) noexcept {
    c.total+=n;while(n){const auto take=std::min(n,64u-c.used);std::memcpy(c.buffer.data()+c.used,p,take);c.used+=take;p+=take;n-=take;if(c.used==64){block(c,c.buffer.data());c.used=0;}}
}
std::array<std::uint8_t,32> finish(sha256_context &c) noexcept {
    const auto bits=c.total*8u;c.buffer[c.used++]=0x80u;if(c.used>56){std::fill(c.buffer.begin()+c.used,c.buffer.end(),0u);block(c,c.buffer.data());c.used=0;}std::fill(c.buffer.begin()+c.used,c.buffer.begin()+56,0u);for(int i=0;i<8;++i)c.buffer[63-i]=static_cast<std::uint8_t>(bits>>(8*i));block(c,c.buffer.data());
    std::array<std::uint8_t,32> out{};for(int i=0;i<8;++i){out[4*i]=static_cast<std::uint8_t>(c.h[i]>>24);out[4*i+1]=static_cast<std::uint8_t>(c.h[i]>>16);out[4*i+2]=static_cast<std::uint8_t>(c.h[i]>>8);out[4*i+3]=static_cast<std::uint8_t>(c.h[i]);}return out;
}
std::array<std::uint8_t,32> digest(const void *p,std::size_t n) noexcept {sha256_context c;update(c,static_cast<const std::uint8_t*>(p),n);return finish(c);}

std::mutex g_mutex;
std::unordered_map<const void*,std::array<std::uint8_t,32>> g_by_model;
std::atomic<std::uint64_t> g_inserts{0},g_lookups{0},g_hits{0},g_misses{0},g_erases{0},g_invalid{0};

} // namespace

bool flver_identity_observe_parse(const void *model,const void *raw,std::size_t readable_bytes) noexcept {
    if(!model||!raw||readable_bytes<0x18u){++g_invalid;return false;}
    const auto *bytes=static_cast<const std::uint8_t*>(raw);
    constexpr std::array<std::uint8_t,6> magic={'F','L','V','E','R',0};
    if(!std::equal(magic.begin(),magic.end(),bytes)){++g_invalid;return false;}
    std::uint32_t data_offset=0,data_length=0;std::memcpy(&data_offset,bytes+0x0c,4);std::memcpy(&data_length,bytes+0x10,4);
    const std::uint64_t total=static_cast<std::uint64_t>(data_offset)+data_length;
    if(data_offset<0x40u||total<data_offset||total>k_max_flver_bytes||total>readable_bytes){++g_invalid;return false;}
    const auto sha=digest(raw,static_cast<std::size_t>(total));
    {std::lock_guard<std::mutex> lock(g_mutex);g_by_model[model]=sha;}++g_inserts;return true;
}
void flver_identity_observe_destroy(const void *model) noexcept {
    if (!model) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_by_model.erase(model) != 0u) {
        ++g_erases;
    }
}
bool flver_identity_lookup(const void *selector_container,std::array<std::uint8_t,32> &sha256) noexcept {
    ++g_lookups;if(!selector_container){++g_misses;return false;}const auto address=reinterpret_cast<std::uintptr_t>(selector_container);if(address<k_container_offset){++g_misses;return false;}const auto *model=reinterpret_cast<const void*>(address-k_container_offset);
    std::lock_guard<std::mutex> lock(g_mutex);const auto it=g_by_model.find(model);if(it==g_by_model.end()){++g_misses;return false;}sha256=it->second;++g_hits;return true;
}
void flver_identity_reset() noexcept {std::lock_guard<std::mutex> lock(g_mutex);g_by_model.clear();}
flver_identity_telemetry flver_identity_stats() noexcept {return {g_inserts.load(),g_lookups.load(),g_hits.load(),g_misses.load(),g_erases.load(),g_invalid.load()};}

} // namespace dsrrl::runtime
