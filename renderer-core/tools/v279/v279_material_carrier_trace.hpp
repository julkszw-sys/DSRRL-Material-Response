#pragma once
// V279: exact GOOD P_Metal / BAD-family Leather operator transport census.
// Observe PREPARED transaction only; never rebind or mutate DXBC/CB/SRV/PS.
// Restricted to owner-visible two FLVER assets, their paired metal/leather
// material slots, and RX33/RX34 Stable+Lerp. Not a GPU pixel sampler.
#include "dsrrl/runtime/v275_material_draw_trace.hpp"
#include <atomic>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <d3d11.h>
#include <reshade.hpp>

namespace dsrrl::runtime::v279 {
namespace mr=dsrrl::operators::material_response;
inline constexpr const char *k_metal =
    "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";
inline constexpr const char *k_leather =
    "0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e";
inline constexpr const char *k_asset0 =
    "eb3b77ee419f9e2997b7d427987c002411fdf08edda12834be4a9f51b3c4eb7b";
inline constexpr const char *k_asset1 =
    "672e35606495f24e36c704f32fceda9b933563a64e03379375f47f9fe04a0a7d";
inline std::array<std::atomic<std::uint64_t>,1024u> g_seen{};
inline std::atomic<std::uint32_t> g_rows{0u};
inline std::atomic<std::uint32_t> g_overflow{0u};
inline bool reserve(std::uint64_t key) noexcept {
    key=v275::mix(key)|1ull;
    std::uint32_t start=static_cast<std::uint32_t>(key)&1023u;
    for(std::uint32_t k=0;k<1024u;++k){
        auto &e=g_seen[(start+k)&1023u];
        auto old=e.load(std::memory_order_relaxed);
        if(old==key)return false;
        if(old==0ull && e.compare_exchange_strong(
            old,key,std::memory_order_relaxed)){
            g_rows.fetch_add(1u,std::memory_order_relaxed);
            return true;
        }
    }
    g_overflow.fetch_add(1u,std::memory_order_relaxed);
    return false;
}
inline void append(char *buffer,std::size_t cap,std::size_t &pos,
                   const char *format,...) noexcept {
    if(pos>=cap || format==nullptr)return;
    va_list args; va_start(args,format);
    auto n=std::vsnprintf(buffer+pos,cap-pos,format,args);
    va_end(args);
    if(n<0){pos=cap;return;}
    auto length=static_cast<std::size_t>(n);
    pos+=length>=cap-pos ? cap-pos : length;
}
inline void observe(reshade::api::command_list *cmd,
                    const mr::material_identity &m,
                    const mr::decision &d,std::uint32_t rx,bool lerp,
                    const draw_tx_mutation &mutation) noexcept {
    if(cmd==nullptr||rx<33u||rx>34u||
       !d.active||!m.owner_tuple_exact||!m.material_slot_valid)return;
    char raw[65]{},flver[65]{};
    v275::hex_digest(m.raw_mtd_sha256,raw);
    v275::hex_digest(m.flver_sha256,flver);
    const int family=std::strcmp(raw,k_metal)==0 ? 0:
        std::strcmp(raw,k_leather)==0 ? 1:-1;
    if(family<0)return;
    const int asset=std::strcmp(flver,k_asset0)==0 ? 0:
        std::strcmp(flver,k_asset1)==0 ? 1:-1;
    if(asset<0)return;
    const std::uint32_t expected_slot=asset==0 ?
        (family==0?3u:2u) : (family==0?5u:8u);
    if(m.material_slot!=expected_slot ||
       (family==0 && d.route_index!=345u) ||
       (family==1 && d.route_index!=1073741869u))return;
    auto *ctx=reinterpret_cast<ID3D11DeviceContext *>(cmd->get_native());
    if(ctx==nullptr)return;
    const auto cb=v274b::observe(ctx); // CPU writer shadow, NOT GPU b0 readback
    std::uint32_t bucket=0xffffu;
    if(cb.has_shadow&&std::isfinite(cb.atmosphere[3]))
        bucket=static_cast<std::uint32_t>(std::max(0.0f,
            std::min(64.0f,cb.atmosphere[3]*40.0f)));
    const std::uint64_t key=
       (static_cast<std::uint64_t>(family)<<60u) |
       (static_cast<std::uint64_t>(asset)<<59u) |
       (static_cast<std::uint64_t>(lerp)<<58u) |
       (static_cast<std::uint64_t>(rx)<<48u) |
       static_cast<std::uint64_t>(bucket);
    if(!reserve(key))return;
    char line[3400]{};
    std::size_t n=0u;
    append(line,sizeof(line),n,
       "[DSRRL V279 MATERIAL CARRIER] class=%s phase=%s rx=%u "
       "asset=%u slot=%u flver=%s mtd=%s route=%u "
       "fog_bucket=%u cb85_cpu=%.9g cb_authority=CPU_WRITER_ONLY "
       "mutation_ps=%016llx ps_change=%u "
       "operators=%016llx shader_ops=%016llx cb_ops=%016llx "
       "srv_ops=%016llx carrier_ops=%016llx "
       "cb_count=%u srv_count=%u smp_count=%u ",
       family==0?"METAL_GOOD":"LEATHER_BAD_FAMILY",
       lerp?"LERP":"STABLE",rx,asset,expected_slot,flver,raw,
       d.route_index,bucket,static_cast<double>(cb.cb85),
       static_cast<unsigned long long>(v275::ptr_id(mutation.pixel_shader)),
       mutation.replace_pixel_shader?1u:0u,
       static_cast<unsigned long long>(mutation.owners),
       static_cast<unsigned long long>(mutation.shader_owners),
       static_cast<unsigned long long>(mutation.constant_buffer_owners),
       static_cast<unsigned long long>(mutation.resource_owners),
       static_cast<unsigned long long>(mutation.carrier_owners),
       mutation.constant_buffer_count,mutation.srv_count,
       mutation.sampler_count);
    for(std::uint32_t i=0u;
        i<mutation.constant_buffer_count && i<draw_tx_max_cb;++i) {
        const auto &x=mutation.constant_buffers[i];
        if(x.slot>15u)continue;
        D3D11_BUFFER_DESC desc{};
        if(x.buffer)x.buffer->GetDesc(&desc);
        append(line,sizeof(line),n,"CB%u=%016llx/bytes%u/owners%016llx ",
           x.slot,static_cast<unsigned long long>(v275::ptr_id(x.buffer)),
           desc.ByteWidth,static_cast<unsigned long long>(x.owners));
    }
    for(std::uint32_t i=0u;i<mutation.srv_count && i<draw_tx_max_srv;++i) {
        const auto &x=mutation.srvs[i];
        if(x.slot!=10u&&x.slot!=12u&&x.slot!=13u&&x.slot!=14u)continue;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        if(x.srv)x.srv->GetDesc(&desc);
        append(line,sizeof(line),n,"SRV%u=%016llx/fmt%u/dim%u ",
            x.slot,static_cast<unsigned long long>(v275::ptr_id(x.srv)),
            static_cast<unsigned>(desc.Format),
            static_cast<unsigned>(desc.ViewDimension));
    }
    for(std::uint32_t i=0u;
        i<mutation.sampler_count && i<draw_tx_max_sampler;++i){
        const auto &x=mutation.samplers[i];
        if(x.slot!=10u&&x.slot!=12u&&x.slot!=14u)continue;
        D3D11_SAMPLER_DESC desc{};
        if(x.sampler)x.sampler->GetDesc(&desc);
        append(line,sizeof(line),n,
            "SMP%u=%016llx/filter%u/U%u/V%u/W%u/lod%g:%g ",
            x.slot,static_cast<unsigned long long>(v275::ptr_id(x.sampler)),
            static_cast<unsigned>(desc.Filter),
            static_cast<unsigned>(desc.AddressU),
            static_cast<unsigned>(desc.AddressV),
            static_cast<unsigned>(desc.AddressW),
            static_cast<double>(desc.MinLOD),
            static_cast<double>(desc.MaxLOD));
    }
    // Read only the *pre-transaction* bound host b12 and t12/t14.
    // Mutated replacement state is represented by the explicit batch above.
    ID3D11Buffer *native_b12=nullptr;
    ID3D11ShaderResourceView *native_srvs[3]{};
    ctx->PSGetConstantBuffers(12u,1u,&native_b12);
    ctx->PSGetShaderResources(12u,3u,native_srvs);
    append(line,sizeof(line),n,
      "HOST_PRE_CB12=%016llx HOST_PRE_T12=%016llx "
      "HOST_PRE_T14=%016llx GPU_CB_VALUES=UNOBSERVED "
      "GPU_PSFIRST_CONSUMER=UNOBSERVED PIXEL=OPEN",
      static_cast<unsigned long long>(v275::ptr_id(native_b12)),
      static_cast<unsigned long long>(v275::ptr_id(native_srvs[0])),
      static_cast<unsigned long long>(v275::ptr_id(native_srvs[2])));
    if(native_b12)native_b12->Release();
    for(auto *p:native_srvs)if(p)p->Release();
    reshade::log::message(reshade::log::level::info,line);
}
}
