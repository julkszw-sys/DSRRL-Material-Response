#pragma once

#include "dsrrl/operators/point_light/fixed_local_specular_legacy_kernel.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_output_cut.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class fixed_local_specular_island_plan_result : std::uint8_t {
    ready = 0,
    pass_not_fixed_local_specular,
    fail_output_cut,
    fail_light_count,
    fail_exponent_carrier,
    fail_angular_kernel
};

struct fixed_local_specular_island_plan {
    fixed_local_specular_island_plan_result result =
        fixed_local_specular_island_plan_result::
            pass_not_fixed_local_specular;

    fixed_local_specular_output_cut output_cut{};
    std::array<fixed_local_specular_legacy_kernel,4> angular{};
    std::uint8_t light_count = 0u;

    std::uint8_t raw_q_srv_slot = 19u;
    std::uint8_t material_cb_slot = 12u;

    // Direct PTDE material ABI. b12[0].xyz is deliberately NOT listed here:
    // that lane is c101_f0q for the stock-host Material Response bridge.
    // The direct PointLight island owns authored PTDE material terms only.
    std::uint16_t ptde_c100_cb_index = 1u;
    std::uint16_t ptde_c101_cb_index = 2u;
    std::uint16_t ptde_specular_power_cb_index = 0u;
    std::uint8_t ptde_specular_power_component = 3u;

    bool replace_complete_local_specular = false;
    bool stock_microfacet_dead_at_output_cut = false;
    bool preserve_downstream_fog = true;
};

// Final construction contract before byte emission. It intentionally does not
// mutate DXBC: all exact identity, operand, exponent and output-splice
// prerequisites must be closed first.
fixed_local_specular_island_plan
build_fixed_local_specular_island_plan(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
