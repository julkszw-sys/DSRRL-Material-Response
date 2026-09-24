#pragma once

#include <cstdint>

namespace dsrrl::operators::surface {

enum class diffuse_material_domain_result : std::uint8_t {
    exact_local_host_forward = 0,
    fail_open_nonfinite_input
};

struct diffuse_material_domain_sample {
    diffuse_material_domain_result result =
        diffuse_material_domain_result::fail_open_nonfinite_input;
    float dsr_pretransform = 0.0f;
    float stock_dsr = 0.0f;
    float linear_bridge = 0.0f;
};

// Confirmed local DSR-host material-domain cut for targeted diffuse dependencies.
//
// Stock DSR local dependency: M_D = abs(Z_D)^2.2
// PTDE-style local bridge on the same DSR pretransform carrier: M_bridge = Z_D
//
// This does NOT assert Z_D == Z_PTDE and does not own SPEC, WORKFLOW,
// atmosphere, textures, vertex colors or downstream composition.
diffuse_material_domain_sample evaluate_diffuse_material_domain_host(
    float dsr_pretransform) noexcept;

} // namespace dsrrl::operators::surface
