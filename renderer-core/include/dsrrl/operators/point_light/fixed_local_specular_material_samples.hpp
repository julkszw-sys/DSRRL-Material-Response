#pragma once

#include "dsrrl/operators/point_light/fixed_local_specular_island_plan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class fixed_local_specular_material_topology : std::uint8_t {
    unsupported = 0,
    single_diffuse_spec,
    blended_diffuse_spec
};

enum class fixed_local_specular_material_sample_result : std::uint8_t {
    exact = 0,
    pass_not_fixed_local_specular,
    fail_island_plan,
    fail_invalid_dxbc,
    fail_sample_shape,
    fail_sample_count,
    fail_sample_order
};

struct fixed_local_specular_sample_site {
    std::uint32_t instruction_word = 0u;
    std::uint32_t destination_token = 0u;
    std::uint32_t destination_register = 0u;
};

struct fixed_local_specular_material_samples {
    fixed_local_specular_material_sample_result result =
        fixed_local_specular_material_sample_result::
            pass_not_fixed_local_specular;
    fixed_local_specular_island_plan island{};
    fixed_local_specular_material_topology topology =
        fixed_local_specular_material_topology::unsupported;

    std::array<fixed_local_specular_sample_site,2> diffuse_t0{};
    std::array<fixed_local_specular_sample_site,2> specular_t1{};
    std::uint8_t diffuse_count = 0u;
    std::uint8_t specular_count = 0u;
};

// Locates the stock texture sample sites that must be split for the fixed
// direct-PTDE island. Exact corpus invariant:
//   24 bodies: one t0 + one t1 sample,
//   24 bodies: two t0 + two t1 samples.
// All sample sites must precede the first fixed-light microfacet window.
// This scanner does not mutate resources or shader bytes.
fixed_local_specular_material_samples
locate_fixed_local_specular_material_samples(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
