#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {

// Construction-safe registry contract. The full source-complete table is generated
// by tools/generate_dsr_mtd_identity_registry.py from exact raw DSR MTD payloads.
// Until that generated payload is materialized in-repo, this checked-in fallback
// intentionally resolves nothing: absence is UNKNOWN/fail-open, never NO_USE.
struct dsr_mtd_identity_record {
    std::uint64_t semantic_name_hash;
    std::array<std::uint8_t, 32> raw_mtd_sha256;
    bool ambiguous;
};

inline constexpr bool k_dsr_mtd_identity_source_complete = false;
inline constexpr std::array<dsr_mtd_identity_record, 0> k_dsr_mtd_identity_registry{};

constexpr bool dsr_mtd_identity_resolve(
    std::uint64_t,
    std::array<std::uint8_t, 32> &out) noexcept
{
    out = {};
    return false;
}

constexpr bool dsr_mtd_identity_ambiguous(std::uint64_t) noexcept
{
    return false;
}

} // namespace dsrrl::operators::material_response::generated
