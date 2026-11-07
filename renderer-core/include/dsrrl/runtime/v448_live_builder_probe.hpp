#pragma once
// V448 observed DSR native builder stack witness (read-only). Exact retail
// EXE/ABI and SHA must be verified by deployment before trusting any status.
#include "dsrrl/runtime/v448_live_builder_core.hpp"
#include "dsrrl/runtime/v443_native_actor_certificate.hpp"
#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime::v448 {
enum class probe_status : unsigned {
    no_stack=0, no_model_builder=1, wrong_entity_type=2,
    wrong_drawparam_type=3, wrong_model_renderer=4,
    packet_unreadable=5, packet_disagrees=6, live_default_model=7,
    alternate_builder_unproven=8
};
struct live_probe_result {builder_witness w{};probe_status status=probe_status::no_stack;};
inline live_probe_result probe_native_builder(std::uintptr_t exe_base) noexcept {
    live_probe_result out{};
    if(!exe_base)return out;
    ULONG_PTR low0=0, high0=0;
    GetCurrentThreadStackLimits(&low0,&high0);
    const auto low=static_cast<std::uintptr_t>(low0);
    const auto high=static_cast<std::uintptr_t>(high0);
    if(low>=high)return out;
    CONTEXT c{};
    RtlCaptureContext(&c);
    // Unlike V443 PACKER probe, RX34 and FLVER selector are AFTER the native
    // emitter has returned. Look for the ORIGINAL virtual-model caller frame.
    for(unsigned depth=0;depth<48u;++depth){
        if(c.Rip>=exe_base){
            const auto rva=static_cast<std::uintptr_t>(c.Rip-exe_base);
            if(rva==0x2172ADu){out.status=probe_status::alternate_builder_unproven;return out;}
            if(rva==0x220CF0u){
                out.status=probe_status::no_model_builder;
                if(c.Rbp<low+0x30u||c.Rbp>=high||c.Rdi<0x108u)return out;
                builder_witness w{};
                w.entity=static_cast<std::uintptr_t>(c.Rdi);
                w.frame_rbp=static_cast<std::uintptr_t>(c.Rbp);
                w.dp=w.entity+0x108u;
                w.packet=w.frame_rbp-0x30u;
                w.builder_family=1u;
                std::uintptr_t evtable=0u,dpvtable=0u,mvtable=0u,method=0u;
                if(!v443::read(w.entity,evtable)||evtable!=exe_base+0x12ECA10u){
                    out.status=probe_status::wrong_entity_type;return out;
                }
                if(!v443::read(w.dp,dpvtable)||dpvtable!=exe_base+0x12E5C08u){
                    out.status=probe_status::wrong_drawparam_type;return out;
                }
                if(!v443::read(w.entity+0x40u,w.engine_model)||!w.engine_model||
                   !v443::read(w.engine_model,mvtable)||
                   (mvtable!=exe_base+0x12EB130u && mvtable!=exe_base+0x12EB760u)||
                   !v443::read(mvtable+0x18u,method)||method!=exe_base+0x20DDA0u){
                    out.status=probe_status::wrong_model_renderer;return out;
                }
                if(!v443::read(w.packet,w.context)||!w.context ||
                   !v443::read(w.dp+8u,w.a)||!v443::read(w.dp+10u,w.b)||
                   !v443::read(w.dp+12u,w.beta_bits)){
                    out.status=probe_status::packet_unreadable;return out;
                }
                std::uint16_t pa=0,pb=0;
                std::uint32_t pbeta=0;
                if(!v443::read(w.packet+0x4cu,pa)||
                   !v443::read(w.packet+0x4eu,pb)||
                   !v443::read(w.packet+0x50u,pbeta)||
                   pa!=w.a||pb!=w.b||pbeta!=w.beta_bits){
                    out.status=probe_status::packet_disagrees;return out;
                }
                w.valid=true;out.w=w;out.status=probe_status::live_default_model;
                return out;
            }
        }
        if(!v443::unwind_one(c,low,high))break;
    }
    out.status=probe_status::no_model_builder;
    return out;
}
inline bool target_native_row(const builder_witness&w) noexcept {
    return w.valid && w.a==w.b && w.beta_bits==0u &&
           ((w.a&0xffffu)==0x051eu||(w.a&0xffffu)==0x051fu);
}
} // namespace dsrrl::runtime::v448
