#!/usr/bin/env python3
"""V465 one addon: typed ChrIns+0x60 → ChrModel+0x10 and issued RX34 entity+0x10.
The only new runtime hook is SHA-guarded ChrIns vtable +0x120 method at RVA 0x321CE0.
V459 Light pair, V451 model resource, Source1, Source2, R43, renderer state unchanged.
All new events are read-only. No GL/D3D shader, CB or EXE disk mutations.
"""
from pathlib import Path
R=Path("renderer-core")
f=R/"src/runtime/flver_engine_hooks.cpp"
i=R/"integrated/integrated_addon.cpp"
t=R/"include/dsrrl/runtime/v438_one_shot_trace.hpp"
s=R/"src/runtime/pmetal_env_source_runtime.cpp"

def rep(path,old,new):
    src=path.read_text(encoding="utf-8")
    assert src.count(old)==1,(str(path),src.count(old),old[:100])
    path.write_text(src.replace(old,new,1),encoding="utf-8")

rep(t,"V451_MODEL_RESOURCE_AT_ISSUE=21",
      "V451_MODEL_RESOURCE_AT_ISSUE=21,V465_CHRINS_OWNER=22,V465_RX34_HANDLE=23")

rep(f,'#include <reshade.hpp>',
      '#include <reshade.hpp>\n#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)\n'
      '#include "dsrrl/runtime/v438_one_shot_trace.hpp"\n#endif')

rep(f,'hook_status g_state{};',
'''hook_status g_state{};
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
// Original EXE SHA is checked by flver_identity_transport::install() before arming.
// RVA 321CE0: mov [rsp+18],rbx; push rdi; sub rsp,C0 = exactly 13 relocation-free bytes.
// The following instruction is RIP-relative and MUST NOT be stolen.
constexpr std::uintptr_t k_v465_chrins_rva=0x321CE0u;
constexpr std::array<std::uint8_t,13> k_v465_chrins_preimage={
    0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x81,0xec,0xc0,0x00,0x00,0x00};
hook g_v465_chrins{};
using v465_chrins_fn=std::uintptr_t (__fastcall *)(void *,void *);
v465_chrins_fn g_v465_chrins_original=nullptr;
#endif''')

rep(f,'std::wstring path(){',
'''#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
// The host's shared prep() requires 14+ bytes. Here exactly 13 complete,
// relocation-free instructions precede a RIP-relative stack-cookie load.
// A 12-byte mov rax,imm64 / jmp rax + NOP preserves this exact boundary.
// RAX is volatile under MS x64 ABI; original first 13 bytes do not read RAX.
// Fail-open when exact original bytes or executable provenance mismatch.
bool v465_prep_chrins() noexcept {
    const auto *source=reinterpret_cast<const std::uint8_t *>(
        g_base+k_v465_chrins_rva);
    if (!g_base || !range_ok(source,k_v465_chrins_preimage.size()) ||
        std::memcmp(source,k_v465_chrins_preimage.data(),
                    k_v465_chrins_preimage.size())!=0)
        return false;
    auto *tr=static_cast<std::uint8_t *>(VirtualAlloc(
        nullptr,13u+14u,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!tr)return false;
    std::memcpy(tr,source,13u);
    tr[13u]=0xffu;tr[14u]=0x25u;
    std::uint32_t zero=0u;std::memcpy(tr+15u,&zero,4u);
    const auto back=static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(source+13u));
    std::memcpy(tr+19u,&back,8u);
    if(FlushInstructionCache(GetCurrentProcess(),tr,27u)==FALSE){
        VirtualFree(tr,0u,MEM_RELEASE);return false;
    }
    g_v465_chrins.target=const_cast<std::uint8_t *>(source);
    g_v465_chrins.trampoline=tr;
    g_v465_chrins.detour=nullptr;
    g_v465_chrins.stolen=13u;
    std::copy(k_v465_chrins_preimage.begin(),
              k_v465_chrins_preimage.end(),
              g_v465_chrins.original.begin());
    return true;
}
std::uintptr_t __fastcall v465_chrins_entry(void *owner,void *out) noexcept {
    // The exact vtable+0x120 method uses RCX owner and RDX output pointer;
    // preserve original return RAX and both arguments with no stack rewriting.
    if(owner!=nullptr &&
       dsrrl::runtime::v438::state().active.load(std::memory_order_acquire)) {
        const auto addr=reinterpret_cast<std::uintptr_t>(owner);
        std::uintptr_t owner_vt=0u,model=0u,model_vt=0u,other=0u;
        std::uint32_t handle=0u;
        std::uint8_t light_selector=0xffu;
        std::uint32_t type=0u,flags=0u;
        if(range_ok(owner,0x68u)) {
            std::memcpy(&owner_vt,owner,sizeof(owner_vt));
            std::memcpy(&model,reinterpret_cast<const void *>(addr+0x60u),
                        sizeof(model));
            if(owner_vt==g_base+0x13251F0u)type=1u;       // PlayerIns
            else if(owner_vt==g_base+0x1322E68u)type=2u;  // EnemyIns
            else if(owner_vt==g_base+0x131E008u)type=3u;  // ChrIns base
            if(type)flags|=1u;
            if(type && range_ok(reinterpret_cast<const void *>(addr+0x68u),
                                sizeof(other))) {
                std::memcpy(&other,reinterpret_cast<const void *>(addr+0x68u),
                            sizeof(other));
            }
            if(type && model &&
               range_ok(reinterpret_cast<const void *>(model),0x98u)) {
                flags|=2u;
                std::memcpy(&model_vt,
                            reinterpret_cast<const void *>(model),8u);
                if(model_vt==g_base+0x12EAF30u){
                    flags|=4u;
                    std::memcpy(&handle,
                                reinterpret_cast<const void *>(model+0x10u),4u);
                    std::memcpy(&light_selector,
                                reinterpret_cast<const void *>(model+0x93u),1u);
                    flags|=8u;
                    if(handle&0x80000000u)flags|=16u;
                }
            }
        }
        // Same F9 8s session and ring; do not infer ownership from shared FLVER.
        dsrrl::runtime::v438::emit(
            dsrrl::runtime::v438::V465_CHRINS_OWNER,{
            dsrrl::runtime::v438::p(owner),owner_vt,model,model_vt,handle,
            static_cast<std::uint64_t>(light_selector),type,flags,
            k_v465_chrins_rva,dsrrl::runtime::v438::p(out),other});
    }
    return g_v465_chrins_original?
           g_v465_chrins_original(owner,out):0u;
}
bool v465_arm_chrins() noexcept {
    if(!g_v465_chrins.target || !g_v465_chrins.trampoline ||
       g_v465_chrins.stolen!=13u)return false;
    std::array<std::uint8_t,13u> bytes{};
    bytes[0]=0x48u;bytes[1]=0xb8u;
    const auto detour=static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(&v465_chrins_entry));
    std::memcpy(bytes.data()+2u,&detour,8u);
    bytes[10]=0xffu;bytes[11]=0xe0u;bytes[12]=0x90u;
    // Existing write() restores page protection and flushes instruction cache.
    // Only mark hooked after successful write; verify bytes before PASS.
    // If the write changes bytes but flushing fails, restore() must still
    // know that it owns a potentially modified target.
    g_v465_chrins.patched=true;
    if(!write(g_v465_chrins.target,bytes.data(),bytes.size()))return false;
    if(std::memcmp(g_v465_chrins.target,bytes.data(),bytes.size())!=0)return false;
    return true;
}
#endif
std::wstring path(){''')

rep(f,'''#endif
 return true;
fail:
 uninstall();
 return false;
}''',
'''#endif
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
 // Diagnostic only; failure MUST NOT stop otherwise working renderer hooks.
 if(v465_prep_chrins()){
   g_v465_chrins_original=reinterpret_cast<v465_chrins_fn>(
       g_v465_chrins.trampoline);
   if(v465_arm_chrins()){
     reshade::log::message(reshade::log::level::warning,
       "[DSRRL V465] CHRINS_OWNER_HOOK_ACTIVE rva=321CE0 exact_exe=1 patch=OFF pixel=OPEN");
   }else{
     // Restoration failure is not concealed as success.
     const bool restored=restore(g_v465_chrins);
     // Never clear the only valid trampoline when restoring live EXE code
     // fails: the existing detour may still be executing.
     if(restored)g_v465_chrins_original=nullptr;
     reshade::log::message(restored?reshade::log::level::warning:
                                   reshade::log::level::error,
       restored?"[DSRRL V465] CHRINS_OWNER_HOOK_FAILOPEN ARM_FAILED":
                "[DSRRL V465] CHRINS_OWNER_HOOK_RESTORE_FAILED");
   }
 }else{
   reshade::log::message(reshade::log::level::warning,
       "[DSRRL V465] CHRINS_OWNER_HOOK_FAILOPEN PREIMAGE_MISMATCH");
 }
#endif
 return true;
fail:
 uninstall();
 return false;
}''')
rep(f,'''void uninstall() noexcept {
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)''',
'''void uninstall() noexcept {
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
 if(!restore(g_v465_chrins)){
   g_state.restore_failed=true;
   return;
 }
 g_v465_chrins_original=nullptr;
#endif
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)''')

p=i.read_text(encoding="utf-8")
needle='''            entity_vt459});'''
assert p.count(needle)==1,("V459 inline-event drift",p.count(needle))
extra='''            entity_vt459});
        // V465 exact native receiver instance handle. This is not a FLVER
        // or a kind16.v00 counter. The only required new identity cut is
        // entity+0x10; preserve existing V459 Light pair unchanged.
        std::uint32_t handle465=0u,status465=0u;
        std::uintptr_t vt465=0u;
        if(valid_builder451 && base459 && entity459 &&
           dsrrl::runtime::v443::read(entity459,vt465) &&
           vt465==base459+0x12ECA10u) {
            status465|=1u;
            if(dsrrl::runtime::v443::read(entity459+0x10u,handle465)) {
                status465|=2u;
                if(handle465&0x80000000u)status465|=4u;
            }
        }
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V465_RX34_HANDLE,{
            rx447.native_sequence,issued448.w.entity,handle465,
            rx447.row_a,static_cast<unsigned>(prepared.source_kind),status465,
            issued448.w.dp,issued448.w.engine_model,
            material.material_slot,material.route_index,vt465,
            static_cast<unsigned>(issued448.status),
            rx447.context,issued448.w.packet,
            issued448.w.frame_rbp});'''
i.write_text(p.replace(needle,extra,1),encoding="utf-8")
rep(s,'''        "[DSRRL V459] RX34_TYPED_LIGHT_PAIR_INLINE_V451_EVENT F9_8s MAX1024 patch=OFF owner=OPEN pixel=OPEN");''',
'''        "[DSRRL V459] RX34_TYPED_LIGHT_PAIR_INLINE_V451_EVENT F9_8s MAX1024 patch=OFF owner=OPEN pixel=OPEN");
    reshade::log::message(reshade::log::level::warning,
        "[DSRRL V465] CHRINS_OWNER_FULL_HANDLE_RX34 F9_8s read_only=1 exact_type=1 unknown_failopen=1 pixel=OPEN");''')
print("V465 integrated overlay PASS: one new guarded ChrIns hook, typed owner kind22 and issued exact receiver kind23; no lighting patch")
