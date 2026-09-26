#pragma once

#include "dsrrl/operators/point_light/fixed_local_specular_patch_plan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class fixed_local_specular_operand_result : std::uint8_t {
    ready = 0,
    pass_not_fixed_local_specular,
    fail_plan_not_ready,
    fail_invalid_dxbc,
    fail_window_bounds,
    fail_dp3_shape,
    fail_operand_shape,
    fail_tail_shape,
    fail_fixed_cb_identity
};

struct fixed_local_specular_operand_pair {
    std::array<std::uint32_t,2> token{};
};

struct fixed_local_specular_light_operands {
    fixed_local_specular_operand_pair normal{};
    fixed_local_specular_operand_pair view{};
    fixed_local_specular_operand_pair light{};

    // Scalar destinations already owned by the DSR microfacet window:
    //   stock VdotH -> replacement NdotV / final RdotL
    //   stock NdotH -> replacement VdotL auxiliary
    //   stock NdotL -> retained NdotL for the separately preserved diffuse path.
    // Reusing these destinations avoids introducing a new temporary-register ABI.
    fixed_local_specular_operand_pair primary_scalar_dst{};
    fixed_local_specular_operand_pair auxiliary_scalar_dst{};
    fixed_local_specular_operand_pair ndotl_scalar_dst{};

    // Stock tail anchors retained only as structural witnesses. A later
    // materializer must not reuse their DSR common-NdotL specular semantics.
    std::uint32_t ndotl_dp3_word = 0u;
    std::uint32_t stock_light_color_cb = 0u;
    std::uint32_t stock_position_begin_cb = 0u;
};

struct fixed_local_specular_operand_contract {
    fixed_local_specular_operand_result result =
        fixed_local_specular_operand_result::pass_not_fixed_local_specular;
    fixed_local_specular_patch_plan plan{};
    std::array<fixed_local_specular_light_operands,4> lights{};
    std::uint8_t light_count = 0u;

    // Stock cb0[11].x remains an attested routing witness. The actual PTDE
    // exponent donor is carried by Material Response in b12[0].w.
    std::uint8_t stock_specular_power_cb_slot = 0u;
    std::uint16_t stock_specular_power_cb_index = 11u;
    std::uint8_t stock_specular_power_component = 0u;
    std::uint8_t ptde_specular_power_cb_slot = 12u;
    std::uint16_t ptde_specular_power_cb_index = 0u;
    std::uint8_t ptde_specular_power_component = 3u;
    bool exponent_carrier_attested = false;
};

// Exact-token semantic extraction after receiver/window attestation.
// It recovers host-register operands for N/V/L from the canonical
// DP3(V,H), DP3(N,H), DP3(N,L) triplet. This avoids any hardcoded r# ABI.
// Deterministic test/offline seam after exact receiver identity and
// token-window attestation. Production runtime must use the DXBC wrapper below.
fixed_local_specular_operand_contract
extract_fixed_local_specular_operand_contract_from_attested_shex_words(
    const fixed_local_specular_patch_plan &plan,
    const std::uint32_t *shex_words,
    std::size_t word_count) noexcept;

fixed_local_specular_operand_contract
extract_fixed_local_specular_operand_contract(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
