#pragma once

#include "dsrrl/core/types.hpp"

#include <array>
#include <cstdint>

namespace dsrrl::operators::legacy_plan {

struct decomposition {
    std::array<core::operator_id, 8> owners{};
    std::uint8_t owner_count = 0;
    std::uint32_t closed_bits = 0;
    std::uint32_t nonclosed_bits = 0;
    std::uint32_t rejected_bits = 0;
    std::uint32_t unknown_bits = 0;

    bool automatic_migration_safe() const noexcept
    {
        return nonclosed_bits == 0 &&
               rejected_bits == 0 &&
               unknown_bits == 0;
    }
};

decomposition decompose_p22_mask(std::uint32_t legacy_mask) noexcept;
std::uint32_t legacy_bits_for(core::operator_id op) noexcept;

} // namespace dsrrl::operators::legacy_plan
