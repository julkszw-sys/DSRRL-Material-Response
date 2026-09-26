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

    fixed_local_specular_sample_site diffuse_a_t0{};
    fixed_local_specular_sample_site specular_a_t1{};
    fixed_local_specular_sample_site diffuse_b_t3{};
    fixed_local_specular_sample_site specular_b_t4{};
    bool has_blend_b = false;
};

// Locates the stock material endpoint samples needed by the fixed direct-PTDE
// island. Exact corpus invariant on the 48 unique Spc bodies:
//   every body: exactly one base diffuse t0 and one base specular t1 sample;
//   Mul/blended bodies: exactly one additional diffuse t3 + specular t4 pair.
// t2/t5 normal, t6 lightmap and t7 shadow resources are intentionally outside
// this material-endpoint contract. All four material endpoints, when present,
// precede the first fixed-light window. This scanner does not mutate bytes.
fixed_local_specular_material_samples
locate_fixed_local_specular_material_samples(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
