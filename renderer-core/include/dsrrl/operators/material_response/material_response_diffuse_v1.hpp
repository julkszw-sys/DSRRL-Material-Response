#pragma once

#include "dsrrl/core/feature_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::material_response {

enum class diffuse_v1_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_patch_precondition,
    fail_rebuild
};

enum class diffuse_v1_family : std::uint8_t {
    stable_hemenv = 0,
    hemenvlerp
};

struct diffuse_v1_outcome {
    diffuse_v1_result result =
        diffuse_v1_result::pass_not_candidate;
    diffuse_v1_family family =
        diffuse_v1_family::stable_hemenv;
    std::uint8_t family_index = 0xffu;
    std::uint32_t receiver_id = 0u;
    core::operator_mask composed_owners = 0u;
};

// Active generic Material Response operator.
//
// This is a direct stock-DSR -> PTDE diffuse-material-domain bridge:
//   cb0[9] DSR diffuse material factor -> b12[1] PTDE c100
//   DSR abs(Z)^2.2 material-domain response -> linear Z
//
// It deliberately does NOT consume c101, does NOT modify DSR F0, and does
// NOT retain or reproduce the historical V2.10/V2.11 Material Response proxy.
// SpecRGB/c101/c102 are owned by their verified downstream PTDE operators.
diffuse_v1_outcome materialize_ptde_diffuse_response_v1(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output,
    bool defer_surface_operators = false) noexcept;

} // namespace dsrrl::operators::material_response
