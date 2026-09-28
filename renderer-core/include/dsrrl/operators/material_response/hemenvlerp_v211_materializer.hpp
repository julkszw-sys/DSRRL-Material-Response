#pragma once

#include "dsrrl/core/feature_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::material_response {

enum class hemenvlerp_v211_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_patch_precondition,
    fail_stage_sha,
    fail_rebuild
};

struct hemenvlerp_v211_outcome {
    hemenvlerp_v211_result result =
        hemenvlerp_v211_result::pass_not_candidate;
    std::uint8_t pair_index = 0xffu;
    core::operator_mask composed_owners = 0u;
};

// New active HemEnvLerp Material Response base. Built directly from the
// exact stock shader and limited to PTDE c100 diffuse response plus
// independently certified surface composition. Historical V2.10/V2.11
// c101->DSR-F0 semantics are not executed.
hemenvlerp_v211_outcome
materialize_ptde_diffuse_hemenvlerp_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

hemenvlerp_v211_outcome
materialize_hemenvlerp_v211_certified_stage(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

// Compose only independently certified post-MR surface islands after the
// exact V2.11 HemEnvLerp stage. The certified-stage hashes above remain the
// provenance boundary; unknown terminal write shapes fail open.
hemenvlerp_v211_outcome
materialize_hemenvlerp_v211_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::material_response
