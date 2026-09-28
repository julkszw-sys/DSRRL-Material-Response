#pragma once

#include "dsrrl/operators/material_response/material_response_island.hpp"
#include <array>

namespace dsrrl::operators::material_response {

using material_response_b12_payload = std::array<std::array<float,4>,4>;

inline material_response_b12_payload make_material_response_b12_payload(
    const decision &value) noexcept
{
    // Active ABI after the Material Response reset:
    //   b12[0].xyz = reserved/neutral (historical c101_f0q proxy removed)
    //   b12[0].w   = verified PTDE c102 for local-specular only
    //   b12[1].xyz = PTDE c100 for the diffuse-material-domain operator
    //   b12[2].xyz = raw PTDE c101 for verified PTDE EnvSpec/local-specular
    // No active generic receiver may interpret b12[0].xyz as DSR F0 input.
    return {{
        {{0.0f,0.0f,0.0f,
          value.ptde_specular_power_verified ? value.ptde_specular_power : 1.0f}},
        {{value.c100[0],value.c100[1],value.c100[2],1.0f}},
        {{value.c101,value.c101,value.c101,1.0f}},
        {{0.0f,0.0f,0.0f,0.0f}}
    }};
}

} // namespace dsrrl::operators::material_response
