#pragma once

#include "dsrrl/runtime/pointlight_bank_structure_authority_v1.hpp"

#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime::clustered_pointlight_source_gate {

enum class gameplay_bank : std::uint8_t {
    m10 = 0u,
    m11 = 1u,
    m12 = 2u,
    m13 = 3u,
    m14 = 4u,
    m15_1 = 5u,
    m15 = 6u,
    m16 = 7u,
    m17 = 8u,
    m18 = 9u,
    unknown = 0xffu
};

static_assert(
    pointlight_bank_structure_authority_v1::
        k_signatures.size() == 10u);

inline gameplay_bank gameplay_bank_from_structure_signature(
    std::uint64_t signature) noexcept
{
    for (std::size_t i = 0u;
         i < pointlight_bank_structure_authority_v1::
                 k_signatures.size();
         ++i) {
        if (pointlight_bank_structure_authority_v1::
                k_signatures[i] == signature)
            return static_cast<gameplay_bank>(i);
    }
    return gameplay_bank::unknown;
}

inline bool known_non_ptde_structure(
    std::uint64_t signature) noexcept
{
    for (const auto candidate :
         pointlight_bank_structure_authority_v1::
             k_known_non_donor_signatures)
        if (candidate == signature)
            return true;
    return false;
}

// Direct DSR-vs-PTDE DrawParam audit: these gameplay coordinates are named,
// repurposed DSR lights while the corresponding PTDE row is unnamed/default.
// They are therefore outside the PTDE source bridge and must remain stock DSR.
inline bool dsr_only_semantic_row(
    gameplay_bank bank,
    std::uint32_t row_id) noexcept
{
    switch (bank) {
    case gameplay_bank::m10:
        return row_id == 13u ||
               row_id == 16u ||
               row_id == 17u;
    case gameplay_bank::m12:
        return row_id == 4u ||
               row_id == 28u;
    case gameplay_bank::m17:
        return row_id == 29u;
    case gameplay_bank::m18:
        return row_id == 7u ||
               row_id == 8u ||
               row_id == 18u ||
               row_id == 27u;
    default:
        return false;
    }
}

} // namespace dsrrl::runtime::clustered_pointlight_source_gate
