#pragma once
// V290: diagnostic-only observation of sidecar SpecRGB reachability on
// draw-local material identities. An asset name is NOT shader authorization.
// No D3D query, allocation, I/O, or draw mutation in the dedupe function.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v290_spec_census {
inline std::array<std::atomic<std::uint64_t>,8192u> first_seen{};

inline bool claim(
    const operators::material_response::material_identity &m,
    std::uint32_t rx, std::uint32_t family) noexcept
{
    std::uint64_t h=14695981039346656037ull;
    const auto mix=[&](std::uint8_t b) noexcept {
        h=(h ^ static_cast<std::uint64_t>(b))*1099511628211ull;
    };
    for(const auto v:m.raw_mtd_sha256) mix(v);
    for(const auto v:m.flver_sha256) mix(v);
    for(unsigned j=0u;j<4u;j++) {
        mix(static_cast<std::uint8_t>(m.route_index>>(8u*j)));
        mix(static_cast<std::uint8_t>(m.material_slot>>(8u*j)));
        mix(static_cast<std::uint8_t>(rx>>(8u*j)));
        mix(static_cast<std::uint8_t>(family>>(8u*j)));
    }
    if(h==0u)h=1u;
    constexpr auto mask=first_seen.size()-1u;
    for(std::size_t i=0u;i<16u;i++) {
        auto &cell=first_seen[(static_cast<std::size_t>(h)+i*127u)&mask];
        const auto previous=cell.load(std::memory_order_relaxed);
        if(previous==h)return false;
        if(previous==0u) {
            std::uint64_t zero=0u;
            if(cell.compare_exchange_strong(zero,h,std::memory_order_relaxed))
                return true;
            if(zero==h)return false;
        }
    }
    return false; // diagnostic capacity overflow only, never fail a draw
}

inline void report(
    const operators::material_response::material_identity &m,
    std::uint32_t rx,
    std::uint32_t family,
    bool atomic_consumer_authorized,
    const specular_companion_probe &probe) noexcept
{
    const char *cut =
        !probe.stock_bound ? "NO_STOCK_T1" :
        !probe.snapshot_resolved ? "NO_LOGICAL_BIND" :
        !probe.logical_hash_allowed ? "NOT_IN_OCT10_OR_V12" :
        !probe.companion_ready ? "SIDECAR_NOT_READY" :
        !m.valid ? "NO_MTD" :
        !m.owner_tuple_exact ? "NO_FLVER_OWNER" :
        !atomic_consumer_authorized ? "NO_EXACT_PTDE_CONSUMER" :
        "ATOMIC_CANDIDATE";
    char line[704]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V290 SPEC DRAW CENSUS] cut=%s rx=%u family=%u route=%u slot=%u "
        "material=%u owner=%u stock_t1=%u logical=%u manifest=%u file=%u "
        "spec=%016llx mtd=%02x%02x%02x%02x%02x%02x%02x%02x "
        "flver=%02x%02x%02x%02x%02x%02x%02x%02x",
        cut,static_cast<unsigned>(rx),static_cast<unsigned>(family),
        static_cast<unsigned>(m.route_index),
        static_cast<unsigned>(m.material_slot),
        m.valid?1u:0u,m.owner_tuple_exact?1u:0u,
        probe.stock_bound?1u:0u,probe.snapshot_resolved?1u:0u,
        probe.logical_hash_allowed?1u:0u,
        probe.companion_ready?1u:0u,
        static_cast<unsigned long long>(probe.logical_hash),
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
} // namespace dsrrl::runtime::v290_spec_census
