#pragma once
// V291: opt-in read-only material stage observer, never a bridge authority.
// Log routed materials regardless of SpecRGB and include P_Metal route345.
// Unrouted stock pipelines have no authenticated FLVER/MTD owner: do not
// infer one from a texture view. Bounded dedup may lose logs, never draws.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
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
} // namespace dsrrl::runtime::v291_material_trace
