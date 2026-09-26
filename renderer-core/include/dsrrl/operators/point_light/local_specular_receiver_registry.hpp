#pragma once

#include "dsrrl/operators/point_light/legacy_specular.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

struct local_specular_receiver_identity {
    local_specular_receiver_class receiver_class =
        local_specular_receiver_class::unsupported;
    std::uint16_t representative_shader_index = 0u;
    std::uint8_t alias_count = 0u;
};

bool local_specular_receiver_for_digest(
    const std::array<std::uint8_t,32> &sha256,
    std::size_t code_size,
    local_specular_receiver_identity &identity) noexcept;

bool local_specular_receiver_for_shader(
    const void *pixel_shader_code,
    std::size_t code_size,
    local_specular_receiver_identity &identity) noexcept;

std::size_t local_specular_receiver_count() noexcept;

} // namespace dsrrl::operators::point_light
