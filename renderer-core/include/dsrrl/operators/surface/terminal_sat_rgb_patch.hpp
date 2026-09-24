#pragma once

#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::surface {

inline constexpr std::uint32_t dxbc_saturate_modifier_bit = 0x00002000u;

enum class terminal_sat_patch_result : std::uint8_t {
    applied = 0,
    already_saturated,
    fail_open_invalid_recipe,
    fail_open_unverified_write_shape,
    fail_open_out_of_bounds,
    fail_open_token_mismatch
};

struct terminal_sat_patch_recipe {
    std::size_t instruction_byte_offset = 0;
    std::uint32_t expected_unsaturated_token = 0;
    bool verified_separate_rgb_write = false;
};

terminal_sat_patch_result apply_terminal_rgb_sat(
    std::uint8_t *dxbc,
    std::size_t dxbc_size,
    const terminal_sat_patch_recipe &recipe) noexcept;

} // namespace dsrrl::operators::surface
