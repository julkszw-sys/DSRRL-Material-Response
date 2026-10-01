#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/flver_identity_registry.hpp"
#include "dsrrl/runtime/exact_material_cache.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"
#include "dsrrl/runtime/hemdir3_mode_transport.hpp"
#include "dsrrl/runtime/clustered_pnts_draw_runtime.hpp"
#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_exact_binding_mr_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
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
extern "C" void dsrrl_clustered_pnts_builder_hook_entry();
extern "C" {
void *g_dsrrl_flver_selector_trampoline=nullptr;
void *g_dsrrl_flver_builder_trampoline=nullptr;
}

namespace dsrrl::runtime::flver_identity_transport {
namespace {
constexpr char k_sha[]="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr std::uintptr_t k_parse=0x20D910u,k_selector=0x22BA20u,k_destroy=0x20D7A0u,k_mtd=0x295ED0u,k_builder=0x22084Fu;
constexpr std::uintptr_t k_ret_sel_1=0x20E019u,k_ret_sel_2=0x20EB7Fu,k_ret_sel_3=0x20FB9Eu;
constexpr std::array<std::uint8_t,16> k_parse_b={0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
constexpr std::array<std::uint8_t,15> k_selector_b={0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA};
constexpr std::array<std::uint8_t,20> k_destroy_b={0x40,0x57,0x48,0x83,0xEC,0x30,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,0x48,0x89,0x5C,0x24,0x40};
constexpr std::array<std::uint8_t,15> k_mtd_b={0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF};
constexpr std::array<std::uint8_t,17> k_builder_b={
    0x48,0x8D,0x8F,0xD0,0x00,0x00,0x00,
    0x33,0xDB,
    0x0F,0x28,0x41,0x10,
    0x0F,0xC2,0x01,0x01};
struct hook{void *target=nullptr,*trampoline=nullptr,*detour=nullptr;std::size_t stolen=0;std::array<std::uint8_t,32> original{};bool patched=false;};
std::uintptr_t g_base=0; hook g_p{},g_s{},g_d{},g_m{},g_b{}; hook_status g_state{};
using parser_fn=void(__fastcall *)(void *,const void *); using destructor_fn=void(__fastcall *)(void *);
using mtd_fn=void(__fastcall *)(void *,const void *,std::uint32_t,const wchar_t *);
parser_fn g_po=nullptr; destructor_fn g_do=nullptr; mtd_fn g_mo=nullptr;

std::atomic<std::uint64_t> g_selector_events{0};
std::atomic<std::uint64_t> g_owner_sha_hits{0};
std::atomic<std::uint64_t> g_owner_mtd_hits{0};
std::atomic<std::uint64_t> g_exact_owner_ready{0};
std::atomic<std::uint64_t> g_owner_fail_open{0};
std::atomic<std::uint64_t> g_runtime_material_hits{0};
std::atomic<std::uint64_t> g_runtime_material_ready{0};
std::atomic_bool g_runtime_mtd_classified{false};
std::atomic_bool g_runtime_mtd_cache_hit{false};
std::atomic_bool g_runtime_mtd_selection_published{false};
std::atomic<std::uint64_t> g_owner_consumed{0};

std::atomic_bool g_selector_upper_lower_enabled{true};
std::atomic_bool g_selector_hemdir3_enabled{true};

void latch_once(
    std::atomic_bool &flag) noexcept
{
    if (!flag.load(
            std::memory_order_relaxed))
        flag.store(
            true,
            std::memory_order_relaxed);
}
std::atomic<std::uint64_t> g_owner_consume_misses{0};

constexpr std::size_t k_exact_material_cache_sets=1024u;
constexpr std::size_t k_exact_material_cache_ways=4u;

exact_material_cache<
    k_exact_material_cache_sets,
    k_exact_material_cache_ways>
    g_exact_runtime_materials{};

constexpr std::size_t k_exact_runtime_route_count =
    operators::material_response::generated::
        k_material_route_count_v1 +
    operators::material_response::generated::
        k_exact_binding_mr_v1.size();

static_assert(
    k_exact_runtime_route_count < 0xffffu,
    "exact runtime MR route ordinal must fit the cache carrier");

void exact_material_cache_erase(
    const void *material) noexcept
{
    (void)g_exact_runtime_materials.erase(
        material);
}

bool same_mr_behavior(
    const operators::material_response::generated::route_seed &a,
    const operators::material_response::generated::route_seed &b) noexcept
{
 return
     a.route_index==b.route_index &&
     a.c101==b.c101 &&
     a.lod_min==b.lod_min &&
     a.lod_max==b.lod_max &&
     a.receiver0==b.receiver0 &&
     a.receiver1==b.receiver1 &&
     a.receiver2==b.receiver2 &&
     operators::material_response::mtd_semantic_hash(
         a.material_family)==
     operators::material_response::mtd_semantic_hash(
         b.material_family);
}

bool classify_exact_runtime_route(
    const core::sha256_digest &digest,
    std::uint16_t &route_ordinal) noexcept
{
 route_ordinal=0u;
 const operators::material_response::generated::route_seed
     *match=nullptr;
 std::size_t match_index=0u;

 for(std::size_t i=0u;
     i<operators::material_response::generated::
         k_material_route_count_v1;
     ++i){
  const auto &route=
      operators::material_response::generated::
          k_material_routes_v1[i];

  if(!operators::legacy_plan::hashing::matches_hex(
        digest,
        route.sha256))
   continue;

  if(match!=nullptr &&
     !same_mr_behavior(*match,route))
   return false;

  if(match==nullptr){
   match=&route;
   match_index=i;
  }
 }

 if(match!=nullptr){
  if(match_index>=0xffffu)
   return false;
  route_ordinal=
      static_cast<std::uint16_t>(
          match_index);
  return true;
 }

 const operators::material_response::generated::
     exact_binding_mr_record *extension_match=nullptr;
 std::size_t extension_index=0u;

 for(std::size_t i=0u;
     i<operators::material_response::generated::
         k_exact_binding_mr_v1.size();
     ++i){
  const auto &route=
      operators::material_response::generated::
          k_exact_binding_mr_v1[i];

  // Shared base MTDs are intentionally owner/companion gated. The retail
  // parser SHA alone may authorize only DIRECT_EXACT extension records.
  if(!route.runtime_mtd_allowed ||
     route.raw_mtd_sha256!=digest)
   continue;

  if(extension_match!=nullptr)
   return false;

  extension_match=&route;
  extension_index=i;
 }

 if(extension_match==nullptr)
  return false;

 const auto combined_ordinal=
     operators::material_response::generated::
         k_material_route_count_v1+
     extension_index;
 if(combined_ordinal>=0xffffu)
  return false;

 route_ordinal=
     static_cast<std::uint16_t>(
         combined_ordinal);
 return true;
}

void exact_material_cache_publish(
    const void *material,
    std::uint16_t route_ordinal) noexcept
{
    (void)g_exact_runtime_materials.publish(
        material,
        route_ordinal,
        static_cast<std::uint16_t>(
            k_exact_runtime_route_count));
}

bool exact_material_cache_lookup(
    const void *material,
    std::uint16_t &route_ordinal) noexcept
{
    return g_exact_runtime_materials.lookup(
        material,
        static_cast<std::uint16_t>(
            k_exact_runtime_route_count),
        route_ordinal);
}

void clear_exact_material_cache() noexcept
{
    g_exact_runtime_materials.clear();
}

int hex_nibble(char c) noexcept
{
 if(c>='0'&&c<='9')return c-'0';
 if(c>='a'&&c<='f')return c-'a'+10;
 if(c>='A'&&c<='F')return c-'A'+10;
 return -1;
}

bool route_digest(
    const operators::material_response::generated::route_seed &route,
    core::sha256_digest &digest) noexcept
{
 digest={};
 for(std::size_t i=0u;i<digest.size();++i){
  const int hi=hex_nibble(route.sha256[2u*i]);
  const int lo=hex_nibble(route.sha256[2u*i+1u]);
  if(hi<0||lo<0){
   digest={};
   return false;
  }
  digest[i]=
      static_cast<std::uint8_t>(
          (hi<<4)|lo);
 }
 return true;
}

bool range_ok(const void *p,std::size_t n) noexcept {
 if(!p)return false;if(n==0)return true;auto cur=reinterpret_cast<std::uintptr_t>(p);const auto end=cur+n;if(end<cur)return false;
 while(cur<end){MEMORY_BASIC_INFORMATION m{};if(VirtualQuery(reinterpret_cast<const void*>(cur),&m,sizeof(m))!=sizeof(m))return false;
  const DWORD a=m.Protect&0xffu;const bool rd=m.State==MEM_COMMIT&&(m.Protect&PAGE_GUARD)==0u&&(a==PAGE_READONLY||a==PAGE_READWRITE||a==PAGE_WRITECOPY||a==PAGE_EXECUTE_READ||a==PAGE_EXECUTE_READWRITE||a==PAGE_EXECUTE_WRITECOPY);if(!rd)return false;
  const auto re=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;if(re<=cur)return false;cur=std::min(re,end);}return true;
}

void observe_exact_runtime_mtd(
    void *material,
    const void *raw,
    std::uint32_t len,
    const wchar_t *semantic_key) noexcept
{
 (void)semantic_key;
 if(material==nullptr)return;

 // Reused engine material objects are invalidated before classifying the new
 // MTD image. The working 1.45 carrier selected the MR donor from the exact
 // raw-MTD digest; do not add a runtime-name prerequisite that the donor did
 // not require.
 exact_material_cache_erase(material);

 constexpr std::uint32_t k_max_mtd_bytes=8u*1024u*1024u;
 if(raw==nullptr||len==0u||len>k_max_mtd_bytes||
    !range_ok(raw,len))
  return;

 const auto digest=
  operators::legacy_plan::hashing::sha256(
   static_cast<const std::uint8_t*>(raw),
   static_cast<std::size_t>(len));

 std::uint16_t route_ordinal=0u;
 if(!classify_exact_runtime_route(
      digest,
      route_ordinal))
  return;

 latch_once(
     g_runtime_mtd_classified);

 exact_material_cache_publish(
     material,
     route_ordinal);
}

bool lookup_exact_runtime_material(
    const void *material,
    operators::material_response::material_identity &identity) noexcept
{
 identity={};

 std::uint16_t route_ordinal=0u;
 if(!exact_material_cache_lookup(
      material,
      route_ordinal))
  return false;

 latch_once(
     g_runtime_mtd_cache_hit);

 const auto standard_count=
     operators::material_response::generated::
         k_material_route_count_v1;

 identity.valid=true;
 identity.actual_material_exact=true;

 if(route_ordinal<standard_count){
  const auto &route=
      operators::material_response::generated::
          k_material_routes_v1[
              route_ordinal];

  core::sha256_digest digest{};
  if(!route_digest(route,digest))
   return false;

  identity.route_index=route.route_index;
  // SHA collisions in the certified MR table are accepted only when every
  // colliding entry has identical MR behavior. Use the first certified route
  // as the canonical semantic identity for that behaviorally equivalent class.
  identity.semantic_name_hash=
      operators::material_response::
          mtd_semantic_hash(
              route.mtd_name);
  identity.raw_mtd_sha256=digest;
  identity.material_family_hash=
      operators::material_response::
          mtd_semantic_hash(
              route.material_family);
 } else {
  const auto extension_index=
      static_cast<std::size_t>(
          route_ordinal-standard_count);
  if(extension_index>=
         operators::material_response::generated::
             k_exact_binding_mr_v1.size())
   return false;

  const auto &route=
      operators::material_response::generated::
          k_exact_binding_mr_v1[
              extension_index];
  if(!route.runtime_mtd_allowed)
   return false;

  identity.route_index=route.route_tag;
  identity.semantic_name_hash=
      operators::material_response::
          mtd_semantic_hash(
              route.mtd_name);
  identity.raw_mtd_sha256=
      route.raw_mtd_sha256;
  identity.material_family_hash=
      operators::material_response::
          mtd_semantic_hash(
              route.material_family);
 }

 return operators::material_response::
  exact_runtime_material_response_identity(
      identity);
}

void *resolve_actual_material(
    void *container,
    std::int32_t material_index) noexcept
{
 if(container==nullptr||
    material_index<0||
    material_index>0x100000)
  return nullptr;

 // Exact retail selector return-RVA validation has already authenticated this
 // call shape. Match the post-performance LightBank/P_Metal policy: direct
 // fixed-layout reads on the draw-hot path, no VirtualQuery and no mutex.
 void *array=nullptr;
 std::memcpy(
     &array,
     static_cast<const std::uint8_t*>(
         container)+0x10u,
     sizeof(array));
 if(array==nullptr)return nullptr;

 const auto index=
     static_cast<std::size_t>(
         material_index);
 if(index>(SIZE_MAX/24u))
  return nullptr;

 void *material=nullptr;
 std::memcpy(
     &material,
     static_cast<const std::uint8_t*>(
         array)+index*24u,
     sizeof(material));
 return material;
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
void __fastcall mtd_entry(
    void *material,
    const void *raw,
    std::uint32_t len,
    const wchar_t *semantic_key) noexcept
{
 observe_exact_runtime_mtd(material,raw,len,semantic_key);
 if(g_mo)g_mo(material,raw,len,semantic_key);
}
}
extern "C" void dsrrl_clustered_pnts_builder_observer(
    void *draw,
    void *renderer_context) noexcept
{
 clustered_pnts_builder_event_bridge(
     draw,
     renderer_context);
}

extern "C" void dsrrl_flver_selector_observer(
    void *container,
    void *owner,
    void *ret,
    void *r14,
    void *r15,
    std::int32_t material_index,
    std::uint32_t incoming_mode,
    const void *selector_stack) noexcept
{
 telemetry::hot_count(g_selector_events);
 material_owner_selection_clear();
 pmetal_env_source_selector_clear();

 if (g_selector_hemdir3_enabled.load(
         std::memory_order_relaxed))
  hemdir3_mode_transport::selector_begin(
      incoming_mode);

 if (g_selector_upper_lower_enabled.load(
         std::memory_order_relaxed))
  upper_lower_selector_event_bridge(
      owner,
      ret,
      r14,
      r15,
      g_state.builder_armed);
 else if (g_state.builder_armed)
  fixed_pointlight_selector_event_bridge(owner);

 if(g_base==0u || ret==nullptr || material_index<0){telemetry::hot_count(g_owner_fail_open);return;}
 const auto ret_addr=reinterpret_cast<std::uintptr_t>(ret);
 if(ret_addr<g_base){telemetry::hot_count(g_owner_fail_open);return;}
 const auto rva=ret_addr-g_base;
 if(rva!=k_ret_sel_1 && rva!=k_ret_sel_2 && rva!=k_ret_sel_3){
  telemetry::hot_count(g_owner_fail_open);
  return;
 }

 const void *actual_material=
     resolve_actual_material(
         container,
         material_index);

 if(g_state.builder_armed)
  clustered_pnts_selector_event_bridge(
      owner,
      actual_material);

 actual_material_owner_observation observation{};
 if(flver_identity_enrich_owner(
        container,static_cast<std::uint32_t>(material_index),observation)){
  telemetry::hot_count(g_owner_sha_hits);

  if(enrich_exact_owner_mtd_identity(observation)){
   telemetry::hot_count(g_owner_mtd_hits);

   const auto identity=
       make_actual_material_identity(
           observation);
   if(identity.owner_tuple_exact &&
      material_owner_selection_publish(identity)){
    // Complete the P_Metal LightBank semantic join only after this exact
    // selector event has proven the actual FLVER/material owner tuple. The
    // LightBank selector bridge above merely staged the matching source token;
    // non-P_Metal materials must never publish it into P_Metal draw state.
    if (g_selector_upper_lower_enabled.load(
            std::memory_order_relaxed))
     upper_lower_pmetal_material_event_bridge(
         owner,
         identity);

    // Route 345 is only a necessary P_Metal condition; the isolated source
    // runtime still verifies exact semantic/raw-MTD identity before decoding.
    // Avoid entering that decoder for every other exact material selector.
    if (identity.route_index == 345u)
     pmetal_env_source_selector_event(
         owner,
         ret,
         r14,
         r15,
         selector_stack,
         identity);

    telemetry::hot_count(g_exact_owner_ready);
    return;
   }
  }
 }

 // Material Response c100/c101 is MTD-local. If FLVER owner enrichment
 // is unavailable, fall back to the exact runtime material object observed
 // by the retail MTD parser. This carrier is accepted only for certified MR
 // profiles; resource/asset operators still require their own owner gates.
 operators::material_response::material_identity
     runtime_material{};
 if(lookup_exact_runtime_material(
        actual_material,
        runtime_material)){
  telemetry::hot_count(
      g_runtime_material_hits);

  if(material_owner_selection_publish(
        runtime_material)){
   latch_once(
       g_runtime_mtd_selection_published);
   telemetry::hot_count(
       g_runtime_material_ready);
   return;
  }
 }

 telemetry::hot_count(g_owner_fail_open);
}
bool install(
    bool enable_clustered_builder,
    bool enable_upper_lower_selector,
    bool enable_hemdir3_selector) noexcept {
 if(g_p.patched||g_s.patched||g_d.patched||g_m.patched||g_b.patched)return false;
 g_state={};
 g_selector_upper_lower_enabled.store(
     enable_upper_lower_selector,
     std::memory_order_relaxed);
 g_selector_hemdir3_enabled.store(
     enable_hemdir3_selector,
     std::memory_order_relaxed);
 g_runtime_mtd_classified.store(false);
 g_runtime_mtd_cache_hit.store(false);
 g_runtime_mtd_selection_published.store(false);
 if(!exe_ok())return false;
 g_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 if(!g_base)return false;
 g_state.provenance_ok=true;
 if(!prep(g_p,k_parse,k_parse_b,reinterpret_cast<void*>(&parse_entry)))goto fail;
 g_po=reinterpret_cast<parser_fn>(g_p.trampoline);
 if(!prep(g_d,k_destroy,k_destroy_b,reinterpret_cast<void*>(&destroy_entry)))goto fail;
 g_do=reinterpret_cast<destructor_fn>(g_d.trampoline);
 if(!prep(g_m,k_mtd,k_mtd_b,reinterpret_cast<void*>(&mtd_entry)))goto fail;
 g_mo=reinterpret_cast<mtd_fn>(g_m.trampoline);
 if(!prep(g_s,k_selector,k_selector_b,reinterpret_cast<void*>(&dsrrl_flver_selector_hook_entry)))goto fail;
 g_dsrrl_flver_selector_trampoline=g_s.trampoline;
 if(enable_clustered_builder){
  if(!prep(g_b,k_builder,k_builder_b,reinterpret_cast<void*>(&dsrrl_clustered_pnts_builder_hook_entry)))goto fail;
  g_dsrrl_flver_builder_trampoline=g_b.trampoline;
 }
 if(!arm(g_p)||!arm(g_d)||!arm(g_m)||!arm(g_s)||
    (enable_clustered_builder&&!arm(g_b)))goto fail;
 g_state.parser_armed=true;
 g_state.destructor_armed=true;
 g_state.mtd_armed=true;
 g_state.selector_armed=true;
 g_state.builder_armed=enable_clustered_builder;
 g_state.selector_owner_enrichment=true;
 g_state.exact_runtime_material_carrier=true;
 return true;
fail:
 uninstall();
 return false;
}
void uninstall() noexcept {
 const bool builder_ok=restore(g_b);
 const bool selector_ok=restore(g_s);
 const bool mtd_ok=restore(g_m);
 const bool destructor_ok=restore(g_d);
 const bool parser_ok=restore(g_p);
 if(!(builder_ok&&selector_ok&&mtd_ok&&destructor_ok&&parser_ok)){
  // Preserve failed hook state/trampolines for diagnostics and do not claim a
  // clean teardown. A failed restore is construction/runtime safety evidence,
  // never a reason to free state and pretend stock code was restored.
  g_state.restore_failed=true;
  g_state.builder_armed=g_b.patched;
  g_state.selector_armed=g_s.patched;
  g_state.mtd_armed=g_m.patched;
  g_state.destructor_armed=g_d.patched;
  g_state.parser_armed=g_p.patched;
  return;
 }
 g_dsrrl_flver_builder_trampoline=nullptr;
 g_dsrrl_flver_selector_trampoline=nullptr;
 g_po=nullptr;
 g_do=nullptr;
 g_mo=nullptr;
 clear_exact_material_cache();
 material_owner_selection_clear();
 pmetal_env_source_selector_clear();
 flver_identity_reset();
 g_selector_upper_lower_enabled.store(
     true,
     std::memory_order_relaxed);
 g_selector_hemdir3_enabled.store(
     true,
     std::memory_order_relaxed);
 g_state={};
}
hook_status status() noexcept
{
 hook_status out=g_state;
 out.runtime_mtd_classified=
     g_runtime_mtd_classified.load(
         std::memory_order_relaxed);
 out.runtime_mtd_cache_hit=
     g_runtime_mtd_cache_hit.load(
         std::memory_order_relaxed);
 out.runtime_mtd_selection_published=
     g_runtime_mtd_selection_published.load(
         std::memory_order_relaxed);
 out.upper_lower_selector_enabled=
     g_selector_upper_lower_enabled.load(
         std::memory_order_relaxed);
 out.hemdir3_selector_enabled=
     g_selector_hemdir3_enabled.load(
         std::memory_order_relaxed);
 return out;
}

bool consume_selector_owner_candidate(
    operators::material_response::material_identity &material) noexcept
{
    if(!material_owner_selection_consume(material)){
        telemetry::hot_count(g_owner_consume_misses);
        return false;
    }

    telemetry::hot_count(g_owner_consumed);
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
        g_runtime_material_hits.load(),
        g_runtime_material_ready.load(),
        g_owner_consumed.load(),
        g_owner_consume_misses.load()
    };
}
}
