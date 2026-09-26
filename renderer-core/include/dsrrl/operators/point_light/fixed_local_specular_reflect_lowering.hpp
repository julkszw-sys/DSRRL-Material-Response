#pragma once

#include "dsrrl/operators/point_light/fixed_local_specular_operand_contract.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

// Fixed-family scalar lowering for the PTDE reflection-vector angular term:
//
//   ndv   = dot(N,V)
//   vdl   = dot(V,L)
//   ndl   = dot(N,L)       // retained for the independent diffuse branch
//   prod  = ndv * ndl
//   prod2 = prod * 2
//   rdl   = vdl * (-1) + prod2
//
// Hence rdl = dot(2*dot(N,V)*N - V, L) without allocating a new vector
// temporary.  The three scalar destinations are existing, window-owned DSR
// temporaries attested by the operand contract.
struct fixed_local_specular_reflect_lowering {
    static constexpr std::size_t word_count = 44u;
    std::array<std::uint32_t,word_count> words{};
    std::uint32_t result_temp_register = 0u;
    std::uint8_t result_component = 0u;
};

enum class fixed_local_specular_reflect_emit_result : std::uint8_t {
    exact = 0,
    fail_invalid_operand,
    fail_non_scalar_destination,
    fail_aliasing_destination
};

fixed_local_specular_reflect_emit_result
emit_fixed_local_specular_reflect_rdotl(
    const fixed_local_specular_light_operands &operands,
    fixed_local_specular_reflect_lowering &out) noexcept;

} // namespace dsrrl::operators::point_light
