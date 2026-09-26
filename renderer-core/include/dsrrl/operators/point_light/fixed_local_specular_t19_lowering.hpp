#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

// Native DSR SM5 structured-resource encoding recovered from stock PntS:
//   dcl_resource_structured t16/t17 stride4, t18 stride48
// and vector LD_STRUCTURED from t18. The bridge uses the identical token
// shapes for a mod-owned float4 raw-q array at t19, stride16.
struct fixed_local_specular_t19_decl {
    static constexpr std::size_t word_count = 4u;
    std::array<std::uint32_t,word_count> words{};
};

struct fixed_local_specular_t19_load {
    static constexpr std::size_t word_count = 11u;
    std::array<std::uint32_t,word_count> words{};
};

enum class fixed_local_specular_t19_emit_result : std::uint8_t {
    exact = 0,
    fail_invalid_register,
    fail_invalid_light_ordinal
};

fixed_local_specular_t19_emit_result
emit_fixed_local_specular_t19_decl(
    fixed_local_specular_t19_decl &out) noexcept;

// Emits:
//   ld_structured rN.xyzw, l(light_ordinal), l(0), t19.xyzw
// where each structure element is raw q=float4 and stride is 16 bytes.
fixed_local_specular_t19_emit_result
emit_fixed_local_specular_t19_load(
    std::uint32_t destination_temp,
    std::uint8_t light_ordinal,
    fixed_local_specular_t19_load &out) noexcept;

} // namespace dsrrl::operators::point_light
