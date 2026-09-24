#include "dsrrl/operators/surface/terminal_sat_rgb_patch.hpp"

#include <cstring>

namespace dsrrl::operators::surface {

terminal_sat_patch_result apply_terminal_rgb_sat(
    std::uint8_t *dxbc,
    std::size_t dxbc_size,
    const terminal_sat_patch_recipe &recipe) noexcept
{
    if ((recipe.instruction_byte_offset & 3u) != 0u ||
        (recipe.expected_unsaturated_token & dxbc_saturate_modifier_bit) != 0u)
        return terminal_sat_patch_result::fail_open_invalid_recipe;

    // The one-bit bridge is valid only for the RE-verified final *separate*
    // RGB write. A combined RGBA write would also clamp alpha and therefore
    // belongs to the separate diagnostic/body-rewrite island.
    if (!recipe.verified_separate_rgb_write)
        return terminal_sat_patch_result::fail_open_unverified_write_shape;

    if (dxbc == nullptr ||
        recipe.instruction_byte_offset > dxbc_size ||
        dxbc_size - recipe.instruction_byte_offset < sizeof(std::uint32_t))
        return terminal_sat_patch_result::fail_open_out_of_bounds;

    std::uint32_t token = 0;
    std::memcpy(
        &token,
        dxbc + recipe.instruction_byte_offset,
        sizeof(token));

    const std::uint32_t saturated_token =
        recipe.expected_unsaturated_token | dxbc_saturate_modifier_bit;

    if (token == saturated_token)
        return terminal_sat_patch_result::already_saturated;

    if (token != recipe.expected_unsaturated_token)
        return terminal_sat_patch_result::fail_open_token_mismatch;

    std::memcpy(
        dxbc + recipe.instruction_byte_offset,
        &saturated_token,
        sizeof(saturated_token));

    return terminal_sat_patch_result::applied;
}

} // namespace dsrrl::operators::surface
