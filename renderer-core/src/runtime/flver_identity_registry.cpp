#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/flver_identity_registry.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {
namespace {

constexpr std::uintptr_t k_flver_parse_rva = 0x20D910u;
constexpr std::uintptr_t k_model_destructor_rva = 0x20D7A0u;
constexpr std::size_t k_container_offset = 0x88u;
constexpr std::uint64_t k_max_flver_bytes = 0x40000000ull;
constexpr char k_expected_exe_sha[] =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";

constexpr std::array<std::uint8_t, 16> k_flver_parse_bytes = {
    0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57
};
constexpr std::array<std::uint8_t, 15> k_model_destructor_bytes = {
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18
};

struct sha_ctx {
    std::array<std::uint32_t,8> h{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    std::array<std::uint8_t,64> buf{};
    std::size_t used=0;
    std::uint64_t total=0;
};

constexpr std::array<std::uint32_t,64> k_sha = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

constexpr std::uint32_t rotr(std::uint32_t x,int n) noexcept { return (x>>n)|(x<<(32-n)); }
void sha_block(sha_ctx &c,const std::uint8_t *p) noexcept {
    std::uint32_t w[64]{};
    for(int i=0;i<16;++i) w[i]=(std::uint32_t(p[4*i])<<24)|(std::uint32_t(p[4*i+1])<<16)|(std::uint32_t(p[4*i+2])<<8)|p[4*i+3];
    for(int i=16;i<64;++i){const auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);const auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
    auto a=c.h[0],b=c.h[1],cc=c.h[2],d=c.h[3],e=c.h[4],f=c.h[5],g=c.h[6],hh=c.h[7];
    for(int i=0;i<64;++i){const auto s1=rotr(e,6)^rotr(e,11)^rotr(e,25);const auto ch=(e&f)^((~e)&g);const auto t1=hh+s1+ch+k_sha[i]+w[i];const auto s0=rotr(a,2)^rotr(a,13)^rotr(a,22);const auto maj=(a&b)^(a&cc)^(b&cc);const auto t2=s0+maj;hh=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;}
    c.h[0]+=a;c.h[1]+=b;c.h[2]+=cc;c.h[3]+=d;c.h[4]+=e;c.h[5]+=f;c.h[6]+=g;c.h[7]+=hh;
}
void sha_update(sha_ctx &c,const std::uint8_t *p,std::size_t n) noexcept {
    c.total+=n; while(n){const auto take=std::min(n,64u-c.used);std::memcpy(c.buf.data()+c.used,p,take);c.used+=take;p+=take;n-=take;if(c.used==64){sha_block(c,c.buf.data());c.used=0;}}
}
std::array<std::uint8_t,32> sha_finish(sha_ctx &c) noexcept {
    const auto bits=c.total*8u;c.buf[c.used++]=0x80u;if(c.used>56){std::fill(c.buf.begin()+c.used,c.buf.end(),0u);sha_block(c,c.buf.data());c.used=0;}std::fill(c.buf.begin()+c.used,c.buf.begin()+56,0u);for(int i=0;i<8;++i)c.buf[63-i]=static_cast<std::uint8_t>(bits>>(8*i));sha_block(c,c.buf.data());
    std::array<std::uint8_t,32> out{};for(int i=0;i<8;++i){out[4*i]=static_cast<std::uint8_t>(c.h[i]>>24);out[4*i+1]=static_cast<std::uint8_t>(c.h[i]>>16);out[4*i+2]=static_cast<std::uint8_t>(c.h[i]>>8);out[4*i+3]=static_cast<std::uint8_t>(c.h[i]);}return out;
}
std::array<std::uint8_t,32> sha_memory(const void *p,std::size_t n) noexcept { sha_ctx c;sha_update(c,static_cast<const std::uint8_t*>(p),n);return sha_finish(c); }
std::string hex(const std::array<std::uint8_t,32> &d){static constexpr char h[]="0123456789abcdef";std::string s(64,'0');for(std::size_t i=0;i<d.size();++i){s[2*i]=h[d[i]>>4];s[2*i+1]=h[d[i]&15u];}return s;}

bool safe_copy(const void *src,void *dst,std::size_t n) noexcept {
#if defined(_MSC_VER)
    __try { std::memcpy(dst,src,n); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    if(!src||!dst)return false;std::memcpy(dst,src,n);return true;
#endif
}

struct hook {
    void *target=nullptr,*trampoline=nullptr,*detour=nullptr;
    std::size_t stolen=0;
    std::array<std::uint8_t,32> original{};
    bool patched=false;
};
std::uintptr_t g_base=0;
hook g_parse{},g_destroy{};
using parse_fn=void(__fastcall*)(void*,const void*);
using destroy_fn=void(__fastcall*)(void*);
parse_fn g_parse_original=nullptr;
destroy_fn g_destroy_original=nullptr;

std::mutex g_mutex;
std::unordered_map<const void*,std::array<std::uint8_t,32>> g_by_model;
std::atomic<std::uint64_t> g_inserts{0},g_lookups{0},g_hits{0},g_misses{0},g_erases{0},g_invalid{0};

bool write_bytes(void *at,const void *src,std::size_t n) noexcept {
    DWORD old=0;if(!VirtualProtect(at,n,PAGE_EXECUTE_READWRITE,&old))return false;std::memcpy(at,src,n);const bool ok=FlushInstructionCache(GetCurrentProcess(),at,n)!=FALSE;DWORD ignored=0;VirtualProtect(at,n,old,&ignored);return ok;
}
template<std::size_t N>
bool prepare(hook &h,std::uintptr_t rva,const std::array<std::uint8_t,N> &expected,void *detour) noexcept {
    static_assert(N>=14&&N<=32);auto *target=reinterpret_cast<std::uint8_t*>(g_base+rva);std::array<std::uint8_t,N> got{};if(!safe_copy(target,got.data(),N)||got!=expected)return false;
    void *tr=VirtualAlloc(nullptr,N+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tr)return false;std::memcpy(tr,target,N);auto *tail=static_cast<std::uint8_t*>(tr)+N;tail[0]=0xff;tail[1]=0x25;std::uint32_t z=0;std::memcpy(tail+2,&z,4);const auto back=reinterpret_cast<std::uint64_t>(target+N);std::memcpy(tail+6,&back,8);FlushInstructionCache(GetCurrentProcess(),tr,N+14);
    h.target=target;h.trampoline=tr;h.detour=detour;h.stolen=N;std::copy(got.begin(),got.end(),h.original.begin());return true;
}
bool patch(hook &h) noexcept {
    std::array<std::uint8_t,32> p{};p.fill(0x90);p[0]=0xff;p[1]=0x25;std::uint32_t z=0;std::memcpy(p.data()+2,&z,4);const auto d=reinterpret_cast<std::uint64_t>(h.detour);std::memcpy(p.data()+6,&d,8);h.patched=true;return write_bytes(h.target,p.data(),h.stolen);
}
void restore(hook &h) noexcept {
    if(h.patched){write_bytes(h.target,h.original.data(),h.stolen);h.patched=false;}if(h.trampoline){VirtualFree(h.trampoline,0,MEM_RELEASE);h.trampoline=nullptr;}h.target=nullptr;h.detour=nullptr;h.stolen=0;
}

bool exe_provenance() {
    wchar_t path[32768]{};const DWORD n=GetModuleFileNameW(nullptr,path,32768);if(n==0||n>=32768)return false;std::ifstream f(path,std::ios::binary);if(!f)return false;sha_ctx c;std::vector<char> b(1u<<20);while(f){f.read(b.data(),static_cast<std::streamsize>(b.size()));const auto got=f.gcount();if(got>0)sha_update(c,reinterpret_cast<const std::uint8_t*>(b.data()),static_cast<std::size_t>(got));}return hex(sha_finish(c))==k_expected_exe_sha;
}

void observe_parse(void *model,const void *raw) noexcept {
    if(!model||!raw){++g_invalid;return;}
    std::array<std::uint8_t,0x18> header{};if(!safe_copy(raw,header.data(),header.size())){++g_invalid;return;}
    constexpr std::array<std::uint8_t,6> magic={'F','L','V','E','R',0};if(!std::equal(magic.begin(),magic.end(),header.begin())){++g_invalid;return;}
    std::uint32_t off=0,len=0;std::memcpy(&off,header.data()+0x0c,4);std::memcpy(&len,header.data()+0x10,4);const std::uint64_t total=static_cast<std::uint64_t>(off)+len;
    if(off<0x40u||total<off||total>k_max_flver_bytes){++g_invalid;return;}
    // Hash before the retail parser rebases offsets in-place.
    const auto digest=sha_memory(raw,static_cast<std::size_t>(total));
    {std::lock_guard<std::mutex> lock(g_mutex);g_by_model[model]=digest;}++g_inserts;
}
void erase_model(void *model) noexcept {
    if(!model)return;std::lock_guard<std::mutex> lock(g_mutex);const auto n=g_by_model.erase(model);if(n)++g_erases;
}
void __fastcall parse_detour(void *model,const void *raw) noexcept { observe_parse(model,raw);if(g_parse_original)g_parse_original(model,raw); }
void __fastcall destroy_detour(void *model) noexcept { erase_model(model);if(g_destroy_original)g_destroy_original(model); }

} // namespace

bool flver_identity_install() noexcept {
    if(g_parse.patched||g_destroy.patched)return true;
    try {
        if(!exe_provenance())return false;
        g_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));if(!g_base)return false;
        if(!prepare(g_parse,k_flver_parse_rva,k_flver_parse_bytes,reinterpret_cast<void*>(&parse_detour)))goto fail;
        g_parse_original=reinterpret_cast<parse_fn>(g_parse.trampoline);
        if(!prepare(g_destroy,k_model_destructor_rva,k_model_destructor_bytes,reinterpret_cast<void*>(&destroy_detour)))goto fail;
        g_destroy_original=reinterpret_cast<destroy_fn>(g_destroy.trampoline);
        if(!patch(g_parse)||!patch(g_destroy))goto fail;
        return true;
    } catch(...) {}
fail:
    flver_identity_uninstall();return false;
}
void flver_identity_uninstall() noexcept {
    restore(g_destroy);restore(g_parse);g_destroy_original=nullptr;g_parse_original=nullptr;g_base=0;
    std::lock_guard<std::mutex> lock(g_mutex);g_by_model.clear();
}
bool flver_identity_lookup(const void *selector_container,std::array<std::uint8_t,32> &sha256) noexcept {
    ++g_lookups;if(!selector_container){++g_misses;return false;}const auto address=reinterpret_cast<std::uintptr_t>(selector_container);if(address<k_container_offset){++g_misses;return false;}const void *model=reinterpret_cast<const void*>(address-k_container_offset);
    std::lock_guard<std::mutex> lock(g_mutex);const auto it=g_by_model.find(model);if(it==g_by_model.end()){++g_misses;return false;}sha256=it->second;++g_hits;return true;
}
flver_identity_telemetry flver_identity_stats() noexcept {
    return {g_inserts.load(),g_lookups.load(),g_hits.load(),g_misses.load(),g_erases.load(),g_invalid.load()};
}

} // namespace dsrrl::runtime
