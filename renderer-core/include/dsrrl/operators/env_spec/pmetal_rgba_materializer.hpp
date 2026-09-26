#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::env_spec {

enum class pmetal_rgba_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_v211_stage,
    fail_a1_composition,
    fail_build131_precondition,
    fail_build131_rdef,
    fail_b12_rdef,
    fail_upper_lower_composition,
    fail_spec_rgb_consumer,
    fail_postcondition,
    fail_rebuild
};

struct pmetal_rgba_materialize_outcome {
    pmetal_rgba_materialize_result result =
        pmetal_rgba_materialize_result::pass_not_candidate;
    std::uint32_t receiver_id = 0;
    core::operator_mask composed_owners = 0;
    bool upper_lower_composed = false;
    bool spec_rgb_consumer = false;
};

pmetal_rgba_materialize_outcome
materialize_pmetal_rgba_receiver(
    const core::feature_registry &features,
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    bool compose_upper_lower,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::env_spec
