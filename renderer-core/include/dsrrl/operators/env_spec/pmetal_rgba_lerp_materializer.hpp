#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::env_spec {

enum class pmetal_rgba_lerp_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_v211_stage,
    fail_a1_overlap,
    fail_invalid_dxbc,
    fail_operator_precondition,
    fail_b12_rdef,
    fail_terminal_sat,
    fail_spec_rgb_consumer,
    fail_postcondition,
    fail_rebuild
};

struct pmetal_rgba_lerp_materialize_outcome {
    pmetal_rgba_lerp_materialize_result result =
        pmetal_rgba_lerp_materialize_result::pass_not_candidate;

    std::uint8_t pair_index = 0xffu;
    std::uint32_t semantic_receiver_id = 0u;
    bool envdiffuse_preserved = false;
    bool terminal_sat_rgb_composed = false;
    bool spec_rgb_consumer = false;
};

pmetal_rgba_lerp_materialize_outcome
materialize_pmetal_rgba_lerp_receiver(
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::env_spec
