#pragma once
// V464: diagnostic ONLY. Original EXE bytes are not changed on disk.
// Fails open; no render/LightBank/Source1/Source2 decisions use this cache.
// Exact DSR retail RVA 0x32D410 is the shared ChrIns descriptor router.
// Exact PlayerIns/EnemyIns vtables; talkable/hostile meaning is NOT implied.
// No lifetime certificate, no actor-based patch. Runtime/Pixels OPEN.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "dsrrl/runtime/v443_native_actor_certificate.hpp"

namespace dsrrl::runtime::v464 {
using uptr = std::uintptr_t;
struct actor_stamp {
 uptr actor=0,actor_vt=0,model=0,model_vt=0;
 std::uint32_t handle=0;
 unsigned cls=0; // 1 PlayerIns; 2 EnemyIns; NEVER "friendly"/"hostile"
 bool valid=false;
};
struct owner_match {
 unsigned status=0; // 0 untyped,1 unknown,2 PlayerIns,3 EnemyIns,4 ambiguous,5 hook_off,6 invalid_handle
 unsigned candidates=0;
 uptr owner=0,actor_vt=0,model=0,model_vt=0;
 std::uint32_t model_handle=0,entity_handle=0;
};
inline std::mutex g_actor_mutex;
inline std::array<actor_stamp,384> g_actor_entries{};
inline std::size_t g_actor_next=0;
inline std::atomic<bool> g_hook_installed{false};
inline std::atomic<std::uint64_t> g_router_hits{0},g_actor_records{0},g_actor_rejects{0};
inline uptr g_base=0;
inline void *g_target=nullptr,*g_trampoline=nullptr;
using router_fn=void(__fastcall *)(void *);
inline router_fn g_router_original=nullptr;
inline constexpr std::array<std::uint8_t,15> k_router_start{{
 0x48,0x8B,0xC4,0x55,0x57,0x41,0x54,0x41,0x56,0x41,0x57,
 0x48,0x8D,0x68,0xA1
}};
template<std::size_t N>
inline bool exact(uptr a,const std::array<std::uint8_t,N> &bytes) noexcept {
 std::array<std::uint8_t,N> actual{};
 return dsrrl::runtime::v443::copy_readable(reinterpret_cast<const void*>(a),
         actual.data(),N) && actual==bytes;
}
inline bool image_contract(uptr base) noexcept {
 return base && exact(base+0x32D410u,k_router_start) &&
   exact(base+0x34A17Fu,std::array<std::uint8_t,4>{{0x49,0x89,0x4D,0x60}}) &&
   exact(base+0x21C559u,std::array<std::uint8_t,3>{{0x89,0x43,0x10}}) &&
   exact(base+0x11B891Eu,std::array<std::uint8_t,4>{{0x44,0x89,0x41,0x10}}) &&
   exact(base+0x20B152u,std::array<std::uint8_t,5>{{0xE8,0x59,0x12,0x01,0x00}});
}
inline bool classify_live(uptr owner,actor_stamp &out) noexcept {
 if (!g_base || !owner)return false;
 uptr vt=0,model=0,model_vt=0;std::uint32_t handle=0;
 if(!v443::read(owner,vt))return false;
 const unsigned cls=vt==g_base+0x13251F0u?1u:
                    vt==g_base+0x1322E68u?2u:0u;
 if(cls==0u || !v443::read(owner+0x60u,model) || !model ||
    !v443::read(model,model_vt) || model_vt!=g_base+0x12EAF30u ||
    !v443::read(model+0x10u,handle) || (handle&0x80000000u)==0u)
    return false;
 out={owner,vt,model,model_vt,handle,cls,true};return true;
}
inline void record_owner(uptr owner) noexcept {
 actor_stamp stamp{};
 if(!classify_live(owner,stamp)){
   g_actor_rejects.fetch_add(1,std::memory_order_relaxed);return;
 }
 std::unique_lock<std::mutex> lock(g_actor_mutex,std::try_to_lock);
 if(!lock.owns_lock())return;
 for(auto &e:g_actor_entries){
   if(e.valid && e.actor==owner){
     e=stamp;g_actor_records.fetch_add(1,std::memory_order_relaxed);return;
   }
 }
 g_actor_entries[g_actor_next%g_actor_entries.size()]=stamp;
 ++g_actor_next;g_actor_records.fetch_add(1,std::memory_order_relaxed);
}
inline owner_match resolve(uptr entity,bool typed) noexcept {
 owner_match out{};
 if(!typed)return out;
 if(!g_hook_installed.load(std::memory_order_acquire)){out.status=5u;return out;}
 if(!entity || !v443::read(entity+0x10u,out.entity_handle) ||
    (out.entity_handle&0x80000000u)==0u){out.status=6u;return out;}
 out.status=1u;
 // Strict full handle first; low24 is NEVER actor identity.
 std::array<actor_stamp,12> candidates{};
 unsigned count=0;
 {
  std::unique_lock<std::mutex> lock(g_actor_mutex,std::try_to_lock);
  if(!lock.owns_lock())return out;
  for(const auto &s:g_actor_entries){
   if(!s.valid||s.handle!=out.entity_handle)continue;
   if(count>=candidates.size()){out.status=4u;return out;}
   candidates[count++]=s;
  }
 }
 for(unsigned i=0;i<count;++i){
  const auto &s=candidates[i];actor_stamp now{};
  if(!classify_live(s.actor,now) ||
     now.actor_vt!=s.actor_vt || now.model!=s.model ||
     now.model_vt!=s.model_vt || now.handle!=s.handle)
   continue;
  ++out.candidates;
  if(out.candidates==1u){
   out.owner=now.actor;out.actor_vt=now.actor_vt;
   out.model=now.model;out.model_vt=now.model_vt;
   out.model_handle=now.handle;
   out.status=now.cls==1u?2u:3u;
  } else {
   out.status=4u;
   out.owner=0;out.actor_vt=0;out.model=0;out.model_vt=0;
   out.model_handle=0;
  }
 }
 return out;
}
inline std::mutex g_emit_mutex;
struct emitted_stamp {uptr entity=0;std::uint32_t handle=0;unsigned status=0;bool valid=false;};
inline std::array<emitted_stamp,192> g_emitted{};
inline std::size_t g_emit_next=0;
inline bool first_or_transition(uptr entity,std::uint32_t handle,unsigned status) noexcept {
 std::unique_lock<std::mutex> lock(g_emit_mutex,std::try_to_lock);
 if(!lock.owns_lock())return false;
 for(auto &e:g_emitted){
  if(!e.valid||e.entity!=entity||e.handle!=handle)continue;
  if(e.status==status)return false;
  e.status=status;return true;
 }
 g_emitted[g_emit_next%g_emitted.size()]={entity,handle,status,true};
 ++g_emit_next;return true;
}
inline bool write_bytes(void *target,const void *src,std::size_t n) noexcept {
 DWORD old=0,ignored=0;
 if(!target||!src||!n||!VirtualProtect(target,n,PAGE_EXECUTE_READWRITE,&old))
   return false;
 std::memcpy(target,src,n);
 bool ok=FlushInstructionCache(GetCurrentProcess(),target,n)!=FALSE;
 if(!VirtualProtect(target,n,old,&ignored))ok=false;
 return ok;
}
inline void __fastcall observed_router(void *actor) noexcept {
 // Preserve the game's original actor update and its pointer identity.
 if(g_router_original)g_router_original(actor);
 g_router_hits.fetch_add(1,std::memory_order_relaxed);
 record_owner(reinterpret_cast<uptr>(actor));
}
inline bool install(uptr base) noexcept {
 if(g_hook_installed.load()||g_target||!image_contract(base))return false;
 auto *target=reinterpret_cast<std::uint8_t*>(base+0x32D410u);
 auto *tramp=static_cast<std::uint8_t*>(
    VirtualAlloc(nullptr,15u+14u,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
 if(!tramp)return false;
 std::memcpy(tramp,target,15u);
 auto *tail=tramp+15u;tail[0]=0xFFu;tail[1]=0x25u;
 const std::uint32_t zero=0u;
 const std::uint64_t ret=static_cast<std::uint64_t>(base+0x32D41Fu);
 std::memcpy(tail+2,&zero,4);std::memcpy(tail+6,&ret,8);
 if(!FlushInstructionCache(GetCurrentProcess(),tramp,29u)){
  VirtualFree(tramp,0,MEM_RELEASE);return false;
 }
 std::array<std::uint8_t,15> patch{};
 patch.fill(0x90u);patch[0]=0xFFu;patch[1]=0x25u;
 const auto dest=static_cast<std::uint64_t>(
    reinterpret_cast<uptr>(&observed_router));
 std::memcpy(patch.data()+6u,&dest,8u);
 g_base=base;g_target=target;g_trampoline=tramp;
 g_router_original=reinterpret_cast<router_fn>(tramp);
 if(!write_bytes(target,patch.data(),patch.size())){
  (void)write_bytes(target,k_router_start.data(),k_router_start.size());
  g_router_original=nullptr;g_target=nullptr;g_trampoline=nullptr;g_base=0;
  VirtualFree(tramp,0,MEM_RELEASE);return false;
 }
 g_hook_installed.store(true,std::memory_order_release);return true;
}
inline bool uninstall() noexcept {
 if(!g_target)return true;
 g_hook_installed.store(false,std::memory_order_release);
 if(!write_bytes(g_target,k_router_start.data(),k_router_start.size()))
    return false; // keep trampoline if uninstall failed; never free active code
 if(g_trampoline)VirtualFree(g_trampoline,0,MEM_RELEASE);
 g_router_original=nullptr;g_trampoline=nullptr;g_target=nullptr;g_base=0;
 return true;
}
} // namespace dsrrl::runtime::v464
