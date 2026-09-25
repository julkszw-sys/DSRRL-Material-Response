#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::lightbank {

enum class upper_lower_hemenv_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_plan_inconsistent,
    fail_operand_precondition,
    fail_rebuild,
    fail_stage_sha,
    fail_a1_composition
};

enum class upper_lower_hemenv_stratum : std::uint8_t {
    nospc = 0,
    spc
};

struct upper_lower_hemenv_materialize_outcome {
    upper_lower_hemenv_materialize_result result =
        upper_lower_hemenv_materialize_result::pass_not_candidate;
    std::uint16_t plan_index = 0xFFFFu;
    std::uint16_t shader_index = 0xFFFFu;
    std::uint8_t stable_receiver_id = 0u;
    upper_lower_hemenv_stratum stratum =
        upper_lower_hemenv_stratum::nospc;
    core::operator_mask composed_owners = 0u;
};

upper_lower_hemenv_materialize_outcome
materialize_upper_lower_hemenv_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::lightbank
