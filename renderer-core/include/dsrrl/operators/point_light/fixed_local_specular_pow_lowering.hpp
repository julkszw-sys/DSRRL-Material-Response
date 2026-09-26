#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

// Native DSR SM5 lowering of:
//   pow(max(x, 0), exponent)
// expressed as MAX -> LOG -> MUL -> EXP.
//
// The token shape is attested by 12 stock DSR Water shaders that consume
// gFC_SpcParam.x / cb0[11].x.  The PointLight bridge reuses the same lowering
// but sources the PTDE-authored exponent from Material Response b12[0].w.
struct fixed_local_specular_pow_lowering {
    static constexpr std::size_t word_count = 26u;
    std::array<std::uint32_t,word_count> words{};
};

enum class fixed_local_specular_pow_emit_result : std::uint8_t {
    exact = 0,
    fail_invalid_component,
    fail_invalid_register,
    fail_invalid_exponent_carrier
};

// Emit an in-place scalar POW lowering.
// Before: temp[component] = R dot L.
// After:  temp[component] = pow(max(R dot L,0), b12[0].w).
fixed_local_specular_pow_emit_result
emit_fixed_local_specular_ptde_pow(
    std::uint32_t temp_register,
    std::uint8_t component,
    fixed_local_specular_pow_lowering &out) noexcept;

// Test/provenance seam: emit the stock Water form
// pow(max(x,0), cb0[11].x) for byte-exact template comparison.
fixed_local_specular_pow_emit_result
emit_stock_dsr_water_specular_pow(
    std::uint32_t temp_register,
    std::uint8_t component,
    fixed_local_specular_pow_lowering &out) noexcept;

} // namespace dsrrl::operators::point_light
