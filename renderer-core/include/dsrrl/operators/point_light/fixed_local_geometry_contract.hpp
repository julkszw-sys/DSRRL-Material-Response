#pragma once
#include "dsrrl/operators/point_light/fixed_local_specular_island_plan.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace dsrrl::operators::point_light {
enum class fixed_local_geometry_result : std::uint8_t {
 exact=0, pass_not_fixed_local_specular, fail_island_plan,
 fail_invalid_dxbc, fail_prefix_shape
};
struct fixed_local_geometry_light {
 std::uint32_t vector_add_word=0u;
 std::uint32_t distance_sq_dp3_word=0u;
 std::uint32_t distance_sqrt_word=0u;
 std::uint32_t end_compare_word=0u;
 std::uint32_t light_if_word=0u;
};
struct fixed_local_geometry_contract {
 fixed_local_geometry_result result=fixed_local_geometry_result::pass_not_fixed_local_specular;
 fixed_local_specular_island_plan island{};
 std::array<fixed_local_geometry_light,4> lights{};
 std::uint8_t light_count=0u;
};
fixed_local_geometry_contract attest_fixed_local_geometry_contract(
 const void *pixel_shader_code,std::size_t code_size) noexcept;
} // namespace dsrrl::operators::point_light
