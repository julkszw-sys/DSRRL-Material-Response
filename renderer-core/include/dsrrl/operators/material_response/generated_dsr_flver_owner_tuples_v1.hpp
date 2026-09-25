#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {

struct flver_owner_tuple_record {
    std::array<std::uint8_t, 32> flver_sha256{};
    std::uint32_t material_slot = 0;
    std::uint64_t semantic_name_hash = 0;
};

// Placeholder generated surface. The source-complete compact DSR corpus is not
// presently available in the repository, so positive owner authentication is
// deliberately impossible. materialize_dsr_flver_owner_tuples.py replaces
// this surface only from the content-addressed canonical 4161-FLVER /
// 19985-material-slot corpus.
inline constexpr char k_dsr_flver_owner_tuple_source_sha256[] = "";
inline constexpr bool k_dsr_flver_owner_tuple_source_complete = false;
inline constexpr std::size_t k_dsr_flver_owner_tuple_count = 0u;
inline constexpr std::array<flver_owner_tuple_record, 0>
    k_dsr_flver_owner_tuples = {};

constexpr bool dsr_flver_owner_tuple_authenticated(
    const std::array<std::uint8_t, 32> &,
    std::uint32_t,
    std::uint64_t) noexcept
{
    return false;
}

} // namespace dsrrl::operators::material_response::generated
