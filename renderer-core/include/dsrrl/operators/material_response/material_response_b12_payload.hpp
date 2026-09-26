#pragma once

#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <array>

namespace dsrrl::operators::material_response {

using material_response_b12_payload = std::array<std::array<float,4>,4>;

inline material_response_b12_payload make_material_response_b12_payload(
    const decision &value) noexcept
{
    return {{
        {{value.c101_f0q[0],value.c101_f0q[1],value.c101_f0q[2],
          value.ptde_specular_power_verified ? value.ptde_specular_power : 1.0f}},
        {{value.c100[0],value.c100[1],value.c100[2],1.0f}},
        {{value.c101,value.c101,value.c101,1.0f}},
        {{0.0f,0.0f,0.0f,0.0f}}
    }};
}

} // namespace dsrrl::operators::material_response
