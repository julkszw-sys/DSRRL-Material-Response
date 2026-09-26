#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dsrrl/core/feature_registry.hpp"

namespace dsrrl::operators::material_response {

enum class v211_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_patch_precondition,
    fail_rebuild,
    fail_stage_sha
};

struct v211_materialize_outcome {
    v211_materialize_result result =
        v211_materialize_result::pass_unknown_exact_sha;
    std::uint32_t receiver_id = 0;
    core::operator_mask composed_owners = 0;
};

// Exact historical V2.11 stage before generic A1 composition and final
// reflection augmentation. This is the certified semantic cut used by
// downstream operator-island materializers such as P_Metal EnvSpec.
v211_materialize_outcome materialize_v211_certified_stage(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

v211_materialize_outcome materialize_v211_stable_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::material_response
