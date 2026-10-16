#pragma once
// V316: explicit owner-requested NO LOW LOD operational policy.
// Preserve the original 749 directly-owned DSR source identities and V315
// diagnostic ledger. Five valid DSR-direct *_l_s names remain source facts,
// but are excluded from PTDE equipment SpecRGB sidecar activation.
// No aliases, MTD or shader grants, owner inference, or fallback changes.
#include "dsrrl/runtime/generated_spec_whitelist_20261010.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime::v316 {
constexpr std::uint64_t literal_spec_hash(const char *p) noexcept {
    std::uint64_t h=14695981039346656037ull;
    for(;*p!='\0';++p)
        h=(h^static_cast<std::uint8_t>(*p))*1099511628211ull;
    return h;
}
inline constexpr std::array<std::uint64_t,5u> k_dsr_direct_low_suffix = {{
    literal_spec_hash("am_f_9450_l_s"),
    literal_spec_hash("am_m_9372_l_s"),
    literal_spec_hash("bd_m_9440_1_l_s"),
    literal_spec_hash("bd_m_9440_2_l_s"),
    literal_spec_hash("wp_a_0652_l_s")
}};
constexpr bool is_excluded_direct_low(std::uint64_t hash) noexcept {
    for(const auto exact : k_dsr_direct_low_suffix)
        if(hash==exact)return true;
    return false;
}
constexpr bool all_five_are_original_dsr_direct() noexcept {
    for(const auto h : k_dsr_direct_low_suffix) {
        bool found=false;
        for(const auto source : sidecar_20261010::k_dsr_direct_spec_hashes)
            if(source==h)found=true;
        if(!found)return false;
    }
    return true;
}
static_assert(sidecar_20261010::k_dsr_direct_spec_hashes.size()==749u,
              "V316 source-direct corpus drift");
static_assert(all_five_are_original_dsr_direct(),
              "V316 low-exclusion identities must have exact source ownership");
constexpr std::size_t strict_source_count() noexcept {
    std::size_t result=0u;
    for(const auto h : sidecar_20261010::k_dsr_direct_spec_hashes)
        if(!is_excluded_direct_low(h))++result;
    return result;
}
static_assert(strict_source_count()==744u,
              "V316 strict 744 non-low source coverage drift");
inline bool strict_direct_source(std::uint64_t hash) noexcept {
    return sidecar_20261010::dsr_direct_spec_allowed(hash) &&
           !is_excluded_direct_low(hash);
}
inline std::atomic<std::uint64_t> low_suffix_policy_filter_events{0u};
inline bool allow_sidecar_equipment_name(std::uint64_t hash) noexcept {
    if(!is_excluded_direct_low(hash))return true;
    low_suffix_policy_filter_events.fetch_add(1u,std::memory_order_relaxed);
    return false;
}
} // namespace dsrrl::runtime::v316
