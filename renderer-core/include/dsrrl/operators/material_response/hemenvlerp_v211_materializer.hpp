#pragma once

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
};

hemenvlerp_v211_outcome
materialize_hemenvlerp_v211_certified_stage(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::material_response
