#pragma once
// V287 diagnostic observer only. Never used for draw authorization or
// materialization. Bounded lock-free stage deduplication (max 512 keys), no
// allocations, disk I/O, D3D queries, or source identity guesses.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v287_nonmetal_trace {

inline std::array<std::atomic<std::uint64_t>, 512> seen{};

inline bool claim(const operators::material_response::material_identity &m,
                  std::uint32_t rx, std::uint32_t family,
                  const char *stage) noexcept
{
    if (stage == nullptr) return false;
    std::uint64_t hash=14695981039346656037ull;
    const auto mix=[&](std::uint8_t v) noexcept {
        hash=(hash ^ static_cast<std::uint64_t>(v))*1099511628211ull;
    };
    for (const auto v:m.raw_mtd_sha256) mix(v);
    for (const auto v:m.flver_sha256) mix(v);
    for (unsigned i=0;i<4;i++) {
        mix(static_cast<std::uint8_t>(m.route_index>>(i*8)));
        mix(static_cast<std::uint8_t>(m.material_slot>>(i*8)));
        mix(static_cast<std::uint8_t>(rx>>(i*8)));
        mix(static_cast<std::uint8_t>(family>>(i*8)));
    }
    for (auto *p=stage;*p;++p) mix(static_cast<std::uint8_t>(*p));
    if (hash==0u) hash=1u;
    // No false-positive authorization is possible: collision only suppresses
    // a diagnostic line. Existing 4-hop bounded independent hash probes.
    constexpr auto mask=seen.size()-1u;
    for (std::size_t n=0;n<8u;n++) {
        const auto idx=(static_cast<std::size_t>(hash)+n*73u)&mask;
        auto &entry=seen[idx];
        const auto old=entry.load(std::memory_order_relaxed);
        if (old==hash) return false;
        if (old==0u) {
            std::uint64_t zero=0u;
            if (entry.compare_exchange_strong(zero,hash,
                    std::memory_order_relaxed)) return true;
            if (zero==hash) return false;
        }
    }
    return false;
}

inline void log(const char *stage,
                const operators::material_response::material_identity &m,
                const operators::material_response::decision &d,
                std::uint32_t family) noexcept
{
    // Preserve historical P_Metal instrumentation and priority unchanged.
    if (m.route_index==345u || !claim(m,d.receiver_id,family,stage))
        return;
    char line[640]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V287 MAT TRACE] stage=%s family=%u rx=%u route=%u slot=%u exact=%u valid=%u mtd=%02x%02x%02x%02x%02x%02x%02x%02x flver=%02x%02x%02x%02x%02x%02x%02x%02x",
        stage,static_cast<unsigned>(family),
        static_cast<unsigned>(d.receiver_id),
        static_cast<unsigned>(m.route_index),
        static_cast<unsigned>(m.material_slot),
        m.owner_tuple_exact?1u:0u,m.valid?1u:0u,
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

} // namespace dsrrl::runtime::v287_nonmetal_trace
