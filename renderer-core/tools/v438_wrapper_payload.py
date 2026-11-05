"""V438 exact native wrapper diagnostic (read-only, typed runtime model identity)."""
WRAPPER_SOURCE=r'''
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
struct v438_wrapper_hook {
 void *target=nullptr,*trampoline=nullptr;
 std::array<std::uint8_t,17> original{};
 bool armed=false;
};
using v438_wrapper_fn=void(__fastcall *)(void*,void*,void*,float);
v438_wrapper_hook v438_type5{},v438_type6{};
v438_wrapper_fn v438_original5=nullptr,v438_original6=nullptr;
constexpr std::array<std::uint8_t,17> v438_preimage{{
 0x48,0x83,0xec,0x38,0x4d,0x8b,0xc8,0xf3,0x0f,0x11,0x5c,0x24,0x20,
 0x4c,0x8b,0x41,0x40}};
static void v438_wrapper_event(bool type6,void *original_caller,
                               void *wrapper,void *out,
                               void *drawparam,float blend) noexcept {
 auto old=v438::current;
 v438::invocation observed{};
 const auto address=reinterpret_cast<std::uintptr_t>(drawparam);
 const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 const auto returned=reinterpret_cast<std::uintptr_t>(original_caller);
 observed.caller=returned>=base ? returned-base : 0u;
 observed.drawparam=address;
 const bool known_caller=observed.caller==0x220637u ||
     observed.caller==0x22063eu || observed.caller==0x2167bfu;
 if(known_caller && address>=0x108u) {
  const auto entity=address-0x108u;
  std::uintptr_t vt=0,model=0;
  std::uint16_t a=0,b=0;float beta=0;
  const bool valid=
   safe_read(reinterpret_cast<void*>(entity),vt) &&
   safe_read(reinterpret_cast<void*>(entity+0x40u),model) &&
   safe_read(reinterpret_cast<void*>(address+0x08u),a) &&
   safe_read(reinterpret_cast<void*>(address+0x0au),b) &&
   safe_read(reinterpret_cast<void*>(address+0x0cu),beta);
  if(valid && vt==base+0x12eca10u) {
   observed.entity=entity;observed.model=model;
   observed.rawab=static_cast<std::uint32_t>(a) |
       (static_cast<std::uint32_t>(b)<<16);
   observed.beta=v438::fbits(beta);
   observed.id=v438::invocation_issuer.fetch_add(1u)+1u;
  }
 }
 v438::current=observed;
 v438::emit(v438::WRAPPER_BEGIN,{observed.id,observed.entity,
     observed.model,address,type6?6ull:5ull,observed.rawab,
     observed.beta,v438::p(wrapper),v438::p(out),v438::fbits(blend),
     observed.caller});
 auto original=type6?v438_original6:v438_original5;
 if(original)original(wrapper,out,drawparam,blend);
 v438::emit(v438::WRAPPER_END,{observed.id,observed.entity,
     observed.model,address,type6?6ull:5ull});
 v438::last=observed;
 v438::current=old;
}
void __fastcall v438_wrapper5(void *wrapper,void *out,void *dp,float beta) noexcept {
 if(!v438::state().active.load(std::memory_order_relaxed)) {
     if(v438_original5)v438_original5(wrapper,out,dp,beta);return;
 }
 v438_wrapper_event(false,_ReturnAddress(),wrapper,out,dp,beta);
}
void __fastcall v438_wrapper6(void *wrapper,void *out,void *dp,float beta) noexcept {
 if(!v438::state().active.load(std::memory_order_relaxed)) {
     if(v438_original6)v438_original6(wrapper,out,dp,beta);return;
 }
 v438_wrapper_event(true,_ReturnAddress(),wrapper,out,dp,beta);
}
static bool v438_prepare(v438_wrapper_hook &h,std::uintptr_t base,
                          std::uintptr_t rva,void *entry) noexcept {
 auto *target=reinterpret_cast<std::uint8_t*>(base+rva);
 if(!readable_range(target,v438_preimage.size()) ||
    std::memcmp(target,v438_preimage.data(),v438_preimage.size())!=0)
    return false;
 auto *tr=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,31,
     MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
 if(!tr)return false;
 std::memcpy(tr,target,17);
 tr[17]=0xff;tr[18]=0x25;std::uint32_t zero=0;
 std::memcpy(tr+19,&zero,4);
 const std::uint64_t back=base+rva+17;
 std::memcpy(tr+23,&back,8);
 if(!FlushInstructionCache(GetCurrentProcess(),tr,31)){
    VirtualFree(tr,0,MEM_RELEASE);return false;}
 h.target=target;h.trampoline=tr;h.original=v438_preimage;
 (void)entry;return true;
}
static bool v438_arm(v438_wrapper_hook &h,void *entry) noexcept {
 if(!h.target||!h.trampoline||!entry)return false;
 std::array<std::uint8_t,17> jump{};jump.fill(0x90);
 jump[0]=0xff;jump[1]=0x25;
 const std::uint64_t destination=reinterpret_cast<std::uintptr_t>(entry);
 std::memcpy(jump.data()+6,&destination,8);
 if(!write_source_hook_code(h.target,jump.data(),jump.size()))return false;
 h.armed=true;return true;
}
static bool v438_restore(v438_wrapper_hook &h) noexcept {
 if(!h.armed)return true;
 if(!write_source_hook_code(h.target,h.original.data(),17))return false;
 h.armed=false;return true;
}
static bool v438_install_wrappers(std::uintptr_t base) noexcept {
 if(!v438_prepare(v438_type5,base,0x1c0be0u,(void*)&v438_wrapper5)||
    !v438_prepare(v438_type6,base,0x1c0c10u,(void*)&v438_wrapper6))
    return false;
 v438_original5=reinterpret_cast<v438_wrapper_fn>(v438_type5.trampoline);
 v438_original6=reinterpret_cast<v438_wrapper_fn>(v438_type6.trampoline);
 if(!v438_arm(v438_type5,(void*)&v438_wrapper5)||
    !v438_arm(v438_type6,(void*)&v438_wrapper6)){
    (void)v438_restore(v438_type6);(void)v438_restore(v438_type5);
    return false;
 }
 reshade::log::message(reshade::log::level::warning,
  "[DSRRL V438] WRAPPER_HOOKS_ACTIVE type5=1 type6=1 exact_DSR_preimages=1");
 return true;
}
static bool v438_uninstall_wrappers() noexcept {
 const bool good6=v438_restore(v438_type6);
 const bool good5=v438_restore(v438_type5);
 if(!good6||!good5)return false;
 if(v438_type5.trampoline)VirtualFree(v438_type5.trampoline,0,MEM_RELEASE);
 if(v438_type6.trampoline)VirtualFree(v438_type6.trampoline,0,MEM_RELEASE);
 v438_type5={};v438_type6={};v438_original5=nullptr;v438_original6=nullptr;
 return true;
}
#endif // DSRRL_V438_ONE_SHOT_CAPTURE
'''
