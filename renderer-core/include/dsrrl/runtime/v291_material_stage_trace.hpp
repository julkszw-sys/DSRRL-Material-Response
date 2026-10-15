#pragma once
// V291: opt-in read-only material stage observer, never a bridge authority.
// Log routed materials regardless of SpecRGB and include P_Metal route345.
// Unrouted stock pipelines have no authenticated FLVER/MTD owner: do not
// infer one from a texture view. Bounded dedup may lose logs, never draws.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <cstddef>
namespace dsrrl::runtime::v291_material_trace {
inline std::array<std::atomic<std::uint64_t>,16384u> recorded{};
inline bool first(const operators::material_response::material_identity &m,
                  std::uint32_t receiver,std::uint32_t family,
                  const char *stage) noexcept {
    if(stage==nullptr)return false;
    std::uint64_t h=14695981039346656037ull;
    const auto mix=[&](std::uint8_t b) noexcept {
        h=(h^static_cast<std::uint64_t>(b))*1099511628211ull;
    };
    for(const auto b:m.raw_mtd_sha256)mix(b);
    for(const auto b:m.flver_sha256)mix(b);
    for(const auto n:{m.route_index,m.material_slot,receiver,family})
        for(unsigned i=0;i<4u;i++)
            mix(static_cast<std::uint8_t>(n>>(8u*i)));
    for(auto p=stage;*p;++p)mix(static_cast<std::uint8_t>(*p));
    if(h==0u)h=1u;
    constexpr auto mask=recorded.size()-1u;
    for(unsigned i=0u;i<16u;i++) {
        auto &cell=recorded[(static_cast<std::size_t>(h)+101u*i)&mask];
        const auto old=cell.load(std::memory_order_relaxed);
        if(old==h)return false;
        if(old==0u) {
            std::uint64_t zero=0u;
            if(cell.compare_exchange_strong(
                zero,h,std::memory_order_relaxed))return true;
            if(zero==h)return false;
        }
    }
    return false;
}
inline void log(const char *stage,
                const operators::material_response::material_identity &m,
                const operators::material_response::decision &d,
                std::uint32_t rx,std::uint32_t family,
                std::uint32_t island_mask=0u) noexcept {
    if(!first(m,rx,family,stage))return;
    char line[768]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V291 MATERIAL STAGE] stage=%s rx=%u family=%u route=%u "
        "slot=%u material=%u owner=%u active=%u island_mask=%u "
        "mtd=%02x%02x%02x%02x%02x%02x%02x%02x "
        "flver=%02x%02x%02x%02x%02x%02x%02x%02x",
        stage,static_cast<unsigned>(rx),static_cast<unsigned>(family),
        static_cast<unsigned>(d.route_index),
        static_cast<unsigned>(m.material_slot),
        m.valid?1u:0u,m.owner_tuple_exact?1u:0u,
        d.active?1u:0u,static_cast<unsigned>(island_mask),
        m.raw_mtd_sha256[0],m.raw_mtd_sha256[1],
        m.raw_mtd_sha256[2],m.raw_mtd_sha256[3],
        m.raw_mtd_sha256[4],m.raw_mtd_sha256[5],
        m.raw_mtd_sha256[6],m.raw_mtd_sha256[7],
        m.flver_sha256[0],m.flver_sha256[1],
        m.flver_sha256[2],m.flver_sha256[3],
        m.flver_sha256[4],m.flver_sha256[5],
        m.flver_sha256[6],m.flver_sha256[7]);
    reshade::log::message(reshade::log::level::info,line);
}
// No-route pipelines have no authenticated MTD/FLVER. The bound stock t1
// resource identity is diagnostic only and cannot authorize an operator.
inline void log_unrouted(const void *stock_t1,
                         const specular_companion_probe &probe) noexcept {
    char line[320]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V291 UNROUTED MATERIAL] source=STOCK_T1_ONLY "
        "mtd=UNKNOWN flver=UNKNOWN receiver=UNKNOWN "
        "view=%016llx stock_t1=%u logical=%u manifest=%u file=%u "
        "spec=%016llx",
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(stock_t1)),
        probe.stock_bound?1u:0u,probe.snapshot_resolved?1u:0u,
        probe.logical_hash_allowed?1u:0u,probe.companion_ready?1u:0u,
        static_cast<unsigned long long>(probe.logical_hash));
    reshade::log::message(reshade::log::level::info,line);
}
// Bound PS pipeline handles are session-local evidence, NOT shader hashes.
// This exposes unknown pixel receiver families even when stock t1 is absent.
inline std::array<std::atomic<std::uint64_t>,8192u> pipelines_seen{};
inline void log_pipeline_bind(std::uint64_t handle,
                              std::uint32_t route_mask) noexcept {
    if(handle==0u)return;
    const std::size_t mask=pipelines_seen.size()-1u;
    const auto h=(handle ^ (static_cast<std::uint64_t>(route_mask)<<32u)) | 1u;
    for(std::size_t i=0;i<16u;++i) {
        auto &cell=pipelines_seen[(static_cast<std::size_t>(h)+i*113u)&mask];
        const auto old=cell.load(std::memory_order_relaxed);
        if(old==h)return;
        if(old==0u) {
            std::uint64_t zero=0u;
            if(cell.compare_exchange_strong(zero,h,std::memory_order_relaxed)) {
                char line[272]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V291 SHADER PIPELINE] bound=%016llx route_mask=%u "
                    "material=UNKNOWN flver=UNKNOWN shader_sha=UNKNOWN",
                    static_cast<unsigned long long>(handle),
                    static_cast<unsigned>(route_mask));
                reshade::log::message(reshade::log::level::info,line);
                return;
            }
            if(zero==h)return;
        }
    }
}

#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
inline std::array<std::atomic<std::uint64_t>,8192u> lerp_cubemap_seen{};
inline void log_lerp_cubemaps(
    const char *stage,
    const operators::material_response::material_identity &m,
    const operators::material_response::decision &d,
    const prepared_pmetal_envspec_draw &p) noexcept
{
    if (!p.diagnostic_lerp || stage == nullptr) return;
    const float beta=p.diagnostic_beta;
    const unsigned band=beta<0.0f?6u:beta==0.0f?0u:
        beta<0.1f?1u:beta<0.4f?2u:beta<0.7f?3u:beta<1.0f?4u:5u;
    std::uint64_t hash=14695981039346656037ull;
    const auto mix=[&](std::uint8_t x) noexcept {
        hash=(hash^static_cast<std::uint64_t>(x))*1099511628211ull;
    };
    for(auto v:m.raw_mtd_sha256)mix(v);
    for(auto v:m.flver_sha256)mix(v);
    for(auto n:{d.receiver_id,m.route_index,m.material_slot,
                static_cast<std::uint32_t>(band),
                static_cast<std::uint32_t>(p.diagnostic_probe_a),
                static_cast<std::uint32_t>(p.diagnostic_probe_b)})
        for(unsigned k=0u;k<4u;k++)
            mix(static_cast<std::uint8_t>(n>>(8u*k)));
    for(auto ptr=stage;*ptr;++ptr)mix(static_cast<std::uint8_t>(*ptr));
    if(hash==0u)hash=1u;
    constexpr auto mask=8191u;
    bool new_record=false;
    for(unsigned i=0u;i<16u;++i) {
        auto &v=lerp_cubemap_seen[(static_cast<std::size_t>(hash)+i*97u)&mask];
        if(v.load(std::memory_order_relaxed)==hash)return;
        std::uint64_t empty=0u;
        if(v.compare_exchange_strong(empty,hash,std::memory_order_relaxed)) {
            new_record=true;break;
        }
        if(empty==hash)return;
    }
    if(!new_record)return;
    const bool pair_valid =
        p.diagnostic_probe_a==p.diagnostic_diff_a &&
        p.diagnostic_probe_b==p.diagnostic_diff_b &&
        p.env_resources.ptde_a!=nullptr &&
        p.env_resources.ptde_b!=nullptr &&
        p.envdiffuse_resources.ptde_a!=nullptr &&
        p.envdiffuse_resources.ptde_b!=nullptr &&
        p.env_resources.sampler!=nullptr &&
        p.envdiffuse_resources.sampler!=nullptr &&
        p.b12!=nullptr;
    char line[1152]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V291 LERP CUBEMAP] stage=%s rx=%u route=%u slot=%u "
        "beta=%.9g beta_band=%u serial=%llu pair_join=%u "
        "spec_a=%u spec_b=%u diffuse_a=%u diffuse_b=%u "
        "t11=%016llx t13=%016llx t12=%016llx t14=%016llx "
        "s_envspec=%u s_envdiffuse=%u b12=%u "
        "mtd=%02x%02x%02x%02x%02x%02x%02x%02x "
        "flver=%02x%02x%02x%02x%02x%02x%02x%02x",
        stage,static_cast<unsigned>(d.receiver_id),
        static_cast<unsigned>(m.route_index),
        static_cast<unsigned>(m.material_slot),
        static_cast<double>(beta),band,
        static_cast<unsigned long long>(p.diagnostic_serial),
        pair_valid?1u:0u,
        static_cast<unsigned>(p.diagnostic_probe_a),
        static_cast<unsigned>(p.diagnostic_probe_b),
        static_cast<unsigned>(p.diagnostic_diff_a),
        static_cast<unsigned>(p.diagnostic_diff_b),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(p.envdiffuse_resources.ptde_a)),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(p.envdiffuse_resources.ptde_b)),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(p.env_resources.ptde_a)),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(p.env_resources.ptde_b)),
        p.env_resources.sampler!=nullptr?1u:0u,
        p.envdiffuse_resources.sampler!=nullptr?1u:0u,
        p.b12!=nullptr?1u:0u,
        m.raw_mtd_sha256[0],m.raw_mtd_sha256[1],m.raw_mtd_sha256[2],m.raw_mtd_sha256[3],
        m.raw_mtd_sha256[4],m.raw_mtd_sha256[5],m.raw_mtd_sha256[6],m.raw_mtd_sha256[7],
        m.flver_sha256[0],m.flver_sha256[1],m.flver_sha256[2],m.flver_sha256[3],
        m.flver_sha256[4],m.flver_sha256[5],m.flver_sha256[6],m.flver_sha256[7]);
    reshade::log::message(reshade::log::level::info,line);
}
#endif
} // namespace dsrrl::runtime::v291_material_trace
