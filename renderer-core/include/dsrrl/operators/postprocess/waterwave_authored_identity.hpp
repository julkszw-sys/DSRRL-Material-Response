#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::postprocess {

inline constexpr std::size_t
    k_waterwave_mtd_size = 1232u;

inline constexpr std::array<std::uint8_t,32>
    k_waterwave_mtd_sha256 = {
        0xF5u,0x4Bu,0x8Cu,0xD6u,0x20u,0xC6u,0xDCu,0xA4u,
        0x31u,0x9Du,0x83u,0x68u,0xF5u,0xD3u,0x0Fu,0x9Du,
        0x3Fu,0xE1u,0xDDu,0xF6u,0x08u,0x49u,0xDDu,0xDCu,
        0x0Eu,0x63u,0x08u,0xC5u,0x9Eu,0xF7u,0xA8u,0x37u
    };

inline constexpr std::uint32_t
    k_waterwave_spx_semantic_id = 0x0E35u;

inline constexpr std::uint32_t
    k_waterwave_blend_mode = 9u;

inline constexpr std::uint32_t
    k_waterwave_ptde_param_index = 0x41u;

inline constexpr std::uint32_t
    k_waterwave_dsr_param_index = 0x53u;

inline constexpr std::uint32_t
    k_waterwave_dsr_source_complete_mtd_count = 581u;

inline constexpr std::uint32_t
    k_waterwave_dsr_material_name_occurrences = 1u;

inline constexpr std::uint32_t
    k_waterwave_dsr_spx_occurrences = 1u;

inline constexpr std::uint32_t
    k_waterwave_dsr_flag_name_occurrences = 1u;

inline constexpr bool
waterwave_dsr_authored_semantic_is_source_complete_unique() noexcept
{
    return
        k_waterwave_dsr_source_complete_mtd_count == 581u &&
        k_waterwave_dsr_material_name_occurrences == 1u &&
        k_waterwave_dsr_spx_occurrences == 1u &&
        k_waterwave_dsr_flag_name_occurrences == 1u;
}

enum class waterwave_authored_identity_result : std::uint8_t {
    exact_authored_identity = 0,
    raw_mtd_not_exact,
    semantic_spx_not_exact,
    blend_mode_not_exact,
    waterwave_flag_not_exact
};

struct waterwave_authored_identity {
    bool raw_mtd_exact = false;
    std::uint32_t spx_semantic_id = 0u;
    std::uint32_t blend_mode = 0u;
    bool is_waterwave_sfx = false;
};

inline waterwave_authored_identity_result
validate_waterwave_authored_identity(
    const waterwave_authored_identity &identity) noexcept
{
    if (!identity.raw_mtd_exact)
        return waterwave_authored_identity_result::
            raw_mtd_not_exact;

    if (identity.spx_semantic_id !=
        k_waterwave_spx_semantic_id)
        return waterwave_authored_identity_result::
            semantic_spx_not_exact;

    if (identity.blend_mode !=
        k_waterwave_blend_mode)
        return waterwave_authored_identity_result::
            blend_mode_not_exact;

    if (!identity.is_waterwave_sfx)
        return waterwave_authored_identity_result::
            waterwave_flag_not_exact;

    return waterwave_authored_identity_result::
        exact_authored_identity;
}

// Cross-render authored identity is CONFIRMED, but it is intentionally not a
// live draw authorization. Runtime still must transport this exact identity
// through the FX appearance/entity instance to the same collector draw before
// Bloom Q8 recurrence can advance to collector_draw_token_closed.
inline constexpr bool
waterwave_authored_identity_is_draw_authority() noexcept
{
    return false;
}

} // namespace dsrrl::operators::postprocess
