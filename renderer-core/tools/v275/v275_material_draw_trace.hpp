#pragma once
// V275 SOURCE-ONLY DRAW TRACE: observe the exact authenticated material and
// original bound DX11 PS/t1/t10/RTV, plus planned atomic island mutation.
// No GPU readback, no pixel claims, no shader/resource/CB writes, no draw replay.
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/draw_state_transaction.hpp"
#include "dsrrl/runtime/v274b_cb_shadow.hpp"
#include <reshade.hpp>
#include <Windows.h>
#include <d3d11.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace dsrrl::runtime::v275 {
namespace mr = dsrrl::operators::material_response;
inline constexpr std::array<std::uint16_t,27u> k_exact_cloth_leather_indices = {{
    146u, // C_5250_Fire_DullLeather[DSB].mtd
    147u, // C_5260_Leather[DSB].mtd
    153u, // C_DullLeather[DSB].mtd
    154u, // C_DullLeather[DSB][L].mtd
    155u, // C_DullLeather[DSB][L]_Alp.mtd
    156u, // C_DullLeather[DSB]_Alp.mtd
    157u, // C_DullLeather[DSB]_Edge.mtd
    158u, // C_Fire_DullLeather[DSB].mtd
    159u, // C_Fire_DullLeather[DSB]_Edge.mtd
    160u, // C_Leather[DSB].mtd
    161u, // C_Leather[DSB]_Alp.mtd
    162u, // C_Leather[DSB]_Edge.mtd
    166u, // C_RoughCloth[DSB].mtd
    167u, // C_RoughCloth[DSB]_Edge.mtd
    276u, // P_Damage_L_DullLeather[DSB].mtd
    277u, // P_Damage_L_Leather[DSB].mtd
    280u, // P_Damage_S_DullLeather[DSB].mtd
    281u, // P_Damage_S_Leather[DSB].mtd
    284u, // P_DullLeather[DSB].mtd
    285u, // P_DullLeather[DSB].mtd
    286u, // P_DullLeather[DSB]_Edge.mtd
    287u, // P_Leather[DSB].mtd
    288u, // P_Leather[DSB]_Alp.mtd
    289u, // P_Leather[DSB]_Edge.mtd
    290u, // P_Leather[DS].mtd
    294u, // P_RoughCloth[DSB].mtd
    295u // P_RoughCloth[DSB]_Edge.mtd
}};
static_assert(mr::generated::k_envspec_router_record_count==325u,
              "V275 exact MTD router changed, census must be reverified");
// V276: per-class fairness, not a global 768 event cutoff. 27 MTD classes
// and the independent P_Metal control each get their own 1024 open-addressed
// keys, allowing late Lerp beta bands even if a prior material is verbose.
// Monotone, no background thread and no D3D state mutation.
inline constexpr std::uint32_t k_family_count=28u;
inline constexpr std::uint32_t k_slots_per_family=1024u;
inline std::array<std::atomic<std::uint64_t>,
    k_family_count*k_slots_per_family> g_seen{};
inline std::array<std::atomic<std::uint32_t>, k_family_count> g_family_seen{};
inline std::array<std::atomic<std::uint32_t>, k_family_count> g_family_exhausted{};
inline std::atomic<std::uint32_t> g_count{0u};
inline std::atomic<std::uint32_t> g_overflow{0u};
inline void hex_digest(const dsrrl::core::sha256_digest &h, char (&out)[65]) noexcept {
    constexpr char hex[]="0123456789abcdef";
    for (std::size_t i=0; i<32u; ++i) {
        const auto x=h[i];
        out[2u*i]=hex[(x>>4u)&15u];
        out[2u*i+1u]=hex[x&15u];
    }
    out[64]='\0';
}
struct match {
    const mr::generated::envspec_router_record *row=nullptr;
    bool sha_exact=false;
    bool semantic_exact=false;
    bool duplicate_sha=false;
    bool ambiguous=false;
    bool control=false;
};
inline match classify(const mr::material_identity &m,
                      const mr::decision &d) noexcept {
    match out{};
    const auto &all=mr::generated::k_envspec_router_v1;
    // Explicit only P_Metal control: cannot broaden EnvSpec bridge.
    if (d.route_index==345u) {
        out.control=true;
        return out;
    }
    std::size_t candidates=0u;
    for (auto i:k_exact_cloth_leather_indices) {
        const auto &e=all[i];
        const bool sh=std::memcmp(e.raw_mtd_sha256.data(),
                                  m.raw_mtd_sha256.data(),32u)==0;
        const bool nm=e.semantic_name_hash==m.semantic_name_hash;
        if (!sh && !nm) continue;
        if (sh) ++candidates;
        if (sh && nm) {
            out.row=&e;
            out.sha_exact=true;
            out.semantic_exact=true;
        } else if (!out.sha_exact && out.row==nullptr) {
            out.row=&e;
            out.sha_exact=sh;
            out.semantic_exact=nm;
        }
    }
    out.duplicate_sha=(candidates>1u);
    out.ambiguous=out.row!=nullptr &&
                  !(out.sha_exact && out.semantic_exact);
    return out;
}
inline std::uint64_t mix(std::uint64_t x) noexcept {
    x^=x>>33; x*=0xff51afd7ed558ccdull;
    x^=x>>33; x*=0xc4ceb9fe1a85ec53ull; x^=x>>33;
    return x;
}
inline std::uint32_t family_id(const match &matched) noexcept {
    if (matched.control || matched.row==nullptr) return 27u;
    const auto &a=mr::generated::k_envspec_router_v1;
    for (std::uint32_t i=0u;i<27u;++i) {
        if (&a[k_exact_cloth_leather_indices[i]] == matched.row)
            return i;
    }
    return 27u; // unresolved is never admitted by classify() gate
}
inline bool reserve_key(std::uint64_t key, std::uint32_t family) noexcept {
    if (family >= k_family_count) return false;
    key=mix(key)|1ull;
    const auto base=family*k_slots_per_family;
    const auto start=static_cast<std::uint32_t>((key>>12u)
                      & (k_slots_per_family-1u));
    // Avoid a 768/28672-element linear scan on EVERY frequently repeated draw.
    for (std::uint32_t attempt=0u; attempt<k_slots_per_family;++attempt) {
        auto &entry=g_seen[base+((start+attempt)&(k_slots_per_family-1u))];
        auto prior=entry.load(std::memory_order_relaxed);
        if(prior==key) return false;
        if(prior==0ull &&
           entry.compare_exchange_strong(prior,key,std::memory_order_relaxed)) {
            g_family_seen[family].fetch_add(1u,std::memory_order_relaxed);
            g_count.fetch_add(1u,std::memory_order_relaxed);
            return true;
        }
    }
    g_overflow.fetch_add(1u,std::memory_order_relaxed);
    g_family_exhausted[family].fetch_add(1u,std::memory_order_relaxed);
    return false;
}
inline std::uint64_t ptr_id(const void *p) noexcept {
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(p));
}
inline void observe(reshade::api::command_list *cmd,
                    const mr::material_identity &material,
                    const mr::decision &decision,
                    std::uint32_t receiver, bool lerp,
                    const char *stage,
                    std::uint32_t stage_id,
                    const draw_tx_mutation *mutation,
                    std::uint32_t draw_count,
                    bool indexed) noexcept {
    if(cmd==nullptr || stage==nullptr ||
       receiver<24u || receiver>47u) return;
    const auto matched=classify(material,decision);
    if(matched.row==nullptr && !matched.control) return;
    auto *context=reinterpret_cast<ID3D11DeviceContext *>(cmd->get_native());
    if(context==nullptr) return;
    const auto cb=v274b::observe(context); // CPU writer witness, NOT GPU readback
    // A coarse Fog-c12.w bin is an observed atmospheric STATE CLASS, NOT beta.
    std::uint32_t fog_bucket=0xffffu;
    if(cb.has_shadow && std::isfinite(cb.atmosphere[3]))
        fog_bucket=static_cast<std::uint32_t>(
            std::max(0.0f,std::min(64.0f,cb.atmosphere[3]*40.0f)));
    const auto id=matched.row==nullptr ? 0ull:
        matched.row->semantic_name_hash;
    const std::uint64_t key=id ^
        (material.semantic_name_hash<<1u) ^
        (static_cast<std::uint64_t>(receiver)<<37u) ^
        (static_cast<std::uint64_t>(stage_id)<<42u) ^
        (static_cast<std::uint64_t>(fog_bucket)<<19u) ^
        (static_cast<std::uint64_t>(material.material_slot)<<52u) ^
        (static_cast<std::uint64_t>(lerp)<<61u) ^
        (static_cast<std::uint64_t>(decision.active)<<60u) ^
        (static_cast<std::uint64_t>(matched.sha_exact)<<59u) ^
        (static_cast<std::uint64_t>(decision.route_index)<<5u);
    const auto family=family_id(matched);
    if(!reserve_key(key,family)) return;
    const auto unique=g_count.load(std::memory_order_relaxed);
    if((unique%256u)==0u) {
        char progress[220]{};
        std::snprintf(progress,sizeof(progress),
            "[DSRRL V276 COVERAGE] unique=%u material_group=%u group_unique=%u "
            "family_overflow=%u total_overflow=%u capacity=28x1024 "
            "CB12W_ONLY_NOT_BETA", unique,family,
            g_family_seen[family].load(std::memory_order_relaxed),
            g_family_exhausted[family].load(std::memory_order_relaxed),
            g_overflow.load(std::memory_order_relaxed));
        reshade::log::message(reshade::log::level::info,progress);
    }
    ID3D11PixelShader *ps=nullptr;
    ID3D11ShaderResourceView *srvs[3]{};
    ID3D11RenderTargetView *rtv[2]{};
    context->PSGetShader(&ps,nullptr,nullptr);
    context->PSGetShaderResources(1u,1u,&srvs[0]);
    context->PSGetShaderResources(10u,1u,&srvs[1]);
    context->PSGetShaderResources(12u,1u,&srvs[2]);
    context->OMGetRenderTargets(2u,rtv,nullptr);
    auto release_all=[&]() noexcept {
        if(ps)ps->Release();
        for(auto *v:srvs)if(v)v->Release();
        for(auto *v:rtv)if(v)v->Release();
    };
    char mtd[65]{},flver[65]{};
    hex_digest(material.raw_mtd_sha256,mtd);
    hex_digest(material.flver_sha256,flver);
    std::uint32_t planned_t10=0u,planned_t12=0u;
    std::uint64_t replacement_t10=0u,replacement_t12=0u;
    if(mutation) for(std::uint32_t i=0u;
       i<mutation->srv_count && i<draw_tx_max_srv;++i) {
        const auto &v=mutation->srvs[i];
        if(v.slot==10u) {planned_t10=1u; replacement_t10=ptr_id(v.srv);}
        if(v.slot==12u) {planned_t12=1u; replacement_t12=ptr_id(v.srv);}
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC t1desc{};
    D3D11_SHADER_RESOURCE_VIEW_DESC t10desc{};
    if(srvs[0])srvs[0]->GetDesc(&t1desc);
    if(srvs[1])srvs[1]->GetDesc(&t10desc);
    const auto *name=matched.control ? "P_Metal[DSB].mtd" :
        matched.row->mtd_name;
    char line[2400]{};
    std::snprintf(line,sizeof(line),
       "[DSRRL V275 MATERIAL DRAW] v276_balanced=1 stage=%s phase=%s rx=%u "
       "name=%s route=%u decision_active=%u reason=%u certified=0x%x "
       "full_mtd=%s full_flver=%s sem=%016llx "
       "tuple=%u mat_exact=%u slot=%u slot_valid=%u "
       "profile_sha_exact=%u name_exact=%u duplicate_sha=%u ambiguous=%u "
       "ptde_c101=%.9g ptde_c102=%.9g ptde_probe_slot=%u "
       "host_ps=%016llx planned_ps=%016llx replace_ps=%u "
       "host_t1=%016llx fmt_t1=%u host_t10=%016llx fmt_t10=%u "
       "host_t12=%016llx planned_t10=%u repl_t10=%016llx "
       "planned_t12=%u repl_t12=%016llx "
       "rt0=%016llx rt1=%016llx "
       "cb0_shadow=%u cb_authority=%s cb85=%.8g cb10=%.8g,%.8g,%.8g "
       "fog12w=%.8g fog_bucket=%u writer_epoch=%llu "
       "draw_count=%u indexed=%u "
       "RUNTIME_STAGE=%s GPU_CB_READ=NO PIXEL_RGB=UNOBSERVED",
       stage,lerp?"LERP":"STABLE",receiver,name,
       decision.route_index,decision.active?1u:0u,
       static_cast<unsigned>(decision.reason),
       decision.certified_operations,
       mtd,flver,static_cast<unsigned long long>(material.semantic_name_hash),
       material.owner_tuple_exact?1u:0u,material.actual_material_exact?1u:0u,
       material.material_slot,material.material_slot_valid?1u:0u,
       matched.sha_exact?1u:0u,matched.semantic_exact?1u:0u,
       matched.duplicate_sha?1u:0u,matched.ambiguous?1u:0u,
       matched.row?static_cast<double>([](
         const mr::generated::envspec_router_record *r) noexcept {
         float v=0.0f;std::memcpy(&v,&r->c101_bits[0],4u);return v;
       }(matched.row)):0.0,
       matched.row?static_cast<double>([](
         const mr::generated::envspec_router_record *r) noexcept {
         float v=0.0f;std::memcpy(&v,&r->c102_bits,4u);return v;
       }(matched.row)):0.0,
       matched.row?matched.row->envspc_slot:255u,
       static_cast<unsigned long long>(ptr_id(ps)),
       static_cast<unsigned long long>(
         mutation ? ptr_id(mutation->pixel_shader):0ull),
       mutation && mutation->replace_pixel_shader?1u:0u,
       static_cast<unsigned long long>(ptr_id(srvs[0])),
       static_cast<unsigned>(t1desc.Format),
       static_cast<unsigned long long>(ptr_id(srvs[1])),
       static_cast<unsigned>(t10desc.Format),
       static_cast<unsigned long long>(ptr_id(srvs[2])),
       planned_t10,static_cast<unsigned long long>(replacement_t10),
       planned_t12,static_cast<unsigned long long>(replacement_t12),
       static_cast<unsigned long long>(ptr_id(rtv[0])),
       static_cast<unsigned long long>(ptr_id(rtv[1])),
       cb.has_shadow?1u:0u,cb.status,
       static_cast<double>(cb.cb85),
       static_cast<double>(cb.cb10[0]),
       static_cast<double>(cb.cb10[1]),
       static_cast<double>(cb.cb10[2]),
       static_cast<double>(cb.atmosphere[3]),fog_bucket,
       static_cast<unsigned long long>(cb.epoch),
       draw_count,indexed?1u:0u,stage);
    reshade::log::message(reshade::log::level::info,line);
    release_all();
}
inline void summary() noexcept {
    char b[250]{};
    std::snprintf(b,sizeof(b),
      "[DSRRL V276 SUMMARY] unique=%u overflow=%u "
      "DRAW_CLASSIFICATION_SOURCE_AUTHENTICATED_ONLY "
      "GPU_READBACK=NOT_IMPLEMENTED PIXEL=OPEN",
      g_count.load(std::memory_order_relaxed),
      g_overflow.load(std::memory_order_relaxed));
    reshade::log::message(reshade::log::level::info,b);
}
} // namespace dsrrl::runtime::v275
