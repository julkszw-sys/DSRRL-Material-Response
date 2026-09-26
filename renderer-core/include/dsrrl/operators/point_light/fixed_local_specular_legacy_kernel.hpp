#pragma once
#include "dsrrl/operators/point_light/fixed_local_specular_operand_contract.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace dsrrl::operators::point_light {
struct fixed_local_specular_legacy_kernel {
    static constexpr std::size_t word_count = 69u;
    std::array<std::uint32_t,word_count> words{};
    std::uint32_t result_temp_register = 0u;
    std::uint8_t result_component = 0u;
};
enum class fixed_local_specular_legacy_kernel_result : std::uint8_t {
    exact = 0,
    fail_reflect_lowering,
    fail_pow_lowering
};
fixed_local_specular_legacy_kernel_result emit_fixed_local_specular_legacy_kernel(
    const fixed_local_specular_light_operands &operands,
    fixed_local_specular_legacy_kernel &out) noexcept;
} // namespace dsrrl::operators::point_light
