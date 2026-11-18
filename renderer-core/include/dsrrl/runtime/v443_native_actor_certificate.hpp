#pragma once
// V443 read-only diagnostic. No new detours, no source-selection decisions.
// This file MUST be included only by the V439 diagnostic build after v438 trace.
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <intrin.h>

namespace dsrrl::runtime::v443 {
using u64=std::uint64_t;
using uptr=std::uintptr_t;

enum class verdict:unsigned {
 not_target=0, no_stack=1, no_emitter=2, no_dp=3,
 bad_dp_class=4, no_builder=5, actor_disagreement=6,
 packet_disagreement=7, alt_partial=8, default_builder_packet=9
};
struct witness {
 unsigned status=0,emitter_family=0;
 u64 emitter_return_rva=0,builder_return_rva=0;
 u64 entity=0,dp=0,actor_vtable=0,model=0,model_vtable=0;
 u64 packet=0,dp_a=0,dp_b=0,dp_beta=0;
 u64 packet_a=0,packet_b=0,packet_beta=0;
 u64 builder_rdi=0,builder_rbp=0;
};

// SEH is intentionally isolated from code with nontrivial C++ destructors.
__declspec(noinline) inline bool copy_readable(const void *src,void *dst,std::size_t n) noexcept {
 if(!src||!dst||n==0)return false;
 const uptr a=reinterpret_cast<uptr>(src);
 if(a+n<a)return false;
 MEMORY_BASIC_INFORMATION mb{};
 if(VirtualQuery(src,&mb,sizeof(mb))!=sizeof(mb))return false;
 const uptr end=reinterpret_cast<uptr>(mb.BaseAddress)+mb.RegionSize;
 if(end<a||a+n>end||mb.State!=MEM_COMMIT||
    (mb.Protect&(PAGE_GUARD|PAGE_NOACCESS))!=0)return false;
 __try { std::memcpy(dst,src,n); }
 __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
 return true;
}
template<typename T> inline bool read(uptr a,T &out) noexcept {
 return copy_readable(reinterpret_cast<const void*>(a),&out,sizeof(out));
}

__declspec(noinline) inline bool unwind_one(CONTEXT &c,uptr low,uptr high) noexcept {
 if(c.Rsp<low||c.Rsp+16u>high||c.Rsp+16u<c.Rsp||c.Rip==0)return false;
 DWORD64 image=0;
 PRUNTIME_FUNCTION fn=RtlLookupFunctionEntry(c.Rip,&image,nullptr);
 if(!fn){
  u64 ret=0;
  if(!read(static_cast<uptr>(c.Rsp),ret))return false;
  c.Rsp+=8;
  c.Rip=ret;
  return ret!=0;
 }
 PVOID handler_data=nullptr;
 DWORD64 establisher=0;
 __try {
  (void)RtlVirtualUnwind(UNW_FLAG_NHANDLER,image,c.Rip,fn,
       &c,&handler_data,&establisher,nullptr);
 } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
 return c.Rip!=0&&c.Rsp>=low&&c.Rsp<high;
}

inline bool emitter_return(uptr rva,unsigned &family) noexcept {
 switch(rva){
  case 0x1C6A03u: case 0x1C6A9Cu: case 0x1C6BCAu: family=5u;return true;
  case 0x1C6D5Au: case 0x1C6DF3u: case 0x1C6F4Fu: family=6u;return true;
  default:return false;
 }
}

// RtlCaptureContext is called *inside* the existing native packer hook. The
// original game's CALL returns to one of six exact emitter sites; the detour
// is a JMP, so this original return address remains on the native stack.
__declspec(noinline) inline witness probe(uptr exe_base) noexcept {
 witness w{};
 w.status=static_cast<unsigned>(verdict::no_stack);
 ULONG_PTR low0=0,high0=0;
 GetCurrentThreadStackLimits(&low0,&high0);
 const uptr low=static_cast<uptr>(low0),high=static_cast<uptr>(high0);
 if(!exe_base||low>=high)return w;
 CONTEXT c{};
 RtlCaptureContext(&c);
 bool emitter=false;
 for(unsigned i=0;i<12u;++i){
  if(c.Rip>=exe_base){
   const uptr rva=static_cast<uptr>(c.Rip-exe_base);
   unsigned family=0;
   if(emitter_return(rva,family)){
    w.emitter_family=family;w.emitter_return_rva=rva;
    w.dp=family==5u?c.R14:c.R15;
    emitter=true;
    break;
   }
  }
  if(!unwind_one(c,low,high))return w;
 }
 if(!emitter){w.status=static_cast<unsigned>(verdict::no_emitter);return w;}
 if(w.dp<0x108u){w.status=static_cast<unsigned>(verdict::no_dp);return w;}
 w.entity=w.dp-0x108u;
 u64 dp_vtable=0;
 if(!read(static_cast<uptr>(w.dp),dp_vtable)||dp_vtable!=exe_base+0x12E5C08u||
    !read(static_cast<uptr>(w.entity),w.actor_vtable)||
    (w.actor_vtable!=exe_base+0x12ECA10u&&w.actor_vtable!=exe_base+0x12EBFE8u)){
  w.status=static_cast<unsigned>(verdict::bad_dp_class);return w;
 }
 std::uint16_t a=0,b=0;std::uint32_t beta=0;
 if(!read(static_cast<uptr>(w.dp+8u),a)||
    !read(static_cast<uptr>(w.dp+10u),b)||
    !read(static_cast<uptr>(w.dp+12u),beta)||
    !read(static_cast<uptr>(w.entity+0x40u),w.model)){
  w.status=static_cast<unsigned>(verdict::no_dp);return w;
 }
 w.dp_a=a;w.dp_b=b;w.dp_beta=beta;
 (void)read(static_cast<uptr>(w.model),w.model_vtable);
 bool builder=false;
 for(unsigned i=0;i<12u;++i){
  if(!unwind_one(c,low,high))break;
  if(c.Rip<exe_base)continue;
  const uptr rva=static_cast<uptr>(c.Rip-exe_base);
  if(rva==0x220637u||rva==0x22063Eu||rva==0x2167BFu){
   w.builder_return_rva=rva;w.builder_rdi=c.Rdi;w.builder_rbp=c.Rbp;
   builder=true;break;
  }
 }
 if(!builder){w.status=static_cast<unsigned>(verdict::no_builder);return w;}
 if(w.builder_rdi!=w.entity){
  w.status=static_cast<unsigned>(verdict::actor_disagreement);return w;
 }
 if(w.builder_return_rva==0x2167BFu){
  w.status=static_cast<unsigned>(verdict::alt_partial);return w;
 }
 // Default builder uses exactly one native packet [rbp-0x30]. Compare its
 // pre-emission A/B/beta with the embedded DrawParam. Never alter a mismatch.
 if(w.builder_rbp<low+0x30u||w.builder_rbp>=high){
  w.status=static_cast<unsigned>(verdict::no_builder);return w;
 }
 w.packet=w.builder_rbp-0x30u;
 std::uint16_t pa=0,pb=0;std::uint32_t pbet=0;
 if(!read(static_cast<uptr>(w.packet+0x4Cu),pa)||
    !read(static_cast<uptr>(w.packet+0x4Eu),pb)||
    !read(static_cast<uptr>(w.packet+0x50u),pbet)){
  w.status=static_cast<unsigned>(verdict::no_builder);return w;
 }
 w.packet_a=pa;w.packet_b=pb;w.packet_beta=pbet;
 if(pa!=a||pb!=b||pbet!=beta){
  w.status=static_cast<unsigned>(verdict::packet_disagreement);return w;
 }
 w.status=static_cast<unsigned>(verdict::default_builder_packet);
 return w;
}
}
