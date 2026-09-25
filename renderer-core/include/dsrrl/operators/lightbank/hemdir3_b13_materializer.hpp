#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::lightbank {

enum class hemdir3_b13_materialize_result : std::uint8_t {
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

enum class hemdir3_native_stratum : std::uint8_t {
    nospc = 0,
    spc
};

struct hemdir3_b13_materialize_outcome {
    hemdir3_b13_materialize_result result =
        hemdir3_b13_materialize_result::pass_not_candidate;
    std::uint16_t plan_index = 0xFFFFu;
    std::uint16_t shader_index = 0xFFFFu;
    hemdir3_native_stratum stratum =
        hemdir3_native_stratum::nospc;
    std::uint8_t paired_stable_receiver_id = 0u;
    core::operator_mask composed_owners = 0u;
};

hemdir3_b13_materialize_outcome materialize_hemdir3_b13_receiver(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::lightbank
