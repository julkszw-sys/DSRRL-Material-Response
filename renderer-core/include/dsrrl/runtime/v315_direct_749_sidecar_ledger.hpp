#pragma once
// V315: bounded, persistent-per-process evidence for EXACT 749 directly
// owned DSR logical SpecRGB names. PTDE-only low _L_s and two unresolved
// weapons never get an index. This ledger does NOT authorize a shader,
// resource, FLVER alias, or material operator and cannot change the draw.
#include "dsrrl/runtime/generated_spec_whitelist_20261010.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>

namespace dsrrl::runtime::v315 {
namespace names=dsrrl::runtime::sidecar_20261010;
static_assert(names::k_dsr_direct_spec_hashes.size()==749u,
    "V315 exact DSR non-low source corpus drift");
enum state : std::uint8_t {
    stock_t1_seen = 1u,
    original_logical = 2u,
    ptde_companion = 4u,
    exact_owner = 8u,
    full_prepare = 16u,
    draw_issued = 32u,
    lerp_draw = 64u,
    state_restored = 128u
};
inline std::array<std::atomic<std::uint8_t>,749u> bits{};
inline std::atomic<std::uint32_t> unidentifiable_draws{0u};
inline std::atomic<std::uint32_t> rejected_low_or_unknown{0u};

inline std::size_t find_index(std::uint64_t hash) noexcept {
    if(hash==0u) return 749u;
    std::size_t left=0u,right=749u;
    while(left<right) {
        const auto mid=left+((right-left)>>1u);
        const auto m=names::k_dsr_direct_spec_hashes[mid];
        if(m<hash)left=mid+1u;
        else right=mid;
    }
    return left<749u && names::k_dsr_direct_spec_hashes[left]==hash
        ? left : 749u;
}
inline std::uint32_t count(std::uint8_t mask) noexcept {
    std::uint32_t n=0u;
    for(const auto &b:bits)
        if((b.load(std::memory_order_relaxed)&mask)==mask)++n;
    return n;
}
inline void observe(std::uint64_t hash,std::uint8_t new_bits,
                    const char*stage) noexcept {
    const auto idx=find_index(hash);
    if(idx==749u) {
        // Low-LOD and unresolved resources cannot advance 749 targets.
        rejected_low_or_unknown.fetch_add(1u,std::memory_order_relaxed);
        return;
    }
    auto &v=bits[idx];
    auto before=v.fetch_or(new_bits,std::memory_order_relaxed);
    if((before&new_bits)==new_bits)return;
    // Log just first-ever transitions for a given exact name and stage.
    // No disk file, no per-draw allocation or unbounded telemetry.
    if(new_bits & (ptde_companion|full_prepare|draw_issued)) {
        char line[350]{};
        std::snprintf(line,sizeof(line),
          "[DSRRL V315 749 SIDECAR COVERAGE] stage=%s "
          "spec=%016llx flags=%u seen=%u ready=%u "
          "prepared=%u issued=%u lerp_issued=%u "
          "restored=%u scope=DIRECT_DSR_ONLY "
          "all_draws_verified=0 ptde_pixels=OPEN",
          stage,
          static_cast<unsigned long long>(hash),
          static_cast<unsigned>(before|new_bits),
          count(original_logical),count(ptde_companion),
          count(full_prepare),count(draw_issued),
          count(draw_issued|lerp_draw),
          count(draw_issued|state_restored));
        reshade::log::message(reshade::log::level::info,line);
    }
}
inline void observe_probe(const specular_companion_probe &p,
                          bool owner,bool consumer) noexcept {
    if(!p.snapshot_resolved || p.logical_hash==0u) {
        unidentifiable_draws.fetch_add(1u,std::memory_order_relaxed);
        return;
    }
    std::uint8_t stage=original_logical;
    if(p.stock_bound)stage|=stock_t1_seen;
    if(p.companion_ready && p.logical_hash_allowed)
        stage|=ptde_companion;
    if(owner)stage|=exact_owner;
    // Consumer candidacy cannot be promoted to a prepared/issued draw.
    (void)consumer;
    observe(p.logical_hash,stage,"logical_observed");
}
inline void prepared(std::uint64_t hash) noexcept {
    if(hash==0u)return;
    observe(hash,stock_t1_seen|original_logical|ptde_companion|
                 exact_owner|full_prepare,"atomic_prepared");
}
inline void issued(std::uint64_t hash,bool lerp,bool restored) noexcept {
    if(hash==0u)return;
    std::uint8_t flags=stock_t1_seen|original_logical|
        ptde_companion|exact_owner|full_prepare|draw_issued;
    if(lerp)flags|=lerp_draw;
    if(restored)flags|=state_restored;
    observe(hash,flags,lerp?"lerp_draw_issued":"envspec_draw_issued");
}
inline void summary() noexcept {
    char line[480]{};
    std::snprintf(line,sizeof(line),
      "[DSRRL V315 749 SIDECAR SUMMARY] "
      "target_direct=749 ptde_low_only_excluded=544 "
      "unresolved_weapons_excluded=2 "
      "logical_seen=%u sidecar_ready=%u "
      "atomic_prepared=%u indexed_or_draw_issued=%u "
      "lerp_issued=%u issued_state_restored=%u "
      "unidentified_samples=%u excluded_sample_events=%u "
      "all_draws_verified=0 ptde_pixels=OPEN",
      count(original_logical),count(ptde_companion),
      count(full_prepare),count(draw_issued),
      count(draw_issued|lerp_draw),
      count(draw_issued|state_restored),
      unidentifiable_draws.load(std::memory_order_relaxed),
      rejected_low_or_unknown.load(std::memory_order_relaxed));
    reshade::log::message(reshade::log::level::info,line);
}
} // namespace dsrrl::runtime::v315
