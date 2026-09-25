#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/flver_identity_registry.hpp"
#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/runtime/material_owner_producer.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#pragma comment(lib,"bcrypt.lib")

extern "C" void dsrrl_flver_selector_hook_entry();
extern "C" { void *g_dsrrl_flver_selector_trampoline=nullptr; }

namespace dsrrl::runtime::flver_identity_transport {
namespace {
constexpr char k_sha[]="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr std::uintptr_t k_parse=0x20D910u,k_selector=0x22BA20u,k_destroy=0x20D7A0u;
constexpr std::array<std::uint8_t,16> k_parse_b={0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
constexpr std::array<std::uint8_t,15> k_selector_b={0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA};
constexpr std::array<std::uint8_t,20> k_destroy_b={0x40,0x57,0x48,0x83,0xEC,0x30,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,0x48,0x89,0x5C,0x24,0x40};
struct hook{void *target=nullptr,*trampoline=nullptr,*detour=nullptr;std::size_t stolen=0;std::array<std::uint8_t,32> original{};bool patched=false;};
std::uintptr_t g_base=0; hook g_p{},g_s{},g_d{}; hook_status g_state{};
using parser_fn=void(__fastcall *)(void *,const void *); using destructor_fn=void(__fastcall *)(void *);
parser_fn g_po=nullptr; destructor_fn g_do=nullptr;

thread_local operators::material_response::material_identity g_selector_owner_candidate{};
thread_local bool g_selector_owner_valid=false;
std::atomic<std::uint64_t> g_selector_events{0};
std::atomic<std::uint64_t> g_owner_sha_hits{0};
std::atomic<std::uint64_t> g_owner_mtd_hits{0};
std::atomic<std::uint64_t> g_exact_owner_ready{0};
std::atomic<std::uint64_t> g_owner_fail_open{0};
std::atomic<std::uint64_t> g_owner_consumed{0};
std::atomic<std::uint64_t> g_owner_consume_misses{0};

bool range_ok(const void *p,std::size_t n) noexcept {
 if(!p)return false;if(n==0)return true;auto cur=reinterpret_cast<std::uintptr_t>(p);const auto end=cur+n;if(end<cur)return false;
 while(cur<end){MEMORY_BASIC_INFORMATION m{};if(VirtualQuery(reinterpret_cast<const void*>(cur),&m,sizeof(m))!=sizeof(m))return false;
  const DWORD a=m.Protect&0xffu;const bool rd=m.State==MEM_COMMIT&&(m.Protect&PAGE_GUARD)==0u&&(a==PAGE_READONLY||a==PAGE_READWRITE||a==PAGE_WRITECOPY||a==PAGE_EXECUTE_READ||a==PAGE_EXECUTE_READWRITE||a==PAGE_EXECUTE_WRITECOPY);if(!rd)return false;
  const auto re=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;if(re<=cur)return false;cur=std::min(re,end);}return true;
}
bool write(void *p,const void *s,std::size_t n) noexcept {DWORD old=0;if(!VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&old))return false;std::memcpy(p,s,n);const bool ok=FlushInstructionCache(GetCurrentProcess(),p,n)!=FALSE;DWORD x=0;VirtualProtect(p,n,old,&x);return ok;}
template<std::size_t N> bool prep(hook &h,std::uintptr_t r,const std::array<std::uint8_t,N>&e,void*d) noexcept {
 static_assert(N>=14&&N<=32);auto*t=reinterpret_cast<std::uint8_t*>(g_base+r);if(!range_ok(t,N)||std::memcmp(t,e.data(),N)!=0)return false;
 void*tr=VirtualAlloc(nullptr,N+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tr)return false;std::memcpy(tr,t,N);auto*tail=static_cast<std::uint8_t*>(tr)+N;tail[0]=0xff;tail[1]=0x25;std::uint32_t z=0;std::memcpy(tail+2,&z,4);const auto back=reinterpret_cast<std::uint64_t>(t+N);std::memcpy(tail+6,&back,8);
 if(FlushInstructionCache(GetCurrentProcess(),tr,N+14)==FALSE){VirtualFree(tr,0,MEM_RELEASE);return false;}
 h.target=t;h.trampoline=tr;h.detour=d;h.stolen=N;std::copy(e.begin(),e.end(),h.original.begin());return true;}
bool arm(hook&h) noexcept {std::array<std::uint8_t,32>b{};b.fill(0x90);b[0]=0xff;b[1]=0x25;std::uint32_t z=0;std::memcpy(b.data()+2,&z,4);const auto d=reinterpret_cast<std::uint64_t>(h.detour);std::memcpy(b.data()+6,&d,8);h.patched=true;return write(h.target,b.data(),h.stolen);}
bool restore(hook&h) noexcept {
 if(h.patched){
  if(!h.target||h.stolen==0u||!write(h.target,h.original.data(),h.stolen))
   return false;
  // Verify the actual instruction bytes before releasing the trampoline/state.
  if(std::memcmp(h.target,h.original.data(),h.stolen)!=0)
   return false;
  h.patched=false;
 }
 if(h.trampoline){
  if(VirtualFree(h.trampoline,0,MEM_RELEASE)==FALSE)
   return false;
 }
 h={};
 return true;
}
std::wstring path(){std::wstring p(32768,L'\0');const DWORD n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));if(n==0||n>=p.size())return{};p.resize(n);return p;}
bool exe_ok(){
 const auto p=path();if(p.empty())return false;BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;DWORD ol=0,hl=0,cb=0;std::array<std::uint8_t,32>d{};bool ok=false;
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 if(BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&ol),sizeof(ol),&cb,0)>=0&&BCryptGetProperty(a,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hl),sizeof(hl),&cb,0)>=0&&hl==32){std::vector<std::uint8_t>o(ol);if(BCryptCreateHash(a,&h,o.data(),ol,nullptr,0,0)>=0){std::ifstream f(p,std::ios::binary);std::array<char,65536>b{};ok=!!f;while(ok&&f){f.read(b.data(),b.size());const auto n=f.gcount();if(n>0&&BCryptHashData(h,reinterpret_cast<PUCHAR>(b.data()),static_cast<ULONG>(n),0)<0)ok=false;}if(ok)ok=BCryptFinishHash(h,d.data(),32,0)>=0;}}
 if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);if(!ok)return false;static constexpr char x[]="0123456789abcdef";std::string s(64,'0');for(std::size_t i=0;i<32;++i){s[2*i]=x[d[i]>>4];s[2*i+1]=x[d[i]&15];}return s==k_sha;
}
void __fastcall parse_entry(void*m,const void*r) noexcept {
 if(m)flver_identity_observe_destroy(m);
 if(m&&r&&range_ok(r,0x18)){
  std::uint32_t o=0,l=0;
  std::memcpy(&o,static_cast<const std::uint8_t*>(r)+0xc,4);
  std::memcpy(&l,static_cast<const std::uint8_t*>(r)+0x10,4);
  const std::uint64_t n=static_cast<std::uint64_t>(o)+l;
  constexpr std::uint64_t k_live_flver_cap=16ull*1024ull*1024ull;
  if(n>=0x40u&&n<=k_live_flver_cap&&n<=SIZE_MAX&&
     range_ok(r,static_cast<std::size_t>(n)))
   (void)flver_identity_observe_parse(m,r,static_cast<std::size_t>(n));
 }
 if(g_po)g_po(m,r);
}
void __fastcall destroy_entry(void*m) noexcept {flver_identity_observe_destroy(m);if(g_do)g_do(m);}
}
extern "C" void dsrrl_flver_selector_observer(void *c, std::int32_t i) noexcept {
 material_owner_selection_clear();
 if(i<0)return;
 actual_material_owner_observation observation{};
 if(!flver_identity_enrich_owner(c,static_cast<std::uint32_t>(i),observation))
  return;
 if(!enrich_exact_owner_mtd_identity(observation))
  return;
 const auto identity=make_actual_material_identity(observation);
 material_owner_selection_publish(identity);
}
bool install() noexcept {if(g_p.patched||g_s.patched||g_d.patched)return false;g_state={};if(!exe_ok())return false;g_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));if(!g_base)return false;g_state.provenance_ok=true;
 if(!prep(g_p,k_parse,k_parse_b,reinterpret_cast<void*>(&parse_entry)))goto fail;g_po=reinterpret_cast<parser_fn>(g_p.trampoline);
 if(!prep(g_d,k_destroy,k_destroy_b,reinterpret_cast<void*>(&destroy_entry)))goto fail;g_do=reinterpret_cast<destructor_fn>(g_d.trampoline);
 if(!prep(g_s,k_selector,k_selector_b,reinterpret_cast<void*>(&dsrrl_flver_selector_hook_entry)))goto fail;g_dsrrl_flver_selector_trampoline=g_s.trampoline;
 if(!arm(g_p)||!arm(g_d)||!arm(g_s))goto fail;g_state.parser_armed=true;g_state.destructor_armed=true;g_state.selector_armed=true;g_state.selector_owner_enrichment=true;return true;
fail:
 uninstall();
 return false;
}
void uninstall() noexcept {
 const bool selector_ok=restore(g_s);
 const bool destructor_ok=restore(g_d);
 const bool parser_ok=restore(g_p);
 if(!(selector_ok&&destructor_ok&&parser_ok)){
  // Preserve failed hook state/trampolines for diagnostics and do not claim a
  // clean teardown. A failed restore is construction/runtime safety evidence,
  // never a reason to free state and pretend stock code was restored.
  g_state.restore_failed=true;
  g_state.selector_armed=g_s.patched;
  g_state.destructor_armed=g_d.patched;
  g_state.parser_armed=g_p.patched;
  return;
 }
 g_dsrrl_flver_selector_trampoline=nullptr;
 g_po=nullptr;
 g_do=nullptr;
 flver_identity_reset();
 g_state={};
}
hook_status status() noexcept{return g_state;}

bool consume_selector_owner_candidate(
    operators::material_response::material_identity &material) noexcept
{
    material={};
    if(!g_selector_owner_valid){
        ++g_owner_consume_misses;
        return false;
    }

    material=g_selector_owner_candidate;
    g_selector_owner_candidate={};
    g_selector_owner_valid=false;
    ++g_owner_consumed;
    return true;
}

selector_owner_telemetry selector_owner_stats() noexcept
{
    return {
        g_selector_events.load(),
        g_owner_sha_hits.load(),
        g_owner_mtd_hits.load(),
        g_exact_owner_ready.load(),
        g_owner_fail_open.load(),
        g_owner_consumed.load(),
        g_owner_consume_misses.load()
    };
}
}
