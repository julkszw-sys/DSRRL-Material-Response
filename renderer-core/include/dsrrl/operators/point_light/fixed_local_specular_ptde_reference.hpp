#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class fixed_local_specular_ptde_profile : std::uint8_t {
    hemenv = 0,
    hemenvlerp = 1
};

struct fixed_local_specular_ptde_reference {
    std::uint8_t light_count = 0u;
    std::array<std::uint8_t,32> ptde_sha256{};
    std::uint32_t ptde_size = 0u;
};

bool fixed_local_specular_ptde_reference_for_digest(
    const std::array<std::uint8_t,32> &dsr_sha256,
    std::size_t dsr_size,
    fixed_local_specular_ptde_profile profile,
    fixed_local_specular_ptde_reference &reference) noexcept;

bool fixed_local_specular_ptde_reference_for_shader(
    const void *pixel_shader_code,
    std::size_t code_size,
    fixed_local_specular_ptde_profile profile,
    fixed_local_specular_ptde_reference &reference) noexcept;

std::size_t fixed_local_specular_ptde_reference_count() noexcept;

} // namespace dsrrl::operators::point_light
