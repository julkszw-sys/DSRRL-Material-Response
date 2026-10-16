#pragma once
// V316 derived view of V315's original 749-entry atomic draw ledger.
// Only STRICT 744 names count towards the owner-requested operational target.
// Do not mutate V315 source/provenance accounting or treat one issued draw
// as proof that every FLVER/slot/receiver of a logical name is equivalent.
#include "dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp"
#include "dsrrl/runtime/v316_strict_744_sidecar_gate.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v316 {
inline std::uint32_t count_strict(std::uint8_t mask) noexcept {
    std::uint32_t result=0u;
    for(std::size_t i=0u;i<v315::bits.size();++i) {
        if(is_excluded_direct_low(
            sidecar_20261010::k_dsr_direct_spec_hashes[i]))continue;
        if((v315::bits[i].load(std::memory_order_relaxed)&mask)==mask)++result;
    }
    return result;
}
inline void strict_summary() noexcept {
    char line[512]{};
    std::snprintf(line,sizeof(line),
      "[DSRRL V316 STRICT 744 SIDECAR SUMMARY] "
      "source_direct=749 strict_target=744 excluded_direct_low=5 "
      "logical_seen=%u companion_ready=%u atomic_prepared=%u "
      "draw_issued=%u lerp_issued=%u issued_and_restored=%u "
      "low_policy_filter_events=%llu "
      "all_draws_verified=0 ptde_pixels=OPEN",
      count_strict(v315::original_logical),
      count_strict(v315::ptde_companion),
      count_strict(v315::full_prepare),
      count_strict(v315::draw_issued),
      count_strict(v315::draw_issued|v315::lerp_draw),
      count_strict(v315::draw_issued|v315::state_restored),
      static_cast<unsigned long long>(
          low_suffix_policy_filter_events.load(std::memory_order_relaxed)));
    reshade::log::message(reshade::log::level::info,line);
}
} // namespace dsrrl::runtime::v316
