#pragma once

#include "types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::core {

constexpr std::uint32_t carrier_abi_v1 = 1;
constexpr std::size_t carrier_v1_lane_count = 8;

enum class carrier_v1_slot : std::uint8_t {
    d1_direction = 0,
    d2_direction = 1,
    d3_direction = 2,
    d1_ptde_color = 3,
    d2_ptde_color = 4,
    d3_ptde_color = 5,
    upper_ptde = 6,
    lower_ptde = 7
};

struct alignas(16) carrier_v1 {
    std::array<float4, carrier_v1_lane_count> lanes{};
};

static_assert(sizeof(carrier_v1) == 128, "DSRRL carrier ABI v1 must remain exactly 8 float4 / 128 bytes.");
static_assert(alignof(carrier_v1) == 16, "DSRRL carrier ABI v1 must remain 16-byte aligned.");

constexpr std::uint32_t carrier_slot_bit(carrier_v1_slot slot) noexcept
{
    return 1u << static_cast<std::uint32_t>(slot);
}

constexpr std::uint32_t carrier_ul_mask =
    carrier_slot_bit(carrier_v1_slot::upper_ptde) |
    carrier_slot_bit(carrier_v1_slot::lower_ptde);

constexpr std::uint32_t carrier_hemdir3_mask =
    carrier_slot_bit(carrier_v1_slot::d1_direction) |
    carrier_slot_bit(carrier_v1_slot::d2_direction) |
    carrier_slot_bit(carrier_v1_slot::d3_direction) |
    carrier_slot_bit(carrier_v1_slot::d1_ptde_color) |
    carrier_slot_bit(carrier_v1_slot::d2_ptde_color) |
    carrier_slot_bit(carrier_v1_slot::d3_ptde_color);

} // namespace dsrrl::core
